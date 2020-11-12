/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This file is auto-generated from keymaps.csv on 2018-12-22 15:48
 * Database checksum sha256(ef8f29f4e4294479e2789aa61e410c4b0464d4f0ad16bcc1526086a4f123bc10)
 * To re-generate, run:
 *   keymap-gen --lang=stdc++ --varname=qemu_input_map_win32_to_qcode code-map keymaps.csv win32 qcode
*/
#include <array>
#include "hid.h"
extern const std::array<QKeyCode, 252> qemu_input_map_win32_to_qcode;
