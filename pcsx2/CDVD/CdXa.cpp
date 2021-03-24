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

void DecodeChunck(u8* block_header, u8* xaData, u8* samples, ADPCM_Decode* decp, int nbits)
{
	for (u32 block = 0; block < cdr.Xa.nbits; block++)
	{
		u8* wordP = samples; // After 16 byts or the "28 Word Data Bytes"

		for (int i = 0; i < 28; i++)
		{

			u32 word_data; // The Data words are 32bit little endian
			std::memcpy(&word_data, &wordP[i * sizeof(u32)], sizeof(word_data));

			u16 nibble = nibble = ((word_data >> 4) & 0xF); // I'm looking for the octect value of the binary
			u8 shift = 12 - (*xaData & 0x0f);
			s16 data = (*xaData << 12) >> shift; // shift left 12 shift right by calculated value to get 16 bit sample data out. nibble messes the equation up right now.

			u8 filter = (*block_header & 0x04) >> 4;
			u8 filterID = *block_header >> 4 & 0xF;

			Console.Warning("Shift: %02d", shift);
			Console.Warning("Filter: %02d", filter);

			u8 filterPos = tbl_XA_Factor[filterID][0];
			u8 filterNeg = tbl_XA_Factor[filterID][1];

			s32 pcm = data + (((filterPos * decp->y0) + (filterNeg * decp->y1) + 32) >> 6); // decp needs be replaced with "old"

			Clampify(pcm, -0x8000, 0x7fff);
			*(samples++) = pcm;

			s32 pcm2 = data + (((filterPos * pcm) + (filterNeg * last_sampleL) + 32) >> 6);

			Clampify(pcm2, -0x8000, 0x7fff);
			*(samples++) = pcm2;

			decp->y0 = pcm;
			decp->y1 = pcm2;
		}
	}
}

static void DecodeBlock(s16* buffer, int pass, const s16* block, ADPCM_Decode prev)
{
	const s32 header = *block;
	const s32 shift = (header & 0xF) + 16; // TODO: cap at shift 9
	const int id = header >> 4 & 0xF;
	if (id > 4 && MsgToConsole())
		ConLog("* SPU2: Unknown ADPCM coefficients table id %d\n", id);

	const s32 pred1 = tbl_XA_Factor[id][0];
	const s32 pred2 = tbl_XA_Factor[id][1];

	const s8* blockbytes = (s8*)&block[1];
	const s8* blockend = &blockbytes[13];

	for (u32 dst = pass; blockbytes <= blockend; ++blockbytes, dst += 2)
	{
		s32 nibble = *blockbytes >> (pass * 4);
		s32 data = (nibble << 28) & 0xF0000000;
		s32 pcm = (data >> shift) + (((pred1 * prev.y0) + (pred2 * prev.y1) + 32) >> 6);

		Clampify(pcm, -0x8000, 0x7fff);
		buffer[dst] = pcm;

		prev.y1 = prev.y0;
		prev.y0 = pcm;
	}
}


void DecodeADPCM(xa_subheader* header, u8* xaData)
{
	u8* block_header, * sound_datap, * sound_datap2;
	u8 filterPos;
	int i, j, k, nbits;
	u16 data[4096];

	nbits = (header->submode >> 5) & 0x1;

	for (j = 0; j < 18; j++)
	{
		block_header = xaData + j * 128;
		// 16 bytes after header
		sound_datap = block_header + 16;

		if (header->Stereo())
		{
			DecodeChunck(block_header, xaData, sound_datap, &decoded->left, nbits); // Note we access the positive table first
			DecodeChunck(block_header, xaData, sound_datap, &decoded->right, nbits);
		}
		else
		{
			// Mono sound
			DecodeChunck(block_header, xaData, sound_datap, &decoded->left, nbits); // Note we access the positive table first
		}
	}
}
