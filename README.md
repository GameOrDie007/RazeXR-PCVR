# RazeXR PCVR

Seven Build engine games in PC VR: **Duke Nukem 3D**, **Blood**, **Shadow Warrior**,
**Redneck Rampage**, **NAM**, **WWII GI** and **Exhumed/PowerSlave** — plus their
expansions, seventeen entries in all.

This is a PC port of [Team Beef](https://www.teambeef.org/)'s **RazeXR**, their Quest VR
build of [Raze](https://github.com/ZDoom/Raze). RazeXR runs on Android and OpenXR; this
moves it to Windows, desktop OpenGL and PCVR, and adds voxel weapons held in your hand.

Tested with a Quest 3 over Virtual Desktop (VDXR).

## Credits

This exists because of other people's work.

- **Raze** — Christoph Oelckers, Mitchell Richters and the ZDoom team. See `AUTHORS.md`
  for the full chain back through EDuke32, JFDuke3D, NBlood, PCExhumed, SWP and the
  BUILD engine by Ken Silverman.
- **RazeXR** — [Team Beef](https://www.teambeef.org/) (DrBeef and contributors). The VR
  layer in `vr/RazeXR/` is theirs, imported and adapted for PC; the head and hand
  tracking, the VR projection and the input scheme are all their design.
- **VRaze** — Domyoji, whose voxel weapon data this port's weapon code was written
  against. See "Voxel weapons" below.
- **Duke3D Voxel Pack** — ReaperMan and the Duke4.net community.
- The original developers: 3D Realms, Monolith, Lobotomy Software, Xatrix, TNT Team.

## What this adds over RazeXR

- Windows / desktop OpenGL instead of Android and GLES
- A desktop mirror, so the monitor shows what the headset sees
- Voxel weapons held at the controller, across all seven games
- Smooth turn by default, and Alt Weapon bound to the off-hand stick click
- Per-game launcher scripts and a self-contained portable layout
- A `vrweapons` console command that prints the resolved weapon table

## Requirements

You need your own copies of the games. Nothing here contains game data.

- A PCVR headset and an OpenXR runtime (Virtual Desktop's VDXR, SteamVR or Oculus)
- The game data for whichever games you want — `DUKE3D.GRP`, `BLOOD.RFF`, `SW.GRP`,
  `REDNECK.GRP`, `NAM.GRP`, `WW2GI.GRP`, `STUFF.DAT` and so on
- `libsndfile-1.dll` beside the executable, or Ogg music will be silent while MIDI
  games play normally

## Optional extras

Neither is included, and neither is ours to distribute. Both are picked up
automatically if you put them beside the executable.

**Voxel weapons** need `vrweapons.pk3`, built from your own VRaze installation with
`tools/build-vrweapons-pk3.py`. **VRaze's public downloads have been withdrawn**, so if
you do not already have a copy you will not be able to build this pack, and the games
will use their ordinary flat weapon sprites instead. Everything else works normally.

**The Duke3D Voxel Pack** — `duke3d_voxels.zip` from
[its release page](https://github.com/NightFright2k19/duke3d_voxelpack/releases) —
turns Duke's pickups, props and monsters into voxels and adds map lighting fixes. Drop
the zip in and the Duke launchers use it. Its art is non-commercial and share-alike, so
it is deliberately not bundled here.

## Controls

Right-handed default. Everything is rebindable in Options → Customize Controls.

| Control | Action |
|---|---|
| Dominant trigger | Fire |
| Off-hand trigger | Alt fire |
| A | Jump |
| B | Open / Use |
| X | Crouch |
| Y | Toggle map |
| Right stick click | Crouch |
| **Left stick click** | **Alt Weapon** |
| Right stick up/down | Next / previous weapon |
| Dominant thumbrest | Quick kick |

Alt Weapon matters in Shadow Warrior — it re-selects the weapon already in your hand,
which is how you reach the quad shotgun and the nuke. In Duke and Blood it picks the
alternate weapon in a shared slot.

## Known issues

- Some weapons stay as flat sprites because VRaze ships no model for them: Shadow
  Warrior's fists and sword, Exhumed's sword and mummified hands, Duke's mighty foot,
  Redneck's crowbar and bowling ball. That is their data, not a fault here.
- Shadow Warrior's akimbo uzis, quad shotgun and nuke are only reachable if Alt Weapon
  is bound — it is by default, to the off-hand stick click.
- Exhumed and Redneck have no weapon animation frames in VRaze's data, so their weapons
  do not cycle models.
- Alien World Order needs World Tour's loose script and sound files, which are separate
  from `DUKE3D.GRP`. Without them the episode does not appear.
- VRaze's own Duke "knee" model cannot be read by Raze and logs an error at startup.
  Harmless, and it happens in VRaze too.
- Turn speed is degrees per rendered frame, so it is faster on a 120 Hz headset than a
  90 Hz one. Options → VR Options → Turning Mode.

## Licence

GPL-2.0, inherited from Raze — see `package/common/gpl-2.0.txt`. The additions made by
this port are under the same licence. The OpenXR SDK headers in `vr/OpenXR-SDK/` are
Khronos', under `Apache-2.0 OR MIT`.

No game data, and no third-party art, is contained in this repository.

## Building

Windows, Visual Studio 2019 or newer, CMake. Build the default target — that also
regenerates `raze.pk3`, which is needed whenever anything under `wadsrc/` changes.

`PROGRESS.md` is the long-form record of how the port was made and why each decision
went the way it did.
