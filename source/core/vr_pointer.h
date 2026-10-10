#pragma once

/*
	PC branch: point at menus. Hold the gun hand's grip in a menu and its aim
	is a laser at the panel: a dot where it lands, and the trigger clicks.
	See vr_pointer.cpp.
*/

// Once a frame from the VR layer: on = the grip is held and the aim lands on
// the menu panel at (u, v), 0..1 from its top left; trigger = pulled.
void VRPointer_Update(bool on, float u, float v, bool trigger);

// After the menu has drawn (DrawOverlays): the dot.
void VRPointer_Draw();
