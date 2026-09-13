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

float vr_hunits_per_meter();

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
