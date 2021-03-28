
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

/*********************************************************
 decode_28_nibbles(src,blk,nibble,dst,old,older)
  shift  = 12 - (src[4+blk*2+nibble] AND 0Fh)
  filter =      (src[4+blk*2+nibble] AND 30h) SHR 4
  f0 = pos_xa_adpcm_table[filter]
  f1 = neg_xa_adpcm_table[filter]
  for j=0 to 27
	t = signed4bit((src[16+blk+j*4] SHR (nibble*4)) AND 0Fh)
	s = (t SHL shift) + ((old*f0 + older*f1+32)/64);
	s = MinMax(s,-8000h,+7FFFh)
	halfword[dst]=s, dst=dst+2, older=old, old=s
  next j
***************************************************************/
void DecodeChunck(u8* block_header, xa_subheader* header, u16* samples, ADPCM_Decode* decp, std::vector<s16>& dest)
{
	u16 sample;
	for (int i = 0; i < sizeof(samples); i++)
	{
		u8 shift = 12 - (block_header[4 + i] & 0xF);
		u8 filter = (block_header[4 + i] & 0x30) >> 4;

		decp->y1 = decp->y0;
		decp->y0 = sample;
	}

	for (u32 block = 0; block < header->Bits(); block++)
	{
		u16 unprocessedSample = *samples;

		// Extract 4 or 8 bit nibble depending on BPS
		int bps = header->Bits() ? 8 : 4;

		u16 nibble;
		switch (bps)
		{
		case 4:
			nibble = ((unprocessedSample >> (block * bps)) & 0x0F);
			break;
		case 8:
			nibble = ((unprocessedSample >> (block * bps)) & 0xFF);
			break;
		}

		u8 shift = 12 - (block_header[4 + block * 2 + nibble] & 0xF);
		u8 filter = (block_header[4 + block * 2 + nibble] & 0x30) >> 4;
		u8 filterPos = tbl_XA_Factor[filter][0];
		u8 filterNeg = tbl_XA_Factor[filter][0];

		u32 sam = 0;

		Console.Warning("Shift: %02d", shift);
		Console.Warning("Filter: %02d", filter);
		//Console.Warning("Nibble: %02d", nibble);

		if (header->Bits() == 8)
		{
			sample = (u16)(sam << 8) >> shift; // shift left 8 shift right by calculated value to expand 8bit sample
		}

		else
		{
			sample = (u16)(sam << 12) >> shift; // shift left 12 shift right by calculated value to expand 4bit sample
		}

		u8 filterPos = tbl_XA_Factor[filter][0];
		u8 filterNeg = tbl_XA_Factor[filter][1];

		CLAMP(sample, -0x8000, 0x7fff);
		dest.push_back(sample);

		dest.push_back(sample);

		//Console.Warning("PCM1: %02x", pcm);
	}
}

void DecodeADPCM(xa_subheader* header, xa_decode* decoded, u8* xaData)
{
	u8 *block_header, *sound_datap, *sound_datap2, *Left, *Right;
	u8 filterPos;
	int i, j, k, nbits;
	u16 data[4096], * datap;
	int currentForm;
	// Check if Form 2
	// 5   Form2             (0=Form1/800h-byte data, 1=Form2, 914h-byte data)
	currentForm = (header->submode >> 5) & 0x1;
	nbits = header->Bits() == 4 ? 4 : 2;

	// TODO. Extract and mix sample data

	for (j = 0; j < 18; j++)
	{
		block_header = xaData + j * 128;

		// 16 bytes after header
		sound_datap = block_header + 16;

		// 4 bit vs 8 bit sound
		for (int i = 0; i < header->Bits(); i++)
		{
			datap = data;
			sound_datap2 = sound_datap + i;

			// Actual samples?
			for (k = 0; k < 7; k++, sound_datap2 += 16) {
				*(datap++) = (u16)(sound_datap2[0] & 0x0f) |
					((u16)(sound_datap2[4] & 0x0f) << 4) |
					((u16)(sound_datap2[8] & 0x0f) << 8) |
					((u16)(sound_datap2[12] & 0x0f) << 12);
			}
			if (header->Stereo())
			{
				DecodeChunck(block_header, header, data, &decoded->left, cdr.Xa.pcm[0]);
				DecodeChunck(block_header, header, data, &decoded->right, cdr.Xa.pcm[1]);
			}
			else
			{
				// Mono sound
				DecodeChunck(block_header, header, data, &decoded->left, cdr.Xa.pcm[0]);
				DecodeChunck(block_header, header, data, &decoded->left, cdr.Xa.pcm[0]);
			}
		}
		sound_datap++;
	}
}