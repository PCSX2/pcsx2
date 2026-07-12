#!/usr/bin/env python3

import sys
import os
import glob
import re
import functools

# PCSX2 - PS2 Emulator for PCs
# Copyright (C) 2002-2026 PCSX2 Dev Team
#
# PCSX2 is free software: you can redistribute it and/or modify it under the terms
# of the GNU General Public License as published by the Free Software Found-
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
fa_file = os.path.join(os.path.dirname(__file__), "..", "3rdparty", "include", "IconsFontAwesome.h")
pf_file = os.path.join(os.path.dirname(__file__), "..", "3rdparty", "include", "IconsPromptFont.h")
dst_file = os.path.join(os.path.dirname(__file__), "..", "pcsx2", "ImGui", "ImGuiManager.cpp")


all_source_files = list(functools.reduce(lambda prev, src_dir: prev + glob.glob(os.path.join(src_dir, "**", "*.cpp"), recursive=True) + \
    glob.glob(os.path.join(src_dir, "**", "*.h"), recursive=True) + \
    glob.glob(os.path.join(src_dir, "**", "*.inl"), recursive=True), src_dirs, []))

# All FA tokens are within a Unicode private area
# PF, however, needs to replace predefined Unicode characters
fa_tokens = set()
pf_tokens = set()
for filename in all_source_files:
    data = None
    with open(filename, "r") as f:
        try:
            data = f.read()
        except:
            continue

    fa_tokens = fa_tokens.union(set(re.findall("(ICON_FA_[a-zA-Z0-9_]+)", data)))
    pf_tokens = pf_tokens.union(set(re.findall("(ICON_PF_[a-zA-Z0-9_]+)", data)))

print("{}/{} tokens found.".format(len(fa_tokens), len(pf_tokens)))
if len(pf_tokens) == 0:
    sys.exit(0)

def decode_encoding(value):
    if value.startswith("\\x"):
        return bytes.fromhex(value.replace("\\x", ""))

    if len(value) > 1:
        raise ValueError("Unhandled encoding value {}".format(value))

    return bytes(value, 'utf-8')

u8_encodings_fa = {}
with open(fa_file, "r") as f:
    for line in f.readlines():
        match = re.match("#define (ICON_FA_[^ ]+) \"([^\"]+)\"", line)
        if match is None:
            continue
        u8_encodings_fa[match[1]] = decode_encoding(match[2])
u8_encodings_pf = {}
with open(pf_file, "r") as f:
    for line in f.readlines():
        match = re.match("#define (ICON_PF_[^ ]+) \"([^\"]+)\"", line)
        if match is None:
            continue
        u8_encodings_pf[match[1]] = decode_encoding(match[2])

# PF also uses the Unicode private area, check for conflicts with FA
cf_tokens_all = {}
for pf_token in u8_encodings_pf.keys():
    for fa_token in u8_encodings_fa.keys():
        if u8_encodings_pf[pf_token] == u8_encodings_fa[fa_token]:
            cf_tokens_all[pf_token] = fa_token

cf_tokens_used = []
for token in pf_tokens:
    if token in cf_tokens_all:
        cf_tokens_used.append(token)

print("{} font conflicts found, of which we use {} of them.".format(len(cf_tokens_all), len(cf_tokens_used)))
if len(cf_tokens_used) > 0:
    raise NotImplementedError("A used PF token conflicts with a FA token, generating exclude ranges is not implemented")

out_ex_pattern = r"(static constexpr ImWchar range_exclude_icons\[\] = \{)[0-9A-Z_a-z, \n]+(\};)"

def get_pairs(tokens, limit_pairs=-1):
    codepoints = list()
    for token in tokens:
        u8_bytes = u8_encodings_pf[token]
        u8 = str(u8_bytes, "utf-8")
        u16 = u8.encode("utf-16le")
        if len(u16) > 2:
            raise ValueError("{} {} too long".format(u8_bytes, token))

        codepoint = int.from_bytes(u16, byteorder="little", signed=False)
        if codepoint >= 0xe000 and codepoint <= 0xf8ff:
            continue
        codepoints.append(codepoint)
    codepoints.sort()
    codepoints.append(0) # null terminator

    merge_range = 0
    while True:
        merge_range = merge_range + 1
        startc = codepoints[0]
        endc = None
        pairs = [startc]
        for codepoint in codepoints:
            if endc is not None and (((endc + merge_range) < codepoint) or (codepoint == 0)):
                pairs.append(endc)
                pairs.append(codepoint)
                startc = codepoint
                endc = codepoint
            else:
                endc = codepoint
        pairs.append(endc)

        if limit_pairs == -1 or len(pairs) <= (limit_pairs << 1):
            break

    print("Created {} pairs with a merge range of {}".format(len(pairs) >> 1, merge_range))
    pairs_str = ",".join(list(map("0x{:x}".format, pairs)))
    return pairs_str

with open(dst_file, "r") as f:
    original = f.read()
    # ImGui asserts if more than 32 ranges are provided for exclusion
    # we should also use as few as reasonable for performance reasons
    updated = re.sub(out_ex_pattern, "\\1 " + get_pairs(pf_tokens, 32) + " \\2", original)
    if original != updated:
        with open(dst_file, "w") as f:
            f.write(updated)
            print("Updated {}".format(dst_file))
    else:
        print("Skipping updating {}".format(dst_file))
