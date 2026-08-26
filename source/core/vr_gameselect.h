/*
	vr_gameselect.h - PC branch.

	The in-game Switch Game menu, modelled on VRaze's.

	VRaze's engine source was never published, but its menudef documents the
	contract it expected, and this keeps to it so that its menu definitions drop
	in unchanged:

		// Items are added dynamically from C++ by BuildVRGameSelectMenu() in
		// vr_gameselect.cpp.
		// Each item fires CCMD "vrselectgame <index>" handled in razemenu.cpp.

	The two halves differ from theirs in one respect worth stating. On the Quest
	they rewrite commandline.txt and restart the app. PC Raze already enumerates
	every installed game during startup - GrpScan, matched against grpinfo.txt by
	CRC - so the list comes for free, and switching relaunches the executable with
	a different -gamegrp rather than editing a file on the way out.

	Copyright (C) 2026 RazeXR PCVR port
*/

#ifndef VR_GAMESELECT_H
#define VR_GAMESELECT_H

#include "tarray.h"

struct GrpEntry;

// Handed the full scan from SetupGame, before it picks one to run. Kept so the
// menu can be built later without scanning the disk a second time.
void VRGameSelect_SetScannedGames(const TArray<GrpEntry>& games);

// Fills VRGameSelectMenu with one entry per installed game. Safe to call when
// the menu does not exist, or when only one game is installed, in which case it
// leaves the menu empty and Switch Game has nothing to show.
void BuildVRGameSelectMenu();

// How many switchable games were found. Zero or one means there is nothing to
// switch between.
int VRGameSelect_Count();

#endif
