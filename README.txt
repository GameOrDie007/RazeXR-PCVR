RazeXR - PCVR
=============

Nineteen Build engine games in PC VR, from the copies you already own. This is
a PC port of Team Beef's RazeXR, their Quest build of Raze, with the voxel
weapons from VRaze - ninety-one models, every game covered.

Tested with a Quest 3 over Virtual Desktop.


Install
-------

1. Extract this folder anywhere.

2. Run SETUP.bat and leave it alone. No prompts, no arguments. It finds the
   Build games you own on Steam and GOG, copies their data in, downloads the
   voxel packs, builds the in-hand weapons from your own game files, and writes
   a launcher for every game it ends up with.

3. Start Virtual Desktop and connect the headset BEFORE launching. The game
   looks for an OpenXR session at startup; with no headset streaming it falls
   back to a flat window.

4. Run "Play RazeXR PCVR.bat". It opens the game you played last.

The folder is self-contained afterwards. Copy it to another PC and it runs
there with nothing installed and no setup to repeat. Nothing is written outside
the folder, and no Visual C++ redistributable is needed - the runtime is
linked statically.

To update, extract a new release over the top. Your configs, saves, launchers
and game data are not in the archive and are left alone.


Launching
---------

One .bat per game in launchers\, so they can be added to LaunchBox or pinned
individually. "Play RazeXR PCVR.bat" at the top level opens the last game you
played.

You do not need any of them once you are in VR: Switch Game, on the main menu
and in every game's pause menu, moves between all nineteen without taking the
headset off. It shows the cover art of whichever game is selected.


What lives where
----------------

    raze.exe          the engine
    raze.pk3          engine data, key bindings, the startup splash
    launchers\        one .bat per game, written by setup
    config\           one .ini per game, so settings are per game
    games\            game data, one folder per game
    Save\             savegames, one folder per game
    logs\             one log per game, plus startup.log
    assets\           voxel packs, weapon models, cover art
    soundfonts\       music
    libsndfile-1.dll  Ogg music decoding. Without it Shadow Warrior, Redneck
                      Rampage and anything else with .ogg music plays silently,
                      with no error in the game.

The launchers prefer games\ beside them, and fall back to where the data was
found when they were written - which is why the folder still runs on the
machine that set it up after being moved.


Controls worth knowing
----------------------

    Dominant trigger        Fire
    Off-hand trigger        Alt fire
    Off-hand stick click    Alt Weapon
    Dominant stick click    Crouch
    A                       Jump
    B                       Open / Use
    X                       Crouch
    Y                       Toggle Map
    Dominant stick up/down  Next / previous weapon
    Menu button             Pause menu

Alt Weapon matters in Shadow Warrior: it re-selects the weapon already in your
hand, which is how you reach the akimbo uzis, the quad shotgun and the nuke.
In Duke and Blood it picks the alternate weapon in a shared slot.

Turning is smooth by default. Snap is still there, under Options - VR Options -
Turning Mode.


The pause menu
--------------

Press the menu button in a level and the menu hangs in the world as a panel
while the game holds still around it. You can look around it, and your hands
and weapon keep tracking.

    Off-hand stick click    hide the panel, for a clean screenshot
    Any button              bring it back

Under Options - VR Options:

    Menu In The World       off puts the menu back on a flat virtual screen
    Menu Stays Where Opened off lets it follow your gaze instead
    Menu Size               how much of your view it covers
    Menu Depth              how far away it hangs, 1 to 10 metres

Menu Depth does not change how big the menu looks - it keeps its apparent size
as it moves. Further away is flatter in your view and easier to read while
turning your head; closer feels more immediate. 3.5 metres is the default.


Playing seated
--------------

Options - VR Options - Recenter Height. Sit however you mean to play, select
it, and that height becomes the one the game treats as standing, so a chair
stops reading as a permanent crouch. Same command as the Quake and Quake II
ports - vr_recenter at the console does the same thing.

It is a capture, not a guess: the Height Adjust slider under it shows what was
taken and can be nudged by hand afterwards.


Voxel weapons
-------------

Every game has them. A few weapons stay as flat sprites, and that is correct -
VRaze declares them but ships no model:

    Shadow Warrior    fists, sword
    Exhumed           sword, mummified hands
    Duke, NAM         the melee weapon
    Redneck           crowbar, bowling ball

Type  vrweapons  at the console (~) to see what the running game resolved:
every weapon, its model, and whether it has a placement.


If something goes wrong
-----------------------

logs\ holds one log per game plus startup.log, which records each startup stage
in order. Between them they say how far it got.

Redneck Rampage has no music unless you have its soundtrack. GOG ships it in a
separate free bonus download that comes with the game rather than inside it.
Leave that zip in your Downloads folder and run SETUP.bat again, or unpack it
into games\rampage\ - setup prints the exact path when it cannot find it.

PowerSlave / Exhumed must be the DOS release. The 2022 Nightdive remaster will
not work. Take Steam's free soundtrack DLC with it, or the game has no music.

Blood must be One Unit Whole Blood, or a collection containing it.


Licence and credit
------------------

GPL-2.0, inherited from Raze. Licence text in gpl-2.0.txt.

The complete corresponding source is at
  https://github.com/GameOrDie007/RazeXR-PCVR
That is not a formality: this is a port of Team Beef's work, released with
their agreement on the condition that every derivative stays open, and the
engine under it is GPL. If you were given this folder by someone else, that
link is yours as much as theirs.

Raze by Christoph Oelckers, Mitchell Richters and the ZDoom team. RazeXR by
Team Beef. VRaze by Domyoji. The voxel models by the Build modding community -
every bundled one is here with its author's permission, listed in
THIRD-PARTY-PERMISSIONS.md. The games themselves by 3D Realms, Monolith,
Lobotomy Software, Xatrix and TNT Team.

Known issues are listed in README.md.
