<#
    RazeXR PCVR - one-file setup.

    Finds the Build games you already own on Steam and GOG, copies their data
    into this folder, fetches the optional extras, and writes a launcher for
    every game it ends up with.

    Nothing here is downloaded from us and no game data ships with this port.
    Everything comes from your own installs or from the projects that publish it.

        -InPlace     point the launchers at the games where they are instead of
                     copying, for when disk space matters more than being able
                     to move this folder to another PC
        -NoDownload  skip the network step entirely
        -Root <path> extra folder to search for game data
        -VRaze <pk3> a VRaze raze.pk3, if you have one. The voxel weapons work
                     without it; this adds the weapon animation frames and the
                     four games whose models are not the games' own.
#>

param(
    [switch]$InPlace,
    [switch]$NoDownload,
    [string]$Root,
    [string]$VRaze
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (Test-Path (Join-Path $here "raze.exe")) { $dest = $here }
else { $dest = Split-Path -Parent $here }     # running from tools/

$exe = Join-Path $dest "raze.exe"
if (-not (Test-Path $exe)) {
    Write-Host "ERROR: raze.exe not found next to this script." -ForegroundColor Red
    exit 1
}

function Line($t) { Write-Host $t }
function Ok($t)   { Write-Host "  [ok]   $t"   -ForegroundColor Green }
function Warn($t) { Write-Host "  [ ! ]  $t"   -ForegroundColor Yellow }
function Info($t) { Write-Host "  [--]   $t"   -ForegroundColor DarkGray }

Line ""
Line "RazeXR PCVR setup"
Line "================="
Line ""

# ---------------------------------------------------------------- 1. where the games might be

$roots = New-Object System.Collections.Generic.List[string]

function AddRoot($p) {
    if ([string]::IsNullOrWhiteSpace($p)) { return }
    if (Test-Path $p) { if (-not $roots.Contains($p)) { $roots.Add($p) } }
}

# Steam, including every library folder it knows about
$steam = $null
foreach ($k in @("HKLM:\SOFTWARE\WOW6432Node\Valve\Steam",
                 "HKLM:\SOFTWARE\Valve\Steam",
                 "HKCU:\Software\Valve\Steam")) {
    try {
        $v = (Get-ItemProperty -Path $k -ErrorAction Stop)
        if ($v.InstallPath) { $steam = $v.InstallPath; break }
        if ($v.SteamPath)   { $steam = $v.SteamPath;   break }
    } catch {}
}
if ($steam) {
    AddRoot (Join-Path $steam "steamapps\common")
    $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
    if (Test-Path $vdf) {
        foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) {
            AddRoot (Join-Path ($m.Groups[1].Value -replace '\\\\', '\') "steamapps\common")
        }
    }
}

# GOG
foreach ($base in @("HKLM:\SOFTWARE\WOW6432Node\GOG.com\Games",
                    "HKLM:\SOFTWARE\GOG.com\Games")) {
    if (Test-Path $base) {
        foreach ($g in Get-ChildItem $base -ErrorAction SilentlyContinue) {
            try { AddRoot (Get-ItemProperty $g.PSPath -ErrorAction Stop).path } catch {}
        }
    }
}

# The usual manual spots
foreach ($d in @("C:\GOG Games", "D:\GOG Games", "E:\GOG Games",
                 "C:\Games", "D:\Games", "E:\Games")) { AddRoot $d }
if ($Root) { AddRoot $Root }

Line "Looking for games"
if ($roots.Count -eq 0) {
    Warn "no Steam or GOG install found - pass -Root <folder> to point at your games"
} else {
    foreach ($r in $roots) { Info $r }
}
Line ""

# ---------------------------------------------------------------- 2. what we are looking for

# A recognised file identifies the game; its whole folder comes across, so
# companions (extra ART, maps, expansion GRPs, music) arrive with it.
$games = @(
    @{ Folder = "duke";          Name = "Duke Nukem 3D";        Files = @("DUKE3D.GRP") },
    @{ Folder = "blood";         Name = "Blood";                Files = @("BLOOD.RFF") },
    @{ Folder = "shadowwarrior"; Name = "Shadow Warrior";       Files = @("SW.GRP") },
    @{ Folder = "rampage";       Name = "Redneck Rampage";      Files = @("REDNECK.GRP") },
    @{ Folder = "ridesagain";    Name = "Redneck Rides Again";  Files = @("REDNECK.GRP", "RIDES.GRP", "RIDESAGAIN.GRP") },
    @{ Folder = "nam";           Name = "NAM";                  Files = @("NAM.GRP") },
    @{ Folder = "ww2gi";         Name = "WWII GI";              Files = @("WW2GI.GRP") },
    @{ Folder = "exhumed";       Name = "Exhumed / PowerSlave"; Files = @("STUFF.DAT") }
)

# Rides Again ships its data as REDNECK.GRP too, so it is told apart by size.
$RIDESAGAIN_SIZE = 191798609

<#
    Exhumed is identified by STUFF.DAT, and Duke Nukem 3D ships a stuff.dat of
    its own - 840 KB of Duke data against Exhumed's 27 MB. Without this, anyone
    who owns Duke on Steam got Duke's gameroot copied into games\exhumed, 1527
    files of the wrong game, and Exhumed would not run. The size in grpinfo.txt
    is exact, so the test only has to separate two files that are nothing alike.
#>
$EXHUMED_MIN = 20000000

<#
    Duke: prefer Atomic over 1.3D.

    Several things ship a Duke 1.3D DUKE3D.GRP as their own base - Penthouse
    Paradise does - and the search takes its first hit. A user whose Atomic copy
    is found second would quietly end up playing 1.3D, losing Duke it out in
    D.C., Life's a Beach and Nuclear Winter, which all depend on the Atomic GRP.
    Atomic and World Tour are both 44,356,548 bytes; 1.3D is 26,524,524. So
    anything below this is kept only as a fallback.
#>
$DUKE_ATOMIC_MIN = 40000000

# Modern remasters that answer to a game's name and contain nothing Raze can
# use. Checked, not guessed - see the note where this is used.
$remasters = @(
    @{ Folder = "exhumed"; Dir = "PowerSlave Exhumed";
       Why    = "the 2022 Nightdive remaster, a rewrite with its own assets";
       Needs  = "the DOS original - Steam sells it as PowerSlave (DOS Classic Edition)" }
)

$skipExt = @(".exe", ".dll", ".msi", ".cab", ".log", ".url", ".ico", ".bat", ".sh")

<#
    Every path here goes through -LiteralPath, and the directories are made with
    .NET rather than New-Item.

    Game folders contain names PowerShell reads as wildcards. A Duke install
    here ships "hudstatfont_[.png", and a bare Copy-Item treats the '[' as the
    start of a character class, fails with "The specified wildcard character
    pattern is not valid", and takes the whole setup down with it - after the
    "Copying game data" banner, so it looks like the copy itself broke. Square
    brackets are legal in filenames and mods use them freely.
#>
function CopyGameFolder($src, $targetName) {
    $target = Join-Path (Join-Path $dest "games") $targetName
    [void][System.IO.Directory]::CreateDirectory($target)
    $n = 0
    foreach ($f in Get-ChildItem -LiteralPath $src -Recurse -File -ErrorAction SilentlyContinue) {
        if ($skipExt -contains $f.Extension.ToLower()) { continue }
        $rel = $f.FullName.Substring($src.Length).TrimStart('\')
        $out = Join-Path $target $rel
        $dir = Split-Path -Parent $out
        if (-not (Test-Path -LiteralPath $dir)) { [void][System.IO.Directory]::CreateDirectory($dir) }
        if (-not (Test-Path -LiteralPath $out)) {
            Copy-Item -LiteralPath $f.FullName -Destination $out -ErrorAction SilentlyContinue
            $n++
        }
    }
    return $n
}

<#
    Everything that is not the engine, the game data or the player's own files
    lives in assets\. Ten archives in the root was ten things to mistake for
    something you were meant to open.
#>
$assets = Join-Path $dest "assets"
[void][System.IO.Directory]::CreateDirectory($assets)

Line "Copying game data"
$found = 0
$seedPath = ""      # a real data file, to hand the engine as -gamegrp later

<#
    World Tour is searched for in its own right, before anything else.

    Doing it as a side effect of the Duke search did not work: that loop stops
    at its first hit, so on a machine where another Duke install is found first
    the World Tour folder is never looked at and episode five silently does not
    appear. It is identified by FIREFLYTROOPER.CON, which no other release has.
#>
$wtDir = ""
foreach ($r in $roots) {
    $c = Get-ChildItem -LiteralPath $r -Filter "FIREFLYTROOPER.CON" -Recurse -File -Depth 4 -ErrorAction SilentlyContinue |
         Select-Object -First 1
    if ($c) { $wtDir = $c.DirectoryName; break }
}

foreach ($g in $games) {
    $hit = $null
    $wtFallback = $null
    $smallDuke = $null
    foreach ($r in $roots) {
        foreach ($fn in $g.Files) {
            $c = Get-ChildItem -Path $r -Filter $fn -Recurse -File -Depth 4 -ErrorAction SilentlyContinue |
                 Select-Object -First 5
            foreach ($cand in $c) {
                if ($g.Folder -eq "ridesagain" -and $cand.Length -ne $RIDESAGAIN_SIZE) { continue }
                if ($g.Folder -eq "rampage"    -and $cand.Length -eq $RIDESAGAIN_SIZE) { continue }
                if ($g.Folder -eq "exhumed"    -and $cand.Length -lt $EXHUMED_MIN) { continue }
                if ($g.Folder -eq "duke" -and $cand.Length -lt $DUKE_ATOMIC_MIN) {
                    if (-not $smallDuke) { $smallDuke = $cand }
                    continue
                }
                <#
                    World Tour ships DUKE3D.GRP like every other Duke release,
                    so it answers the search for Duke - but its folder must not
                    become games\duke. Four of its loose files carry the same
                    names as lumps inside the Atomic GRP (GAME.CON, USER.CON,
                    DEFS.CON, TILES009.ART) and 41 of its 49 maps do too, and a
                    loose file wins over a GRP lump. Copying it wholesale would
                    quietly re-script and re-map Atomic, Duke it out in D.C.,
                    Caribbean and Nuclear Winter as well.

                    So it is remembered and set aside. Its episode five is added
                    afterwards, by name, without the four that collide.
                #>
                if ($g.Folder -eq "duke" -and
                    (Test-Path -LiteralPath (Join-Path $cand.DirectoryName "FIREFLYTROOPER.CON"))) {
                    if (-not $wtFallback) { $wtFallback = $cand }
                    continue
                }
                $hit = $cand; break
            }
            if ($hit) { break }
        }
        if ($hit) { break }
    }

    # Only World Tour was found, so it has to serve as the base game after all,
    # and episode five comes with it rather than being added separately.
    if (-not $hit -and $wtFallback) { $hit = $wtFallback; $wtDir = "" }

    # No Atomic anywhere, so an older Duke is better than none.
    if (-not $hit -and $smallDuke) { $hit = $smallDuke }

    if (-not $hit) {
        Info ("{0,-24} not found" -f $g.Name)
        <#
            "not found" is a poor thing to tell somebody who owns the game.

            Raze runs the original Build engine releases. The modern remasters
            are rewrites with their own repacked assets and ship nothing Raze
            can read - PowerSlave Exhumed (Nightdive, 2022) has no STUFF.DAT in
            it anywhere, which was confirmed on a machine that owns it. So when
            the folder is sitting right there in the search, say why rather than
            leaving the owner to conclude the search is broken.

            Only releases actually checked are listed. A remaster that does ship
            the original data alongside would be found by the normal search and
            never reach here.
        #>
        foreach ($r in $roots) {
            foreach ($rm in $remasters) {
                if ($rm.Folder -ne $g.Folder) { continue }
                $cand = Join-Path $r $rm.Dir
                if (Test-Path -LiteralPath $cand) {
                    Info ("  found {0}, which is {1}" -f $rm.Dir, $rm.Why)
                    Info ("  Raze needs {0}" -f $rm.Needs)
                }
            }
        }
        continue
    }

    $found++
    if ($InPlace) {
        <#
            A junction rather than a copy.

            The engine finds other games by walking out from the one it was
            given, so games scattered across Steam and GOG are invisible to each
            other - point it at Duke under Steam and it will never see Redneck
            under GOG. Linking them all into games\ puts every one under a single
            root, which is what the copy does, without duplicating gigabytes.

            Directory junctions need no administrator rights. They do not survive
            being copied to another PC, which is the trade this switch makes.
        #>
        $target = Join-Path (Join-Path $dest "games") $g.Folder
        if (Test-Path $target) { cmd /c rmdir "$target" 2>$null | Out-Null }
        New-Item -ItemType Directory -Force -Path (Join-Path $dest "games") | Out-Null
        cmd /c mklink /J "$target" "$($hit.DirectoryName)" | Out-Null
        if (Test-Path (Join-Path $target $hit.Name)) {
            if (-not $seedPath) { $seedPath = Join-Path $target $hit.Name }
            Ok ("{0,-24} linked to {1}" -f $g.Name, $hit.DirectoryName)
        } else {
            if (-not $seedPath) { $seedPath = $hit.FullName }
            Warn ("{0,-24} could not link - using it where it is" -f $g.Name)
        }
    } else {
        if (-not $seedPath) { $seedPath = $hit.FullName }
        $n = CopyGameFolder $hit.DirectoryName $g.Folder
        Ok ("{0,-24} {1} files from {2}" -f $g.Name, $n, $hit.DirectoryName)
    }
}

<#
    A game archive that also exists deeper in the same folder is thrown away.

    Steam's Duke ships the Atomic GRP twice, byte for byte: gameroot/duke3d.grp
    and gameroot/classic/DUKE3D.GRP. Both come across, and then which one the
    scan reports decides which folder the engine loads - so when it picked the
    one in classic\ the game ran from a folder holding 43 files instead of the
    one holding World Tour's scripts, and World Tour died on
    "WT_GAME.CON: Missing con file(s)".

    Nothing is gained by keeping a second identical copy, and everything that
    reads the folder beside the game data depends on there being one answer. So
    the deeper duplicate goes, and only on an exact hash match - a same-sized
    file that differs is a different release and stays.
#>
$gamesRoot = Join-Path $dest "games"
if ((Test-Path -LiteralPath $gamesRoot) -and -not $InPlace) {
    $dropped = 0
    foreach ($gdir in Get-ChildItem -LiteralPath $gamesRoot -Directory -ErrorAction SilentlyContinue) {
        $top = @{}
        foreach ($f in Get-ChildItem -LiteralPath $gdir.FullName -File -ErrorAction SilentlyContinue) {
            if ($f.Extension -notmatch '^\.(grp|rff|dat)$') { continue }
            $top[(Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash] = $f.FullName
        }
        if ($top.Count -eq 0) { continue }

        foreach ($f in Get-ChildItem -LiteralPath $gdir.FullName -File -Recurse -ErrorAction SilentlyContinue) {
            if ($f.DirectoryName -eq $gdir.FullName) { continue }      # the keepers
            if ($f.Extension -notmatch '^\.(grp|rff|dat)$') { continue }
            $h = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
            if (-not $top.ContainsKey($h)) { continue }
            Remove-Item -LiteralPath $f.FullName -Force -ErrorAction SilentlyContinue
            $dropped++
        }
    }
    if ($dropped -gt 0) {
        Ok ("{0,-24} {1} duplicate copies removed" -f "Game data", $dropped)
    }
}

<#
    Duke's expansions are scattered across its releases.

    No single Duke release has them all. World Tour is the base game and episode
    five and nothing else; Megaton Edition is the only one with Duke!ZONE II;
    the plain Steam release keeps D.C., Life's a Beach and Nuclear Winter under
    gameroot\addons\. Setup copies one Duke folder as the base game, so
    whichever it picked, the others' expansions were left behind - a machine
    with all three installed still only got what one of them happened to carry.

    So each expansion is fetched by name from wherever it turns up, and only if
    it is not already there. They are small, they sit beside Duke, and Raze
    identifies each by its own CRC.
#>
$dukeDir = Join-Path (Join-Path $dest "games") "duke"
if ((Test-Path -LiteralPath $dukeDir) -and -not $InPlace) {
    $dukeAddons = @("DUKEDC.GRP", "VACATION.GRP", "NWINTER.GRP", "DUKE!ZON.GRP")
    $got = 0
    foreach ($an in $dukeAddons) {
        # Already have it, at the root or in an addons\ subfolder? Leave it.
        $have = Get-ChildItem -LiteralPath $dukeDir -Filter $an -Recurse -File -ErrorAction SilentlyContinue |
                Select-Object -First 1
        if ($have) { continue }

        foreach ($r in $roots) {
            $f = Get-ChildItem -LiteralPath $r -Filter $an -Recurse -File -Depth 4 -ErrorAction SilentlyContinue |
                 Select-Object -First 1
            if (-not $f) { continue }
            Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $dukeDir $f.Name) -Force
            Info ("  + {0} from {1}" -f $f.Name, $f.DirectoryName)
            $got++
            break
        }
    }
    if ($got -gt 0) { Ok ("{0,-24} {1} added from other Duke releases" -f "Duke expansions", $got) }
}

<#
    Duke Nukem's Penthouse Paradise.

    Raze's entries for it expect a repacked .grp, and the original release is a
    folder of loose files, so it is never identified and never gets a launcher.
    Four files are the whole add-on. Its CONs include only each other, none of
    the names collide with anything in the Duke GRP, and it depends on the
    Atomic GRP - so they go in beside Duke and the launcher names the script.

    The DUKE3D.GRP in that folder is a 1.3D copy the add-on shipped with. It is
    not needed and is deliberately not taken.
#>
$ppakDir = ""
foreach ($r in $roots) {
    $c = Get-ChildItem -LiteralPath $r -Filter "ppakgame.con" -Recurse -File -Depth 4 -ErrorAction SilentlyContinue |
         Select-Object -First 1
    if ($c) { $ppakDir = $c.DirectoryName; break }
}

if ($ppakDir -and -not $InPlace) {
    <#
        Packed into an archive of its own rather than dropped beside Duke.

        It ships a TILES014.art, and the Atomic GRP has a TILES014 as well, so
        loose files would have replaced that art for every other Duke game. Only
        the Penthouse launcher names this archive, so nothing else ever sees it.

        Its sounds and music come along for the same reason they belong to it,
        and the base game's own GAME/USER/DEFS.CON are left behind - those three
        names are the collision, not the add-on's own p* and ppak* sets.
    #>
    $ppakZip = Join-Path $assets "penthouse_paradise.zip"
    $stage = Join-Path $env:TEMP ("razexr_ppak_" + [guid]::NewGuid().ToString())
    try {
        [void][System.IO.Directory]::CreateDirectory($stage)
        $skip = @("GAME.CON", "USER.CON", "DEFS.CON")
        $n = 0
        foreach ($f in Get-ChildItem -LiteralPath $ppakDir -File) {
            if ($f.Extension -notmatch '^\.(con|map|art|voc|wav|mid)$') { continue }
            if ($skip -contains $f.Name.ToUpper()) { continue }
            Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $stage $f.Name) -Force
            $n++
        }
        if ($n -ge 4) {
            if (Test-Path -LiteralPath $ppakZip) { Remove-Item -LiteralPath $ppakZip -Force }
            Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $ppakZip
            Ok ("{0,-24} {1} files from {2}" -f "Penthouse Paradise", $n, $ppakDir)
        } else {
            Warn "Penthouse Paradise found but incomplete - skipped"
        }
    } catch {
        Warn "could not pack Penthouse Paradise - everything else still works"
    } finally {
        if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue }
    }
}

<#
    Duke Nukem 3D: World Tour - episode five, Alien World Order.

    Episode five is not a separate game to select. It is definevolumename 4 in
    World Tour's USER.CON, which its GAME.CON includes alongside the three
    scripts that add the incinerator and the firefly trooper. Load those scripts
    and the episode is simply in the list.

    The three that collide are copied under a WT_ prefix with their own include
    lines repointed to match, so nothing shadows what the other Duke games load
    out of the GRP. The launcher for World Tour then names WT_GAME.CON and gets
    all five episodes; every other Duke launcher is untouched.

    Nothing here is redistributed - it is copied from the user's own install.
#>
if ($wtDir -and -not $InPlace) {
    Line "Duke Nukem 3D: World Tour"
    $dukeDir = Join-Path (Join-Path $dest "games") "duke"
    if (-not (Test-Path $dukeDir)) {
        Warn "World Tour found but Duke was not copied - skipping episode five"
    } else {
        $n = 0

        # Names that appear nowhere in the Atomic GRP, so they are safe as they are.
        foreach ($f in @("FIREFLYTROOPER.CON", "FLAMETHROWER.CON", "EPISODE5BOSS.CON",
                         "TILES020.ART", "TILES021.ART", "TILES022.ART")) {
            $srcf = Join-Path $wtDir $f
            if (Test-Path -LiteralPath $srcf) {
                Copy-Item -LiteralPath $srcf -Destination (Join-Path $dukeDir $f) -Force; $n++
            }
        }

        <#
            All of World Tour's maps, keeping the maps\ folder and the names.

            USER.CON names every level with the prefix, not just episode five:
            "definelevelname 0 0 maps/E1L1.map" as much as
            "definelevelname 4 0 maps/E5L1.map". So the engine asks the file
            system for "maps/E1L1.map", and copying only E5* left World Tour
            able to list all five episodes and unable to start any of the first
            four - "Unable to open map maps/E1L1.map" on New Game.

            Copying all of them is safe, and is what World Tour is: they go in a
            maps\ subfolder, so they are looked up only by a CON that asks for
            that path. Every other Duke launcher asks for a bare "E1L1.MAP" and
            still gets the Atomic GRP's own copy, untouched.
        #>
        $mapsrc = Join-Path $wtDir "maps"
        if (Test-Path $mapsrc) {
            $mapdst = Join-Path $dukeDir "maps"
            if (-not (Test-Path $mapdst)) { New-Item -ItemType Directory -Force -Path $mapdst | Out-Null }
            foreach ($m in Get-ChildItem -LiteralPath $mapsrc -Filter "*.map" -File) {
                Copy-Item -LiteralPath $m.FullName -Destination (Join-Path $mapdst $m.Name) -Force; $n++
            }
        }

        # The episode five voice-overs. None of these collide either.
        $sndsrc = Join-Path $wtDir "sound"
        if (Test-Path $sndsrc) {
            $snddst = Join-Path $dukeDir "sound"
            if (-not (Test-Path $snddst)) { New-Item -ItemType Directory -Force -Path $snddst | Out-Null }
            foreach ($f in Get-ChildItem -LiteralPath $sndsrc -File) {
                $o = Join-Path $snddst $f.Name
                if (-not (Test-Path -LiteralPath $o)) {
                    Copy-Item -LiteralPath $f.FullName -Destination $o -Force; $n++
                }
            }
        }

        # The three whose names collide, renamed, with their includes repointed.
        $enc = [System.Text.Encoding]::GetEncoding("iso-8859-1")
        $conok = $true
        foreach ($c in @("GAME.CON", "USER.CON", "DEFS.CON")) {
            $srcf = Join-Path $wtDir $c
            if (-not (Test-Path $srcf)) { $conok = $false; continue }
            $txt = [System.IO.File]::ReadAllText($srcf, $enc)
            $txt = $txt -replace "(?im)^(\s*include\s+)GAME\.CON", '${1}WT_GAME.CON'
            $txt = $txt -replace "(?im)^(\s*include\s+)USER\.CON", '${1}WT_USER.CON'
            $txt = $txt -replace "(?im)^(\s*include\s+)DEFS\.CON", '${1}WT_DEFS.CON'
            [System.IO.File]::WriteAllText((Join-Path $dukeDir ("WT_" + $c)), $txt, $enc)
            $n++
        }

        if ($conok) {
            Ok ("{0,-24} {1} files from {2}" -f "episode five", $n, $wtDir)
        } else {
            Warn "World Tour scripts incomplete - episode five may not appear"
        }
    }
} elseif ($wtDir -and $InPlace) {
    Info "World Tour episode five needs copied data; re-run without -InPlace for it"
}

if ($found -eq 0) {
    Line ""
    Write-Host "No game data was found, so there is nothing to play yet." -ForegroundColor Red
    Write-Host "Put your game folders somewhere and re-run with -Root <that folder>." -ForegroundColor Red
    Line ""
}

# ---------------------------------------------------------------- 3. optional extras

Line ""
Line "Optional extras"

$vox = Join-Path $assets "duke3d_voxels.zip"
$voxCheello = Join-Path $assets "voxel_duke3d.zip"

if (Test-Path $voxCheello) {
    Ok "Voxel Duke 3D already present - monsters and props as voxels"
} elseif (Test-Path $vox) {
    Ok "Duke3D Voxel Pack already present - props and pickups as voxels"
} elseif ($NoDownload) {
    Info "voxel pack skipped (-NoDownload)"
} else {
    $url = "https://github.com/NightFright2k19/duke3d_voxelpack/releases/download/2.0-rc2/duke3d_voxels.zip"
    try {
        Write-Host "  ...  downloading the Duke3D Voxel Pack (4 MB)" -ForegroundColor DarkGray
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $url -OutFile $vox -UseBasicParsing
        $size = (Get-Item $vox).Length
        if ($size -lt 1000000) { Remove-Item $vox -Force; throw "download too small" }
        Ok ("Duke3D Voxel Pack, {0:N0} bytes" -f $size)
    } catch {
        Warn "could not download the voxel pack - this is optional, everything else works"
        Info "get it yourself from: $url"
    }
}

if (-not (Test-Path -LiteralPath $voxCheello)) {
    Info "for voxel MONSTERS as well, get Voxel Duke 3D and drop the zip in this folder:"
    Info "  https://www.moddb.com/mods/voxel-duke-nukem-3d/downloads"
}

<#
    The other games' voxel packs.

    Blood, Shadow Warrior and Exhumed have community packs of their own - props,
    scenery and some monsters - by fgsfds and contributors, under the same
    non-commercial share-alike Voxel Pack Art License as the Duke3D pack. They
    are fetched from the authors' own repositories rather than redistributed by
    us, and each carries its own license.txt inside.

    Taken as the repository archive rather than a release, because only one of
    the three publishes release assets and all three keep the pack in the tree.
    GitHub wraps a source zip in a <repo>-<branch>/ folder and Raze reads the
    defs from the archive root, so the wrapper is stripped and the contents
    repacked here, on the user's machine.

    Each pack ships <game>-raze.def, so dropping the archive in is the whole
    installation - the launcher just names it.
#>
$packs = @(
    @{ Name = "Blood voxel pack";          File = "voxels_blood.zip";
       Url  = "https://github.com/fgsfds/Blood-Voxel-Pack/archive/refs/heads/master.zip" },
    @{ Name = "Shadow Warrior voxel pack"; File = "voxels_sw.zip";
       Url  = "https://github.com/fgsfds/Shadow-Warrior-Voxel-Pack/archive/refs/heads/master.zip" },
    @{ Name = "Exhumed voxel pack";        File = "voxels_exhumed.zip";
       Url  = "https://github.com/fgsfds/Powerslave-Voxel-Pack/archive/refs/heads/master.zip" }
)

foreach ($pk in $packs) {
    $out = Join-Path $assets $pk.File
    if (Test-Path -LiteralPath $out) {
        Ok ("{0} already present" -f $pk.Name)
        continue
    }
    if ($NoDownload) {
        Info ("{0} skipped (-NoDownload)" -f $pk.Name)
        continue
    }

    $tmp = Join-Path $env:TEMP ("razexr_" + [guid]::NewGuid().ToString() + ".zip")
    $ex  = Join-Path $env:TEMP ("razexr_x_" + [guid]::NewGuid().ToString())
    try {
        Write-Host ("  ...  downloading the {0}" -f $pk.Name) -ForegroundColor DarkGray
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $pk.Url -OutFile $tmp -UseBasicParsing

        Expand-Archive -LiteralPath $tmp -DestinationPath $ex -Force
        $inner = Get-ChildItem -LiteralPath $ex -Directory | Select-Object -First 1
        if (-not $inner) { throw "archive had no folder in it" }
        Compress-Archive -Path (Join-Path $inner.FullName '*') -DestinationPath $out -Force

        $size = (Get-Item -LiteralPath $out).Length
        if ($size -lt 100000) { Remove-Item -LiteralPath $out -Force; throw "result too small" }
        Ok ("{0}, {1:N0} bytes" -f $pk.Name, $size)
    } catch {
        Warn ("could not fetch the {0} - optional, everything else works" -f $pk.Name)
        Info ("  " + $pk.Url)
    } finally {
        if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue }
        if (Test-Path -LiteralPath $ex)  { Remove-Item -LiteralPath $ex -Recurse -Force -ErrorAction SilentlyContinue }
    }
}

# The voxel weapons in your hands. Built here rather than shipped: the models
# belong to the games and to Cheello, so they are taken from the copies you
# already have. See THIRD-PARTY-PERMISSIONS.md.
$builder = Join-Path $here "build-vrweapons.ps1"
if (-not (Test-Path $builder)) { $builder = Join-Path $dest "build-vrweapons.ps1" }
if (Test-Path $builder) {
    <#
        The bundled overlay, unless the user pointed at a VRaze install of their
        own.

        vrweapons_models.pk3 holds the 66 weapon models that cannot be derived
        from anything the user already has - Exhumed's, NAM's, Redneck's and
        WWII GI's, plus the animation frames. The other 34 are Blood's and
        Shadow Warrior's own pickup voxels and Duke's from Cheello's pack, and
        those are still taken from the user's own copies rather than shipped:
        no permission from a modder covers redistributing a game's own data.
    #>
    $overlay = $VRaze
    if (-not $overlay) {
        $bundled = Join-Path $assets "vrweapons_models.pk3"
        if (Test-Path -LiteralPath $bundled) { $overlay = $bundled }
    }

    try {
        & $builder -Root $dest -VRaze $overlay
        if (-not $overlay) {
            Info "for the animation frames and the Exhumed / NAM / Redneck / WWII GI"
            Info "weapons, re-run with -VRaze pointing at a VRaze raze.pk3."
        }
    } catch {
        Warn "could not build the voxel weapons pack - everything else still works"
        Info $_.Exception.Message
    }
} else {
    Warn "build-vrweapons.ps1 is missing - no voxel weapons"
}

<#
    Redneck Rampage's soundtrack.

    Redneck is CD audio and has no MIDI to fall back on, so a copy with no
    tracks is simply silent - which is what every Redneck game did until now.
    GOG does ship the music, as MP3s under Extras with the song titles for
    names, so nothing needs downloading; it just has to be where the engine
    looks and named what the engine asks for.

    S_PlayRRMusic asks for redneck02..09 (redneckrides02..09 for Rides Again),
    and OpenMusic always searches a "music" subfolder as well. Track 1 on the
    disc is the data track, so the first song is track 2.

    The launchers pass +set mus_extendedlookup 1, which is what lets the .mp3
    answer a request for .ogg.
#>
$soundtracks = @(
    @{ Dir = Join-Path $dest "games\rampage";       Base = "redneck";      Match = "*Redneck Rampage Soundtrack*"; Zip = "*redneck_rampage_soundtrack*.zip" },
    @{ Dir = Join-Path $dest "games\rampage\AGAIN"; Base = "redneckrides"; Match = "*Rides Again soundtrack*";     Zip = "*rides_again_soundtrack*.zip" }
)

<#
    Where a soundtrack might be.

    GOG does not put this music in the game. The only copy anywhere in a
    Redneck install is under Extras\, and Extras\ is filled by a separate
    "bonus content" download - so an ordinary install has no music at all and
    the game is silent, which is what it did on the machine this was reported
    from.

    Nothing can conjure it, but it can be looked for properly: beside the game
    data, then anywhere setup already searches for games, and then inside the
    zip GOG leaves next to the folder it unpacks - because a download that was
    never unpacked still has the music in it.
#>
Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue

function Get-SongsFromZip($path, $st, $depth) {
    <#
        What is in this archive, without unpacking all of it.

        GOG's bonus content is a zip of zips with no installer, so the music
        can be nested one level down and may never have been extracted at all.
        Reading the entry list first means a large or unrelated archive costs a
        directory read rather than a full extraction.
    #>
    try {
        $zip = [System.IO.Compression.ZipFile]::OpenRead($path)
    } catch { return @() }

    $audio = @()
    $inner = @()
    try {
        foreach ($e in $zip.Entries) {
            if ($e.Name -match '\.(mp3|ogg|flac)$') { $audio += $e.FullName }
            elseif ($e.Name -match '\.zip$' -and $e.Name -like $st.Zip) { $inner += $e.FullName }
        }
    } finally { $zip.Dispose() }

    if ($audio.Count -gt 0) {
        $tmp = Join-Path $env:TEMP ("razexr_mus_" + [guid]::NewGuid().ToString())
        try {
            Expand-Archive -LiteralPath $path -DestinationPath $tmp -Force -ErrorAction Stop
            return @(Get-ChildItem -LiteralPath $tmp -File -Recurse -ErrorAction SilentlyContinue |
                     Where-Object { $_.Extension -match '^\.(mp3|ogg|flac)$' } | Sort-Object Name)
        } catch { return @() }
    }

    # A zip of zips. One level only - the download is not deeper than that,
    # and following archives without a bound is how a setup script hangs.
    if ($inner.Count -gt 0 -and $depth -lt 1) {
        $tmp = Join-Path $env:TEMP ("razexr_musz_" + [guid]::NewGuid().ToString())
        try {
            Expand-Archive -LiteralPath $path -DestinationPath $tmp -Force -ErrorAction Stop
        } catch { return @() }
        foreach ($z in (Get-ChildItem -LiteralPath $tmp -File -Recurse -ErrorAction SilentlyContinue |
                        Where-Object { $_.Name -like $st.Zip })) {
            $songs = @(Get-SongsFromZip $z.FullName $st ($depth + 1))
            if ($songs.Count -gt 0) { return $songs }
        }
    }

    return @()
}

function Find-Soundtrack($st) {
    <#
        Where the music might be, in the order it is most likely to be.

        GOG does not put this music in the game - the only copy in an install
        is under Extras\, which a separate "bonus content" download fills, and
        that download is a zip of zips with no installer that people extract
        wherever they happen to be standing. So look beside the game data, then
        anywhere setup already searches, then the places a download lands, and
        finally inside the archives themselves.
    #>
    $places = @((Join-Path $dest "games\rampage"))
    $places += $roots
    foreach ($d in @("$env:USERPROFILE\Downloads", "$env:USERPROFILE\Desktop",
                     "$env:USERPROFILE\Documents")) {
        if (Test-Path -LiteralPath $d) { $places += $d }
    }

    # Unpacked, somewhere.
    foreach ($r in $places) {
        if (-not (Test-Path -LiteralPath $r)) { continue }
        foreach ($d in (Get-ChildItem -Path $r -Directory -Recurse -Depth 4 -ErrorAction SilentlyContinue |
                        Where-Object { $_.Name -like $st.Match })) {
            $songs = @(Get-ChildItem -LiteralPath $d.FullName -File -ErrorAction SilentlyContinue |
                       Where-Object { $_.Extension -match '^\.(mp3|ogg|flac)$' } | Sort-Object Name)
            if ($songs.Count -gt 0) { return $songs }
        }
    }

    # Still in an archive: the soundtrack zip itself, or the bundle holding it.
    foreach ($r in $places) {
        if (-not (Test-Path -LiteralPath $r)) { continue }
        foreach ($z in (Get-ChildItem -Path $r -File -Recurse -Depth 4 -ErrorAction SilentlyContinue |
                        Where-Object { $_.Name -like "*redneck*.zip" })) {
            $songs = @(Get-SongsFromZip $z.FullName $st 0)
            if ($songs.Count -gt 0) {
                Info ("using the soundtrack from " + $z.Name)
                return $songs
            }
        }
    }

    return @()
}

foreach ($st in $soundtracks) {
    if (-not (Test-Path -LiteralPath $st.Dir)) { continue }

    $musicDir = Join-Path $st.Dir "music"
    # Already done by a previous run.
    if (Test-Path -LiteralPath (Join-Path $musicDir ($st.Base + "02.mp3"))) { continue }

    $songs = @(Find-Soundtrack $st)
    if ($songs.Count -eq 0) {
        # Say so. Redneck is CD audio with no MIDI to fall back on, so this is
        # the difference between a game with music and a silent one.
        Warn ("no soundtrack found for {0} - the game will be silent" -f $st.Base)
        Info "on GOG it is a separate 'bonus content' download, not part of the game"
        Info "installer. Install it, or drop its folder anywhere setup searches,"
        Info "then re-run SETUP."
        continue
    }

    if (-not (Test-Path -LiteralPath $musicDir)) {
        New-Item -ItemType Directory -Path $musicDir -Force | Out-Null
    }

    $track = 2
    $n = 0
    foreach ($song in $songs) {
        if ($track -gt 9) { break }
        $target = Join-Path $musicDir ("{0}{1:D2}{2}" -f $st.Base, $track, $song.Extension)
        Copy-Item -LiteralPath $song.FullName -Destination $target -Force -ErrorAction SilentlyContinue
        $track++
        $n++
    }
    if ($n -gt 0) { Ok ("{0,-24} {1} CD tracks" -f "Redneck music", $n) }
}

<#
    CD music that shipped with a different copy of the same game.

    PowerSlave is why this exists. The Steam DOS Classic Edition and the GOG DOS
    release ship a byte-identical STUFF.DAT - 27,020,745 bytes either way - so
    the search above can legitimately pick either one. Only Steam's has the free
    soundtrack DLC's MUSIC folder beside it, and PowerSlave has no music
    anywhere else: its data holds 648 entries and every one of them is a sound
    effect. So whichever copy the walk happened to reach first decided, in
    silence, whether the game had any music at all.

    A game that came across without music therefore gets one more look: any
    other copy of the same data file, with a music folder beside it, and that
    folder alone is brought over. The game data itself is never touched, and a
    game that already has its music is skipped.
#>
foreach ($g in $games) {
    $gdir = Join-Path (Join-Path $dest "games") $g.Folder
    if (-not (Test-Path -LiteralPath $gdir)) { continue }

    $already = @(Get-ChildItem -LiteralPath $gdir -Directory -ErrorAction SilentlyContinue |
                 Where-Object { $_.Name -ieq "music" })
    if ($already.Count -gt 0) { continue }

    $srcMusic = $null
    foreach ($r in $roots) {
        foreach ($fn in $g.Files) {
            $c = Get-ChildItem -Path $r -Filter $fn -Recurse -File -Depth 4 -ErrorAction SilentlyContinue |
                 Select-Object -First 5
            foreach ($cand in $c) {
                # The same guards the search above uses. Without them Duke's own
                # 840KB stuff.dat answers the search for Exhumed - it has a MUSIC
                # folder beside it, so Duke's soundtrack lands in games\exhumed.
                if ($g.Folder -eq "ridesagain" -and $cand.Length -ne $RIDESAGAIN_SIZE) { continue }
                if ($g.Folder -eq "rampage"    -and $cand.Length -eq $RIDESAGAIN_SIZE) { continue }
                if ($g.Folder -eq "exhumed"    -and $cand.Length -lt $EXHUMED_MIN)     { continue }
                if ($g.Folder -eq "duke"       -and $cand.Length -lt $DUKE_ATOMIC_MIN) { continue }

                $m = Join-Path $cand.DirectoryName "MUSIC"
                if (Test-Path -LiteralPath $m) { $srcMusic = $m; break }
            }
            if ($srcMusic) { break }
        }
        if ($srcMusic) { break }
    }
    if (-not $srcMusic) { continue }

    $dstMusic = Join-Path $gdir "music"
    [void][System.IO.Directory]::CreateDirectory($dstMusic)
    $n = 0
    foreach ($f in Get-ChildItem -LiteralPath $srcMusic -File -ErrorAction SilentlyContinue) {
        if ($skipExt -contains $f.Extension.ToLower()) { continue }
        $o = Join-Path $dstMusic $f.Name
        if (-not (Test-Path -LiteralPath $o)) {
            Copy-Item -LiteralPath $f.FullName -Destination $o -ErrorAction SilentlyContinue
            $n++
        }
    }
    if ($n -gt 0) { Ok ("{0,-24} {1} CD tracks" -f ($g.Name + " music"), $n) }
}

# ---------------------------------------------------------------- 4. launchers

Line ""
Line "Writing launchers"

# Whatever route we took, prefer a file under games\ - that is the root the
# engine will walk to find everything else.
$anyGrp = Get-ChildItem -Path (Join-Path $dest "games") -Include *.grp,*.rff,*.dat -Recurse -File -ErrorAction SilentlyContinue |
          Select-Object -First 1
if ($anyGrp) { $seedPath = $anyGrp.FullName }

if (-not $seedPath) {
    # An empty -gamegrp contributes no search path, finds no game, and takes the
    # engine down before its first frame. Never send one.
    Warn "no game data found - skipping launcher generation"
} else {
    $cfg = Join-Path $dest "cfg_setup.ini"
    $log = Join-Path $dest "setup.log"
    if (Test-Path $log) { Remove-Item $log -Force -ErrorAction SilentlyContinue }

    # NOT $args - that is a PowerShell automatic variable, and assigning it here
    # leaves Start-Process with the script's own arguments instead of these.
    #
    # And the list has to be quoted by hand. PowerShell 5.1 joins -ArgumentList
    # with spaces and quotes nothing, so every path containing a space arrives
    # split in two. This folder is called "RazeXR Setup Test (Miles's)" for
    # exactly that reason - a path with no spaces proves nothing here.
    # -portable, for the same reason the launchers carry it: without it this
    # run scans every Steam and GOG install and can write launchers naming data
    # outside this folder, which is exactly what the portable copy exists to
    # avoid. It also takes minutes off the scan.
    # -novr: this run only writes the launcher files. Without it the engine
    # brings up a VR session, and with no headset connected it waits minutes for
    # one to become active - setup looks frozen and the desktop stutters behind
    # it. Playing is unaffected; the launchers do not pass it.
    # +quit, or this never ends on its own. Without it the engine writes the
    # launchers and then sits in the game until the timeout below kills it -
    # two minutes of looking frozen, and a race the launchers can lose: killed
    # before the write, setup reports that nothing was written, which is exactly
    # what happened once the game list got long enough.
    $razeArgs = ('-nosetup -portable -novr -gamegrp "{0}" -config "{1}" +logfile "{2}" +vrwritelaunchers +quit' `
                 -f $seedPath, $cfg, $log)

    $p = Start-Process -FilePath $exe -ArgumentList $razeArgs `
                       -WorkingDirectory $dest -PassThru -WindowStyle Minimized
    # It exits on its own in about a second now. The wait is only a backstop.
    if (-not $p.WaitForExit(60000)) { $p.Kill() }
    Start-Sleep -Seconds 1

    $wrote = $null
    if (Test-Path $log) { $wrote = Select-String -Path $log -Pattern "Wrote (\d+) launchers" }

    if ($wrote) {
        Ok $wrote.Matches[0].Value
        # Only the block the launcher step itself printed, which sits directly
        # above its summary line - the log above that is engine startup noise.
        $lines = Get-Content $log
        $end = $wrote.LineNumber - 2
        $start = $end
        while ($start -ge 0 -and $lines[$start] -match "^  \S") { $start-- }
        for ($i = $start + 1; $i -le $end; $i++) { Info $lines[$i].Trim() }
    } else {
        # Silence here used to mean the whole step had done nothing and said so
        # in no way at all. Never again.
        Write-Host ""
        Write-Host "  LAUNCHERS WERE NOT WRITTEN." -ForegroundColor Red
        if (Test-Path $log) {
            Write-Host "  The engine ran but did not report - last lines of setup.log:" -ForegroundColor Red
            foreach ($l in (Get-Content $log -Tail 5)) { Info $l }
        } else {
            Write-Host "  The engine did not start, or wrote no log." -ForegroundColor Red
            Info ("tried: {0} {1}" -f $exe, $razeArgs)
        }
        Write-Host ""
    }
    Remove-Item $cfg -ErrorAction SilentlyContinue
}

# ---------------------------------------------------------------- done

Line ""
Line "Done."
Line ""
Line "Start Virtual Desktop and connect your headset FIRST, then run"
Line "  Play RazeXR PCVR.bat"
Line ""
Line "It opens the game you played last, and Switch Game in the menu moves"
Line "between all of them without leaving the headset. To start one directly,"
Line "the individual scripts are in the launchers folder."
Line ""
Line "Settings go in config, logs go in logs, game data in games. Everything"
Line "lives in this folder - copy it to another PC and it runs."
Line ""
