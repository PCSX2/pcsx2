/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "IopCommon.h"
#include "PrecompiledHeader.h"

enum Forms {FORM1, FORM2};

// Taken from pcsx
struct xa_subheader
{
	u8 filenum;
	u8 channum;
	// Used to detect if video, audio or general data. Used to detect what form. Might be better to use then numbits
	u8 submode;
	u8 coding;

	u8 filenum2;
	u8 channum2;
	u8 submode2;
	u8 coding2;

	u32 Samplerate() { return (coding & (1 << 2)) ? 18900 : 37800; }
	bool Stereo() { return (coding & 1) ? false : true; }
	u8 Bits() { return (coding & (1 << 4)) ? 8 : 4; }
};

struct ADPCM_Decode
{
	s32 y0, y1;
};

struct xa_decode
{
	// Sample rate which can be anywhere from 44100 to 37800 to 18900
	s32 freq;
	s32 nbits;
	s32 stereo;
	s32 nsamples;
	ADPCM_Decode left, right;
	s16 pcm[2][16384];
};


s16 zigZagInterpolation(int channel, const s16 *Table);
void DecodeADPCM(xa_subheader* header, xa_decode* decoded, u8* xaData);
void DecodeChunck(const u8 block_header, xa_subheader* header, const u16* samples, ADPCM_Decode* decp, int channel, s16 *dest);
