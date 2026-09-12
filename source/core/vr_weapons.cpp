/*
	vr_weapons.cpp - PC branch. See vr_weapons.h.

	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version. See package/common/gpl-2.0.txt.
*/

#include "vr_weapons.h"

#include "build.h"
#include "coreactor.h"
#include "gamefuncs.h"
#include "hw_drawinfo.h"
#include "hw_voxels.h"
#include "texinfo.h"
#include "buildtiles.h"
#include "sc_man.h"
#include "printf.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "filesystem.h"
#include "texturemanager.h"
#include "tarray.h"
#include "zstring.h"

CVAR(Bool, vr_voxel_weapons, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weapon_scale, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Global nudges on top of the per-weapon placement, adjustable from the
// console so the fit can be dialled in without rebuilding. Map units and
// degrees.
CVAR(Float, vr_weapon_off_forward, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weapon_off_right, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weapon_off_up, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weapon_rot_yaw, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weapon_rot_pitch, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weapon_rot_roll, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

/*
	Diagnostic. Pins the model a fixed distance straight in front of the player
	at eye height, ignoring the controller entirely. If the weapon is visible
	with this on and not with it off, the model and the render path are fine and
	only the placement is wrong - which separates two very different problems
	without needing to see the headset.
*/
CVAR(Float, vr_weapon_debug, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

EXTERN_CVAR(Bool, vr_6dof_weapons)

float vr_hunits_per_meter();
void get_weapon_pos_and_angle(float& x, float& y, float& z, float& pitch, float& yaw);
// Theirs. The controller's offset from the HMD, already rotated into a
// yaw-aligned frame by VrInputDefault: [0] and [2] horizontal, [1] vertical.
extern float weaponoffset[3];
// Theirs. The dominant controller's orientation, [0] pitch, [1] yaw, [2] roll.
// get_weapon_pos_and_angle hands back pitch and yaw but not roll, which is why
// the wrist twist was never reaching the model.
extern float weaponangles[3];
bool TBXR_VREnabled();

//==========================================================================
//
// Parsed definition data
//
//==========================================================================

static TMap<FString, VRWeaponOffsets> WeaponOffsets;
static TMap<FString, int> WeaponBaseTile;	// weapon name -> voxel tile of frame 0
static TMap<int, int> FrameToVoxel;
static TMap<int, bool> OurTiles;		// every voxel tile the weapon defs claim			// 2D sprite tile -> voxel tile
static bool DefsLoaded = false;

static FString CurrentWeapon;
static int CurrentSpriteTile = -1;
static bool CurrentValid = false;
static bool Capturing = false;

//==========================================================================
//
// Recovers the weapon name from a voxel model's filename.
//
// Duke names them vr_weapon_<name><frame>.kvx, the other games vr_<name><frame>.kvx,
// and the trailing frame digit is present only on weapons that animate. For five
// of the seven games the stem that remains is already the name that
// vr_weapon_offsets.def uses, which is what ties the three files together. NAM
// and WW2GI are the exception, and go through CanonicalWeaponName below.
//
//==========================================================================

static bool WeaponStemFromPath(const char* path, FString& stem)
{
	FString s = path;
	auto slash = s.LastIndexOf('/');
	if (slash >= 0) s = s.Mid(slash + 1);

	if (!s.Right(4).CompareNoCase(".kvx")) s.Truncate(s.Len() - 4);

	// Only the "this is a VR weapon model" prefix comes off here. The game
	// prefix that follows it - rr_, nam_, ww2gi_ - is deliberately kept,
	// because it is what makes a stem unique across all seven games, and that
	// is what lets the alias table below be one flat list which never has to
	// ask which game is running.
	static const char* const prefixes[] = { "vr_weapon_", "vr_" };
	for (auto pre : prefixes)
	{
		size_t n = strlen(pre);
		if (s.Len() > n && !s.Left((int)n).Compare(pre))
		{
			stem = s.Mid((int)n);
			return !stem.IsEmpty();
		}
	}
	return false;
}

/*
	NAM and WW2GI run on Duke's weapon enum, so their vr_weapon_offsets.def
	carries Duke's slot names - knee, pistol, chaingun - while their models are
	named after the real weapons - knife, rifle, machinegun. The two files never
	meet, and no rule over the names alone can join them.

	The tile numbering joins them. Tiles run 30000 + slot*10, and each game's
	offsets list is Duke's slots in enum order with handremote left out, so the
	Nth name is the Nth decade. Their own animation defs confirm that reading
	rather than leaving it an inference: NAM maps "shrinker" to 30060, which is
	vr_weapon_nam_grenadelauncher, and "grow" to 30100, which is
	vr_weapon_nam_sniperrifle. Both land exactly where the numbering says.

	WW2GI ships no model in the tenth decade, so its "grow" slot has none and
	keeps the flat sprite, as handremote does in all three games.
*/
struct VRWeaponAlias { const char* model; const char* slot; };

static const VRWeaponAlias WeaponAliases[] = {
	{ "nam_knife",             "knee" },
	{ "nam_rifle",             "pistol" },
	{ "nam_shotgun",           "shotgun" },
	{ "nam_machinegun",        "chaingun" },
	{ "nam_law",               "rpg" },
	{ "nam_grenade",           "handbomb" },
	{ "nam_grenadelauncher",   "shrinker" },
	{ "nam_rocketlauncher",    "devastator" },
	{ "nam_claymore",          "tripbomb" },
	{ "nam_flamethrower",      "freeze" },
	{ "nam_sniperrifle",       "grow" },

	{ "ww2gi_knife",           "knee" },
	{ "ww2gi_m1thompson",      "pistol" },
	{ "ww2gi_mp40",            "shotgun" },
	{ "ww2gi_bar",             "chaingun" },
	{ "ww2gi_bazooka",         "rpg" },
	{ "ww2gi_grenade",         "handbomb" },
	{ "ww2gi_garand_launcher", "shrinker" },
	{ "ww2gi_colt1911",        "devastator" },
	{ "ww2gi_tnt",             "tripbomb" },
	{ "ww2gi_flamethrower",    "freeze" },
};

/*
	Turns a model stem into the name vr_weapon_offsets.def uses for it. An alias
	where the two disagree; otherwise the game prefix comes off and what is left
	is already the offsets name - Redneck's rr_crowbar against its "crowbar",
	and Shadow Warrior's and Exhumed's models against theirs unchanged.
*/
static FString CanonicalWeaponName(const FString& stem)
{
	for (auto& a : WeaponAliases)
	{
		if (!stem.Compare(a.model)) return FString(a.slot);
	}

	static const char* const gamePrefixes[] = { "rr_", "nam_", "ww2gi_" };
	for (auto pre : gamePrefixes)
	{
		size_t n = strlen(pre);
		if (stem.Len() > n && !stem.Left((int)n).Compare(pre)) return stem.Mid((int)n);
	}
	return stem;
}

static void ParseWeaponTiles()
{
	if (fileSystem.FindFile("engine/vr_weapons.def") < 0) return;

	// tile -> stem, collected first so the decade grouping below can see how
	// many frames each weapon has.
	TMap<int, FString> stems;

	FScanner sc;
	try
	{
		sc.Open("engine/vr_weapons.def");
		while (sc.GetString())
		{
			if (!sc.Compare("voxel")) continue;

			sc.MustGetString();
			FString path = sc.String;

			int tile = -1;
			if (sc.CheckString("{"))
			{
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					if (sc.Compare("tile")) { sc.MustGetNumber(); tile = sc.Number; }
					else if (sc.Compare("scale")) sc.MustGetFloat();
				}
			}

			FString stem;
			if (tile >= 0 && WeaponStemFromPath(path.GetChars(), stem))
			{
				stems.Insert(tile, stem);
				OurTiles.Insert(tile, true);
			}
		}
	}
	catch (const CRecoverableError& err)
	{
		Printf(TEXTCOLOR_YELLOW "vr_weapons.def: %s\n", err.what());
		return;
	}

	/*
		Tiles run 30000 + slot*10 + frame, so everything in one decade is the
		same weapon. That is what says whether a trailing digit is a frame index
		or part of the name: "pistol0" sits in a decade of three and loses its
		digit, while Exhumed's "m60" is alone in its decade and keeps it. A
		plain "strip a trailing digit" rule turned that one into "m6".
	*/
	TMap<int, int> frameCount;
	{
		TMap<int, FString>::Iterator it(stems);
		TMap<int, FString>::Pair* pair;
		while (it.NextPair(pair))
		{
			int decade = pair->Key / 10;
			int* n = frameCount.CheckKey(decade);
			frameCount.Insert(decade, n ? *n + 1 : 1);
		}
	}

	TMap<int, FString>::Iterator it(stems);
	TMap<int, FString>::Pair* pair;
	// Frame 0 names the weapon. Done first and on its own pass, because the
	// variant pass below must be able to see whether a name is already a
	// weapon in its own right.
	while (it.NextPair(pair))
	{
		if (pair->Key % 10 != 0) continue;

		FString name = pair->Value;
		int* n = frameCount.CheckKey(pair->Key / 10);
		if (n && *n > 1 && name.Len() > 1)
		{
			char last = name[name.Len() - 1];
			if (last >= '0' && last <= '9') name.Truncate(name.Len() - 1);
		}
		WeaponBaseTile.Insert(CanonicalWeaponName(name), pair->Key);
	}

	/*
		Everything that is not frame 0, where two different things live.

		Animation frames, whose stems end in the frame digit - pistol1, napalm3,
		rail2 - are reached through the animation table by sprite tile and never
		need a name.

		Named state variants do not end in a digit: Shadow Warrior's uzi_akimbo,
		shotgun_quad and nuke. Nothing draws a distinct sprite tile for those, so
		the animation table cannot reach them; the game's own state picks them,
		and for that they need a name to be asked for by. VRaze ships a
		placement for each, which is what says they were meant to be selected.

		A variant never displaces a weapon that already owns its name. NAM
		declares vr_weapon_nam_sniperrifle at both 30061 and 30100, and both
		alias to Duke's "grow" slot, so without this the odd-numbered one would
		overwrite the real weapon's tile depending on which the map iterated
		last. Same model either way, so nothing would have looked wrong - which
		is exactly why it needs to be written down rather than left to luck.
	*/
	TMap<int, FString>::Iterator vit(stems);
	while (vit.NextPair(pair))
	{
		if (pair->Key % 10 == 0) continue;

		FString name = pair->Value;
		if (name.IsEmpty()) continue;

		char last = name[name.Len() - 1];
		if (last >= '0' && last <= '9') continue;

		FString canon = CanonicalWeaponName(name);
		if (WeaponBaseTile.CheckKey(canon)) continue;

		WeaponBaseTile.Insert(canon, pair->Key);
	}
}

//==========================================================================

static void ParseOffsets()
{
	if (fileSystem.FindFile("engine/vr_weapon_offsets.def") < 0) return;

	FScanner sc;
	try
	{
		sc.Open("engine/vr_weapon_offsets.def");
		while (sc.GetString())
		{
			if (!sc.Compare("weapon")) continue;

			sc.MustGetString();
			FString name = sc.String;
			VRWeaponOffsets o;

			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				FString key = sc.String;

				sc.MustGetFloat();
				float v = (float)sc.Float;

				if (!key.CompareNoCase("forward")) o.forward = v;
				else if (!key.CompareNoCase("right")) o.right = v;
				else if (!key.CompareNoCase("up")) o.up = v;
				else if (!key.CompareNoCase("yaw")) o.yaw = v;
				else if (!key.CompareNoCase("pitch")) o.pitch = v;
				else if (!key.CompareNoCase("roll")) o.roll = v;
				else if (!key.CompareNoCase("pivot_x")) o.pivot_x = v;
				else if (!key.CompareNoCase("pivot_y")) o.pivot_y = v;
				else if (!key.CompareNoCase("pivot_z")) o.pivot_z = v;
				else if (!key.CompareNoCase("casing_forward")) o.casing_forward = v;
				else if (!key.CompareNoCase("casing_right")) o.casing_right = v;
				else if (!key.CompareNoCase("casing_up")) o.casing_up = v;
				else if (!key.CompareNoCase("casing_yaw")) o.casing_yaw = v;
				else if (!key.CompareNoCase("casing_pitch")) o.casing_pitch = v;
				else if (!key.CompareNoCase("casing_roll")) o.casing_roll = v;
				else if (!key.CompareNoCase("two_hand_compatible")) o.two_hand_compatible = v != 0;
				// Unknown keys are ignored rather than fatal: VRaze's data is
				// the specification here, and it may carry keys this does not
				// use yet.
			}

			WeaponOffsets.Insert(name, o);
		}
	}
	catch (const CRecoverableError& err)
	{
		Printf(TEXTCOLOR_YELLOW "vr_weapon_offsets.def: %s\n", err.what());
	}
}

//==========================================================================

static void ParseAnimations()
{
	if (fileSystem.FindFile("engine/vr_weapon_animations.def") < 0) return;

	FScanner sc;
	try
	{
		sc.Open("engine/vr_weapon_animations.def");
		while (sc.GetString())
		{
			if (!sc.Compare("weapon")) continue;

			sc.MustGetString();	// name, not needed - the sprite tile is unique
			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				if (sc.Compare("frame"))
				{
					sc.MustGetNumber();
					int spriteTile = sc.Number;
					sc.MustGetNumber();
					int voxelTile = sc.Number;
					FrameToVoxel.Insert(spriteTile, voxelTile);
				}
			}
		}
	}
	catch (const CRecoverableError& err)
	{
		Printf(TEXTCOLOR_YELLOW "vr_weapon_animations.def: %s\n", err.what());
	}
}

//==========================================================================

void VRWeapons_LoadDefs()
{
	WeaponOffsets.Clear();
	OurTiles.Clear();
	WeaponBaseTile.Clear();
	FrameToVoxel.Clear();
	DefsLoaded = false;

	ParseWeaponTiles();
	if (WeaponBaseTile.CountUsed() == 0) return;	// no voxel pack installed

	ParseOffsets();
	ParseAnimations();

	DefsLoaded = true;
	Printf("VR weapons: %d models, %d placements, %d animation frames\n",
		WeaponBaseTile.CountUsed(), WeaponOffsets.CountUsed(), FrameToVoxel.CountUsed());
}

bool VRWeapons_Active()
{
	return DefsLoaded && vr_voxel_weapons && vr_6dof_weapons && TBXR_VREnabled();
}

static bool TileHasVoxel(int tile)
{
	if (tile < 0) return false;
	auto texid = tileGetTextureID(tile);
	if (!texid.isValid()) return false;
	int vox = GetExtInfo(texid).tiletovox;
	return vox >= 0 && vox < MAXVOXELS && voxmodels[vox] != nullptr;
}

bool VRWeapons_HasModel(const char* name)
{
	if (!DefsLoaded) return false;
	auto t = WeaponBaseTile.CheckKey(FString(name));
	return t && TileHasVoxel(*t);
}

void VRWeapons_SetCurrent(const char* name, int spriteTile)
{
	CurrentWeapon = name;
	CurrentSpriteTile = spriteTile;
	CurrentValid = true;
}

bool VRWeapons_BeginWeapon(const char* name)
{
	/*
		Logged before the active check, and only when the weapon changes.

		Deliberately not behind VRWeapons_Active: that is false on a desktop
		with no OpenXR session, so gating this on it would hide exactly the
		thing worth checking. This is what proves a game's hook fires at all and
		hands over a name the definition files know, which together with the
		vrweapons listing settles a newly wired game without a headset - Shadow
		Warrior and Exhumed were both brought up this way. Needs developer 3.
	*/
	{
		static FString lastLogged;
		static bool everLogged = false;
		if (!everLogged || lastLogged.Compare(name))
		{
			everLogged = true;
			lastLogged = name;
			DPrintf(DMSG_NOTIFY, "VR weapon hook: [%s] model %s, placement %s\n",
				name,
				VRWeapons_HasModel(name) ? "yes" : "no",
				WeaponOffsets.CheckKey(FString(name)) ? "yes" : "no");
		}
	}

	if (!VRWeapons_Active() || !VRWeapons_HasModel(name))
	{
		CurrentValid = false;
		Capturing = false;
		return false;
	}

	CurrentWeapon = name;
	CurrentSpriteTile = -1;		// filled in by NoteDrawnTile if a frame matches
	CurrentValid = true;
	Capturing = true;
	return true;
}

bool VRWeapons_IsWeaponTile(int tile)
{
	return OurTiles.CheckKey(tile) != nullptr;
}

bool VRWeapons_WantsVoxel(int tile)
{
	return VRWeapons_Active() && VRWeapons_IsWeaponTile(tile);
}

float VRWeapons_ModelYaw()
{
	if (!CurrentValid) return 0.f;
	auto o = WeaponOffsets.CheckKey(CurrentWeapon);
	return (o ? o->yaw : 0.f) + vr_weapon_rot_yaw;
}

bool VRWeapons_DrawingModel()
{
	return Capturing;
}

void VRWeapons_EndWeapon()
{
	Capturing = false;
}

void VRWeapons_NoteDrawnTile(int tile)
{
	if (!CurrentValid) return;
	if (FrameToVoxel.CheckKey(tile)) CurrentSpriteTile = tile;
}

void VRWeapons_ClearCurrent()
{
	CurrentValid = false;
	Capturing = false;
}

//==========================================================================
//
// Appends the held weapon to this frame's sprite list.
//
//==========================================================================

/*
	A silent return from VRWeapons_AddSprite is the worst failure this module
	has, because every other diagnostic says the weapon is fine: the hook
	resolves the name, finds the model, reports "model yes", and suppresses the
	flat sprite - and then nothing is drawn and nothing is logged. That is
	exactly how Exhumed lost every weapon while its vrweapons listing was
	perfect. Report the reason instead, once per weapon and reason.
*/
static void VRWeapons_NotDrawn(const char* why)
{
	static FString last;
	FString key;
	key.Format("%s|%s", CurrentWeapon.GetChars(), why);
	if (!last.Compare(key)) return;
	last = key;
	DPrintf(DMSG_NOTIFY, "VR weapon: %s NOT DRAWN - %s\n", CurrentWeapon.GetChars(), why);
}

void VRWeapons_AddSprite(tspriteArray& tsprites, const FRenderViewpoint& vp)
{
	// Not logged: these are the ordinary "no VR" and "no weapon" cases.
	if (!VRWeapons_Active() || !CurrentValid) return;

	auto owner = vp.CameraActor;
	if (owner == nullptr)
	{
		VRWeapons_NotDrawn("no camera actor - this game passes nullptr to render_drawrooms");
		return;
	}

	// Animation frame first, base model otherwise.
	int tile = -1;
	if (CurrentSpriteTile >= 0)
	{
		if (auto v = FrameToVoxel.CheckKey(CurrentSpriteTile)) tile = *v;
	}
	if (tile < 0)
	{
		if (auto v = WeaponBaseTile.CheckKey(CurrentWeapon)) tile = *v;
	}
	if (!TileHasVoxel(tile))
	{
		VRWeapons_NotDrawn("resolved tile has no voxel");
		return;
	}

	VRWeaponOffsets off;
	if (auto o = WeaponOffsets.CheckKey(CurrentWeapon)) off = *o;

	const float hupm = vr_hunits_per_meter();

	float wx, wy, wz, wpitch, wyaw;
	get_weapon_pos_and_angle(wx, wy, wz, wpitch, wyaw);

	// Same construction their crosshair and shoot override use, so the model
	// lands where the shots already come from.
	/*
		The view's yaw, not the player actor's.

		Those two deliberately disagree while turning: SetupViewpoint lerps
		vrYaw towards the game's yaw a fraction each frame, their "frame yaw
		resync", so the actor leads and the view follows. Placing the weapon
		from the actor while the scene is drawn from the view makes the model
		swim against everything else - it reads as the weapon vibrating or
		ghosting under smooth turn, and settling the moment the stick is
		released and the two converge again.
	*/
	DAngle playerYaw = DAngle::fromBam(vp.RotAngle);
	/*
		Two different yaws, and conflating them was the bug.

		handYaw is where the controller points. The model's own yaw adds
		off.yaw, which for the pistol is -90 - a correction for how the .kvx is
		authored, nothing to do with where the weapon sits. Building the
		placement basis from the model yaw rotated the whole placement by 90
		degrees, which is why "right" pushed the weapon away from the viewer.
	*/
	DAngle handYaw = playerYaw + DAngle::fromDeg(wyaw);
	/*
		The sprite carries the hand's yaw only. The model's own correction is
		applied by the renderer after pitch and roll, so that a model authored
		facing a different way - Duke's pistol is -90, Redneck's weapons are 0 -
		does not change which axis is twist and which is tilt. Getting that
		wrong reversed both in every game whose models are not authored like
		Duke's.
	*/
	DAngle yaw = handYaw;
	DAngle pitch = owner->spr.Angles.Pitch - DAngle::fromDeg(wpitch - off.pitch - vr_weapon_rot_pitch);

	/*
		Place the weapon relative to the eye, not to the player actor.

		Their crosshair and shoot override build their origin as
		spr.pos.plusZ(-(wz * hunits)), where wz is the hand's height above the
		*floor*. That works for a hitscan, where being vertically off by a
		constant barely moves the impact point at range. It does not work for
		something you look at: the player actor's origin is not on the floor -
		measured 85 units above it - so the construction double counts most of
		a player height and throws the model well over the viewer's head.

		vp.Pos is the actual eye, in render space, so convert it back and add
		the controller's offset from the HMD. weaponoffset[1] is that offset
		vertically, as against get_weapon_pos_and_angle's z, which has been
		turned into an absolute height for the hitscan's benefit.
	*/
	DVector3 pos;
	pos.X = vp.Pos.X;
	pos.Y = -vp.Pos.Y;
	pos.Z = -vp.Pos.Z;

	/*
		The nudges are folded in here, before the rotation, so they live in the
		same frame as the tracked offset by construction.

		Measured, not assumed: with the player facing (0.28, 0.96), the frame's
		first axis comes out at (0.96, -0.27) - perpendicular to the facing -
		and the second at (0.27, 0.96), along it. So the first component is
		lateral and the second is forward, which is the opposite of what the
		names in get_weapon_pos_and_angle suggest.
	*/
	DVector2 posXY(wx * hupm - vr_weapon_off_right,
				   wy * hupm - vr_weapon_off_forward);
	posXY = posXY.Rotated(-DAngle90 + playerYaw);
	pos.X -= posXY.X;
	pos.Y -= posXY.Y;
	pos.Z -= weaponoffset[1] * hupm;	// Build Z is downwards

	/*
		off.up is deliberately not applied to the position. Every Duke weapon
		carries the same -40 against a "// Player height: 40" comment, which
		reads as VRaze lifting from an origin at the feet to head height - a
		correction this placement has already made by starting at the eye.
		Applying it again would raise the weapon by a further player height.
		It remains reachable through vr_weapon_off_up if that reading is wrong.
	*/
	/*
		off.forward and off.right are zero for every Duke weapon, and applying
		them here in a second frame is what produced the 90 degree error. If a
		game turns out to use them they belong folded in above, alongside the
		nudges.
	*/
	/*
		Build's Z runs downwards - their own crosshair goes up with
		plusZ(-(z * hunits)) - while VRaze's "up" is positive upwards. The
		pistol's is -40 against a "// Player height: 40" comment, so adding it
		raises the weapon by 40. Subtracting buried it in the floor, which is
		why nothing was visible.
	*/
	/*
		The model's origin sits a little above the grip, so without this the
		weapon hangs high of the hand. Dialled in against Duke's 24 units per
		metre: two units down, then one back up once the pitch and roll axes
		were corrected, which changed how the model sits about its origin.
		One unit net.

		Held in metres rather than map units so it carries to the games that use
		41 units per metre instead of Duke's 24, where one map unit would be a
		different physical distance.
	*/
	const double GripDropMetres = 1.0 / 24.0;
	pos.Z += GripDropMetres * hupm;		// Build Z is downwards

	pos.Z -= vr_weapon_off_up;

	if (vr_weapon_debug > 0)
	{
		/*
			Pinned a fixed distance ahead of the eye, ignoring the controller.

			Ahead of the *eye* deliberately, not the player actor. Built on the
			actor this diagnostic ghosted under movement entirely by itself -
			the actor steps at tic rate while the scene is drawn from the
			interpolated view - and it needed no eye-height fudge either.

			That cost a round. Left archived after a diagnostic run, the pin was
			reported as three separate faults in the weapon: sitting too far
			away, not twisting with the wrist, and ghosting while moving. All
			three were the instrument. A diagnostic should differ from the real
			path in exactly the one way it is testing, so that anything else it
			shows is real.
		*/
		DVector2 ahead = playerYaw.ToVector();
		pos.X = vp.Pos.X + ahead.X * vr_weapon_debug;
		pos.Y = -vp.Pos.Y + ahead.Y * vr_weapon_debug;
		pos.Z = -vp.Pos.Z;
		yaw = playerYaw;
		pitch = nullAngle;
	}

	auto sectp = owner->sector();
	updatesector(pos.XY(), &sectp);
	if (sectp == nullptr) sectp = owner->sector();

	static FString lastReported;
	if (lastReported.Compare(CurrentWeapon) != 0)
	{
		lastReported = CurrentWeapon;
		DPrintf(DMSG_NOTIFY, "VR weapon: %s tile %d voxel %s | gun (%.0f %.0f %.0f) player (%.0f %.0f %.0f) sect %d scale %.2f\n",
			CurrentWeapon.GetChars(), tile, TileHasVoxel(tile) ? "yes" : "NO",
			pos.X, pos.Y, pos.Z, owner->spr.pos.X, owner->spr.pos.Y, owner->spr.pos.Z,
			sectp ? sectindex(sectp) : -1, (float)vr_weapon_scale);
		DPrintf(DMSG_NOTIFY, "   floor %.0f ceil %.0f | wz %.2f m -> %.0f u | off.up %.0f | hupm %.1f\n",
			owner->sector() ? owner->sector()->floorz : 0.0, owner->sector() ? owner->sector()->ceilingz : 0.0,
			wz, wz * hupm, off.up, hupm);
		{
			DVector2 aWorld = DVector2(1,0).Rotated(-DAngle90 + playerYaw);
			DVector2 bWorld = DVector2(0,1).Rotated(-DAngle90 + playerYaw);
			DVector2 fwdWorld = yaw.ToVector();
			DPrintf(DMSG_NOTIFY, "   yaw %.0f | axisA (%.2f %.2f) axisB (%.2f %.2f) | yawvec (%.2f %.2f) | viewvec (%.2f %.2f)\n",
				playerYaw.Degrees(), aWorld.X, aWorld.Y, bWorld.X, bWorld.Y,
				fwdWorld.X, fwdWorld.Y, vp.ViewVector.X, vp.ViewVector.Y);
		}
	}

	auto tspr = tsprites.newTSprite();
	*tspr = {};
	tspr->ownerActor = owner;
	tspr->pos = pos;
	tspr->sectp = sectp;
	tspr->picnum = tile;
	tspr->shade = -32;
	tspr->pal = 0;
	tspr->scale = DVector2(vr_weapon_scale, vr_weapon_scale);
	tspr->Angles.Yaw = yaw;
	tspr->Angles.Pitch = pitch;
	tspr->Angles.Roll = DAngle::fromDeg(weaponangles[2] + off.roll + vr_weapon_rot_roll);
	tspr->statnum = MAXSTATUS;
	// Never animate the tile and never model-substitute it: this is already the
	// model, chosen by the animation table above.
	/*
		NOANIMATE only. Not NOMODEL: in DispatchSprites the voxel lookup lives
		inside the same test as the model substitution, so setting NOMODEL
		disables voxels as well and the sprite falls through to flat billboard
		rendering of a voxel-only tile - which has no texture, and is therefore
		invisible. That was the first cut's bug.
	*/
	tspr->cstat2 = CSTAT2_SPRITE_NOANIMATE;
	tspr->cstat = CSTAT_SPRITE_YCENTER;
}


//==========================================================================
//
// Diagnostic: what the three definition files actually resolved to.
//
// Prints every weapon name the running game ended up with, the voxel tile it
// resolved to, whether that tile's model loaded, and whether it has a
// placement. Names come from the models and placements from a separate file,
// and the two only meet if the naming lines up - so this is what catches a game
// wired to the wrong table, which is the one mistake the data cannot prevent.
//
// It needs no headset. The table is built at startup from the data alone, so
//
//     raze -gamegrp <game> -file vrweapons.pk3 +vrweapons
//
// settles whether a game is correct without costing a testing round.
//
//==========================================================================

CCMD(vrweapons)
{
	if (!DefsLoaded)
	{
		Printf("VR weapons: no definitions loaded - is vrweapons.pk3 on the command line?\n");
		return;
	}

	Printf("VR weapons: %d models, %d placements, %d animation frames\n",
		WeaponBaseTile.CountUsed(), WeaponOffsets.CountUsed(), FrameToVoxel.CountUsed());

	/*
		Walked in tile order rather than map order, so the listing comes out in
		the game's own weapon order and can be read straight against its weapon
		enum. Tiles run 30000 + slot*10, and no Build game has twenty weapons.
	*/
	for (int tile = 30000; tile < 30200; tile++)
	{
		TMap<FString, int>::Iterator it(WeaponBaseTile);
		TMap<FString, int>::Pair* pair;
		while (it.NextPair(pair))
		{
			if (pair->Value != tile) continue;
			Printf("  slot %2d%s %-16s tile %5d  model %-3s  placement %-3s\n",
				(tile - 30000) / 10, tile % 10 ? "*" : " ",
				pair->Key.GetChars(), tile,
				TileHasVoxel(tile) ? "yes" : "NO",
				WeaponOffsets.CheckKey(pair->Key) ? "yes" : "NO");
		}
	}
	Printf("  (* is a named state variant of the slot above it)\n");
}
