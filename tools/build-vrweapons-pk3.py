#!/usr/bin/env python3
"""
Builds vrweapons.pk3 from a VRaze install.

The voxel weapon models and their definitions are VRaze's assets, not ours, and
this repository does not commit game data or binaries. So they are packed into a
separate archive at build time and loaded alongside raze.pk3.

    python tools/build-vrweapons-pk3.py [VRaze raze.pk3] [output.pk3]

Defaults to the VRaze install this project was developed against.
"""

import os
import sys
import zipfile

DEFAULT_SRC = r"E:/Games/Quest Ports/VRaze/raze.pk3"
DEFAULT_OUT = "run/vrweapons.pk3"


def wanted(name):
    """The weapon voxels and their definitions, and nothing else.

    The rest of VRaze's pk3 is either identical to RazeXR's or is VRaze's own
    menu and engine data, which this branch does not use.
    """
    if name.endswith(".kvx") and name.startswith("models/weapons/"):
        return True
    if name.endswith(".def") and "/engine/vr_weapon" in name:
        return True
    return False


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT

    if not os.path.isfile(src):
        sys.exit("VRaze raze.pk3 not found at: %s\nPass its path as the first argument." % src)

    with zipfile.ZipFile(src) as zin:
        names = [n for n in zin.namelist() if wanted(n)]
        if not any(n.endswith(".kvx") for n in names):
            sys.exit("No .kvx models found in %s - is that really a VRaze pk3?" % src)

        outdir = os.path.dirname(out)
        if outdir:
            os.makedirs(outdir, exist_ok=True)

        # Stored rather than deflated: .kvx are small and Raze memory-maps the
        # archive, so there is nothing to gain from compressing them.
        with zipfile.ZipFile(out, "w", zipfile.ZIP_STORED) as zout:
            for n in sorted(names):
                zout.writestr(n, zin.read(n))

    vox = sum(1 for n in names if n.endswith(".kvx"))
    defs = len(names) - vox
    print("%s: %d voxel models, %d definition files" % (out, vox, defs))

    """
    A second, Redneck-only pack.

    Route 66 crashes on startup with the full pack - 0xC0000409, a stack buffer
    overrun, before the first frame - and the trigger is GAME66.CON together
    with the other games' def files. Redneck's own defs are fine, and so are all
    the models, so this pack gives Route 66 the same thirteen weapons without
    tripping it. Its launcher picks this one up.
    """
    rrout = os.path.join(os.path.dirname(out), "vrweapons_rr.pk3") if os.path.dirname(out) else "vrweapons_rr.pk3"
    with zipfile.ZipFile(src) as zin:
        keep = [n for n in names
                if n.startswith("filter/redneck/") or n.endswith(".kvx")]
        with zipfile.ZipFile(rrout, "w", zipfile.ZIP_STORED) as zout:
            for n in sorted(keep):
                zout.writestr(n, zin.read(n))
    rrvox = sum(1 for n in keep if n.endswith(".kvx"))
    print("%s: %d voxel models, %d definition files (Route 66)"
          % (rrout, rrvox, len(keep) - rrvox))


if __name__ == "__main__":
    main()
