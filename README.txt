RazeXR PCVR
===========

Nineteen Build engine games in PC VR, from the copies you already own. This is
a PC port of Team Beef's RazeXR, their Quest build of Raze, with the voxel
weapons from VRaze: ninety-one models, every game covered.

Tested on a Meta Quest 3 over Virtual Desktop. Valve Index and Pimax layouts
are supported but not tested by us.


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
the folder, and no Visual C++ redistributable is needed; the runtime is
linked statically.

To update, extract a new release over the top. Updating keeps your saves and
settings: your configs, saves, launchers and game data are not in the archive
and are left alone.


FAQ
---

Is it free?
    Yes.

Do I need my own copies of the games?
    Yes. Steam, GOG and original CD releases all work. PowerSlave needs the
    DOS Classic Edition, not the 2022 remaster.

Which headsets work?
    Any PC VR headset with an OpenXR runtime: Virtual Desktop, SteamVR (Steam
    Link included) or the Meta Quest Link app. Tested on a Meta Quest 3 over
    Virtual Desktop. The Valve Index and Pimax layouts were made from players'
    reports and are supported but not tested by us.

Does it run standalone on a Quest 3 or a Steam Frame?
    Not this version. It runs on your PC, and a Quest 3 or a Steam Frame plays
    it streamed over Virtual Desktop or Steam Link.

Multiplayer?
    No, single player only.

Are my saves safe?
    Yes. Saves, settings, launchers and game data live in this folder and none
    of them are in the release zip, so updating keeps them.

What PC do I need?
    These are 1990s games. A PC that runs VR runs them.

Does it work with mods?
    Blood's Death Wish and MARROW, yes, and the voxel packs are built in. Mods
    made for EDuke32's own scripting, such as Brutal Duke and most big total
    conversions, don't run in Raze, and the 3D models in Duke's High
    Resolution Pack don't show.

Left-handed? Seated?
    Yes to both. VR Options > Hands and Weapons > Leftorium (left-handed) puts
    the gun in your left hand, and VR Options > Your Body > Recenter Height
    makes a chair count as standing.

It started flat, or something broke.
    Start Virtual Desktop (or your VR runtime) and connect the headset before
    you launch. If it still goes wrong, see "If something goes wrong" below.


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
    gametome\         the Game Tome: a book per game, and your notes in them
    soundfonts\       music
    libsndfile-1.dll  Ogg music decoding. Without it Shadow Warrior, Redneck
                      Rampage and anything else with .ogg music plays silently,
                      with no error in the game.

The launchers prefer games\ beside them, and fall back to where the data was
found when they were written, which is why the folder still runs on the
machine that set it up after being moved.


Controls worth knowing
----------------------

Every button is labelled in the picture beside this file:
    Controls - RazeXR PCVR - Meta Quest 3.png

    Dominant trigger        Fire
    Off-hand trigger        Alt fire
    Off-hand stick click    Alt Weapon
    Dominant stick click    Weapon wheel (hold)
    A                       Jump
    B                       Open / Use
    X                       Crouch
    Y                       Toggle Map
    Dominant stick up/down  Next / previous weapon
    Dominant thumbrest      Quick kick   (Duke, NAM, Redneck; touch it)
    Menu button             Pause menu
    Both palms up together  The Game Tome (hold them there a second)

Alt Weapon matters in Shadow Warrior: it re-selects the weapon already in your
hand, which is how you reach the akimbo uzis, the quad shotgun and the nuke.
In Duke and Blood it picks the alternate weapon in a shared slot.

Turning is smooth by default, at 120 degrees a second on "Medium" whatever
your headset's refresh rate. Slow, Fast, Very Fast and snap turning are under
VR Options > Turning and Moving > Turning Mode.

Squeeze both grips together to recenter: where you stand becomes the middle,
the way you face becomes forward, your head height becomes standing height,
and an open menu moves back in front of you. It works in the menus too.
VR Options > Recenter, the first row, does the same.

Left-handed? VR Options > Hands and Weapons > Leftorium (left-handed) puts the
gun in your left hand, drawn as its mirror image, with the trigger, the grip
and the face buttons swapped to match. The sticks stay where they are: the
left stick still walks and the right stick turns. Switch Sticks, under
Turning and Moving, swaps them for anyone who wants that too.

Duck your head and you crouch in the game, so you fit wherever a crouch fits;
stand up and you stand. The view stays with your head. X still crouches too.
VR Options > Your Body > Crouch For Real turns it off.

Lean your head into a wall and the view darkens rather than showing the inside
of the map: the game's body stops at the wall, your head does not, and the
view goes black over the next 15 cm. VR Options > Your Body > Wall Fade.

Paintball turns every mark a shot leaves on a wall into a splat of bright
paint. Modern is wet paint in one bright color a splat; Classic is the old
GoldenEye 007 look, every splat a blend of four colors. Damage, sounds and
blood stay as they were. VR Options > Hands and Weapons > Paintball, off at
the start.

The Game Tome: hold both hands out in front of you, palms up, as if holding an
open book, for a second, and the game's manual opens in your off hand, with
the controls and VR Options at the front and pages for your own notes at the
back. It works in play and in the menus, and the game waits while it is open.
VR Options > Game Tome opens it too. Turn a page by reaching to its edge with
your gun hand, pulling the trigger and drawing it across, or with either
stick. On a notes page your hand holds a pen: touch the paper to write. The PC
keyboard types on the notes too. B, Y or Menu closes it. Each game has its own
book, and what you write stays in gametome\ when you update.


Hold the grip for a second set of controls
------------------------------------------

The grip button on your gun hand works like a shift key. Hold it and the same
buttons and stick do something else. This is where inventory lives, so if you
have ever wondered how to take the steroids, this is it.

    Grip + dominant stick down   Next inventory item
    Grip + dominant stick up     Previous inventory item
    Grip + A                     Use the selected item
    Grip + B                     Turn around
    Grip + dominant stick click  Item wheel (hold)
    Grip + trigger               Quick kick   (Duke, NAM, Redneck)

On a jetpack or in water, Jump (A) goes up and Crouch (X) goes down.

Without the grip held, the dominant stick up and down changes weapon instead.
Nothing else on the controller changes, so you can hold the grip, flick to what
you want, press A, and let go.

There is no button that takes steroids directly. Select them first, then use
them, the same as every other item. Or use the item wheel, below.


The weapon wheel
----------------

Hold your gun hand's stick click and the weapons you are carrying hang in a
ring in front of you, as the same 3D models you hold, on a disc of slices.
Move your hand towards one (its slice lights up in the game's color and the
model grows) and let go of the stick to switch to it. Let go with your hand
still in the middle and nothing changes.

Hold the grip as well and the same ring holds your inventory instead: the
medkit, steroids, jetpack and the rest, as their status bar icons. Let go on
one to use it. An item that switches itself on (Duke's scuba gear and boots,
Blood's diving suit) is not on it. VR Options > Hands and Weapons > Item Wheel turns it
off, and grip + stick click then does nothing, as before.

Time slows while the wheel is open, so choosing is not a gamble. VR Options >
Hands and Weapons > Wheel Slow Motion sets how much: 0.3 is a third of normal
speed, 1 is no slow motion. Wheel Size, on the same page, makes the whole wheel
bigger or smaller.

Crouch is on X. It used to be on the stick click as well; the wheel took that
button. Turn the wheel off (VR Options > Hands and Weapons > Weapon Wheel) and the
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

Press, not touch: resting a thumb on a trackpad does nothing. Everything else
matches the table above.


The pause menu
--------------

Press the menu button in a level and the menu hangs in the world as a panel
while the game holds still around it. You can look around it, and your hands
and weapon keep tracking.

    Off-hand stick click    hide the panel, for a clean screenshot
    Any button              bring it back
    Gun hand's grip, held   point at the menu: a dot shows, the trigger clicks

Pointing works in every menu, the main menu too. The sticks and buttons still
work the menu as well.

Under VR Options > Pause Menu:

    Menu In The World       off puts the menu back on a flat virtual screen
    Menu Stays Where Opened off lets it follow your gaze instead
    Menu Size               how much of your view it covers
    Menu Depth              how far away it hangs, 1 to 10 meters

Menu Depth does not change how big the menu looks; it keeps its apparent size
as it moves. Further away is flatter in your view and easier to read while
turning your head; closer feels more immediate. 3.5 meters is the default.


The observer view
-----------------

The monitor shows what you see, for a stream or a recording: the game, its
menus and the pause menu. In a level it is a steady cut-out of your view that
follows your turns but not the small wobble of your head, with the horizon
kept level when you lean. It fills the monitor with no border; Alt+Enter puts
it in a window and back, and the game remembers which.

VR Options > Screen and Aim > Steady Observer View off shows the plain view.


Playing seated
--------------

VR Options > Your Body > Recenter Height. Sit however you mean to play, select
it, and that height becomes the one the game treats as standing, so a chair
stops reading as a permanent crouch. Same command as the Quake and Quake II
ports: vr_recenter at the console does the same thing.

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

Voxel Weapons, under VR Options > Hands and Weapons, is separate from the Voxels setting
under Display. Voxels there is the world: enemies, pickups, decoration. Voxel
Weapons is only the one in your hand. You can turn the world's voxels off and
keep the weapon a model, which is what Duke's expansions want: their enemies
carry Christmas hats and Hawaiian shirts, and Duke It Out In D.C. has weapon
skins of its own, none of which survive being replaced by a voxel.

Turn Voxel Weapons off and the flat weapon sprite comes back.


The expansions
--------------

The official add-ons are recognized on sight. Put the file in the game's own
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
Duke add-on also has a loose .CON beside it (DUKEDC.CON, VACATION.CON,
NWINTER.CON), so bring that across too. Some releases keep the script outside the
GRP and will not start without it.

You need the base game first. Every add-on is tied to it, and on its own it is
not a game Raze can identify.

If you are working from an original CD, the disc may not hold a .GRP at all.
Sunstorm's Duke add-ons shipped their data as .SSI: Duke it out in D.C. is
DUKEDCPP.SSI in a DUKEDC folder. Copy that in and run SETUP.bat; Raze can read
.SSI directly and it may simply be recognized. If it is not, the usual add-on
patch utility converts it to a .GRP, and that goes in as above.


Fan campaigns
-------------

Death Wish and MARROW, Blood's two big fan episodes, are recognized the same way
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
unpacking overwrites the base game's art, and Death Wish's own readme warns about
this. Mounted as an archive the replacement art applies only to the episode you
are playing.

The archive has to be flat: the .INI and the maps at the top of the zip, not
inside a folder. The downloads from ModDB wrap everything in a folder, so open
the zip and move the contents up a level before copying it in.

Both bring their own soundtracks as music tracks, and their launchers switch
those on. Nothing to set.

Anything else with its own .INI runs too, but is not recognized: there is no
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
records what your headset and runtime reported. Include it with any report.

Each of them keeps the run before as .prev.log beside it (startup.prev.log,
razexr_vr.prev.log, and one per game). Starting the game again after a crash
does not lose the crash's log: send both.

The game stops responding when I alt-tab
    Fixed in 1.1. On a monitor the game rightly stops drawing when its window
    loses focus; in a headset that stopped the VR picture too, and SteamVR
    read it as a hang. It now keeps drawing to the headset. The switch is
    VR Options > Troubleshooting > Keep Running When Alt-Tabbed,
    on by default.

Double vision, or crossed eyes, on a Pimax
    1.1 tells the headset where each eye's picture was drawn, which should
    fix it; it also makes menu text a little sharper on a Quest. It is on by
    default: VR Options > Troubleshooting > Fix Double Vision
    (Pimax). Seen under Pimax Play; the sboys3 SteamVR driver does not show
    it. If the world swims or shears with it on, turn it off and please say
    so, with razexr_vr.log, which records exactly what your headset reports
    per eye.

Changing the render resolution
    Set it in SteamVR (Settings > Video > Per-application video settings) or
    in Pimax Play. The port renders at whatever size the runtime asks for.

Walking goes where I look, and I want it to go where I point
    VR Options > Turning and Moving > Direction Mode > Off-hand controller.

I want walking to ignore both my head and my hand
    VR Options > Turning and Moving > Direction Mode > Off. The stick then walks the way
    your body faces in the game, which only turning with the stick changes.
    Looking around and pointing leave it alone. Recenter if forward drifts.

Redneck Rampage has no music unless you have its soundtrack. GOG ships it in a
separate free bonus download that comes with the game rather than inside it.
Leave that zip in your Downloads folder and run SETUP.bat again, or unpack it
into games\rampage\. Setup prints the exact path when it cannot find it.

PowerSlave / Exhumed must be the DOS release. The 2022 Nightdive remaster will
not work. Take Steam's free soundtrack DLC with it, or the game has no music.

Blood must be One Unit Whole Blood, or a collection containing it.


Get an email when the next port ships: follow Game Or Die on Patreon
for free at https://www.patreon.com/cw/GameOrDie. Every port I've released
is free for everyone.


License and credit
------------------

GPL-2.0, inherited from Raze. License text in gpl-2.0.txt, and Ken
Silverman's BUILD engine terms in buildlic.txt, since Raze descends from BUILD
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
