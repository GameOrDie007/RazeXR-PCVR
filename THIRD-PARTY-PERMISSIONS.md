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

## VRaze weapon data

- **Author:** Domyoji
- **Basis:** none sought, and none needed — nothing of theirs is included.

VRaze's engine source was never published, so this port's weapon code could not
be and is not derived from it. It was written against VRaze's *published data
format* — the `.def` files describing voxel tiles, placements and animation
frames — and `source/core/vr_weapons.h` records that at the point it was written.

The models themselves are not distributed here. `tools/build-vrweapons-pk3.py`
builds the pack on the user's machine from their own VRaze installation. VRaze's
public downloads have since been withdrawn, so users without an existing copy
simply do not get this feature.

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
