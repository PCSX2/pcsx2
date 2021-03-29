
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

void DecodeChunck(u8* block_header, xa_subheader* header, const u8* samples, ADPCM_Decode* decp, int channel, std::vector<s16>& dest)
{	
	s32 sam;
	s32 nibble;
	s16 sample;	
	int bps = 0;
	s32 sampleData;

	for (int i = 0; i < sizeof(samples); i++)
	{
		u8 shift = 12 - (block_header[4 + i] & 0xF);

		u8 filter = (block_header[4 + i] & 0x30) >> 4;

		u8 filterPos = tbl_XA_Factor[filter][0];
		u8 filterNeg = tbl_XA_Factor[filter][1];

		for (u32 block = 0; block < header->Bits(); block++)
		{
			// Note, the interleave changes based on SampleRate, Stenznek mentioned some games like rugrats
			// handle this interleave incorrectly and will spam the buffer with too much data.
			// We must crash and clear the buffer for audio to continue?
			sampleData = (u32)(samples[0] & 0x0f);
			sampleData |= ((u32)(samples[4] & 0x0f) << 4);
			sampleData |= ((u32)(samples[8] & 0x0f) << 8);
			sampleData |= ((u32)(samples[12] & 0x0f) << 12);

			if (header->Samplerate() == 37800 && bps == 8)
			{
				nibble = ((sampleData >> (block * bps)) & 0xFF);
			}
			sam = (nibble << 12) >> shift;
			s32 data = samples[block + i * 4] >> (nibble * 4) & 0xf000;
			//Console.Warning("Data: %02x", sampleData);
			//Console.Warning("Nibble: %02x", nibble);
			sample = (sam << shift) + ((decp->y0 * filterPos) + (decp->y1 * filterNeg) + 32) / 64;

			CLAMP(sample, -0x8000, 0x7fff);
			//Console.Warning("Sample: %02x", sample);

			s32 sample = (s32)(sam >> shift) + ((decp->y0 * filterPos + decp->y1 * filterNeg + 32) / 64);

			decp->y1 = decp->y0;
			decp->y0 = static_cast<s32>(sample);
			//Console.Warning("Previous Sample: %02x", decp->y1);
		}
		decp->y0 = sample;
		decp->y1 = decp->y0;
	}
}

void DecodeADPCM(xa_subheader* header, xa_decode* decoded, u8* xaData)
{
	u8 *block_header, *sound_datap, *sound_datap2, *Left, *Right;
	u8 filterPos;
	int i, j, k, nbits;
	u16 data[4096], * datap;

	nbits = header->Bits() == 4 ? 4 : 2;

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

			Console.Warning("Sample L: %02x", cdr.Xa.pcm[0][0]);
			Console.Warning("Sample R: %02x", cdr.Xa.pcm[1][0]);
		}
		else
		{
			// Mono sound
			//cdr.Xa.pcm[0].reserve(16384);
			DecodeChunck(block_header, header, Left, &decoded->left, 0, cdr.Xa.pcm[0]);
			DecodeChunck(block_header, header, Right, &decoded->left, 0, cdr.Xa.pcm[0]);

			Console.Warning("Sample M: %02x", cdr.Xa.pcm[0][0]);
		}
		sound_datap++;
	}
}