<#
    RazeXR PCVR - install, update, uninstall and log collection, in one script.

    Finds the Build games you already own (Steam, GOG, the 3D Realms Anthology,
    the usual folders, or a folder you name), copies their data into the
    RazeXR PCVR folder, fetches the optional extras, and writes a launcher for
    every game it ends up with.

    Where it installs:
      - run from a folder that is already set up (an older RazeXR PCVR folder
        you extracted the new zip over), it updates that folder where it is
      - given an older RazeXR PCVR folder (dropped on SETUP.bat, or -GamePath),
        it updates that one where it is
      - otherwise it copies the port to Documents\RazeXR PCVR
        (%USERPROFILE%\Games\RazeXR PCVR when OneDrive keeps your Documents),
        or to the folder it installed to before

    Nothing here is downloaded from us and no game data ships with this port.
    Everything comes from your own installs or from the projects that publish it.

      SETUP.bat               install, or update - this script with no switch
      Uninstall.bat           -Uninstall: removes what it installed; your saves,
                              settings and Game Tome notes are kept
      Collect logs.bat        -Collect: zips the logs onto your desktop

    For people:
      -InPlace     point at the games where they are instead of copying them
      -NoDownload  skip the network step entirely
      -Root <path> one more folder to search for game data
      -VRaze <pk3> a VRaze raze.pk3, if you have one (adds the weapon animation
                   frames; the voxel weapons work without it)

    For tools and hubs (the same switches in every Game Or Die port):
      -Detect                 change nothing; report the games and the install
      -Quiet                  no questions, no pause
      -Json                   print the result as JSON on stdout, nothing else
      -GamePath "<a>;<b>"     only these folders; no searching. A RazeXR PCVR
                              folder among them is the install to update
      -AddToSteam             with -Quiet: add the Steam library entry
      -CloseSteam             with -Quiet: close Steam to write it (reopened after)
      -NoSteam, -NoShortcuts  no Steam entry / no desktop shortcut
      Tests: -NoPrompt, -NoSearch, -SteamRoot, -DocumentsRoot, -UserRoot, -DesktopRoot
    Exit codes: 0 done, 1 nothing could be installed (no games found), 2 error.
#>

[CmdletBinding(PositionalBinding = $false)]
param(
    [switch] $Uninstall,
    [switch] $Collect,
    [switch] $Detect,
    [switch] $Quiet,
    [switch] $Json,
    [string[]] $GamePath,
    [switch] $AddToSteam,
    [switch] $CloseSteam,
    [switch] $NoSteam,
    [switch] $NoShortcuts,
    [switch] $NoDownload,
    [switch] $InPlace,
    [string] $Root,
    [string] $VRaze,
    # Tests: no questions; only the folders given; stand-ins for Steam,
    # Documents, the user folder and the desktop.
    [switch] $NoPrompt,
    [switch] $NoSearch,
    [string] $SteamRoot,
    [string] $DocumentsRoot,
    [string] $UserRoot,
    [string] $DesktopRoot,
    # Folders dropped on SETUP.bat arrive as bare arguments.
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $Dropped
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$Product = 'RazeXR PCVR'
$ProductFile = 'RazeXR PCVR'
$Version = '1.3'

if ($Quiet -or $Json) { $NoPrompt = $true }
if ($GamePath) { $NoSearch = $true; $Dropped = @($Dropped) + @($GamePath | ForEach-Object { $_ -split ';' } | Where-Object { $_ }) }
$Dropped = @($Dropped | Where-Object { $_ } | ForEach-Object { $_.Trim('"').TrimEnd('\') })

# ProviderPath, not Path: on a network share .Path carries a provider prefix
# that .NET file calls refuse.
$Here = (Resolve-Path -LiteralPath $PSScriptRoot).ProviderPath

# ---------------------------------------------------------------- where things are

function Is-RazeFolder([string] $d) {
    if (-not $d) { return $false }
    return (Test-Path -LiteralPath (Join-Path $d 'raze.exe')) -and (Test-Path -LiteralPath (Join-Path $d 'raze.pk3'))
}
# A RazeXR PCVR folder somebody has played from: set up by an earlier Setup.
function Is-SetUp([string] $d) {
    if (-not (Is-RazeFolder $d)) { return $false }
    foreach ($m in @('version.txt', 'games', 'launchers', 'config')) {
        if (Test-Path -LiteralPath (Join-Path $d $m)) { return $true }
    }
    return $false
}

# The port's files: beside this script in the download, or this script is the
# copy in an installed folder's tools\.
$Package = $null
if (Is-RazeFolder $Here) { $Package = $Here }
elseif ((Split-Path -Leaf $Here) -ieq 'tools' -and (Is-RazeFolder (Split-Path -Parent $Here))) { $Package = Split-Path -Parent $Here }

function Documents { if ($DocumentsRoot) { return $DocumentsRoot } return [Environment]::GetFolderPath('MyDocuments') }
function Desktop   { if ($DesktopRoot)   { return $DesktopRoot }   return [Environment]::GetFolderPath('Desktop') }
function UserHome  { if ($UserRoot)      { return $UserRoot }      return $env:USERPROFILE }
function LocalData {
    if ($UserRoot) { return (Join-Path $UserRoot 'AppData\Local') }
    return [Environment]::GetFolderPath('LocalApplicationData')
}
$Remembered = Join-Path (LocalData) ('Game Or Die\' + $ProductFile + '\home.txt')

function Default-Home {
    $docs = Documents
    # A Documents folder OneDrive keeps would upload gigabytes of game data.
    if ($docs -like '*OneDrive*') { return (Join-Path (UserHome) ('Games\' + $ProductFile)) }
    return (Join-Path $docs $ProductFile)
}

# Which RazeXR PCVR folder this run is about, in order: one the player named,
# the one this script runs from (if it is set up), the one installed to
# before, the default one. $ForInstall: the default even when it is not there.
function Find-Home([bool] $ForInstall) {
    foreach ($d in $Dropped) { if (Is-RazeFolder $d) { return $d } }
    if ($Package -and (Is-SetUp $Package)) { return $Package }
    if (Test-Path -LiteralPath $Remembered) {
        $r = ([string](Get-Content -LiteralPath $Remembered -TotalCount 1)).Trim()
        if (Is-RazeFolder $r) { return $r }
    }
    $d = Default-Home
    if ($ForInstall -or (Is-RazeFolder $d)) { return $d }
    return $null
}

# ---------------------------------------------------------------- the log, and -Json

$Mode = 'install'
if ($Uninstall) { $Mode = 'uninstall' }
if ($Collect)   { $Mode = 'collect' }
if ($Detect)    { $Mode = 'detect' }
$LogFile = Join-Path ([System.IO.Path]::GetTempPath()) ('razexr-pcvr-' + $Mode + '.txt')
$Result = [ordered]@{ product = $Product; version = $Version; mode = $Mode; result = 'ok' }

function Say([string] $m, [string] $color) {
    if ($Json) { return }	# stdout carries the JSON alone
    if ($color) { Write-Host $m -ForegroundColor $color } else { Write-Host $m }
}
function Log([string] $m, [string] $color) {
    Add-Content -LiteralPath $LogFile -Value $m -Encoding UTF8
    Say $m $color
}
function Line($t) { Log $t }
function Ok($t)   { Log "  [ok]   $t" 'Green' }
function Warn($t) { Log "  [ ! ]  $t" 'Yellow' }
function Info($t) { Log "  [--]   $t" 'DarkGray' }
Set-Content -LiteralPath $LogFile -Value ($Product + ' ' + $Version + ' ' + $Mode + ', ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')) -Encoding UTF8

function Finish([int] $code) {
    if ($Json) {
        $Result['exitCode'] = $code
        $Result['games'] = @(Game-Status)
        Write-Output ($Result | ConvertTo-Json -Depth 5)
    }
    exit $code
}
trap {
    Log ''
    Log ('ERROR: ' + $_.Exception.Message)
    Log ('  at ' + $_.InvocationInfo.PositionMessage)
    Log ''
    Log 'Setup stopped.  Run "Collect logs.bat" and send us the file it makes.'
    $Result['result'] = 'error'
    $Result['error'] = $_.Exception.Message
    if ($Json) { $Result['exitCode'] = 2; Write-Output ($Result | ConvertTo-Json -Depth 5) }
    exit 2
}

function Ask-YesNo([string] $q) {
    if ($NoPrompt) { return $false }
    $a = Read-Host ($q + ' [Y/n]')
    return ($a -eq '' -or $a -match '^[Yy]')
}

# ---------------------------------------------------------------- the games

# A recognised file identifies the game; its whole folder comes across, so
# companions (extra ART, maps, expansion GRPs, music) arrive with it.
$games = @(
    @{ Folder = 'duke';          Name = 'Duke Nukem 3D';        Files = @('DUKE3D.GRP');
       Launchers = @('Duke Nukem 3D - World Tour VR', 'Duke Nukem 3D - Atomic Edition VR', 'Duke Nukem 3D*VR') },
    @{ Folder = 'blood';         Name = 'Blood';                Files = @('BLOOD.RFF');
       Launchers = @('BLOOD - One Unit Whole Blood VR', 'BLOOD*VR') },
    @{ Folder = 'shadowwarrior'; Name = 'Shadow Warrior';       Files = @('SW.GRP');
       Launchers = @('Shadow Warrior VR', 'Shadow Warrior*VR') },
    @{ Folder = 'rampage';       Name = 'Redneck Rampage';      Files = @('REDNECK.GRP');
       Launchers = @('Redneck Rampage VR') },
    @{ Folder = 'ridesagain';    Name = 'Redneck Rides Again';  Files = @('REDNECK.GRP', 'RIDES.GRP', 'RIDESAGAIN.GRP');
       Launchers = @('Redneck Rampage Rides Again VR') },
    @{ Folder = 'nam';           Name = 'NAM';                  Files = @('NAM.GRP');
       Launchers = @('NAM VR', 'NAPALM VR') },
    @{ Folder = 'ww2gi';         Name = 'WWII GI';              Files = @('WW2GI.GRP');
       Launchers = @('WWII GI VR') },
    @{ Folder = 'exhumed';       Name = 'Exhumed / PowerSlave'; Files = @('STUFF.DAT');
       Launchers = @('Powerslave VR', 'Exhumed VR', 'PowerSlave*VR') }
)

# Rides Again ships its data as REDNECK.GRP too, so it is told apart by size.
$RIDESAGAIN_SIZE = 191798609

<#
    Exhumed is identified by STUFF.DAT, and Duke Nukem 3D ships a stuff.dat of
    its own - 840 KB of Duke data against Exhumed's 27 MB. Without this, anyone
    who owns Duke on Steam got Duke's gameroot copied into games\exhumed, 1527
    files of the wrong game, and Exhumed would not run.
#>
$EXHUMED_MIN = 20000000

<#
    Duke: prefer Atomic over 1.3D. Several things ship a Duke 1.3D DUKE3D.GRP
    as their own base (Penthouse Paradise does), and an Atomic copy found second
    would quietly lose D.C., Life's a Beach and Nuclear Winter. Atomic and World
    Tour are both 44,356,548 bytes; 1.3D is 26,524,524.
#>
$DUKE_ATOMIC_MIN = 40000000

# Modern remasters that answer to a game's name and contain nothing Raze can use.
$remasters = @(
    @{ Folder = 'exhumed'; Dir = 'PowerSlave Exhumed';
       Why    = 'the 2022 Nightdive remaster, a rewrite with its own assets';
       Needs  = 'the DOS original - Steam sells it as PowerSlave (DOS Classic Edition)' }
)

$skipExt = @('.exe', '.dll', '.msi', '.cab', '.log', '.url', '.ico', '.bat', '.sh')

# ---------------------------------------------------------------- where the games might be

$roots = New-Object System.Collections.Generic.List[string]
function AddRoot($p) {
    if ([string]::IsNullOrWhiteSpace($p)) { return }
    if (Test-Path -LiteralPath $p) { if (-not $roots.Contains($p)) { $roots.Add($p) } }
}

function Steam-Dir {
    if ($SteamRoot) { return $SteamRoot }
    foreach ($k in @('HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam', 'HKCU:\Software\Valve\Steam')) {
        try {
            $v = Get-ItemProperty -Path $k -ErrorAction Stop
            if ($v.InstallPath) { return $v.InstallPath }
            if ($v.SteamPath)   { return $v.SteamPath }
        } catch {}
    }
    return $null
}

$searched = $false
function Search-Roots {
    if ($script:searched) { return }
    $script:searched = $true
    # The folders named: game folders to search (a RazeXR PCVR folder among
    # them is the install, not a place to look for games).
    foreach ($d in $Dropped) { if (-not (Is-RazeFolder $d)) { AddRoot $d } }
    if ($Root) { AddRoot $Root }
    if ($NoSearch) { return }

    <#
        [System.IO.Path]::Combine, not Join-Path, for every path read from Steam.
        libraryfolders.vdf lists every library Steam has ever had - including
        external drives that are switched off - and Join-Path throws on a drive
        that is not there ("Cannot find drive", a player, 29 Sep 2026).
    #>
    $steam = Steam-Dir
    if ($steam) {
        AddRoot ([System.IO.Path]::Combine($steam, 'steamapps\common'))
        $vdf = [System.IO.Path]::Combine($steam, 'steamapps\libraryfolders.vdf')
        if (Test-Path -LiteralPath $vdf) {
            foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"')) {
                AddRoot ([System.IO.Path]::Combine(($m.Groups[1].Value -replace '\\\\', '\'), 'steamapps\common'))
            }
        }
    }
    if ($SteamRoot) { return }	# a test: its stand-in Steam and nothing of this PC's

    foreach ($base in @('HKLM:\SOFTWARE\WOW6432Node\GOG.com\Games', 'HKLM:\SOFTWARE\GOG.com\Games')) {
        if (Test-Path $base) {
            foreach ($g in Get-ChildItem $base -ErrorAction SilentlyContinue) {
                try { AddRoot (Get-ItemProperty $g.PSPath -ErrorAction Stop).path } catch {}
            }
        }
    }
    # The 3D Realms Anthology bought from 3D Realms installs outside Steam.
    foreach ($k in @('HKLM:\SOFTWARE\WOW6432Node\3DRealms\Anthology', 'HKLM:\SOFTWARE\3DRealms\Anthology', 'HKCU:\Software\3DRealms\Anthology')) {
        try { AddRoot (Get-ItemProperty -Path $k -ErrorAction Stop).'(default)' } catch {}
    }
    foreach ($d in @('C:\Program Files (x86)\3D Realms', 'C:\Program Files\3D Realms', 'C:\3D Realms', 'C:\Games\3D Realms Anthology',
                     'C:\GOG Games', 'D:\GOG Games', 'E:\GOG Games', 'C:\Games', 'D:\Games', 'E:\Games')) { AddRoot $d }
}

function BloodHasCampaigns($dir) {
    foreach ($c in @('Death Wish', 'MARROW')) {
        if (Test-Path -LiteralPath (Join-Path (Join-Path $dir 'addons') $c)) { return $true }
    }
    return $false
}

<#
    Every game's data, found once: folder -> the file that identifies it.

    World Tour is looked for in its own right first: as a side effect of the
    Duke search it was never reached on a machine where another Duke is found
    first. It is identified by FIREFLYTROOPER.CON, which no other release has.
#>
$hits = $null
$wtDir = ''
function Find-AllGames {
    if ($script:hits) { return $script:hits }
    Search-Roots
    $found = @{}
    foreach ($r in $roots) {
        $c = Get-ChildItem -LiteralPath $r -Filter 'FIREFLYTROOPER.CON' -Recurse -File -Depth 4 -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($c) { $script:wtDir = $c.DirectoryName; break }
    }
    foreach ($g in $games) {
        $hit = $null; $wtFallback = $null; $smallDuke = $null; $plainBlood = $null
        foreach ($r in $roots) {
            foreach ($fn in $g.Files) {
                $cands = Get-ChildItem -LiteralPath $r -Filter $fn -Recurse -File -Depth 4 -ErrorAction SilentlyContinue | Select-Object -First 5
                foreach ($cand in $cands) {
                    if ($g.Folder -eq 'ridesagain' -and $cand.Length -ne $RIDESAGAIN_SIZE) { continue }
                    if ($g.Folder -eq 'rampage'    -and $cand.Length -eq $RIDESAGAIN_SIZE) { continue }
                    if ($g.Folder -eq 'exhumed'    -and $cand.Length -lt $EXHUMED_MIN) { continue }
                    # Blood: prefer the install that carries Death Wish and MARROW
                    # (Blood: Refreshed Supply); a plain one only as a fallback.
                    if ($g.Folder -eq 'blood' -and -not (BloodHasCampaigns $cand.DirectoryName)) {
                        if (-not $plainBlood) { $plainBlood = $cand }
                        continue
                    }
                    if ($g.Folder -eq 'duke' -and $cand.Length -lt $DUKE_ATOMIC_MIN) {
                        if (-not $smallDuke) { $smallDuke = $cand }
                        continue
                    }
                    <#
                        World Tour's folder must not become games\duke: four of its
                        loose files share names with lumps in the Atomic GRP, and a
                        loose file wins, so it would re-script every other Duke game.
                        Its episode five is added afterwards, by name.
                    #>
                    if ($g.Folder -eq 'duke' -and (Test-Path -LiteralPath (Join-Path $cand.DirectoryName 'FIREFLYTROOPER.CON'))) {
                        if (-not $wtFallback) { $wtFallback = $cand }
                        continue
                    }
                    $hit = $cand; break
                }
                if ($hit) { break }
            }
            if ($hit) { break }
        }
        # Only World Tour: it serves as the base game, episode five with it.
        if (-not $hit -and $wtFallback) { $hit = $wtFallback; $script:wtDir = '' }
        if (-not $hit -and $smallDuke)  { $hit = $smallDuke }
        if (-not $hit -and $plainBlood) { $hit = $plainBlood }
        if ($hit) { $found[$g.Folder] = $hit }
    }
    $script:hits = $found
    return $found
}

# Does a RazeXR PCVR folder hold this game's data?
function Home-HasGame([string] $vrHome, $g) {
    if (-not $vrHome) { return $false }
    $gd = Join-Path (Join-Path $vrHome 'games') $g.Folder
    if (-not (Test-Path -LiteralPath $gd)) { return $false }
    foreach ($fn in $g.Files) {
        if (Get-ChildItem -LiteralPath $gd -Filter $fn -File -Recurse -Depth 2 -ErrorAction SilentlyContinue | Select-Object -First 1) { return $true }
    }
    return $false
}

function Installed-Version([string] $vrHome) {
    if (-not (Is-RazeFolder $vrHome)) { return $null }
    $v = Join-Path $vrHome 'version.txt'
    if (Test-Path -LiteralPath $v) { return ([string](Get-Content -LiteralPath $v -TotalCount 1)).Trim() }
    # Set up by hand before 1.2.1, with no version.txt: 1.2 brought the Game Tome.
    if (-not (Is-SetUp $vrHome)) { return $null }
    if (Test-Path -LiteralPath (Join-Path $vrHome 'gametome')) { return ($Product + ' 1.2') }
    return ($Product + ' 1.1')
}

function Launcher-For([string] $vrHome, $g) {
    $dir = Join-Path $vrHome 'launchers'
    if (-not (Test-Path -LiteralPath $dir)) { return $null }
    foreach ($name in $g.Launchers) {
        $f = Get-ChildItem -LiteralPath $dir -Filter ($name + '.bat') -File -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -First 1
        if ($f) { return $f.FullName }
    }
    return $null
}

# ---------------------------------------------------------------- the Steam library
#
# Steam keeps "Add a Non-Steam Game" entries in a binary file,
# userdata\<account>\config\shortcuts.vdf, one per account. It reads it when it
# starts and writes it when it exits, so it must not be running while this is
# changed. Every other entry is kept byte for byte.

function Vdf-ReadString([byte[]] $b, [ref] $i) {
    $start = $i.Value
    while ($b[$i.Value] -ne 0) { $i.Value++ }
    $s = [System.Text.Encoding]::UTF8.GetString($b, $start, $i.Value - $start)
    $i.Value++
    return $s
}
function Vdf-ReadMap([byte[]] $b, [ref] $i) {
    $entries = New-Object System.Collections.ArrayList
    while ($true) {
        if ($i.Value -ge $b.Length) { throw 'shortcuts.vdf ends early' }
        $t = $b[$i.Value]; $i.Value++
        if ($t -eq 8) { return ,$entries }
        $k = Vdf-ReadString $b $i
        switch ($t) {
            0 { $v = Vdf-ReadMap $b $i }
            1 { $v = Vdf-ReadString $b $i }
            2 { $v = [BitConverter]::ToInt32($b, $i.Value); $i.Value += 4 }
            7 { $v = [BitConverter]::ToUInt64($b, $i.Value); $i.Value += 8 }
            default { throw ('shortcuts.vdf has a value type this does not know (' + $t + '); left alone') }
        }
        [void]$entries.Add(@{ T = [int]$t; K = $k; V = $v })
    }
}
function Vdf-Write($entries, [System.IO.MemoryStream] $ms) {
    foreach ($e in $entries) {
        $ms.WriteByte([byte]$e.T)
        $kb = [System.Text.Encoding]::UTF8.GetBytes($e.K); $ms.Write($kb, 0, $kb.Length); $ms.WriteByte(0)
        switch ($e.T) {
            0 { Vdf-Write $e.V $ms; $ms.WriteByte(8) }
            1 { $vb = [System.Text.Encoding]::UTF8.GetBytes([string]$e.V); $ms.Write($vb, 0, $vb.Length); $ms.WriteByte(0) }
            2 { $vb = [BitConverter]::GetBytes([int32]$e.V); $ms.Write($vb, 0, 4) }
            7 { $vb = [BitConverter]::GetBytes([uint64]$e.V); $ms.Write($vb, 0, 8) }
        }
    }
}
function Vdf-Get($entries, [string] $k) {
    foreach ($e in $entries) { if ($e.K -ieq $k) { return ,$e.V } }
    return $null
}

# Steam's id for a non-Steam game: CRC-32 of the quoted exe and the name, top
# bit set. Unsigned constants written out: PowerShell 5.1 reads 0xFFFFFFFF,
# 0xEDB88320 and 0x80000000 as negative int32s.
$Crc = $null
$Poly = [uint32]3988292384
$TopBit = [uint32]2147483648
function Crc32([string] $s) {
    if (-not $script:Crc) {
        $script:Crc = New-Object 'uint32[]' 256
        for ($n = 0; $n -lt 256; $n++) {
            $c = [uint32]$n
            for ($k = 0; $k -lt 8; $k++) { if ($c -band 1) { $c = [uint32]($Poly -bxor ($c -shr 1)) } else { $c = [uint32]($c -shr 1) } }
            $script:Crc[$n] = $c
        }
    }
    $c = [uint32]::MaxValue
    foreach ($byte in [System.Text.Encoding]::UTF8.GetBytes($s)) { $c = [uint32]($script:Crc[($c -bxor $byte) -band 0xFF] -bxor ($c -shr 8)) }
    return [uint32]($c -bxor [uint32]::MaxValue)
}
function Shortcut-Id([string] $exeQuoted, [string] $name) { return [uint32]((Crc32 ($exeQuoted + $name)) -bor $TopBit) }

function Steam-Configs {
    $s = Steam-Dir
    if (-not $s) { return @() }
    $u = Join-Path $s 'userdata'
    if (-not (Test-Path -LiteralPath $u)) { return @() }
    return @(Get-ChildItem -LiteralPath $u -Directory | Where-Object { $_.Name -match '^\d+$' -and $_.Name -ne '0' } |
        ForEach-Object { Join-Path $_.FullName 'config' } | Where-Object { Test-Path -LiteralPath $_ })
}
function Steam-Running {
    if ($SteamRoot) { return $false }	# a stand-in folder in a test
    return [bool](Get-Process -Name 'steam' -ErrorAction SilentlyContinue)
}
$SteamWasClosed = $false
function Steam-Close {
    $exe = Join-Path (Steam-Dir) 'steam.exe'
    Log '  Closing Steam...'
    Start-Process -FilePath $exe -ArgumentList '-shutdown'
    for ($n = 0; $n -lt 60 -and (Steam-Running); $n++) { Start-Sleep -Milliseconds 500 }
    if (Steam-Running) { Log '  Steam did not close.'; return $false }
    $script:SteamWasClosed = $true
    return $true
}
function Steam-Reopen {
    if (-not $SteamWasClosed) { return }
    Start-Process -FilePath (Join-Path (Steam-Dir) 'steam.exe')
    Log '  Steam started again.'
}

$PlayBat = 'Play RazeXR PCVR.bat'
# Ours: named as ours AND starting the Play script of THIS install. Another
# install's entry, or anything else, is left alone.
function Is-Ours($entry, [string] $vrHome) {
    if ((Vdf-Get $entry.V 'AppName') -ne $Product) { return $false }
    return ([string](Vdf-Get $entry.V 'Exe')) -like ('*' + (Join-Path $vrHome $PlayBat) + '*')
}
function Read-Shortcuts([string] $file) {
    if (-not (Test-Path -LiteralPath $file)) {
        $root = New-Object System.Collections.ArrayList
        [void]$root.Add(@{ T = 0; K = 'shortcuts'; V = (New-Object System.Collections.ArrayList) })
        return ,$root
    }
    $b = [System.IO.File]::ReadAllBytes($file)
    $i = 0
    $root = Vdf-ReadMap $b ([ref]$i)
    if (-not ($root | Where-Object { $_.K -ieq 'shortcuts' -and $_.T -eq 0 })) { throw ('no shortcuts list in ' + $file + '; left alone') }
    return ,$root
}
function Write-Shortcuts([string] $file, $root) {
    $ms = New-Object System.IO.MemoryStream
    Vdf-Write $root $ms
    $ms.WriteByte(8)
    $backup = $file + '.before-razexr-pcvr'
    if ((Test-Path -LiteralPath $file) -and -not (Test-Path -LiteralPath $backup)) { Copy-Item -LiteralPath $file -Destination $backup }
    [System.IO.File]::WriteAllBytes($file, $ms.ToArray())
}
function Renumber($list) { for ($n = 0; $n -lt $list.Count; $n++) { $list[$n].K = [string]$n } }

function New-ShortcutEntry([string] $vrHome) {
    $exe = '"' + (Join-Path $vrHome $PlayBat) + '"'
    $id = Shortcut-Id $exe $Product
    $v = New-Object System.Collections.ArrayList
    foreach ($f in @(
        @{ T = 2; K = 'appid'; V = [BitConverter]::ToInt32([BitConverter]::GetBytes($id), 0) },
        @{ T = 1; K = 'AppName'; V = $Product },
        @{ T = 1; K = 'Exe'; V = $exe },
        @{ T = 1; K = 'StartDir'; V = ('"' + $vrHome + '\"') },
        @{ T = 1; K = 'icon'; V = (Join-Path $vrHome 'raze.exe') },
        @{ T = 1; K = 'ShortcutPath'; V = '' },
        @{ T = 1; K = 'LaunchOptions'; V = '' },
        @{ T = 2; K = 'IsHidden'; V = 0 },
        @{ T = 2; K = 'AllowDesktopConfig'; V = 1 },
        @{ T = 2; K = 'AllowOverlay'; V = 1 },
        @{ T = 2; K = 'OpenVR'; V = 1 },	# a VR game: in SteamVR's library too
        @{ T = 2; K = 'Devkit'; V = 0 },
        @{ T = 1; K = 'DevkitGameID'; V = '' },
        @{ T = 2; K = 'DevkitOverrideAppID'; V = 0 },
        @{ T = 2; K = 'LastPlayTime'; V = 0 },
        @{ T = 1; K = 'FlatpakAppID'; V = '' },
        @{ T = 0; K = 'tags'; V = (New-Object System.Collections.ArrayList) })) { [void]$v.Add($f) }
    return @{ T = 0; K = '0'; V = $v }
}

function Steam-Add([string] $vrHome) {
    $configs = Steam-Configs
    if ($configs.Count -eq 0) { return 'no Steam' }
    foreach ($c in $configs) {
        $file = Join-Path $c 'shortcuts.vdf'
        $root = Read-Shortcuts $file
        $list = Vdf-Get $root 'shortcuts'
        foreach ($e in @($list)) { if (Is-Ours $e $vrHome) { [void]$list.Remove($e) } }
        [void]$list.Add((New-ShortcutEntry $vrHome))
        Renumber $list
        Write-Shortcuts $file $root
    }
    return 'added'
}
function Steam-HasOurs([string] $vrHome) {
    if (-not $vrHome) { return $false }
    foreach ($c in Steam-Configs) {
        $file = Join-Path $c 'shortcuts.vdf'
        if (-not (Test-Path -LiteralPath $file)) { continue }
        try { foreach ($e in (Vdf-Get (Read-Shortcuts $file) 'shortcuts')) { if (Is-Ours $e $vrHome) { return $true } } } catch { }
    }
    return $false
}
function Steam-Remove([string] $vrHome) {
    foreach ($c in Steam-Configs) {
        $file = Join-Path $c 'shortcuts.vdf'
        if (-not (Test-Path -LiteralPath $file)) { continue }
        $root = Read-Shortcuts $file
        $list = Vdf-Get $root 'shortcuts'
        $removed = 0
        foreach ($e in @($list)) { if (Is-Ours $e $vrHome) { [void]$list.Remove($e); $removed++ } }
        if ($removed) { Renumber $list; Write-Shortcuts $file $root }
    }
}
function Steam-Ready([bool] $quietAllowed) {
    if (-not (Steam-Running)) { return $true }
    if ($NoPrompt) {
        if ($quietAllowed -and $CloseSteam) { return (Steam-Close) }
        return $false
    }
    if (Ask-YesNo 'Steam is open, and it has to be closed for this.  Close Steam now (it opens again after)?') { return (Steam-Close) }
    return $false
}

function Make-Shortcut([string] $lnk, [string] $target, [string] $workdir, [string] $icon, [string] $what) {
    $sh = New-Object -ComObject WScript.Shell
    $s = $sh.CreateShortcut($lnk)
    $s.TargetPath = $target
    $s.WorkingDirectory = $workdir
    $s.IconLocation = $icon + ',0'
    $s.Description = $what
    $s.WindowStyle = 7	# minimised: the script's window stays out of the way
    $s.Save()
}
$DesktopLink = $ProductFile + '.lnk'
function Shortcut-Target([string] $lnk) {
    try { return (New-Object -ComObject WScript.Shell).CreateShortcut($lnk).TargetPath } catch { return $null }
}

# ---------------------------------------------------------------- what an install owns

# What the release zip carries (payload.txt, made by make-release.py): copied
# into the install on every run, so an update replaces them all.
function Payload-List {
    $list = Join-Path $Package 'payload.txt'
    if (Test-Path -LiteralPath $list) {
        return @(Get-Content -LiteralPath $list | Where-Object { $_ -and -not $_.StartsWith('#') })
    }
    # A folder with no list (a build tree, or a 1.1/1.2 folder): the port's
    # own files by name, never the player's.
    $mine = @('raze.exe', 'raze.pk3', 'raze_portable.ini', 'libsndfile-1.dll', 'openal32.dll', 'openxr_loader.dll', 'zmusiclite.dll',
              'SETUP.bat', 'setup.ps1', 'build-vrweapons.ps1', 'README.md', 'README.txt', 'THIRD-PARTY-PERMISSIONS.md',
              'AUTHORS.md', 'gpl-2.0.txt', 'buildlic.txt', 'Controls - RazeXR PCVR - Meta Quest 3.png',
              'assets\voxel_duke3d.zip', 'assets\vrweapons_models.pk3', 'assets\boxart.pk3', 'soundfonts\raze.sf2')
    $out = @($mine | Where-Object { Test-Path -LiteralPath (Join-Path $Package $_) })
    $gt = Join-Path $Package 'gametome'
    if (Test-Path -LiteralPath $gt) {
        foreach ($f in Get-ChildItem -LiteralPath $gt -Recurse -File) {
            if ($f.Name -in @('ink.txt', 'place.txt', 'typed.txt') -or $f.Name -like '*_log.txt') { continue }
            $out += $f.FullName.Substring($Package.Length).TrimStart('\')
        }
    }
    return $out
}

# Made by Setup in assets\ (downloaded or built here); the rest of assets\ is
# the zip's.
$MadeAssets = @('duke3d_voxels.zip', 'voxels_blood.zip', 'voxels_sw.zip', 'voxels_exhumed.zip',
                'vrweapons.pk3', 'vrweapons_rr.pk3', 'penthouse_paradise.zip')

<#
    The game data an install owns, for Uninstall: in games\<game>, the files
    that came from the player's own copy of that game (the same path in the
    folder it was copied from) and the ones Setup made from it (World Tour's
    episode five, the CD music). Anything the player added there by hand - a
    Blood fan campaign zip, an add-on GRP off an old disc - is not on it, and
    stays. Worked out from the folder as it is, so an update and a fresh
    install list the same files.
#>
function Games-Owned([string] $vrHome, $sources) {
    $out = New-Object System.Collections.Generic.List[string]
    $gamesDir = Join-Path $vrHome 'games'
    if (-not (Test-Path -LiteralPath $gamesDir)) { return $out }
    $dukeExtra = @('FIREFLYTROOPER.CON', 'FLAMETHROWER.CON', 'EPISODE5BOSS.CON', 'TILES020.ART', 'TILES021.ART', 'TILES022.ART',
                   'WT_GAME.CON', 'WT_USER.CON', 'WT_DEFS.CON', 'DUKEDC.GRP', 'VACATION.GRP', 'NWINTER.GRP', 'DUKE!ZON.GRP')
    foreach ($g in $games) {
        $gd = Join-Path $gamesDir $g.Folder
        if (-not (Test-Path -LiteralPath $gd)) { continue }
        $item = Get-Item -LiteralPath $gd -Force
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) { $out.Add('junction:games\' + $g.Folder); continue }
        $src = $sources[$g.Folder]
        foreach ($f in Get-ChildItem -LiteralPath $gd -Recurse -File -Force -ErrorAction SilentlyContinue) {
            $rel = $f.FullName.Substring($gd.Length).TrimStart('\')
            $own = $false
            if ($src -and (Test-Path -LiteralPath (Join-Path $src $rel))) { $own = $true }
            elseif ($rel -match '^music\\') { $own = $true }
            elseif ($g.Folder -eq 'duke' -and ($dukeExtra -contains $rel.ToUpper() -or $rel -match '^(maps|sound)\\')) { $own = $true }
            if ($own) { $out.Add('games\' + $g.Folder + '\' + $rel) }
        }
    }
    return $out
}

# The game folders an earlier run copied from, kept in its manifest, so an
# update with a game no longer installed still knows what it owns.
function Manifest-Sources([string] $vrHome) {
    $s = @{}
    $m = Join-Path $vrHome 'install-manifest.txt'
    if (Test-Path -LiteralPath $m) {
        foreach ($line in Get-Content -LiteralPath $m) {
            if ($line -match '^# source (\S+): (.+)$') { $s[$Matches[1]] = $Matches[2] }
        }
    }
    return $s
}

# ---------------------------------------------------------------- install

function Copy-Payload([string] $vrHome) {
    $n = 0
    foreach ($rel in Payload-List) {
        $src = Join-Path $Package $rel
        if (-not (Test-Path -LiteralPath $src)) { continue }
        $dst = Join-Path $vrHome $rel
        $dir = Split-Path -Parent $dst
        if (-not (Test-Path -LiteralPath $dir)) { [void][System.IO.Directory]::CreateDirectory($dir) }
        Copy-Item -LiteralPath $src -Destination $dst -Force
        $n++
    }
    return $n
}

<#
    Every path here goes through -LiteralPath, and the directories are made with
    .NET rather than New-Item: game folders hold names like "hudstatfont_[.png",
    and a bare Copy-Item reads the '[' as a wildcard and fails.
#>
function CopyGameFolder($dest, $src, $targetName) {
    $target = Join-Path (Join-Path $dest 'games') $targetName
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

function Do-Install {
    Line ''
    Line 'RazeXR PCVR setup'
    Line '================='
    Line ''
    if (-not $Package) { throw 'Run SETUP.bat from the RazeXR PCVR folder you unzipped (it holds raze.exe).' }

    Search-Roots
    Line 'Looking for games'
    if ($roots.Count -eq 0) { Warn 'no Steam or GOG install found - pass -Root <folder> to point at your games' }
    else { foreach ($r in $roots) { Info $r } }
    Line ''
    $found = Find-AllGames

    $dest = Find-Home $true
    $update = Is-RazeFolder $dest
    $hadGames = @($games | Where-Object { Home-HasGame $dest $_ }).Count
    if ($found.Count -eq 0 -and $hadGames -eq 0) {
        Line 'No game data was found, so there is nothing to play yet.'
        Line 'Install one of the games (Steam, GOG or a disc), or point Setup at its folder, and run it again.'
        $Result['result'] = 'no game found'
        Finish 1
    }

    if ($Package -ne $dest) {
        [void][System.IO.Directory]::CreateDirectory($dest)
        $n = Copy-Payload $dest
        if ($update) { Ok ('{0,-24} {1} files updated in {2}' -f 'RazeXR PCVR', $n, $dest) }
        else         { Ok ('{0,-24} {1} files installed to {2}' -f 'RazeXR PCVR', $n, $dest) }
    } else {
        Ok ('{0,-24} set up where it is, {1}' -f 'RazeXR PCVR', $dest)
    }
    $Result['vrFolder'] = $dest
    # Installed again after an uninstall: the note beside the kept saves is no longer true.
    $note = Join-Path $dest 'Your saves are kept here.txt'
    if (Test-Path -LiteralPath $note) { Remove-Item -LiteralPath $note -Force }

    $exe = Join-Path $dest 'raze.exe'
    $assets = Join-Path $dest 'assets'
    [void][System.IO.Directory]::CreateDirectory($assets)
    $sources = Manifest-Sources $dest

    Line 'Copying game data'
    $seedPath = ''
    foreach ($g in $games) {
        $hit = $found[$g.Folder]
        if (-not $hit) {
            if (Home-HasGame $dest $g) { Info ('{0,-24} already here' -f $g.Name); continue }
            Info ('{0,-24} not found' -f $g.Name)
            # "not found" is a poor thing to tell somebody who owns the remaster.
            foreach ($r in $roots) {
                foreach ($rm in $remasters) {
                    if ($rm.Folder -ne $g.Folder) { continue }
                    if (Test-Path -LiteralPath (Join-Path $r $rm.Dir)) {
                        Info ('  found {0}, which is {1}' -f $rm.Dir, $rm.Why)
                        Info ('  Raze needs {0}' -f $rm.Needs)
                    }
                }
            }
            continue
        }
        $sources[$g.Folder] = $hit.DirectoryName
        if ($InPlace) {
            <#
                A junction rather than a copy: every game under one root, which
                the engine needs to see the others, without duplicating
                gigabytes. Junctions need no administrator rights; they do not
                survive being copied to another PC, which is the trade.
            #>
            $target = Join-Path (Join-Path $dest 'games') $g.Folder
            if (Test-Path -LiteralPath $target) { cmd /c rmdir "$target" 2>$null | Out-Null }
            [void][System.IO.Directory]::CreateDirectory((Join-Path $dest 'games'))
            cmd /c mklink /J "$target" "$($hit.DirectoryName)" | Out-Null
            if (Test-Path -LiteralPath (Join-Path $target $hit.Name)) {
                if (-not $seedPath) { $seedPath = Join-Path $target $hit.Name }
                Ok ('{0,-24} linked to {1}' -f $g.Name, $hit.DirectoryName)
            } else {
                if (-not $seedPath) { $seedPath = $hit.FullName }
                Warn ('{0,-24} could not link - using it where it is' -f $g.Name)
            }
        } else {
            if (-not $seedPath) { $seedPath = $hit.FullName }
            $n = CopyGameFolder $dest $hit.DirectoryName $g.Folder
            Ok ('{0,-24} {1} files from {2}' -f $g.Name, $n, $hit.DirectoryName)
            if ($g.Folder -eq 'blood' -and (BloodHasCampaigns $hit.DirectoryName)) {
                $bloodAddons = Join-Path $hit.DirectoryName 'addons'
                $hasDW = Test-Path -LiteralPath (Join-Path $bloodAddons 'Death Wish')
                $hasMRW = Test-Path -LiteralPath (Join-Path $bloodAddons 'MARROW')
                if ($hasDW -and $hasMRW) { Info '  with Death Wish and MARROW, from Blood: Refreshed Supply' }
                elseif ($hasMRW) {
                    Info '  with MARROW, from Blood: Refreshed Supply'
                    Warn '  no Death Wish: update Blood: Refreshed Supply (patch 3.0 added it, free) and run SETUP.bat again'
                } else { Info '  with Death Wish, from Blood: Refreshed Supply' }
            }
        }
    }

    <#
        A game archive that also exists deeper in the same folder is thrown away,
        on an exact hash match only. Steam's Duke ships the Atomic GRP twice, and
        which one the scan reported decided whether World Tour found its scripts.
    #>
    $gamesRoot = Join-Path $dest 'games'
    if ((Test-Path -LiteralPath $gamesRoot) -and -not $InPlace) {
        $dupes = 0
        foreach ($gdir in Get-ChildItem -LiteralPath $gamesRoot -Directory -ErrorAction SilentlyContinue) {
            $top = @{}
            foreach ($f in Get-ChildItem -LiteralPath $gdir.FullName -File -ErrorAction SilentlyContinue) {
                if ($f.Extension -notmatch '^\.(grp|rff|dat)$') { continue }
                $top[(Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash] = $f.FullName
            }
            if ($top.Count -eq 0) { continue }
            foreach ($f in Get-ChildItem -LiteralPath $gdir.FullName -File -Recurse -ErrorAction SilentlyContinue) {
                if ($f.DirectoryName -eq $gdir.FullName) { continue }
                if ($f.Extension -notmatch '^\.(grp|rff|dat)$') { continue }
                $h = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
                if (-not $top.ContainsKey($h)) { continue }
                Remove-Item -LiteralPath $f.FullName -Force -ErrorAction SilentlyContinue
                $dupes++
            }
        }
        if ($dupes -gt 0) { Ok ('{0,-24} {1} duplicate copies removed' -f 'Game data', $dupes) }
    }

    <#
        Duke's expansions are scattered across its releases (World Tour, Megaton,
        the plain Steam release's addons\), so each is fetched by name from
        wherever it turns up, and only if it is not already there.
    #>
    $dukeDir = Join-Path (Join-Path $dest 'games') 'duke'
    if ((Test-Path -LiteralPath $dukeDir) -and -not $InPlace) {
        $got = 0
        foreach ($an in @('DUKEDC.GRP', 'VACATION.GRP', 'NWINTER.GRP', 'DUKE!ZON.GRP')) {
            $have = Get-ChildItem -LiteralPath $dukeDir -Filter $an -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($have) { continue }
            foreach ($r in $roots) {
                $f = Get-ChildItem -LiteralPath $r -Filter $an -Recurse -File -Depth 4 -ErrorAction SilentlyContinue | Select-Object -First 1
                if (-not $f) { continue }
                Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $dukeDir $f.Name) -Force
                Info ('  + {0} from {1}' -f $f.Name, $f.DirectoryName)
                $got++
                break
            }
        }
        if ($got -gt 0) { Ok ('{0,-24} {1} added from other Duke releases' -f 'Duke expansions', $got) }
    }

    <#
        Duke Nukem's Penthouse Paradise: a folder of loose files Raze never
        identifies on its own. Packed into an archive of its own, because its
        TILES014.art would replace the Atomic GRP's for every other Duke game;
        only the Penthouse launcher names the archive.
    #>
    $ppakDir = ''
    foreach ($r in $roots) {
        $c = Get-ChildItem -LiteralPath $r -Filter 'ppakgame.con' -Recurse -File -Depth 4 -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($c) { $ppakDir = $c.DirectoryName; break }
    }
    if ($ppakDir -and -not $InPlace) {
        $ppakZip = Join-Path $assets 'penthouse_paradise.zip'
        $stage = Join-Path $env:TEMP ('razexr_ppak_' + [guid]::NewGuid().ToString())
        try {
            [void][System.IO.Directory]::CreateDirectory($stage)
            $skip = @('GAME.CON', 'USER.CON', 'DEFS.CON')
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
                Ok ('{0,-24} {1} files from {2}' -f 'Penthouse Paradise', $n, $ppakDir)
            } else { Warn 'Penthouse Paradise found but incomplete - skipped' }
        } catch { Warn 'could not pack Penthouse Paradise - everything else still works' }
        finally { if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue } }
    }

    <#
        Duke Nukem 3D: World Tour - episode five, Alien World Order. It is
        definevolumename 4 in World Tour's USER.CON. The three scripts whose
        names collide with the Atomic GRP's are copied under a WT_ prefix with
        their includes repointed, and only the World Tour launcher names
        WT_GAME.CON. All of World Tour's maps go in maps\, which only a CON
        asking for that path ever reads.
    #>
    if ($wtDir -and -not $InPlace) {
        Line 'Duke Nukem 3D: World Tour'
        if (-not (Test-Path -LiteralPath $dukeDir)) {
            Warn 'World Tour found but Duke was not copied - skipping episode five'
        } else {
            $n = 0
            foreach ($f in @('FIREFLYTROOPER.CON', 'FLAMETHROWER.CON', 'EPISODE5BOSS.CON', 'TILES020.ART', 'TILES021.ART', 'TILES022.ART')) {
                $srcf = Join-Path $wtDir $f
                if (Test-Path -LiteralPath $srcf) { Copy-Item -LiteralPath $srcf -Destination (Join-Path $dukeDir $f) -Force; $n++ }
            }
            $mapsrc = Join-Path $wtDir 'maps'
            if (Test-Path -LiteralPath $mapsrc) {
                $mapdst = Join-Path $dukeDir 'maps'
                [void][System.IO.Directory]::CreateDirectory($mapdst)
                foreach ($m in Get-ChildItem -LiteralPath $mapsrc -Filter '*.map' -File) {
                    Copy-Item -LiteralPath $m.FullName -Destination (Join-Path $mapdst $m.Name) -Force; $n++
                }
            }
            $sndsrc = Join-Path $wtDir 'sound'
            if (Test-Path -LiteralPath $sndsrc) {
                $snddst = Join-Path $dukeDir 'sound'
                [void][System.IO.Directory]::CreateDirectory($snddst)
                foreach ($f in Get-ChildItem -LiteralPath $sndsrc -File) {
                    $o = Join-Path $snddst $f.Name
                    if (-not (Test-Path -LiteralPath $o)) { Copy-Item -LiteralPath $f.FullName -Destination $o -Force; $n++ }
                }
            }
            $enc = [System.Text.Encoding]::GetEncoding('iso-8859-1')
            $conok = $true
            foreach ($c in @('GAME.CON', 'USER.CON', 'DEFS.CON')) {
                $srcf = Join-Path $wtDir $c
                if (-not (Test-Path -LiteralPath $srcf)) { $conok = $false; continue }
                $txt = [System.IO.File]::ReadAllText($srcf, $enc)
                $txt = $txt -replace '(?im)^(\s*include\s+)GAME\.CON', '${1}WT_GAME.CON'
                $txt = $txt -replace '(?im)^(\s*include\s+)USER\.CON', '${1}WT_USER.CON'
                $txt = $txt -replace '(?im)^(\s*include\s+)DEFS\.CON', '${1}WT_DEFS.CON'
                [System.IO.File]::WriteAllText((Join-Path $dukeDir ('WT_' + $c)), $txt, $enc)
                $n++
            }
            if ($conok) { Ok ('{0,-24} {1} files from {2}' -f 'episode five', $n, $wtDir) }
            else { Warn 'World Tour scripts incomplete - episode five may not appear' }
        }
    } elseif ($wtDir -and $InPlace) {
        Info 'World Tour episode five needs copied data; re-run without -InPlace for it'
    }

    # ------------------------------------------------------------ optional extras
    Line ''
    Line 'Optional extras'
    $vox = Join-Path $assets 'duke3d_voxels.zip'
    $voxCheello = Join-Path $assets 'voxel_duke3d.zip'
    if (Test-Path -LiteralPath $voxCheello) { Ok 'Voxel Duke 3D already present - monsters and props as voxels' }
    elseif (Test-Path -LiteralPath $vox) { Ok 'Duke3D Voxel Pack already present - props and pickups as voxels' }
    elseif ($NoDownload) { Info 'voxel pack skipped (-NoDownload)' }
    else {
        $url = 'https://github.com/NightFright2k19/duke3d_voxelpack/releases/download/2.0-rc2/duke3d_voxels.zip'
        try {
            Say '  ...  downloading the Duke3D Voxel Pack (4 MB)' 'DarkGray'
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $url -OutFile $vox -UseBasicParsing
            $size = (Get-Item -LiteralPath $vox).Length
            if ($size -lt 1000000) { Remove-Item -LiteralPath $vox -Force; throw 'download too small' }
            Ok ('Duke3D Voxel Pack, {0:N0} bytes' -f $size)
        } catch {
            Warn 'could not download the voxel pack - this is optional, everything else works'
            Info "get it yourself from: $url"
        }
    }

    <#
        The other games' voxel packs, by fgsfds and contributors, under the same
        non-commercial share-alike licence as the Duke3D pack: fetched from the
        authors' own repositories rather than redistributed, each with its own
        license.txt inside. GitHub wraps a source zip in a <repo>-<branch>
        folder, and Raze reads the defs from the archive root, so the wrapper
        is stripped and the contents repacked here.
    #>
    $packs = @(
        @{ Name = 'Blood voxel pack';          File = 'voxels_blood.zip';   Url = 'https://github.com/fgsfds/Blood-Voxel-Pack/archive/refs/heads/master.zip' },
        @{ Name = 'Shadow Warrior voxel pack'; File = 'voxels_sw.zip';      Url = 'https://github.com/fgsfds/Shadow-Warrior-Voxel-Pack/archive/refs/heads/master.zip' },
        @{ Name = 'Exhumed voxel pack';        File = 'voxels_exhumed.zip'; Url = 'https://github.com/fgsfds/Powerslave-Voxel-Pack/archive/refs/heads/master.zip' }
    )
    foreach ($pk in $packs) {
        $out = Join-Path $assets $pk.File
        if (Test-Path -LiteralPath $out) { Ok ('{0} already present' -f $pk.Name); continue }
        if ($NoDownload) { Info ('{0} skipped (-NoDownload)' -f $pk.Name); continue }
        $tmp = Join-Path $env:TEMP ('razexr_' + [guid]::NewGuid().ToString() + '.zip')
        $ex  = Join-Path $env:TEMP ('razexr_x_' + [guid]::NewGuid().ToString())
        try {
            Say ('  ...  downloading the {0}' -f $pk.Name) 'DarkGray'
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $pk.Url -OutFile $tmp -UseBasicParsing
            Expand-Archive -LiteralPath $tmp -DestinationPath $ex -Force
            $inner = Get-ChildItem -LiteralPath $ex -Directory | Select-Object -First 1
            if (-not $inner) { throw 'archive had no folder in it' }
            Compress-Archive -Path (Join-Path $inner.FullName '*') -DestinationPath $out -Force
            $size = (Get-Item -LiteralPath $out).Length
            if ($size -lt 100000) { Remove-Item -LiteralPath $out -Force; throw 'result too small' }
            Ok ('{0}, {1:N0} bytes' -f $pk.Name, $size)
        } catch {
            Warn ('could not fetch the {0} - optional, everything else works' -f $pk.Name)
            Info ('  ' + $pk.Url)
        } finally {
            if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue }
            if (Test-Path -LiteralPath $ex)  { Remove-Item -LiteralPath $ex -Recurse -Force -ErrorAction SilentlyContinue }
        }
    }

    <#
        The voxel weapons in your hands, built here from the copies you already
        have (the models belong to the games and to Cheello); the bundled
        vrweapons_models.pk3 adds the 66 that cannot be derived from them.
        Its output is caught here so -Json keeps stdout to itself.
    #>
    $builder = Join-Path $dest 'build-vrweapons.ps1'
    if (Test-Path -LiteralPath $builder) {
        $overlay = $VRaze
        if (-not $overlay) {
            $bundled = Join-Path $assets 'vrweapons_models.pk3'
            if (Test-Path -LiteralPath $bundled) { $overlay = $bundled }
        }
        try {
            & $builder -Root $dest -VRaze $overlay *>&1 | ForEach-Object { Log ('  ' + [string]$_) }
        } catch {
            Warn 'could not build the voxel weapons pack - everything else still works'
            Info $_.Exception.Message
        }
    } else { Warn 'build-vrweapons.ps1 is missing - no voxel weapons' }

    # ------------------------------------------------------------ music
    <#
        Redneck Rampage is CD audio with no MIDI underneath, and GOG keeps its
        music in a separate free "bonus content" download. Found beside the game,
        anywhere Setup searches, in Downloads, Desktop or Documents, or inside
        the zip GOG leaves (read without unpacking all of it), and named the way
        the engine asks: redneck02..09, redneckrides02..09.
    #>
    $soundtracks = @(
        @{ Dir = Join-Path $dest 'games\rampage';       Base = 'redneck';      Name = 'Redneck Rampage';             Match = '*Redneck Rampage Soundtrack*'; Zip = '*redneck_rampage_soundtrack*.zip' },
        @{ Dir = Join-Path $dest 'games\rampage\AGAIN'; Base = 'redneckrides'; Name = 'Redneck Rampage Rides Again'; Match = '*Rides Again soundtrack*';     Zip = '*rides_again_soundtrack*.zip' }
    )
    Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
    function Get-SongsFromZip($path, $st, $depth) {
        try { $zip = [System.IO.Compression.ZipFile]::OpenRead($path) } catch { return @() }
        $audio = @(); $inner = @()
        try {
            foreach ($e in $zip.Entries) {
                if ($e.Name -match '\.(mp3|ogg|flac)$') { $audio += $e.FullName }
                elseif ($e.Name -match '\.zip$' -and $e.Name -like $st.Zip) { $inner += $e.FullName }
            }
        } finally { $zip.Dispose() }
        if ($audio.Count -gt 0) {
            $tmp = Join-Path $env:TEMP ('razexr_mus_' + [guid]::NewGuid().ToString())
            try {
                Expand-Archive -LiteralPath $path -DestinationPath $tmp -Force -ErrorAction Stop
                return @(Get-ChildItem -LiteralPath $tmp -File -Recurse -ErrorAction SilentlyContinue |
                         Where-Object { $_.Extension -match '^\.(mp3|ogg|flac)$' } | Sort-Object Name)
            } catch { return @() }
        }
        # A zip of zips, one level only: following archives without a bound hangs.
        if ($inner.Count -gt 0 -and $depth -lt 1) {
            $tmp = Join-Path $env:TEMP ('razexr_musz_' + [guid]::NewGuid().ToString())
            try { Expand-Archive -LiteralPath $path -DestinationPath $tmp -Force -ErrorAction Stop } catch { return @() }
            foreach ($z in (Get-ChildItem -LiteralPath $tmp -File -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.Name -like $st.Zip })) {
                $songs = @(Get-SongsFromZip $z.FullName $st ($depth + 1))
                if ($songs.Count -gt 0) { return $songs }
            }
        }
        return @()
    }
    function Find-Soundtrack($st) {
        $places = @((Join-Path $dest 'games\rampage'))
        $places += $roots
        if (-not $NoSearch) {
            foreach ($d in @("$(UserHome)\Downloads", "$(UserHome)\Desktop", "$(UserHome)\Documents")) {
                if (Test-Path -LiteralPath $d) { $places += $d }
            }
        }
        foreach ($r in $places) {
            if (-not (Test-Path -LiteralPath $r)) { continue }
            foreach ($d in (Get-ChildItem -LiteralPath $r -Directory -Recurse -Depth 4 -ErrorAction SilentlyContinue | Where-Object { $_.Name -like $st.Match })) {
                $songs = @(Get-ChildItem -LiteralPath $d.FullName -File -ErrorAction SilentlyContinue |
                           Where-Object { $_.Extension -match '^\.(mp3|ogg|flac)$' } | Sort-Object Name)
                if ($songs.Count -gt 0) { return $songs }
            }
        }
        foreach ($r in $places) {
            if (-not (Test-Path -LiteralPath $r)) { continue }
            foreach ($z in (Get-ChildItem -LiteralPath $r -File -Recurse -Depth 4 -ErrorAction SilentlyContinue | Where-Object { $_.Name -like '*redneck*.zip' })) {
                $songs = @(Get-SongsFromZip $z.FullName $st 0)
                if ($songs.Count -gt 0) { Info ('using the soundtrack from ' + $z.Name); return $songs }
            }
        }
        return @()
    }
    foreach ($st in $soundtracks) {
        if (-not (Test-Path -LiteralPath $st.Dir)) { continue }
        $musicDir = Join-Path $st.Dir 'music'
        if (Test-Path -LiteralPath (Join-Path $musicDir ($st.Base + '02.mp3'))) { continue }
        $songs = @(Find-Soundtrack $st)
        if ($songs.Count -eq 0) {
            Warn ('{0} has no music - the game will be silent' -f $st.Name)
            Info "Its soundtrack is a separate free 'bonus content' download on GOG,"
            Info 'not part of the game installer - so you already own it. Get it from'
            Info "the game's page in your GOG library, then either:"
            Info '  - leave the .zip in your Downloads folder and run SETUP.bat again'
            Info '    (it is read straight out of the zip - no need to unpack it), or'
            Info ('  - unpack it and put the soundtrack folder in ' + $st.Dir)
            continue
        }
        [void][System.IO.Directory]::CreateDirectory($musicDir)
        $track = 2; $n = 0
        foreach ($song in $songs) {
            if ($track -gt 9) { break }
            Copy-Item -LiteralPath $song.FullName -Destination (Join-Path $musicDir ('{0}{1:D2}{2}' -f $st.Base, $track, $song.Extension)) -Force -ErrorAction SilentlyContinue
            $track++; $n++
        }
        if ($n -gt 0) { Ok ('{0,-24} {1} CD tracks' -f 'Redneck music', $n) }
    }

    <#
        CD music that shipped with a different copy of the same game. Steam's
        and GOG's PowerSlave carry a byte-identical STUFF.DAT, and only Steam's
        has the soundtrack DLC's MUSIC folder beside it, so a game that came
        across without music gets one more look at the other copies.
    #>
    foreach ($g in $games) {
        $gdir = Join-Path (Join-Path $dest 'games') $g.Folder
        if (-not (Test-Path -LiteralPath $gdir)) { continue }
        if (@(Get-ChildItem -LiteralPath $gdir -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -ieq 'music' }).Count -gt 0) { continue }
        $srcMusic = $null
        foreach ($r in $roots) {
            foreach ($fn in $g.Files) {
                foreach ($cand in (Get-ChildItem -LiteralPath $r -Filter $fn -Recurse -File -Depth 4 -ErrorAction SilentlyContinue | Select-Object -First 5)) {
                    if ($g.Folder -eq 'ridesagain' -and $cand.Length -ne $RIDESAGAIN_SIZE) { continue }
                    if ($g.Folder -eq 'rampage'    -and $cand.Length -eq $RIDESAGAIN_SIZE) { continue }
                    if ($g.Folder -eq 'exhumed'    -and $cand.Length -lt $EXHUMED_MIN)     { continue }
                    if ($g.Folder -eq 'duke'       -and $cand.Length -lt $DUKE_ATOMIC_MIN) { continue }
                    $m = Join-Path $cand.DirectoryName 'MUSIC'
                    if (Test-Path -LiteralPath $m) { $srcMusic = $m; break }
                }
                if ($srcMusic) { break }
            }
            if ($srcMusic) { break }
        }
        if (-not $srcMusic) { continue }
        $dstMusic = Join-Path $gdir 'music'
        [void][System.IO.Directory]::CreateDirectory($dstMusic)
        $n = 0
        foreach ($f in Get-ChildItem -LiteralPath $srcMusic -File -ErrorAction SilentlyContinue) {
            if ($skipExt -contains $f.Extension.ToLower()) { continue }
            $o = Join-Path $dstMusic $f.Name
            if (-not (Test-Path -LiteralPath $o)) { Copy-Item -LiteralPath $f.FullName -Destination $o -ErrorAction SilentlyContinue; $n++ }
        }
        if ($n -gt 0) { Ok ('{0,-24} {1} CD tracks' -f ($g.Name + ' music'), $n) }
    }

    # ------------------------------------------------------------ launchers
    Line ''
    Line 'Writing launchers'
    $anyGrp = Get-ChildItem -LiteralPath (Join-Path $dest 'games') -Include *.grp, *.rff, *.dat -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($anyGrp) { $seedPath = $anyGrp.FullName }
    if (-not $seedPath) {
        # An empty -gamegrp finds no game and takes the engine down. Never send one.
        Warn 'no game data found - skipping launcher generation'
    } else {
        $cfg = Join-Path $dest 'cfg_setup.ini'
        $log = Join-Path $dest 'log-launchers.txt'
        if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force -ErrorAction SilentlyContinue }
        <#
            -portable keeps the scan inside this folder; -novr writes the launchers
            without waiting for a headset; +quit ends the run on its own. The list
            is quoted by hand: PowerShell 5.1 joins -ArgumentList with spaces and
            quotes nothing.
        #>
        <#
            The engine writes its own start-up logs beside raze.exe and keeps
            the run before as .prev.log. This run is only Setup's, so the
            player's logs from their last game are put back as they were (and
            ones this run made alone go), or a crash log would be lost to an
            update, and an update would differ from a fresh install.
        #>
        $engineLogs = @('startup.log', 'startup.prev.log', 'razexr_vr.log', 'razexr_vr.prev.log')
        $keep = Join-Path ([System.IO.Path]::GetTempPath()) ('razexr_logs_' + [guid]::NewGuid().ToString())
        [void][System.IO.Directory]::CreateDirectory($keep)
        foreach ($l in $engineLogs) {
            $p0 = Join-Path $dest $l
            if (Test-Path -LiteralPath $p0) { Copy-Item -LiteralPath $p0 -Destination (Join-Path $keep $l) }
        }
        $razeArgs = ('-nosetup -portable -novr -gamegrp "{0}" -config "{1}" +logfile "{2}" +vrwritelaunchers +quit' -f $seedPath, $cfg, $log)
        $p = Start-Process -FilePath $exe -ArgumentList $razeArgs -WorkingDirectory $dest -PassThru -WindowStyle Minimized
        # About a second normally. The first run on a PC compiles the graphics
        # driver's shaders and took over a minute once, so the backstop is long.
        if (-not $p.WaitForExit(180000)) { $p.Kill() }
        Start-Sleep -Seconds 1
        $wrote = $null
        if (Test-Path -LiteralPath $log) { $wrote = Select-String -LiteralPath $log -Pattern 'Wrote (\d+) launchers' }
        if ($wrote) {
            Ok $wrote.Matches[0].Value
            $lines = Get-Content -LiteralPath $log
            $end = $wrote.LineNumber - 2
            $start = $end
            while ($start -ge 0 -and $lines[$start] -match '^  \S') { $start-- }
            for ($i = $start + 1; $i -le $end; $i++) { Info $lines[$i].Trim() }
        } else {
            Log '  LAUNCHERS WERE NOT WRITTEN.' 'Red'
            if (Test-Path -LiteralPath $log) {
                Log '  The engine ran but did not report - last lines of its log:' 'Red'
                foreach ($l in (Get-Content -LiteralPath $log -Tail 5)) { Info $l }
            } else {
                Log '  The engine did not start, or wrote no log.' 'Red'
                Info ('tried: {0} {1}' -f $exe, $razeArgs)
            }
        }
        Remove-Item -LiteralPath $cfg -ErrorAction SilentlyContinue
        foreach ($l in $engineLogs) {
            $p0 = Join-Path $dest $l
            $k0 = Join-Path $keep $l
            if (Test-Path -LiteralPath $k0) { Copy-Item -LiteralPath $k0 -Destination $p0 -Force }
            elseif (Test-Path -LiteralPath $p0) { Remove-Item -LiteralPath $p0 -Force -ErrorAction SilentlyContinue }
        }
        Remove-Item -LiteralPath $keep -Recurse -Force -ErrorAction SilentlyContinue
        # Before 1.2.1 this run's log was setup.log; log-launchers.txt now.
        $o = Join-Path $dest 'setup.log'
        if (Test-Path -LiteralPath $o) { Remove-Item -LiteralPath $o -Force -ErrorAction SilentlyContinue }
    }

    # ------------------------------------------------------------ the kit
    # Uninstall and Collect logs, there without the download.
    $tools = Join-Path $dest 'tools'
    [void][System.IO.Directory]::CreateDirectory($tools)
    $me = Join-Path $Here 'setup.ps1'
    if ((Resolve-Path -LiteralPath $me).ProviderPath -ne (Join-Path $tools 'setup.ps1')) {
        Copy-Item -LiteralPath $me -Destination (Join-Path $tools 'setup.ps1') -Force
    }
    Set-Content -LiteralPath (Join-Path $dest 'Uninstall.bat') -Encoding ASCII -Value @(
        '@echo off',
        ('rem ' + $Product + ': removes what Setup installed.  Your saves, settings and Game Tome notes are kept.'),
        'powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\setup.ps1" -Uninstall %*',
        'if not defined VRXR_NOPAUSE pause')
    Set-Content -LiteralPath (Join-Path $dest 'Collect logs.bat') -Encoding ASCII -Value @(
        '@echo off',
        ('rem ' + $Product + ': zips the logs onto your desktop, to send with a report.'),
        'powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\setup.ps1" -Collect %*',
        'if not defined VRXR_NOPAUSE pause')
    Set-Content -LiteralPath (Join-Path $dest 'version.txt') -Value ($Product + ' ' + $Version) -Encoding ASCII

    # ------------------------------------------------------------ desktop and Steam
    $play = Join-Path $dest $PlayBat
    if (-not $NoShortcuts -and (Test-Path -LiteralPath $play)) {
        Make-Shortcut (Join-Path (Desktop) $DesktopLink) $play $dest $exe ($Product + ': the game you played last')
        Log ('  Desktop shortcut: ' + $DesktopLink)
    }
    $Result['steam'] = 'skipped'
    if (-not $NoSteam -and (Test-Path -LiteralPath $play) -and (Steam-Configs).Count -gt 0) {
        $want = $false
        if ($NoPrompt) { $want = [bool]$AddToSteam }
        else { Say ''; $want = Ask-YesNo ('Add ' + $Product + ' to your Steam library?') }
        if ($want) {
            if (Steam-Ready $true) {
                $Result['steam'] = Steam-Add $dest
                Log ('Added to your Steam library (and SteamVR''s).')
                Steam-Reopen
            } else {
                $Result['steam'] = 'skipped: Steam is open'
                Log 'Not added to Steam: Steam has to be closed.  Run SETUP.bat again any time to add it.'
            }
        }
    }

    # Where it is, for the next run of a newer Setup.
    [void][System.IO.Directory]::CreateDirectory((Split-Path -Parent $Remembered))
    Set-Content -LiteralPath $Remembered -Value $dest -Encoding UTF8

    # ------------------------------------------------------------ the manifest
    $manifest = New-Object System.Collections.Generic.List[string]
    foreach ($rel in Payload-List) { if (Test-Path -LiteralPath (Join-Path $dest $rel)) { $manifest.Add($rel) } }
    foreach ($a in $MadeAssets) { if (Test-Path -LiteralPath (Join-Path $assets $a)) { $manifest.Add('assets\' + $a) } }
    $ldir = Join-Path $dest 'launchers'
    if (Test-Path -LiteralPath $ldir) {
        foreach ($f in Get-ChildItem -LiteralPath $ldir -Filter '*.bat' -File | Sort-Object Name) { $manifest.Add('launchers\' + $f.Name) }
    }
    foreach ($k in @($PlayBat, 'tools\setup.ps1', 'Uninstall.bat', 'Collect logs.bat', 'version.txt', 'log-launchers.txt', 'setup-log.txt', 'install-manifest.txt')) {
        if (-not $manifest.Contains($k)) { $manifest.Add($k) }
    }
    $owned = Games-Owned $dest $sources
    $head = @(('# ' + $Product + ' ' + $Version),
              '# Uninstall removes exactly these files.  Saves, settings (config), the Game Tome',
              '# notes (ink.txt, place.txt, typed.txt) and anything you added are not in it.')
    foreach ($k in ($sources.Keys | Sort-Object)) { $head += ('# source ' + $k + ': ' + $sources[$k]) }
    Set-Content -LiteralPath (Join-Path $dest 'install-manifest.txt') -Value ($head + ($manifest | Select-Object -Unique) + $owned) -Encoding UTF8

    Line ''
    Line 'Done.'
    Line ''
    Line ('RazeXR PCVR is in ' + $dest)
    Line 'Start Virtual Desktop and connect your headset FIRST, then run'
    Line ('  ' + $PlayBat + ' (the desktop shortcut starts it too)')
    Line 'It opens the game you played last, and Switch Game in the menu moves'
    Line 'between all of them without leaving the headset.'
    Copy-Item -LiteralPath $LogFile -Destination (Join-Path $dest 'setup-log.txt') -Force
    $Result['result'] = 'installed'
}

# ---------------------------------------------------------------- uninstall

function Remove-Empty([string] $dir) {
    if (-not (Test-Path -LiteralPath $dir)) { return }
    foreach ($d in Get-ChildItem -LiteralPath $dir -Directory -Recurse -Force | Sort-Object { $_.FullName.Length } -Descending) {
        if (-not (Get-ChildItem -LiteralPath $d.FullName -Force)) { Remove-Item -LiteralPath $d.FullName -Force }
    }
    if (-not (Get-ChildItem -LiteralPath $dir -Force)) { Remove-Item -LiteralPath $dir -Force }
}

function Do-Uninstall {
    Line ''
    Line 'RazeXR PCVR uninstall'
    Line ''
    $dest = Find-Home $false
    $Result['steam'] = 'none'
    if ($dest -and (Steam-HasOurs $dest)) {
        if (Steam-Ready $true) {
            Steam-Remove $dest
            $Result['steam'] = 'removed'
            Log 'Removed from your Steam library.'
            Steam-Reopen
        } else {
            $Result['steam'] = 'left: Steam is open'
            Log 'Still in your Steam library: Steam has to be closed.  Close it and run Uninstall again.'
        }
    }
    $lnk = Join-Path (Desktop) $DesktopLink
    if ($dest -and (Test-Path -LiteralPath $lnk) -and ((Shortcut-Target $lnk) -like ($dest + '\*'))) {
        Remove-Item -LiteralPath $lnk -Force
        Log ('Removed the desktop shortcut ' + $DesktopLink)
    }
    if (-not $dest -or -not (Test-Path -LiteralPath (Join-Path $dest 'install-manifest.txt'))) {
        Log 'Nothing to uninstall: no RazeXR PCVR folder installed by Setup was found.'
        $Result['result'] = 'nothing installed'
        return
    }
    $Result['vrFolder'] = $dest
    Log ('Removing ' + $dest)
    foreach ($rel in (Get-Content -LiteralPath (Join-Path $dest 'install-manifest.txt') | Where-Object { $_ -and -not $_.StartsWith('#') })) {
        if ($rel.StartsWith('junction:')) {
            $j = Join-Path $dest $rel.Substring(9)
            if (Test-Path -LiteralPath $j) { cmd /c rmdir "$j" 2>$null | Out-Null }	# the link only, never the game behind it
            continue
        }
        $p = Join-Path $dest $rel
        if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force }
    }
    # What the game wrote that is not the player's: logs and crash dumps.
    foreach ($pat in @('*.log', '*.prev.log', '*.dmp')) {
        Get-ChildItem -LiteralPath $dest -Filter $pat -File -ErrorAction SilentlyContinue | Remove-Item -Force
        Get-ChildItem -LiteralPath (Join-Path $dest 'logs') -Filter $pat -File -ErrorAction SilentlyContinue | Remove-Item -Force
    }
    Get-ChildItem -LiteralPath (Join-Path $dest 'gametome\layer') -Filter '*_log.txt' -File -ErrorAction SilentlyContinue | Remove-Item -Force
    if (Test-Path -LiteralPath $Remembered) {
        if (([string](Get-Content -LiteralPath $Remembered -TotalCount 1)).Trim() -eq $dest) { Remove-Item -LiteralPath $Remembered -Force }
    }
    Remove-Empty $dest
    if (Test-Path -LiteralPath $dest) {
        Set-Content -LiteralPath (Join-Path $dest 'Your saves are kept here.txt') -Encoding ASCII -Value @(
            ($Product + ' was uninstalled.  Your saved games, settings and Game Tome notes are'),
            'kept here, and come back if you install it again.',
            'Delete this folder to remove them too.')
        Log '  Your saves and settings are kept there.'
    } else { Log '  Removed.' }
    Log ''
    Log ($Product + ' is uninstalled.  Your games themselves are untouched.')
    $Result['result'] = 'uninstalled'
}

# ---------------------------------------------------------------- collect logs

function Do-Collect {
    Line ''
    Line 'RazeXR PCVR: collect logs'
    Line ''
    $dest = Find-Home $false
    $stamp = Get-Date -Format 'yyyyMMdd-HHmm'
    $work = Join-Path ([System.IO.Path]::GetTempPath()) ('razexr-pcvr-logs-' + $stamp)
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
    [void][System.IO.Directory]::CreateDirectory($work)
    if ($dest) {
        foreach ($f in @('install-manifest.txt', 'setup-log.txt', 'version.txt', 'log-launchers.txt', 'razexr_vr.log', 'razexr_vr.prev.log')) {
            $p = Join-Path $dest $f
            if (Test-Path -LiteralPath $p) { Copy-Item -LiteralPath $p -Destination $work }
        }
        foreach ($sub in @('logs', 'config')) {
            $d = Join-Path $dest $sub
            if (Test-Path -LiteralPath $d) {
                $o = Join-Path $work $sub
                [void][System.IO.Directory]::CreateDirectory($o)
                Get-ChildItem -LiteralPath $d -File -ErrorAction SilentlyContinue | Copy-Item -Destination $o
            }
        }
        Get-ChildItem -LiteralPath (Join-Path $dest 'gametome\layer') -Filter '*_log.txt' -File -ErrorAction SilentlyContinue | Copy-Item -Destination $work
        Get-ChildItem -LiteralPath $dest -Filter '*.dmp' -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 2 | Copy-Item -Destination $work
        Log ('Logs from ' + $dest)
    } else { Log 'No RazeXR PCVR folder was found; only this PC''s details go in.' }
    foreach ($m in @('install', 'uninstall')) {
        $p = Join-Path ([System.IO.Path]::GetTempPath()) ('razexr-pcvr-' + $m + '.txt')
        if (Test-Path -LiteralPath $p) { Copy-Item -LiteralPath $p -Destination $work }
    }
    $pc = @(($Product + ' ' + $Version + ' - this PC, ' + (Get-Date -Format 'yyyy-MM-dd HH:mm')), '')
    try { $os = Get-CimInstance Win32_OperatingSystem; $pc += ('Windows: ' + $os.Caption + ' ' + $os.Version) } catch { }
    try { foreach ($v in Get-CimInstance Win32_VideoController) { $pc += ('Graphics: ' + $v.Name + ', driver ' + $v.DriverVersion) } } catch { }
    foreach ($k in @('HKLM:\SOFTWARE\Khronos\OpenXR\1', 'HKCU:\SOFTWARE\Khronos\OpenXR\1')) {
        try { $v = Get-ItemProperty -LiteralPath $k -ErrorAction Stop; if ($v.ActiveRuntime) { $pc += ('OpenXR runtime (' + $k + '): ' + $v.ActiveRuntime) } } catch { }
    }
    foreach ($n in @('VirtualDesktop.Streamer', 'vrserver', 'vrcompositor', 'OVRServer_x64')) {
        if (Get-Process -Name $n -ErrorAction SilentlyContinue) { $pc += ('Running: ' + $n) }
    }
    $pc += ''
    $pc += ('Installed in: ' + $dest)
    $pc += ('Installed version: ' + (Installed-Version $dest))
    $pc += ('In the Steam library: ' + (Steam-HasOurs $dest))
    Set-Content -LiteralPath (Join-Path $work 'this-pc.txt') -Value $pc -Encoding UTF8
    $zip = Join-Path (Desktop) ('RazeXR-PCVR-logs-' + $stamp + '.zip')
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
    Compress-Archive -Path (Join-Path $work '*') -DestinationPath $zip
    Remove-Item -LiteralPath $work -Recurse -Force
    Log ''
    Log ('Made ' + $zip)
    Log 'Send that file to us (Discord or a GitHub issue) with a line on what happened.'
    $Result['zip'] = $zip
    if (-not $NoPrompt) { try { Start-Process explorer.exe -ArgumentList ('/select,"' + $zip + '"') } catch { } }
}

# ---------------------------------------------------------------- status, for -Detect and -Json

function Game-Status {
    $vrHome = Find-Home $false
    $ver = Installed-Version $vrHome
    $found = Find-AllGames
    foreach ($g in $games) {
        $hit = $found[$g.Folder]
        $has = ($ver -ne $null) -and (Home-HasGame $vrHome $g)
        $launcher = $null
        if ($has) { $launcher = Launcher-For $vrHome $g }
        $title = $g.Name + ' VR'
        if ($launcher) { $title = [System.IO.Path]::GetFileNameWithoutExtension($launcher) }
        $note = $null
        if (-not $hit -and -not $has -and $g.Folder -eq 'exhumed') {
            $note = 'Raze needs the DOS original: Steam sells it as PowerSlave (DOS Classic Edition). The 2022 PowerSlave Exhumed will not work.'
        }
        $gameFolder = $null
        if ($hit) { $gameFolder = $hit.DirectoryName } elseif ($has) { $gameFolder = Join-Path (Join-Path $vrHome 'games') $g.Folder }
        [pscustomobject][ordered]@{
            key = $g.Folder; name = $g.Name; title = $title
            found = [bool]($hit -or $has)
            gameFolder = $gameFolder
            installed = [bool]$has
            vrFolder = $(if ($ver) { $vrHome } else { $null })
            installedVersion = $(if ($has) { $ver } else { $null })
            launcher = $launcher
            installNote = $note
        }
    }
}

function Do-Detect {
    Line ''
    Line 'RazeXR PCVR: what is here'
    Search-Roots
    Log ('Looking in ' + $roots.Count + ' place(s):')
    foreach ($r in $roots) { Log ('  ' + $r) }
    $vrHome = Find-Home $false
    if ($vrHome) { Log ('Installed: ' + (Installed-Version $vrHome) + ' in ' + $vrHome) } else { Log 'Not installed.' }
    foreach ($s in Game-Status) {
        if ($s.found) { Log ($s.name + ': ' + $s.gameFolder + $(if ($s.installed) { ', installed' } else { ', not installed yet' })) }
        else { Log ($s.name + ': not found') }
    }
    $Result['vrFolder'] = $vrHome
    $Result['steamLibrary'] = Steam-HasOurs $vrHome
}

switch ($Mode) {
    'detect'    { Do-Detect }
    'install'   { Do-Install }
    'uninstall' { Do-Uninstall }
    'collect'   { Do-Collect }
}
Finish 0
