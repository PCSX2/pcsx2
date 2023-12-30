#!/usr/bin/env python3

import sys
import os
import glob
import re
import functools

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

# pylint: disable=bare-except, disable=missing-function-docstring

src_dirs = [os.path.join(os.path.dirname(__file__), "..", "pcsx2"), os.path.join(os.path.dirname(__file__), "..", "pcsx2-qt")]
fa_file = os.path.join(os.path.dirname(__file__), "..", "3rdparty", "include", "IconsFontAwesome5.h")
pf_file = os.path.join(os.path.dirname(__file__), "..", "3rdparty", "include", "IconsPromptFont.h")
dst_file = os.path.join(os.path.dirname(__file__), "..", "pcsx2", "ImGui", "ImGuiManager.cpp")


all_source_files = list(functools.reduce(lambda prev, src_dir: prev + glob.glob(os.path.join(src_dir, "**", "*.cpp"), recursive=True) + \
    glob.glob(os.path.join(src_dir, "**", "*.h"), recursive=True) + \
    glob.glob(os.path.join(src_dir, "**", "*.inl"), recursive=True), src_dirs, []))

tokens = set()
pf_tokens = set()
for filename in all_source_files:
    data = None
    with open(filename, "r") as f:
        try:
            data = f.read()
        except:
            continue

    tokens = tokens.union(set(re.findall("(ICON_FA_[a-zA-Z0-9_]+)", data)))
    pf_tokens = pf_tokens.union(set(re.findall("(ICON_PF_[a-zA-Z0-9_]+)", data)))

print("{}/{} tokens found.".format(len(tokens), len(pf_tokens)))
if len(tokens) == 0 and len(pf_tokens) == 0:
    sys.exit(0)

u8_encodings = {}
with open(fa_file, "r") as f:
    for line in f.readlines():
        match = re.match("#define (ICON_FA_[^ ]+) \"([^\"]+)\"", line)
        if match is None:
            continue
        u8_encodings[match[1]] = bytes.fromhex(match[2].replace("\\x", ""))
with open(pf_file, "r") as f:
    for line in f.readlines():
        match = re.match("#define (ICON_PF_[^ ]+) \"([^\"]+)\"", line)
        if match is None:
            continue
        u8_encodings[match[1]] = bytes.fromhex(match[2].replace("\\x", ""))

out_pattern = "(static constexpr ImWchar range_fa\[\] = \{)[0-9A-Z_a-z, \n]+(\};)"
out_pf_pattern = "(static constexpr ImWchar range_pf\[\] = \{)[0-9A-Z_a-z, \n]+(\};)"

def get_pairs(tokens):
    codepoints = list()
    for token in tokens:
        u8_bytes = u8_encodings[token]
        u8 = str(u8_bytes, "utf-8")
        u16 = u8.encode("utf-16le")
        if len(u16) > 2:
            raise ValueError("{} {} too long".format(u8_bytes, token))

        codepoint = int.from_bytes(u16, byteorder="little", signed=False)
        codepoints.append(codepoint)
    codepoints.sort()
    codepoints.append(0) # null terminator

    startc = codepoints[0]
    endc = None
    pairs = [startc]
    for codepoint in codepoints:
        if endc is not None and (endc + 1) != codepoint:
            pairs.append(endc)
            pairs.append(codepoint)
            startc = codepoint
            endc = codepoint
        else:
            endc = codepoint
    pairs.append(endc)

    pairs_str = ",".join(list(map("0x{:x}".format, pairs)))
    return pairs_str

with open(dst_file, "r") as f:
    original = f.read()
    updated = re.sub(out_pattern, "\\1 " + get_pairs(tokens) + " \\2", original)
    updated = re.sub(out_pf_pattern, "\\1 " + get_pairs(pf_tokens) + " \\2", updated)
    if original != updated:
        with open(dst_file, "w") as f:
            f.write(updated)
            print("Updated {}".format(dst_file))
    else:
        print("Skipping updating {}".format(dst_file))