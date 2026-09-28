/*
	vr_crosshair.cpp - PC branch. See vr_crosshair.h.

	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version. See package/common/gpl-2.0.txt.
*/

#include "vr_crosshair.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "printf.h"
#include "coreactor.h"
#include "maptypes.h"
#include "gamefuncs.h"
#include "hw_drawinfo.h"

float vr_hunits_per_meter();
void get_weapon_pos_and_angle(float& x, float& y, float& z, float& pitch, float& yaw);
bool TBXR_VREnabled();
EXTERN_CVAR(Bool, vr_6dof_weapons)
EXTERN_CVAR(Bool, vr_6dof_crosshair)

// What the last tic's aim was made of, for VRCrosshair_Frame. The actors are
// compared, never dereferenced, from there: the one dereferenced at draw time
// is the frame's own camera actor, and only once it is known to be this player.
static DCoreActor* AimPlayer = nullptr;
static DCoreActor* AimCrosshair = nullptr;
static int AimCrosshairStat = -1;	// its status list, checked on the live sprite
static DVector3 AimExtra;		// the part of the origin that is neither hand nor actor

/*
	How far to back the crosshair off the surface it hit, in metres.

	In metres rather than map units on purpose: the games disagree about how
	many units a metre is - Duke and Redneck 24, Blood and Shadow Warrior 41 -
	so a constant in units would be a different physical distance in each, and
	the whole point is that it clears the sprite by the same amount everywhere.

	The default is a guess made without a headset. It is a cvar so that it can
	be dialled in during one session rather than costing a build per attempt.
*/
CVAR(Float, vr_crosshair_offset, 0.25f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

DVector3 VRCrosshair_Place(const DVector3& start, const DVector3& hitpos)
{
	DVector3 back = start - hitpos;
	const double dist = back.Length();
	if (dist <= 0.0) return hitpos;

	double offset = vr_crosshair_offset * vr_hunits_per_meter();
	if (offset <= 0.0) return hitpos;

	/*
		Never more than a quarter of the way back to the muzzle.

		Pressed against a wall the whole shot may be shorter than the offset,
		and backing off by the full amount would put the crosshair behind the
		hand - visibly wrong, and at zero distance it would be behind the eye.
		Scaling down when the shot is short keeps it on the surface where the
		surface is close, which is also where the error would be most obvious.
	*/
	const double cap = dist * 0.25;
	if (offset > cap) offset = cap;

	return hitpos + back * (offset / dist);
}

void VRCrosshair_SetFlags(DCoreActor* actor)
{
	if (actor == nullptr) return;
	AimCrosshair = actor;	// the one VRCrosshair_Frame re-aims
	AimCrosshairStat = actor->spr.statnum;

	// Centred on the impact point, not standing on it.
	actor->spr.cstat |= CSTAT_SPRITE_YCENTER;

	/*
		Out of the way of everything that looks for something in the world.

		NOFIND is the flag the trip mine needs: its placement hitscan is
		searching for a wall, and an unflagged crosshair sitting in front of the
		player is what it finds instead. Clearing the blocking bits is the same
		idea for movement - a crosshair should never be something you bump into.
	*/
	actor->spr.cstat2 |= CSTAT2_SPRITE_NOFIND;
	actor->spr.cstat &= ~CSTAT_SPRITE_BLOCK_ALL;
}

//==========================================================================
//
// Per frame. See vr_crosshair.h.
//
//==========================================================================

CVAR(Bool, vr_crosshair_per_frame, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


/*
	Every game builds the shot's origin the same way:

	    spos = actor + (0, 0, -z * hupm) - rotate(x, y, yaw - 90) * hupm - extra

	x, y, z being the controller from get_weapon_pos_and_angle. Blood alone
	has an "extra", a height of its own (viewzoffset against the posture's
	weapon height). Rather than know each game's, take it as whatever is left
	of the origin once the hand and the actor are accounted for.
*/
static DVector3 HandPart(double yawDeg, float x, float y, float z)
{
	const double hupm = vr_hunits_per_meter();
	DVector2 xy(x * hupm, y * hupm);
	xy = xy.Rotated(-DAngle90 + DAngle::fromDeg(yawDeg));
	return DVector3(-xy.X, -xy.Y, -(z * hupm));
}

void VRCrosshair_Aim(DCoreActor* player, const DVector3& spos)
{
	if (player == nullptr) return;
	float x, y, z, pitch, yaw;
	get_weapon_pos_and_angle(x, y, z, pitch, yaw);
	AimPlayer = player;
	AimExtra = spos - player->spr.pos - HandPart(player->spr.Angles.Yaw.Degrees(), x, y, z);
}

void VRCrosshair_Frame(tspriteArray& tsprites, const FRenderViewpoint& vp)
{
	if (!vr_crosshair_per_frame || !vr_6dof_weapons || !vr_6dof_crosshair) return;
	if (!TBXR_VREnabled()) return;
	if (AimCrosshair == nullptr || AimPlayer == nullptr) return;

	// Only the player's own view: a camera or a viewscreen shows the world
	// from somewhere else, and the crosshair belongs to the hand.
	DCoreActor* player = vp.CameraActor;
	if (player == nullptr || player != AimPlayer) return;

	tspritetype* tspr = nullptr;
	for (unsigned i = 0; i < tsprites.Size(); i++)
	{
		if (tsprites.get(i)->ownerActor == AimCrosshair) { tspr = tsprites.get(i); break; }
	}
	if (tspr == nullptr) return;					// not drawn this frame
	/*
		The remembered pointer can outlive its actor - a level change frees it
		before the game spawns a new crosshair - and the memory can come back
		as some other actor. The sprite found here is live, so its status list
		can be read: only the game's crosshair list is the crosshair.
	*/
	if (tspr->ownerActor->spr.statnum != AimCrosshairStat) return;
	if (tspr->scale.X <= 0 || tspr->scale.Y <= 0) return;	// the game has hidden it

	float x, y, z, wpitch, wyaw;
	get_weapon_pos_and_angle(x, y, z, wpitch, wyaw);

	// The view's yaw, as the weapon in the hand is drawn - see
	// VRWeapons_AddSprite for why that and not the actor's.
	const DAngle viewYaw = DAngle::fromBam(vp.RotAngle);
	const DVector3 start = player->interpolatedpos(vp.TicFrac) + AimExtra + HandPart(viewYaw.Degrees(), x, y, z);
	const DAngle aimYaw = viewYaw + DAngle::fromDeg(wyaw);

	double vel = 1024, zvel = 0;
	setFreeAimVelocity(vel, zvel, player->spr.Angles.Pitch - DAngle::fromDeg(wpitch), 16.);

	auto sectp = player->sector();
	updatesector(start.XY(), &sectp);
	if (sectp == nullptr) return;

	HitInfoBase hit{};
	const auto savedCstat = player->spr.cstat;
	player->spr.cstat &= ~CSTAT_SPRITE_BLOCK_ALL;
	hitscan(start, sectp, DVector3(aimYaw.ToVector() * vel, zvel * 64), hit, CLIPMASK1);
	player->spr.cstat = savedCstat;
	if (hit.hitSector == nullptr) return;

	tspr->pos = VRCrosshair_Place(start, hit.hitpos);
	tspr->sectp = hit.hitSector;
}

//==========================================================================
//
// Diagnostic: what the offset does, at the distances you actually shoot at.
//
// Needs no headset. The placement is arithmetic on two points, so this runs the
// shipping function on known values and prints what comes back - which is also
// the quickest way to see what a change to vr_crosshair_offset is worth before
// putting the headset on.
//
// The sign is the thing to check. A crosshair pushed INTO the wall instead of
// out of it would read as "no change" in the headset and cost a testing round.
//
//==========================================================================

CCMD(vrcrosshair)
{
	const double hupm = vr_hunits_per_meter();
	Printf("vr_crosshair_offset %.3f m = %.2f map units at %.1f units per metre\n",
		(float)vr_crosshair_offset, vr_crosshair_offset * hupm, hupm);
	Printf("  %-8s %-10s %-10s %s\n", "shot", "placed", "backed off", "note");

	// Straight down +X from the origin, so the numbers read directly.
	for (double d : { 8.0, 32.0, 128.0, 512.0, 2048.0 })
	{
		const DVector3 start(0, 0, 0);
		const DVector3 hitpos(d, 0, 0);
		const DVector3 at = VRCrosshair_Place(start, hitpos);
		const double moved = d - at.X;
		Printf("  %-8.1f %-10.2f %-10.2f %s\n", d, at.X, moved,
			moved <= 0 ? "WRONG - not backed off at all" :
			(moved >= d * 0.2501 ? "capped at a quarter of the shot" : ""));
	}
}
