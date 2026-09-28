/*
	vr_crosshair.h - PC branch.

	The 6DoF crosshair is not a 2D overlay. It is a world sprite - Team Beef's
	"aiming decal" - created at the point a hitscan from the controller lands,
	so that what you see is genuinely where the shot goes. That is the right
	idea, and it is why the crosshair can be trusted.

	It also means the crosshair is an object in the level, subject to every rule
	an object is subject to, and three of Domyoji's reports are the same
	consequence of that:

	  - it clips into geometry and is eaten by mirror surfaces and warp sectors,
	    because it sits exactly in the surface it hit;
	  - it is foot-aligned on the impact point rather than centred on it,
	    because a Build sprite's origin is its base unless told otherwise;
	  - it blocks the laser trip mine, because a hitscan looking for a wall to
	    place the mine on finds the crosshair first.

	All five games spawn it in their own player code. These two calls are what
	they share, so that the answer is in one place rather than five.

	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version. See package/common/gpl-2.0.txt.
*/

#ifndef VR_CROSSHAIR_H
#define VR_CROSSHAIR_H

#include "vectors.h"

class DCoreActor;

/*
	Where the crosshair sprite goes, given where the shot started and where it
	landed.

	Not the impact point itself. A Build sprite standing in a wall plane is
	clipped by that wall, so whichever half of it falls behind the surface
	simply disappears - which is the reported "sweeping onto a wall to the left
	loses the crosshair's left arm". Backing it off along the ray by a little
	under a hand's width leaves it clear of the surface while staying, to the
	eye, on it.

	The distance is vr_crosshair_offset, in metres, converted per game so that
	it is the same physical distance in Duke's 24 units to the metre as in
	Blood's 41. It is clamped to a fraction of the shot, so a muzzle-pressed
	shot cannot pull the crosshair behind the viewer.

	Returns hitpos unchanged when the two points coincide.
*/
DVector3 VRCrosshair_Place(const DVector3& start, const DVector3& hitpos);

/*
	The flags the crosshair sprite must carry, applied every frame because the
	games rebuild and reuse this actor.

	YCENTER centres it on the impact point; without it Build hangs a sprite by
	its feet, which is the reported "foot-aligned at the hitscan impact point
	instead of centered".

	NOFIND makes it invisible to hitscan and neartag. That is the one that
	matters beyond appearance: without it the crosshair is a solid object in
	front of the player, and the laser trip mine's own hitscan - looking for a
	wall to attach to - hits the crosshair instead. Clearing the blocking bits
	keeps it out of the way of movement clipping too.
*/
void VRCrosshair_SetFlags(DCoreActor* actor);

/*
	The crosshair, re-aimed every frame.

	Each game places its crosshair once per game tic, from the player actor's
	yaw - 30 times a second in Duke, while the view and the weapon in your
	hand are drawn every frame from the view's yaw. Turning, the crosshair
	trailed; after a snap turn it hung where you had been facing for up to a
	tic. Domyoji's cluster B, 2026.

	The fix keeps each game's own aim - so the crosshair still marks where its
	shots go - and only re-runs it at draw time with this frame's numbers:

	  VRCrosshair_Aim    called by each game where it builds the shot's
	                     origin, before it moves the actor for the shot.
	                     Records what that origin was made of.
	  VRCrosshair_Frame  called while the frame's sprites are gathered.
	                     Rebuilds the origin from the view's yaw, the actor's
	                     interpolated position and the controller as it is
	                     now, hitscans again, and moves the crosshair's
	                     sprite - the sprite drawn this frame, never the
	                     actor, so nothing the game simulates is touched.

	vr_crosshair_per_frame 0 puts it back to once a tic, for comparison.
*/
class tspriteArray;
struct FRenderViewpoint;
void VRCrosshair_Aim(DCoreActor* player, const DVector3& spos);
// Once a frame, before any scene pass, from the head's viewpoint (render_drawrooms).
void VRCrosshair_Update(const FRenderViewpoint& vp);
// Every scene pass: applies what VRCrosshair_Update worked out.
void VRCrosshair_Frame(tspriteArray& tsprites, const FRenderViewpoint& vp);

#endif
