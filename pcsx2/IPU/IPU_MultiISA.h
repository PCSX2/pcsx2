// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "IPU/IPU.h"
#include "IPU/mpeg2_vlc.h"
#include "GS/MultiISA.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef _MSC_VER
#define BigEndian(in) _byteswap_ulong(in)
#else
#define BigEndian(in) __builtin_bswap32(in) // or we could use the asm function bswap...
#endif

#ifdef _MSC_VER
#define BigEndian64(in) _byteswap_uint64(in)
#else
#define BigEndian64(in) __builtin_bswap64(in) // or we could use the asm function bswap...
#endif

struct macroblock_8{
	u8 Y[16][16];		//0
	u8 Cb[8][8];		//1
	u8 Cr[8][8];		//2
};

struct macroblock_16{
	s16 Y[16][16];			//0
	s16 Cb[8][8];			//1
	s16 Cr[8][8];			//2
};

struct macroblock_rgb32{
	struct {
		u8 r, g, b, a;
	} c[16][16];
};

struct rgb16_t{
	u16 r:5, g:5, b:5, a:1;
};

struct macroblock_rgb16{
	rgb16_t	c[16][16];
};

struct decoder_t {
	/* first, state that carries information from one macroblock to the */
	/* next inside a slice, and is never used outside of mpeg2_slice() */

	/* DCT coefficients - should be kept aligned ! */
	s16 DCTblock[64];

	u8 niq[64];			//non-intraquant matrix (sequence header)
	u8 iq[64];			//intraquant matrix (sequence header)

	macroblock_8 mb8;
	macroblock_16 mb16;
	macroblock_rgb32 rgb32;
	macroblock_rgb16 rgb16;

	uint ipu0_data;		// amount of data in the output macroblock (in QWC)
	uint ipu0_idx;

	int quantizer_scale;

	/* now non-slice-specific information */

	/* picture header stuff */

	/* what type of picture this is (I, P, B, D) */
	int coding_type;

	/* picture coding extension stuff */

	/* predictor for DC coefficients in intra blocks */
	s16 dc_dct_pred[3];

	/* quantization factor for intra dc coefficients */
	int intra_dc_precision;
	/* top/bottom/both fields */
	int picture_structure;
	/* bool to indicate all predictions are frame based */
	int frame_pred_frame_dct;
	/* bool to indicate whether intra blocks have motion vectors */
	/* (for concealment) */
	int concealment_motion_vectors;
	/* bit to indicate which quantization table to use */
	int q_scale_type;
	/* bool to use different vlc tables */
	int intra_vlc_format;
	/* used for DMV MC */
	int top_field_first;
	// Pseudo Sign Offset
	int sgn;
	// Dither Enable
	int dte;
	// Output Format
	int ofm;
	// Macroblock type
	int macroblock_modes;
	// DC Reset
	int dcr;
	// Coded block pattern
	int coded_block_pattern;

	/* stuff derived from bitstream */

	/* the zigzag scan we're supposed to be using, true for alt, false for normal */
	bool scantype;

	int mpeg1;

	template< typename T >
	void SetOutputTo( T& obj )
	{
		uint mb_offset = ((uptr)&obj - (uptr)&mb8);
		pxAssume( (mb_offset & 15) == 0 );
		ipu0_idx	= mb_offset / 16;
		ipu0_data	= sizeof(obj)/16;
	}

	u128* GetIpuDataPtr()
	{
		return ((u128*)&mb8) + ipu0_idx;
	}

	void AdvanceIpuDataBy(uint amt)
	{
		pxAssertMsg(ipu0_data>=amt, "IPU FIFO Overflow on advance!" );
		ipu0_idx  += amt;
		ipu0_data -= amt;
	}
};

alignas(16) extern decoder_t decoder;
alignas(16) extern tIPU_BP g_BP;

MULTI_ISA_DEF(
	extern void ipu_dither(const macroblock_rgb32& rgb32, macroblock_rgb16& rgb16, int dte);

	void IPUWorker();
)

// Quantization matrix
extern rgb16_t g_ipu_vqclut[16]; //clut conversion table
extern u16 g_ipu_thresh[2]; //thresholds for color conversions

alignas(16) extern u8 g_ipu_indx4[16*16/2];
alignas(16) extern const int non_linear_quantizer_scale[32];
extern int coded_block_pattern;

struct mpeg2_scan_pack
{
	u8 norm[64];
	u8 alt[64];
};

alignas(16) extern const std::array<u8, 1024> g_idct_clip_lut;
alignas(16) extern const mpeg2_scan_pack mpeg2_scan;
