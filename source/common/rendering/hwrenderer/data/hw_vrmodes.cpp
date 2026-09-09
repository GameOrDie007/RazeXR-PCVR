/*
** hw_vrmodes.cpp
** Matrix handling for stereo 3D rendering
**
**---------------------------------------------------------------------------
** Copyright 2015 Christopher Bruns
** Copyright 2016-2021 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/

#include "vectors.h" // RAD2DEG
#include "hw_cvars.h"
#include "hw_vrmodes.h"
#include "v_video.h"
#include "v_draw.h"
#include "version.h"
#include "i_interface.h"
#include "RazeXR/mathlib.h"
#include "menustate.h"
#include "gamestate.h"

extern vec3_t hmdPosition;
extern vec3_t hmdOrigin;
extern vec3_t hmdorientation;
extern vec3_t weaponangles;
extern vec3_t rawcontrollerangles; // angles unadjusted by weapon adjustment cvars

float RazeXR_GetFOV();
bool TBXR_VREnabled();
void VR_GetMove(float *joy_forward, float *joy_side, float *hmd_forward, float *hmd_side, float *up,
				float *yaw, float *pitch, float *roll);

void get_weapon_pos_and_angle(float &x, float &y, float &z, float &pitch, float &yaw);

// Set up 3D-specific console variables:
CVAR(Int, vr_mode, 15, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)

// switch left and right eye views
CVAR(Bool, vr_swap_eyes, false, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)


// intraocular distance in meters
CVAR(Float, vr_ipd, 0.062f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // METERS

// distance between viewer and the display screen
CVAR(Float, vr_screendist, 0.80f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS

//If the following is 0 then it uses the default for the game, this gives player the opportunity to override it themselves
CVAR(Float, vr_units_per_meter, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS

CVAR(Float, vr_height_adjust, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS
CVAR(Int, vr_control_scheme, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
CVAR(Bool, vr_move_use_offhand, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weaponPitchAdjust, 20.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weaponYawAdjust, 6.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_allowPitchOverride, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
/*
	PC branch: smooth turn by default, where theirs snaps 45 degrees.

	The value is not a style flag - it is degrees, and the turning code treats
	anything over 10 as a snap by latching the stick so it fires once per push,
	while 10 or under repeats every frame and so turns smoothly. 3 is the
	"Smooth Medium" preset in the Turning Mode menu.

	A deliberate divergence, asked for. Note it is degrees per *frame* and so
	scales with refresh rate, which is theirs and is left alone.
*/
CVAR(Float, vr_snapTurn, 3.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, vr_move_speed, 19, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_run_multiplier, 1.5, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_switch_sticks, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_secondary_button_mappings, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_two_handed_weapons, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_positional_tracking, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_crouch_use_button, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_6dof_weapons, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_6dof_crosshair, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CVAR(Float, vr_pickup_haptic_level, 0.2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_quake_haptic_level, 0.8, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

//HUD control
CVAR(Float, vr_hud_scale, 0.5f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_hud_stereo, 2.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_hud_rotate, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_hud_fixed_pitch, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_hud_fixed_roll, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

/*
	PCVR port: the pause menu as a panel in the world.

	When a menu opens inside a level, theirs switches the compositor to a quad
	layer: the whole eye buffer - paused level and menu together - becomes a
	virtual screen four metres ahead, and the projection layers stop. The menu
	was never in the world; it was a television showing a picture of it. And
	that screen was placed with its centre at a fixed 1.0 m while eyes sit
	around 1.6, so its bottom rows fell below comfortable view - which is
	where Quit went.

	Head tracking is applied at render time, through VR_GetMove, not by the
	game tick - so the paused world can be drawn in stereo and looked around.
	That is what makes this possible: the projection layers stay up, the level
	keeps rendering around you, and the 2D layer is projected onto a panel the
	same way the HUD already is, anchored where you were looking when the
	menu opened. vr_menu_world 0 restores the virtual screen exactly as before.
*/
CVAR(Bool, vr_menu_world, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_menu_lock, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)      // stays where opened, or follows the head
CVAR(Float, vr_menu_scale, 0.75f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)    // half-width in metres, like vr_hud_scale
CVAR(Float, vr_menu_distance, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)  // metres ahead - the eye-painted fallback
/*
	Depth of the panel when it is a compositor layer. Quake II's 3.5 m, and
	the main menu's own screen layer is 4 m. Not 1 m: a panel that close moves
	about six degrees against the far world for the ten centimetres a head
	turn carries the eyes, which is correct optics and reads as the menu
	swinging. At 3.5 m the same turn gives a degree and a half. Angular size
	is preserved by widening the panel with the distance.
*/
CVAR(Float, vr_menu_depth, 3.5f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

/*
	UNUSED on the normal path - see TBXR_PC.cpp, where the panel is a
	compositor layer placed from the runtime's own pose. These remain only
	for the fallback below, which draws the menu into the eye texture when a
	menu swapchain could not be created.

	Which way the panel answers head movement, one axis at a time.

	The panel is pinned by translating it by the head's displacement since
	the menu opened, and that displacement has to be expressed in the frame
	the matrix below is working in. Two attempts at deriving those signs
	from the code left a residual sway, and a sway is not something the
	person writing the matrix can see. So each axis is a dial: -1, 0 or 1.
	0 makes that axis ignore head movement, which is what it did before any
	of this. Turn whichever axis still drifts until it stops.

	Temporary. Once the three are known they become constants and the dials
	go - they exist to spend one session in a headset instead of one build
	per guess.
*/
CVAR(Int, vr_menu_lockx, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, vr_menu_locky,  1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, vr_menu_lockz, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

bool VR_MenuInWorld()
{
	// The eye-count check is what keeps -novr and a PC with no headset on
	// the old path: GetVRMode answers mono for both.
	return vr_menu_world
		&& gamestate == GS_LEVEL
		&& menuactive != MENU_Off
		&& VRMode::GetVRMode(true)->mEyeCount == 2;
}

// Where the head was looking on the first frame the menu was up. Forgotten
// when it closes, so the next one opens wherever you are looking then.
static bool  menuAnchored = false;
static bool  menuHidden = false;
static float menuAnchorYaw = 0.f;
static float menuAnchorPitch = 0.f;
static vec3_t menuAnchorPos = { 0.f, 0.f, 0.f };

void VR_MenuAnchorUpdate()
{
	if (!VR_MenuInWorld())
	{
		menuAnchored = false;
		menuHidden = false;	// the next pause always opens visible
		return;
	}
	if (!menuAnchored)
	{
		menuAnchorYaw = hmdorientation[YAW];
		menuAnchorPitch = hmdorientation[PITCH];
		VectorCopy(hmdPosition, menuAnchorPos);
		menuAnchored = true;
	}
}

float playerHeight = 0;
float heightScaler = 1.0f;

extern int g_gameType;

enum
{
	GAMEFLAG_DUKE       = 0x00000001,
	GAMEFLAG_NAM        = 0x00000002,
	GAMEFLAG_NAPALM     = 0x00000004,
	GAMEFLAG_WW2GI      = 0x00000008,
	GAMEFLAG_ADDON      = 0x00000010,
	GAMEFLAG_SHAREWARE  = 0x00000020,
	GAMEFLAG_DUKEBETA   = 0x00000060, // includes 0x20 since it's a shareware beta
	GAMEFLAG_PLUTOPAK	= 0x00000080,
	GAMEFLAG_RR         = 0x00000100,
	GAMEFLAG_RRRA       = 0x00000200,
	GAMEFLAG_RRALL		= GAMEFLAG_RR | GAMEFLAG_RRRA,
	GAMEFLAG_BLOOD      = 0x00000800,
	GAMEFLAG_SW			= 0x00001000,
	GAMEFLAG_POWERSLAVE	= 0x00002000,
	GAMEFLAG_EXHUMED	= 0x00004000,
	GAMEFLAG_PSEXHUMED  = GAMEFLAG_POWERSLAVE | GAMEFLAG_EXHUMED,	// the two games really are the same, except for the name and the publisher.
	GAMEFLAG_WORLDTOUR	= 0x00008000,
	GAMEFLAG_DUKEDC		= 0x00010000,
	GAMEFLAG_DUKENW		= 0x00020000,
	GAMEFLAG_DUKEVACA	= 0x00040000,
	GAMEFLAG_BLOODCP	= 0x00080000,
	GAMEFLAG_ROUTE66	= 0x00100000,
	GAMEFLAG_SWWANTON	= 0x00200000,
	GAMEFLAG_SWTWINDRAG	= 0x00400000,

	GAMEFLAG_DUKECOMPAT = GAMEFLAG_DUKE | GAMEFLAG_NAM | GAMEFLAG_NAPALM | GAMEFLAG_WW2GI | GAMEFLAG_RRALL,
	GAMEFLAGMASK        = 0x0000FFFF, // flags allowed from grpinfo

	// We still need these for the parsers.
	GAMEFLAG_FURY = 0,
	GAMEFLAG_DEER = 0,

};

inline bool isDuke()
{
	return g_gameType & (GAMEFLAG_DUKECOMPAT | GAMEFLAG_DUKEBETA | GAMEFLAG_WORLDTOUR | GAMEFLAG_DUKEDC | GAMEFLAG_DUKENW | GAMEFLAG_DUKEVACA);
}

inline bool isRR()
{
	return g_gameType & (GAMEFLAG_RRALL);
}

float vr_hunits_per_meter()
{
	if (vr_units_per_meter != 0.0)
	{
		return vr_units_per_meter;
	}

	if (isDuke() || isRR())
	{
		return 24.0f;
	}

	return 41.0f;
}

float getHmdAdjustedHeightInMapUnit()
{
	if (playerHeight != 0)
	{
		return ((hmdPosition[1] + vr_height_adjust) * vr_hunits_per_meter()) -
				(playerHeight * (heightScaler != 1.0f ? (1.0f - heightScaler) : 1.0f));
	}

	//Just use offset from origin
	return ((hmdPosition[1] - hmdOrigin[1]) * vr_hunits_per_meter());
}

#define isqrt2 0.7071067812f
static VRMode vrmi_mono = { 1, 1.f, 1.f, 1.f,{ { 0.f, 1.f },{ 0.f, 0.f } } };
static VRMode vrmi_stereo = { 2, 1.f, 1.f, 1.f,{ { -.5f, 1.f },{ .5f, 1.f } } };
#ifdef __MOBILE__
// Theirs: one pass, both eyes written by GL_OVR_multiview2.
static VRMode vrmi_openxr = { 1, 1.f, 1.f, 1.f,{ { -.5f, 1.f },{ .5f, 1.f } } };
#else
// PCVR port: two passes, one per eye. See PROGRESS.md.
static VRMode vrmi_openxr = { 2, 1.f, 1.f, 1.f,{ { -.5f, 1.f },{ .5f, 1.f } } };
#endif
static VRMode vrmi_sbsfull = { 2, .5f, 1.f, 2.f,{ { -.5f, .5f },{ .5f, .5f } } };
static VRMode vrmi_sbssquished = { 2, .5f, 1.f, 1.f,{ { -.5f, 1.f },{ .5f, 1.f } } };
static VRMode vrmi_lefteye = { 1, 1.f, 1.f, 1.f, { { -.5f, 1.f },{ 0.f, 0.f } } };
static VRMode vrmi_righteye = { 1, 1.f, 1.f, 1.f,{ { .5f, 1.f },{ 0.f, 0.f } } };
static VRMode vrmi_topbottom = { 2, 1.f, .5f, 1.f,{ { -.5f, 1.f },{ .5f, 1.f } } };
static VRMode vrmi_checker = { 2, isqrt2, isqrt2, 1.f,{ { -.5f, 1.f },{ .5f, 1.f } } };

const VRMode *VRMode::GetVRMode(bool toscreen)
{
	int mode = !toscreen || (sysCallbacks.DisableTextureFilter && sysCallbacks.DisableTextureFilter()) ? 0 : vr_mode;

	switch (mode)
	{
	default:
	case VR_MONO:
		return &vrmi_mono;

	case VR_GREENMAGENTA:
	case VR_REDCYAN:
	case VR_QUADSTEREO:
	case VR_AMBERBLUE:
	case VR_SIDEBYSIDELETTERBOX:
        return &vrmi_stereo;

	case VR_OPENXR:
#ifndef __MOBILE__
		// PCVR port: vr_mode defaults to VR_OPENXR, so a PC with no headset
		// would otherwise present into swapchains that were never created.
		if (!TBXR_VREnabled())
			return &vrmi_mono;
#endif
		return &vrmi_openxr;

	case VR_SIDEBYSIDESQUISHED:
	case VR_COLUMNINTERLEAVED:
		return &vrmi_sbssquished;

	case VR_SIDEBYSIDEFULL:
		return &vrmi_sbsfull;

	case VR_TOPBOTTOM:
	case VR_ROWINTERLEAVED:
		return &vrmi_topbottom;

	case VR_LEFTEYEVIEW:
		return &vrmi_lefteye;

	case VR_RIGHTEYEVIEW:
		return &vrmi_righteye;

	case VR_CHECKERINTERLEAVED:
		return &vrmi_checker;
	}
}

void VRMode::AdjustViewport(DFrameBuffer *screen) const
{
	screen->mSceneViewport.height = (int)(screen->mSceneViewport.height * mVerticalViewportScale);
	screen->mSceneViewport.top = (int)(screen->mSceneViewport.top * mVerticalViewportScale);
	screen->mSceneViewport.width = (int)(screen->mSceneViewport.width * mHorizontalViewportScale);
	screen->mSceneViewport.left = (int)(screen->mSceneViewport.left * mHorizontalViewportScale);

	screen->mScreenViewport.height = (int)(screen->mScreenViewport.height * mVerticalViewportScale);
	screen->mScreenViewport.top = (int)(screen->mScreenViewport.top * mVerticalViewportScale);
	screen->mScreenViewport.width = (int)(screen->mScreenViewport.width * mHorizontalViewportScale);
	screen->mScreenViewport.left = (int)(screen->mScreenViewport.left * mHorizontalViewportScale);
}

extern float vrYaw;
float getViewpointYaw()
{
	return vrYaw;
}

VSMatrix VREyeInfo::GetHUDProjection(int width, int height) const
{
	// now render the main view
	float fovratio;
	float ratio = ActiveRatio(width, height);
	if (ratio >= 1.33f)
	{
		fovratio = 1.33f;
	}
	else
	{
		fovratio = ratio;
	}

	VSMatrix new_projection;
	new_projection.loadIdentity();

	float stereo_separation = getStereoSeparation(vr_hud_stereo);
	new_projection.translate(stereo_separation, 0, 0);

	// doom_units from meters
	new_projection.scale(
			-vr_hunits_per_meter(),
			vr_hunits_per_meter(),
			-vr_hunits_per_meter());

	if (vr_hud_fixed_roll)
	{
		new_projection.rotate(-hmdorientation[ROLL], 0, 0, 1);
	}

	new_projection.rotate(vr_hud_rotate, 1, 0, 0);

	if (vr_hud_fixed_pitch)
	{
		new_projection.rotate(-hmdorientation[PITCH], 1, 0, 0);
	}

	// hmd coordinates (meters) from ndc coordinates
	// const float weapon_distance_meters = 0.55f;
	// const float weapon_width_meters = 0.3f;
	new_projection.translate(0.0, 0.0, 1.0);
	new_projection.scale(
			-vr_hud_scale,
            vr_hud_scale,
			-vr_hud_scale);

	// ndc coordinates from pixel coordinates
	new_projection.translate(-1.0, 1.0, 0);
	new_projection.scale(2.0 / width, -2.0 / height, -1.0);

	VSMatrix proj = GetCenterProjection(RazeXR_GetFOV(), ratio, fovratio);
	proj.multMatrix(new_projection);
	new_projection = proj;

	return new_projection;
}

/*
	Hidden for a screenshot. Set from the off-hand stick click in the menu
	input path, cleared by any button there or by the menu closing, read by
	Draw2D, which draws nothing while it is set. Meaningful only while the
	menu is a panel in the world - on the virtual screen there is nothing
	behind the panel to photograph.
*/
// For the compositor, which places the panel itself. Metres.
float VR_MenuScale()    { return vr_menu_scale; }
float VR_MenuDistance() { return vr_menu_distance; }
float VR_MenuDepth()    { return vr_menu_depth; }

// See hw_vrmodes.h. True only inside VR_PaintMenuLayer.
static bool menuLayerPainting = false;
void VR_SetMenuLayerPainting(bool on) { menuLayerPainting = on; }
bool VR_MenuLayerPainting()           { return menuLayerPainting; }

// The world camera for eye 0 this frame, map units, for the pause diagnostic.
static float worldEyePos[3] = {0, 0, 0};
void VR_SetWorldEyePos(float x, float y, float z) { worldEyePos[0] = x; worldEyePos[1] = y; worldEyePos[2] = z; }
void VR_GetWorldEyePos(float* x, float* y, float* z) { *x = worldEyePos[0]; *y = worldEyePos[1]; *z = worldEyePos[2]; }

bool VR_MenuHidden()
{
	return menuHidden && VR_MenuInWorld();
}

void VR_MenuSetHidden(bool hidden)
{
	menuHidden = hidden;
}

/*
	The HUD projection with the panel put somewhere and left there.

	Same chain as GetHUDProjection, so it lands at the same kind of place the
	HUD does. The anchor is expressed the way the weapon already expresses its
	placement - rotate by (where it is) minus (where the head is) - and the
	inverse-head part follows the HUD's order (pitch, then roll, applied to
	points), which is the correct inverse of a yaw-pitch-roll orientation.
	Roll is always levelled: a menu that tilts with your head is nothing but
	harder to read.
*/
VSMatrix VREyeInfo::GetMenuProjection(int width, int height) const
{
	float fovratio;
	float ratio = ActiveRatio(width, height);
	fovratio = ratio >= 1.33f ? 1.33f : ratio;

	VSMatrix m;
	m.loadIdentity();

	m.translate(getStereoSeparation(vr_hud_stereo), 0, 0);

	// doom_units from meters
	m.scale(
			-vr_hunits_per_meter(),
			vr_hunits_per_meter(),
			-vr_hunits_per_meter());

	m.rotate(-hmdorientation[ROLL], 0, 0, 1);

	if (vr_menu_lock && menuAnchored)
	{
		/*
			The panel hangs in the world, so the head's whole pose is undone
			and the panel's own is applied - rotation and position both.

			Rotation alone was not enough. Every line below the rotations
			places the panel a fixed distance ahead in HEAD space, so it went
			wherever the head went; turning was anchored and leaning was not,
			which reads as swaying. The step that fixes it is the translation
			by (anchor - head), applied once the head's rotation is undone so
			that it is a displacement in the world rather than in view.

			The axis signs follow the scale above: it negates x and z, so a
			world delta arrives here as (-dx, dy, -dz). Metres throughout -
			the conversion to map units is that same scale.
		*/
		m.rotate(-hmdorientation[PITCH], 1, 0, 0);
		m.rotate(-hmdorientation[YAW], 0, 1, 0);

		m.translate(
				clamp((int)vr_menu_lockx, -1, 1) * (menuAnchorPos[0] - hmdPosition[0]),
				clamp((int)vr_menu_locky, -1, 1) * (menuAnchorPos[1] - hmdPosition[1]),
				clamp((int)vr_menu_lockz, -1, 1) * (menuAnchorPos[2] - hmdPosition[2]));

		m.rotate(menuAnchorYaw, 0, 1, 0);
		m.rotate(menuAnchorPitch, 1, 0, 0);
	}

	// hmd coordinates (meters) from ndc coordinates
	m.translate(0.0, 0.0, vr_menu_distance);
	m.scale(
			-vr_menu_scale,
			vr_menu_scale,
			-vr_menu_scale);

	// ndc coordinates from pixel coordinates
	m.translate(-1.0, 1.0, 0);
	m.scale(2.0 / width, -2.0 / height, -1.0);

	VSMatrix proj = GetCenterProjection(RazeXR_GetFOV(), ratio, fovratio);
	proj.multMatrix(m);
	return proj;
}

float VREyeInfo::getStereoSeparation(double stereoLevel) const
{
	float stereo_separation = (vr_ipd * 0.5) * vr_hunits_per_meter() * stereoLevel * (getEye() == 1 ? -1.0 : 1.0);
	return stereo_separation;
}

VSMatrix VREyeInfo::GetPlayerSpriteProjection(int width, int height) const
{
	// now render the main view
	float fovratio;
	float ratio = ActiveRatio(width, height);
	if (ratio >= 1.33f)
	{
		fovratio = 1.33f;
	}
	else
	{
		fovratio = ratio;
	}

	VSMatrix new_projection;
	new_projection.loadIdentity();

	float weapon_stereo_effect = 2.8f;
	new_projection.translate(getStereoSeparation(weapon_stereo_effect), 0, 0);

	// doom_units from meters
	new_projection.scale(
			-vr_hunits_per_meter(),
			vr_hunits_per_meter(),
			-vr_hunits_per_meter());

	if (vr_6dof_weapons)
	{
		new_projection.rotate(-hmdorientation[PITCH], 1, 0, 0);
		new_projection.rotate(-hmdorientation[ROLL], 0, 0, 1);

		float x, y, z, pitch, yaw;
		get_weapon_pos_and_angle(x, y, z, pitch, yaw);
		z -= vr_height_adjust; // We don't want to include the height adjust in this calculation
		new_projection.translate(-x * weapon_stereo_effect, (z-hmdPosition[1]) * weapon_stereo_effect, -y * weapon_stereo_effect);

		if (vr_control_scheme < 10)
		{
			// Right-handed
			new_projection.rotate(weaponangles[YAW] - hmdorientation[YAW], 0, 1, 0);
			new_projection.rotate(weaponangles[PITCH], 1, 0, 0);
			new_projection.rotate(weaponangles[ROLL], 0, 0, 1);
		}
		else
		{
			// Left-handed
			new_projection.rotate(180.0f + weaponangles[YAW] - hmdorientation[YAW], 0, 1, 0);
			new_projection.rotate(-weaponangles[PITCH], 1, 0, 0);
			new_projection.rotate(-weaponangles[ROLL], 0, 0, 1);
		}

		float weapon_scale = 0.6f;
		new_projection.scale(-weapon_scale, weapon_scale, -weapon_scale);

		// ndc coordinates from pixel coordinates
		new_projection.translate(-1.5, 1.5, 0.0);
	}
	else
	{
		new_projection.rotate(-hmdorientation[ROLL], 0, 0, 1);

		new_projection.translate(0.0, 0.0, 1.0);

		float weapon_scale = 0.7f;
		new_projection.scale(-weapon_scale, weapon_scale, -weapon_scale);

		// ndc coordinates from pixel coordinates
		new_projection.translate(-1.0, 1.0, 0);
	}
	new_projection.scale(2.0 / width, -2.0 / height, -1.0);

	VSMatrix proj = GetCenterProjection(RazeXR_GetFOV(), ratio, fovratio);
	proj.multMatrix(new_projection);
	new_projection = proj;

	return new_projection;
}

float VREyeInfo::getShift() const
{
	return mShiftFactor * vr_ipd * vr_hunits_per_meter();
}

int VREyeInfo::getEye() const
{
	return mShiftFactor < 0 ? 0 : 1;
}

bool VR_GetVRProjection(int eye, float zNear, float zFar, float* projection);

VSMatrix VREyeInfo::GetStereoProjection(float fov, float aspectRatio, float fovRatio) const
{
	VSMatrix projection = GetCenterProjection(fov, aspectRatio, fovRatio);
	projection.translate(getStereoSeparation(1.0f) * heightScaler, 0, 0);
	return projection;
}

VSMatrix VREyeInfo::GetCenterProjection(float fov, float aspectRatio, float fovRatio) const
{
	VSMatrix result;

	if (mShiftFactor == 0)
	{
		float fovy = (float)(2 * RAD2DEG(atan(tan(DEG2RAD(fov) / 2) / fovRatio)));
		result.perspective(fovy, aspectRatio, screen->GetZNear(), screen->GetZFar());
		return result;
	}
	else
	{
		double zNear = screen->GetZNear();
		double zFar = screen->GetZFar();


		// For stereo 3D, use asymmetric frustum shift in projection matrix
		// Q: shouldn't shift vary with roll angle, at least for desktop display?
		// A: No. (lab) roll is not measured on desktop display (yet)
		double frustumShift = zNear * getShift() / vr_screendist; // meters cancel, leaving doom units
																  // double frustumShift = 0; // Turning off shift for debugging
		double fH = zNear * tan(DEG2RAD(fov) / 2) / fovRatio;
		double fW = fH * aspectRatio * mScaleFactor;
		double left = -fW - frustumShift;
		double right = fW - frustumShift;
		double bottom = -fH;
		double top = fH;

		VSMatrix result(1);
		result.frustum((float)left, (float)right, (float)bottom, (float)top, (float)zNear, (float)zFar);

		float m[16];
		VR_GetVRProjection(getEye(), zNear, zFar, m);
		result.loadMatrix(m);

		return result;
	}
}

/* virtual */
DVector3 VREyeInfo::GetViewShift(FRotator viewAngles) const
{

	if (mShiftFactor == 0)
	{
		// pass-through for Mono view
		return { 0,0,0 };
	}
	else
	{
		vec3_t angles;
		VectorSet(angles, viewAngles.Pitch.Degrees(), viewAngles.Yaw.Degrees(),
				  viewAngles.Roll.Degrees());

		vec3_t v_forward, v_right, v_up;
		AngleVectors(angles, v_forward, v_right, v_up);

		float posforward = 0;
		float posside = 0;
		float dummy = 0;
		VR_GetMove(&dummy, &dummy, &posforward, &posside, &dummy, &dummy, &dummy, &dummy);
		DVector3 eyeOffset;
		eyeOffset.Zero();

		if (vr_positional_tracking)
		{
			eyeOffset[1] += -posforward * vr_hunits_per_meter();
			eyeOffset[0] += posside * vr_hunits_per_meter();
			eyeOffset[2] += getHmdAdjustedHeightInMapUnit();
		}

		return {eyeOffset[1], eyeOffset[0], eyeOffset[2]};
	}
}

