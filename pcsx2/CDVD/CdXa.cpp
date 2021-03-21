#include "PrecompiledHeader.h"
#include "SPU2/Global.h"
#include "SPU2/Mixer.h"
#include "IopCommon.h"
#include "CdXa.h"

#include <algorithm>
#include <array>

// No$ calls this pos_xa_adpcm_table. This is also the other neg one as well
static const s8 tbl_XA_Factor[16][2] = {
	{0, 0},
	{60, 0},
	{115, -52},
	{98, -55},
	{122, -60} };

void DecodeChuncka(ADPCM_Decode* decp, u8 filter, u8* blk, u16* samples, s16 last_sampleL, s16 last_sampleR)
{
	for (u32 block = 0; block < cdr.Xa.nbits; block++)
	{
		const u16 header = filter;
		const u8 filterID = header >> 4 & 0xF;

		u8 filterPos = filter;
		u8 filterNeg = tbl_XA_Factor[filterID][1];

		u8* wordP = blk + 16; // After 16 byts or the "28 Word Data Bytes"

		for (int i = 0; i < 28; i++)
		{

			u32 word_data; // The Data words are 32bit little endian
			std::memcpy(&word_data, &wordP[i * sizeof(u32)], sizeof(word_data));

			u16 nibble = nibble = ((word_data >> 4) & 0xF); // I'm looking for the octect value of the binary
			const u8 shift = 12 - (blk[4 + block * 2 + nibble]);

			s16 data = (*blk << 12) >> shift; // shift left 12 shift right by calculated value to get 16 bit sample data out. nibble messes the equation up right now.
			s32 pcm = data + (((filterPos * last_sampleL) + (filterNeg * last_sampleR) + 32) >> 6);

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
	for (u32 block = 0; block < 18; block++)
	{
		if (header->Stereo())
		{
			//DecodeChunck(&decoded.left, 0, xaData, data, cdr.Xa.left);
			//DecodeChunck(&decoded.right, 1, xaData, data, cdr.Xa.right);
		}
		else
		{
			// Mono sound
			//DecodeChunck(&decoded.left, xaData, data, cdr.Xa.Left);
			//DecodeChunck(&decoded.left, xaData, data, cdr.Xa.Left);
		}
	}
}
