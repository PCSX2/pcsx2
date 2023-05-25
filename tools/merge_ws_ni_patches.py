#!/usr/bin/env python3

# PCSX2 - PS2 Emulator for PCs
# Copyright (C) 2023 PCSX2 Dev Team
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

# pylint: disable=bare-except, disable=missing-function-docstring

import glob
import os
import sys

def merge_patches(srcdir, dstdir, label, desc, extralines=None):
    for file in glob.glob(os.path.join(srcdir, "*.pnach")):
        print(f"Reading {file}...")

        name = os.path.basename(file)
        with open(file, "rb") as f:
            lines = f.read().decode().strip().split("\n")

        gametitle_line = None
        comment_line = None
        for line in lines:
            line = line.strip()
            if line.startswith("gametitle=") and gametitle_line is None:
                gametitle_line = line
            elif line.startswith("comment=") and comment_line is None:
                comment_line = line[8:]

        # ignore gametitle if file already exists
        outname = os.path.join(dstdir, name)
        if os.path.exists(outname):
            gametitle_line = None

        with open(outname, "ab") as f:
            if gametitle_line is not None:
                f.write((gametitle_line + "\n\n").encode())

            f.write(f"[{label}]\n".encode())
            if desc is not None and comment_line is None:
                f.write(f"description={desc}\n".encode())
            if extralines is not None:
                f.write(f"{extralines}\n".encode())
            for line in lines:
                line = line.strip()
                if not line.startswith("gametitle="):
                    f.write((line + "\n").encode())
            f.write("\n\n".encode())

        print(f"Wrote/updated {outname}")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <ws directory> <ni directory> <output directory>")
        sys.exit(1)

    outdir = sys.argv[3]
    if not os.path.isdir(outdir):
        os.mkdir(outdir)

    merge_patches(sys.argv[1], outdir, "Widescreen 16:9", "Renders the game in 16:9 aspect ratio, instead of 4:3.", "gsaspectratio=16:9")
    merge_patches(sys.argv[2], outdir, "No-Interlacing", "Attempts to disable interlaced offset rendering.", "gsinterlacemode=1")
