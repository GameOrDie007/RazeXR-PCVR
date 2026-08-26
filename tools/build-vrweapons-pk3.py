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


if __name__ == "__main__":
    main()
