/*
	vr_launchers.cpp - PC branch. See vr_launchers.h.

	Copyright (C) 2026 RazeXR PCVR port

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version. See package/common/gpl-2.0.txt.
*/

#include "vr_launchers.h"

#include "gamecontrol.h"
#include "c_dispatch.h"
#include "printf.h"
#include "cmdlib.h"
#include "zstring.h"
#include "m_argv.h"
#include "filesystem.h"
#include "menu.h"

#include <algorithm>

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
	/*
		Duke Nukem 3D and its three expansions, and nothing else. The community
		voxel pack replaces Duke's own tiles, so it must not be loaded into NAM,
		WW2GI or Redneck, which run on Duke's module but have their own art.
	*/
	bool isDuke = false;
	/*
		World Tour, told apart by the game filter rather than by its file name,
		which is DUKE3D.GRP like every other Duke release. Episode five lives in
		loose scripts beside the GRP whose names collide with the Atomic ones in
		the GRP itself, so setup copies those three under a WT_ prefix. Where
		that copy exists, this launcher points the engine at it and World Tour
		runs with all five episodes; where it does not, the launcher is exactly
		what it was and the game runs its first four.
	*/
	bool isWorldTour = false;
	/*
		The community voxel pack for this game, if it has one - monsters, props
		and scenery as voxel models. Duke is the exception and carries its own
		two-file rule below, because two different packs exist for it.
	*/
	FString voxPack;
};

static TArray<VRGame> Games;

//==========================================================================
//
// The same game found twice
//
// Raze looks for game data in the portable games/ folder and, on its own, in
// every Steam and GOG install it can find. Somebody who owns Duke on Steam and
// has also had it copied into games/ gets both, under one name - and since the
// launcher file is named after the game, the one written last silently wins.
// That put "Duke it out in D.C" in the run folder pointing at C:\Program
// Files, which is the exact thing the portable copy exists to avoid.
//
// So collapse duplicates by name and keep the copy that lives beside raze.exe.
// A game found ONLY outside the run folder is still kept - that is how someone
// who has never run setup gets launchers at all, and how a game they own but
// have not copied in, like Duke!ZONE II, becomes available.
//
//==========================================================================

static void PreferLocalCopies()
{
	FString here = progdir;
	FixPathSeperator(here);
	here.ToLower();
	while (here.Len() > 1 && here.Back() == '/') here.Truncate(here.Len() - 1);

	auto isLocal = [&here](const VRGame& g) -> bool
	{
		if (here.Len() <= 1) return false;
		FString p = g.path;
		FixPathSeperator(p);
		p.ToLower();
		return p.IndexOf(here) == 0;
	};

	for (int i = (int)Games.Size() - 1; i >= 0; i--)
	{
		for (int j = 0; j < i; j++)
		{
			if (Games[i].name.CompareNoCase(Games[j].name) != 0) continue;

			// Same game twice. The local copy wins; if neither is local, or
			// both are, the one found first stays.
			if (isLocal(Games[i]) && !isLocal(Games[j])) Games[j] = Games[i];
			Games.Delete(i);
			break;
		}
	}
}

//==========================================================================

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
		e.isDuke = (g.FileInfo.flags & GAMEFLAG_DUKE) != 0;
		e.isWorldTour = g.FileInfo.gamefilter.CompareNoCase("Duke.WorldTour") == 0;

		// Named for the game rather than for whoever packaged it, so setup can
		// change where a pack comes from without the launchers caring.
		if (g.FileInfo.flags & GAMEFLAG_BLOOD)          e.voxPack = "voxels_blood.zip";
		else if (g.FileInfo.flags & GAMEFLAG_SW)        e.voxPack = "voxels_sw.zip";
		else if (g.FileInfo.flags & GAMEFLAG_PSEXHUMED) e.voxPack = "voxels_exhumed.zip";

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
		/*
			A colon is illegal in a filename and it is the only punctuation
			these names actually use, always to separate a game from its
			edition: "Duke Nukem 3D: Atomic Edition", "BLOOD: One Unit Whole
			Blood". Dropping it ran the two halves together. It becomes " - ",
			and the space that follows it in the source is swallowed so the
			result is not "3D -  Atomic".
		*/
		if (*c == ':')
		{
			if (out.Len() > 0)
			{
				out << " - ";
				lastWasSpace = true;
			}
			continue;
		}

		// Anything else Windows will not take in a filename, plus a few that
		// are legal but awkward to type at a prompt.
		if (strchr("\\/*?\"<>|", *c) != nullptr) continue;

		if (*c == ' ')
		{
			if (lastWasSpace) continue;
			lastWasSpace = true;
		}
		else lastWasSpace = false;

		out << *c;
	}

	while (out.Len() > 0 && (out.Back() == ' ' || out.Back() == '-')) out.Truncate(out.Len() - 1);
	if (out.IsEmpty()) out = "Raze";
	return out;
}

//==========================================================================
//
// What the file is called: "<game> VR.bat".
//
// The suffix is there because these sit in a folder the player opens to pick a
// game, often alongside the flat versions of the same games, and "Duke Nukem 3D
// - Atomic Edition VR.bat" says which one it is without opening it. Appending
// after the trailing-dot trim rather than before is what keeps "Duke it out in
// D.C." its full stop - a trailing dot is illegal, one in the middle is not.
//
//==========================================================================

static FString LauncherName(const char* gameName)
{
	FString base = SafeFileName(gameName);
	while (base.Len() > 0 && base.Back() == ' ') base.Truncate(base.Len() - 1);
	base << " VR";
	return base;
}

//==========================================================================

// Defined with the Switch Game menu below; used here to tell a launcher of ours
// from any other .bat sitting in the folder.
static bool ReadLauncherName(const char* path, FString& nameOut);

CCMD(vrwritelaunchers)
{
	PreferLocalCopies();

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
	TArray<FString> writtenFiles;

	for (auto& g : Games)
	{
		FString base = LauncherName(g.name.GetChars());
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
			The Duke3D Voxel Pack, if the user has installed it. Duke and its
			expansions only - it replaces Duke's own tiles.

			Not shipped with this port and not ours to ship: it is ReaperMan and
			the Duke4.net community's, under a non-commercial share-alike art
			licence. The user downloads it from its own release page and drops it
			in; the launcher picks it up if it is there and says nothing if not.
		*/
		if (g.isDuke)
		{
			/*
				Either Duke voxel pack, whichever is present. They replace the
				same tiles, so loading both would leave the result to whichever
				parsed last - the second test overrides the first rather than
				adding to it.

				Voxel Duke 3D wins because it is a superset in the way that
				matters: it covers the monsters, which the older pack does not
				beyond a few machines. Its readme says eDuke32 only; that is
				untested-elsewhere rather than incompatible, and Raze loads its
				duke3d.def without complaint.
			*/
			body << "rem A Duke voxel pack, if one has been installed beside this script.\r\n";
			body << "rem Voxel Duke 3D wins where both are present - it covers monsters too.\r\n";
			body << "set \"VOX=\"\r\n";
			body << "if exist \"%~dp0duke3d_voxels.zip\" set \"VOX=-file \"%~dp0duke3d_voxels.zip\"\"\r\n";
			body << "if exist \"%~dp0voxel_duke3d.zip\" set \"VOX=-file \"%~dp0voxel_duke3d.zip\"\"\r\n";
			body << "\r\n";
		}
		/*
			Every other game that has one. Blood, Shadow Warrior and Exhumed all
			have community voxel packs of their own - props, scenery and some
			monsters - which setup fetches from their authors' own repositories.
			Each ships a <game>-raze.def, so dropping the archive in is the whole
			installation; the launcher only has to name it.
		*/
		else if (g.voxPack.IsNotEmpty())
		{
			body << "rem The voxel pack for this game, if setup was able to fetch it.\r\n";
			body << "set \"VOX=\"\r\n";
			body << "if exist \"%~dp0" << g.voxPack << "\" set \"VOX=-file \"%~dp0" << g.voxPack << "\"\"\r\n";
			body << "\r\n";
		}
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
			body << "\"%~dp0raze.exe\" -nosetup -portable -route66 -gamegrp \"%GRP%\" %VRW% ";
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
			writtenFiles.Push(base + ".bat");
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
		// -portable: look for game data in this folder only, never in Steam
		// or GOG. See CollectSearchPaths - it is what makes -gamegrp below
		// resolve to the copy sitting beside this script, every time.
		body << "\"%~dp0raze.exe\" -nosetup -portable";

		/*
			Episode five. WT_GAME.CON is World Tour's own GAME.CON with its
			include lines repointed at the other two renamed copies; naming it
			here is what brings in FLAMETHROWER.CON, FIREFLYTROOPER.CON and
			EPISODE5BOSS.CON, and the definevolumename that puts Alien World
			Order in the episode list. Written by setup, from the user's own
			World Tour install, and absent for everyone else.
		*/
		if (g.isWorldTour)
		{
			FString wtcon = ExtractFilePath(g.path.GetChars());
			if (wtcon.Len() > 0 && wtcon.Back() != '/' && wtcon.Back() != '\\') wtcon += '/';
			wtcon += "WT_GAME.CON";
			if (FileExists(wtcon.GetChars())) body << " -con WT_GAME.CON";
		}

		body << " -gamegrp \"%GRP%\"";
		if (g.isDuke || g.voxPack.IsNotEmpty()) body << " %VOX%";
		body << " %VRW% ";
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
		writtenFiles.Push(base + ".bat");
		written++;

		/*
			A second launcher for World Tour, where setup has built it.

			Episode five rides on whichever Duke GRP is present - Alien World
			Order depends on the Atomic one - so keying this on the World Tour
			GRP was wrong: a user whose Duke is the Atomic release got the
			episode installed and no way to reach it. What decides it is
			WT_GAME.CON sitting beside the data, which is setup's own mark that
			it built the thing.

			Two launchers, because they are two different games to a player:
			the base one keeps its four episodes, and this one has five.
		*/
		if (g.isDuke && !g.isAddon && !g.isRoute66)
		{
			FString wtcon = ExtractFilePath(g.path.GetChars());
			if (wtcon.Len() > 0 && wtcon.Back() != '/' && wtcon.Back() != '\\') wtcon += '/';
			wtcon += "WT_GAME.CON";

			if (FileExists(wtcon.GetChars()))
			{
				FString wtbase = "Duke Nukem 3D - World Tour VR";
				FString wtbody = body;
				wtbody.Substitute("-nosetup -portable", "-nosetup -portable -con WT_GAME.CON");
				FString oldcfg, newcfg;
				oldcfg.Format("cfg_%s.ini", base.GetChars());
				newcfg.Format("cfg_%s.ini", wtbase.GetChars());
				wtbody.Substitute(oldcfg.GetChars(), newcfg.GetChars());

				FString wtfile;
				wtfile.Format("%s/%s.bat", dir.GetChars(), wtbase.GetChars());
				FileWriter* ww = FileWriter::Open(wtfile.GetChars());
				if (ww != nullptr)
				{
					ww->Write(wtbody.GetChars(), wtbody.Len());
					delete ww;
					Printf("  %s   (episode five)\n", wtbase.GetChars());
					writtenFiles.Push(wtbase + ".bat");
					written++;
				}
			}
		}
	}

	/*
		Take away the launchers this run did not write.

		Only ones carrying our own marker line are touched, so SETUP.bat and
		anything the player keeps here are safe. Without this a game that is no
		longer installed, or one whose name has changed, leaves a .bat behind
		that still looks valid - and the Switch Game menu reads the folder, so
		a stale file becomes a menu entry that starts nothing.
	*/
	int removed = 0;
	{
		FString pattern;
		pattern.Format("%s/*.bat", dir.GetChars());
		std::wstring wpattern = pattern.WideString().c_str();

		WIN32_FIND_DATAW fd = {};
		HANDLE h = FindFirstFileW(wpattern.c_str(), &fd);
		if (h != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

				FString name = FString(fd.cFileName);
				bool ours = false;
				for (auto& w : writtenFiles)
				{
					if (w.CompareNoCase(name) == 0) { ours = true; break; }
				}
				if (ours) continue;

				// Not written this time. Ours to remove only if it carries the
				// marker; a name we cannot read is left alone.
				FString full;
				full.Format("%s/%s", dir.GetChars(), name.GetChars());
				FString ignored;
				if (!ReadLauncherName(full.GetChars(), ignored)) continue;

				if (remove(full.GetChars()) == 0)
				{
					Printf("  removed %s   (no longer installed)\n", name.GetChars());
					removed++;
				}
			}
			while (FindNextFileW(h, &fd));
			FindClose(h);
		}
	}

	Printf("Wrote %d launcher%s to %s\n", written, written == 1 ? "" : "s", dir.GetChars());
	if (removed > 0) Printf("Removed %d stale launcher%s\n", removed, removed == 1 ? "" : "s");
#else
	Printf("vrwritelaunchers is only implemented for Windows.\n");
#endif
}

//==========================================================================
//
// The Switch Game menu
//
// It lists the launchers rather than the games, and starts the chosen .bat
// rather than rebuilding a command line.
//
// That is not a shortcut, it is the only correct source. A launcher carries
// more than -gamegrp: the game's own config, the voxel weapon pack, the Duke
// voxel pack for Duke and its expansions only, and Route 66's -route66 and
// base GRP. The first attempt at this menu carried the running process's
// command line over to the next game, which would have handed Blood Duke's
// config and Duke's voxel pack. Reading the launchers means switching lands
// in exactly what double-clicking that game does, and there stays one place
// where how a game starts is decided.
//
// The list is filled when the menu is opened, not when the menus are built.
// M_CreateMenus runs before the startup scan, so anything built there is built
// from nothing - which is why the first version came up empty in the headset
// while the console command it fires worked perfectly.
//
//==========================================================================

struct VRLauncher
{
	FString name;	// what the player sees, taken from the launcher itself
	FString file;	// the .bat to start
};

static TArray<VRLauncher> Launchers;

// Set by the menu, acted on by the main loop. -1 is "nothing asked for".
static int PendingSwitch = -1;

// Every launcher vrwritelaunchers writes carries this line, and nothing else
// does. It is what tells a game launcher from PLAY.bat, SETUP.bat or whatever
// else the user keeps beside raze.exe.
static const char* const LauncherMarker = "Written by the vrwritelaunchers console command.";

//==========================================================================
//
// A launcher's second line is "rem <the name grpinfo gave the game>", which is
// the name the menu shows. Anything without the marker is not ours.
//
//==========================================================================

static bool ReadLauncherName(const char* path, FString& nameOut)
{
	FileReader fr;
	if (!fr.OpenFile(path)) return false;

	char buf[1024];
	auto got = fr.Read(buf, sizeof(buf) - 1);
	if (got <= 0) return false;
	buf[got] = 0;

	if (strstr(buf, LauncherMarker) == nullptr) return false;

	const char* p = strstr(buf, "rem ");
	if (p == nullptr) return false;
	p += 4;

	const char* end = p;
	while (*end != 0 && *end != '\r' && *end != '\n') end++;

	nameOut = FString(p, end - p);
	nameOut.StripRight();
	return nameOut.IsNotEmpty();
}

//==========================================================================

static void ScanLaunchers()
{
	Launchers.Clear();

#ifdef _WIN32
	FString dir = progdir;
	FixPathSeperator(dir);
	while (dir.Len() > 1 && dir.Back() == '/') dir.Truncate(dir.Len() - 1);
	if (dir.IsEmpty()) dir = ".";

	FString pattern;
	pattern.Format("%s/*.bat", dir.GetChars());
	std::wstring wpattern = pattern.WideString().c_str();

	WIN32_FIND_DATAW fd = {};
	HANDLE h = FindFirstFileW(wpattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return;

	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

		VRLauncher e;
		e.file.Format("%s/%s", dir.GetChars(), FString(fd.cFileName).GetChars());
		if (!ReadLauncherName(e.file.GetChars(), e.name)) continue;
		Launchers.Push(e);
	}
	while (FindNextFileW(h, &fd));

	FindClose(h);

	// Alphabetical, which is the order they appear in the folder and in the
	// front end, so the menu is not a third arrangement to learn.
	std::sort(Launchers.begin(), Launchers.end(), [](const VRLauncher& a, const VRLauncher& b)
		{ return stricmp(a.name.GetChars(), b.name.GetChars()) < 0; });
#endif
}

//==========================================================================
//
// Fills the menu the menudef declares. Called every time it is opened.
//
//==========================================================================

void BuildVRGameSelectMenu()
{
	DMenuDescriptor** menu = MenuDescriptors.CheckKey("VRGameSelectMenu");
	if (menu == nullptr) return;

	auto desc = static_cast<DOptionMenuDescriptor*>(*menu);

	// Whatever the menudef itself declared is kept; everything a previous build
	// of this menu added goes. Counting once is what makes a rebuild idempotent
	// - matching on "has no action" would have kept accumulating the static
	// text used to report an empty list.
	static int declaredItems = -1;
	if (declaredItems < 0) declaredItems = (int)desc->mItems.Size();
	while ((int)desc->mItems.Size() > declaredItems) desc->mItems.Delete(desc->mItems.Size() - 1);

	ScanLaunchers();

	DPrintf(DMSG_NOTIFY, "Switch Game: %u launcher%s found\n",
		Launchers.Size(), Launchers.Size() == 1 ? "" : "s");

	if (Launchers.Size() == 0)
	{
		// Say so on screen. An empty menu in a headset looks like a broken
		// build, and the fix is one console command.
		desc->mItems.Push(CreateOptionMenuItemStaticText("No game launchers were found"));
		desc->mItems.Push(CreateOptionMenuItemStaticText("beside raze.exe. Run vrwritelaunchers"));
		desc->mItems.Push(CreateOptionMenuItemStaticText("at the console to write them."));
		return;
	}

	for (unsigned i = 0; i < Launchers.Size(); i++)
	{
		desc->mItems.Push(CreateOptionMenuItemCommand(Launchers[i].name.GetChars(),
			FStringf("vrselectgame %u", i), true));
	}

	desc->mScrollPos = 0;
	desc->mSelectedItem = -1;
}

//==========================================================================
//
// Starting the next game
//
// GameMain calls RunGame exactly once and there is no way back into it - the
// file system, the tile store, the ZScript VM and GameStartupInfo all belong
// to that call - so switching is a new process, not a reload. GZDoom has never
// supported changing IWAD without a restart and Raze inherits that.
//
//==========================================================================

static bool StartLauncher(const char* bat)
{
#ifdef _WIN32
	/*
		Through cmd, because a .bat is not an executable, and after a wait.

		The wait is the part that matters. This process still holds the OpenXR
		session when the successor is created, and the runtime does not hand
		the headset over until this one is actually gone. Without it the new
		process can reach its own session creation first and be refused, which
		presents as a game that launches to a black headset for no visible
		reason. Six pings is about five seconds - far longer than the quit path
		needs, and nothing beside the load that follows.

		ping rather than timeout: timeout exits immediately with an error when
		it cannot read console input, which is exactly the case here.
	*/
	FString cmd;
	cmd.Format("cmd.exe /c ping -n 6 127.0.0.1 >nul & \"%s\"", bat);

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOWMINNOACTIVE;

	PROCESS_INFORMATION pi = {};

	std::wstring buf = cmd.WideString().c_str();

	BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE,
		CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi);

	if (ok)
	{
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	return ok != 0;
#else
	(void)bat;
	return false;
#endif
}

//==========================================================================
//
// The CCMD each menu entry fires. Also usable on its own, which is how this
// gets tested without a headset.
//
//==========================================================================

CCMD(vrselectgame)
{
	if (Launchers.Size() == 0) ScanLaunchers();

	if (Launchers.Size() == 0)
	{
		Printf("No game launchers beside raze.exe. Run vrwritelaunchers first.\n");
		return;
	}

	if (argv.argc() < 2)
	{
		Printf("vrselectgame <index>: leave this game and start another\n");
		for (unsigned i = 0; i < Launchers.Size(); i++)
			Printf("  %u  %s\n", i, Launchers[i].name.GetChars());
		return;
	}

	int idx = atoi(argv[1]);
	if (idx < 0 || idx >= (int)Launchers.Size())
	{
		Printf("vrselectgame: no game %d\n", idx);
		return;
	}

	Printf("Switching to %s\n", Launchers[idx].name.GetChars());

	/*
		Only booked here. The launcher is started and the game left from the
		main loop, one frame from now - see VRLaunchers_StartPendingSwitch.
	*/
	PendingSwitch = idx;
}

//==========================================================================
//
// Called at the top of the main loop, outside everything.
//
// Leaving from inside the menu item's own command is what crashed a switch out
// of Duke it out in D.C. with an access violation. That command runs as a
// native call from OptionMenuItemCommand.Activate, which is ZScript, which is
// JIT compiled - so `quit`, which is a `throw CExitEvent`, was unwinding a C++
// exception straight out through a VM frame. It survived that most of the time,
// which is the worst way for it to behave.
//
// Nothing else in the engine quits this way. Raze's own Quit is a menu that
// opens a message box, and the message box's handler clears the menus before it
// exits. So the switch is booked by the menu and carried out here instead,
// where the throw happens in ordinary loop code with nothing of the VM's on the
// stack, exactly as any other exit does.
//
//==========================================================================

bool VRLaunchers_StartPendingSwitch()
{
	if (PendingSwitch < 0) return false;

	int idx = PendingSwitch;
	PendingSwitch = -1;

	if (idx >= (int)Launchers.Size()) return false;

	if (!StartLauncher(Launchers[idx].file.GetChars()))
	{
		Printf(TEXTCOLOR_RED "Could not start %s - staying in this game.\n", Launchers[idx].file.GetChars());
		return false;
	}

	// The caller exits through the ordinary path from here, so the config is
	// written and the OpenXR session is ended rather than abandoned.
	return true;
}
