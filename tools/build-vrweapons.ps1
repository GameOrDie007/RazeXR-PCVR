<#
    build-vrweapons.ps1 - assembles vrweapons.pk3, the voxel weapons held in hand.

    The models are not ours to redistribute and are not shipped. This builds them
    on the user's machine out of things they already have:

      Blood, Shadow Warrior   their own pickup voxels, straight out of BLOOD.RFF
                              and SW.GRP. Domyoji confirmed these are the games'
                              own models and hashing agrees, byte for byte.
      Duke                    Cheello's Voxel Duke 3D pickup models, which setup
                              already downloads. Permission on file.
      everything else         only with a VRaze install, passed as -VRaze. Those
                              models are third-party and stay on the user's disk.

    The definition files live inside raze.pk3 under vrweapons\, which is an inert
    path - Raze only flattens a leading filter\, so nothing loads them there. This
    writes the ones it has models for to filter\<game>\engine\, where they do load.
    A weapon with no model draws its ordinary flat sprite, exactly as before.

    See THIRD-PARTY-PERMISSIONS.md for who gave what permission.
#>

param(
    [string]$Root  = "",
    [string]$VRaze = "",
    [string]$Out   = "",
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (-not $Root) { $Root = Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not (Test-Path (Join-Path $Root "raze.pk3"))) {
    $up = Split-Path -Parent $Root
    if (Test-Path (Join-Path $up "raze.pk3")) { $Root = $up }
}
if (-not $Out) { $Out = Join-Path $Root "vrweapons.pk3" }

function Say($t)  { if (-not $Quiet) { Write-Host "  $t" } }
function Note($t) { if (-not $Quiet) { Write-Host "  $t" -ForegroundColor DarkGray } }

# ---------------------------------------------------------------- archive readers

function Read-Grp($path) {
    # Ken Silverman's GRP: "KenSilverman", count, then count 16-byte entries.
    $d = [System.IO.File]::ReadAllBytes($path)
    $out = @{}
    if ($d.Length -lt 16) { return $out }
    if ([System.Text.Encoding]::ASCII.GetString($d, 0, 12) -ne "KenSilverman") { return $out }
    $n = [BitConverter]::ToUInt32($d, 12)
    $pos = 16 + $n * 16
    for ($i = 0; $i -lt $n; $i++) {
        $e = 16 + $i * 16
        $name = [System.Text.Encoding]::ASCII.GetString($d, $e, 12).Split([char]0)[0].Trim().ToUpper()
        $size = [BitConverter]::ToUInt32($d, $e + 12)
        if ($pos + $size -le $d.Length) {
            $b = New-Object byte[] $size
            [Array]::Copy($d, $pos, $b, 0, $size)
            $out[$name] = $b
        }
        $pos += $size
    }
    return $out
}

function Read-Rff($path) {
    # Blood's RFF: 32-byte header, then a directory encrypted with its own offset.
    $d = [System.IO.File]::ReadAllBytes($path)
    $out = @{}
    if ($d.Length -lt 32) { return $out }
    $dirOfs = [BitConverter]::ToUInt32($d, 8)
    $n      = [BitConverter]::ToUInt32($d, 12)
    if ($dirOfs + $n * 48 -gt $d.Length) { return $out }
    $dir = New-Object byte[] ($n * 48)
    [Array]::Copy($d, $dirOfs, $dir, 0, $dir.Length)
    $p = $dirOfs -band 0xff
    for ($i = 0; $i -lt $dir.Length; $i++) {
        $dir[$i] = $dir[$i] -bxor (($p + ($i -shr 1)) -band 0xff)
    }
    for ($i = 0; $i -lt $n; $i++) {
        $e    = $i * 48
        $ofs  = [BitConverter]::ToUInt32($dir, $e + 16)
        $size = [BitConverter]::ToUInt32($dir, $e + 20)
        $flags = $dir[$e + 32]
        $ext  = [System.Text.Encoding]::ASCII.GetString($dir, $e + 33, 3).Trim([char]0)
        $name = [System.Text.Encoding]::ASCII.GetString($dir, $e + 36, 8).Split([char]0)[0].Trim()
        if ($ofs + $size -le $d.Length) {
            $b = New-Object byte[] $size
            [Array]::Copy($d, $ofs, $b, 0, $size)
            if ($flags -band 0x10) {
                $c = [Math]::Min([int]$size, 256)
                for ($k = 0; $k -lt $c; $k++) { $b[$k] = $b[$k] -bxor (($k -shr 1) -band 0xff) }
            }
            $out[("$name.$ext").ToUpper()] = $b
        }
    }
    return $out
}

function Read-ZipEntries($path, $pattern) {
    $out = @{}
    $z = [System.IO.Compression.ZipFile]::OpenRead($path)
    try {
        foreach ($e in $z.Entries) {
            if ($e.FullName -notlike $pattern) { continue }
            $ms = New-Object System.IO.MemoryStream
            $s = $e.Open()
            $s.CopyTo($ms)
            $s.Close()
            $out[$e.FullName] = $ms.ToArray()
            $ms.Close()
        }
    } finally { $z.Dispose() }
    return $out
}

# ---------------------------------------------------------------- what comes from where
<#
    Every entry was established by hashing VRaze's model against the candidate
    and requiring the bytes to be identical, so none of this is guesswork. The
    handful that are not one-to-one are VRaze's own choices, kept as they are:
    Duke's flamethrower reuses the freeze model, and Shadow Warrior draws the
    guardian head for the head, napalm and ring-of-fire slots.

    Where VRaze animates a weapon, only frame 0 is listed. The frames themselves
    are Domyoji's own work and are not reproducible from game data, so without a
    VRaze install the weapon is present but still.
#>

$SOURCEMAP = [ordered]@{
    "models/weapons/blood/vr_flaregun.kvx"          = "BLOOD|FLAREGUN.KVX"
    "models/weapons/blood/vr_shotgun.kvx"           = "BLOOD|SHOTGUN.KVX"
    "models/weapons/blood/vr_tommygun.kvx"          = "BLOOD|TOMMYGUN.KVX"
    "models/weapons/blood/vr_dynamite.kvx"          = "BLOOD|TNTBUNDL.KVX"
    "models/weapons/blood/vr_spraycan.kvx"          = "BLOOD|SPRAYCAN.KVX"
    "models/weapons/blood/vr_tesla.kvx"             = "BLOOD|SPEARGUN.KVX"
    "models/weapons/blood/vr_lifeleech.kvx"         = "BLOOD|HELLSTAF.KVX"
    "models/weapons/blood/vr_voodoo.kvx"            = "BLOOD|VOODOO.KVX"
    "models/weapons/blood/vr_proximity.kvx"         = "BLOOD|TNTPROX.KVX"
    "models/weapons/blood/vr_remote.kvx"            = "BLOOD|REMOTE.KVX"
    "models/weapons/blood/vr_napalm.kvx"            = "BLOOD|DARKGUN.KVX"
    "models/weapons/blood/vr_napalm0.kvx"           = "BLOOD|DARKGUN.KVX"

    "models/weapons/duke/vr_weapon_pistol0.kvx"     = "DUKE|voxels/weapons/21_PISTOL.kvx"
    "models/weapons/duke/vr_weapon_shotgun.kvx"     = "DUKE|voxels/weapons/28_SHOTGUN.kvx"
    "models/weapons/duke/vr_weapon_chaingun.kvx"    = "DUKE|voxels/weapons/22_RIPPER.kvx"
    "models/weapons/duke/vr_weapon_rpg.kvx"         = "DUKE|voxels/weapons/23_RPG.kvx"
    "models/weapons/duke/vr_weapon_handbomb.kvx"    = "DUKE|voxels/items/26_PIPE.kvx"
    "models/weapons/duke/vr_weapon_shrinker.kvx"    = "DUKE|voxels/weapons/25_SHRINK.kvx"
    "models/weapons/duke/vr_weapon_devastator.kvx"  = "DUKE|voxels/weapons/29_DEVASTATOR.kvx"
    "models/weapons/duke/vr_weapon_tripbomb.kvx"    = "DUKE|voxels/weapons/27_TRIPBOMB.kvx"
    "models/weapons/duke/vr_weapon_freeze.kvx"      = "DUKE|voxels/weapons/24_FREEZE.kvx"
    "models/weapons/duke/vr_weapon_grow.kvx"        = "DUKE|voxels/weapons/32_EXPANDER.kvx"
    "models/weapons/duke/vr_weapon_flamethrower.kvx"= "DUKE|voxels/weapons/24_FREEZE.kvx"

    "models/weapons/sw/vr_star.kvx"                 = "SW|VOXEL000.KVX"
    "models/weapons/sw/vr_uzi.kvx"                  = "SW|UZI.KVX"
    "models/weapons/sw/vr_grenade.kvx"              = "SW|GRENADE.KVX"
    "models/weapons/sw/vr_heart.kvx"                = "SW|HEART.KVX"
    "models/weapons/sw/vr_shotgun.kvx"              = "SW|SHOTGUN.KVX"
    "models/weapons/sw/vr_micro.kvx"                = "SW|ROCKET.KVX"
    "models/weapons/sw/vr_rocket.kvx"               = "SW|ROCKET.KVX"
    "models/weapons/sw/vr_mine.kvx"                 = "SW|MINES.KVX"
    "models/weapons/sw/vr_hothead.kvx"              = "SW|GOROHEAD.KVX"
    "models/weapons/sw/vr_napalm.kvx"               = "SW|GOROHEAD.KVX"
    "models/weapons/sw/vr_ring.kvx"                 = "SW|GOROHEAD.KVX"
    "models/weapons/sw/vr_rail0.kvx"                = "SW|RAILGUN.KVX"
}

# Duke's models above are Cheello's, which is what VRaze used and what the scales
# were tuned against - but that pack is on ModDB and cannot be fetched by script.
# The Duke3D Voxel Pack can be, and setup downloads it. Its weapon pickups are
# different art on the same voxel grid (checked dimension by dimension, within a
# few voxels either way), so the placements hold. Used only where Cheello's is
# absent, which for most people is always.
$FALLBACK = [ordered]@{
    "models/weapons/duke/vr_weapon_pistol0.kvx"     = "DUKE2|voxels/pickups/0021_pistol.kvx"
    "models/weapons/duke/vr_weapon_shotgun.kvx"     = "DUKE2|voxels/pickups/0028_shotgun.kvx"
    "models/weapons/duke/vr_weapon_chaingun.kvx"    = "DUKE2|voxels/pickups/0022_chaingun.kvx"
    "models/weapons/duke/vr_weapon_rpg.kvx"         = "DUKE2|voxels/pickups/0023_rpg.kvx"
    "models/weapons/duke/vr_weapon_handbomb.kvx"    = "DUKE2|voxels/pickups/0026_pipebomb.kvx"
    "models/weapons/duke/vr_weapon_shrinker.kvx"    = "DUKE2|voxels/pickups/0025_shrinker.kvx"
    "models/weapons/duke/vr_weapon_devastator.kvx"  = "DUKE2|voxels/pickups/0029_devastator.kvx"
    "models/weapons/duke/vr_weapon_tripbomb.kvx"    = "DUKE2|voxels/pickups/0027_tripbomb.kvx"
    "models/weapons/duke/vr_weapon_freeze.kvx"      = "DUKE2|voxels/pickups/0024_freezer.kvx"
    "models/weapons/duke/vr_weapon_grow.kvx"        = "DUKE2|voxels/pickups/0032_expander.kvx"
    "models/weapons/duke/vr_weapon_flamethrower.kvx"= "DUKE2|voxels/pickups/0024_freezer.kvx"
}

# ---------------------------------------------------------------- gather the sources

$games = Join-Path $Root "games"
$sources = @{}

function First-Existing($paths) {
    foreach ($p in $paths) { if ($p -and (Test-Path $p)) { return $p } }
    return $null
}

$rff = First-Existing @((Join-Path $games "blood\BLOOD.RFF"), (Join-Path $Root "BLOOD.RFF"))
if ($rff) {
    $sources["BLOOD"] = Read-Rff $rff
    Say ("Blood: {0} voxels in {1}" -f ($sources["BLOOD"].Keys | Where-Object { $_ -like "*.KVX" }).Count, (Split-Path -Leaf $rff))
} else { Note "Blood data not found - its weapons will be skipped" }

$swDir = Join-Path $games "shadowwarrior"
$grp = $null
if (Test-Path $swDir) {
    foreach ($cand in @("Sw.grp", "SW.GRP", "WT.GRP", "TD.grp")) {
        $p = Join-Path $swDir $cand
        if (Test-Path $p) {
            $t = Read-Grp $p
            if (($t.Keys | Where-Object { $_ -like "*.KVX" }).Count -gt 0) { $sources["SW"] = $t; $grp = $p; break }
        }
    }
}
if ($grp) {
    Say ("Shadow Warrior: {0} voxels in {1}" -f ($sources["SW"].Keys | Where-Object { $_ -like "*.KVX" }).Count, (Split-Path -Leaf $grp))
} else { Note "Shadow Warrior data not found - its weapons will be skipped" }

$duke = First-Existing @((Join-Path $Root "voxel_duke3d.zip"))
if ($duke) {
    $sources["DUKE"] = Read-ZipEntries $duke "*.kvx"
    Say ("Duke: {0} voxels in Voxel Duke 3D" -f $sources["DUKE"].Count)
} else { Note "Voxel Duke 3D not present - falling back to the Duke3D Voxel Pack" }

$duke2 = First-Existing @((Join-Path $Root "duke3d_voxels.zip"))
if ($duke2) {
    $sources["DUKE2"] = Read-ZipEntries $duke2 "*.kvx"
    Say ("Duke fallback: {0} voxels in the Duke3D Voxel Pack" -f $sources["DUKE2"].Count)
} elseif (-not $duke) { Note "no Duke voxel pack found - Duke's weapons will be skipped" }

# ---------------------------------------------------------------- assemble the models

$models = [ordered]@{}
$fromGame = 0

foreach ($path in $SOURCEMAP.Keys) {
    $bits = $SOURCEMAP[$path].Split("|")
    $src = $sources[$bits[0]]
    if (-not $src) { continue }
    $lump = $bits[1]
    if (-not $src.Contains($lump)) { Note "missing from source data: $lump"; continue }
    $models[$path] = $src[$lump]
    $fromGame++
}
# Anything the first choice could not supply, try the fallback table for.
foreach ($path in $FALLBACK.Keys) {
    if ($models.Contains($path)) { continue }
    $bits = $FALLBACK[$path].Split("|")
    $src = $sources[$bits[0]]
    if (-not $src) { continue }
    if (-not $src.Contains($bits[1])) { continue }
    $models[$path] = $src[$bits[1]]
    $fromGame++
}
Say "$fromGame models from your own game data"

# A VRaze install, if there is one, supplies the rest: the animation frames, the
# edited variants, and the four games whose models are third-party. It wins where
# the two overlap, because those edits are what the offsets were tuned against.
$fromVRaze = 0
if ($VRaze) {
    if (-not (Test-Path $VRaze)) { throw "VRaze pk3 not found: $VRaze" }
    $vr = Read-ZipEntries $VRaze "models/weapons/*.kvx"
    foreach ($k in $vr.Keys) {
        if ($k -like "*/originals/*") { continue }
        $models[$k] = $vr[$k]
        $fromVRaze++
    }
    Say "$fromVRaze models from the VRaze install"
}

if ($models.Count -eq 0) {
    Note "no weapon models could be built - the games will use their flat sprites"
    exit 0
}

# ---------------------------------------------------------------- the definitions

$pk3 = Join-Path $Root "raze.pk3"
if (-not (Test-Path $pk3)) { throw "raze.pk3 not found next to this script or at $Root" }
$defs = Read-ZipEntries $pk3 "vrweapons/filter/*"
if ($defs.Count -eq 0) { throw "raze.pk3 carries no weapon definitions - it predates this script" }

$enc = New-Object System.Text.UTF8Encoding($false)

# vr_weapons.def binds a model path to a tile. Drop the lines whose model we do
# not have, so nothing logs an error over a file that was never going to be there,
# and remember which tiles survived.
function Filter-Weapons($text, [ref]$kept) {
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($line in $text -split "`n") {
        $m = [regex]::Match($line, '^\s*voxel\s+"([^"]+)"\s*\{\s*tile\s+(\d+)')
        if ($m.Success) {
            if (-not $models.Contains($m.Groups[1].Value)) { continue }
            $kept.Value[[int]$m.Groups[2].Value] = $true
        }
        $out.Add($line.TrimEnd("`r"))
    }
    return ($out -join "`r`n")
}

# vr_weapon_animations.def maps a 2D frame to a voxel tile. A frame pointing at a
# tile we dropped would freeze the weapon on a blank; drop those, and drop any
# weapon block left with no frames at all.
function Filter-Animations($text, $kept) {
    $out = New-Object System.Collections.Generic.List[string]
    $block = New-Object System.Collections.Generic.List[string]
    $frames = 0
    $depth = 0
    foreach ($raw in $text -split "`n") {
        $line = $raw.TrimEnd("`r")
        if ($depth -eq 0 -and $line -match '^\s*weapon\s+\S+') { $block.Clear(); $frames = 0 }
        if ($block.Count -gt 0 -or ($line -match '^\s*weapon\s+\S+')) {
            $m = [regex]::Match($line, '^\s*frame\s+(\d+)\s+(\d+)')
            if ($m.Success -and -not $kept.Contains([int]$m.Groups[2].Value)) { continue }
            if ($m.Success) { $frames++ }
            $block.Add($line)
            if ($line -match '\{') { $depth++ }
            if ($line -match '\}') {
                $depth--
                if ($depth -le 0) {
                    if ($frames -gt 0) { foreach ($b in $block) { $out.Add($b) } }
                    $block.Clear(); $frames = 0; $depth = 0
                }
            }
            continue
        }
        $out.Add($line)
    }
    return ($out -join "`r`n")
}

# ---------------------------------------------------------------- write the pack

function Write-Pack($outPath, $modelFilter, $gameFilter) {
    $entries = [ordered]@{}
    $kept = @{}

    foreach ($name in ($defs.Keys | Sort-Object)) {
        $rel = $name.Substring("vrweapons/".Length)          # filter/<game>/engine/<file>
        $parts = $rel.Split("/")
        if ($parts.Length -lt 4) { continue }
        $game = $parts[1]
        if ($gameFilter -and $game -notmatch $gameFilter) { continue }
        $text = $enc.GetString($defs[$name])
        if ($parts[3] -eq "vr_weapons.def") {
            $k = @{}
            $body = Filter-Weapons $text ([ref]$k)
            if ($k.Count -eq 0) { continue }                  # this game got no models
            foreach ($t in $k.Keys) { $kept[$t] = $true }
            $entries[$rel] = $body
        } else {
            $entries[$rel] = $text
        }
    }

    # Second pass now that every surviving tile is known.
    foreach ($rel in @($entries.Keys)) {
        if ($rel.EndsWith("vr_weapon_animations.def")) {
            $entries[$rel] = Filter-Animations $entries[$rel] $kept
        }
    }
    # A game whose vr_weapons.def was dropped must not keep its other two files.
    $live = @{}
    foreach ($rel in $entries.Keys) { if ($rel.EndsWith("vr_weapons.def")) { $live[$rel.Split("/")[1]] = $true } }
    foreach ($rel in @($entries.Keys)) { if (-not $live.Contains($rel.Split("/")[1])) { $entries.Remove($rel) } }

    if ($entries.Count -eq 0) { return 0 }

    if (Test-Path $outPath) { Remove-Item $outPath -Force }
    $zip = [System.IO.Compression.ZipFile]::Open($outPath, "Create")
    $n = 0
    try {
        foreach ($rel in $entries.Keys) {
            $e = $zip.CreateEntry($rel, [System.IO.Compression.CompressionLevel]::NoCompression)
            $s = $e.Open()
            $b = $enc.GetBytes($entries[$rel])
            $s.Write($b, 0, $b.Length)
            $s.Close()
        }
        foreach ($path in $models.Keys) {
            if ($modelFilter -and $path -notmatch $modelFilter) { continue }
            # Stored, not deflated: .kvx are small and Raze memory-maps the archive.
            $e = $zip.CreateEntry($path, [System.IO.Compression.CompressionLevel]::NoCompression)
            $s = $e.Open()
            $b = $models[$path]
            $s.Write($b, 0, $b.Length)
            $s.Close()
            $n++
        }
    } finally { $zip.Dispose() }
    return $n
}

$n = Write-Pack $Out $null $null
Say ("{0}: {1} models, {2:N0} bytes" -f (Split-Path -Leaf $Out), $n, (Get-Item $Out).Length)

# Redneck loads a pack of its own. The full one overruns a stack in Route 66, so
# it gets the same weapons through an archive holding nothing else.
$rrModels = @($models.Keys | Where-Object { $_ -like "models/weapons/rr/*" })
if ($rrModels.Count -gt 0) {
    $rrOut = Join-Path (Split-Path -Parent $Out) "vrweapons_rr.pk3"
    $n = Write-Pack $rrOut "^models/weapons/rr/" "^redneck"
    Say ("{0}: {1} models" -f (Split-Path -Leaf $rrOut), $n)
}
