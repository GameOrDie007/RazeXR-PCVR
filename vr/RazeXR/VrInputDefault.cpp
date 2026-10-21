/************************************************************************************

Filename	:	VrInputDefault.c
Content		:	Handles default controller input
Created		:	August 2019
Authors		:	Simon Brown

*************************************************************************************/


// PCVR port: their include of <android/keycodes.h> is vestigial - no
// AKEYCODE_ appears anywhere in this file.
#include <c_dispatch.h>

#include "VrInput.h"

#include "keydef.h"
#include "menustate.h"

#include <chrono>

// hw_vrmodes.cpp - the pause menu as a panel in the world, and hiding it.
bool VR_MenuInWorld();
bool TBXR_IndexControllersActive();	// OpenXrInput.cpp
bool VRWheel_Enabled();				// source/core/vr_wheel.cpp
void VRWheel_SetHeld(bool held);
bool VRWheel_ItemsEnabled();
void VRWheel_SetItemHeld(bool held);
bool VR_MenuHidden();
void VR_MenuSetHidden(bool hidden);

extern ovrInputStateTrackedRemote leftTrackedRemoteState_old;
extern ovrInputStateTrackedRemote leftTrackedRemoteState_new;
extern ovrTrackedController leftRemoteTracking_new;

extern ovrInputStateTrackedRemote rightTrackedRemoteState_old;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_new;
extern ovrTrackedController rightRemoteTracking_new;

extern float remote_movementSideways;
extern float remote_movementForward;
extern float remote_movementUp;
extern float positional_movementSideways;
extern float positional_movementForward;
extern float snapTurn;
bool TBXR_MenuPanel(XrPosef* pose, float* width, float* height);	// TBXR_PC.cpp
void VRPointer_Update(bool on, float u, float v, bool trigger);	// vr_pointer.cpp

/*
    PC branch: where the hand's aim meets the menu panel, as 0..1 across and
    down from its top left. The panel is a quad turned only about the vertical
    and seen from its +Z side; the aim looks down its own -Z.
*/
static bool MenuPointerHit(const XrPosef& aim, float* u, float* v)
{
    XrPosef panel;
    float w, h;
    if (!TBXR_MenuPanel(&panel, &w, &h)) return false;

    const XrQuaternionf& a = aim.orientation;
    const float dx = -2.0f * (a.x * a.z + a.w * a.y);
    const float dy = -2.0f * (a.y * a.z - a.w * a.x);
    const float dz = -(1.0f - 2.0f * (a.x * a.x + a.y * a.y));

    const XrQuaternionf& q = panel.orientation;
    const float nx = 2.0f * (q.x * q.z + q.w * q.y);			// its facing, +Z
    const float ny = 2.0f * (q.y * q.z - q.w * q.x);
    const float nz = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    const float rx = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);		// across, +X
    const float ry = 2.0f * (q.x * q.y + q.w * q.z);
    const float rz = 2.0f * (q.x * q.z - q.w * q.y);
    const float ux = 2.0f * (q.x * q.y - q.w * q.z);			// up, +Y
    const float uy = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
    const float uz = 2.0f * (q.y * q.z + q.w * q.x);

    const float denom = dx * nx + dy * ny + dz * nz;
    if (denom > -1e-4f) return false;							// pointing away from its face
    const float px = panel.position.x - aim.position.x;
    const float py = panel.position.y - aim.position.y;
    const float pz = panel.position.z - aim.position.z;
    const float t = (px * nx + py * ny + pz * nz) / denom;
    if (t <= 0.0f) return false;
    const float hx = aim.position.x + dx * t - panel.position.x;
    const float hy = aim.position.y + dy * t - panel.position.y;
    const float hz = aim.position.z + dz * t - panel.position.z;
    *u = (hx * rx + hy * ry + hz * rz) / w + 0.5f;
    *v = 0.5f - (hx * ux + hy * uy + hz * uz) / h;
    return *u >= 0.0f && *u <= 1.0f && *v >= 0.0f && *v <= 1.0f;
}
extern float recenterYaw;
void VR_RecenterAll();

extern bool weaponStabilised;

int getGameState();
void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, int base);
float getViewpointYaw();

EXTERN_CVAR(Float, vr_height_adjust)

void get_weapon_pos_and_angle(float &x, float &y, float &z, float &pitch, float &yaw)
{
    x = weaponoffset[2];
    y = weaponoffset[0];
    z = weaponoffset[1] + hmdPosition[1] + vr_height_adjust; // position off floor

    pitch = hmdorientation[PITCH] - weaponangles[PITCH];
    yaw = hmdorientation[YAW] - weaponangles[YAW];
}

void HandleInput_Default( int control_scheme, ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTrackedController* pDominantTracking,
                          ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTrackedController* pOffTracking,
                          int domButton1, int domButton2, int offButton1, int offButton2 )

{
    //Menu button - invoke menu
    // PCVR port: not while the menu is hidden for a screenshot - there the
    // press brings the menu back (below) rather than closing it into the game.
    if (!VR_MenuHidden())
    {
        handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, xrButton_Enter, KEY_ESCAPE);
        handleTrackedControllerButton(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, xrButton_Enter, KEY_ESCAPE); // For users who have switched the buttons
    }

    /*
        PC branch: the grips. Both pressed together is the Game Or Die
        recenter, in play and in menus. A lone grip waits a few frames for the
        other before it reaches the game, so a chord never leaks a grip's own
        job (the gun hand's is the shift for the second set of controls, the
        off hand's is Run). "Together" means within a fraction of a second: a
        grip already held for longer - running, then the inventory - is a
        combination, and both work as before.
    */
    {
        static int domFrames = 0, offFrames = 0;
        static bool chord = false;
        const int kGripWait = 8;		// frames, about 90 ms
        const int kTogether = 15;		// frames between the two presses
        const bool dom = (pDominantTrackedRemoteNew->Buttons & xrButton_GripTrigger) != 0;
        const bool off = (pOffTrackedRemoteNew->Buttons & xrButton_GripTrigger) != 0;
        domFrames = dom ? domFrames + 1 : 0;
        offFrames = off ? offFrames + 1 : 0;
        if (dom && off && !chord && abs(domFrames - offFrames) <= kTogether &&
            (domFrames < kGripWait + kTogether || offFrames < kGripWait + kTogether))
        {
            chord = true;
            VR_RecenterAll();
        }
        else if (!dom && !off) chord = false;
        const bool domOk = dom && !chord && (domFrames >= kGripWait || off);
        const bool offOk = off && !chord && (offFrames >= kGripWait || dom);
        if (!domOk) pDominantTrackedRemoteNew->Buttons &= ~xrButton_GripTrigger;
        if (!offOk) pOffTrackedRemoteNew->Buttons &= ~xrButton_GripTrigger;
    }

    //Dominant Grip works like a shift key
    bool dominantGripPushedOld = vr_secondary_button_mappings ?
            (pDominantTrackedRemoteOld->Buttons & xrButton_GripTrigger) : false;
    bool dominantGripPushedNew = vr_secondary_button_mappings ?
            (pDominantTrackedRemoteNew->Buttons & xrButton_GripTrigger) : false;

    ovrInputStateTrackedRemote *pPrimaryTrackedRemoteNew, *pPrimaryTrackedRemoteOld,  *pSecondaryTrackedRemoteNew, *pSecondaryTrackedRemoteOld;
    /*
        PC branch: the Leftorium moves the gun to the left hand and leaves the
        sticks where they were: the left stick walks, the right stick turns,
        and their clicks go with them, which is what left-handed players asked
        for. So it undoes the swap for the sticks, and
        Switch Sticks, a row of its own, swaps them for anyone. "Primary" below
        is the turning stick, "secondary" the walking one.
    */
    const bool leftorium = control_scheme >= 10;
    if (vr_switch_sticks != leftorium)
    {
        pPrimaryTrackedRemoteNew = pOffTrackedRemoteNew;
        pPrimaryTrackedRemoteOld = pOffTrackedRemoteOld;
        pSecondaryTrackedRemoteNew = pDominantTrackedRemoteNew;
        pSecondaryTrackedRemoteOld = pDominantTrackedRemoteOld;
    }
    else
    {
        pPrimaryTrackedRemoteNew = pDominantTrackedRemoteNew;
        pPrimaryTrackedRemoteOld = pDominantTrackedRemoteOld;
        pSecondaryTrackedRemoteNew = pOffTrackedRemoteNew;
        pSecondaryTrackedRemoteOld = pOffTrackedRemoteOld;
    }

    //All this to allow stick and button switching!
    uint32_t primaryButtonsNew;
    uint32_t primaryButtonsOld;
    uint32_t secondaryButtonsNew;
    uint32_t secondaryButtonsOld;
    int primaryButton1;
    int primaryButton2;
    int secondaryButton1;
    int secondaryButton2;

    if (vr_switch_sticks) // Switch sticks should switch buttons too, or you can't move and jump easily
    {
        primaryButtonsNew = pOffTrackedRemoteNew->Buttons;
        primaryButtonsOld = pOffTrackedRemoteOld->Buttons;
        secondaryButtonsNew = pDominantTrackedRemoteNew->Buttons;
        secondaryButtonsOld = pDominantTrackedRemoteOld->Buttons;

        primaryButton1 = offButton1;
        primaryButton2 = offButton2;
        secondaryButton1 = domButton1;
        secondaryButton2 = domButton2;
    }
    else
    {
        primaryButtonsNew = pDominantTrackedRemoteNew->Buttons;
        primaryButtonsOld = pDominantTrackedRemoteOld->Buttons;
        secondaryButtonsNew = pOffTrackedRemoteNew->Buttons;
        secondaryButtonsOld = pOffTrackedRemoteOld->Buttons;

        primaryButton1 = domButton1;
        primaryButton2 = domButton2;
        secondaryButton1 = offButton1;
        secondaryButton2 = offButton2;
    }

    /*
        Theirs stops all of this while any menu is up, which is right for a
        menu on a virtual screen: nothing of the world is being shown, so
        nothing needs tracking.

        A menu hanging in the world is the other case. The hands are still
        in the picture, and a frozen pose does not merely stand still - 
        get_weapon_pos_and_angle subtracts the weapon's angle from the head's
        live one, so a still weapon swings as the head turns. Positional
        movement matters here too: GetViewShift reads it, and without it
        leaning does not move the eye.

        What stays behind the old gate is everything a button drives - the
        two-handed toggle and the snap turn - because the same presses are
        working the menu. Snap turn also resets hmdOrigin, which is the
        anchor the panel is pinned to.
    */
    if (menuactive == MENU_Off || VR_MenuInWorld())
    {
        float distance = sqrtf(powf(pOffTracking->Pose.position.x -
                                    pDominantTracking->Pose.position.x, 2) +
                               powf(pOffTracking->Pose.position.y -
                                    pDominantTracking->Pose.position.y, 2) +
                               powf(pOffTracking->Pose.position.z -
                                    pDominantTracking->Pose.position.z, 2));

        //Turn on weapon stabilisation?
        if (vr_two_handed_weapons && menuactive == MENU_Off &&
                (pOffTrackedRemoteNew->Buttons & xrButton_GripTrigger) !=
            (pOffTrackedRemoteOld->Buttons & xrButton_GripTrigger)) {

            if (pOffTrackedRemoteNew->Buttons & xrButton_GripTrigger) {
                if (distance < 0.50f) {
                    weaponStabilised = true;
                }
            } else {
                weaponStabilised = false;
            }
        }

        //dominant hand stuff first
        {
            ///Weapon location relative to view
            weaponoffset[0] = pDominantTracking->Pose.position.x - hmdPosition[0];
            weaponoffset[1] = pDominantTracking->Pose.position.y - hmdPosition[1];
            weaponoffset[2] = pDominantTracking->Pose.position.z - hmdPosition[2];

            vec2_t v;
            rotateAboutOrigin(weaponoffset[0], weaponoffset[2], hmdorientation[YAW], v);
            weaponoffset[0] = v[1];
            weaponoffset[2] = v[0];

            vec3_t rotation = {0};
            QuatToYawPitchRoll(pDominantTracking->Pose.orientation, rotation, rawcontrollerangles);

            //Set gun angles
            rotation[PITCH] = vr_weaponPitchAdjust;
            rotation[YAW] = -vr_weaponYawAdjust;
            QuatToYawPitchRoll(pDominantTracking->Pose.orientation, rotation, weaponangles);

            ALOGV("        weaponangles  PITCH: %.4f, YAW: %.4f, ROLL: %.4f",
                  weaponangles[PITCH],
                  weaponangles[YAW],
                  weaponangles[ROLL]);

/*            if (weaponStabilised) {
                float z = pOffTracking->Pose.position.z -
                          pDominantTracking->Pose.position.z;
                float x = pOffTracking->Pose.position.x -
                          pDominantTracking->Pose.position.x;
                float y = pOffTracking->Pose.position.y -
                          pDominantTracking->Pose.position.y;
                float zxDist = length(x, z);

                if (zxDist != 0.0f && z != 0.0f) {
                    VectorSet(weaponangles, -RAD2DEG(atanf(y / zxDist)), -RAD2DEG(atan2f(x, -z)),
                              weaponangles[ROLL]);
                }
            }*/
        }

        float controllerYawHeading = 0.0f;

        //off-hand stuff
        {
            offhandoffset[0] = pOffTracking->Pose.position.x - hmdPosition[0];
            offhandoffset[1] = pOffTracking->Pose.position.y - hmdPosition[1];
            offhandoffset[2] = pOffTracking->Pose.position.z - hmdPosition[2];

            vec2_t v;
            float yawRotation = getViewpointYaw() - hmdorientation[YAW];
            rotateAboutOrigin(offhandoffset[0], offhandoffset[2], -yawRotation, v);
            offhandoffset[0] = v[1];
            offhandoffset[2] = v[0];

            vec3_t rotation = {0};
            QuatToYawPitchRoll(pOffTracking->Pose.orientation, rotation, offhandangles);

            // The old yes/no switch, from a saved config or the console,
            // becomes "off-hand controller" once and hands over.
            if (vr_move_use_offhand) {
                vr_move_direction = 1;
                vr_move_use_offhand = false;
            }

            if (vr_move_direction == 1) {
                controllerYawHeading = offhandangles[YAW] - hmdorientation[YAW];
            } else if (vr_move_direction == 2) {
                /*
                    PC branch: "Off" - the stick walks the way the body faces.
                    Head direction is the baseline here (a heading of 0), so
                    taking the head's own yaw back out leaves the tracking
                    space's forward, which recenter sets and stick turning
                    turns: the same thing the off-hand line does with the
                    controller's yaw in place of zero.
                */
                controllerYawHeading = recenterYaw - hmdorientation[YAW];
            } else {
                controllerYawHeading = 0.0f;
            }
        }

        //Positional movement
        {
            vec2_t v;
            rotateAboutOrigin(positionDelta[0], positionDelta[2],
                              vrYaw + hmdorientation[YAW], v);
            positional_movementSideways = v[1];
            positional_movementForward = v[0];
        }

        //Off-hand specific stuff
        {
            //Apply a filter and quadratic scaler so small movements are easier to make
            //and we don't get movement jitter when the joystick doesn't quite center properly
            float dist = length(pSecondaryTrackedRemoteNew->Joystick.x, pSecondaryTrackedRemoteNew->Joystick.y);
            float nlf = nonLinearFilter(dist);
            dist = (dist > 1.0f) ? dist : 1.0f;
            float x = nlf * (pSecondaryTrackedRemoteNew->Joystick.x / dist);
            float y = nlf * (pSecondaryTrackedRemoteNew->Joystick.y / dist);

            //Apply a simple deadzone
            player_moving = (fabs(x) + fabs(y)) > 0.05f;
            x = player_moving ? x : 0;
            y = player_moving ? y : 0;

            //Adjust to be off-hand controller oriented
            vec2_t v;
            rotateAboutOrigin(x, y, controllerYawHeading, v);

            remote_movementSideways = v[0];
            remote_movementForward = v[1];
            ALOGV("        remote_movementSideways: %f, remote_movementForward: %f",
                  remote_movementSideways,
                  remote_movementForward);


            /*
                PC branch: smooth turning by time, not by frame.

                Theirs adds vr_snapTurn degrees every frame the stick is over,
                which on the Quest's fixed refresh was a speed; on PC it is
                3 degrees a frame at "Medium" - 270 a second at 90 Hz, 360 at
                120 - and it read as too fast (27 Sep 2026). Each smooth
                setting now turns vr_snapTurn x 40 degrees a SECOND: Slow 40,
                Medium 120 (every Game Or Die port's speed), Fast 240, Very
                Fast 360, at any refresh rate. Snap settings are unchanged.
            */
            static auto lastTurnTime = std::chrono::steady_clock::now();
            const auto turnNow = std::chrono::steady_clock::now();
            float turnSeconds = std::chrono::duration<float>(turnNow - lastTurnTime).count();
            lastTurnTime = turnNow;
            if (turnSeconds > 0.05f) turnSeconds = 0.05f;	// a hitch is not a lurch
            const bool smoothTurning = vr_snapTurn <= 10.0f;
            const float turnStep = smoothTurning ? vr_snapTurn * 40.0f * turnSeconds : (float)vr_snapTurn;

            // Not while a menu is up: the stick is working the menu, and
            // snap turn resets the origin the panel is anchored to.
            if (!dominantGripPushedNew && menuactive == MENU_Off)
            {
                // Turning logic
                static int increaseSnap = true;
                if (pPrimaryTrackedRemoteNew->Joystick.x > 0.6f) {
                    if (increaseSnap) {
                        VectorCopy(hmdPosition, hmdOrigin);
                        snapTurn -= turnStep;
                        if (vr_snapTurn > 10.0f) {
                            increaseSnap = false;
                        }

                        if (snapTurn < -180.0f) {
                            snapTurn += 360.f;
                        }
                    }
                } else if (pPrimaryTrackedRemoteNew->Joystick.x < 0.4f) {
                    increaseSnap = true;
                }

                static int decreaseSnap = true;
                if (pPrimaryTrackedRemoteNew->Joystick.x < -0.6f) {
                    if (decreaseSnap) {
                        VectorCopy(hmdPosition, hmdOrigin);
                        snapTurn += turnStep;

                        //If snap turn configured for less than 10 degrees
                        if (vr_snapTurn > 10.0f) {
                            decreaseSnap = false;
                        }

                        if (snapTurn > 180.0f) {
                            snapTurn -= 360.f;
                        }
                    }
                } else if (pPrimaryTrackedRemoteNew->Joystick.x > -0.4f) {
                    decreaseSnap = true;
                }
            }
        }
    }

    if (menuactive == MENU_On ||
            menuactive == MENU_OnNoPause)
    {
        /*
            PCVR port: hide the menu for a screenshot.

            Off-hand stick click hides it; any button on either hand brings
            it back. Both presses are kept from the menu, so hiding does not
            also activate something and coming back does not blindly pick
            whatever the cursor was on. Stick click was free to take - inside
            a menu nothing is mapped to it, and in play the off-hand click is
            Alt Weapon, which does not run here.

            Wrapped rather than returned from: the old/new controller states
            are copied after this function's blocks, and returning early
            would freeze the edge detection.
        */
        bool passToMenu = true;
        if (VR_MenuInWorld())
        {
            const bool clickOld = (pSecondaryTrackedRemoteOld->Buttons & xrButton_Joystick) != 0;
            const bool clickNew = (pSecondaryTrackedRemoteNew->Buttons & xrButton_Joystick) != 0;
            if (VR_MenuHidden())
            {
                const uint32_t pressed =
                    (pOffTrackedRemoteNew->Buttons | pDominantTrackedRemoteNew->Buttons) &
                    ~(pOffTrackedRemoteOld->Buttons | pDominantTrackedRemoteOld->Buttons);
                if (pressed != 0)
                    VR_MenuSetHidden(false);
                passToMenu = false;
            }
            else if (clickNew && !clickOld)
            {
                VR_MenuSetHidden(true);
                passToMenu = false;
            }
        }

        /*
            PC branch: hold the gun hand's grip to point at the menu; the
            trigger clicks (vr_pointer.cpp). The grip here has already waited
            out the recenter chord, so squeezing both never points.
        */
        {
            float u = 0.0f, v = 0.0f;
            const bool pointing = passToMenu &&
                (pDominantTrackedRemoteNew->Buttons & xrButton_GripTrigger) != 0 &&
                MenuPointerHit(pDominantTracking->Pose, &u, &v);
            VRPointer_Update(pointing, u, v,
                pointing && (pDominantTrackedRemoteNew->Buttons & xrButton_Trigger) != 0);
        }

        if (passToMenu)
        {
        ovrInputStateTrackedRemote* t[2][2] = { {pOffTrackedRemoteOld, pOffTrackedRemoteNew},
                                                {pDominantTrackedRemoteOld, pDominantTrackedRemoteNew} };
        for (auto &remote: t)
        {
            Joy_GenerateButtonEvents(
                    (remote[0]->Joystick.x > 0.7f && !dominantGripPushedOld ? 1 : 0),
                    (remote[1]->Joystick.x > 0.7f && !dominantGripPushedNew ? 1 : 0),
                    1, KEY_JOYAXIS1PLUS);

            Joy_GenerateButtonEvents(
                    (remote[0]->Joystick.x < -0.7f && !dominantGripPushedOld ? 1 : 0),
                    (remote[1]->Joystick.x < -0.7f && !dominantGripPushedNew ? 1 : 0),
                    1, KEY_JOYAXIS1MINUS);

            Joy_GenerateButtonEvents(
                    (remote[0]->Joystick.y < -0.7f && !dominantGripPushedOld ? 1 : 0),
                    (remote[1]->Joystick.y < -0.7f && !dominantGripPushedNew ? 1 : 0),
                    1, KEY_JOYAXIS2PLUS);

            Joy_GenerateButtonEvents(
                    (remote[0]->Joystick.y > 0.7f && !dominantGripPushedOld ? 1 : 0),
                    (remote[1]->Joystick.y > 0.7f && !dominantGripPushedNew ? 1 : 0),
                    1, KEY_JOYAXIS2MINUS);

            Joy_GenerateButtonEvents(
                    (remote[0]->Joystick.x > 0.7f && dominantGripPushedOld ? 1 : 0),
                    (remote[1]->Joystick.x > 0.7f && dominantGripPushedNew ? 1 : 0),
                    1, KEY_JOYAXIS5PLUS);

            Joy_GenerateButtonEvents(
                    (remote[0]->Joystick.x < -0.7f && dominantGripPushedOld ? 1 : 0),
                    (remote[1]->Joystick.x < -0.7f && dominantGripPushedNew ? 1 : 0),
                    1, KEY_JOYAXIS5MINUS);

            Joy_GenerateButtonEvents(
                    (remote[0]->Joystick.y < -0.7f && dominantGripPushedOld ? 1 : 0),
                    (remote[1]->Joystick.y < -0.7f && dominantGripPushedNew ? 1 : 0),
                    1, KEY_JOYAXIS6PLUS);

            Joy_GenerateButtonEvents(
                    (remote[0]->Joystick.y > 0.7f && dominantGripPushedOld ? 1 : 0),
                    (remote[1]->Joystick.y > 0.7f && dominantGripPushedNew ? 1 : 0),
                    1, KEY_JOYAXIS6MINUS);
        }

        Joy_GenerateButtonEvents(
                ((rightTrackedRemoteState_old.Buttons & xrButton_A) != 0) && !dominantGripPushedOld ? 1 : 0,
                ((rightTrackedRemoteState_new.Buttons & xrButton_A) != 0) && !dominantGripPushedNew ? 1 : 0,
                1, KEY_PAD_A);

        Joy_GenerateButtonEvents(
                ((rightTrackedRemoteState_old.Buttons & xrButton_B) != 0) && !dominantGripPushedOld ? 1 : 0,
                ((rightTrackedRemoteState_new.Buttons & xrButton_B) != 0) && !dominantGripPushedNew ? 1 : 0,
                1, KEY_PAD_B);

        Joy_GenerateButtonEvents(
                ((leftTrackedRemoteState_old.Buttons & xrButton_X) != 0) && !dominantGripPushedOld ? 1 : 0,
                ((leftTrackedRemoteState_new.Buttons & xrButton_X) != 0) && !dominantGripPushedNew ? 1 : 0,
                1, KEY_PAD_A);

        Joy_GenerateButtonEvents(
                ((leftTrackedRemoteState_old.Buttons & xrButton_Y) != 0) && !dominantGripPushedOld ? 1 : 0,
                ((leftTrackedRemoteState_new.Buttons & xrButton_Y) != 0) && !dominantGripPushedNew ? 1 : 0,
                1, KEY_PAD_B);
        }   // passToMenu
    }
    else
    {
        VRPointer_Update(false, 0.0f, 0.0f, false);	// no menu, no pointer

        //Dominant Hand - Primary keys (no grip pushed) - All keys are re-mappable, default bindngs are shown below
        {
            //Fire
            Joy_GenerateButtonEvents(
                    ((pDominantTrackedRemoteOld->Buttons & xrButton_Trigger) != 0) &&
                    !dominantGripPushedOld ? 1 : 0,
                    ((pDominantTrackedRemoteNew->Buttons & xrButton_Trigger) != 0) &&
                    !dominantGripPushedNew ? 1 : 0,
                    1, KEY_MOUSE1);

            //Jump
            Joy_GenerateButtonEvents(
                    ((primaryButtonsOld & primaryButton1) != 0) && !dominantGripPushedOld ? 1 : 0,
                    ((primaryButtonsNew & primaryButton1) != 0) && !dominantGripPushedNew ? 1 : 0,
                    1, KEY_SPACE);

            // Open Door
            Joy_GenerateButtonEvents(
                    ((primaryButtonsOld & primaryButton2) != 0) && !dominantGripPushedOld ? 1 : 0,
                    ((primaryButtonsNew & primaryButton2) != 0) && !dominantGripPushedNew ? 1 : 0,
                    1, KEY_PAD_A);

            /*
                PC branch: with the weapon wheel on, the gun hand's stick click
                opens it instead of crouching. Crouch was on two buttons - here
                and X - which a player pointed out, so X keeps it. Read on its
                state here rather than as a key: a key binding can be swallowed.
            */
            if (VRWheel_Enabled())
            {
                VRWheel_SetHeld(((pPrimaryTrackedRemoteNew->Buttons & xrButton_Joystick) != 0) &&
                                !dominantGripPushedNew);
            }
            else
            {
                VRWheel_SetHeld(false);
                //Crouch
                Joy_GenerateButtonEvents(
                        ((pPrimaryTrackedRemoteOld->Buttons & xrButton_Joystick) != 0) &&
                        !dominantGripPushedOld ? 1 : 0,
                        ((pPrimaryTrackedRemoteNew->Buttons & xrButton_Joystick) != 0) &&
                        !dominantGripPushedNew ? 1 : 0,
                        1, KEY_LCTRL);
            }

            //Just reserve this for the Quest which has a clearly defined thumbrest location
            if (strstr(gAppState.OpenXRHMD, "meta") != NULL || TBXR_IndexControllersActive())
            {
                //(Duke) Quick Kick
                Joy_GenerateButtonEvents(
                        ((pDominantTrackedRemoteOld->Touches & xrButton_ThumbRest) != 0) &&
                        !dominantGripPushedOld ? 1 : 0,
                        ((pDominantTrackedRemoteNew->Touches & xrButton_ThumbRest) != 0) &&
                        !dominantGripPushedNew ? 1 : 0,
                        1, KEY_PAD_B);
            }
        }

        //Dominant Hand - Secondary keys (grip pushed)
        {
            //(Duke) Quick Kick
            Joy_GenerateButtonEvents(
                    ((pDominantTrackedRemoteOld->Buttons & xrButton_Trigger) != 0) &&
                    dominantGripPushedOld ? 1 : 0,
                    ((pDominantTrackedRemoteNew->Buttons & xrButton_Trigger) != 0) &&
                    dominantGripPushedNew ? 1 : 0,
                    1, KEY_PAD_RSHOULDER);


            //Inventory Use
            Joy_GenerateButtonEvents(
                    ((primaryButtonsOld & primaryButton1) != 0) && dominantGripPushedOld ? 1 : 0,
                    ((primaryButtonsNew & primaryButton1) != 0) && dominantGripPushedNew ? 1 : 0,
                    1, KEY_ENTER);

            //Backsapce - Use this to undo any currently defined binds
            Joy_GenerateButtonEvents(
                    ((primaryButtonsOld & primaryButton2) != 0) && dominantGripPushedOld ? 1 : 0,
                    ((primaryButtonsNew & primaryButton2) != 0) && dominantGripPushedNew ? 1 : 0,
                    1, KEY_BACKSPACE);

            /*
                PC branch: grip + the gun hand's stick click opens the item
                wheel. The key it sent before had no binding in any game.
            */
            if (VRWheel_ItemsEnabled())
            {
                VRWheel_SetItemHeld(((pPrimaryTrackedRemoteNew->Buttons & xrButton_Joystick) != 0) &&
                                    dominantGripPushedNew);
            }
            else
            {
                VRWheel_SetItemHeld(false);
                //No Binding
                Joy_GenerateButtonEvents(
                        ((pPrimaryTrackedRemoteOld->Buttons & xrButton_Joystick) != 0) &&
                        dominantGripPushedOld ? 1 : 0,
                        ((pPrimaryTrackedRemoteNew->Buttons & xrButton_Joystick) != 0) &&
                        dominantGripPushedNew ? 1 : 0,
                        1, KEY_PAD_LSHOULDER);
            }

            //No Default Binding
            Joy_GenerateButtonEvents(
                    ((pDominantTrackedRemoteOld->Touches & xrButton_ThumbRest) != 0) &&
                    dominantGripPushedOld ? 1 : 0,
                    ((pDominantTrackedRemoteNew->Touches & xrButton_ThumbRest) != 0) &&
                    dominantGripPushedNew ? 1 : 0,
                    1, KEY_JOY6);
        }


        //Off Hand - Primary keys (no grip pushed)
        {
            //Alt Fire
            Joy_GenerateButtonEvents(
                    ((pOffTrackedRemoteOld->Buttons & xrButton_Trigger) != 0) &&
                    !dominantGripPushedOld ? 1 : 0,
                    ((pOffTrackedRemoteNew->Buttons & xrButton_Trigger) != 0) &&
                    !dominantGripPushedNew ? 1 : 0,
                    1, KEY_PAD_LTRIGGER);

            //No Binding
            Joy_GenerateButtonEvents(
                    ((secondaryButtonsOld & secondaryButton1) != 0) && !dominantGripPushedOld ? 1 : 0,
                    ((secondaryButtonsNew & secondaryButton1) != 0) && !dominantGripPushedNew ? 1 : 0,
                    1, KEY_PAD_LTHUMB);

            //Toggle Map
            Joy_GenerateButtonEvents(
                    ((secondaryButtonsOld & secondaryButton2) != 0) && !dominantGripPushedOld ? 1
                                                                                              : 0,
                    ((secondaryButtonsNew & secondaryButton2) != 0) && !dominantGripPushedNew ? 1
                                                                                              : 0,
                    1, KEY_TAB);

            //No Default Binding
            Joy_GenerateButtonEvents(
                    ((pSecondaryTrackedRemoteOld->Buttons & xrButton_Joystick) != 0) &&
                    !dominantGripPushedOld ? 1 : 0,
                    ((pSecondaryTrackedRemoteNew->Buttons & xrButton_Joystick) != 0) &&
                    !dominantGripPushedNew ? 1 : 0,
                    1, KEY_PAD_RTHUMB);

            //No Default Binding
            Joy_GenerateButtonEvents(
                    ((pOffTrackedRemoteOld->Touches & xrButton_ThumbRest) != 0) &&
                    !dominantGripPushedOld ? 1 : 0,
                    ((pOffTrackedRemoteNew->Touches & xrButton_ThumbRest) != 0) &&
                    !dominantGripPushedNew ? 1 : 0,
                    1, KEY_JOY7);

            // "Run"
            Joy_GenerateButtonEvents(
                    ((pOffTrackedRemoteOld->Buttons & xrButton_GripTrigger) != 0) &&
                    !dominantGripPushedOld ? 1 : 0,
                    ((pOffTrackedRemoteNew->Buttons & xrButton_GripTrigger) != 0) &&
                    !dominantGripPushedNew ? 1 : 0,
                    1, KEY_LSHIFT);
        }

        //Off Hand - Secondary keys (grip pushed)
        {
            //No Default Binding
            Joy_GenerateButtonEvents(
                    ((pOffTrackedRemoteOld->Buttons & xrButton_Trigger) != 0) &&
                    dominantGripPushedOld ? 1 : 0,
                    ((pOffTrackedRemoteNew->Buttons & xrButton_Trigger) != 0) &&
                    dominantGripPushedNew ? 1 : 0,
                    1, KEY_LALT);

            //Move Down
            Joy_GenerateButtonEvents(
                    ((secondaryButtonsOld & secondaryButton1) != 0) && dominantGripPushedOld ? 1
                                                                                             : 0,
                    ((secondaryButtonsNew & secondaryButton1) != 0) && dominantGripPushedNew ? 1
                                                                                             : 0,
                    1, KEY_PGDN);

            //Move Up
            Joy_GenerateButtonEvents(
                    ((secondaryButtonsOld & secondaryButton2) != 0) && dominantGripPushedOld ? 1
                                                                                             : 0,
                    ((secondaryButtonsNew & secondaryButton2) != 0) && dominantGripPushedNew ? 1
                                                                                             : 0,
                    1, KEY_PGUP);

            //Land
            Joy_GenerateButtonEvents(
                    ((pSecondaryTrackedRemoteOld->Buttons & xrButton_Joystick) != 0) &&
                    dominantGripPushedOld ? 1 : 0,
                    ((pSecondaryTrackedRemoteNew->Buttons & xrButton_Joystick) != 0) &&
                    dominantGripPushedNew ? 1 : 0,
                    1, KEY_HOME);

            //No Default Binding
            Joy_GenerateButtonEvents(
                    ((pOffTrackedRemoteOld->Touches & xrButton_ThumbRest) != 0) &&
                    dominantGripPushedOld ? 1 : 0,
                    ((pOffTrackedRemoteNew->Touches & xrButton_ThumbRest) != 0) &&
                    dominantGripPushedNew ? 1 : 0,
                    1, KEY_JOY8);

#ifdef DEBUG
            if (dominantGripPushedOld &&
                    !(pOffTrackedRemoteOld->Buttons & xrButton_GripTrigger) &&
                    (pOffTrackedRemoteNew->Buttons & xrButton_GripTrigger))
            {
                C_DoCommand ("give all");
            }

#else
            //No Default Binding
            Joy_GenerateButtonEvents(
                    ((pOffTrackedRemoteOld->Buttons & xrButton_GripTrigger) != 0) &&
                    dominantGripPushedOld && !vr_two_handed_weapons ? 1 : 0,
                    ((pOffTrackedRemoteNew->Buttons & xrButton_GripTrigger) != 0) &&
                    dominantGripPushedNew && !vr_two_handed_weapons ? 1 : 0,
                    1, KEY_PAD_DPAD_UP);
#endif
        }

        Joy_GenerateButtonEvents(
                (pSecondaryTrackedRemoteOld->Joystick.x > 0.7f && !dominantGripPushedOld ? 1 : 0),
                (pSecondaryTrackedRemoteNew->Joystick.x > 0.7f && !dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS1PLUS);

        Joy_GenerateButtonEvents(
                (pSecondaryTrackedRemoteOld->Joystick.x < -0.7f && !dominantGripPushedOld ? 1 : 0),
                (pSecondaryTrackedRemoteNew->Joystick.x < -0.7f && !dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS1MINUS);

        Joy_GenerateButtonEvents(
                (pPrimaryTrackedRemoteOld->Joystick.x > 0.7f && !dominantGripPushedOld ? 1 : 0),
                (pPrimaryTrackedRemoteNew->Joystick.x > 0.7f && !dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS3PLUS);

        Joy_GenerateButtonEvents(
                (pPrimaryTrackedRemoteOld->Joystick.x < -0.7f && !dominantGripPushedOld ? 1 : 0),
                (pPrimaryTrackedRemoteNew->Joystick.x < -0.7f && !dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS3MINUS);

        Joy_GenerateButtonEvents(
                (pSecondaryTrackedRemoteOld->Joystick.y < -0.7f && !dominantGripPushedOld ? 1 : 0),
                (pSecondaryTrackedRemoteNew->Joystick.y < -0.7f && !dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS2MINUS);

        Joy_GenerateButtonEvents(
                (pSecondaryTrackedRemoteOld->Joystick.y > 0.7f && !dominantGripPushedOld ? 1 : 0),
                (pSecondaryTrackedRemoteNew->Joystick.y > 0.7f && !dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS2PLUS);

        //weapprev
        Joy_GenerateButtonEvents(
                (pPrimaryTrackedRemoteOld->Joystick.y < -0.7f && !dominantGripPushedOld ? 1 : 0),
                (pPrimaryTrackedRemoteNew->Joystick.y < -0.7f && !dominantGripPushedNew ? 1 : 0),
                1, KEY_MWHEELDOWN);

        //weapnext
        Joy_GenerateButtonEvents(
                (pPrimaryTrackedRemoteOld->Joystick.y > 0.7f && !dominantGripPushedOld ? 1 : 0),
                (pPrimaryTrackedRemoteNew->Joystick.y > 0.7f && !dominantGripPushedNew ? 1 : 0),
                1, KEY_MWHEELUP);

        Joy_GenerateButtonEvents(
                (pSecondaryTrackedRemoteOld->Joystick.x > 0.7f && dominantGripPushedOld ? 1 : 0),
                (pSecondaryTrackedRemoteNew->Joystick.x > 0.7f && dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS5PLUS);

        Joy_GenerateButtonEvents(
                (pSecondaryTrackedRemoteOld->Joystick.x < -0.7f && dominantGripPushedOld ? 1 : 0),
                (pSecondaryTrackedRemoteNew->Joystick.x < -0.7f && dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS5MINUS);

        Joy_GenerateButtonEvents(
                (pPrimaryTrackedRemoteOld->Joystick.x > 0.7f && dominantGripPushedOld ? 1 : 0),
                (pPrimaryTrackedRemoteNew->Joystick.x > 0.7f && dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS7PLUS);

        Joy_GenerateButtonEvents(
                (pPrimaryTrackedRemoteOld->Joystick.x < -0.7f && dominantGripPushedOld ? 1 : 0),
                (pPrimaryTrackedRemoteNew->Joystick.x < -0.7f && dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS7MINUS);

        Joy_GenerateButtonEvents(
                (pSecondaryTrackedRemoteOld->Joystick.y < -0.7f && dominantGripPushedOld ? 1 : 0),
                (pSecondaryTrackedRemoteNew->Joystick.y < -0.7f && dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS6MINUS);

        Joy_GenerateButtonEvents(
                (pSecondaryTrackedRemoteOld->Joystick.y > 0.7f && dominantGripPushedOld ? 1 : 0),
                (pSecondaryTrackedRemoteNew->Joystick.y > 0.7f && dominantGripPushedNew ? 1 : 0),
                1, KEY_JOYAXIS6PLUS);

        //invnext
        Joy_GenerateButtonEvents(
                (pPrimaryTrackedRemoteOld->Joystick.y < -0.7f && dominantGripPushedOld ? 1 : 0),
                (pPrimaryTrackedRemoteNew->Joystick.y < -0.7f && dominantGripPushedNew ? 1 : 0),
                1, KEY_PAD_DPAD_RIGHT);

        //invprev
        Joy_GenerateButtonEvents(
                (pPrimaryTrackedRemoteOld->Joystick.y > 0.7f && dominantGripPushedOld ? 1 : 0),
                (pPrimaryTrackedRemoteNew->Joystick.y > 0.7f && dominantGripPushedNew ? 1 : 0),
                1, KEY_PAD_DPAD_LEFT);
    }

    //Save state
    rightTrackedRemoteState_old = rightTrackedRemoteState_new;
    leftTrackedRemoteState_old = leftTrackedRemoteState_new;
}