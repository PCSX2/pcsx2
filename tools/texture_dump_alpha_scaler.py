#!/usr/bin/env python3

import sys
import glob
import os
import argparse
from PIL import Image

# PCSX2 - PS2 Emulator for PCs
# Copyright (C) 2002-2022  PCSX2 Dev Team
#
# PCSX2 is free software: you can redistribute it and/or modify it under the terms
# of the GNU Lesser General Public License as published by the Free Software Found-
# ation, either version 3 of the License, or (at your option) any later version.
#
# PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with PCSX2.
# If not, see <http://www.gnu.org/licenses/>.


DESCRIPTION = """Quick script to scale alpha values commonly seen in PCSX2 texture dumps.
This script will scale textures with a maximum alpha intensity of 128 to 255, and then back
again with the unscale command, suitable for use as replacements. Not unscaling after editing
may result in broken rendering!

Example usage:
  python3 texture_dump_alpha_scaler.py scale path/to/serial/dumps

  <edit your images, move to replacements, including the index txt>

  python3 texture_dump_alpha_scaler.py unscale path/to/serial/replacements
"""

# pylint: disable=bare-except, disable=missing-function-docstring


def get_index_path(idir):
    return os.path.join(idir, "__scaled_images__.txt")


def scale_image(path, relpath):
    try:
        img = Image.open(path, "r")
    except:
        return False

    print("Processing '%s'" % relpath)
    if img.mode != "RGBA":
        print("  Skipping because it's not RGBA (%s)" % img.mode)
        return False

    data = img.getdata()
    max_alpha = max(map(lambda p: p[3], data))
    print("  max alpha %u" % max_alpha)
    if max_alpha > 128:
        print("  skipping because of large alpha value")
        return False

    new_pixels = list(map(lambda p: (p[0], p[1], p[2], min(p[3] * 2 - 1, 255)), data))
    img.putdata(new_pixels)
    img.save(path)
    print("  scaled!")
    return True


def unscale_image(path, relpath):
    try:
        img = Image.open(path, "r")
    except:
        return False

    print("Processing '%s'" % relpath)
    if img.mode != "RGBA":
        print("  Skipping because it's not RGBA (%s)" % img.mode)
        return False

    data = img.getdata()
    new_pixels = list(map(lambda p: (p[0], p[1], p[2], max((p[3] + 1) // 2, 0)), data))
    img.putdata(new_pixels)
    img.save(path)
    print("  unscaled!")
    return True


def get_scaled_images(idir):
    try:
        scaled_images = set()
        with open(get_index_path(idir), "r") as ifile:
            for line in ifile.readlines():
                line = line.strip()
                if len(line) == 0:
                    continue
                scaled_images.add(line)
        return scaled_images
    except:
        return set()


def put_scaled_images(idir, scaled_images):
    if len(scaled_images) > 0:
        with open(get_index_path(idir), "w") as ifile:
            ifile.writelines(map(lambda s: s + "\n", scaled_images))
    elif os.path.exists(get_index_path(idir)):
        os.remove(get_index_path(idir))


def scale_images(idir, force):
    scaled_images = get_scaled_images(idir)

    for path in glob.glob(idir + "/**", recursive=True):
        relpath = os.path.relpath(path, idir)
        if not path.endswith(".png"):
            continue

        if relpath in scaled_images and not force:
            continue

        if not scale_image(path, relpath):
            continue

        scaled_images.add(relpath)

    put_scaled_images(idir, scaled_images)


def unscale_images(idir, force):
    scaled_images = get_scaled_images(idir)
    if force:
        for path in glob.glob(idir + "/**", recursive=True):
            relpath = os.path.relpath(path, idir)
            if not path.endswith(".png"):
                continue
            scaled_images.add(relpath)

    for relpath in list(scaled_images):
        if unscale_image(os.path.join(idir, relpath), relpath):
            scaled_images.remove(relpath)
    put_scaled_images(idir, scaled_images)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=DESCRIPTION,
            formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("command", type=str,
            help="Command, should be scale or unscale")
    parser.add_argument("directory", type=str,
            help="Directory containing images, searched recursively")
    parser.add_argument("--force",
            help="Scale images regardless of whether it's in the index",
            action="store_true", required=False)
    args = parser.parse_args()
    if args.command == "scale":
        scale_images(args.directory, args.force)
        sys.exit(0)
    elif args.command == "unscale":
        unscale_images(args.directory, args.force)
        sys.exit(0)
    else:
        print("Unknown command, should be scale or unscale")
        sys.exit(1)
