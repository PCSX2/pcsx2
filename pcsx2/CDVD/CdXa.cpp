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

#include "PrecompiledHeader.h"
#include "IopCommon.h"
#include "CdXa.h"

#include <algorithm>
#include <array>

// No$ calls this pos_xa_adpcm_table. This is also the other neg one as well XA ADPCM only has 4 values on either side where SPU has 5
static const s8 tbl_XA_Factor[16][2] = {
	{0, 0},
	{60, 0},
	{115, -52},
	{98, -55} };

#define CLAMP(_X_,_MI_,_MA_)	{if(_X_<_MI_)_X_=_MI_;if(_X_>_MA_)_X_=_MA_;}

int pos[2];
static s16 ringBuf[2][32];

void DecodeChunck(u8* block_header, xa_subheader* header, const u8* samples, ADPCM_Decode* decp, int channel, std::vector<s16>& dest)
{
	// Extract 4 or 8 bit nibble depending on BPS
	int bps = header->Bits() ? 8 : 4;

	//Console.Warning("Bps: %01d", bps);
	u32 sampleData;
	u32 nibble;

	for (u32 block = 0; block < bps; block++)
	{
		u8 shift = 12 - (block_header[4 + block] & 0xF);
		u8 filter = (block_header[4 + block * 2] & 0x30) >> 4;
		u8 filterPos = tbl_XA_Factor[filter][0];
		u8 filterNeg = tbl_XA_Factor[filter][1];

		s32 sam;

		for (int i = 0; i < 28; i++)
		{
			// Note, the interleave changes based on SampleRate, Stenznek mentioned some games like rugrats
			// handle this interleave incorrectly and will spam the buffer with too much data.
			// We must crash and clear the buffer for audio to continue?
			sampleData = (u32)(samples[0] & 0x0f);
			sampleData |= ((u32)(samples[4] & 0x0f) << 4);
			sampleData |= ((u32)(samples[8] & 0x0f) << 8);
			sampleData |= ((u32)(samples[12] & 0x0f) << 12);

			if (bps == 4)
			{
				nibble = ((sampleData >> (block * bps)) & 0x0F);
			}
			if (header->Samplerate() == 37800 && bps == 8)
			{
				nibble = ((sampleData >> (block * bps)) & 0xFF);
			}
			sam = static_cast<s32>((nibble << 12) >> shift);

			//Console.Warning("Data: %02x", sampleData);
			//Console.Warning("Nibble: %02x", nibble);

			// Equation taken from Mednafen
			sam += ((decp->y0 * filterPos) >> 6) + ((decp->y1 * filterNeg) >> 6);

			CLAMP(sam, -0x8000, 0x7FFF);
			//Console.Warning("Sample: %02x", sam);

			dest.push_back(static_cast<s16>(sam));

			decp->y1 = decp->y0;
			decp->y0 = sam;

			ringBuf[channel][pos[channel] & 0xF] = sam;

			//Console.Warning("Previous Sample: %02x", decp->y1);
		}
	}
}

s16 zigZagInterpolation(int channel, const s16* Table)
{
	s16 interpolatedSample = 0;

	// Angry and confused doggo noises. NO$ says this array starts at 1
	for (int i = 1; i < 29; i++)
	{
		interpolatedSample += (ringBuf[channel][(pos[channel] - i) & 0x1F] * Table[i]) / 0x8000;
	}

	pos[channel] += 1; // increment position

	CLAMP(interpolatedSample, -0x8000, 0x7FFF);
	return interpolatedSample;
}

void DecodeADPCM(xa_subheader* header, xa_decode* decoded, u8* xaData)
{
	u8* block_header, * sound_datap, * sound_datap2, * Left, * Right;
	u8 filterPos;
	int i, j, k, nbits;
	u16 data[4096], * datap;

	nbits = header->Bits() == 4 ? 4 : 2;

	// TODO. Extract and mix sample data
	//block_header = xaData + 4;

	// 16 bytes after header
	//sound_datap = block_header + 16;

	for (j = 0; j < 18; j++)
	{
		// 4 bit vs 8 bit sound
		for (int i = 0; i < header->Bits(); i++)
		{
			block_header = xaData + j * 128;		// sound groups header
			sound_datap = block_header + 16;	// sound data just after the header
			datap = data;
			sound_datap2 = sound_datap + i;

			*Right = sound_datap[i + 2];

			if (i % 2)
			{
				i++;
				// Odds are Left positives are Right
				*Left = sound_datap[i];
			}

			if (header->Stereo())
			{
				// Allocate maximum sample size
				//cdr.Xa.pcm[0].reserve(16384);
				//cdr.Xa.pcm[1].reserve(16384);

				DecodeChunck(block_header, header, Left, &decoded->left, 0, cdr.Xa.pcm[0]);
				DecodeChunck(block_header, header, Right, &decoded->right, 1, cdr.Xa.pcm[1]);

				//Console.Warning("Sample L: %02x", cdr.Xa.pcm[0][0]);
				//Console.Warning("Sample R: %02x", cdr.Xa.pcm[1][0]);
			}
			else
			{
				// Mono sound
				//cdr.Xa.pcm[0].reserve(16384);
				DecodeChunck(block_header, header, Left, &decoded->left, 0, cdr.Xa.pcm[0]);
				DecodeChunck(block_header, header, Right, &decoded->left, 1, cdr.Xa.pcm[0]);

				//Console.Warning("Sample M: %02x", cdr.Xa.pcm[0][0]);
			}
		}
		sound_datap++;
	}
}
