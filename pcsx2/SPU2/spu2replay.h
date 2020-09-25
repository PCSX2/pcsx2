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

#pragma once

//#define S2R_ENABLE

// s2r dumping
int s2r_open(u32 ticks, char* filename);
void s2r_readreg(u32 ticks, u32 addr);
void s2r_writereg(u32 ticks, u32 addr, s16 value);
void s2r_writedma4(u32 ticks, u16* data, u32 len);
void s2r_writedma7(u32 ticks, u16* data, u32 len);
void s2r_close();

extern bool replay_mode;
