
#include "PrecompiledHeader.h"
#include "SPU2/Global.h"
#include "SPU2/Mixer.h"
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


void DecodeChunck(u8* block_header, xa_subheader* header, const u8* samples, ADPCM_Decode* decp, std::vector<s16>& dest)
{
	u16 sample;
	for (int i = 0; i < sizeof(samples); i++)
	{
		u8 shift = 12 - (block_header[4 + i] & 0xF);

		u8 filter = (block_header[4 + i] & 0x30) >> 4;

		s32 data = (u32)(samples[0] & 0xf000) >> shift;
		s32 data1 = (u32)((samples[1] << 4) & 0xf000) >> shift;
		s32 data2 = (u32)((samples[2] << 8) & 0xf000) >> shift;
		s32 data3 = (u32)((samples[3] << 12) & 0xf000) >> shift;

		//u32 nibble = (u32)data[1]; // I'm looking for the octect value of the current sample

		Console.Warning("Shift: %02d", shift);
		Console.Warning("Filter: %02d", filter);
		//Console.Warning("Nibble: %02d", nibble);

		if (header->Bits() == 8)
		{
			sample = (u16)(data << 8) >> shift; // shift left 8 shift right by calculated value to expand 8bit sample
		}

		else
		{
			sample = (u16)(data << 12) >> shift; // shift left 12 shift right by calculated value to expand 4bit sample
		}

		u8 filterPos = tbl_XA_Factor[filter][0];
		u8 filterNeg = tbl_XA_Factor[filter][1];

		data -= (filterPos * decp->y0 + (filterNeg * decp->y1)) >> 10;
		decp->y1 = decp->y0;
		decp->y0 = data;
		data1 -= (filterPos * decp->y0 + (filterNeg * decp->y1)) >> 10;
		decp->y1 = decp->y0;
		decp->y0 = data1;
		data2 -= (filterPos * decp->y0 + (filterPos * decp->y1)) >> 10;
		decp->y1 = decp->y0;
		decp->y0 = data2;
		data3 -= (filterPos * decp->y0 + (filterNeg * decp->y1)) >> 10;
		decp->y1 = decp->y0;
		decp->y0 = data3;

		//s32 pcm = sample + (((decp->y0 * filterPos) + (decp->y1 * filterNeg + 32)) / 64);

		Clampify(data, -0x8000, 0x7fff);
		dest.push_back(data);
		Clampify(data1, -0x8000, 0x7fff);
		dest.push_back(data1);
		Clampify(data2, -0x8000, 0x7fff);
		dest.push_back(data2);
		Clampify(data3, -0x8000, 0x7fff);
		dest.push_back(data3);

		Console.Warning("Samples: %02d", sample);

		//Console.Warning("PCM1: %02x", pcm);
	}
}

void DecodeADPCM(xa_subheader* header, xa_decode* decoded, u8* xaData)
{
	u8 *block_header, *sound_datap, *sound_datap2, *Left, *Right;
	u8 filterPos;
	int i, j, k, nbits;
	u16 data[4096];
	int currentForm;
	// Check if Form 2
	// 5   Form2             (0=Form1/800h-byte data, 1=Form2, 914h-byte data)
	currentForm = (header->submode >> 5) & 0x1;
	nbits = header->Bits() == 4 ? 4 : 2;

	// TODO. Extract and mix sample data

	for (j = 0; j < 18; j++)
	{
		block_header = xaData + j * 128;

		Console.Warning("HEADER: %02x", block_header);

		// 16 bytes after header
		sound_datap = block_header + 16; // This is sample data
		sound_datap2 = sound_datap + nbits;

		if (header->Stereo())
		{
			DecodeChunck(block_header, header, sound_datap, &decoded->left, cdr.Xa.pcm[0]);
			DecodeChunck(block_header, header, sound_datap, &decoded->right, cdr.Xa.pcm[1]);
		}
		else
		{
			// Mono sound
			DecodeChunck(block_header, header, sound_datap, &decoded->left, cdr.Xa.pcm[0]);
			DecodeChunck(block_header, header, sound_datap, &decoded->left, cdr.Xa.pcm[0]);
		}
		sound_datap++;
	}
}