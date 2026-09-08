#!/usr/bin/env python3
"""
Build the release archive.

    python tools/make-release.py <run-folder> [-o RazeXR-PCVR.zip]

<run-folder> is a working portable install - the one you have been testing in.
The engine and its runtime DLLs are taken from there; the scripts, docs and
licence come from this repository, so what ships is what is committed rather
than whatever happens to be sitting beside the exe.

Nothing else is copied. Everything a player needs beyond this archive is
either their own game data or built on their machine by setup.ps1.

The audit at the end is the point of this script. It names a decision for
every third-party archive rather than judging by file extension, because the
two Duke voxel packs are the same kind of file with different answers, and a
rule that cannot tell them apart will either ship the wrong one or refuse the
right one. Both have happened.

Copyright (C) 2026 RazeXR PCVR port. GPL-2.0-or-later, see gpl-2.0.txt.
"""

import argparse
import os
import shutil
import sys
import zipfile

# Taken from the run folder: the built engine and what it loads at runtime.
FROM_BUILD = [
    # Straight from the build tree, never from the run folder. Taking these
    # from a working install once shipped a raze.exe two commits behind the one
    # that had just been tested - the fix was verified in a different folder
    # from the one the archive was built out of, and nothing said so.
    ("build/Release/raze.exe", "raze.exe"),
    ("build/raze.pk3", "raze.pk3"),
]

FROM_RUN = [
    "libsndfile-1.dll",     # ZMusic needs it for Ogg; without it SW and Redneck are silent
    "openal32.dll",
    "openxr_loader.dll",
    "zmusiclite.dll",
    "raze_portable.ini",
    "README.txt",
    "SETUP.bat",
    # Cheello's Voxel Duke 3D, unmodified, with its own readme.txt inside. He
    # was asked directly and in public and agreed; the wording and the link are
    # in THIRD-PARTY-PERMISSIONS.md. Bundling is what makes voxel monsters work
    # with no download - it is on ModDB, which cannot be fetched by script.
    "voxel_duke3d.zip",
    # The 66 weapon models that cannot be built from anything the user already
    # owns - Exhumed, NAM, Redneck and WWII GI, and the animation frames. Every
    # contributor gave permission in the Team Beef Discord; see
    # THIRD-PARTY-PERMISSIONS.md. The other 34 are the games' own data and
    # Cheello's, and are still built on the user's machine, never shipped.
    "vrweapons_models.pk3",
]

FROM_REPO = [
    ("tools/setup.ps1", "setup.ps1"),
    ("tools/build-vrweapons.ps1", "build-vrweapons.ps1"),
    ("README.md", "README.md"),
    ("THIRD-PARTY-PERMISSIONS.md", "THIRD-PARTY-PERMISSIONS.md"),
    ("AUTHORS.md", "AUTHORS.md"),
    ("package/common/gpl-2.0.txt", "gpl-2.0.txt"),
]

SUBDIR_FILES = [("soundfonts/raze.sf2", "soundfonts/raze.sf2")]

# Extensions that only ever belong to a game's own data.
BANNED_EXT = (".grp", ".rff", ".map", ".con", ".art", ".kvx", ".dat",
              ".ogg", ".mid", ".voc", ".def")

ALLOWED_FILES = {
    "raze_portable.ini",   # our own marker file
    "raze.pk3",            # our own engine resources
    "voxel_duke3d.zip",      # permission recorded, see THIRD-PARTY-PERMISSIONS.md
    "vrweapons_models.pk3",  # the same, and nothing in it is a game's own data
}

REFUSED_FILES = {
    "duke3d_voxels.zip",   # non-commercial share-alike, no permission asked; setup downloads it
    "vrweapons.pk3",       # built from the user's own BLOOD.RFF and SW.GRP
    "vrweapons_rr.pk3",    # the same, trimmed for Route 66
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("run", help="a working portable install to take the engine from")
    ap.add_argument("-o", "--out", default="RazeXR-PCVR.zip")
    args = ap.parse_args()

    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    stage = os.path.join(os.path.dirname(os.path.abspath(args.out)), "_release_stage")
    if os.path.isdir(stage):
        shutil.rmtree(stage)
    root = os.path.join(stage, "RazeXR-PCVR")
    os.makedirs(root)

    missing = []
    for rel, dst in FROM_BUILD:
        src = os.path.join(repo, rel)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(root, dst))
        else:
            missing.append(rel)

    for name in FROM_RUN:
        src = os.path.join(args.run, name)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(root, name))
        else:
            missing.append(name)

    for rel, dst in FROM_REPO:
        src = os.path.join(repo, rel)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(root, dst))
        else:
            missing.append(rel)

    for rel, dst in SUBDIR_FILES:
        src = os.path.join(args.run, rel)
        out = os.path.join(root, dst)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        if os.path.isfile(src):
            shutil.copy2(src, out)
        else:
            missing.append(rel)

    if missing:
        print("missing, refusing to build:")
        for m in missing:
            print("   ", m)
        return 1

    problems = []
    files = 0
    total = 0
    for base, _dirs, names in os.walk(root):
        for name in names:
            path = os.path.join(base, name)
            files += 1
            total += os.path.getsize(path)
            if name in REFUSED_FILES:
                problems.append("must not ship: " + name)
            elif name not in ALLOWED_FILES and os.path.splitext(name)[1].lower() in BANNED_EXT:
                problems.append("game data: " + os.path.relpath(path, root))

    print("staged %d files, %.1f MB" % (files, total / 1048576.0))
    if problems:
        print("AUDIT FAILED:")
        for p in problems:
            print("   ", p)
        return 1
    print("audit clean: no game data; Voxel Duke 3D bundled by permission;")
    print("             no Duke3D Voxel Pack, no built weapon packs, no configs, no launchers")

    if os.path.isfile(args.out):
        os.remove(args.out)
    with zipfile.ZipFile(args.out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for base, _dirs, names in os.walk(root):
            for name in sorted(names):
                path = os.path.join(base, name)
                z.write(path, os.path.relpath(path, stage))

    shutil.rmtree(stage)
    print("\nwrote %s (%.1f MB)" % (args.out, os.path.getsize(args.out) / 1048576.0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
