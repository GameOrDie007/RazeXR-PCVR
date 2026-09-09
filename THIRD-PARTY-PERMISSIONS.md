# Third-party content and permissions

What this port includes that is not ours, and on what basis. Kept as a record so
the question does not have to be reconstructed later.

Nothing in this repository is game data. See `README.md` for what the user has to
supply themselves.

## Voxel Duke 3D

- **Author:** Daniel Peterson ("Cheello")
- **Version used:** 1.0, released 21 November 2025
- **Source:** https://www.moddb.com/mods/voxel-duke-nukem-3d
- **Basis:** the author was asked directly, in public, and agreed.

  Asked: *"I have a question about Duke Voxel, I made a PCVR port of duke, and
  was wondering, would you be OK with me bundling it with credit?"*

  Answered: *"Sure, go ahead! Thanks for asking!"* — September 2026.
  Public record: https://x.com/gameordie007/status/2095200356858728548

Distributed with the release archive rather than committed here, and credited in
`README.md` and `AUTHORS.md`. The pack itself is unmodified.

Its own readme states that it does not work correctly with Raze. That was Raze's
`MAXVOXELS` ceiling of 1024 — the pack defines 884 with its episode-four set left
off, and enabling that set exceeds the limit. This port raises the ceiling to
2048, and the pack runs correctly. The episode-four set remains off because its
author describes it as unfinished, which is his call.

## Duke3D Voxel Pack

- **Authors:** ReaperMan and the Duke4.net community
- **Source:** https://github.com/NightFright2k19/duke3d_voxelpack
- **Basis:** its `voxelpack_art_license.txt` permits copying and distribution
  with attribution, non-commercial, and share-alike.

**Not bundled.** Non-commercial and share-alike terms do not sit comfortably
inside a GPL archive, and the pack is published at a stable URL, so setup
downloads it onto the user's machine from the authors' own release instead. We
host none of it.

It also supplies Duke's **weapon** models when Cheello's pack is absent, which
for most people is always, since ModDB cannot be fetched by script. Its weapon
pickups are different art on the same voxel grid — checked dimension by
dimension, within a few voxels either way — so VRaze's placements and scales
hold without retuning. Same licence reasoning: downloaded, never redistributed.

## VRaze weapon data

- **Author:** Domyoji
- **Basis:** asked directly and agreed — Team Beef Discord, September 2026, in
  the channel where the RazeXR contributors talk, and every contributor there has
  given permission. Domyoji was careful to say most of it was not his to grant:

  Answered: *"Sure go ahead buddy. You don't actually need my permission since
  nearly all the models aren't mine. They're either from the games themselves
  (weapon pickup voxel models from Blood and Shadow Warrior), or from modders
  like Cheello, Ermac, fgsfds, etc. The full list of who they're from is in the
  Contributors section. The only ones that I actually made/edited are the models
  for the animation frames (duke pistol sliding, blood napalm and sw railgun
  pulsing) and for the unique weapon mode overlays in sw (shotgun/head). But
  you're free to use those too if you want. Go nuts."*

So the permission covers a small subset and the disclaimer covers the rest. The
disclaimer was the more useful half: it said where the other models came from,
which is what the hashing below had been unable to settle, and it is the reason
the weapons now work with no VRaze install at all.

**What ships.** No model, here or in the release archive.
`tools/build-vrweapons.ps1` assembles `vrweapons.pk3` on the user's machine and
setup runs it for them. The definition files do ship — they are text, they are
Domyoji's, and the permission above covers them — inside `raze.pk3` under
`vrweapons/`. That path is inert, because Raze only flattens a leading `filter/`.
The builder copies the ones it has models for to the path that does load, and
drops every line naming a model it could not supply, so a game that gets no
models behaves exactly as it did before any of this existed.

VRaze's engine source was never published, so this port's weapon code could not
be and is not derived from it. It was written against VRaze's *published data
format* — the `.def` files describing voxel tiles, placements and animation
frames — and `source/core/vr_weapons.h` records that at the point it was written.

### Where VRaze's models actually came from

Checked by hashing, September 2026, so it does not have to be guessed at again.
VRaze ships 100 weapon voxels across the seven games. Hashed against every public
Build voxel pack we could find, and — after Domyoji's reply pointed at it — against
the voxels the original games ship in their own `BLOOD.RFF` and `SW.GRP`:

| Game | Models | Identical to shipped game data | Identical to a public pack |
| --- | --- | --- | --- |
| Duke | 14 | 0 | **11** — Cheello's Voxel Duke 3D |
| Blood | 21 | **11** — `BLOOD.RFF` | 0 |
| Shadow Warrior | 27 | **12** — `SW.GRP` | 1 (`vr_mine`, which that pack also copies from the game) |
| Exhumed | 6 | 0 | 0 |
| Redneck / NAM / WWII GI | 32 | 0 | 0, and no pack exists for these games |

Both halves of his account check out byte for byte.

**Blood and Shadow Warrior's weapon voxels are the games' own pickup models.**
`vr_tommygun.kvx` is `TOMMYGUN.KVX`, `vr_uzi.kvx` is `UZI.KVX`, `vr_star.kvx` is
`VOXEL000.KVX`, and so on. VRaze even keeps the untouched copies beside the edited
ones in `models/weapons/sw/originals/`, and all seven of those are bit-identical to
`SW.GRP`. This is game data. Nobody can license it to us and nobody needs to: the
user already owns it, and it can be unpacked from their own installation exactly
the way the rest of their game data is.

**Duke's are Cheello's pickup models**, unmodified — same bytes, same pivots.
`vr_weapon_pistol.kvx` is `21_PISTOL.kvx`, `vr_weapon_shotgun.kvx` is
`28_SHOTGUN.kvx`, and so on for ten distinct models. (VRaze's flamethrower is its
own freeze model reused, which is why 11 files map to 10 models.) Cheello has
given us permission in writing, so Duke's voxel weapons need nothing from VRaze.

**Domyoji's own work is the animation frames and the SW mode overlays**, and these
are exactly the files that match nothing anywhere:

- `blood/vr_napalm0-5.kvx` — the napalm launcher pulsing
- `sw/vr_rail0-4.kvx` — the railgun pulsing
- `duke/vr_weapon_pistol0-2.kvx` — the pistol slide
- `sw/vr_shotgun_quad.kvx`, and his edited `sw/vr_hothead.kvx` — the unique
  weapon mode overlays he names

These are covered by the permission above.

**Roughly 40 models have no known author** — Exhumed's 6, Redneck's 12, NAM's
10, WWII GI's 10, and Blood's four `vr_pitchfork` files.

Domyoji was asked for the Contributors list he had pointed at. **He does not have
one**: he said he does not know exactly who made them and that they are unknown.
The list is not in the pk3, and no copy of it has been found. The names he could
give are **Ermac** and **fgsfds**, alongside the wider Build voxel modding
community the models came out of. Both are credited in the README.

That is as far as attribution can honestly be taken, and it does not hold up a
release, because **this project redistributes none of these models.** They exist
only where the user has pointed setup at their own VRaze install with `-VRaze`,
and they are assembled on that user's machine from files they already had.
Nothing of anyone else's is in this repository or in any archive published from
it. Permission was given in the same channel and is not the open question;
attribution was, and this is the answer to it.

If you recognise your own work among them, please open an issue — it will be
credited properly and gladly.

### They are bundled now, as `vrweapons_models.pk3`

The owner confirms that every contributor in that Discord gave permission to
bundle, which is the basis for shipping them. VRaze's own downloads have been
withdrawn, so without this the four games whose weapons are nobody's game data —
Exhumed, NAM, Redneck Rampage and WWII GI — have no voxel weapons at all for
anyone who did not already own a copy.

**66 of the 100 models ship. The other 34 do not, and the split is not
negotiable:** Blood's and Shadow Warrior's weapons are those games' own pickup
voxels and Duke's are Cheello's, and no permission from a modder covers
redistributing a game's own data. Those are still taken from the user's own
`BLOOD.RFF` and `SW.GRP` and from Cheello's pack at setup time. The split is
made by hashing, not by trusting a filename — a model that matches something
buildable locally is left out of the archive.

## Other Build voxel packs (not bundled)

Not used at present, and recorded because they are the only fallbacks if VRaze's
models cannot be used. All three carry the same `Voxel Pack Art License` as the
Duke3D Voxel Pack — non-commercial and share-alike — so the same reasoning
applies: they would be downloaded onto the user's machine, never committed here.
Unlike ModDB, all three are on GitHub, so fetching them can be scripted.

- **Blood Voxel Pack** by fgsfds, Dzierzan and contributors —
  https://github.com/fgsfds/Blood-Voxel-Pack
  595 voxels. Weapon coverage is 6 of VRaze's 21: flare gun, voodoo doll,
  tommy gun, dynamite, remote and spray can. The rest of the pack is props,
  decals and animated scenery.
- **Shadow Warrior Voxel Pack** by fgsfds and contributors —
  https://github.com/fgsfds/Shadow-Warrior-Voxel-Pack
  254 voxels, and **no weapons at all** — the gun-related entries are ejected
  shell casings. Nothing here helps Shadow Warrior's 27 models.
- **Powerslave Voxel Pack** by fgsfds and contributors —
  https://github.com/fgsfds/Powerslave-Voxel-Pack
  105 voxels. Weapon coverage is 3 of Exhumed's 6: flamethrower, grenade, staff.

All three would also give their games the prop and monster voxels that Duke
already gets, independently of the weapons question.

## Cover art (boxart.pk3)

The nineteen covers the Switch Game menu shows were made for this port by its
author. Each one takes the game's own cover art and puts a headset on whoever is
on the front, with "VR" worked into the logo.

The underlying cover art belongs to the publisher of each game - 3D Realms,
Monolith, Lobotomy Software, Xatrix, TNT Team and their successors - and no
permission has been asked for or granted. They are included the way a launcher
or a library front end shows a cover: to identify the game a row starts, for
people who already own that game, in a project that is not sold. If a rights
holder would rather their cover were not used, open an issue and it will be
removed - the menu already draws nothing for a game whose cover is missing, so
removing one is deleting a file.

Nothing in `boxart.pk3` is game data. The images are new files, not extracted
from any game, and no game ships its cover inside its own data.

## RazeXR

- **Author:** Team Beef (DrBeef and contributors)
- **Basis:** GPL-2.0, the same licence as Raze. Team Beef were also asked in
  public whether a PCVR port could be released and said there was no issue,
  August 2026, on the condition every derivative keeps to the open source
  obligation — which is why this repository exists.

The VR layer under `vr/RazeXR/` is theirs, imported and adapted for PC.

## OpenXR SDK headers

- **Author:** The Khronos Group
- **Basis:** `Apache-2.0 OR MIT`. MIT is compatible with GPL-2.0.
