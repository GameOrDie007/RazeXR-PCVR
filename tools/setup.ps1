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
#>

param(
    [switch]$InPlace,
    [switch]$NoDownload,
    [string]$Root
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

$skipExt = @(".exe", ".dll", ".msi", ".cab", ".log", ".url", ".ico", ".bat", ".sh")

function CopyGameFolder($src, $targetName) {
    $target = Join-Path (Join-Path $dest "games") $targetName
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    $n = 0
    foreach ($f in Get-ChildItem -Path $src -Recurse -File -ErrorAction SilentlyContinue) {
        if ($skipExt -contains $f.Extension.ToLower()) { continue }
        $rel = $f.FullName.Substring($src.Length).TrimStart('\')
        $out = Join-Path $target $rel
        $dir = Split-Path -Parent $out
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
        if (-not (Test-Path $out)) { Copy-Item $f.FullName $out -ErrorAction SilentlyContinue; $n++ }
    }
    return $n
}

Line "Copying game data"
$found = 0
$seedPath = ""      # a real data file, to hand the engine as -gamegrp later
foreach ($g in $games) {
    $hit = $null
    foreach ($r in $roots) {
        foreach ($fn in $g.Files) {
            $c = Get-ChildItem -Path $r -Filter $fn -Recurse -File -Depth 4 -ErrorAction SilentlyContinue |
                 Select-Object -First 5
            foreach ($cand in $c) {
                if ($g.Folder -eq "ridesagain" -and $cand.Length -ne $RIDESAGAIN_SIZE) { continue }
                if ($g.Folder -eq "rampage"    -and $cand.Length -eq $RIDESAGAIN_SIZE) { continue }
                $hit = $cand; break
            }
            if ($hit) { break }
        }
        if ($hit) { break }
    }

    if (-not $hit) { Info ("{0,-24} not found" -f $g.Name); continue }

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

if (-not (Test-Path $voxCheello)) {
    Info "for voxel MONSTERS as well, get Voxel Duke 3D and drop the zip in this folder:"
    Info "  https://www.moddb.com/mods/voxel-duke-nukem-3d/downloads"
}

$vrw = Join-Path $dest "vrweapons.pk3"
if (Test-Path $vrw) {
    Ok "voxel weapons pack present"
} else {
    Info "voxel weapons need a VRaze copy, whose downloads have been withdrawn."
    Info "If you have one, run tools\build-vrweapons-pk3.py against its raze.pk3."
    Info "Without it the games use their ordinary flat weapon sprites."
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
    $razeArgs = ('-nosetup -gamegrp "{0}" -config "{1}" +logfile "{2}" +vrwritelaunchers' `
                 -f $seedPath, $cfg, $log)

    $p = Start-Process -FilePath $exe -ArgumentList $razeArgs `
                       -WorkingDirectory $dest -PassThru -WindowStyle Minimized
    if (-not $p.WaitForExit(120000)) { $p.Kill() }
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
