/*
	vr_wheel.cpp - PC branch. See vr_wheel.h.

	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version. See package/common/gpl-2.0.txt.
*/

#include "vr_wheel.h"
#include "vr_weapons.h"

#include "build.h"
#include "coreactor.h"
#include "gamefuncs.h"
#include "gamecontrol.h"
#include "gamestate.h"
#include "gamestruct.h"
#include "hw_drawinfo.h"
#include "menustate.h"
#include "printf.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_net.h"
#include "i_time.h"
#include "automap.h"

#include <math.h>
#include <chrono>

float vr_hunits_per_meter();
extern float weaponoffset[3];	// the gun hand relative to the headset, metres
bool TBXR_VREnabled();

EXTERN_CVAR(Float, i_timescale)
EXTERN_CVAR(Float, vr_weapon_scale)

CVAR(Bool, vr_weapon_wheel, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Game speed while the wheel is open. 1 is no slow motion.
CVAR(Float, vr_wheel_slowmo, 0.3f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

/*
	The ring's radius in metres, and each model's size as a fraction of the
	held weapon's. First tried at 0.17 m and 0.8: in the headset the pistol and
	the rocket launcher alone filled most of the view. Smaller models on a
	wider ring. New names rather than new defaults for the old ones, because
	the test config already holds the old numbers and a saved value beats a
	new default; the old names never shipped.
*/
CVAR(Float, vr_wheel_ring_radius, 0.22f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_wheel_model_scale, 0.35f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

/*
	Desk only, and never saved: opens the ring with no headset, in front of
	the eye, so it can be screenshotted. 1 opens it with nothing chosen; 2
	and up point the hand at entry N-2.
*/
CVAR(Int, vr_wheel_debug, 0, 0)

static constexpr int kMaxEntries = 16;
static constexpr double kDeadzoneMetres = 0.06;

struct WheelItem
{
	int slot;
	int tile;			// voxel tile of the model
	float modelYaw;		// the model's own facing correction
	bool current;
	FString name;
};

static bool Held = false;
static bool Open = false;
static bool TimescaleChanged = false;
static float SavedTimescale = 1.0f;
static int Highlight = -1;
static TArray<WheelItem> Items;

// The ring's plane, fixed in the world when it opens: its centre, and the
// world directions of its "across" and "up".
static DVector3 CentreW;
static DVector3 AcrossW;
static DVector3 UpW;

bool VRWheel_Enabled() { return vr_weapon_wheel; }
bool VRWheel_IsOpen()  { return Open; }

//==========================================================================
//
// The one transform.
//
// Hand-frame metres - across, forward, up, relative to the headset - into
// Build world coordinates. It is the held weapon's own placement
// (VRWeapons_AddSprite) minus that weapon's tuning nudges, so a point here is
// exactly where the hand is drawn.
//
// The ring is drawn through it AND the hand is read through it. That is the
// point: whatever sign the "across" axis has, moving the hand towards a model
// moves it towards that model, because both came out of the same arithmetic.
// A flipped sign could only mirror the ring's order, never the selection.
//
//==========================================================================

static DVector3 HandToWorld(const FRenderViewpoint& vp, double across, double forward, double up)
{
	const double hupm = vr_hunits_per_meter();
	const DAngle yaw = VRWeapons_DrawnYaw(vp);	// as the held weapon is placed

	DVector3 pos(vp.Pos.X, -vp.Pos.Y, -vp.Pos.Z);	// render space back to Build
	DVector2 xy(across * hupm, forward * hupm);
	xy = xy.Rotated(-DAngle90 + yaw);
	pos.X -= xy.X;
	pos.Y -= xy.Y;
	pos.Z -= up * hupm;								// Build Z runs downwards
	return pos;
}

// Where the hand is now, in the hand frame. On the desk there is no hand, so
// the debug mode supplies one.
static void HandNow(double& across, double& forward, double& up)
{
	if (vr_wheel_debug > 0)
	{
		across = 0.0;
		forward = 0.45;
		up = -0.05;
		return;
	}
	// Matches get_weapon_pos_and_angle: x = weaponoffset[2], y = weaponoffset[0],
	// and VRWeapons_AddSprite treats the first as across and the second as forward.
	across = weaponoffset[2];
	forward = weaponoffset[0];
	up = weaponoffset[1];
}

//==========================================================================
//
// When the wheel may be up at all.
//
//==========================================================================

static bool CanOpen()
{
	if (!vr_weapon_wheel) return false;
	if (gamestate != GS_LEVEL) return false;
	if (menuactive != MENU_Off || paused) return false;
	// The full map draws no 3D view, so VRWheel_Update would not run to
	// close the wheel - it stayed up, slowed, for as long as the map did.
	if (automapMode == am_full) return false;
	if (vr_wheel_debug > 0) return true;
	return TBXR_VREnabled();
}

/*
	Slow motion is the engine's own i_timescale, which re-bases the game clock
	so game time runs on without a jump (measured: a steady 30 tics a second in
	Duke either side of the ring, about a third of that while it is up).

	The input clock is not re-based with it. I_GetInputFrac - the frame's scale
	for turning and other unsynchronised input - measures from the last frame
	on the raw scaled clock, which jumps when the scale changes: backwards on
	opening, forwards by most of the time since the game started on closing.
	One frame of turning scaled by that is a snap spin. Resetting the input
	time straight after each change gives that frame an ordinary scale.
*/
static void SetSlowMotion(bool on)
{
	if (on)
	{
		const float want = clamp((float)vr_wheel_slowmo, 0.05f, 1.0f);
		if (want >= 0.999f || netgame) return;
		SavedTimescale = i_timescale;
		i_timescale = want;
		I_ResetInputTime();
		TimescaleChanged = true;
	}
	else if (TimescaleChanged)
	{
		i_timescale = SavedTimescale;
		I_ResetInputTime();
		TimescaleChanged = false;
	}
}

static void Close(bool select)
{
	if (!Open) return;
	Open = false;
	SetSlowMotion(false);

	/*
		Choosing the weapon already in hand keeps it. Every game treats "slot N"
		for the slot you hold as a toggle - Duke's shrinker to expander, Shadow
		Warrior's modes, Blood's alternates - so sending it would switch away
		from the one weapon the player just chose.
	*/
	if (select && Highlight >= 0 && Highlight < (int)Items.Size() && !Items[Highlight].current)
	{
		/*
			The engine's own "slot N". Every game already turns that into the
			right weapon, shared slots and alternates included - pressing it
			on the shrinker's slot in Duke is exactly what pressing 7 does.
		*/
		FString cmd;
		cmd.Format("slot %d", Items[Highlight].slot);
		DPrintf(DMSG_NOTIFY, "VR wheel: %s -> %s\n", cmd.GetChars(), Items[Highlight].name.GetChars());
		C_DoCommand(cmd.GetChars());
	}
	Highlight = -1;
}

void VRWheel_SetHeld(bool held)
{
	// Let go - including by a menu opening - never leaves the game slowed.
	if (Open && !CanOpen()) Close(false);
	/*
		Letting go chooses here, on the input frame, as well as in
		VRWheel_Update: that one runs only when the 3D view is drawn, and a
		game can skip the view - Duke's security cameras, a player outside
		any sector. Only a real release: the desk debug mode opens the wheel
		with nothing held, and must not be closed by not holding.
	*/
	if (Open && Held && !held) Close(CanOpen());
	Held = held;
}

//==========================================================================
//
// Reading the game's slots into models.
//
//==========================================================================

static bool Gather()
{
	Items.Clear();
	VRWheelEntry raw[kMaxEntries];
	const int n = gi ? gi->VRWheelEntries(raw, kMaxEntries) : 0;
	int skipped = 0;
	for (int i = 0; i < n; i++)
	{
		const int tile = VRWeapons_ModelTile(raw[i].model);
		if (tile < 0) { skipped++; continue; }	// no model to show; stick up/down still reaches it
		WheelItem it;
		it.slot = raw[i].slot;
		it.tile = tile;
		it.modelYaw = VRWeapons_ModelYawFor(raw[i].model);
		it.current = raw[i].current;
		it.name = raw[i].model;
		Items.Push(it);
	}
	DPrintf(DMSG_NOTIFY, "VR wheel: %d weapons owned, %d on the wheel, %d without a model\n", n, Items.Size(), skipped);
	return Items.Size() > 0;
}

// Item i's angle on the ring: the first at the top, the rest spaced evenly.
static double ItemAngle(int i)
{
	return 90.0 - 360.0 * i / Items.Size();
}

//==========================================================================
//
// Per frame.
//
//==========================================================================

static void RunSelfTestStep();
static int SelfTest = -1;
static DAngle WheelYaw;		// the frame's drawn yaw, for the models' facing

/*
	Once a frame, from the frame's own head viewpoint - see vr_wheel.h.

	This used to run from VRWheel_AddSprites, which is called once for every
	scene pass: each eye, and each mirror, water or sky portal. Each eye's
	camera sits half an IPD to one side and each portal's somewhere else
	entirely, and the hand was read against whichever pass came last. In
	Blood, full of portals, the highlight was decided from a viewpoint that
	was not the head, and it would not leave the first weapon (27 Sep 2026).
*/
void VRWheel_Update(const FRenderViewpoint& vp)
{
	if (SelfTest >= 0) RunSelfTestStep();

	const bool want = CanOpen() && (Held || vr_wheel_debug > 0);

	if (!want)
	{
		// Released with the wheel up: that is the choice. Closed for any other
		// reason - a menu, a level change - chooses nothing.
		Close(Open && !Held && CanOpen());
		return;
	}

	if (vp.CameraActor == nullptr) return;
	// Duke's security cameras draw the world from the camera, with the viewer
	// hidden while they do (SetupViewpoint's renderingRemoteCamera test). That
	// is not where the hand is, so the ring is neither placed nor read from it.
	if (vp.CameraActor->spr.cstat & CSTAT_SPRITE_INVISIBLE) return;

	const double hupm = vr_hunits_per_meter();
	WheelYaw = VRWeapons_DrawnYaw(vp);

	if (!Open)
	{
		if (!Gather()) return;

		double a, f, u;
		HandNow(a, f, u);
		CentreW = HandToWorld(vp, a, f, u);
		AcrossW = HandToWorld(vp, a + 1.0, f, u) - CentreW;
		AcrossW.MakeUnit();
		UpW = DVector3(0, 0, -1);	// world up, in Build's downward Z
		Open = true;
		Highlight = -1;
		SetSlowMotion(true);
	}

	// Which model the hand is nearest, in the ring's own plane.
	{
		int want_highlight = -1;
		if (vr_wheel_debug >= 2)
		{
			want_highlight = (vr_wheel_debug - 2) % (int)Items.Size();
		}
		else
		{
			double a, f, u;
			HandNow(a, f, u);
			const DVector3 d = HandToWorld(vp, a, f, u) - CentreW;
			const double x = (d.X * AcrossW.X + d.Y * AcrossW.Y + d.Z * AcrossW.Z) / hupm;
			const double y = (d.X * UpW.X + d.Y * UpW.Y + d.Z * UpW.Z) / hupm;
			if (sqrt(x * x + y * y) >= kDeadzoneMetres)
			{
				const double ang = atan2(y, x) * (180.0 / M_PI);
				double best = 1e9;
				for (unsigned i = 0; i < Items.Size(); i++)
				{
					double diff = fabs(fmod(ang - ItemAngle(i) + 540.0, 360.0) - 180.0);
					if (diff < best) { best = diff; want_highlight = i; }
				}
			}
		}
		Highlight = want_highlight;
	}
}

// Every scene pass: draws what VRWheel_Update decided, at its fixed place in
// the world, so every eye and every portal sees the same ring.
void VRWheel_AddSprites(tspriteArray& tsprites, const FRenderViewpoint& vp)
{
	if (!Open) return;
	auto owner = vp.CameraActor;
	if (owner == nullptr) return;
	const double hupm = vr_hunits_per_meter();

	// The models.
	const DAngle viewYaw = WheelYaw;
	/*
		The renderer gives every one of our tiles the HELD weapon's facing
		correction (VRWeapons_ModelYaw, applied in HWSprite::ProcessVoxel after
		pitch and roll). With no pitch or roll every rotation here is about the
		same axis, so each model cancels the held weapon's correction and adds
		its own - exact, and it leaves the headset-validated held weapon alone.
	*/
	const float heldYaw = VRWeapons_ModelYaw();
	const double radius = vr_wheel_ring_radius;

	for (unsigned i = 0; i < Items.Size(); i++)
	{
		const WheelItem& it = Items[i];
		const double rad = ItemAngle(i) * (M_PI / 180.0);
		const DVector3 pos = CentreW + (AcrossW * cos(rad) + UpW * sin(rad)) * (radius * hupm);

		auto sectp = owner->sector();
		updatesector(pos.XY(), &sectp);
		if (sectp == nullptr) sectp = owner->sector();

		const bool lit = (int)i == Highlight;
		const double size = vr_weapon_scale * vr_wheel_model_scale * (lit ? 1.35 : 1.0);

		auto tspr = tsprites.newTSprite();
		*tspr = {};
		tspr->ownerActor = owner;
		tspr->pos = pos;
		tspr->sectp = sectp;
		tspr->picnum = it.tile;
		tspr->shade = lit ? -24 : (it.current ? 0 : 12);
		tspr->pal = 0;
		tspr->scale = DVector2(size, size);
		// Side on, facing across the player, as the held weapon would look from beside it.
		tspr->Angles.Yaw = viewYaw + DAngle90 + DAngle::fromDeg(it.modelYaw - heldYaw);
		tspr->Angles.Pitch = nullAngle;
		tspr->Angles.Roll = nullAngle;
		tspr->statnum = MAXSTATUS;
		tspr->cstat2 = CSTAT2_SPRITE_NOANIMATE;
		tspr->cstat = CSTAT_SPRITE_YCENTER;
	}
}

//==========================================================================
//
// Diagnostic: what the wheel would show here and now, with no headset.
//
// Lists the game's own answer - every slot it says you own and the model it
// names - and what the wheel made of it. Run after "give weapons" and a
// "slot N" to check the entry marked current is slot N: that proves this
// game's mapping from slot to weapon agrees with the game's own.
//
//==========================================================================

CCMD(vrwheel)
{
	if (!gi) return;
	VRWheelEntry raw[kMaxEntries];
	const int n = gi->VRWheelEntries(raw, kMaxEntries);
	Printf("VR wheel: %d owned slot(s), wheel %s, slow motion %.2f, game tic %d\n", n,
		vr_weapon_wheel ? "on" : "off", (float)vr_wheel_slowmo, I_GetTime(GameTicRate));
	for (int i = 0; i < n; i++)
	{
		const int tile = VRWeapons_ModelTile(raw[i].model);
		Printf("  slot %2d  %-18s tile %5d  %s%s\n", raw[i].slot,
			raw[i].model && raw[i].model[0] ? raw[i].model : "(no name)",
			tile, tile < 0 ? "NO MODEL - not on the wheel" : "on the wheel",
			raw[i].current ? "   <- held" : "");
	}
}

//==========================================================================
//
// Desk self-test: the whole wheel, in a real level, with no headset.
//
// Raze has no "wait" command, so a chain of +commands on the command line all
// run in the first instant, before any level exists - which made a first desk
// run report 0 weapons in every game and screenshot a black frame. This is
// driven from the scene's own drawing instead, so every step happens in a
// level by construction, and each step waits real time: a flat desk run draws
// hundreds of frames a second, and a count of frames finished before a weapon
// switch did.
//
//   give all; slot 3; list          - the entry marked held must be slot 3
//   slot 5; list                    - and now slot 5
//   open the ring on entry 0; list  - the game tic runs at the slow-motion rate
//   choose it                       - the wheel's own close-and-choose
//   list, three times               - entry 0's slot is held, and the tic runs
//                                     at full rate again with no jump
//
// Passed in all six game families on 26 Sep 2026. It proves the choosing,
// not the look: a flat desk run's screenshots come back black, so how the
// ring looks is for the headset.
//
// Commands go through the console buffer, so they run at the top of a frame
// rather than in the middle of drawing one.
//
//==========================================================================

static uint64_t SelfTestStart;

static void RunSelfTestStep()
{
	// Wall-clock time, not I_msTime: the engine's clock is scaled by the slow
	// motion under test, and would jump when the ring opens.
	const uint64_t now = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
	if (SelfTest == 0) SelfTestStart = now;
	const uint64_t ms = now - SelfTestStart;

	// Milliseconds after the first frame; each step runs once, in order.
	static const int at[] = { 0, 1500, 3500, 3700, 5700, 5900, 6900, 7900, 8400, 9900, 11900, 12100 };
	if (SelfTest >= (int)countof(at) || ms < (uint64_t)at[SelfTest]) return;

	switch (SelfTest++)
	{
	case 0: AddCommandString("give all"); break;
	case 1: AddCommandString("slot 3"); break;
	case 2: Printf("VR wheel selftest: after slot 3\n"); C_DoCommand("vrwheel"); break;
	case 3: AddCommandString("slot 5"); break;
	case 4: Printf("VR wheel selftest: after slot 5\n"); C_DoCommand("vrwheel"); break;
	case 5: vr_wheel_debug = 2; break;
	case 6: Printf("VR wheel selftest: ring open\n"); C_DoCommand("vrwheel"); break;
	case 7:
		Printf("VR wheel selftest: releasing on entry 0 (%s, slot %d)\n",
			Items.Size() ? Items[0].name.GetChars() : "none", Items.Size() ? Items[0].slot : -1);
		// The wheel's own close-and-choose. Letting go by clearing the debug
		// hand cannot stand in for it at the desk: with no headset and no
		// debug hand the wheel is not allowed up at all, and closes choosing
		// nothing, as it must for a menu opening.
		Close(true);
		vr_wheel_debug = 0;
		break;
	case 8:
	case 9:
	case 10: Printf("VR wheel selftest: after release\n"); C_DoCommand("vrwheel"); break;
	case 11: Printf("VR wheel selftest DONE\n"); SelfTest = -1; break;
	}
}

CCMD(vrwheel_selftest)
{
	SelfTest = 0;
	Printf("VR wheel selftest: armed, starts with the first rendered frame of a level\n");
}
