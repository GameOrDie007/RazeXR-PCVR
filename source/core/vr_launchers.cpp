/*
	vr_launchers.cpp - PC branch. See vr_launchers.h.

	Copyright (C) 2026 RazeXR PCVR port
*/

#include "vr_launchers.h"

#include "gamecontrol.h"
#include "c_dispatch.h"
#include "printf.h"
#include "cmdlib.h"
#include "zstring.h"
#include "m_argv.h"
#include "filesystem.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

struct VRGame
{
	FString name;		// as grpinfo.txt names it
	FString path;		// what goes to -gamegrp
	bool isAddon = false;
	/*
		Route 66 has no archive of its own - grpinfo identifies it by loose
		files sitting beside Redneck's GRP - so there is nothing to hand to
		-gamegrp and the engine has a dedicated switch for it instead, which
		also sets the CON, the replacement art and four renames.
	*/
	bool isRoute66 = false;
};

static TArray<VRGame> Games;

void VRLaunchers_SetScannedGames(const TArray<GrpEntry>& games)
{
	Games.Clear();

	for (auto& g : games)
	{
		VRGame e;
		e.name = g.FileInfo.name.IsNotEmpty() ? g.FileInfo.name : ExtractFileBase(g.FileName.GetChars(), true);
		e.path = g.FileName;
		e.isAddon = g.FileInfo.isAddon || (g.FileInfo.flags & GAMEFLAG_ADDON) != 0;
		e.isRoute66 = (g.FileInfo.flags & GAMEFLAG_ROUTE66) != 0;
		Games.Push(e);
	}
}

//==========================================================================
//
// A game's name becomes its filename, so it has to survive being one.
//
//==========================================================================

static FString SafeFileName(const char* name)
{
	FString out;
	bool lastWasSpace = false;

	for (const char* c = name; *c; c++)
	{
		// Anything Windows will not take in a filename, plus a few that are
		// legal but awkward to type at a prompt.
		if (strchr("\\/:*?\"<>|", *c) != nullptr) continue;

		if (*c == ' ')
		{
			if (lastWasSpace) continue;
			lastWasSpace = true;
		}
		else lastWasSpace = false;

		out << *c;
	}

	while (out.Len() > 0 && (out.Back() == ' ' || out.Back() == '.')) out.Truncate(out.Len() - 1);
	if (out.IsEmpty()) out = "Raze";
	return out;
}

//==========================================================================

CCMD(vrwritelaunchers)
{
	if (Games.Size() == 0)
	{
		Printf("No games were found to write launchers for.\n");
		return;
	}

#ifdef _WIN32
	// progdir is where raze.exe lives, which is where the launchers belong.
	FString dir = progdir;
	FixPathSeperator(dir);
	while (dir.Len() > 1 && dir.Back() == '/') dir.Truncate(dir.Len() - 1);
	if (dir.IsEmpty()) dir = ".";

	int written = 0;

	for (auto& g : Games)
	{
		FString base = SafeFileName(g.name.GetChars());
		FString file;
		file.Format("%s/%s.bat", dir.GetChars(), base.GetChars());

		FString body;
		body << "@echo off\r\n";
		body << "rem " << g.name << "\r\n";
		body << "rem Written by the vrwritelaunchers console command.\r\n";
		body << "rem Start Virtual Desktop and connect the headset before running this.\r\n";
		body << "setlocal\r\n";
		body << "cd /d \"%~dp0\"\r\n";
		body << "\r\n";
		body << "rem The voxel weapon pack, if it has been built.\r\n";
		body << "set \"VRW=\"\r\n";
		/*
			One pair of quotes, not two.

			This used to emit set "VRW=-file ""<path>""", whose doubled quotes cmd
			reads as an empty string followed by an unquoted path - so the argument
			splits at the first space. It survived every test because the folder it
			was written in had no spaces in its name; the moment the build was
			assembled into "RazeXR (PC)" the voxel pack stopped loading, silently,
			in every game.
		*/
		body << "if exist \"%~dp0vrweapons.pk3\" set \"VRW=-file \"%~dp0vrweapons.pk3\"\"\r\n";
		body << "\r\n";
		/*
			Game data. A copy sitting beside the launcher wins, so the whole run
			folder can be moved to another PC; the absolute path the scan found
			is kept as the fallback, so nothing changes on the machine that
			wrote it.

			The portable form is the last two components of the scanned path -
			<game folder>/<file> - which is the shape every Build game's data
			takes here, and the same shape the search path addition walks.
		*/
		if (g.isRoute66)
		{
			/*
				-route66 does the rest of the setup - the CON, the replacement
				art and four renames - but it points -gamegrp at the bare name
				"REDNECK.GRP", and a bare name contributes no search path, so
				nothing is found and startup dies. -gamegrp is read after the
				switch and overrides it, so passing the base game's full path
				alongside fixes that without losing anything -route66 does.
			*/
			FString basegrp;
			for (auto& other : Games)
			{
				FString lower = other.path;
				lower.ToLower();
				FixPathSeperator(lower);
				if (lower.Right(11).Compare("redneck.grp") == 0)
				{
					basegrp = other.path;
					break;
				}
			}

			if (basegrp.IsEmpty())
			{
				Printf(TEXTCOLOR_YELLOW "  %s: skipped, its base game was not found\n", base.GetChars());
				continue;
			}

			/*
				Route 66 crashes on startup with the full voxel pack - exit code
				0xC0000409, a stack buffer overrun, before the first frame. It is
				GAME66.CON together with the *other* games' def files: Redneck's
				own three are fine, and so are all hundred models. Measured by
				bisecting the pack, and it reproduces without -route66, from
				-con GAME66.CON alone.

				vrweapons_rr.pk3 carries Redneck's defs and the models only, so
				Route 66 gets the same thirteen weapons without tripping it.
			*/
			body << "rem Route 66 trips a fault in the full pack, so it takes the\r\n";
			body << "rem Redneck-only one, which gives it the same weapons.\r\n";
			body << "set \"VRW=\"\r\n";
			body << "if exist \"%~dp0vrweapons_rr.pk3\" set \"VRW=-file \"%~dp0vrweapons_rr.pk3\"\"\r\n";
			body << "\r\n";

			FString rel66 = basegrp;
			rel66.Substitute("\\", "/");
			FString tail66 = rel66;
			{
				ptrdiff_t slash = rel66.LastIndexOf('/');
				if (slash > 0)
				{
					ptrdiff_t prev = rel66.LastIndexOf('/', slash - 1);
					tail66 = prev >= 0 ? rel66.Mid(prev + 1) : rel66.Mid(slash + 1);
				}
			}
			tail66.Substitute("/", "\\");

			body << "rem Route 66 has no archive of its own. -route66 sets the CON, the\r\n";
			body << "rem art and the renames; the base game still has to be named in full.\r\n";
			body << "set \"GRP=%~dp0games\\" << tail66 << "\"\r\n";
			body << "if not exist \"%GRP%\" set \"GRP=" << basegrp << "\"\r\n";
			body << "\r\n";
			body << "\"%~dp0raze.exe\" -nosetup -route66 -gamegrp \"%GRP%\" %VRW% ";
			body << "-config \"%~dp0cfg_" << base << ".ini\" +logfile \"%~dp0raze.log\"\r\n";

			FileWriter* w66 = FileWriter::Open(file.GetChars());
			if (w66 == nullptr)
			{
				Printf(TEXTCOLOR_RED "Could not write %s\n", file.GetChars());
				continue;
			}
			w66->Write(body.GetChars(), body.Len());
			delete w66;
			Printf("  %s%s\n", base.GetChars(), g.isAddon ? "   (add-on)" : "");
			written++;
			continue;
		}

		FString rel = g.path;
		rel.Substitute("\\", "/");
		FString tail = rel;
		{
			ptrdiff_t slash = rel.LastIndexOf('/');
			if (slash > 0)
			{
				ptrdiff_t prev = rel.LastIndexOf('/', slash - 1);
				tail = prev >= 0 ? rel.Mid(prev + 1) : rel.Mid(slash + 1);
			}
		}
		tail.Substitute("/", "\\");

		body << "rem Game data. A copy in games\\ beside this script wins, so this\r\n";
		body << "rem folder can be moved to another PC; otherwise where it was found.\r\n";
		body << "set \"GRP=%~dp0games\\" << tail << "\"\r\n";
		body << "if not exist \"%GRP%\" set \"GRP=" << g.path << "\"\r\n";
		body << "\r\n";
		body << "\"%~dp0raze.exe\" -nosetup -gamegrp \"%GRP%\" %VRW% ";
		body << "-config \"%~dp0cfg_" << base << ".ini\" +logfile \"%~dp0raze.log\"\r\n";

		FileWriter* w = FileWriter::Open(file.GetChars());
		if (w == nullptr)
		{
			Printf(TEXTCOLOR_RED "Could not write %s\n", file.GetChars());
			continue;
		}
		w->Write(body.GetChars(), body.Len());
		delete w;

		Printf("  %s%s\n", base.GetChars(), g.isAddon ? "   (add-on)" : "");
		written++;
	}

	Printf("Wrote %d launcher%s to %s\n", written, written == 1 ? "" : "s", dir.GetChars());
#else
	Printf("vrwritelaunchers is only implemented for Windows.\n");
#endif
}
