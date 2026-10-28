/*
	PC branch: controller vibration.

	The OpenXR side was always there (TBXR_Vibrate, an output action bound on
	every controller), but on PC nothing ever asked for it: on the Quest the
	requests came from Team Beef's Android code, which this port does not have.

	So the game is watched instead, once a frame, through one small hook per
	game (GameInterface::VRHapticState): the local player's health, the weapon
	in hand, and that weapon's ammo.

	  - a shot: the ammo goes down with the same weapon in hand. The gun hand
	    gets a short kick, the Leftorium's left hand when that is the gun hand.
	    A chaingun's run of shots becomes a steady buzz.
	  - a hit: the health goes down. Both hands, longer and harder the bigger
	    the hit.

	A new level, a load or a weapon change only sets the baseline again, so
	none of them buzz. VR Options, Hands and Weapons: Controller Vibration,
	and Vibration Strength.

	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#include "c_cvars.h"
#include "gamecontrol.h"
#include "gamestruct.h"
#include "gamestate.h"
#include "mapinfo.h"
#include "vr_haptics.h"

CVAR(Bool, vr_haptics, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_haptics_strength, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
EXTERN_CVAR(Int, vr_control_scheme)

void TBXR_Vibrate(int duration, int chan, float intensity);	// OpenXrInput.cpp: 1 right, 2 left, 3 both

static int lastHealth = -1, lastWeapon = -1, lastAmmo = -1;
static MapRecord* lastLevel = nullptr;

static int heldWeapon = -1;	// read every frame, haptics on or off: two hands use it too

static void Forget()
{
	lastHealth = lastWeapon = lastAmmo = -1;
}

// The pistols: Duke's (and Redneck's revolver, NAM's and WWII GI's, the same
// slot), Blood's flare gun and PowerSlave's magnum. Shadow Warrior has none.
bool VR_HoldingPistol()
{
	if (heldWeapon < 0) return false;
	if (isDukeLike()) return heldWeapon == 1;
	if (isBlood()) return heldWeapon == 2;
	if (isExhumed()) return heldWeapon == 1;
	return false;
}

void VR_HapticsFrame()
{
	int health, weapon, ammo;
	const bool known = gamestate == GS_LEVEL && gi && gi->VRHapticState(health, weapon, ammo);
	heldWeapon = known ? weapon : -1;
	if (!vr_haptics || !known)
	{
		Forget();
		return;
	}
	if (currentLevel != lastLevel)
	{
		lastLevel = currentLevel;
		Forget();
	}

	const float strength = clamp((float)vr_haptics_strength, 0.f, 1.f);
	if (strength > 0.f)
	{
		if (lastHealth > 0 && health < lastHealth)
		{
			const int hit = lastHealth - health;
			const float amp = clamp(0.35f + hit / 40.f, 0.35f, 1.f) * strength;
			const int ms = clamp(90 + hit * 4, 90, 300);
			TBXR_Vibrate(ms, 3, amp);
		}
		if (weapon == lastWeapon && lastAmmo >= 0 && ammo < lastAmmo)
			TBXR_Vibrate(70, vr_control_scheme >= 10 ? 2 : 1, 0.65f * strength);
	}

	lastHealth = health;
	lastWeapon = weapon;
	lastAmmo = ammo;
}
