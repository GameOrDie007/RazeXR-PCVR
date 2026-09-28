/*
	vr_wheel.h - PC branch.

	The weapon wheel. Hold the gun hand's stick click and the weapons you own
	hang in a ring in front of you, as the same 3D models you hold; move the
	hand towards one and let go to switch to it. Time slows while it is open,
	the way the Serious Sam VR ports do it, so choosing is not a gamble.

	Asked for by a PCVR player in September 2026. Every Build game here has
	more weapons than stick up / stick down can reach quickly, and a slot you
	can SEE beats cycling past the ones you do not want.

	Built from three things the port already had, so nothing about a weapon
	is described twice:
	  - selection is the engine's own "slot N" command, which every game
	    already understands, alternates and all;
	  - the models are the VR weapon models, looked up by the same names the
	    held weapon uses;
	  - slow motion is the engine's own i_timescale, which re-bases its clock
	    when changed, so game time stays continuous.
	The only thing each game supplies is which of its slots you own
	(GameInterface::VRWheelEntries).

	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version. See package/common/gpl-2.0.txt.
*/

#ifndef VR_WHEEL_H
#define VR_WHEEL_H

#include "tarray.h"

class tspriteArray;
struct FRenderViewpoint;

// The Weapon Wheel option. Off, the stick click crouches as it always did.
bool VRWheel_Enabled();

// From the VR input handler every frame: is the wheel button down?
void VRWheel_SetHeld(bool held);

// True while the ring is up. The renderer draws our voxel tiles while it is,
// even with voxel weapons or world voxels switched off.
bool VRWheel_IsOpen();

// Once a frame, before any scene pass, from the head's viewpoint: opening,
// closing, reading the hand and choosing. render_drawrooms calls it.
void VRWheel_Update(const FRenderViewpoint& vp);
// Every scene pass: draws the ring VRWheel_Update placed.
void VRWheel_AddSprites(tspriteArray& tsprites, const FRenderViewpoint& vp);

#endif
