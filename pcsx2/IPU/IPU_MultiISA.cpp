// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-2.0+

// Some of the functions in this file are based on the mpeg2dec library,
//
// Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
// Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
//
// under the GPL license. However, they have been heavily rewritten for PCSX2 usage.
// The original author's copyright statement is included above for completeness sake.

#include "IPU/IPU.h"
#include "IPU/IPUdma.h"
#include "IPU/yuv2rgb.h"
#include "IPU/IPU_MultiISA.h"

// the IPU is fixed to 16 byte strides (128-bit / QWC resolution):
static const uint decoder_stride = 16;

#if MULTI_ISA_COMPILE_ONCE

static constexpr std::array<u8, 1024> make_clip_lut()
{
	std::array<u8, 1024> lut = {};
	for (int i = -384; i < 640; i++)
		lut[i+384] = (i < 0) ? 0 : ((i > 255) ? 255 : i);
	return lut;
}

static constexpr mpeg2_scan_pack make_scan_pack()
{
	constexpr u8 mpeg2_scan_norm[64] = {
		/* Zig-Zag scan pattern */
		0,  1,  8,  16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
	};

	constexpr u8 mpeg2_scan_alt[64] = {
		/* Alternate scan pattern */
		0,  8,  16, 24,  1,  9,  2, 10, 17, 25, 32, 40, 48, 56, 57, 49,
		41, 33, 26, 18,  3, 11,  4, 12, 19, 27, 34, 42, 50, 58, 35, 43,
		51, 59, 20, 28,  5, 13,  6, 14, 21, 29, 36, 44, 52, 60, 37, 45,
		53, 61, 22, 30,  7, 15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63
	};

	mpeg2_scan_pack pack = {};

	for (int i = 0; i < 64; i++) {
		int j = mpeg2_scan_norm[i];
		pack.norm[i] = ((j & 0x36) >> 1) | ((j & 0x09) << 2);
		j = mpeg2_scan_alt[i];
		pack.alt[i] = ((j & 0x36) >> 1) | ((j & 0x09) << 2);
	}

	return pack;
}

alignas(16) const std::array<u8, 1024> g_idct_clip_lut = make_clip_lut();
alignas(16) const mpeg2_scan_pack mpeg2_scan = make_scan_pack();

#endif

MULTI_ISA_UNSHARED_START

static void ipu_csc(macroblock_8& mb8, macroblock_rgb32& rgb32, int sgn);
static void ipu_vq(macroblock_rgb16& rgb16, u8* indx4);

// --------------------------------------------------------------------------------------
//  Buffer reader
// --------------------------------------------------------------------------------------

__ri static u32 UBITS(uint bits)
{
	uint readpos8 = g_BP.BP/8;

	uint result = BigEndian(*(u32*)( (u8*)g_BP.internal_qwc + readpos8 ));
	uint bp7 = (g_BP.BP & 7);
	result <<= bp7;
	result >>= (32 - bits);

	return result;
}

__ri static s32 SBITS(uint bits)
{
	// Read an unaligned 32 bit value and then shift the bits up and then back down.

	uint readpos8 = g_BP.BP/8;

	int result = BigEndian(*(s32*)( (s8*)g_BP.internal_qwc + readpos8 ));
	uint bp7 = (g_BP.BP & 7);
	result <<= bp7;
	result >>= (32 - bits);

	return result;
}

__fi static int GETWORD()
{
	return g_BP.FillBuffer(16);
}

// Removes bits from the bitstream.  This is done independently of UBITS/SBITS because a
// lot of mpeg streams have to read ahead and rewind bits and re-read them at different
// bit depths or sign'age.
__fi static void DUMPBITS(uint num)
{
	g_BP.Advance(num);
	//pxAssume(g_BP.FP != 0);
}

__fi static u32 GETBITS(uint num)
{
	uint retVal = UBITS(num);
	g_BP.Advance(num);

	return retVal;
}

// whenever reading fractions of bytes. The low bits always come from the next byte
// while the high bits come from the current byte
__ri static u8 getBits64(u8 *address, bool advance)
{
	if (!g_BP.FillBuffer(64)) return 0;

	const u8* readpos = &g_BP.internal_qwc[0]._u8[g_BP.BP/8];

	if (uint shift = (g_BP.BP & 7))
	{
		u64 mask = (0xff >> shift);
		mask = mask | (mask << 8) | (mask << 16) | (mask << 24) | (mask << 32) | (mask << 40) | (mask << 48) | (mask << 56);

		*(u64*)address = ((~mask & *(u64*)(readpos + 1)) >> (8 - shift)) | (((mask) & *(u64*)readpos) << shift);
	}
	else
	{
		*(u64*)address = *(u64*)readpos;
	}

	if (advance) g_BP.Advance(64);

	return 1;
}

// whenever reading fractions of bytes. The low bits always come from the next byte
// while the high bits come from the current byte
__ri static u8 getBits32(u8 *address, bool advance)
{
	if (!g_BP.FillBuffer(32)) return 0;

	const u8* readpos = &g_BP.internal_qwc->_u8[g_BP.BP/8];

	if(uint shift = (g_BP.BP & 7))
	{
		u32 mask = (0xff >> shift);
		mask = mask | (mask << 8) | (mask << 16) | (mask << 24);

		*(u32*)address = ((~mask & *(u32*)(readpos + 1)) >> (8 - shift)) | (((mask) & *(u32*)readpos) << shift);
	}
	else
	{
		// Bit position-aligned -- no masking/shifting necessary
		*(u32*)address = *(u32*)readpos;
	}

	if (advance) g_BP.Advance(32);

	return 1;
}

__ri static u8 getBits8(u8 *address, bool advance)
{
	if (!g_BP.FillBuffer(8)) return 0;

	const u8* readpos = &g_BP.internal_qwc[0]._u8[g_BP.BP/8];

	if (uint shift = (g_BP.BP & 7))
	{
		uint mask = (0xff >> shift);
		*(u8*)address = (((~mask) & readpos[1]) >> (8 - shift)) | (((mask) & *readpos) << shift);
	}
	else
	{
		*(u8*)address = *(u8*)readpos;
	}

	if (advance) g_BP.Advance(8);

	return 1;
}


#define W1 2841 /* 2048*sqrt (2)*cos (1*pi/16) */
#define W2 2676 /* 2048*sqrt (2)*cos (2*pi/16) */
#define W3 2408 /* 2048*sqrt (2)*cos (3*pi/16) */
#define W5 1609 /* 2048*sqrt (2)*cos (5*pi/16) */
#define W6 1108 /* 2048*sqrt (2)*cos (6*pi/16) */
#define W7 565  /* 2048*sqrt (2)*cos (7*pi/16) */

/*
 * In legal streams, the IDCT output should be between -384 and +384.
 * In corrupted streams, it is possible to force the IDCT output to go
 * to +-3826 - this is the worst case for a column IDCT where the
 * column inputs are 16-bit values.
 */

__fi static void BUTTERFLY(int& t0, int& t1, int w0, int w1, int d0, int d1)
{
	int tmp = w0 * (d0 + d1);
	t0 = tmp + (w1 - w0) * d1;
	t1 = tmp - (w1 + w0) * d0;
}

__ri static void IDCT_Block(s16* block)
{
	for (int i = 0; i < 8; i++)
	{
		s16* const rblock = block + 8 * i;
		if (!(rblock[1] | ((s32*)rblock)[1] | ((s32*)rblock)[2] |
				((s32*)rblock)[3]))
		{
			u32 tmp = (u16)(rblock[0] << 3);
			tmp |= tmp << 16;
			((s32*)rblock)[0] = tmp;
			((s32*)rblock)[1] = tmp;
			((s32*)rblock)[2] = tmp;
			((s32*)rblock)[3] = tmp;
			continue;
		}

		int a0, a1, a2, a3;
		{
			const int d0 = (rblock[0] << 11) + 128;
			const int d1 = rblock[1];
			const int d2 = rblock[2] << 11;
			const int d3 = rblock[3];
			int t0 = d0 + d2;
			int t1 = d0 - d2;
			int t2, t3;
			BUTTERFLY(t2, t3, W6, W2, d3, d1);
			a0 = t0 + t2;
			a1 = t1 + t3;
			a2 = t1 - t3;
			a3 = t0 - t2;
		}

		int b0, b1, b2, b3;
		{
			const int d0 = rblock[4];
			const int d1 = rblock[5];
			const int d2 = rblock[6];
			const int d3 = rblock[7];
			int t0, t1, t2, t3;
			BUTTERFLY(t0, t1, W7, W1, d3, d0);
			BUTTERFLY(t2, t3, W3, W5, d1, d2);
			b0 = t0 + t2;
			b3 = t1 + t3;
			t0 -= t2;
			t1 -= t3;
			b1 = ((t0 + t1) * 181) >> 8;
			b2 = ((t0 - t1) * 181) >> 8;
		}

		rblock[0] = (a0 + b0) >> 8;
		rblock[1] = (a1 + b1) >> 8;
		rblock[2] = (a2 + b2) >> 8;
		rblock[3] = (a3 + b3) >> 8;
		rblock[4] = (a3 - b3) >> 8;
		rblock[5] = (a2 - b2) >> 8;
		rblock[6] = (a1 - b1) >> 8;
		rblock[7] = (a0 - b0) >> 8;
	}

	for (int i = 0; i < 8; i++)
	{
		s16* const cblock = block + i;

		int a0, a1, a2, a3;
		{
			const int d0 = (cblock[8 * 0] << 11) + 65536;
			const int d1 = cblock[8 * 1];
			const int d2 = cblock[8 * 2] << 11;
			const int d3 = cblock[8 * 3];
			const int t0 = d0 + d2;
			const int t1 = d0 - d2;
			int t2;
			int t3;
			BUTTERFLY(t2, t3, W6, W2, d3, d1);
			a0 = t0 + t2;
			a1 = t1 + t3;
			a2 = t1 - t3;
			a3 = t0 - t2;
		}

		int b0, b1, b2, b3;
		{
			const int d0 = cblock[8 * 4];
			const int d1 = cblock[8 * 5];
			const int d2 = cblock[8 * 6];
			const int d3 = cblock[8 * 7];
			int t0, t1, t2, t3;
			BUTTERFLY(t0, t1, W7, W1, d3, d0);
			BUTTERFLY(t2, t3, W3, W5, d1, d2);
			b0 = t0 + t2;
			b3 = t1 + t3;
			t0 = (t0 - t2) >> 8;
			t1 = (t1 - t3) >> 8;
			b1 = (t0 + t1) * 181;
			b2 = (t0 - t1) * 181;
		}

		cblock[8 * 0] = (a0 + b0) >> 17;
		cblock[8 * 1] = (a1 + b1) >> 17;
		cblock[8 * 2] = (a2 + b2) >> 17;
		cblock[8 * 3] = (a3 + b3) >> 17;
		cblock[8 * 4] = (a3 - b3) >> 17;
		cblock[8 * 5] = (a2 - b2) >> 17;
		cblock[8 * 6] = (a1 - b1) >> 17;
		cblock[8 * 7] = (a0 - b0) >> 17;
	}
}

__ri static void IDCT_Copy(s16* block, u8* dest, const int stride)
{
	IDCT_Block(block);

	for (int i = 0; i < 8; i++)
	{
		dest[0] = (g_idct_clip_lut.data() + 384)[block[0]];
		dest[1] = (g_idct_clip_lut.data() + 384)[block[1]];
		dest[2] = (g_idct_clip_lut.data() + 384)[block[2]];
		dest[3] = (g_idct_clip_lut.data() + 384)[block[3]];
		dest[4] = (g_idct_clip_lut.data() + 384)[block[4]];
		dest[5] = (g_idct_clip_lut.data() + 384)[block[5]];
		dest[6] = (g_idct_clip_lut.data() + 384)[block[6]];
		dest[7] = (g_idct_clip_lut.data() + 384)[block[7]];

		std::memset(block, 0, 16);

		dest += stride;
		block += 8;
	}
}


// stride = increment for dest in 16-bit units (typically either 8 [128 bits] or 16 [256 bits]).
__ri static void IDCT_Add(const int last, s16* block, s16* dest, const int stride)
{
	// on the IPU, stride is always assured to be multiples of QWC (bottom 3 bits are 0).

	if (last != 129 || (block[0] & 7) == 4)
	{
		IDCT_Block(block);

		const r128 zero = r128_zero();
		for (int i = 0; i < 8; i++)
		{
			r128_store(dest, r128_load(block));
			r128_store(block, zero);

			dest += stride;
			block += 8;
		}
	}
	else
	{
		const u16 DC = static_cast<u16>((static_cast<s32>(block[0]) + 4) >> 3);
		const r128 dc128 = r128_from_u32_dup(static_cast<u32>(DC) | (static_cast<u32>(DC) << 16));
		block[0] = block[63] = 0;

		for (int i = 0; i < 8; ++i)
			r128_store((dest + (stride * i)), dc128);
	}
}

/* Bitstream and buffer needs to be reallocated in order for successful
	reading of the old data. Here the old data stored in the 2nd slot
	of the internal buffer is copied to 1st slot, and the new data read
	into 1st slot is copied to the 2nd slot. Which will later be copied
	back to the 1st slot when 128bits have been read.
*/
static const DCTtab * tab;
static int mbaCount = 0;

__ri static int BitstreamInit ()
{
	return g_BP.FillBuffer(32);
}

static int GetMacroblockModes()
{
	int macroblock_modes;
	const MBtab * tab;

	switch (decoder.coding_type)
	{
		case I_TYPE:
			macroblock_modes = UBITS(2);

			if (macroblock_modes == 0) return 0;   // error

			tab = MB_I + (macroblock_modes >> 1);
			DUMPBITS(tab->len);
			macroblock_modes = tab->modes;

			if ((!(decoder.frame_pred_frame_dct)) &&
				(decoder.picture_structure == FRAME_PICTURE))
			{
				macroblock_modes |= GETBITS(1) * DCT_TYPE_INTERLACED;
			}
			return macroblock_modes;

		case P_TYPE:
			macroblock_modes = UBITS(6);

			if (macroblock_modes == 0) return 0;   // error

			tab = MB_P + (macroblock_modes >> 1);
			DUMPBITS(tab->len);
			macroblock_modes = tab->modes;

			if (decoder.picture_structure != FRAME_PICTURE)
			{
				if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
				{
					macroblock_modes |= GETBITS(2) * MOTION_TYPE_BASE;
				}

				return macroblock_modes;
			}
			else if (decoder.frame_pred_frame_dct)
			{
				if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
					macroblock_modes |= MC_FRAME;

				return macroblock_modes;
			}
			else
			{
				if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
				{
					macroblock_modes |= GETBITS(2) * MOTION_TYPE_BASE;
				}

				if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
				{
					macroblock_modes |= GETBITS(1) * DCT_TYPE_INTERLACED;
				}

				return macroblock_modes;
			}

		case B_TYPE:
			macroblock_modes = UBITS(6);

			if (macroblock_modes == 0) return 0;   // error

			tab = MB_B + macroblock_modes;
			DUMPBITS(tab->len);
			macroblock_modes = tab->modes;

			if (decoder.picture_structure != FRAME_PICTURE)
			{
				if (!(macroblock_modes & MACROBLOCK_INTRA))
				{
					macroblock_modes |= GETBITS(2) * MOTION_TYPE_BASE;
				}
				return (macroblock_modes | (tab->len << 16));
			}
			else if (decoder.frame_pred_frame_dct)
			{
				/* if (! (macroblock_modes & MACROBLOCK_INTRA)) */
				macroblock_modes |= MC_FRAME;
				return (macroblock_modes | (tab->len << 16));
			}
			else
			{
				if (macroblock_modes & MACROBLOCK_INTRA) goto intra;

				macroblock_modes |= GETBITS(2) * MOTION_TYPE_BASE;

				if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
				{
intra:
					macroblock_modes |= GETBITS(1) * DCT_TYPE_INTERLACED;
				}
				return (macroblock_modes | (tab->len << 16));
			}

		case D_TYPE:
			macroblock_modes = GETBITS(1);
			//I suspect (as this is actually a 2 bit command) that this should be getbits(2)
			//additionally, we arent dumping any bits here when i think we should be, need a game to test. (Refraction)
			DevCon.Warning(" Rare MPEG command! ");
			if (macroblock_modes == 0) return 0;   // error
			return (MACROBLOCK_INTRA | (1 << 16));

		default:
			return 0;
	}
}

__ri static int get_macroblock_address_increment()
{
	const MBAtab *mba;

	u16 code = UBITS(16);

	if (code >= 4096)
		mba = MBA.mba5 + (UBITS(5) - 2);
	else if (code >= 768)
		mba = MBA.mba11 + (UBITS(11) - 24);
	else switch (UBITS(11))
	{
		case 8:		/* macroblock_escape */
			DUMPBITS(11);
			return 0xb0023;

		case 15:	/* macroblock_stuffing (MPEG1 only) */
			if (decoder.mpeg1)
			{
				DUMPBITS(11);
				return 0xb0022;
			}
			[[fallthrough]];

		default:
			return 0;//error
	}

	DUMPBITS(mba->len);

	return ((mba->mba + 1) | (mba->len << 16));
}

__fi static int get_luma_dc_dct_diff()
{
	int size;
	int dc_diff;
	u16 code = UBITS(5);

	if (code < 31)
	{
		size = DCtable.lum0[code].size;
		DUMPBITS(DCtable.lum0[code].len);

		// 5 bits max
	}
	else
	{
		code = UBITS(9) - 0x1f0;
		size = DCtable.lum1[code].size;
		DUMPBITS(DCtable.lum1[code].len);

		// 9 bits max
	}

	if (size==0)
		dc_diff = 0;
	else
	{
		dc_diff = GETBITS(size);

		// 6 for tab0 and 11 for tab1
		if ((dc_diff & (1<<(size-1)))==0)
		  dc_diff-= (1<<size) - 1;
	}

	return dc_diff;
}

__fi static int get_chroma_dc_dct_diff()
{
	int size;
	int dc_diff;
	u16 code = UBITS(5);

	if (code<31)
	{
		size = DCtable.chrom0[code].size;
		DUMPBITS(DCtable.chrom0[code].len);
	}
	else
	{
		code = UBITS(10) - 0x3e0;
		size = DCtable.chrom1[code].size;
		DUMPBITS(DCtable.chrom1[code].len);
	}

	if (size==0)
		dc_diff = 0;
	else
	{
		dc_diff = GETBITS(size);

		if ((dc_diff & (1<<(size-1)))==0)
		{
			dc_diff-= (1<<size) - 1;
		}
	}

	return dc_diff;
}

__fi static void SATURATE(int& val)
{
	if ((u32)(val + 2048) > 4095)
		val = (val >> 31) ^ 2047;
}

__ri static bool get_intra_block()
{
	const u8 * scan = decoder.scantype ? mpeg2_scan.alt : mpeg2_scan.norm;
	const u8 (&quant_matrix)[64] = decoder.iq;
	int quantizer_scale = decoder.quantizer_scale;
	s16 * dest = decoder.DCTblock;
	u16 code;

	/* decode AC coefficients */
  for (int i=1 + ipu_cmd.pos[4]; ; i++)
  {
	  switch (ipu_cmd.pos[5])
	  {
	  case 0:
		if (!GETWORD())
		{
		  ipu_cmd.pos[4] = i - 1;
		  return false;
		}

		code = UBITS(16);

		if (code >= 16384 && (!decoder.intra_vlc_format || decoder.mpeg1))
		{
		  tab = &DCT.next[(code >> 12) - 4];
		}
		else if (code >= 1024)
		{
			if (decoder.intra_vlc_format && !decoder.mpeg1)
			{
				tab = &DCT.tab0a[(code >> 8) - 4];
			}
			else
			{
				tab = &DCT.tab0[(code >> 8) - 4];
			}
		}
		else if (code >= 512)
		{
			if (decoder.intra_vlc_format && !decoder.mpeg1)
			{
				tab = &DCT.tab1a[(code >> 6) - 8];
			}
			else
			{
				tab = &DCT.tab1[(code >> 6) - 8];
			}
		}

		// [TODO] Optimization: Following codes can all be done by a single "expedited" lookup
		// that should use a single unrolled DCT table instead of five separate tables used
		// here.  Multiple conditional statements are very slow, while modern CPU data caches
		// have lots of room to spare.

		else if (code >= 256)
		{
			tab = &DCT.tab2[(code >> 4) - 16];
		}
		else if (code >= 128)
		{
			tab = &DCT.tab3[(code >> 3) - 16];
		}
		else if (code >= 64)
		{
			tab = &DCT.tab4[(code >> 2) - 16];
		}
		else if (code >= 32)
		{
			tab = &DCT.tab5[(code >> 1) - 16];
		}
		else if (code >= 16)
		{
			tab = &DCT.tab6[code - 16];
		}
		else
		{
		  ipu_cmd.pos[4] = 0;
		  return true;
		}

		DUMPBITS(tab->len);

		if (tab->run==64) /* end_of_block */
		{
			ipu_cmd.pos[4] = 0;
			return true;
		}

		i += (tab->run == 65) ? GETBITS(6) : tab->run;
		if (i >= 64)
		{
			ipu_cmd.pos[4] = 0;
			return true;
		}
		[[fallthrough]];

	  case 1:
	  {
			if (!GETWORD())
			{
				ipu_cmd.pos[4] = i - 1;
				ipu_cmd.pos[5] = 1;
				return false;
			}

			uint j = scan[i];
			int val;

			if (tab->run==65) /* escape */
			{
				if(!decoder.mpeg1)
				{
				  val = (SBITS(12) * quantizer_scale * quant_matrix[i]) >> 4;
				  DUMPBITS(12);
				}
				else
				{
				  val = SBITS(8);
				  DUMPBITS(8);

				  if (!(val & 0x7f))
				  {
					val = GETBITS(8) + 2 * val;
				  }

				  val = (val * quantizer_scale * quant_matrix[i]) >> 4;
				  val = (val + ~ (((s32)val) >> 31)) | 1;
				}
			}
			else
			{
				val = (tab->level * quantizer_scale * quant_matrix[i]) >> 4;
				if(decoder.mpeg1)
				{
					/* oddification */
					val = (val - 1) | 1;
				}

				/* if (bitstream_get (1)) val = -val; */
				int bit1 = SBITS(1);
				val = (val ^ bit1) - bit1;
				DUMPBITS(1);
			}

			SATURATE(val);
			dest[j] = val;
			ipu_cmd.pos[5] = 0;
		}
	 }
  }

  ipu_cmd.pos[4] = 0;
  return true;
}

__ri static bool get_non_intra_block(int * last)
{
	int i;
	int j;
	int val;
	const u8 * scan = decoder.scantype ? mpeg2_scan.alt : mpeg2_scan.norm;
	const u8 (&quant_matrix)[64] = decoder.niq;
	int quantizer_scale = decoder.quantizer_scale;
	s16 * dest = decoder.DCTblock;
	u16 code;

	/* decode AC coefficients */
	for (i= ipu_cmd.pos[4] ; ; i++)
	{
		switch (ipu_cmd.pos[5])
		{
		case 0:
			if (!GETWORD())
			{
				ipu_cmd.pos[4] = i;
				return false;
			}

			code = UBITS(16);

			if (code >= 16384)
			{
				if (i==0)
				{
					tab = &DCT.first[(code >> 12) - 4];
				}
				else
				{
					tab = &DCT.next[(code >> 12)- 4];
				}
			}
			else if (code >= 1024)
			{
				tab = &DCT.tab0[(code >> 8) - 4];
			}
			else if (code >= 512)
			{
				tab = &DCT.tab1[(code >> 6) - 8];
			}

			// [TODO] Optimization: Following codes can all be done by a single "expedited" lookup
			// that should use a single unrolled DCT table instead of five separate tables used
			// here.  Multiple conditional statements are very slow, while modern CPU data caches
			// have lots of room to spare.

			else if (code >= 256)
			{
				tab = &DCT.tab2[(code >> 4) - 16];
			}
			else if (code >= 128)
			{
				tab = &DCT.tab3[(code >> 3) - 16];
			}
			else if (code >= 64)
			{
				tab = &DCT.tab4[(code >> 2) - 16];
			}
			else if (code >= 32)
			{
				tab = &DCT.tab5[(code >> 1) - 16];
			}
			else if (code >= 16)
			{
				tab = &DCT.tab6[code - 16];
			}
			else
			{
				ipu_cmd.pos[4] = 0;
				return true;
			}

			DUMPBITS(tab->len);

			if (tab->run==64) /* end_of_block */
			{
				*last = i;
				ipu_cmd.pos[4] = 0;
				return true;
			}

			i += (tab->run == 65) ? GETBITS(6) : tab->run;
			if (i >= 64)
			{
				*last = i;
				ipu_cmd.pos[4] = 0;
				return true;
			}
			[[fallthrough]];

		case 1:
			if (!GETWORD())
			{
			  ipu_cmd.pos[4] = i;
			  ipu_cmd.pos[5] = 1;
			  return false;
			}

			j = scan[i];

			if (tab->run==65) /* escape */
			{
				if (!decoder.mpeg1)
				{
					val = ((2 * (SBITS(12) + SBITS(1)) + 1) * quantizer_scale * quant_matrix[i]) >> 5;
					DUMPBITS(12);
				}
				else
				{
				  val = SBITS(8);
				  DUMPBITS(8);

				  if (!(val & 0x7f))
				  {
					val = GETBITS(8) + 2 * val;
				  }

				  val = ((2 * (val + (((s32)val) >> 31)) + 1) * quantizer_scale * quant_matrix[i]) / 32;
				  val = (val + ~ (((s32)val) >> 31)) | 1;
				}
			}
			else
			{
				int bit1 = SBITS(1);
				val = ((2 * tab->level + 1) * quantizer_scale * quant_matrix[i]) >> 5;
				val = (val ^ bit1) - bit1;
				DUMPBITS(1);
			}

			SATURATE(val);
			dest[j] = val;
			ipu_cmd.pos[5] = 0;
		}
	}

	ipu_cmd.pos[4] = 0;
	return true;
}

__ri static bool slice_intra_DCT(const int cc, u8 * const dest, const int stride, const bool skip)
{
	if (!skip || ipu_cmd.pos[3])
	{
		ipu_cmd.pos[3] = 0;
		if (!GETWORD())
		{
			ipu_cmd.pos[3] = 1;
			return false;
		}

		/* Get the intra DC coefficient and inverse quantize it */
		if (cc == 0)
			decoder.dc_dct_pred[0] += get_luma_dc_dct_diff();
		else
			decoder.dc_dct_pred[cc] += get_chroma_dc_dct_diff();

		decoder.DCTblock[0] = decoder.dc_dct_pred[cc] << (3 - decoder.intra_dc_precision);
	}

	if (!get_intra_block())
	{
		return false;
	}

	IDCT_Copy(decoder.DCTblock, dest, stride);

	return true;
}

__ri static bool slice_non_intra_DCT(s16 * const dest, const int stride, const bool skip)
{
	if (!skip)
		std::memset(decoder.DCTblock, 0, sizeof(decoder.DCTblock));

	int last = 0;
	if (!get_non_intra_block(&last))
		return false;

	IDCT_Add(last, decoder.DCTblock, dest, stride);
	return true;
}

__fi static void finishmpeg2sliceIDEC()
{
	ipuRegs.ctrl.SCD = 0;
	coded_block_pattern = decoder.coded_block_pattern;
}

__ri static bool mpeg2sliceIDEC()
{
	u16 code;
	static bool ready_to_decode = true;
	switch (ipu_cmd.pos[0])
	{
	case 0:
		decoder.dc_dct_pred[0] =
		decoder.dc_dct_pred[1] =
		decoder.dc_dct_pred[2] = 128 << decoder.intra_dc_precision;

		ipuRegs.top = 0;
		ipuRegs.ctrl.ECD = 0;
		[[fallthrough]];

	case 1:
		ipu_cmd.pos[0] = 1;
		if (!BitstreamInit())
		{
			return false;
		}
		[[fallthrough]];

	case 2:
		ipu_cmd.pos[0] = 2;
		while (1)
		{
			// IPU0 isn't ready for data, so let's wait for it to be
			if ((!ipu0ch.chcr.STR || ipuRegs.ctrl.OFC || ipu0ch.qwc == 0) && ipu_cmd.pos[1] <= 2)
			{
				IPUCoreStatus.WaitingOnIPUFrom = true;
				return false;
			}
			macroblock_8& mb8 = decoder.mb8;
			macroblock_rgb16& rgb16 = decoder.rgb16;
			macroblock_rgb32& rgb32 = decoder.rgb32;

			int DCT_offset, DCT_stride;
			const MBAtab * mba;

			switch (ipu_cmd.pos[1])
			{
			case 0:
				decoder.macroblock_modes = GetMacroblockModes();

				if (decoder.macroblock_modes & MACROBLOCK_QUANT) //only IDEC
				{
					const int quantizer_scale_code = GETBITS(5);
					if (decoder.q_scale_type)
						decoder.quantizer_scale = non_linear_quantizer_scale[quantizer_scale_code];
					else
						decoder.quantizer_scale = quantizer_scale_code << 1;
				}

				decoder.coded_block_pattern = 0x3F;//all 6 blocks
				std::memset(&mb8, 0, sizeof(mb8));
				std::memset(&rgb32, 0, sizeof(rgb32));
				[[fallthrough]];

			case 1:
				ipu_cmd.pos[1] = 1;

				if (decoder.macroblock_modes & DCT_TYPE_INTERLACED)
				{
					DCT_offset = decoder_stride;
					DCT_stride = decoder_stride * 2;
				}
				else
				{
					DCT_offset = decoder_stride * 8;
					DCT_stride = decoder_stride;
				}

				switch (ipu_cmd.pos[2])
				{
				case 0:
				case 1:
					if (!slice_intra_DCT(0, (u8*)mb8.Y, DCT_stride, ipu_cmd.pos[2] == 1))
					{
						ipu_cmd.pos[2] = 1;
						return false;
					}
					[[fallthrough]];

				case 2:
					if (!slice_intra_DCT(0, (u8*)mb8.Y + 8, DCT_stride, ipu_cmd.pos[2] == 2))
					{
						ipu_cmd.pos[2] = 2;
						return false;
					}
					[[fallthrough]];

				case 3:
					if (!slice_intra_DCT(0, (u8*)mb8.Y + DCT_offset, DCT_stride, ipu_cmd.pos[2] == 3))
					{
						ipu_cmd.pos[2] = 3;
						return false;
					}
					[[fallthrough]];

				case 4:
					if (!slice_intra_DCT(0, (u8*)mb8.Y + DCT_offset + 8, DCT_stride, ipu_cmd.pos[2] == 4))
					{
						ipu_cmd.pos[2] = 4;
						return false;
					}
					[[fallthrough]];

				case 5:
					if (!slice_intra_DCT(1, (u8*)mb8.Cb, decoder_stride >> 1, ipu_cmd.pos[2] == 5))
					{
						ipu_cmd.pos[2] = 5;
						return false;
					}
					[[fallthrough]];

				case 6:
					if (!slice_intra_DCT(2, (u8*)mb8.Cr, decoder_stride >> 1, ipu_cmd.pos[2] == 6))
					{
						ipu_cmd.pos[2] = 6;
						return false;
					}
					break;

				jNO_DEFAULT;
				}

				// Send The MacroBlock via DmaIpuFrom
				ipu_csc(mb8, rgb32, decoder.sgn);

				if (decoder.ofm == 0)
					decoder.SetOutputTo(rgb32);
				else
				{
					ipu_dither(rgb32, rgb16, decoder.dte);
					decoder.SetOutputTo(rgb16);
				}
				ipu_cmd.pos[1] = 2;
				[[fallthrough]];
			case 2:
			{
				if (ready_to_decode == true)
				{
					ready_to_decode = false;
					IPUCoreStatus.WaitingOnIPUFrom = false;
					IPUCoreStatus.WaitingOnIPUTo = false;
					IPU_INT_PROCESS( 64); // Should probably be much higher, but myst 3 doesn't like it right now.
					ipu_cmd.pos[1] = 2;
					return false;
				}
				pxAssert(decoder.ipu0_data > 0);
				uint read = ipu_fifo.out.write((u32*)decoder.GetIpuDataPtr(), decoder.ipu0_data);
				decoder.AdvanceIpuDataBy(read);

				if (decoder.ipu0_data != 0)
				{
					// IPU FIFO filled up -- Will have to finish transferring later.
					IPUCoreStatus.WaitingOnIPUFrom = true;
					ipu_cmd.pos[1] = 2;
					return false;
				}

				mbaCount = 0;
				if (read)
				{
					IPUCoreStatus.WaitingOnIPUFrom = true;
					ipu_cmd.pos[1] = 3;
					return false;
				}
			}
				[[fallthrough]];

			case 3:
				ready_to_decode = true;
				while (1)
				{
					if (!GETWORD())
					{
						ipu_cmd.pos[1] = 3;
						return false;
					}

					code = UBITS(16);
					if (code >= 0x1000)
					{
						mba = MBA.mba5 + (UBITS(5) - 2);
						break;
					}
					else if (code >= 0x0300)
					{
						mba = MBA.mba11 + (UBITS(11) - 24);
						break;
					}
					else switch (UBITS(11))
					{
						case 8:		/* macroblock_escape */
							mbaCount += 33;
							[[fallthrough]];

						case 15:	/* macroblock_stuffing (MPEG1 only) */
							DUMPBITS(11);
							continue;

						default:	/* end of slice/frame, or error? */
						{
							goto finish_idec;
						}
					}
				}

				DUMPBITS(mba->len);
				mbaCount += mba->mba;

				if (mbaCount)
				{
					decoder.dc_dct_pred[0] =
					decoder.dc_dct_pred[1] =
					decoder.dc_dct_pred[2] = 128 << decoder.intra_dc_precision;
				}
				[[fallthrough]];

			case 4:
				if (!GETWORD())
				{
					ipu_cmd.pos[1] = 4;
					return false;
				}
				break;

			jNO_DEFAULT;
			}

			ipu_cmd.pos[1] = 0;
			ipu_cmd.pos[2] = 0;
		}

finish_idec:
		finishmpeg2sliceIDEC();
		[[fallthrough]];

	case 3:
	{
		u8 bit8;
		u32 start_check;
		if (!getBits8((u8*)&bit8, 0))
		{
			ipu_cmd.pos[0] = 3;
			return false;
		}

		if (bit8 == 0)
		{
			g_BP.Align();
			do
			{
				if (!g_BP.FillBuffer(24))
				{
					ipu_cmd.pos[0] = 3;
					return false;
				}
				start_check = UBITS(24);
				if (start_check != 0)
				{
					if (start_check == 1)
					{
						ipuRegs.ctrl.SCD = 1;
					}
					else
					{
						ipuRegs.ctrl.ECD = 1;
					}
					break;
				}
				DUMPBITS(8);
			} while (1);
		}
	}
		[[fallthrough]];

	case 4:
		if (!getBits32((u8*)&ipuRegs.top, 0))
		{
			ipu_cmd.pos[0] = 4;
			return false;
		}

		ipuRegs.top = BigEndian(ipuRegs.top);
		break;

	jNO_DEFAULT;
	}

	return true;
}

__fi static bool mpeg2_slice()
{
	int DCT_offset, DCT_stride;
	static bool ready_to_decode = true;

	macroblock_8& mb8 = decoder.mb8;
	macroblock_16& mb16 = decoder.mb16;

	switch (ipu_cmd.pos[0])
	{
	case 0:
		if (decoder.dcr)
		{
			decoder.dc_dct_pred[0] =
			decoder.dc_dct_pred[1] =
			decoder.dc_dct_pred[2] = 128 << decoder.intra_dc_precision;
		}

		ipuRegs.ctrl.ECD = 0;
		ipuRegs.top = 0;
		std::memset(&mb8, 0, sizeof(mb8));
		std::memset(&mb16, 0, sizeof(mb16));
		[[fallthrough]];

	case 1:
		if (!BitstreamInit())
		{
			ipu_cmd.pos[0] = 1;
			return false;
		}
		[[fallthrough]];

	case 2:
		ipu_cmd.pos[0] = 2;

		// IPU0 isn't ready for data, so let's wait for it to be
		if ((!ipu0ch.chcr.STR || ipuRegs.ctrl.OFC || ipu0ch.qwc == 0) && ipu_cmd.pos[0] <= 3)
		{
			IPUCoreStatus.WaitingOnIPUFrom = true;
			return false;
		}

		if (decoder.macroblock_modes & DCT_TYPE_INTERLACED)
		{
			DCT_offset = decoder_stride;
			DCT_stride = decoder_stride * 2;
		}
		else
		{
			DCT_offset = decoder_stride * 8;
			DCT_stride = decoder_stride;
		}

		if (decoder.macroblock_modes & MACROBLOCK_INTRA)
		{
			switch(ipu_cmd.pos[1])
			{
			case 0:
				decoder.coded_block_pattern = 0x3F;
				[[fallthrough]];

			case 1:
				if (!slice_intra_DCT(0, (u8*)mb8.Y, DCT_stride, ipu_cmd.pos[1] == 1))
				{
					ipu_cmd.pos[1] = 1;
					return false;
				}
				[[fallthrough]];

			case 2:
				if (!slice_intra_DCT(0, (u8*)mb8.Y + 8, DCT_stride, ipu_cmd.pos[1] == 2))
				{
					ipu_cmd.pos[1] = 2;
					return false;
				}
				[[fallthrough]];

			case 3:
				if (!slice_intra_DCT(0, (u8*)mb8.Y + DCT_offset, DCT_stride, ipu_cmd.pos[1] == 3))
				{
					ipu_cmd.pos[1] = 3;
					return false;
				}
				[[fallthrough]];

			case 4:
				if (!slice_intra_DCT(0, (u8*)mb8.Y + DCT_offset + 8, DCT_stride, ipu_cmd.pos[1] == 4))
				{
					ipu_cmd.pos[1] = 4;
					return false;
				}
				[[fallthrough]];

			case 5:
				if (!slice_intra_DCT(1, (u8*)mb8.Cb, decoder_stride >> 1, ipu_cmd.pos[1] == 5))
				{
					ipu_cmd.pos[1] = 5;
					return false;
				}
				[[fallthrough]];

			case 6:
				if (!slice_intra_DCT(2, (u8*)mb8.Cr, decoder_stride >> 1, ipu_cmd.pos[1] == 6))
				{
					ipu_cmd.pos[1] = 6;
					return false;
				}
				break;

			jNO_DEFAULT;
			}

			// Copy macroblock8 to macroblock16 - without sign extension.
			// Manually inlined due to MSVC refusing to inline the SSE-optimized version.
			{
				const u8	*s = (const u8*)&mb8;
				u16			*d = (u16*)&mb16;

				//Y  bias	- 16 * 16
				//Cr bias	- 8 * 8
				//Cb bias	- 8 * 8

				__m128i zeroreg = _mm_setzero_si128();

				for (uint i = 0; i < (256+64+64) / 32; ++i)
				{
					//*d++ = *s++;
					__m128i woot1 = _mm_load_si128((__m128i*)s);
					__m128i woot2 = _mm_load_si128((__m128i*)s+1);
					_mm_store_si128((__m128i*)d,	_mm_unpacklo_epi8(woot1, zeroreg));
					_mm_store_si128((__m128i*)d+1,	_mm_unpackhi_epi8(woot1, zeroreg));
					_mm_store_si128((__m128i*)d+2,	_mm_unpacklo_epi8(woot2, zeroreg));
					_mm_store_si128((__m128i*)d+3,	_mm_unpackhi_epi8(woot2, zeroreg));
					s += 32;
					d += 32;
				}
			}
		}
		else
		{
			if (decoder.macroblock_modes & MACROBLOCK_PATTERN)
			{
				switch (ipu_cmd.pos[1])
				{
				case 0:
				{
					// Get coded block pattern
					const CBPtab* tab;
					u16 code = UBITS(16);

					if (code >= 0x2000)
						tab = CBP_7 + (UBITS(7) - 16);
					else
						tab = CBP_9 + UBITS(9);

					DUMPBITS(tab->len);
					decoder.coded_block_pattern = tab->cbp;
				}
				[[fallthrough]];

				case 1:
					if (decoder.coded_block_pattern & 0x20)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Y, DCT_stride, ipu_cmd.pos[1] == 1))
						{
							ipu_cmd.pos[1] = 1;
							return false;
						}
					}
					[[fallthrough]];

				case 2:
					if (decoder.coded_block_pattern & 0x10)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Y + 8, DCT_stride, ipu_cmd.pos[1] == 2))
						{
							ipu_cmd.pos[1] = 2;
							return false;
						}
					}
					[[fallthrough]];

				case 3:
					if (decoder.coded_block_pattern & 0x08)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Y + DCT_offset, DCT_stride, ipu_cmd.pos[1] == 3))
						{
							ipu_cmd.pos[1] = 3;
							return false;
						}
					}
					[[fallthrough]];

				case 4:
					if (decoder.coded_block_pattern & 0x04)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Y + DCT_offset + 8, DCT_stride, ipu_cmd.pos[1] == 4))
						{
							ipu_cmd.pos[1] = 4;
							return false;
						}
					}
					[[fallthrough]];

				case 5:
					if (decoder.coded_block_pattern & 0x2)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Cb, decoder_stride >> 1, ipu_cmd.pos[1] == 5))
						{
							ipu_cmd.pos[1] = 5;
							return false;
						}
					}
					[[fallthrough]];

				case 6:
					if (decoder.coded_block_pattern & 0x1)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Cr, decoder_stride >> 1, ipu_cmd.pos[1] == 6))
						{
							ipu_cmd.pos[1] = 6;
							return false;
						}
					}
					break;

					jNO_DEFAULT;
				}
			}
			else
				DevCon.Warning("No macroblock mode");
		}

		// Send The MacroBlock via DmaIpuFrom
		ipuRegs.ctrl.SCD = 0;
		coded_block_pattern = decoder.coded_block_pattern;

		decoder.SetOutputTo(mb16);
		[[fallthrough]];
	case 3:
	{
		if (ready_to_decode == true)
		{
			ipu_cmd.pos[0] = 3;
			ready_to_decode = false;
			IPUCoreStatus.WaitingOnIPUFrom = false;
			IPUCoreStatus.WaitingOnIPUTo = false;
			IPU_INT_PROCESS( 64); // Should probably be much higher, but myst 3 doesn't like it right now.
			return false;
		}

		pxAssert(decoder.ipu0_data > 0);
		uint read = ipu_fifo.out.write((u32*)decoder.GetIpuDataPtr(), decoder.ipu0_data);
		decoder.AdvanceIpuDataBy(read);

		if (decoder.ipu0_data != 0)
		{
			// IPU FIFO filled up -- Will have to finish transferring later.
			IPUCoreStatus.WaitingOnIPUFrom = true;
			ipu_cmd.pos[0] = 3;
			return false;
		}

		mbaCount = 0;
		if (read)
		{
			IPUCoreStatus.WaitingOnIPUFrom = true;
			ipu_cmd.pos[0] = 4;
			return false;
		}
	}
		[[fallthrough]];

	case 4:
	{
		u8 bit8;
		u32 start_check;
		if (!getBits8((u8*)&bit8, 0))
		{
			ipu_cmd.pos[0] = 4;
			return false;
		}

		if (bit8 == 0)
		{
			g_BP.Align();
			do
			{
				if (!g_BP.FillBuffer(24))
				{
					ipu_cmd.pos[0] = 4;
					return false;
				}
				start_check = UBITS(24);
				if (start_check != 0)
				{
					if (start_check == 1)
					{
						ipuRegs.ctrl.SCD = 1;
					}
					else
					{
						ipuRegs.ctrl.ECD = 1;
					}
					break;
				}
				DUMPBITS(8);
			} while (1);
		}
	}
		[[fallthrough]];

	case 5:
		if (!getBits32((u8*)&ipuRegs.top, 0))
		{
			ipu_cmd.pos[0] = 5;
			return false;
		}

		ipuRegs.top = BigEndian(ipuRegs.top);
		break;
	}

	ready_to_decode = true;
	return true;
}


//////////////////////////////////////////////////////
// IPU Commands (exec on worker thread only)

__fi static bool ipuVDEC(u32 val)
{
	static int count = 0;
	if (count++ > 5) {
		if (!FMVstarted) {
			EnableFMV = true;
			FMVstarted = true;
		}
		count = 0;
	}
	eecount_on_last_vdec = cpuRegs.cycle;

	switch (ipu_cmd.pos[0])
	{
		case 0:
			if (!BitstreamInit()) return false;

			switch ((val >> 26) & 3)
			{
				case 0://Macroblock Address Increment
					decoder.mpeg1 = ipuRegs.ctrl.MP1;
					ipuRegs.cmd.DATA = get_macroblock_address_increment();
					break;

				case 1://Macroblock Type
					decoder.frame_pred_frame_dct = 1;
					decoder.coding_type = ipuRegs.ctrl.PCT > 0 ? ipuRegs.ctrl.PCT : 1; // Kaiketsu Zorro Mezase doesn't set a Picture type, seems happy with I
					ipuRegs.cmd.DATA = GetMacroblockModes();
					break;

				case 2://Motion Code
					{
						const u16 code = UBITS(16);
						if ((code & 0x8000))
						{
							DUMPBITS(1);
							ipuRegs.cmd.DATA = 0x00010000;
						}
						else
						{
							const MVtab* tab;
							if ((code & 0xf000) || ((code & 0xfc00) == 0x0c00))
								tab = MV_4 + UBITS(4);
							else
								tab = MV_10 + UBITS(10);

							const int delta = tab->delta + 1;
							DUMPBITS(tab->len);

							const int sign = SBITS(1);
							DUMPBITS(1);

							ipuRegs.cmd.DATA = (((delta ^ sign) - sign) | (tab->len << 16));
						}
					}
					break;

				case 3://DMVector
					{
						const DMVtab* tab = DMV_2 + UBITS(2);
						DUMPBITS(tab->len);
						ipuRegs.cmd.DATA = (tab->dmv | (tab->len << 16));
					}
					break;

				jNO_DEFAULT
			}

			// HACK ATTACK!  This code OR's the MPEG decoder's bitstream position into the upper
			// 16 bits of DATA; which really doesn't make sense since (a) we already rewound the bits
			// back into the IPU internal buffer above, and (b) the IPU doesn't have an MPEG internal
			// 32-bit decoder buffer of its own anyway.  Furthermore, setting the upper 16 bits to
			// any value other than zero appears to work fine.  When set to zero, however, FMVs run
			// very choppy (basically only decoding/updating every 30th frame or so). So yeah,
			// someone with knowledge on the subject please feel free to explain this one. :) --air

			// The upper bits are the "length" of the decoded command, where the lower is the address.
			// This is due to differences with IPU and the MPEG standard. See get_macroblock_address_increment().

			ipuRegs.ctrl.ECD = (ipuRegs.cmd.DATA == 0);
			[[fallthrough]];

		case 1:
			if (!getBits32((u8*)&ipuRegs.top, 0))
			{
				ipu_cmd.pos[0] = 1;
				return false;
			}

			ipuRegs.top = BigEndian(ipuRegs.top);

			IPU_LOG("VDEC command data 0x%x(0x%x). Skip 0x%X bits/Table=%d (%s), pct %d",
			        ipuRegs.cmd.DATA, ipuRegs.cmd.DATA >> 16, val & 0x3f, (val >> 26) & 3, (val >> 26) & 1 ?
			        ((val >> 26) & 2 ? "DMV" : "MBT") : (((val >> 26) & 2 ? "MC" : "MBAI")), ipuRegs.ctrl.PCT);

			return true;

		jNO_DEFAULT
	}

	return false;
}

__ri static bool ipuFDEC(u32 val)
{
	if (!getBits32((u8*)&ipuRegs.cmd.DATA, 0)) return false;

	ipuRegs.cmd.DATA = BigEndian(ipuRegs.cmd.DATA);
	ipuRegs.top = ipuRegs.cmd.DATA;

	IPU_LOG("FDEC read: 0x%08x", ipuRegs.top);

	return true;
}

static bool ipuSETIQ(u32 val)
{
	if ((val >> 27) & 1)
	{
		u8 (&niq)[64] = decoder.niq;

		for(;ipu_cmd.pos[0] < 8; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)niq + 8 * ipu_cmd.pos[0], 1)) return false;
		}

		IPU_LOG("Read non-intra quantization matrix from FIFO.");
		for (uint i = 0; i < 8; i++)
		{
			IPU_LOG("%02X %02X %02X %02X %02X %02X %02X %02X",
			        niq[i * 8 + 0], niq[i * 8 + 1], niq[i * 8 + 2], niq[i * 8 + 3],
			        niq[i * 8 + 4], niq[i * 8 + 5], niq[i * 8 + 6], niq[i * 8 + 7]);
		}
	}
	else
	{
		u8 (&iq)[64] = decoder.iq;

		for(;ipu_cmd.pos[0] < 8; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)iq + 8 * ipu_cmd.pos[0], 1)) return false;
		}

		IPU_LOG("Read intra quantization matrix from FIFO.");
		for (uint i = 0; i < 8; i++)
		{
			IPU_LOG("%02X %02X %02X %02X %02X %02X %02X %02X",
			        iq[i * 8 + 0], iq[i * 8 + 1], iq[i * 8 + 2], iq[i *8 + 3],
			        iq[i * 8 + 4], iq[i * 8 + 5], iq[i * 8 + 6], iq[i *8 + 7]);
		}
	}

	return true;
}

static bool ipuSETVQ(u32 val)
{
	for(;ipu_cmd.pos[0] < 4; ipu_cmd.pos[0]++)
	{
		if (!getBits64(((u8*)g_ipu_vqclut) + 8 * ipu_cmd.pos[0], 1)) return false;
	}

	IPU_LOG("SETVQ command.   Read VQCLUT table from FIFO.\n"
	        "%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d\n"
	        "%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d\n"
	        "%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d\n"
	        "%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d",
	        g_ipu_vqclut[ 0].r, g_ipu_vqclut[ 0].g, g_ipu_vqclut[ 0].b,
	        g_ipu_vqclut[ 1].r, g_ipu_vqclut[ 1].g, g_ipu_vqclut[ 1].b,
	        g_ipu_vqclut[ 2].r, g_ipu_vqclut[ 2].g, g_ipu_vqclut[ 2].b,
	        g_ipu_vqclut[ 3].r, g_ipu_vqclut[ 3].g, g_ipu_vqclut[ 3].b,
	        g_ipu_vqclut[ 4].r, g_ipu_vqclut[ 4].g, g_ipu_vqclut[ 4].b,
	        g_ipu_vqclut[ 5].r, g_ipu_vqclut[ 5].g, g_ipu_vqclut[ 5].b,
	        g_ipu_vqclut[ 6].r, g_ipu_vqclut[ 6].g, g_ipu_vqclut[ 6].b,
	        g_ipu_vqclut[ 7].r, g_ipu_vqclut[ 7].g, g_ipu_vqclut[ 7].b,
	        g_ipu_vqclut[ 8].r, g_ipu_vqclut[ 8].g, g_ipu_vqclut[ 8].b,
	        g_ipu_vqclut[ 9].r, g_ipu_vqclut[ 9].g, g_ipu_vqclut[ 9].b,
	        g_ipu_vqclut[10].r, g_ipu_vqclut[10].g, g_ipu_vqclut[10].b,
	        g_ipu_vqclut[11].r, g_ipu_vqclut[11].g, g_ipu_vqclut[11].b,
	        g_ipu_vqclut[12].r, g_ipu_vqclut[12].g, g_ipu_vqclut[12].b,
	        g_ipu_vqclut[13].r, g_ipu_vqclut[13].g, g_ipu_vqclut[13].b,
	        g_ipu_vqclut[14].r, g_ipu_vqclut[14].g, g_ipu_vqclut[14].b,
	        g_ipu_vqclut[15].r, g_ipu_vqclut[15].g, g_ipu_vqclut[15].b);

	return true;
}

// IPU Transfers are split into 8Qwords so we need to send ALL the data
__ri static bool ipuCSC(tIPU_CMD_CSC csc)
{
	csc.log_from_YCbCr();

	for (;ipu_cmd.index < (int)csc.MBC; ipu_cmd.index++)
	{
		for(;ipu_cmd.pos[0] < 48; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)&decoder.mb8 + 8 * ipu_cmd.pos[0], 1)) return false;
		}

		ipu_csc(decoder.mb8, decoder.rgb32, 0);
		if (csc.OFM) ipu_dither(decoder.rgb32, decoder.rgb16, csc.DTE);

		if (csc.OFM)
		{
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*) & decoder.rgb16) + 4 * ipu_cmd.pos[1], 32 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 32)
			{
				IPUCoreStatus.WaitingOnIPUFrom = true;
				return false;
			}
		}
		else
		{
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*) & decoder.rgb32) + 4 * ipu_cmd.pos[1], 64 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 64)
			{
				IPUCoreStatus.WaitingOnIPUFrom = true;
				return false;
			}
		}

		ipu_cmd.pos[0] = 0;
		ipu_cmd.pos[1] = 0;
	}

	return true;
}

__ri static bool ipuPACK(tIPU_CMD_CSC csc)
{
	csc.log_from_RGB32();

	for (;ipu_cmd.index < (int)csc.MBC; ipu_cmd.index++)
	{
		for(;ipu_cmd.pos[0] < (int)sizeof(macroblock_rgb32) / 8; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)&decoder.rgb32 + 8 * ipu_cmd.pos[0], 1)) return false;
		}

		ipu_dither(decoder.rgb32, decoder.rgb16, csc.DTE);

		if (!csc.OFM) ipu_vq(decoder.rgb16, g_ipu_indx4);

		if (csc.OFM)
		{
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*) & decoder.rgb16) + 4 * ipu_cmd.pos[1], 32 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 32)
			{
				IPUCoreStatus.WaitingOnIPUFrom = true;
				return false;
			}
		}
		else
		{
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*)g_ipu_indx4) + 4 * ipu_cmd.pos[1], 8 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 8)
			{
				IPUCoreStatus.WaitingOnIPUFrom = true;
				return false;
			}
		}

		ipu_cmd.pos[0] = 0;
		ipu_cmd.pos[1] = 0;
	}

	return true;
}

// --------------------------------------------------------------------------------------
//  CORE Functions (referenced from MPEG library)
// --------------------------------------------------------------------------------------

__fi static void ipu_csc(macroblock_8& mb8, macroblock_rgb32& rgb32, int sgn)
{
	int i;
	u8* p = (u8*)&rgb32;

	yuv2rgb();

	if (g_ipu_thresh[0] > 0)
	{
		for (i = 0; i < 16*16; i++, p += 4)
		{
			if ((p[0] < g_ipu_thresh[0]) && (p[1] < g_ipu_thresh[0]) && (p[2] < g_ipu_thresh[0]))
				*(u32*)p = 0;
			else if ((p[0] < g_ipu_thresh[1]) && (p[1] < g_ipu_thresh[1]) && (p[2] < g_ipu_thresh[1]))
				p[3] = 0x40;
		}
	}
	else if (g_ipu_thresh[1] > 0)
	{
		for (i = 0; i < 16*16; i++, p += 4)
		{
			if ((p[0] < g_ipu_thresh[1]) && (p[1] < g_ipu_thresh[1]) && (p[2] < g_ipu_thresh[1]))
				p[3] = 0x40;
		}
	}
	if (sgn)
	{
		for (i = 0; i < 16*16; i++, p += 4)
		{
			*(u32*)p ^= 0x808080;
		}
	}
}

__fi static void ipu_vq(macroblock_rgb16& rgb16, u8* indx4)
{
	const auto closest_index = [&](int i, int j) {
		u8 index = 0;
		int min_distance = std::numeric_limits<int>::max();
		for (u8 k = 0; k < 16; ++k)
		{
			const int dr = rgb16.c[i][j].r - g_ipu_vqclut[k].r;
			const int dg = rgb16.c[i][j].g - g_ipu_vqclut[k].g;
			const int db = rgb16.c[i][j].b - g_ipu_vqclut[k].b;
			const int distance = dr * dr + dg * dg + db * db;

			// XXX: If two distances are the same which index is used?
			if (min_distance > distance)
			{
				index = k;
				min_distance = distance;
			}
		}

		return index;
	};

	for (int i = 0; i < 16; ++i)
		for (int j = 0; j < 8; ++j)
			indx4[i * 8 + j] = closest_index(i, 2 * j + 1) << 4 | closest_index(i, 2 * j);
}

__noinline void IPUWorker()
{
	pxAssert(ipuRegs.ctrl.BUSY);

	switch (ipu_cmd.CMD)
	{
		// These are unreachable (BUSY will always be 0 for them)
		//case SCE_IPU_BCLR:
		//case SCE_IPU_SETTH:
			//break;

		case SCE_IPU_IDEC:
			if (!mpeg2sliceIDEC()) return;

			//ipuRegs.ctrl.OFC = 0;
			ipuRegs.topbusy = 0;
			ipuRegs.cmd.BUSY = 0;
			break;

		case SCE_IPU_BDEC:
			if (!mpeg2_slice()) return;

			ipuRegs.topbusy = 0;
			ipuRegs.cmd.BUSY = 0;

			//if (ipuRegs.ctrl.SCD || ipuRegs.ctrl.ECD) hwIntcIrq(INTC_IPU);
			break;

		case SCE_IPU_VDEC:
			if (!ipuVDEC(ipu_cmd.current)) return;

			ipuRegs.topbusy = 0;
			ipuRegs.cmd.BUSY = 0;
			break;

		case SCE_IPU_FDEC:
			if (!ipuFDEC(ipu_cmd.current)) return;

			ipuRegs.topbusy = 0;
			ipuRegs.cmd.BUSY = 0;
			break;

		case SCE_IPU_SETIQ:
			if (!ipuSETIQ(ipu_cmd.current)) return;
			break;

		case SCE_IPU_SETVQ:
			if (!ipuSETVQ(ipu_cmd.current)) return;
			break;

		case SCE_IPU_CSC:
			if (!ipuCSC(ipu_cmd.current)) return;
			break;

		case SCE_IPU_PACK:
			if (!ipuPACK(ipu_cmd.current)) return;
			break;

		jNO_DEFAULT
	}

	// success
	IPU_LOG("IPU Command finished");
	ipuRegs.ctrl.BUSY = 0;
	//ipu_cmd.current = 0xffffffff;
	hwIntcIrq(INTC_IPU);
}

MULTI_ISA_UNSHARED_END
