#pragma once

/*
	PC branch: crouch for real. Duck your head and the body crouches; stand
	and it stands. See vr_crouch.cpp.
*/

#include "vectors.h"

// Asked once per input gather (inputstate.cpp): does the head hold the crouch?
// buttonCrouch is the crouch button's own state, which stops any view lift.
bool VRCrouch_Held(bool buttonCrouch);

// Once a frame, from the head's own viewpoint (render_drawrooms): eye is the
// camera's position in Build coordinates, interpolated as drawn.
void VRCrouch_Frame(const DVector3& eye, double interpfrac);

// How far the eyes go back up this frame, in map units (hw_vrmodes.cpp).
double VRCrouch_Lift();
