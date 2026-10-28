#pragma once

// PC branch: controller vibration (vr_haptics.cpp).
void VR_HapticsFrame();	// once a frame: a shot or a hit since the last one buzzes the hands
bool VR_HoldingPistol();	// the weapon in hand is a pistol (two hands: no gun stock for it)
