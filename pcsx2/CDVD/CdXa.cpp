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

 //============================================
 //===  ADPCM DECODING ROUTINES
 //============================================

#ifndef FIXED
static double K0[4] = {
	0.0,
	0.9375,
	1.796875,
	1.53125
};

static double K1[4] = {
	0.0,
	0.0,
	-0.8125,
	-0.859375
};
#else
static int K0[4] = {
	0.0 * (1 << 10),
	0.9375 * (1 << 10),
	1.796875 * (1 << 10),
	1.53125 * (1 << 10)
};

static int K1[4] = {
	0.0 * (1 << 10),
	0.0 * (1 << 10),
	-0.8125 * (1 << 10),
	-0.859375 * (1 << 10)
};
#endif

#define BLKSIZ 28       /* block size (32 - 4 nibbles) */

//===========================================
void ADPCM_InitDecode(ADPCM_Decode* decp)
{
	decp->y0 = 0;
	decp->y1 = 0;
}

//===========================================
#ifndef FIXED
#define IK0(fid)	((int)((-K0[fid]) * (1<<10)))
#define IK1(fid)	((int)((-K1[fid]) * (1<<10)))
#else
#define IK0(fid)	(-K0[fid])
#define IK1(fid)	(-K1[fid])
#endif

// No$ calls this pos_xa_adpcm_table. This is also the other neg one as well XA ADPCM only has 4 values on either side where SPU has 5
static const s8 tbl_XA_Factor[16][2] = {
	{0, 0},
	{60, 0},
	{115, -52},
	{98, -55} };

#define CLAMP(_X_,_MI_,_MA_)	{if(_X_<_MI_)_X_=_MI_;if(_X_>_MA_)_X_=_MA_;}

int pos[2];
static s16 ringBuf[2][32];

// The below is mostly correct. This is not recieving the right amount of sample data at once!
void DecodeChunck(const u8 block_header, xa_subheader* header, const std::vector<u16> samples, ADPCM_Decode* decp, int channel, s16* dest)
{
	// Extract 4 or 8 bit nibble depending on BPS
	int bps = header->Bits() ? 8 : 4;

	u8 filterid = (block_header >> 4) & 0x0f;
	u8 range = (block_header >> 0) & 0x0f;

	s8 filterPos = tbl_XA_Factor[filterid][0];
	s8 filterNeg = tbl_XA_Factor[filterid][1];

	// 28 Data words within
	const u16 *sector = &samples[0];
	u32 word = 0;

	u32 fy0 = decp->y0;
	u32 fy1 = decp->y1;

	for (u32 block = 0; block < bps; block++)
	{
		for (u32 i = 0; i < 27; i++)
		{
			std::memcpy(&word, &samples[i * sizeof(u32)], sizeof(samples));
			u16 nibble = ((word >> block) & 0x0F);
			u16 sample = (nibble << 12);
			u16 processSample = (sample << range) + (((fy0 * filterPos) + (fy1 * filterNeg) + 32) / 64);
			CLAMP(processSample, -0x8000, 0x7FFF);
			Console.Warning("Proceesed Sample: %02x", processSample);
			*(dest++) = processSample;

			decp->y0 = fy0;
			decp->y1 = fy1;
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

static int headtable[4] = { 0,2,8,10 };

void DecodeADPCM(xa_subheader* subHeader, xa_decode* decoded, u8* xaData)
{
	std::vector<u16> Left, Right;
	u8* header = xaData + 128;
	u8* sectors = header + 16;
	for (int blk = 0; blk < 4; blk++)
	{
		switch (subHeader->Bits())
		{
		case 4:
			for (int i = 0; i < 7; i++, sectors += 16)
			{
				u32 Sector0 = (sectors[0x10 + blk + i * 4 + 0]);
				u32 Sector1 = ((sectors[0x10 + blk + i * 4 + 4] << 4));
				u32 Sector2 = ((sectors[0x10 + blk + i * 4 + 8] << 8));
				u32 Sector3 = ((sectors[0x10 + blk + 1 * 4 + 12] << 12));
				Left.push_back(Sector0);
				Right.push_back(Sector1);
				Left.push_back(Sector2);
				Right.push_back(Sector3);
			}
			break;

		case 8:
			for (int i = 0; i < 4; i++, sectors += 16)
			{
				u16 Sector0 = sectors[0];
				u16 Sector1 = sectors[8];
				u16 Sector2 = sectors[16];
				u16 Sector3 = sectors[24];
				Left.push_back(Sector0);
				Right.push_back(Sector1);
				Left.push_back(Sector2);
				Right.push_back(Sector3);
			}
			break;
		}
		for (int i = 0; i < 11; i++)
		{
			if (subHeader->Stereo())
			{
				DecodeChunck(header[headtable[i] + 0], subHeader, Left, &decoded->left, 0, decoded->pcm[0]);
				DecodeChunck(header[headtable[i] + 1], subHeader, Right, &decoded->right, 1, decoded->pcm[1]);
			}
			else
			{
				DecodeChunck(header[headtable[i] + 0], subHeader, Left, &decoded->left, 0, decoded->pcm[0]);
				DecodeChunck(header[headtable[i] + 1], subHeader, Right, &decoded->left, 0, decoded->pcm[0]);
			}
		}
		sectors += 128;
	}
}
