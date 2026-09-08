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
    $dukeDir = Join-Path (Join-Path $dest "games") "duke"
    if (Test-Path -LiteralPath $dukeDir) {
        $n = 0
        foreach ($f in @("ppakgame.con", "ppakdefs.con", "ppakuser.con", "ppakpent.map")) {
            $srcf = Join-Path $ppakDir $f
            if (Test-Path -LiteralPath $srcf) {
                Copy-Item -LiteralPath $srcf -Destination (Join-Path $dukeDir $f) -Force
                $n++
            }
        }
        if ($n -ge 4) {
            Ok ("{0,-24} {1} files from {2}" -f "Penthouse Paradise", $n, $ppakDir)
        } else {
            Warn "Penthouse Paradise found but incomplete - skipped"
        }
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
            Episode five's maps, keeping the maps\ folder and the exact names.

            USER.CON names them with the prefix - "definelevelname 4 0
            maps/E5L1.map" - so the engine asks the file system for
            "maps/E5L1.map" and nothing else will do. Copied flat, they are
            found by no lookup at all, the episode appears in the menu and
            every level in it fails to start.

            Only E5* is copied. The other 41 maps in that folder carry the same
            names as lumps in the Atomic GRP.
        #>
        $mapsrc = Join-Path $wtDir "maps"
        if (Test-Path $mapsrc) {
            $mapdst = Join-Path $dukeDir "maps"
            if (-not (Test-Path $mapdst)) { New-Item -ItemType Directory -Force -Path $mapdst | Out-Null }
            foreach ($m in Get-ChildItem -LiteralPath $mapsrc -Filter "E5L*.map" -File) {
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

$vox = Join-Path $dest "duke3d_voxels.zip"
$voxCheello = Join-Path $dest "voxel_duke3d.zip"

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
    $out = Join-Path $dest $pk.File
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
    try {
        & $builder -Root $dest -VRaze $VRaze
        if (-not $VRaze) {
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
Line "Start Virtual Desktop and connect your headset FIRST, then run the .bat for"
Line "the game you want. Everything lives in this folder - copy it anywhere."
Line ""
