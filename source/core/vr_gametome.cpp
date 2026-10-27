/*
	PC branch: the Game Tome, the game's manual as a book in the off hand.

	The book itself is not here. It ships as its own OpenXR API layer beside
	raze.exe (gametome\layer) with one built book per game (gametome\tome-*),
	and TBXR_PC.cpp asks for it. This file is the game's half:

	  - which book: the running game's, named every frame so the open-book
	    gesture (both palms up, a second; the layer's own) opens the right one
	  - which hand turns the pages: the gun hand, so the Leftorium swaps it
	  - the VR Options row, vr_game_tome
	  - while the book is open in play, the game holds still, as it does under
	    the pause menu, without the wait for the next tick that a paused loop
	    would otherwise do (that wait dropped Doom VR's book to 35 fps)

	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation; either version 2 of the License, or (at your option)
	any later version.
*/

#include "c_dispatch.h"
#include "c_cvars.h"
#include "printf.h"
#include "gamecontrol.h"
#include "gamestate.h"
#include "menustate.h"
#include "i_net.h"
#include "vr_gametome.h"

void TBXR_GameTomeSet(const char *book, int mainHand);	// TBXR_PC.cpp
bool TBXR_GameTomeShowing(void);
bool TBXR_GameTomeOpen(void);

EXTERN_CVAR(Int, vr_control_scheme)

static bool bookPause = false;

// The book that goes with the running game. Every Duke release, its add-ons
// and World Tour share Duke's manual; Rides Again has its own.
static const char *VR_GameTomeBook()
{
	if (isRRRA())		return "tome-ridesagain";
	if (isRR())			return "tome-redneck";
	if (isWW2GI())		return "tome-ww2gi";
	if (isNam())		return "tome-nam";
	if (isDukeLike())	return "tome-duke";
	if (isBlood())		return "tome-blood";
	if (isSWALL())		return "tome-shadowwarrior";
	if (isExhumed())	return "tome-powerslave";
	return "";
}

void VR_GameTomeFrame()
{
	// The Leftorium (scheme 10 and up) puts the gun in the left hand, and the
	// gun hand is the one that turns the pages.
	TBXR_GameTomeSet(VR_GameTomeBook(), vr_control_scheme >= 10 ? 0 : 1);

	// Opened by the gesture in play: hold the game still until it closes.
	// Opened from the menu, the menu is already holding it.
	bookPause = TBXR_GameTomeShowing() && !netgame && gamestate == GS_LEVEL && menuactive == MENU_Off;
}

bool VR_GameTomePause()
{
	return bookPause;
}

CCMD(vr_game_tome)
{
	TBXR_GameTomeSet(VR_GameTomeBook(), vr_control_scheme >= 10 ? 0 : 1);
	TBXR_GameTomeOpen();
}
