/*
	vr_launchers.h - PC branch.

	Two halves of one idea.

	vrwritelaunchers writes one .bat per installed game, for launching from the
	desktop or a front end. Raze already scans for installed games at startup
	and CRC-matches them against grpinfo.txt, so what is present and what each
	one is properly called is already known; the launchers are generated from
	that rather than from guesses at folder names.

	The Switch Game menu then reads those launchers back and starts one. A
	launcher carries more than -gamegrp - the game's own config, the voxel
	weapon packs, Route 66's switches - so reading them keeps a single place
	where how a game starts is decided, and makes switching from the menu land
	in exactly what double-clicking that game does.

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

// Fills the VRGameSelectMenu descriptor from the launchers sitting beside
// raze.exe. Called from M_SetMenu each time the menu is opened, because the
// menus are created before the startup scan has found anything.
void BuildVRGameSelectMenu();

#endif
