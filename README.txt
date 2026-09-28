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
    Dominant stick click    Weapon wheel (hold)
    A                       Jump
    B                       Open / Use
    X                       Crouch
    Y                       Toggle Map
    Dominant stick up/down  Next / previous weapon
    Menu button             Pause menu

Alt Weapon matters in Shadow Warrior: it re-selects the weapon already in your
hand, which is how you reach the akimbo uzis, the quad shotgun and the nuke.
In Duke and Blood it picks the alternate weapon in a shared slot.

Turning is smooth by default, at 120 degrees a second on "Medium" whatever
your headset's refresh rate. Slow, Fast, Very Fast and snap turning are under
Options - VR Options - Turning Mode.


Hold the grip for a second set of controls
------------------------------------------

The grip button on your gun hand works like a shift key. Hold it and the same
buttons and stick do something else. This is where inventory lives, so if you
have ever wondered how to take the steroids, this is it.

    Grip + dominant stick down   Next inventory item
    Grip + dominant stick up     Previous inventory item
    Grip + A                     Use the selected item
    Grip + trigger               Quick kick   (Duke, NAM, Redneck)
    Grip + off-hand A            Fly down    (jetpack, or swimming)
    Grip + off-hand B            Fly up
    Grip + off-hand stick click  Land

Without the grip held, the dominant stick up and down changes weapon instead.
Nothing else on the controller changes, so you can hold the grip, flick to what
you want, press A, and let go.

There is no button that takes steroids directly. Select them first, then use
them - the same as every other item.


The weapon wheel
----------------

Hold your gun hand's stick click and the weapons you are carrying hang in a
ring in front of you, as the same 3D models you hold. Move your hand towards
one - it lights up and grows - and let go of the stick to switch to it. Let go
with your hand still in the middle and nothing changes.

Time slows while the wheel is open, so choosing is not a gamble. Options - VR
Options - Wheel Slow Motion sets how much: 0.3 is a third of normal speed, 1 is
no slow motion.

Crouch is on X. It used to be on the stick click as well; the wheel took that
button. Turn the wheel off - Options - VR Options - Weapon Wheel - and the
stick click crouches again.

A weapon only shows on the wheel if it has a VR model. The rare one without is
still reached with the stick up and down.


Valve Index controllers
-----------------------

The Index has no menu button, so the Index gets a layout of its own:

    Left trackpad press     Pause menu
    Right trackpad press    Quick kick
    Left A / left B         X / Y
    Right A / right B       A / B

Press, not touch - resting a thumb on a trackpad does nothing. Everything else
matches the table above.


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

Voxel Weapons, under Options - VR Options, is separate from the Voxels setting
under Display. Voxels there is the world: enemies, pickups, decoration. Voxel
Weapons is only the one in your hand. You can turn the world's voxels off and
keep the weapon a model, which is what Duke's expansions want - their enemies
carry Christmas hats and Hawaiian shirts, and Duke It Out In D.C. has weapon
skins of its own, none of which survive being replaced by a voxel.

Turn Voxel Weapons off and the flat weapon sprite comes back.


The expansions
--------------

The official add-ons are recognised on sight. Put the file in the game's own
folder under games\ , beside the base game's data, and run SETUP.bat again. Each
one then appears in the Switch Game menu with a launcher of its own.

    Duke it out in D.C.            DUKEDC.GRP     into  games\duke\
    Duke Caribbean: Life's a Beach VACATION.GRP   into  games\duke\
    Duke Nuclear Winter            NWINTER.GRP    into  games\duke\
    Duke!ZONE II                   DUKE!ZON.GRP   into  games\duke\
    Blood: Cryptic Passage         cryptic.zip    into  games\blood\
    Shadow Warrior expansions      already in the Classic Redux release
    Redneck Rampage expansions     already in the GOG release

Loose in the folder, not in a subfolder, and keep the name. If your copy of a
Duke add-on also has a loose .CON beside it - DUKEDC.CON, VACATION.CON,
NWINTER.CON - bring that across too. Some releases keep the script outside the
GRP and will not start without it.

You need the base game first. Every add-on is tied to it, and on its own it is
not a game Raze can identify.

If you are working from an original CD, the disc may not hold a .GRP at all.
Sunstorm's Duke add-ons shipped their data as .SSI - Duke it out in D.C. is
DUKEDCPP.SSI in a DUKEDC folder. Copy that in and run SETUP.bat; Raze can read
.SSI directly and it may simply be recognised. If it is not, the usual add-on
patch utility converts it to a .GRP, and that goes in as above.


Fan campaigns
-------------

Death Wish and MARROW, Blood's two big fan episodes, are recognised the same way
the official add-ons are.

Own Blood: Refreshed Supply? Then there is nothing to do: it includes both, and
SETUP.bat brings them across with the rest of Blood. If you have more than one
Blood installed, setup picks the one that has them. Death Wish arrived in
Refreshed Supply's patch 3.0, so if only MARROW appears, update the game and run
SETUP.bat again.

Otherwise get them free from ModDB, put the .zip in  games\blood\  and run
SETUP.bat again.

Hand Raze the zip, and do not unpack it into the Blood folder. Both ship
tiles007.art and tiles008.art, which are names Blood's own art already uses, so
unpacking overwrites the base game's art - Death Wish's own readme warns about
this. Mounted as an archive the replacement art applies only to the episode you
are playing.

The archive has to be flat: the .INI and the maps at the top of the zip, not
inside a folder. The downloads from ModDB wrap everything in a folder, so open
the zip and move the contents up a level before copying it in.

Both bring their own soundtracks as music tracks, and their launchers switch
those on. Nothing to set.

Anything else with its own .INI runs too, but is not recognised - there is no
rule that knows about it, and nothing is wrong when it does not appear. Point a
launcher at it instead. Unpack it into its own folder, say
games\blood\addons\yourmod\ , then copy any launcher in launchers\ to a new
name and change two lines. The second line is the name the Switch Game menu
shows. The raze.exe line gains the folder and the .INI:

    rem Blood: Your Mod

    "%ROOT%\raze.exe" -nosetup -portable -gamegrp "%GRP%" %VRW% %ART% ^
        -j "%ROOT%\games\blood\addons\yourmod" -ini YOURMOD.INI ...

-j adds a folder to the ones Raze searches. -ini names the script to run in
place of BLOOD.INI; what it is called is whatever is inside the mod's own
archive, so open it and look.

Keep the third line, the one saying the file was written by vrwritelaunchers,
or the Switch Game menu will not list it. That line is how the menu tells a
game launcher from any other .bat you keep in the folder.


If something goes wrong
-----------------------

logs\ holds one log per game plus startup.log, which records each startup stage
in order. Between them they say how far it got. razexr_vr.log, beside raze.exe,
records what your headset and runtime reported - include it with any report.

The game stops responding when I alt-tab
    Fixed in 1.1. On a monitor the game rightly stops drawing when its window
    loses focus; in a headset that stopped the VR picture too, and SteamVR
    read it as a hang. It now keeps drawing to the headset. The switch is
    Options - VR Options - Troubleshooting - Keep Running When Alt-Tabbed,
    on by default.

Double vision, or crossed eyes, on a Pimax
    1.1 tells the headset where each eye's picture was drawn, which should
    fix it; it also makes menu text a little sharper on a Quest. It is on by
    default: Options - VR Options - Troubleshooting - Fix Double Vision
    (Pimax). Seen under Pimax Play; the sboys3 SteamVR driver does not show
    it. If the world swims or shears with it on, turn it off and please say
    so, with razexr_vr.log - it records exactly what your headset reports
    per eye.

Changing the render resolution
    Set it in SteamVR (Settings - Video - Per-application video settings) or
    in Pimax Play. The port renders at whatever size the runtime asks for.

Walking goes where I look, and I want it to go where I point
    Options - VR Options - Direction Mode - Off-hand controller.

I want walking to ignore both my head and my hand
    Options - VR Options - Direction Mode - Off. The stick then walks the way
    your body faces in the game, which only turning with the stick changes.
    Looking around and pointing leave it alone. Recenter if forward drifts.

Redneck Rampage has no music unless you have its soundtrack. GOG ships it in a
separate free bonus download that comes with the game rather than inside it.
Leave that zip in your Downloads folder and run SETUP.bat again, or unpack it
into games\rampage\ - setup prints the exact path when it cannot find it.

PowerSlave / Exhumed must be the DOS release. The 2022 Nightdive remaster will
not work. Take Steam's free soundtrack DLC with it, or the game has no music.

Blood must be One Unit Whole Blood, or a collection containing it.


Licence and credit
------------------

GPL-2.0, inherited from Raze. Licence text in gpl-2.0.txt, and Ken
Silverman's BUILD engine terms in buildlic.txt - Raze descends from BUILD
and those terms travel with it.

The four DLLs beside the executable are unmodified upstream builds of
ZMusic, OpenAL Soft, libsndfile and the OpenXR loader. Their licences and
where to get their source are listed in THIRD-PARTY-PERMISSIONS.md.

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
