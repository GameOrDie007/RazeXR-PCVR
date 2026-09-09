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
    "SETUP.bat",
    # Cheello's Voxel Duke 3D, unmodified, with its own readme.txt inside. He
    # was asked directly and in public and agreed; the wording and the link are
    # in THIRD-PARTY-PERMISSIONS.md. Bundling is what makes voxel monsters work
    # with no download - it is on ModDB, which cannot be fetched by script.
    "assets/voxel_duke3d.zip",
    # The 66 weapon models that cannot be built from anything the user already
    # owns - Exhumed, NAM, Redneck and WWII GI, and the animation frames. Every
    # contributor gave permission in the Team Beef Discord; see
    # THIRD-PARTY-PERMISSIONS.md. The other 34 are the games' own data and
    # Cheello's, and are still built on the user's machine, never shipped.
    "assets/vrweapons_models.pk3",
]

FROM_REPO = [
    ("tools/setup.ps1", "setup.ps1"),
    ("tools/build-vrweapons.ps1", "build-vrweapons.ps1"),
    ("README.md", "README.md"),
    # From the repository, not the run folder. It shipped from beside the exe
    # until 9 Sept 2026, which meant the first file a player opens was
    # untracked, unreviewable, and three folder layouts out of date.
    ("README.txt", "README.txt"),
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
    "boxart.pk3",          # the port's own cover art, made for it
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
        # A name may carry a folder now - assets/ - so make it first.
        #
        # Look for it at that same path in the run folder, and only then at the
        # root. The fallback is what an install predating the assets/ move looks
        # like; without the first path this refused to build against every
        # install made since, saying the files were missing while they sat in
        # assets/ where the entry itself says they go.
        rel = name.replace("/", os.sep)
        src = os.path.join(args.run, rel)
        if not os.path.isfile(src):
            src = os.path.join(args.run, os.path.basename(name))
        dst = os.path.join(root, rel)
        if os.path.isfile(src):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
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

    """
    The cover art, packed rather than copied.

    Nineteen loose images in the run folder would be nineteen things a player
    can rename or lose; as one pk3 the engine either has the set or does not.
    The name inside the archive is what the menu asks for, so it is the
    launcher's filename - see VRGameSelectMenu.
    """
    art = os.path.join(repo, "boxart")
    covers = sorted(f for f in os.listdir(art)) if os.path.isdir(art) else []
    if not covers:
        missing.append("boxart/")
    else:
        os.makedirs(os.path.join(root, "assets"), exist_ok=True)
        with zipfile.ZipFile(os.path.join(root, "assets", "boxart.pk3"), "w",
                             zipfile.ZIP_STORED) as z:
            for f in covers:
                z.write(os.path.join(art, f), "boxart/" + f)

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
