/*
	vr_launchers.h - PC branch.

	Writes one .bat per installed game, for launching from the desktop or a
	front end.

	This began as an in-game Switch Game menu, modelled on VRaze's. That was
	dropped: switching needs a process relaunch either way, so a launcher per
	game does the same job without adding eight entries to menus that then push
	Quit off the bottom of the screen - and it fits how the games actually get
	started here, from LaunchBox.

	What survives from the menu work is the useful half. Raze already scans for
	installed games during startup and CRC-matches them against grpinfo.txt, so
	the list of what is present, and what each one is properly called, is
	already known. That is what the launchers are generated from, rather than
	guessing at folder names.

	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version. See package/common/gpl-2.0.txt.
*/

#ifndef VR_LAUNCHERS_H
#define VR_LAUNCHERS_H

#include "tarray.h"

struct GrpEntry;

// Handed the full scan from SetupGame, before it narrows to the one game being
// started. Includes add-ons: Duke's expansions and Blood's Cryptic Passage are
// worth their own launcher even though they were not worth switching to from
// inside a running game.
void VRLaunchers_SetScannedGames(const TArray<GrpEntry>& games);

#endif
