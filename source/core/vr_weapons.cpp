/*
	vr_weapons.cpp - PC branch. See vr_weapons.h.

	Copyright (C) 2026 RazeXR PCVR port
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
#include "filesystem.h"
#include "texturemanager.h"
#include "tarray.h"
#include "zstring.h"

CVAR(Bool, vr_voxel_weapons, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weapon_scale, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

EXTERN_CVAR(Bool, vr_6dof_weapons)

float vr_hunits_per_meter();
void get_weapon_pos_and_angle(float& x, float& y, float& z, float& pitch, float& yaw);
bool TBXR_VREnabled();

//==========================================================================
//
// Parsed definition data
//
//==========================================================================

static TMap<FString, VRWeaponOffsets> WeaponOffsets;
static TMap<FString, int> WeaponBaseTile;	// weapon name -> voxel tile of frame 0
static TMap<int, int> FrameToVoxel;			// 2D sprite tile -> voxel tile
static bool DefsLoaded = false;

static FString CurrentWeapon;
static int CurrentSpriteTile = -1;
static bool CurrentValid = false;

//==========================================================================
//
// Recovers the weapon name from a voxel model's filename.
//
// Duke names them vr_weapon_<name><frame>.kvx, the other games vr_<name><frame>.kvx,
// and the trailing frame digit is present only on weapons that animate. The stem
// that remains matches the name used by vr_weapon_offsets.def exactly, which is
// what ties the three files together.
//
//==========================================================================

static bool WeaponNameFromPath(const char* path, FString& name, int& frame)
{
	FString s = path;
	auto slash = s.LastIndexOf('/');
	if (slash >= 0) s = s.Mid(slash + 1);

	if (!s.Right(4).CompareNoCase(".kvx")) s.Truncate(s.Len() - 4);

	if (s.IndexOf("vr_weapon_") == 0) s = s.Mid(10);
	else if (s.IndexOf("vr_") == 0) s = s.Mid(3);
	else return false;

	frame = 0;
	if (s.Len() > 1)
	{
		char last = s[s.Len() - 1];
		if (last >= '0' && last <= '9')
		{
			frame = last - '0';
			s.Truncate(s.Len() - 1);
		}
	}

	if (s.IsEmpty()) return false;
	name = s;
	return true;
}

//==========================================================================
//
// vr_weapons.def is loaded by Raze's own def parser, which binds each voxel to
// a tile. It is re-read here only to recover which weapon owns which tile.
//
//==========================================================================

static void ParseWeaponTiles()
{
	if (fileSystem.FindFile("engine/vr_weapons.def") < 0) return;

	FScanner sc;
	try
	{
		sc.Open("engine/vr_weapons.def");
		while (sc.GetString())
		{
			if (sc.Compare("voxel"))
			{
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

				FString name;
				int frame;
				if (tile >= 0 && WeaponNameFromPath(path.GetChars(), name, frame) && frame == 0)
				{
					WeaponBaseTile.Insert(name, tile);
				}
			}
		}
	}
	catch (const CRecoverableError& err)
	{
		Printf(TEXTCOLOR_YELLOW "vr_weapons.def: %s\n", err.what());
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

void VRWeapons_ClearCurrent()
{
	CurrentValid = false;
}

//==========================================================================
//
// Appends the held weapon to this frame's sprite list.
//
//==========================================================================

void VRWeapons_AddSprite(tspriteArray& tsprites, const FRenderViewpoint& vp)
{
	if (!VRWeapons_Active() || !CurrentValid) return;

	auto owner = vp.CameraActor;
	if (owner == nullptr) return;

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
	if (!TileHasVoxel(tile)) return;

	VRWeaponOffsets off;
	if (auto o = WeaponOffsets.CheckKey(CurrentWeapon)) off = *o;

	const float hupm = vr_hunits_per_meter();

	float wx, wy, wz, wpitch, wyaw;
	get_weapon_pos_and_angle(wx, wy, wz, wpitch, wyaw);

	// Same construction their crosshair and shoot override use, so the model
	// lands where the shots already come from.
	DAngle playerYaw = owner->spr.Angles.Yaw;
	DAngle yaw = playerYaw + DAngle::fromDeg(wyaw + off.yaw);
	DAngle pitch = owner->spr.Angles.Pitch - DAngle::fromDeg(wpitch - off.pitch);

	DVector3 pos = owner->spr.pos.plusZ(-(wz * hupm));
	DVector2 posXY(wx * hupm, wy * hupm);
	posXY = posXY.Rotated(-DAngle90 + playerYaw);
	pos.X -= posXY.X;
	pos.Y -= posXY.Y;

	// Per weapon placement, in the weapon's own frame.
	DVector2 fwd = yaw.ToVector();
	pos.X += fwd.X * off.forward - fwd.Y * off.right;
	pos.Y += fwd.Y * off.forward + fwd.X * off.right;
	pos.Z -= off.up;

	auto sectp = owner->sector();
	updatesector(pos.XY(), &sectp);
	if (sectp == nullptr) sectp = owner->sector();

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
	tspr->Angles.Roll = DAngle::fromDeg(off.roll);
	tspr->statnum = MAXSTATUS;
	// Never animate the tile and never model-substitute it: this is already the
	// model, chosen by the animation table above.
	tspr->cstat2 = CSTAT2_SPRITE_NOANIMATE | CSTAT2_SPRITE_NOMODEL;
	tspr->cstat = CSTAT_SPRITE_YCENTER;
}
