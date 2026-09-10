# RazeXR PCVR — first release

Seven Build engine games in PC VR, nineteen entries with their expansions, from the
copies you already own. This is a PC port of [Team Beef](https://www.teambeefvr.com/)'s
**RazeXR**, their Quest build of [Raze](https://github.com/ZDoom/Raze).

Tested with a Quest 3 over Virtual Desktop (VDXR).

## Getting it running

1. Download and extract anywhere.
2. Run **`SETUP.bat`**. No prompts and no arguments — it finds the Build games you
   already own on Steam and GOG, copies their data into the folder, downloads the voxel
   packs, builds the in-hand weapon models from your own game files, and writes a
   launcher for every game it ends up with.
3. Start Virtual Desktop, connect the headset, and run **`Play RazeXR PCVR.bat`**.

It opens the game you played last, and **Switch Game** in the menu moves between all of
them without taking the headset off.

The folder is self-contained afterwards. Copy it to another PC and it runs there with
nothing installed and no setup to repeat.

## What's in it

**Nineteen games.** Duke Nukem 3D Atomic and its expansions (Duke it out in D.C.,
Caribbean, Nuclear Winter, Duke!ZONE II, Penthouse Paradise), World Tour with Alien
World Order as episode five, Blood and Cryptic Passage, Shadow Warrior with Wanton
Destruction and Twin Dragon, Redneck Rampage with Rides Again and Route 66, NAM,
WWII GI with Platoon Leader, and Exhumed/PowerSlave.

**Voxel weapons in your hands — ninety-one models, every game covered.** Thirty-five are
built on your machine from your own game data; sixty-six ship here with their authors'
permission (see `THIRD-PARTY-PERMISSIONS.md`).

**Voxel monsters, props and scenery** for Duke, Blood, Shadow Warrior and Exhumed, from
the community packs. Setup fetches the ones that can't be bundled.

**Switch Game** on the main menu and in the pause menu of every game, so you can move
between all nineteen without leaving VR.

**A pause menu that hangs in the world.** Press the menu button in a level and the menu
is a panel floating in front of you while the game holds still around it — you can look
around it, and your hands and weapon keep tracking. Team Beef's build put the whole
paused frame on a virtual screen instead, which meant the world stopped being a world
the moment you opened a menu. Off-hand stick click hides the panel for a clean
screenshot; any button brings it back. Its distance, size and behaviour are all under
*VR Options*.

**A splash screen** before each game's own intro. `vr_splash 0` turns it off.

**A log per game** in `logs\`, plus `logs\startup.log` recording each startup stage — if
something goes wrong, that pair says where.

## What you need

The games themselves. Nothing here contains game data and setup never downloads any.
`README.md` lists what to buy and which release each game comes from — the two that
catch people out:

- **PowerSlave / Exhumed** must be the **DOS** release. The 2022 Nightdive
  *PowerSlave Exhumed* remaster will not work. Take Steam's free **soundtrack DLC**
  with it, or the game has no music.
- **Blood** must be **One Unit Whole Blood** or a collection containing it. *Blood II*
  is a different engine entirely.

Windows, a headset, and an OpenXR runtime.

## Known limitations

Listed in full under **Known issues** in `README.md`. The short version: a handful of
melee weapons have no voxel model anywhere and stay as sprites; Exhumed's and Redneck's
weapons hold a single pose because no animation frames exist for them; Voxel Duke 3D
does not cover episode four.

## Licence

GPL-2.0, inherited from Raze. The source is this repository. Third-party content and the
permission behind each piece is documented in `THIRD-PARTY-PERMISSIONS.md`.

Credit where it is owed: Raze (Christoph Oelckers, Mitchell Richters and the ZDoom
team), RazeXR (Team Beef), VRaze (Domyoji), the Build voxel modders, and the original
developers — 3D Realms, Monolith, Lobotomy Software, Xatrix and TNT Team.
