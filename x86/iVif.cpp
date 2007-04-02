#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "Common.h"
#include "ix86/ix86.h"
#include "Vif.h"
#include "VUmicro.h"

#include <assert.h>

extern VIFregisters *_vifRegs;
extern u32* _vifMaskRegs;
extern u32* _vifRow;

#if defined(_MSC_VER) // gcc functions can be found in iVif.S

#include <xmmintrin.h>
#include <emmintrin.h>

// sse2 highly optimized vif (~200 separate functions are built) zerofrog(@gmail.com)
extern u32 g_vif1Masks[48], g_vif0Masks[48];
extern u32 g_vif1HasMask3[4], g_vif0HasMask3[4];

//static const u32 writearr[4] = { 0xffffffff, 0, 0, 0 };
//static const u32 rowarr[4] = { 0, 0xffffffff, 0, 0 };
//static const u32 colarr[4] = { 0, 0, 0xffffffff, 0 };
//static const u32 updatearr[4] = {0xffffffff, 0xffffffff, 0xffffffff, 0 };

// arranged in writearr, rowarr, colarr, updatearr
static PCSX2_ALIGNED16(u32 s_maskarr[16][4]) = {
	0xffffffff, 0x00000000, 0x00000000, 0xffffffff,
	0xffff0000, 0x0000ffff, 0x00000000, 0xffffffff,
	0xffff0000, 0x00000000, 0x0000ffff, 0xffffffff,
	0xffff0000, 0x00000000, 0x00000000, 0xffff0000,
	0x0000ffff, 0xffff0000, 0x00000000, 0xffffffff,
	0x00000000, 0xffffffff, 0x00000000, 0xffffffff,
	0x00000000, 0xffff0000, 0x0000ffff, 0xffffffff,
	0x00000000, 0xffff0000, 0x00000000, 0xffff0000,
	0x0000ffff, 0x00000000, 0xffff0000, 0xffffffff,
	0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff,
	0x00000000, 0x00000000, 0xffffffff, 0xffffffff,
	0x00000000, 0x00000000, 0xffff0000, 0xffff0000,
	0x0000ffff, 0x00000000, 0x00000000, 0x0000ffff,
	0x00000000, 0x0000ffff, 0x00000000, 0x0000ffff,
	0x00000000, 0x00000000, 0x0000ffff, 0x0000ffff,
	0x00000000, 0x00000000, 0x00000000, 0x00000000
};

u8 s_maskwrite[256];
void SetNewMask(u32* vif1masks, u32* hasmask, u32 mask, u32 oldmask)
{
    u32 i;
	u32 prev = 0;
	if( !cpucaps.hasStreamingSIMD2Extensions ) return;
	FreezeXMMRegs(1);

	for(i = 0; i < 4; ++i, mask >>= 8, oldmask >>= 8, vif1masks += 16) {

		prev |= s_maskwrite[mask&0xff];//((mask&3)==3)||((mask&0xc)==0xc)||((mask&0x30)==0x30)||((mask&0xc0)==0xc0);
		hasmask[i] = prev;

		if( (mask&0xff) != (oldmask&0xff) ) {
			__m128i r0, r1, r2, r3;
			r0 = _mm_load_si128((__m128i*)&s_maskarr[mask&15][0]);
			r2 = _mm_unpackhi_epi16(r0, r0);
			r0 = _mm_unpacklo_epi16(r0, r0);

			r1 = _mm_load_si128((__m128i*)&s_maskarr[(mask>>4)&15][0]);
			r3 = _mm_unpackhi_epi16(r1, r1);
			r1 = _mm_unpacklo_epi16(r1, r1);

			_mm_storel_pi((__m64*)&vif1masks[0], *(__m128*)&r0);
			_mm_storel_pi((__m64*)&vif1masks[2], *(__m128*)&r1);
			_mm_storeh_pi((__m64*)&vif1masks[4], *(__m128*)&r0);
			_mm_storeh_pi((__m64*)&vif1masks[6], *(__m128*)&r1);

			_mm_storel_pi((__m64*)&vif1masks[8], *(__m128*)&r2);
			_mm_storel_pi((__m64*)&vif1masks[10], *(__m128*)&r3);
			_mm_storeh_pi((__m64*)&vif1masks[12], *(__m128*)&r2);
			_mm_storeh_pi((__m64*)&vif1masks[14], *(__m128*)&r3);
		}
	}
}

// msvc++
#define VIF_SRC	ecx
#define VIF_INC	edx
#define VIF_DST edi

// writing masks
#define UNPACK_Write0_Regular(r0, CL, DEST_OFFSET, MOVDQA) \
{ \
	__asm MOVDQA qword ptr [VIF_DST+(DEST_OFFSET)], r0 \
} \

#define UNPACK_Write1_Regular(r0, CL, DEST_OFFSET, MOVDQA) \
{ \
	__asm MOVDQA qword ptr [VIF_DST], r0 \
	__asm add VIF_DST, VIF_INC \
} \

#define UNPACK_Write0_Mask UNPACK_Write0_Regular
#define UNPACK_Write1_Mask UNPACK_Write1_Regular

#define UNPACK_Write0_WriteMask(r0, CL, DEST_OFFSET, MOVDQA) \
{ \
	/* masked write (dest needs to be in edi) */ \
	__asm movdqa XMM_WRITEMASK, qword ptr [eax + 64*(CL) + 48] \
	/*__asm maskmovdqu r0, XMM_WRITEMASK*/ \
	__asm pand r0, XMM_WRITEMASK \
	__asm pandn XMM_WRITEMASK, qword ptr [VIF_DST] \
	__asm por r0, XMM_WRITEMASK \
	__asm MOVDQA qword ptr [VIF_DST], r0 \
	__asm add VIF_DST, 16 \
} \

#define UNPACK_Write1_WriteMask(r0, CL, DEST_OFFSET, MOVDQA) \
{ \
	__asm movdqa XMM_WRITEMASK, qword ptr [eax + 64*(0) + 48] \
	/* masked write (dest needs to be in edi) */ \
	/*__asm maskmovdqu r0, XMM_WRITEMASK*/ \
	__asm pand r0, XMM_WRITEMASK \
	__asm pandn XMM_WRITEMASK, qword ptr [VIF_DST] \
	__asm por r0, XMM_WRITEMASK \
	__asm MOVDQA qword ptr [VIF_DST], r0 \
	__asm add VIF_DST, VIF_INC \
} \

#define UNPACK_Mask_SSE_0(r0) \
{ \
	__asm pand r0, XMM_WRITEMASK \
	__asm por r0, XMM_ROWCOLMASK \
} \

// once a qword is uncomprssed, applies masks and saves
// note: modifying XMM_WRITEMASK
#define UNPACK_Mask_SSE_1(r0) \
{ \
	/* dest = row + write (only when mask=0), otherwise write */ \
	__asm pand r0, XMM_WRITEMASK \
	__asm por r0, XMM_ROWCOLMASK \
	__asm pand XMM_WRITEMASK, XMM_ROW \
	__asm paddd r0, XMM_WRITEMASK \
} \

#define UNPACK_Mask_SSE_2(r0) \
{ \
	/* dest = row + write (only when mask=0), otherwise write \
		row = row + write (only when mask = 0), otherwise row */ \
	__asm pand r0, XMM_WRITEMASK \
	__asm pand XMM_WRITEMASK, XMM_ROW \
	__asm paddd XMM_ROW, r0 \
	__asm por r0, XMM_ROWCOLMASK \
	__asm paddd r0, XMM_WRITEMASK \
} \

#define UNPACK_WriteMask_SSE_0 UNPACK_Mask_SSE_0
#define UNPACK_WriteMask_SSE_1 UNPACK_Mask_SSE_1
#define UNPACK_WriteMask_SSE_2 UNPACK_Mask_SSE_2

#define UNPACK_Regular_SSE_0(r0)

#define UNPACK_Regular_SSE_1(r0) \
{ \
	__asm paddd r0, XMM_ROW \
} \

#define UNPACK_Regular_SSE_2(r0) \
{ \
	__asm paddd r0, XMM_ROW \
	__asm movdqa XMM_ROW, r0 \
} \

// setting up masks
#define UNPACK_Setup_Mask_SSE(CL) \
{ \
	__asm mov eax, _vifMaskRegs \
	__asm movdqa XMM_ROWMASK, qword ptr [eax + 64*(CL) + 16] \
	__asm movdqa XMM_ROWCOLMASK, qword ptr [eax + 64*(CL) + 32] \
	__asm movdqa XMM_WRITEMASK, qword ptr [eax + 64*(CL)] \
	__asm pand XMM_ROWMASK, XMM_ROW \
	__asm pand XMM_ROWCOLMASK, XMM_COL \
	__asm por XMM_ROWCOLMASK, XMM_ROWMASK \
} \

#define UNPACK_Start_Setup_Mask_SSE_0(CL) UNPACK_Setup_Mask_SSE(CL);
#define UNPACK_Start_Setup_Mask_SSE_1(CL) \
{ \
	__asm mov eax, _vifMaskRegs \
	__asm movdqa XMM_ROWMASK, qword ptr [eax + 64*(CL) + 16] \
	__asm movdqa XMM_ROWCOLMASK, qword ptr [eax + 64*(CL) + 32] \
	__asm pand XMM_ROWMASK, XMM_ROW \
	__asm pand XMM_ROWCOLMASK, XMM_COL \
	__asm por XMM_ROWCOLMASK, XMM_ROWMASK \
} \

#define UNPACK_Start_Setup_Mask_SSE_2(CL)

#define UNPACK_Setup_Mask_SSE_0_1(CL) 
#define UNPACK_Setup_Mask_SSE_1_1(CL) \
{ \
	__asm mov eax, _vifMaskRegs \
	__asm movdqa XMM_WRITEMASK, qword ptr [eax + 64*(0)] \
} \

#define UNPACK_Setup_Mask_SSE_2_1(CL) { \
	/* ignore CL, since vif.cycle.wl == 1 */ \
	__asm mov eax, _vifMaskRegs \
	__asm movdqa XMM_ROWMASK, qword ptr [eax + 64*(0) + 16] \
	__asm movdqa XMM_ROWCOLMASK, qword ptr [eax + 64*(0) + 32] \
	__asm movdqa XMM_WRITEMASK, qword ptr [eax + 64*(0)] \
	__asm pand XMM_ROWMASK, XMM_ROW \
	__asm pand XMM_ROWCOLMASK, XMM_COL \
	__asm por XMM_ROWCOLMASK, XMM_ROWMASK \
} \

#define UNPACK_Setup_Mask_SSE_0_0(CL) UNPACK_Setup_Mask_SSE(CL)
#define UNPACK_Setup_Mask_SSE_1_0(CL) UNPACK_Setup_Mask_SSE(CL)
#define UNPACK_Setup_Mask_SSE_2_0(CL) UNPACK_Setup_Mask_SSE(CL)

// write mask always destroys XMM_WRITEMASK, so 0_0 = 1_0
#define UNPACK_Setup_WriteMask_SSE_0_0(CL) UNPACK_Setup_Mask_SSE(CL)
#define UNPACK_Setup_WriteMask_SSE_1_0(CL) UNPACK_Setup_Mask_SSE(CL)
#define UNPACK_Setup_WriteMask_SSE_2_0(CL) UNPACK_Setup_Mask_SSE(CL)
#define UNPACK_Setup_WriteMask_SSE_0_1(CL) UNPACK_Setup_Mask_SSE_1_1(CL)
#define UNPACK_Setup_WriteMask_SSE_1_1(CL) UNPACK_Setup_Mask_SSE_1_1(CL)
#define UNPACK_Setup_WriteMask_SSE_2_1(CL) UNPACK_Setup_Mask_SSE_2_1(CL)

#define UNPACK_Start_Setup_WriteMask_SSE_0(CL) UNPACK_Start_Setup_Mask_SSE_1(CL)
#define UNPACK_Start_Setup_WriteMask_SSE_1(CL) UNPACK_Start_Setup_Mask_SSE_1(CL)
#define UNPACK_Start_Setup_WriteMask_SSE_2(CL) UNPACK_Start_Setup_Mask_SSE_2(CL)

#define UNPACK_Start_Setup_Regular_SSE_0(CL)
#define UNPACK_Start_Setup_Regular_SSE_1(CL)
#define UNPACK_Start_Setup_Regular_SSE_2(CL)
#define UNPACK_Setup_Regular_SSE_0_0(CL)
#define UNPACK_Setup_Regular_SSE_1_0(CL)
#define UNPACK_Setup_Regular_SSE_2_0(CL)
#define UNPACK_Setup_Regular_SSE_0_1(CL)
#define UNPACK_Setup_Regular_SSE_1_1(CL)
#define UNPACK_Setup_Regular_SSE_2_1(CL)

#define UNPACK_INC_DST_0_Regular(qw) __asm add VIF_DST, (16*qw)
#define UNPACK_INC_DST_1_Regular(qw)
#define UNPACK_INC_DST_0_Mask(qw) __asm add VIF_DST, (16*qw)
#define UNPACK_INC_DST_1_Mask(qw)
#define UNPACK_INC_DST_0_WriteMask(qw)
#define UNPACK_INC_DST_1_WriteMask(qw)

// unpacks for 1,2,3,4 elements (V3 uses this directly)
#define UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType) { \
	UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+0); \
	UNPACK_##MaskType##_SSE_##ModeType##(XMM_R0); \
	UNPACK_Write##TOTALCL##_##MaskType##(XMM_R0, CL, 0, movdqa); \
	\
	UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+1); \
	UNPACK_##MaskType##_SSE_##ModeType##(XMM_R1); \
	UNPACK_Write##TOTALCL##_##MaskType##(XMM_R1, CL+1, 16, movdqa); \
	\
	UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+2); \
	UNPACK_##MaskType##_SSE_##ModeType##(XMM_R2); \
	UNPACK_Write##TOTALCL##_##MaskType##(XMM_R2, CL+2, 32, movdqa); \
	\
	UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+3); \
	UNPACK_##MaskType##_SSE_##ModeType##(XMM_R3); \
	UNPACK_Write##TOTALCL##_##MaskType##(XMM_R3, CL+3, 48, movdqa); \
	\
	UNPACK_INC_DST_##TOTALCL##_##MaskType##(4) \
} \

// V3 uses this directly
#define UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType) { \
	UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL); \
	UNPACK_##MaskType##_SSE_##ModeType##(XMM_R0); \
	UNPACK_Write##TOTALCL##_##MaskType##(XMM_R0, CL, 0, movdqa); \
	\
	UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+1); \
	UNPACK_##MaskType##_SSE_##ModeType##(XMM_R1); \
	UNPACK_Write##TOTALCL##_##MaskType##(XMM_R1, CL+1, 16, movdqa); \
	\
	UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+2); \
	UNPACK_##MaskType##_SSE_##ModeType##(XMM_R2); \
	UNPACK_Write##TOTALCL##_##MaskType##(XMM_R2, CL+2, 32, movdqa); \
	\
	UNPACK_INC_DST_##TOTALCL##_##MaskType##(3) \
} \

#define UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType) { \
	UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL); \
	UNPACK_##MaskType##_SSE_##ModeType##(XMM_R0); \
	UNPACK_Write##TOTALCL##_##MaskType##(XMM_R0, CL, 0, movdqa); \
	\
	UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+1); \
	UNPACK_##MaskType##_SSE_##ModeType##(XMM_R1); \
	UNPACK_Write##TOTALCL##_##MaskType##(XMM_R1, CL+1, 16, movdqa); \
	\
	UNPACK_INC_DST_##TOTALCL##_##MaskType##(2) \
} \

#define UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType) { \
	UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL); \
	UNPACK_##MaskType##_SSE_##ModeType##(XMM_R0); \
	UNPACK_Write##TOTALCL##_##MaskType##(XMM_R0, CL, 0, movdqa); \
	\
	UNPACK_INC_DST_##TOTALCL##_##MaskType##(1) \
} \

// S-32
// only when cl==1
#define UNPACK_S_32SSE_4x(CL, TOTALCL, MaskType, ModeType, MOVDQA) { \
	{ \
		__asm MOVDQA XMM_R3, qword ptr [VIF_SRC] \
		\
		__asm pshufd XMM_R0, XMM_R3, 0 \
		__asm pshufd XMM_R1, XMM_R3, 0x55 \
		__asm pshufd XMM_R2, XMM_R3, 0xaa \
		__asm pshufd XMM_R3, XMM_R3, 0xff \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 16 \
	} \
}

#define UNPACK_S_32SSE_4A(CL, TOTALCL, MaskType, ModeType) UNPACK_S_32SSE_4x(CL, TOTALCL, MaskType, ModeType, movdqa)
#define UNPACK_S_32SSE_4(CL, TOTALCL, MaskType, ModeType) UNPACK_S_32SSE_4x(CL, TOTALCL, MaskType, ModeType, movdqu)

#define UNPACK_S_32SSE_3x(CL, TOTALCL, MaskType, ModeType, MOVDQA) { \
	{ \
		__asm MOVDQA XMM_R2, qword ptr [VIF_SRC] \
		\
		__asm pshufd XMM_R0, XMM_R2, 0 \
		__asm pshufd XMM_R1, XMM_R2, 0x55 \
		__asm pshufd XMM_R2, XMM_R2, 0xaa \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 12 \
	} \
} \

#define UNPACK_S_32SSE_3A(CL, TOTALCL, MaskType, ModeType) UNPACK_S_32SSE_3x(CL, TOTALCL, MaskType, ModeType, movdqa)
#define UNPACK_S_32SSE_3(CL, TOTALCL, MaskType, ModeType) UNPACK_S_32SSE_3x(CL, TOTALCL, MaskType, ModeType, movdqu)

#define UNPACK_S_32SSE_2(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R1, qword ptr [VIF_SRC] \
		\
		__asm pshufd XMM_R0, XMM_R1, 0 \
		__asm pshufd XMM_R1, XMM_R1, 0x55 \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
} \

#define UNPACK_S_32SSE_2A UNPACK_S_32SSE_2

#define UNPACK_S_32SSE_1(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R0, dword ptr [VIF_SRC] \
		__asm pshufd XMM_R0, XMM_R0, 0 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 4 \
	} \
} \

#define UNPACK_S_32SSE_1A UNPACK_S_32SSE_1

// S-16
#define UNPACK_S_16SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R3, qword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R3, XMM_R3 \
		__asm UNPACK_RIGHTSHIFT XMM_R3, 16 \
		\
		__asm pshufd XMM_R0, XMM_R3, 0 \
		__asm pshufd XMM_R1, XMM_R3, 0x55 \
		__asm pshufd XMM_R2, XMM_R3, 0xaa \
		__asm pshufd XMM_R3, XMM_R3, 0xff \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
}

#define UNPACK_S_16SSE_4A UNPACK_S_16SSE_4

#define UNPACK_S_16SSE_3(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R2, qword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R2, XMM_R2 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
		\
		__asm pshufd XMM_R0, XMM_R2, 0 \
		__asm pshufd XMM_R1, XMM_R2, 0x55 \
		__asm pshufd XMM_R2, XMM_R2, 0xaa \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
	__asm add VIF_SRC, 6 \
	} \
} \

#define UNPACK_S_16SSE_3A UNPACK_S_16SSE_3

#define UNPACK_S_16SSE_2(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R1, dword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R1, XMM_R1 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 16 \
		\
		__asm pshufd XMM_R0, XMM_R1, 0 \
		__asm pshufd XMM_R1, XMM_R1, 0x55 \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 4 \
	} \
} \

#define UNPACK_S_16SSE_2A UNPACK_S_16SSE_2

#define UNPACK_S_16SSE_1(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R0, dword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm pshufd XMM_R0, XMM_R0, 0 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 2 \
	} \
} \

#define UNPACK_S_16SSE_1A UNPACK_S_16SSE_1

// S-8
#define UNPACK_S_8SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R3, dword ptr [VIF_SRC] \
		__asm punpcklbw XMM_R3, XMM_R3 \
		__asm punpcklwd XMM_R3, XMM_R3 \
		__asm UNPACK_RIGHTSHIFT XMM_R3, 24 \
		\
		__asm pshufd XMM_R0, XMM_R3, 0 \
		__asm pshufd XMM_R1, XMM_R3, 0x55 \
		__asm pshufd XMM_R2, XMM_R3, 0xaa \
		__asm pshufd XMM_R3, XMM_R3, 0xff \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 4 \
	} \
}

#define UNPACK_S_8SSE_4A UNPACK_S_8SSE_4

#define UNPACK_S_8SSE_3(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R2, dword ptr [VIF_SRC] \
		__asm punpcklbw XMM_R2, XMM_R2 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 24 \
		\
		__asm pshufd XMM_R0, XMM_R2, 0 \
		__asm pshufd XMM_R1, XMM_R2, 0x55 \
		__asm pshufd XMM_R2, XMM_R2, 0xaa \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 3 \
	} \
} \

#define UNPACK_S_8SSE_3A UNPACK_S_8SSE_3

#define UNPACK_S_8SSE_2(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R1, dword ptr [VIF_SRC] \
		__asm punpcklbw XMM_R1, XMM_R1 \
		__asm punpcklwd XMM_R1, XMM_R1 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 24 \
		\
		__asm pshufd XMM_R0, XMM_R1, 0 \
		__asm pshufd XMM_R1, XMM_R1, 0x55 \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 2 \
	} \
} \

#define UNPACK_S_8SSE_2A UNPACK_S_8SSE_2

#define UNPACK_S_8SSE_1(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R0, dword ptr [VIF_SRC] \
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		__asm pshufd XMM_R0, XMM_R0, 0 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm inc VIF_SRC \
	} \
} \

#define UNPACK_S_8SSE_1A UNPACK_S_8SSE_1

// V2-32
#define UNPACK_V2_32SSE_4A(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm MOVDQA XMM_R0, qword ptr [VIF_SRC] \
		__asm MOVDQA XMM_R2, qword ptr [VIF_SRC+16] \
		\
		__asm pshufd XMM_R1, XMM_R0, 0xee \
		__asm pshufd XMM_R3, XMM_R2, 0xee \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 32 \
	} \
}

#define UNPACK_V2_32SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm movq XMM_R1, qword ptr [VIF_SRC+8] \
		__asm movq XMM_R2, qword ptr [VIF_SRC+16] \
		__asm movq XMM_R3, qword ptr [VIF_SRC+24] \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 32 \
	} \
}

#define UNPACK_V2_32SSE_3A(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm MOVDQA XMM_R0, qword ptr [VIF_SRC] \
		__asm movq XMM_R2, qword ptr [VIF_SRC+16] \
		__asm pshufd XMM_R1, XMM_R0, 0xee \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 24 \
	} \
} \

#define UNPACK_V2_32SSE_3(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm movq XMM_R1, qword ptr [VIF_SRC+8] \
		__asm movq XMM_R2, qword ptr [VIF_SRC+16] \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 24 \
	} \
} \

#define UNPACK_V2_32SSE_2(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm movq XMM_R1, qword ptr [VIF_SRC+8] \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 16 \
	} \
} \

#define UNPACK_V2_32SSE_2A UNPACK_V2_32SSE_2

#define UNPACK_V2_32SSE_1(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
} \

#define UNPACK_V2_32SSE_1A UNPACK_V2_32SSE_1

// V2-16
#define UNPACK_V2_16SSE_4A(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm punpcklwd XMM_R0, qword ptr [VIF_SRC] \
		__asm punpckhwd XMM_R2, qword ptr [VIF_SRC] \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
		\
		/* move the lower 64 bits down*/ \
		__asm pshufd XMM_R1, XMM_R0, 0xee \
		__asm pshufd XMM_R3, XMM_R2, 0xee \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 16 \
	} \
}

#define UNPACK_V2_16SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movdqu XMM_R0, qword ptr [VIF_SRC] \
		\
		__asm punpckhwd XMM_R2, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
		\
		/* move the lower 64 bits down*/ \
		__asm pshufd XMM_R1, XMM_R0, 0xee \
		__asm pshufd XMM_R3, XMM_R2, 0xee \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 16 \
	} \
}

#define UNPACK_V2_16SSE_3A(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm punpcklwd XMM_R0, qword ptr [VIF_SRC] \
		__asm punpckhwd XMM_R2, qword ptr [VIF_SRC] \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
		\
		/* move the lower 64 bits down*/ \
		__asm pshufd XMM_R1, XMM_R0, 0xee \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 12 \
	} \
} \

#define UNPACK_V2_16SSE_3(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movdqu XMM_R0, qword ptr [VIF_SRC] \
		\
		__asm punpckhwd XMM_R2, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
		\
		/* move the lower 64 bits down*/ \
		__asm pshufd XMM_R1, XMM_R0, 0xee \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 12 \
	} \
} \

#define UNPACK_V2_16SSE_2A(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm punpcklwd XMM_R0, qword ptr [VIF_SRC] \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		\
		/* move the lower 64 bits down*/ \
		__asm pshufd XMM_R1, XMM_R0, 0xee \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
} \

#define UNPACK_V2_16SSE_2(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		\
		/* move the lower 64 bits down*/ \
		__asm pshufd XMM_R1, XMM_R0, 0xee \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
} \

#define UNPACK_V2_16SSE_1A(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm punpcklwd XMM_R0, dword ptr [VIF_SRC] \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 4 \
	} \
} \

#define UNPACK_V2_16SSE_1(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movd XMM_R0, dword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 4 \
	} \
} \

// V2-8
#define UNPACK_V2_8SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		\
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm punpckhwd XMM_R2, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 24 \
		\
		/* move the lower 64 bits down*/ \
		__asm pshufd XMM_R1, XMM_R0, 0xee \
		__asm pshufd XMM_R3, XMM_R2, 0xee \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
}

#define UNPACK_V2_8SSE_4A UNPACK_V2_8SSE_4

#define UNPACK_V2_8SSE_3(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		\
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm punpckhwd XMM_R2, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 24 \
		\
		/* move the lower 64 bits down*/ \
		__asm pshufd XMM_R1, XMM_R0, 0xee \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 6 \
	} \
} \

#define UNPACK_V2_8SSE_3A UNPACK_V2_8SSE_3

#define UNPACK_V2_8SSE_2(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R0, dword ptr [VIF_SRC] \
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		\
		/* move the lower 64 bits down*/ \
		__asm pshufd XMM_R1, XMM_R0, 0xee \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 4 \
	} \
} \

#define UNPACK_V2_8SSE_2A UNPACK_V2_8SSE_2

#define UNPACK_V2_8SSE_1(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R0, dword ptr [VIF_SRC] \
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 2 \
	} \
} \

#define UNPACK_V2_8SSE_1A UNPACK_V2_8SSE_1

// V3-32
#define UNPACK_V3_32SSE_4x(CL, TOTALCL, MaskType, ModeType, MOVDQA) { \
	{ \
		__asm MOVDQA XMM_R0, qword ptr [VIF_SRC] \
		__asm movdqu XMM_R1, qword ptr [VIF_SRC+12] \
	} \
	{ \
		UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+0); \
		UNPACK_##MaskType##_SSE_##ModeType##(XMM_R0); \
		UNPACK_Write##TOTALCL##_##MaskType##(XMM_R0, CL, 0, movdqa); \
		\
		UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+1); \
		UNPACK_##MaskType##_SSE_##ModeType##(XMM_R1); \
		UNPACK_Write##TOTALCL##_##MaskType##(XMM_R1, CL+1, 16, movdqa); \
	} \
	{ \
        /* midnight club 2 crashes because reading a qw at +36 is out of bounds */ \
        __asm MOVDQA XMM_R3, qword ptr [VIF_SRC+32] \
		__asm movdqu XMM_R2, qword ptr [VIF_SRC+24] \
		__asm psrldq XMM_R3, 4 \
	} \
	{ \
		UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+2); \
		UNPACK_##MaskType##_SSE_##ModeType##(XMM_R2); \
		UNPACK_Write##TOTALCL##_##MaskType##(XMM_R2, CL+2, 32, movdqa); \
		\
		UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+3); \
		UNPACK_##MaskType##_SSE_##ModeType##(XMM_R3); \
		UNPACK_Write##TOTALCL##_##MaskType##(XMM_R3, CL+3, 48, movdqa); \
		\
		UNPACK_INC_DST_##TOTALCL##_##MaskType##(4) \
	} \
	{ \
		__asm add VIF_SRC, 48 \
	} \
}

#define UNPACK_V3_32SSE_4A(CL, TOTALCL, MaskType, ModeType) UNPACK_V3_32SSE_4x(CL, TOTALCL, MaskType, ModeType, movdqa)
#define UNPACK_V3_32SSE_4(CL, TOTALCL, MaskType, ModeType) UNPACK_V3_32SSE_4x(CL, TOTALCL, MaskType, ModeType, movdqu)

#define UNPACK_V3_32SSE_3x(CL, TOTALCL, MaskType, ModeType, MOVDQA) \
{ \
	{ \
		__asm MOVDQA XMM_R0, qword ptr [VIF_SRC] \
		__asm movdqu XMM_R1, qword ptr [VIF_SRC+12] \
	} \
	{ \
		UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL); \
		UNPACK_##MaskType##_SSE_##ModeType##(XMM_R0); \
		UNPACK_Write##TOTALCL##_##MaskType##(XMM_R0, CL, 0, movdqa); \
		\
		UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+1); \
		UNPACK_##MaskType##_SSE_##ModeType##(XMM_R1); \
		UNPACK_Write##TOTALCL##_##MaskType##(XMM_R1, CL+1, 16, movdqa); \
	} \
	{ \
		__asm movdqu XMM_R2, qword ptr [VIF_SRC+24] \
	} \
	{ \
		UNPACK_Setup_##MaskType##_SSE_##ModeType##_##TOTALCL##(CL+2); \
		UNPACK_##MaskType##_SSE_##ModeType##(XMM_R2); \
		UNPACK_Write##TOTALCL##_##MaskType##(XMM_R2, CL+2, 32, movdqa); \
		\
		UNPACK_INC_DST_##TOTALCL##_##MaskType##(3) \
	} \
	{ \
		__asm add VIF_SRC, 36 \
	} \
} \

#define UNPACK_V3_32SSE_3A(CL, TOTALCL, MaskType, ModeType) UNPACK_V3_32SSE_3x(CL, TOTALCL, MaskType, ModeType, movdqa)
#define UNPACK_V3_32SSE_3(CL, TOTALCL, MaskType, ModeType) UNPACK_V3_32SSE_3x(CL, TOTALCL, MaskType, ModeType, movdqu)

#define UNPACK_V3_32SSE_2x(CL, TOTALCL, MaskType, ModeType, MOVDQA) \
{ \
	{ \
		__asm MOVDQA XMM_R0, qword ptr [VIF_SRC] \
		__asm movdqu XMM_R1, qword ptr [VIF_SRC+12] \
	} \
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 24 \
	} \
} \

#define UNPACK_V3_32SSE_2A(CL, TOTALCL, MaskType, ModeType) UNPACK_V3_32SSE_2x(CL, TOTALCL, MaskType, ModeType, movdqa)
#define UNPACK_V3_32SSE_2(CL, TOTALCL, MaskType, ModeType) UNPACK_V3_32SSE_2x(CL, TOTALCL, MaskType, ModeType, movdqu)

#define UNPACK_V3_32SSE_1x(CL, TOTALCL, MaskType, ModeType, MOVDQA) \
{ \
	{ \
		__asm MOVDQA XMM_R0, qword ptr [VIF_SRC] \
	} \
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 12 \
	} \
} \

#define UNPACK_V3_32SSE_1A(CL, TOTALCL, MaskType, ModeType) UNPACK_V3_32SSE_1x(CL, TOTALCL, MaskType, ModeType, movdqa)
#define UNPACK_V3_32SSE_1(CL, TOTALCL, MaskType, ModeType) UNPACK_V3_32SSE_1x(CL, TOTALCL, MaskType, ModeType, movdqu)

// V3-16
#define UNPACK_V3_16SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm movq XMM_R1, qword ptr [VIF_SRC+6] \
		\
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm movq XMM_R2, qword ptr [VIF_SRC+12] \
		__asm punpcklwd XMM_R1, XMM_R1 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm movq XMM_R3, qword ptr [VIF_SRC+18] \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 16 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		__asm punpcklwd XMM_R3, XMM_R3 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R3, 16 \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 24 \
	} \
}

#define UNPACK_V3_16SSE_4A UNPACK_V3_16SSE_4

#define UNPACK_V3_16SSE_3(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm movq XMM_R1, qword ptr [VIF_SRC+6] \
		\
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm movq XMM_R2, qword ptr [VIF_SRC+12] \
		__asm punpcklwd XMM_R1, XMM_R1 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R1, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 18 \
	} \
} \

#define UNPACK_V3_16SSE_3A UNPACK_V3_16SSE_3

#define UNPACK_V3_16SSE_2(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm movq XMM_R1, qword ptr [VIF_SRC+6] \
		\
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R1, XMM_R1 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 16 \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 12 \
	} \
} \

#define UNPACK_V3_16SSE_2A UNPACK_V3_16SSE_2

#define UNPACK_V3_16SSE_1(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 6 \
	} \
} \

#define UNPACK_V3_16SSE_1A UNPACK_V3_16SSE_1

// V3-8
#define UNPACK_V3_8SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R1, qword ptr [VIF_SRC] \
		__asm movq XMM_R3, qword ptr [VIF_SRC+6] \
		\
		__asm punpcklbw XMM_R1, XMM_R1 \
		__asm punpcklbw XMM_R3, XMM_R3 \
		__asm punpcklwd XMM_R0, XMM_R1 \
		__asm psrldq XMM_R1, 6 \
		__asm punpcklwd XMM_R2, XMM_R3 \
		__asm psrldq XMM_R3, 6 \
		__asm punpcklwd XMM_R1, XMM_R1 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		__asm punpcklwd XMM_R3, XMM_R3 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R2, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R3, 24 \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 12 \
	} \
}

#define UNPACK_V3_8SSE_4A UNPACK_V3_8SSE_4

#define UNPACK_V3_8SSE_3(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R0, word ptr [VIF_SRC] \
		__asm movd XMM_R1, dword ptr [VIF_SRC+3] \
		\
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm movd XMM_R2, dword ptr [VIF_SRC+6] \
		__asm punpcklbw XMM_R1, XMM_R1 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm punpcklbw XMM_R2, XMM_R2 \
		\
		__asm punpcklwd XMM_R1, XMM_R1 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 24 \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 9 \
	} \
} \

#define UNPACK_V3_8SSE_3A UNPACK_V3_8SSE_3

#define UNPACK_V3_8SSE_2(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R0, dword ptr [VIF_SRC] \
		__asm movd XMM_R1, dword ptr [VIF_SRC+3] \
		\
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm punpcklbw XMM_R1, XMM_R1 \
		\
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R1, XMM_R1 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 24 \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 6 \
	} \
} \

#define UNPACK_V3_8SSE_2A UNPACK_V3_8SSE_2

#define UNPACK_V3_8SSE_1(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R0, dword ptr [VIF_SRC] \
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 3 \
	} \
} \

#define UNPACK_V3_8SSE_1A UNPACK_V3_8SSE_1

// V4-32
#define UNPACK_V4_32SSE_4A(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movdqa XMM_R0, qword ptr [VIF_SRC] \
		__asm movdqa XMM_R1, qword ptr [VIF_SRC+16] \
		__asm movdqa XMM_R2, qword ptr [VIF_SRC+32] \
		__asm movdqa XMM_R3, qword ptr [VIF_SRC+48] \
	} \
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 64 \
	} \
}

#define UNPACK_V4_32SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movdqu XMM_R0, qword ptr [VIF_SRC] \
		__asm movdqu XMM_R1, qword ptr [VIF_SRC+16] \
		__asm movdqu XMM_R2, qword ptr [VIF_SRC+32] \
		__asm movdqu XMM_R3, qword ptr [VIF_SRC+48] \
	} \
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 64 \
	} \
}

#define UNPACK_V4_32SSE_3A(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movdqa XMM_R0, qword ptr [VIF_SRC] \
		__asm movdqa XMM_R1, qword ptr [VIF_SRC+16] \
		__asm movdqa XMM_R2, qword ptr [VIF_SRC+32] \
	} \
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 48 \
	} \
}

#define UNPACK_V4_32SSE_3(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movdqu XMM_R0, qword ptr [VIF_SRC] \
		__asm movdqu XMM_R1, qword ptr [VIF_SRC+16] \
		__asm movdqu XMM_R2, qword ptr [VIF_SRC+32] \
	} \
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 48 \
	} \
}

#define UNPACK_V4_32SSE_2A(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movdqa XMM_R0, qword ptr [VIF_SRC] \
		__asm movdqa XMM_R1, qword ptr [VIF_SRC+16] \
	} \
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 32 \
	} \
}

#define UNPACK_V4_32SSE_2(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movdqu XMM_R0, qword ptr [VIF_SRC] \
		__asm movdqu XMM_R1, qword ptr [VIF_SRC+16] \
	} \
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 32 \
	} \
}

#define UNPACK_V4_32SSE_1A(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movdqa XMM_R0, qword ptr [VIF_SRC] \
	} \
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 16 \
	} \
}

#define UNPACK_V4_32SSE_1(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movdqu XMM_R0, qword ptr [VIF_SRC] \
	} \
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 16 \
	} \
}

// V4-16
#define UNPACK_V4_16SSE_4A(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm punpcklwd XMM_R0, qword ptr [VIF_SRC] \
		__asm punpckhwd XMM_R1, qword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R2, qword ptr [VIF_SRC+16] \
		__asm punpckhwd XMM_R3, qword ptr [VIF_SRC+16] \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R1, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R3, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 32 \
	} \
}

#define UNPACK_V4_16SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movdqu XMM_R0, qword ptr [VIF_SRC] \
		__asm movdqu XMM_R2, qword ptr [VIF_SRC+16] \
		\
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpckhwd XMM_R3, XMM_R2 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R1, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R3, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 32 \
	} \
}

#define UNPACK_V4_16SSE_3A(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm punpcklwd XMM_R0, qword ptr [VIF_SRC] \
		__asm punpckhwd XMM_R1, qword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R2, qword ptr [VIF_SRC+16] \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 24 \
	} \
} \

#define UNPACK_V4_16SSE_3(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movdqu XMM_R0, qword ptr [VIF_SRC] \
		__asm movq XMM_R2, qword ptr [VIF_SRC+16] \
		\
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 16 \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 24 \
	} \
} \

#define UNPACK_V4_16SSE_2A(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm punpcklwd XMM_R0, qword ptr [VIF_SRC] \
		__asm punpckhwd XMM_R1, qword ptr [VIF_SRC] \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 16 \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 16 \
	} \
} \

#define UNPACK_V4_16SSE_2(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm movq XMM_R1, qword ptr [VIF_SRC+8] \
		\
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R1, XMM_R1 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 16 \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 16 \
	} \
} \

#define UNPACK_V4_16SSE_1A(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm punpcklwd XMM_R0, qword ptr [VIF_SRC] \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
} \

#define UNPACK_V4_16SSE_1(CL, TOTALCL, MaskType, ModeType) \
{ \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 16 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
} \

// V4-8
#define UNPACK_V4_8SSE_4A(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm punpcklbw XMM_R0, qword ptr [VIF_SRC] \
		__asm punpckhbw XMM_R2, qword ptr [VIF_SRC] \
		\
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpckhwd XMM_R3, XMM_R2 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R1, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R3, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 24 \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 16 \
	} \
}

#define UNPACK_V4_8SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movdqu XMM_R0, qword ptr [VIF_SRC] \
		\
		__asm punpckhbw XMM_R2, XMM_R0 \
		__asm punpcklbw XMM_R0, XMM_R0 \
		\
		__asm punpckhwd XMM_R3, XMM_R2 \
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R3, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 24 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R1, 24 \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 16 \
	} \
}

#define UNPACK_V4_8SSE_3A(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm punpcklbw XMM_R0, qword ptr [VIF_SRC] \
		__asm punpckhbw XMM_R2, qword ptr [VIF_SRC] \
		\
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R1, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 24 \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 12 \
	} \
} \

#define UNPACK_V4_8SSE_3(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		__asm movd XMM_R2, dword ptr [VIF_SRC+8] \
		\
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm punpcklbw XMM_R2, XMM_R2 \
		\
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R1, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R2, 24 \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 12 \
	} \
} \

#define UNPACK_V4_8SSE_2A(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm punpcklbw XMM_R0, qword ptr [VIF_SRC] \
		\
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R1, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
} \

#define UNPACK_V4_8SSE_2(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movq XMM_R0, qword ptr [VIF_SRC] \
		\
		__asm punpcklbw XMM_R0, XMM_R0 \
		\
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		\
		__asm UNPACK_RIGHTSHIFT XMM_R1, 24 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
} \

#define UNPACK_V4_8SSE_1A(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm punpcklbw XMM_R0, qword ptr [VIF_SRC] \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 4 \
	} \
} \

#define UNPACK_V4_8SSE_1(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm movd XMM_R0, dword ptr [VIF_SRC] \
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm UNPACK_RIGHTSHIFT XMM_R0, 24 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 4 \
	} \
} \

// V4-5
static PCSX2_ALIGNED16(u32 s_TempDecompress[4]) = {0};

#define DECOMPRESS_RGBA(OFFSET) { \
	/* R */ \
	__asm mov bl, al \
	__asm shl bl, 3 \
	__asm mov byte ptr [s_TempDecompress+OFFSET], bl \
	/* G */ \
	__asm mov bx, ax \
	__asm shr bx, 2 \
	__asm and bx, 0xf8 \
	__asm mov byte ptr [s_TempDecompress+OFFSET+1], bl \
	/* B */ \
	__asm mov bx, ax \
	__asm shr bx, 7 \
	__asm and bx, 0xf8 \
	__asm mov byte ptr [s_TempDecompress+OFFSET+2], bl \
	__asm mov bx, ax \
	__asm shr bx, 8 \
	__asm and bx, 0x80 \
	__asm mov byte ptr [s_TempDecompress+OFFSET+3], bl \
} \

#define UNPACK_V4_5SSE_4(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm mov eax, dword ptr [VIF_SRC] \
	} \
	DECOMPRESS_RGBA(0); \
	{ \
		__asm shr eax, 16 \
	} \
	DECOMPRESS_RGBA(4); \
	{ \
		__asm mov ax, word ptr [VIF_SRC+4] \
	} \
	DECOMPRESS_RGBA(8); \
	{ \
		__asm mov ax, word ptr [VIF_SRC+6] \
	} \
	DECOMPRESS_RGBA(12); \
	{ \
		__asm movdqa XMM_R0, qword ptr [s_TempDecompress] \
		\
		__asm punpckhbw XMM_R2, XMM_R0 \
		__asm punpcklbw XMM_R0, XMM_R0 \
		\
		__asm punpckhwd XMM_R3, XMM_R2 \
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		\
		__asm psrld XMM_R0, 24 \
		__asm psrld XMM_R1, 24 \
		__asm psrld XMM_R2, 24 \
		__asm psrld XMM_R3, 24 \
	} \
	\
	UNPACK4_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 8 \
	} \
}

#define UNPACK_V4_5SSE_4A UNPACK_V4_5SSE_4

#define UNPACK_V4_5SSE_3(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm mov eax, dword ptr [VIF_SRC] \
	} \
	DECOMPRESS_RGBA(0); \
	{ \
		__asm shr eax, 16 \
	} \
	DECOMPRESS_RGBA(4); \
	{ \
		__asm mov ax, word ptr [VIF_SRC+4] \
	} \
	DECOMPRESS_RGBA(8); \
	{ \
		__asm movdqa XMM_R0, qword ptr [s_TempDecompress] \
		\
		__asm punpckhbw XMM_R2, XMM_R0 \
		__asm punpcklbw XMM_R0, XMM_R0 \
		\
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R2, XMM_R2 \
		\
		__asm psrld XMM_R0, 24 \
		__asm psrld XMM_R1, 24 \
		__asm psrld XMM_R2, 24 \
	} \
	\
	UNPACK3_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 6 \
	} \
} \

#define UNPACK_V4_5SSE_3A UNPACK_V4_5SSE_3

#define UNPACK_V4_5SSE_2(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm mov eax, dword ptr [VIF_SRC] \
	} \
	DECOMPRESS_RGBA(0); \
	{ \
		__asm shr eax, 16 \
	} \
	DECOMPRESS_RGBA(4); \
	{ \
		__asm movq XMM_R0, qword ptr [s_TempDecompress] \
		\
		__asm punpcklbw XMM_R0, XMM_R0 \
		\
		__asm punpckhwd XMM_R1, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		\
		__asm psrld XMM_R0, 24 \
		__asm psrld XMM_R1, 24 \
	} \
	\
	UNPACK2_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 4 \
	} \
} \

#define UNPACK_V4_5SSE_2A UNPACK_V4_5SSE_2

#define UNPACK_V4_5SSE_1(CL, TOTALCL, MaskType, ModeType) { \
	{ \
		__asm mov ax, word ptr [VIF_SRC] \
	} \
	DECOMPRESS_RGBA(0); \
	{ \
		__asm movd XMM_R0, dword ptr [s_TempDecompress] \
		__asm punpcklbw XMM_R0, XMM_R0 \
		__asm punpcklwd XMM_R0, XMM_R0 \
		\
		__asm psrld XMM_R0, 24 \
	} \
	\
	UNPACK1_SSE(CL, TOTALCL, MaskType, ModeType); \
	{ \
		__asm add VIF_SRC, 2 \
	} \
} \

#define UNPACK_V4_5SSE_1A UNPACK_V4_5SSE_1

#pragma warning(disable:4731)

//#ifdef _DEBUG
#define PUSHESI __asm mov s_saveesi, esi
#define POPESI __asm mov esi, s_saveesi
//#else
//#define PUSHESI
//#define POPESI
//#endif

#define PUSHEDI
#define POPEDI
#define PUSHEBP //__asm mov dword ptr [esp-4], ebp
#define POPEBP //__asm mov ebp, dword ptr [esp-4]

//__asm mov eax, pr0 \
///* load row */ \
//__asm movss XMM_ROW, dword ptr [eax] \
//__asm movss XMM_R1, dword ptr [eax+4] \
//__asm punpckldq XMM_ROW, XMM_R1 \
//__asm movss XMM_R0, dword ptr [eax+8] \
//__asm movss XMM_R1, dword ptr [eax+12] \
//__asm punpckldq XMM_R0, XMM_R1 \
//__asm punpcklqdq XMM_ROW, XMM_R0 \
//\
//__asm mov eax, pc0 \
//__asm movss XMM_R3, dword ptr [eax] \
//__asm movss XMM_R1, dword ptr [eax+4] \
//__asm punpckldq XMM_R3, XMM_R1 \
//__asm movss XMM_R0, dword ptr [eax+8] \
//__asm movss XMM_R1, dword ptr [eax+12] \
//__asm punpckldq XMM_R0, XMM_R1 \
//__asm punpcklqdq XMM_R3, XMM_R0 \

#define SAVE_ROW_REG_BASE { \
	{ \
		/* save the row reg */ \
		__asm mov eax, _vifRow \
		__asm movdqa qword ptr [eax], XMM_ROW \
		__asm mov eax, _vifRegs \
		__asm movss dword ptr [eax+0x100], XMM_ROW \
		__asm psrldq XMM_ROW, 4 \
		__asm movss dword ptr [eax+0x110], XMM_ROW \
		__asm psrldq XMM_ROW, 4 \
		__asm movss dword ptr [eax+0x120], XMM_ROW \
		__asm psrldq XMM_ROW, 4 \
		__asm movss dword ptr [eax+0x130], XMM_ROW \
	} \
} \

#define SAVE_NO_REG

extern int g_nCounters[4];

static int tempcl = 0, incdest;
static int s_saveesi, s_saveinc;

// qsize - bytes of compressed size of 1 decompressed qword
#define defUNPACK_SkippingWrite(name, MaskType, ModeType, qsize, sign, SAVE_ROW_REG) \
int UNPACK_SkippingWrite_##name##_##sign##_##MaskType##_##ModeType##(u32* dest, u32* data, int dmasize) \
{ \
	incdest = ((_vifRegs->cycle.cl - _vifRegs->cycle.wl)<<4); \
	\
	switch( _vifRegs->cycle.wl ) { \
		case 1: \
			{ \
				/*__asm inc dword ptr [g_nCounters] \
				__asm mov eax, dmasize \
				__asm add dword ptr [g_nCounters+4], eax*/ \
				PUSHESI \
				PUSHEDI \
				__asm mov esi, dmasize \
			} \
			UNPACK_Start_Setup_##MaskType##_SSE_##ModeType##(0); \
			{ \
				__asm cmp esi, qsize \
				__asm jl C1_Done3 \
				\
				/* move source and dest */ \
				__asm mov VIF_DST, dest \
				__asm mov VIF_SRC, data \
				__asm mov VIF_INC, incdest \
				__asm add VIF_INC, 16 \
			} \
			\
			/* first align VIF_SRC to 16 bytes */ \
C1_Align16: \
			{ \
				__asm test VIF_SRC, 15 \
				__asm jz C1_UnpackAligned \
			} \
			UNPACK_##name##SSE_1(0, 1, MaskType, ModeType); \
			{ \
				__asm cmp esi, (2*qsize) \
				__asm jl C1_DoneWithDec \
				__asm sub esi, qsize \
				__asm jmp C1_Align16 \
			} \
C1_UnpackAligned: \
			{ \
				__asm cmp esi, (2*qsize) \
				__asm jl C1_Unpack1 \
				__asm cmp esi, (3*qsize) \
				__asm jl C1_Unpack2 \
				__asm cmp esi, (4*qsize) \
				__asm jl C1_Unpack3 \
				__asm prefetchnta [eax + 192] \
			} \
C1_Unpack4: \
			UNPACK_##name##SSE_4A(0, 1, MaskType, ModeType); \
			{ \
				__asm cmp esi, (8*qsize) \
				__asm jl C1_DoneUnpack4 \
				__asm sub esi, (4*qsize) \
				__asm jmp C1_Unpack4 \
			} \
C1_DoneUnpack4: \
			{ \
				__asm sub esi, (4*qsize) \
				__asm cmp esi, qsize \
				__asm jl C1_Done3 \
				__asm cmp esi, (2*qsize) \
				__asm jl C1_Unpack1 \
				__asm cmp esi, (3*qsize) \
				__asm jl C1_Unpack2 \
				/* fall through */ \
			} \
C1_Unpack3: \
			UNPACK_##name##SSE_3A(0, 1, MaskType, ModeType); \
			{ \
				__asm sub esi, (3*qsize) \
				__asm jmp C1_Done3 \
			} \
C1_Unpack2: \
			UNPACK_##name##SSE_2A(0, 1, MaskType, ModeType); \
			{ \
				__asm sub esi, (2*qsize) \
				__asm jmp C1_Done3 \
			} \
C1_Unpack1: \
			UNPACK_##name##SSE_1A(0, 1, MaskType, ModeType); \
C1_DoneWithDec: \
			{ \
				__asm sub esi, qsize \
			} \
C1_Done3: \
			{ \
				POPEDI \
				__asm mov dmasize, esi \
				POPESI \
			} \
			SAVE_ROW_REG; \
			return dmasize; \
		case 2: \
			{ \
				/*__asm inc dword ptr [g_nCounters+4]*/ \
				PUSHESI \
				PUSHEDI \
				__asm mov VIF_INC, incdest \
				__asm mov esi, dmasize \
				__asm cmp esi, (2*qsize) \
				/* move source and dest */ \
				__asm mov VIF_DST, dest \
				__asm mov VIF_SRC, data \
				__asm jl C2_Done3 \
			} \
C2_Unpack: \
			UNPACK_##name##SSE_2(0, 0, MaskType, ModeType); \
 \
			{ \
				__asm add VIF_DST, VIF_INC /* take into account wl */ \
				__asm cmp esi, (4*qsize) \
				__asm jl C2_Done2 \
				__asm sub esi, (2*qsize) \
				__asm jmp C2_Unpack /* unpack next */ \
			} \
C2_Done2: \
			{ \
				__asm sub esi, (2*qsize) \
			} \
C2_Done3: \
			{ \
				__asm cmp esi, qsize \
				__asm jl C2_Done4 \
			} \
			/* execute left over qw */ \
			UNPACK_##name##SSE_1(0, 0, MaskType, ModeType); \
			{ \
				__asm sub esi, qsize \
			} \
C2_Done4: \
			{ \
				POPEDI \
				__asm mov dmasize, esi \
				POPESI \
			} \
			SAVE_ROW_REG; \
			return dmasize; \
 \
		case 3: \
			{ \
				/*__asm inc dword ptr [g_nCounters+8]*/ \
				PUSHESI \
				PUSHEDI \
				__asm mov VIF_INC, incdest \
				__asm mov esi, dmasize \
				__asm cmp esi, (3*qsize) \
				/* move source and dest */ \
				__asm mov VIF_DST, dest \
				__asm mov VIF_SRC, data \
				__asm jl C3_Done5 \
			} \
C3_Unpack: \
			UNPACK_##name##SSE_3(0, 0, MaskType, ModeType); \
 \
			{ \
				__asm add VIF_DST, VIF_INC /* take into account wl */ \
				__asm cmp esi, (6*qsize) \
				__asm jl C3_Done2 \
				__asm sub esi, (3*qsize) \
				__asm jmp C3_Unpack /* unpack next */ \
			} \
C3_Done2: \
			{ \
				__asm sub esi, (3*qsize) \
			} \
C3_Done5: \
			{ \
				__asm cmp esi, qsize \
				__asm jl C3_Done4 \
				/* execute left over qw */ \
				__asm cmp esi, (2*qsize) \
				__asm jl C3_Done3 \
			} \
			\
			/* process 2 qws */ \
			UNPACK_##name##SSE_2(0, 0, MaskType, ModeType); \
			{ \
				__asm sub esi, (2*qsize) \
				__asm jmp C3_Done4 \
			} \
 \
C3_Done3: \
			/* process 1 qw */ \
			{ \
				__asm sub esi, qsize \
			} \
			UNPACK_##name##SSE_1(0, 0, MaskType, ModeType); \
C3_Done4: \
			{ \
				POPEDI \
				__asm mov dmasize, esi \
				POPESI \
			} \
			SAVE_ROW_REG; \
			return dmasize; \
 \
		default: /* >= 4 */ \
			tempcl = _vifRegs->cycle.wl-3; \
			{ \
				/*__asm inc dword ptr [g_nCounters+12]*/ \
				PUSHESI \
				PUSHEDI \
				__asm mov VIF_INC, tempcl \
				__asm mov s_saveinc, VIF_INC \
				__asm mov esi, dmasize \
				__asm cmp esi, qsize \
				__asm jl C4_Done \
				/* move source and dest */ \
				__asm mov VIF_DST, dest \
				__asm mov VIF_SRC, data \
			} \
C4_Unpack: \
			{ \
				__asm cmp esi, (3*qsize) \
				__asm jge C4_Unpack3 \
				__asm cmp esi, (2*qsize) \
				__asm jge C4_Unpack2 \
			} \
			UNPACK_##name##SSE_1(0, 0, MaskType, ModeType); \
			/* not enough data left */ \
			{ \
				__asm sub esi, qsize \
				__asm jmp C4_Done \
			} \
C4_Unpack2: \
			UNPACK_##name##SSE_2(0, 0, MaskType, ModeType); \
			/* not enough data left */ \
			{ \
				__asm sub esi, (2*qsize) \
				__asm jmp C4_Done \
			} \
C4_Unpack3: \
			UNPACK_##name##SSE_3(0, 0, MaskType, ModeType); \
			{ \
				__asm sub esi, (3*qsize) \
				/* more data left, process 1qw at a time */ \
				__asm mov VIF_INC, s_saveinc \
			} \
C4_UnpackX: \
			{ \
				/* check if any data left */ \
				__asm cmp esi, qsize \
				__asm jl C4_Done \
				\
			} \
			UNPACK_##name##SSE_1(3, 0, MaskType, ModeType); \
			{ \
				__asm sub esi, qsize \
				__asm cmp VIF_INC, 1 \
				__asm je C4_DoneLoop \
				__asm sub VIF_INC, 1 \
				__asm jmp C4_UnpackX \
			} \
C4_DoneLoop: \
			{ \
				__asm add VIF_DST, incdest /* take into account wl */ \
				__asm cmp esi, qsize \
				__asm jl C4_Done \
				__asm jmp C4_Unpack /* unpack next */ \
			} \
C4_Done: \
			{ \
				POPEDI \
				__asm mov dmasize, esi \
				POPESI \
			} \
			SAVE_ROW_REG; \
			return dmasize; \
	} \
 \
	return dmasize; \
} \

//{ \
//				/*__asm inc dword ptr [g_nCounters] \
//				__asm mov eax, dmasize \
//				__asm add dword ptr [g_nCounters+4], eax*/ \
//				PUSHESI \
//				PUSHEDI \
//				__asm mov esi, dmasize \
//			} \
//			UNPACK_Start_Setup_##MaskType##_SSE_##ModeType##(0); \
//			{ \
//				__asm cmp esi, qsize \
//				__asm jl C1_Done3 \
//				\
//				/* move source and dest */ \
//				__asm mov VIF_DST, dest \
//				__asm mov VIF_SRC, data \
//				__asm mov VIF_INC, incdest \
//				__asm cmp esi, (2*qsize) \
//				__asm jl C1_Unpack1 \
//				__asm cmp esi, (3*qsize) \
//				__asm jl C1_Unpack2 \
//				__asm imul VIF_INC, 3 \
//				__asm prefetchnta [eax + 192] \
//			} \
//C1_Unpack3: \
//			UNPACK_##name##SSE_3(0, 1, MaskType, ModeType); \
//			{ \
//				__asm add VIF_DST, VIF_INC /* take into account wl */ \
//				__asm cmp esi, (6*qsize) \
//				__asm jl C1_DoneUnpack3 \
//				__asm sub esi, (3*qsize) \
//				__asm jmp C1_Unpack3 \
//			} \
//C1_DoneUnpack3: \
//			{ \
//				__asm sub esi, (3*qsize) \
//				__asm mov VIF_INC, dword ptr [esp] /* restore old ptr */ \
//				__asm cmp esi, (2*qsize) \
//				__asm jl C1_Unpack1 \
//				/* fall through */ \
//			} \
//C1_Unpack2: \
//			UNPACK_##name##SSE_2(0, 1, MaskType, ModeType); \
//			{ \
//				__asm add VIF_DST, VIF_INC \
//				__asm add VIF_DST, VIF_INC \
//				__asm sub esi, (2*qsize) \
//				__asm jmp C1_Done3 \
//			} \
//C1_Unpack1: \
//			UNPACK_##name##SSE_1(0, 1, MaskType, ModeType); \
//			{ \
//				__asm add VIF_DST, VIF_INC /* take into account wl */ \
//				__asm sub esi, qsize \
//			} \
//C1_Done3: \
//			{ \
//				POPEDI \
//				__asm mov dmasize, esi \
//				POPESI \
//			} \
//			SAVE_ROW_REG; \

//while(size >= qsize) {
//	funcP( dest, (u32*)cdata, chans);
//	cdata += ft->qsize;
//	size -= ft->qsize;
//
//	if (vif->cl >= wl) {
//		dest += incdest;
//		vif->cl = 0;
//	}
//	else {
//		dest += 4;
//		vif->cl++;
//	}
//}

#define UNPACK_RIGHTSHIFT psrld
#define defUNPACK_SkippingWrite2(name, qsize) \
	defUNPACK_SkippingWrite(name, Regular, 0, qsize, u, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, Regular, 1, qsize, u, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, Regular, 2, qsize, u, SAVE_ROW_REG_BASE); \
	defUNPACK_SkippingWrite(name, Mask, 0, qsize, u, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, Mask, 1, qsize, u, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, Mask, 2, qsize, u, SAVE_ROW_REG_BASE); \
	defUNPACK_SkippingWrite(name, WriteMask, 0, qsize, u, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, WriteMask, 1, qsize, u, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, WriteMask, 2, qsize, u, SAVE_ROW_REG_BASE); \

defUNPACK_SkippingWrite2(S_32, 4);
defUNPACK_SkippingWrite2(S_16, 2);
defUNPACK_SkippingWrite2(S_8, 1);
defUNPACK_SkippingWrite2(V2_32, 8);
defUNPACK_SkippingWrite2(V2_16, 4);
defUNPACK_SkippingWrite2(V2_8, 2);
defUNPACK_SkippingWrite2(V3_32, 12);
defUNPACK_SkippingWrite2(V3_16, 6);
defUNPACK_SkippingWrite2(V3_8, 3);
defUNPACK_SkippingWrite2(V4_32, 16);
defUNPACK_SkippingWrite2(V4_16, 8);
defUNPACK_SkippingWrite2(V4_8, 4);
defUNPACK_SkippingWrite2(V4_5, 2);

#undef UNPACK_RIGHTSHIFT
#undef defUNPACK_SkippingWrite2

#define UNPACK_RIGHTSHIFT psrad
#define defUNPACK_SkippingWrite2(name, qsize) \
	defUNPACK_SkippingWrite(name, Mask, 0, qsize, s, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, Regular, 0, qsize, s, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, Regular, 1, qsize, s, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, Regular, 2, qsize, s, SAVE_ROW_REG_BASE); \
	defUNPACK_SkippingWrite(name, Mask, 1, qsize, s, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, Mask, 2, qsize, s, SAVE_ROW_REG_BASE); \
	defUNPACK_SkippingWrite(name, WriteMask, 0, qsize, s, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, WriteMask, 1, qsize, s, SAVE_NO_REG); \
	defUNPACK_SkippingWrite(name, WriteMask, 2, qsize, s, SAVE_ROW_REG_BASE); \

defUNPACK_SkippingWrite2(S_16, 2);
defUNPACK_SkippingWrite2(S_8, 1);
defUNPACK_SkippingWrite2(V2_16, 4);
defUNPACK_SkippingWrite2(V2_8, 2);
defUNPACK_SkippingWrite2(V3_16, 6);
defUNPACK_SkippingWrite2(V3_8, 3);
defUNPACK_SkippingWrite2(V4_16, 8);
defUNPACK_SkippingWrite2(V4_8, 4);

#undef UNPACK_RIGHTSHIFT
#undef defUNPACK_SkippingWrite2

#endif

#ifdef __cplusplus
}
#endif
