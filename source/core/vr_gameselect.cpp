/*
	vr_gameselect.cpp - PC branch. See vr_gameselect.h.

	Copyright (C) 2026 RazeXR PCVR port
*/

#include "vr_gameselect.h"

#include "gamecontrol.h"
#include "menu.h"
#include "c_dispatch.h"
#include "printf.h"
#include "cmdlib.h"
#include "i_specialpaths.h"
#include "zstring.h"
#include "m_argv.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

//==========================================================================
//
// The games found at startup, in the order the menu presents them.
//
//==========================================================================

struct VRGame
{
	FString name;		// what the player sees
	FString path;		// what goes to -gamegrp
};

static TArray<VRGame> Games;

void VRGameSelect_SetScannedGames(const TArray<GrpEntry>& games)
{
	Games.Clear();

	DPrintf(DMSG_NOTIFY, "VR game select: scan returned %u entries\n", games.Size());

	for (auto& g : games)
	{
		/*
			Add-ons are skipped. They are episodes layered on a base game -
			Duke's Nuclear Winter, Blood's Cryptic Passage - and depend on it
			being loaded, so they are not something to switch *to*. VRaze
			presents mods through a separate menu for the same reason.
		*/
		if (g.FileInfo.isAddon || (g.FileInfo.flags & GAMEFLAG_ADDON)) continue;

		VRGame e;
		e.name = g.FileInfo.name.IsNotEmpty() ? g.FileInfo.name : ExtractFileBase(g.FileName.GetChars(), true);
		e.path = g.FileName;
		Games.Push(e);
	}

	DPrintf(DMSG_NOTIFY, "VR game select: %u switchable\n", Games.Size());
}

int VRGameSelect_Count()
{
	return (int)Games.Size();
}

//==========================================================================
//
// Fills the menu VRaze's menudef declares.
//
//==========================================================================

void BuildVRGameSelectMenu()
{
	DMenuDescriptor** menu = MenuDescriptors.CheckKey("VRGameSelectMenu");
	if (menu == nullptr) return;

	auto desc = static_cast<DOptionMenuDescriptor*>(*menu);

	// Their menudef pads the top with StaticText, which must survive a rebuild,
	// so only previously added commands are dropped.
	for (int i = (int)desc->mItems.Size() - 1; i >= 0; i--)
	{
		if (desc->mItems[i]->mAction == NAME_None) continue;
		desc->mItems.Delete(i);
	}

	if (Games.Size() < 2) return;	// nothing to switch between

	for (unsigned i = 0; i < Games.Size(); i++)
	{
		auto it = CreateOptionMenuItemCommand(Games[i].name.GetChars(),
			FStringf("vrselectgame %u", i), true);
		desc->mItems.Push(it);
	}
}

//==========================================================================
//
// Relaunching
//
// GameMain calls RunGame exactly once and there is no way back into it: the
// file system, the tile store, the ZScript VM and GameStartupInfo are all set
// up during that call. GZDoom has never supported switching IWAD without a
// restart and Raze inherits that, so the game is started again with a different
// -gamegrp rather than reloaded in place.
//
// Theirs rewrites commandline.txt and relies on the Android launcher to bring
// the app back. On PC the process can simply start its successor.
//
//==========================================================================

static FString QuoteArg(const char* arg)
{
	FString s = "\"";
	s << arg << "\"";
	return s;
}

static bool RelaunchWithGame(const char* grpPath)
{
#ifdef _WIN32
	wchar_t exe[MAX_PATH];
	if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0) return false;

	/*
		Rebuild the command line, dropping any -gamegrp that is already there
		along with its value, and any -map, which belongs to the game being left
		behind. Everything else - the config, the voxel weapon pack, -nosetup -
		is carried over so the new game starts the way this one did.
	*/
	FString cmd = QuoteArg(FString(exe).GetChars());

	for (int i = 1; i < Args->NumArgs(); i++)
	{
		const char* a = Args->GetArg(i);
		if (!stricmp(a, "-gamegrp") || !stricmp(a, "-map"))
		{
			i++;	// skip its value too
			continue;
		}

		// A +vrselectgame left on the command line would make the new
		// process switch again the moment it started, and so on forever.
		if (!stricmp(a, "+vrselectgame"))
		{
			i++;
			continue;
		}
		cmd << " " << QuoteArg(a);
	}

	cmd << " -gamegrp " << QuoteArg(grpPath);

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = {};

	auto wide = cmd.WideString();
	std::wstring buf = wide.c_str();

	BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE,
		0, nullptr, nullptr, &si, &pi);

	if (ok)
	{
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	return ok != 0;
#else
	(void)grpPath;
	return false;
#endif
}

//==========================================================================
//
// The CCMD their menudef fires.
//
//==========================================================================

CCMD(vrselectgame)
{
	if (argv.argc() < 2)
	{
		Printf("vrselectgame <index>: switch to one of the installed games\n");
		for (unsigned i = 0; i < Games.Size(); i++)
			Printf("  %u  %s\n", i, Games[i].name.GetChars());
		return;
	}

	int idx = atoi(argv[1]);
	if (idx < 0 || idx >= (int)Games.Size())
	{
		Printf("vrselectgame: no game %d\n", idx);
		return;
	}

	Printf("Switching to %s\n", Games[idx].name.GetChars());

	if (!RelaunchWithGame(Games[idx].path.GetChars()))
	{
		Printf(TEXTCOLOR_RED "Could not start a new instance - staying in this game.\n");
		return;
	}

	// Leave through the ordinary quit path so the config is written and the
	// OpenXR session is ended before the new process claims the headset.
	AddCommandString("quit");
}
