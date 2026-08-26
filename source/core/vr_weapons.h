/*
	vr_weapons.h - PC branch.

	Voxel weapons in hand, modelled on VRaze.

	RazeXR draws Build's ordinary flat weapon sprite. VRaze replaces it with a
	3D voxel model held at the controller. Its engine code was never published,
	but its data fully specifies the interface, and that data is what this is
	written against:

	  vr_weapons.def            voxel "<path>" { tile N scale S }
	                            Standard Build syntax. Raze's own def parser
	                            loads these and binds each voxel to a tile, so
	                            the models come in for free. This module only
	                            re-reads the file to recover which weapon each
	                            tile belongs to, which it takes from the model's
	                            filename.

	  vr_weapon_offsets.def     weapon <name> { forward/right/up, yaw/pitch/roll,
	                            pivot_*, casing_*, two_hand_compatible }

	  vr_weapon_animations.def  weapon <name> { frame <spriteTile> <voxelTile> }
	                            Maps the game's existing 2D weapon frames onto
	                            voxel variants, so an animating weapon animates.

	Tiles run 30000 + slot*10 + frame. The slot is not the game's weapon enum -
	Duke's handremote has offsets but no model, and the numbering closes up
	around it - so the weapon *name* is the key throughout.

	Copyright (C) 2026 RazeXR PCVR port
*/

#ifndef VR_WEAPONS_H
#define VR_WEAPONS_H

#include "maptypes.h"

struct FRenderViewpoint;

struct VRWeaponOffsets
{
	float forward = 0, right = 0, up = 0;
	float yaw = 0, pitch = 0, roll = 0;
	float pivot_x = 0, pivot_y = 0, pivot_z = 0;
	float casing_forward = 0, casing_right = 0, casing_up = 0;
	float casing_yaw = 0, casing_pitch = 0, casing_roll = 0;
	bool two_hand_compatible = false;
};

// Read the three definition files. Safe to call when they are absent, in which
// case voxel weapons stay off and the flat sprite is drawn as it always was.
void VRWeapons_LoadDefs();

// True when the definitions loaded and vr_voxel_weapons is on.
bool VRWeapons_Active();

// True when this weapon has a voxel that actually loaded. VRaze's own data has
// at least one model Raze cannot read - Duke's knee - and their build logs the
// same failure, so the flat sprite has to remain the fallback.
bool VRWeapons_HasModel(const char* name);

// Set by each game's weapon display code once per tic: which weapon is in hand,
// and the 2D tile it would otherwise have drawn. The tile drives the animation
// lookup; the name drives everything else.
void VRWeapons_SetCurrent(const char* name, int spriteTile);

// Called at the top of a game's weapon display. Returns true when there is a
// model for this weapon, in which case the caller should let its drawing code
// run but suppress the actual draws, reporting each tile it would have drawn
// through VRWeapons_NoteDrawnTile.
//
// Watching the tiles rather than reimplementing each game's frame selection is
// what keeps this out of the games: the animation table already says which
// sprite tiles matter, so anything else drawn is simply ignored.
bool VRWeapons_BeginWeapon(const char* name);
void VRWeapons_NoteDrawnTile(int tile);

// True while a model is standing in for the flat weapon, so the game's drawing
// code should report tiles rather than draw them.
bool VRWeapons_DrawingModel();
void VRWeapons_ClearCurrent();

// Appends the held weapon to this frame's sprite list, positioned from the live
// controller pose. Called once per frame between processSprites and
// DispatchSprites, so it never inherits the tic-rate quantisation that makes
// their crosshair lag a snap turn.
void VRWeapons_AddSprite(tspriteArray& tsprites, const FRenderViewpoint& vp);

#endif
