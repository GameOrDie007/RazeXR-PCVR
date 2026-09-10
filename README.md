# RazeXR PCVR

Seven Build engine games in PC VR: **Duke Nukem 3D**, **Blood**, **Shadow Warrior**,
**Redneck Rampage**, **NAM**, **WWII GI** and **Exhumed/PowerSlave** — plus their
expansions - nineteen entries in all, including World Tour's Alien World Order.

This is a PC port of [Team Beef](https://www.teambeef.org/)'s **RazeXR**, their Quest VR
build of [Raze](https://github.com/ZDoom/Raze). RazeXR runs on Android and OpenXR; this
moves it to Windows, desktop OpenGL and PCVR, and adds voxel weapons held in your hand - ninety-one of them, every game covered.

Tested with a Quest 3 over Virtual Desktop (VDXR).

## Credits

This exists because of other people's work.

The PC VR port itself is by **Game Or Die** ([github.com/GameOrDie007](https://github.com/GameOrDie007)).

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
  collected are theirs, and this port **does ship them**, in `vrweapons_models.pk3`,
  with permission given in the Team Beef Discord — see `THIRD-PARTY-PERMISSIONS.md`.
  If one is yours and you would rather it were not included, please open an issue
  and it will be removed.
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
| **Exhumed / PowerSlave** | the original DOS release, plus its free soundtrack DLC — see below |

Raze also recognises **NAPALM** (a NAM variant) and **Duke Nukem's Penthouse Paradise**,
both obscure enough that most people will never see them, and the shareware and demo
versions of Duke and Shadow Warrior.

### Exhumed / PowerSlave needs the DOS original

The 2022 **PowerSlave Exhumed** from Nightdive - the one currently sold on Steam and GOG -
**will not work**. It is a rewrite on the KEX engine with its own repacked assets: no
`STUFF.DAT`, and its maps are in Nightdive's own format rather than Build's. There is
nothing inside it Raze can read and nothing that can be converted. Checked, not assumed.

What Raze needs is the original DOS **Powerslave** or **Exhumed**, identified by a
`STUFF.DAT` of about 27 MB.

Steam sells it separately as
**[PowerSlave (DOS Classic Edition)](https://store.steampowered.com/app/1260020/PowerSlave_DOS_Classic_Edition/)**,
which is the DOS build rather than the remaster. **That is the one to buy, and it is
confirmed working** - its `STUFF.DAT` is byte for byte the same 27,020,745 bytes as the
GOG DOS release.

Take the free
**[soundtrack DLC](https://store.steampowered.com/app/1722540/PowerSlave_DOS_Classic_Edition_Soundtrack/)**
with it. PowerSlave's music was on the disc as CD audio and is in none of the game's own
files, so without the DLC the game runs silent - and the GOG DOS release has no way to
get it at all. The DLC installs the tracks beside the game data, setup brings them
across, and the game scores itself.

If you bought the classic version years ago, before the remaster replaced it, it is
still in your library under its own entry. Otherwise a disc.

Setup says so when it finds the remaster, rather than only reporting the game as missing.

## What this adds over RazeXR

- Windows / desktop OpenGL instead of Android and GLES
- A desktop mirror, so the monitor shows what the headset sees
- Voxel weapons held at the controller, across all seven games
- Smooth turn by default, and Alt Weapon bound to the off-hand stick click
- Cover art for every game in the Switch Game menu, made for this port
- One launcher that opens the game you played last, and Switch Game in the menu
- A self-contained portable layout - copy the folder to another PC and it runs
- A `vrweapons` console command that prints the resolved weapon table
- `MAXVOXELS` raised from 1024 to 2048, so the community voxel packs fit

## Installing

**Run `SETUP.bat`. That is the whole thing.**

It finds the Build games you already own on Steam and GOG, copies their data into
this folder, downloads the optional voxel pack, and writes a launcher for each
game it ends up with. No prompts and no arguments.

Then start Virtual Desktop, connect the headset, and run:

    Play RazeXR PCVR.bat

It opens whichever game you played last, and **Switch Game** in the menu moves
between all of them without taking the headset off. To start one game directly,
the individual scripts are in `launchers\`.

    Play RazeXR PCVR.bat   the one you want
    launchers\             a script per game, for a direct shortcut
    games\                 your game data, copied here by setup
    assets\                the voxel packs, weapon models and cover art
    config\                one settings file per game
    logs\                  one log per game - this is what to send if something breaks

Afterwards the folder is self-contained: copy it to another PC and it runs there
with nothing installed and no setup to repeat. The exception is `-InPlace`, which
points at your existing installs rather than copying them in: that saves the disk
space and gives up the portability.

    SETUP                 the normal way
    SETUP -InPlace        link to the games where they are instead of copying
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

**Voxel weapons in your hands** work out of the box for every game, ninety-one
models in all. Setup assembles `vrweapons.pk3` from two places:

- **Thirty-five come off your own disk.** Blood's and Shadow Warrior's weapons are
  the pickup voxels those games already contain, read out of your own `BLOOD.RFF`
  and `SW.GRP`; Duke's are Cheello's, out of the voxel pack. None of these ship
  here and none need to - no permission covers redistributing a game's own data,
  and there is nothing to redistribute when the file is already on your disk.
- **Sixty-six are bundled**, as `vrweapons_models.pk3`. These are the ones that are
  nobody's game data: Exhumed's, NAM's, Redneck's and WWII GI's weapons, and the
  animation frames that make a weapon cycle. They come from VRaze, whose own
  downloads have been withdrawn, and every contributor gave permission - see
  `THIRD-PARTY-PERMISSIONS.md`.

If you have a VRaze install of your own you can still point setup at it, and it
will be used instead:

    SETUP.bat -VRaze "path\to\VRaze\raze.pk3"

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

## Music

Most of these games keep their music inside their own data and it just plays. Three
of them used CD audio instead, so the music is a set of files rather than something
in the GRP - and whether you get it depends on what your copy shipped with:

- **Blood** and **Shadow Warrior** - GOG ships the tracks with the game and setup
  copies them across. Nothing to do.
- **Redneck Rampage** and **Rides Again** - these are CD audio with no MIDI to fall
  back on, so without the tracks they are completely silent. On GOG the soundtrack
  is a **separate bonus download**, not part of the game installer - get it from the
  game's page in your library, and setup will do the rest. You do not have to unpack
  it or put it anywhere in particular: setup looks beside the game data, everywhere it
  searches for games, and in your Downloads, Desktop and Documents, and it will read
  the music straight out of GOG's bonus zip - including the soundtrack zip nested
  inside it. It says so plainly if it cannot find it.
- **PowerSlave / Exhumed** - the music was CD audio and is in none of the game's own
  files: its data holds 648 entries and every one is a sound effect. Steam's free
  **soundtrack DLC** for the DOS Classic Edition installs the eighteen tracks, already
  named the way the engine asks for them, and setup copies them across. Without it the
  game is silent, and the GOG DOS release has no way to get them. If you have the tracks
  from somewhere else, a `music` folder holding `track02.ogg` upward is what to make.

The launchers pass `+set mus_extendedlookup 1`, which lets a request for a `.ogg`
track be answered by the `.mp3` or `.flac` you actually have. If you rip your own
CD, any of those formats will do.

## Known issues

- Some weapons stay as flat sprites because no voxel model exists for them anywhere:
  Shadow Warrior's fists and sword, Exhumed's sword and mummified hands, Duke's mighty
  foot, Redneck's crowbar and bowling ball.
- WWII GI's mauser and Redneck's blaster and thrown dynamite carry model names that
  differ from the ones VRaze's placements are written against (`alienblaster` and
  `throwingdynamite`). This port defines both spellings, using the same standard
  placement every other weapon in those games uses.
- Shadow Warrior's akimbo uzis, quad shotgun and nuke are only reachable if Alt Weapon
  is bound — it is by default, to the off-hand stick click.
- Exhumed and Redneck weapons hold a single pose rather than cycling, because
  VRaze's data has no animation frames for them. The frames that do exist - Duke's
  pistol slide, Blood's napalm launcher, Shadow Warrior's railgun - are bundled and
  work out of the box.
- The pause-menu panel covers about 73 degrees of view at the default size. A flat
  surface pinned in the world genuinely changes shape as you turn to look along it, the
  way a cinema screen does from a side seat, so at that width a little of that is
  visible. **Menu Depth** and **Menu Size** under *VR Options* both reduce it; the menu
  keeps its apparent size as you change the depth.
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

## Playing seated

**Options → VR Options → Recenter Height.** Sit the way you mean to play, select it, and
that height becomes the one the game treats as standing — a chair stops reading as a
permanent crouch. `vr_recenter` at the console does the same. It is a capture rather
than a guessed number, and the **Height Adjust** slider below it shows what was taken.

Each game has its own idea of eye height — Duke and Redneck 40 map units, Blood 60,
Shadow Warrior and Exhumed 58, against their own world scales — so recentring lands
correctly in each without setting it per game.

Named and behaving the same as in the Quake and Quake II PCVR ports.

## Licence

GPL-2.0, inherited from Raze — see `COPYING`. The additions made by
this port are under the same licence. The OpenXR SDK headers in `vr/OpenXR-SDK/` are
Khronos', under `Apache-2.0 OR MIT`.

No game data is contained in this repository. Third-party content shipped with the
release, and the basis for each, is listed in `THIRD-PARTY-PERMISSIONS.md`.

## Building

Windows, Visual Studio 2019 or newer, CMake. Build the default target — that also
regenerates `raze.pk3`, which is needed whenever anything under `wadsrc/` changes.

`PROGRESS.md` is the long-form record of how the port was made and why each decision
went the way it did.
