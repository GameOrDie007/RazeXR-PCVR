# RazeXR PCVR

Seven Build engine games in PC VR: **Duke Nukem 3D**, **Blood**, **Shadow Warrior**,
**Redneck Rampage**, **NAM**, **WWII GI** and **Exhumed/PowerSlave** — plus their
expansions - eighteen entries in all, including World Tour's Alien World Order.

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
- **The Build voxel modders** — **Ermac**, **fgsfds** and others whose names are
  not recorded anywhere we could find. Around forty of the weapon models VRaze
  collected are theirs; this port ships none of them, and builds them on your
  machine only if you already have VRaze. If one is yours, please open an issue.
- **Voxel Duke 3D** — Daniel Peterson ("Cheello"), included with his permission.
- **Duke3D Voxel Pack** — ReaperMan and the Duke4.net community.
- The original developers: 3D Realms, Monolith, Lobotomy Software, Xatrix, TNT Team.

## Which games, and where each one comes from

Setup finds these on your PC. You need to own them; nothing here supplies game data.

Almost every expansion **ships inside its base game** - there is nothing separate to buy,
and nothing to hunt for on GOG. That trips people up, so it is spelled out.

| Game | Where it comes from |
|---|---|
| **Duke Nukem 3D** — 1.3D, Atomic, Plutonium or World Tour | any Duke release |
| Duke it out in D.C. | in the Duke bundles (`addons\dc`) |
| Duke Caribbean: Life's a Beach | in the Duke bundles (`addons\vacation`) |
| Duke: Nuclear Winter | in the Duke bundles (`addons\nw`) |
| Duke!ZONE II | in Megaton Edition |
| Duke: Alien World Order | **World Tour** — episode five, built by setup |
| **Blood** — One Unit Whole Blood | the classic release, *not* Fresh Supply |
| BLOOD: Cryptic Passage | in One Unit Whole Blood |
| **Shadow Warrior** | Classic or Classic Redux both work |
| Shadow Warrior: Wanton Destruction | in Shadow Warrior |
| Shadow Warrior: Twin Dragon | in Shadow Warrior |
| **Redneck Rampage** | GOG's Redneck Rampage Collection |
| Redneck Rampage Rides Again | in the same collection |
| Redneck Rampage: Suckin' Grits on Route 66 | in the same collection |
| **NAM** | its own release |
| **WWII GI** | its own release |
| Platoon Leader | **in WWII GI** — never sold separately |
| **Exhumed / PowerSlave** | the original DOS release only — see below |

Raze also recognises **NAPALM** (a NAM variant) and **Duke Nukem's Penthouse Paradise**,
both obscure enough that most people will never see them, and the shareware and demo
versions of Duke and Shadow Warrior.

### Exhumed / PowerSlave needs the DOS original

The 2022 **PowerSlave Exhumed** from Nightdive - the one currently sold on Steam and GOG -
**will not work**. It is a rewrite on the KEX engine with its own repacked assets: no
`STUFF.DAT`, and its maps are in Nightdive's own format rather than Build's. There is
nothing inside it Raze can read and nothing that can be converted. Checked, not assumed.

What Raze needs is the original DOS **Powerslave** or **Exhumed**, identified by a
`STUFF.DAT` of about 27 MB. That release was sold digitally before the remaster replaced
it, so if you bought it back then it is still in your library - look for a separate
classic entry, and check the remaster's Extras or bonus downloads, which sometimes carry
the original. Otherwise a disc.

Setup says so when it finds the remaster, rather than only reporting the game as missing.

## What this adds over RazeXR

- Windows / desktop OpenGL instead of Android and GLES
- A desktop mirror, so the monitor shows what the headset sees
- Voxel weapons held at the controller, across all seven games
- Smooth turn by default, and Alt Weapon bound to the off-hand stick click
- Per-game launcher scripts and a self-contained portable layout
- A `vrweapons` console command that prints the resolved weapon table
- `MAXVOXELS` raised from 1024 to 2048, so the community voxel packs fit

## Installing

**Run `SETUP.bat`. That is the whole thing.**

It finds the Build games you already own on Steam and GOG, copies their data into
this folder, downloads the optional voxel pack, and writes a launcher for each
game it ends up with. No prompts and no arguments.

Afterwards the folder is self-contained: copy it to another PC and it runs there
with nothing installed and no setup to repeat.

    SETUP                 the normal way
    SETUP -InPlace        link to the games where they are instead of copying,
                          if disk space matters more than being able to move this
                          folder elsewhere
    SETUP -NoDownload     skip the network step
    SETUP -Root D:\Games   also search this folder

If a game is missing afterwards, setup did not find it — re-run with `-Root`
pointing at where it lives.

## Requirements

You need your own copies of the games. Nothing here contains game data.

- A PCVR headset and an OpenXR runtime (Virtual Desktop's VDXR, SteamVR or Oculus)
- The game data for whichever games you want — `DUKE3D.GRP`, `BLOOD.RFF`, `SW.GRP`,
  `REDNECK.GRP`, `NAM.GRP`, `WW2GI.GRP`, `STUFF.DAT` and so on
- `libsndfile-1.dll` beside the executable, or Ogg music will be silent while MIDI
  games play normally

## Optional extras

None of these ship with this port and none are ours to distribute. Each is picked up
automatically if the file is beside the executable.

**Voxel weapons in your hands** work out of the box for Duke, Blood and Shadow
Warrior. Setup builds `vrweapons.pk3` for you, because the models are not ours to
ship and do not need to be: Blood's and Shadow Warrior's weapons are the pickup
voxels those games already contain, taken from your own `BLOOD.RFF` and `SW.GRP`,
and Duke's come from a voxel pack setup downloads. Thirty-five weapons, no extra
downloads, nothing of anyone else's redistributed.

The remaining four games - Exhumed, NAM, Redneck Rampage and WWII GI - use models
that are neither the games' own nor ours, along with the animation frames that make
a weapon cycle. Those come only from a VRaze install, whose **public downloads have
been withdrawn**. If you have a copy, point setup at it:

    SETUP.bat -VRaze "path\to\VRaze\raze.pk3"

Without it those four games use their ordinary flat weapon sprites, and the three
above stay still rather than animating. Everything else works normally.

**Voxel monsters and props** — two packs, and the launcher takes whichever is present,
preferring the first:

- **Voxel Duke 3D** by Daniel Peterson ("Cheello") — 1,024 voxels including the Pig
  Cops, Troopers, Enforcers and Octabrains. **Included with the author's permission**,
  so there is nothing to download. Its own readme says it does not work with Raze —
  that was Raze's voxel ceiling, which this port raises, and it runs correctly here.
  Original at [ModDB](https://www.moddb.com/mods/voxel-duke-nukem-3d).
- **Duke3D Voxel Pack** by ReaperMan and the Duke4.net community — props, pickups,
  switches and signs, but not the humanoid enemies. Setup downloads this one
  automatically. Its art is non-commercial and share-alike, so it is not bundled.

Both of those are Duke only. **Blood, Shadow Warrior and Exhumed have packs of their
own** - props, scenery, decals and some enemies - by fgsfds, Dzierzan and contributors,
which setup fetches from their authors' own repositories. Same non-commercial
share-alike licence, so they are downloaded rather than bundled, and each keeps its own
`license.txt`.

These are props and monsters, which is a different thing from the weapon models above:
a game can have voxel scenery and still hold a flat weapon, and Exhumed, NAM, Redneck
and WWII GI do exactly that without a VRaze install.

Redneck Rampage, NAM and WWII GI have no voxel pack that we know of.

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

- Some weapons stay as flat sprites because no voxel model exists for them anywhere:
  Shadow Warrior's fists and sword, Exhumed's sword and mummified hands, Duke's mighty
  foot, Redneck's crowbar and bowling ball, Blood's pitchfork.
- Without a VRaze install, Exhumed, NAM, Redneck and WWII GI have no weapon models at
  all and use flat sprites throughout. See Optional extras.
- Shadow Warrior's akimbo uzis, quad shotgun and nuke are only reachable if Alt Weapon
  is bound — it is by default, to the off-hand stick click.
- Weapon animation frames - Duke's pistol slide, Blood's napalm launcher and Shadow
  Warrior's railgun - are Domyoji's own models and come only with a VRaze install.
  Without one those weapons appear, but hold a single pose. Exhumed and Redneck have
  no animation frames in VRaze's data either way.
- Alien World Order - World Tour's fifth episode - needs a **Duke Nukem 3D: 20th
  Anniversary World Tour** install for setup to build it from; its scripts, maps and
  voice-overs are loose files, not part of `DUKE3D.GRP`. With one, setup adds them and
  the World Tour launcher runs all five episodes. `TILES009.ART` is deliberately not
  copied, because its name collides with art the other Duke games load, so any World
  Tour art inside it is absent.
- Voxel Duke 3D covers the first three episodes. Its author left the episode-four set
  out because it is unfinished, so The Birth uses sprites.
- Turning voxels off in Options → Display Options also turns off the weapons in your
  hands, since those are voxels too.
- Turn speed is degrees per rendered frame, so it is faster on a 120 Hz headset than a
  90 Hz one. Options → VR Options → Turning Mode.

## Licence

GPL-2.0, inherited from Raze — see `package/common/gpl-2.0.txt`. The additions made by
this port are under the same licence. The OpenXR SDK headers in `vr/OpenXR-SDK/` are
Khronos', under `Apache-2.0 OR MIT`.

No game data is contained in this repository. Third-party content shipped with the
release, and the basis for each, is listed in `THIRD-PARTY-PERMISSIONS.md`.

## Building

Windows, Visual Studio 2019 or newer, CMake. Build the default target — that also
regenerates `raze.pk3`, which is needed whenever anything under `wadsrc/` changes.

`PROGRESS.md` is the long-form record of how the port was made and why each decision
went the way it did.
