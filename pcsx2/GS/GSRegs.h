/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#pragma once

// clang-format off

// MacOS headers define PAGE_SIZE to the size of an x86 page
#ifdef __APPLE__
	#include <mach/vm_page_size.h>
	#undef PAGE_SIZE
#endif

#define VM_SIZE 4194304u
#define HALF_VM_SIZE (VM_SIZE / 2u)
#define PAGE_SIZE 8192u
#define BLOCK_SIZE 256u
#define COLUMN_SIZE 64u

#define MAX_PAGES (VM_SIZE / PAGE_SIZE)
#define MAX_BLOCKS (VM_SIZE / BLOCK_SIZE)
#define MAX_COLUMNS (VM_SIZE / COLUMN_SIZE)

//if defined, will send much info in reply to the API title info queri from PCSX2
//default should be undefined
//#define GSTITLEINFO_API_FORCE_VERBOSE

#include "GSVector.h"

#pragma pack(push, 1)

enum GS_PRIM
{
	GS_POINTLIST     = 0,
	GS_LINELIST      = 1,
	GS_LINESTRIP     = 2,
	GS_TRIANGLELIST  = 3,
	GS_TRIANGLESTRIP = 4,
	GS_TRIANGLEFAN   = 5,
	GS_SPRITE        = 6,
	GS_INVALID       = 7,
};

enum GS_PRIM_CLASS
{
	GS_POINT_CLASS    = 0,
	GS_LINE_CLASS     = 1,
	GS_TRIANGLE_CLASS = 2,
	GS_SPRITE_CLASS   = 3,
	GS_INVALID_CLASS  = 7,
};

enum GIF_REG
{
	GIF_REG_PRIM    = 0x00,
	GIF_REG_RGBA    = 0x01,
	GIF_REG_STQ     = 0x02,
	GIF_REG_UV      = 0x03,
	GIF_REG_XYZF2   = 0x04,
	GIF_REG_XYZ2    = 0x05,
	GIF_REG_TEX0_1  = 0x06,
	GIF_REG_TEX0_2  = 0x07,
	GIF_REG_CLAMP_1 = 0x08,
	GIF_REG_CLAMP_2 = 0x09,
	GIF_REG_FOG     = 0x0a,
	GIF_REG_INVALID = 0x0b,
	GIF_REG_XYZF3   = 0x0c,
	GIF_REG_XYZ3    = 0x0d,
	GIF_REG_A_D     = 0x0e,
	GIF_REG_NOP     = 0x0f,
};

enum GIF_REG_COMPLEX
{
	GIF_REG_STQRGBAXYZF2 = 0x00,
	GIF_REG_STQRGBAXYZ2 = 0x01,
};

enum GIF_A_D_REG
{
	GIF_A_D_REG_PRIM       = 0x00,
	GIF_A_D_REG_RGBAQ      = 0x01,
	GIF_A_D_REG_ST         = 0x02,
	GIF_A_D_REG_UV         = 0x03,
	GIF_A_D_REG_XYZF2      = 0x04,
	GIF_A_D_REG_XYZ2       = 0x05,
	GIF_A_D_REG_TEX0_1     = 0x06,
	GIF_A_D_REG_TEX0_2     = 0x07,
	GIF_A_D_REG_CLAMP_1    = 0x08,
	GIF_A_D_REG_CLAMP_2    = 0x09,
	GIF_A_D_REG_FOG        = 0x0a,
	GIF_A_D_REG_XYZF3      = 0x0c,
	GIF_A_D_REG_XYZ3       = 0x0d,
	GIF_A_D_REG_NOP        = 0x0f,
	GIF_A_D_REG_TEX1_1     = 0x14,
	GIF_A_D_REG_TEX1_2     = 0x15,
	GIF_A_D_REG_TEX2_1     = 0x16,
	GIF_A_D_REG_TEX2_2     = 0x17,
	GIF_A_D_REG_XYOFFSET_1 = 0x18,
	GIF_A_D_REG_XYOFFSET_2 = 0x19,
	GIF_A_D_REG_PRMODECONT = 0x1a,
	GIF_A_D_REG_PRMODE     = 0x1b,
	GIF_A_D_REG_TEXCLUT    = 0x1c,
	GIF_A_D_REG_SCANMSK    = 0x22,
	GIF_A_D_REG_MIPTBP1_1  = 0x34,
	GIF_A_D_REG_MIPTBP1_2  = 0x35,
	GIF_A_D_REG_MIPTBP2_1  = 0x36,
	GIF_A_D_REG_MIPTBP2_2  = 0x37,
	GIF_A_D_REG_TEXA       = 0x3b,
	GIF_A_D_REG_FOGCOL     = 0x3d,
	GIF_A_D_REG_TEXFLUSH   = 0x3f,
	GIF_A_D_REG_SCISSOR_1  = 0x40,
	GIF_A_D_REG_SCISSOR_2  = 0x41,
	GIF_A_D_REG_ALPHA_1    = 0x42,
	GIF_A_D_REG_ALPHA_2    = 0x43,
	GIF_A_D_REG_DIMX       = 0x44,
	GIF_A_D_REG_DTHE       = 0x45,
	GIF_A_D_REG_COLCLAMP   = 0x46,
	GIF_A_D_REG_TEST_1     = 0x47,
	GIF_A_D_REG_TEST_2     = 0x48,
	GIF_A_D_REG_PABE       = 0x49,
	GIF_A_D_REG_FBA_1      = 0x4a,
	GIF_A_D_REG_FBA_2      = 0x4b,
	GIF_A_D_REG_FRAME_1    = 0x4c,
	GIF_A_D_REG_FRAME_2    = 0x4d,
	GIF_A_D_REG_ZBUF_1     = 0x4e,
	GIF_A_D_REG_ZBUF_2     = 0x4f,
	GIF_A_D_REG_BITBLTBUF  = 0x50,
	GIF_A_D_REG_TRXPOS     = 0x51,
	GIF_A_D_REG_TRXREG     = 0x52,
	GIF_A_D_REG_TRXDIR     = 0x53,
	GIF_A_D_REG_HWREG      = 0x54,
	GIF_A_D_REG_SIGNAL     = 0x60,
	GIF_A_D_REG_FINISH     = 0x61,
	GIF_A_D_REG_LABEL      = 0x62,
};

enum GIF_FLG
{
	GIF_FLG_PACKED  = 0,
	GIF_FLG_REGLIST = 1,
	GIF_FLG_IMAGE   = 2,
	GIF_FLG_IMAGE2  = 3,
};

enum GS_PSM
{
	PSM_PSMCT32  =  0, // 0000-0000
	PSM_PSMCT24  =  1, // 0000-0001
	PSM_PSMCT16  =  2, // 0000-0010
	PSM_PSMCT16S = 10, // 0000-1010
	PSM_PSGPU24  = 18, // 0001-0010
	PSM_PSMT8    = 19, // 0001-0011
	PSM_PSMT4    = 20, // 0001-0100
	PSM_PSMT8H   = 27, // 0001-1011
	PSM_PSMT4HL  = 36, // 0010-0100
	PSM_PSMT4HH  = 44, // 0010-1100
	PSM_PSMZ32   = 48, // 0011-0000
	PSM_PSMZ24   = 49, // 0011-0001
	PSM_PSMZ16   = 50, // 0011-0010
	PSM_PSMZ16S  = 58, // 0011-1010
};

enum GS_TFX
{
	TFX_MODULATE   = 0,
	TFX_DECAL      = 1,
	TFX_HIGHLIGHT  = 2,
	TFX_HIGHLIGHT2 = 3,
	TFX_NONE       = 4,
};

enum GS_CLAMP
{
	CLAMP_REPEAT        = 0,
	CLAMP_CLAMP         = 1,
	CLAMP_REGION_CLAMP  = 2,
	CLAMP_REGION_REPEAT = 3,
};

enum GS_ZTST
{
	ZTST_NEVER   = 0,
	ZTST_ALWAYS  = 1,
	ZTST_GEQUAL  = 2,
	ZTST_GREATER = 3,
};

enum GS_ATST
{
	ATST_NEVER    = 0,
	ATST_ALWAYS   = 1,
	ATST_LESS     = 2,
	ATST_LEQUAL   = 3,
	ATST_EQUAL    = 4,
	ATST_GEQUAL   = 5,
	ATST_GREATER  = 6,
	ATST_NOTEQUAL = 7,
};

enum GS_AFAIL
{
	AFAIL_KEEP     = 0,
	AFAIL_FB_ONLY  = 1,
	AFAIL_ZB_ONLY  = 2,
	AFAIL_RGB_ONLY = 3,
};

enum class GS_MIN_FILTER : uint8_t
{
	Nearest                = 0,
	Linear                 = 1,
	Nearest_Mipmap_Nearest = 2,
	Nearest_Mipmap_Linear  = 3,
	Linear_Mipmap_Nearest  = 4,
	Linear_Mipmap_Linear   = 5,
};

#define REG32(name) \
	union name      \
	{               \
		u32 U32; \
		struct {    \

#define REG64(name)    \
	union name         \
	{                  \
		u64 U64;    \
		u32 U32[2]; \
		void operator = (const GSVector4i& v) {GSVector4i::storel(this, v);} \
		bool operator == (const union name& r) const {return ((GSVector4i)r).eq(*this);} \
		bool operator != (const union name& r) const {return !((GSVector4i)r).eq(*this);} \
		operator GSVector4i() const {return GSVector4i::loadl(this);} \
		struct {

#define REG128(name)   \
	union name         \
	{                  \
		u64 U64[2]; \
		u32 U32[4]; \
		struct {

#define REG32_(prefix, name) REG32(prefix##name)
#define REG64_(prefix, name) REG64(prefix##name)
#define REG128_(prefix, name) REG128(prefix##name)

#define REG_END }; };
#define REG_END2 };

#define REG32_SET(name) \
union name          \
{                   \
	u32 u32;

#define REG64_SET(name) \
	union name          \
	{                   \
		u64 U64;     \
		u32 U32[2];

#define REG128_SET(name) \
	union name           \
	{                    \
		__m128i m128;    \
		u64 U64[2];   \
		u32 U32[4];

#define REG_SET_END };

REG64_(GSReg, BGCOLOR)
	u8 R;
	u8 G;
	u8 B;
	u8 _PAD1[5];
REG_END

REG64_(GSReg, BUSDIR)
	u32 DIR   :  1;
	u32 _PAD1 : 31;
	u32 _PAD2 : 32;
REG_END

REG64_(GSReg, CSR)
	u32 rSIGNAL : 1;
	u32 rFINISH : 1;
	u32 rHSINT  : 1;
	u32 rVSINT  : 1;
	u32 rEDWINT : 1;
	u32 rZERO1  : 1;
	u32 rZERO2  : 1;
	u32 r_PAD1  : 1;
	u32 rFLUSH  : 1;
	u32 rRESET  : 1;
	u32 r_PAD2  : 2;
	u32 rNFIELD : 1;
	u32 rFIELD  : 1;
	u32 rFIFO   : 2;
	u32 rREV    : 8;
	u32 rID     : 8;
	u32 wSIGNAL : 1;
	u32 wFINISH : 1;
	u32 wHSINT  : 1;
	u32 wVSINT  : 1;
	u32 wEDWINT : 1;
	u32 wZERO1  : 1;
	u32 wZERO2  : 1;
	u32 w_PAD1  : 1;
	u32 wFLUSH  : 1;
	u32 wRESET  : 1;
	u32 w_PAD2  : 2;
	u32 wNFIELD : 1;
	u32 wFIELD  : 1;
	u32 wFIFO   : 2;
	u32 wREV    : 8;
	u32 wID     : 8;
REG_END

REG64_(GSReg, DISPFB) // (-1/2)
	u32 FBP   :  9;
	u32 FBW   :  6;
	u32 PSM   :  5;
	u32 _PAD  : 12;
	u32 DBX   : 11;
	u32 DBY   : 11;
	u32 _PAD2 : 10;
REG_END2
	u32 Block() const { return FBP << 5; }
REG_END2

REG64_(GSReg, DISPLAY) // (-1/2)
	u32 DX    : 12;
	u32 DY    : 11;
	u32 MAGH  :  4;
	u32 MAGV  :  2;
	u32 _PAD  :  3;
	u32 DW    : 12;
	u32 DH    : 11;
	u32 _PAD2 :  9;
REG_END

REG64_(GSReg, EXTBUF)
	u32 EXBP  : 14;
	u32 EXBW  :  6;
	u32 FBIN  :  2;
	u32 WFFMD :  1;
	u32 EMODA :  2;
	u32 EMODC :  2;
	u32 _PAD1 :  5;
	u32 WDX   : 11;
	u32 WDY   : 11;
	u32 _PAD2 : 10;
REG_END

REG64_(GSReg, EXTDATA)
	u32 SX    : 12;
	u32 SY    : 11;
	u32 SMPH  :  4;
	u32 SMPV  :  2;
	u32 _PAD1 :  3;
	u32 WW    : 12;
	u32 WH    : 11;
	u32 _PAD2 :  9;
REG_END

REG64_(GSReg, EXTWRITE)
	u32 WRITE :  1;
	u32 _PAD1 : 31;
	u32 _PAD2 : 32;
REG_END

REG64_(GSReg, IMR)
	u32 _PAD1     :  8;
	u32 SIGMSK    :  1;
	u32 FINISHMSK :  1;
	u32 HSMSK     :  1;
	u32 VSMSK     :  1;
	u32 EDWMSK    :  1;
	u32 _PAD2     : 19;
	u32 _PAD3     : 32;
REG_END

REG64_(GSReg, PMODE)
union
{
	struct
	{
		u32 EN1   :  1;
		u32 EN2   :  1;
		u32 CRTMD :  3;
		u32 MMOD  :  1;
		u32 AMOD  :  1;
		u32 SLBG  :  1;
		u32 ALP   :  8;
		u32 _PAD  : 16;
		u32 _PAD1 : 32;
	};

	struct
	{
		u32 EN    :  2;
		u32 _PAD2 : 30;
		u32 _PAD3 : 32;
	};
};
REG_END

REG64_(GSReg, SIGLBLID)
	u32 SIGID;
	u32 LBLID;
REG_END

REG64_(GSReg, SMODE1)
	u32 RC     :  3;
	u32 LC     :  7;
	u32 T1248  :  2;
	u32 SLCK   :  1;
	u32 CMOD   :  2;
	u32 EX     :  1;
	u32 PRST   :  1;
	u32 SINT   :  1;
	u32 XPCK   :  1;
	u32 PCK2   :  2;
	u32 SPML   :  4;
	u32 GCONT  :  1; // YCrCb
	u32 PHS    :  1;
	u32 PVS    :  1;
	u32 PEHS   :  1;
	u32 PEVS   :  1;
	u32 CLKSEL :  2;
	u32 NVCK   :  1;
	u32 SLCK2  :  1;
	u32 VCKSEL :  2;
	u32 VHP    :  1;
	u32 _PAD1  : 27;
REG_END

/*

// pal

CLKSEL=1 CMOD=3 EX=0 GCONT=0 LC=32 NVCK=1 PCK2=0 PEHS=0 PEVS=0 PHS=0 PRST=1 PVS=0 RC=4 SINT=0 SLCK=0 SLCK2=1 SPML=4 T1248=1 VCKSEL=1 VHP=0 XPCK=0

// ntsc

CLKSEL=1 CMOD=2 EX=0 GCONT=0 LC=32 NVCK=1 PCK2=0 PEHS=0 PEVS=0 PHS=0 PRST=1 PVS=0 RC=4 SINT=0 SLCK=0 SLCK2=1 SPML=4 T1248=1 VCKSEL=1 VHP=0 XPCK=0

// ntsc progressive (SoTC)

CLKSEL=1 CMOD=0 EX=0 GCONT=0 LC=32 NVCK=1 PCK2=0 PEHS=0 PEVS=0 PHS=0 PRST=1 PVS=0 RC=4 SINT=0 SLCK=0 SLCK2=1 SPML=2 T1248=1 VCKSEL=1 VHP=1 XPCK=0

*/

REG64_(GSReg, SMODE2)
	u32 INT   :  1;
	u32 FFMD  :  1;
	u32 DPMS  :  2;
	u32 _PAD2 : 28;
	u32 _PAD3 : 32;
REG_END

REG64_(GSReg, SRFSH)
	u32 _DUMMY;
	// TODO
REG_END

REG64_(GSReg, SYNCH1)
	u32 _DUMMY;
	// TODO
REG_END

REG64_(GSReg, SYNCH2)
	u32 _DUMMY;
	// TODO
REG_END

REG64_(GSReg, SYNCV)
	u32 VFP  : 10; // Vertical Front Porchinterval (?s)
	u32 VFPE : 10; // Vertical Front Porchinterval End (?s)
	u32 VBP  : 12; // Vertical Back Porchinterval (?s)
	u32 VBPE : 10; // Vertical Back Porchinterval End (?s)
	u32 VDP  : 11; // Vertical Differential Phase
	u32 VS   : 11; // Vertical Synchronization Timing
REG_END

REG64_SET(GSReg)
	GSRegBGCOLOR  BGCOLOR;
	GSRegBUSDIR   BUSDIR;
	GSRegCSR      CSR;
	GSRegDISPFB   DISPFB;
	GSRegDISPLAY  DISPLAY;
	GSRegEXTBUF   EXTBUF;
	GSRegEXTDATA  EXTDATA;
	GSRegEXTWRITE EXTWRITE;
	GSRegIMR      IMR;
	GSRegPMODE    PMODE;
	GSRegSIGLBLID SIGLBLID;
	GSRegSMODE1   SMODE1;
	GSRegSMODE2   SMODE2;
REG_SET_END

//
// GIFTag

REG128(GIFTag)
	u32 NLOOP : 15;
	u32 EOP   :  1;
	u32 _PAD1 : 16;
	u32 _PAD2 : 14;
	u32 PRE   :  1;
	u32 PRIM  : 11;
	u32 FLG   :  2; // enum GIF_FLG
	u32 NREG  :  4;
	u64 REGS;
REG_END

// GIFReg

REG64_(GIFReg, ALPHA)
	u32 A     :  2;
	u32 B     :  2;
	u32 C     :  2;
	u32 D     :  2;
	u32 _PAD1 : 24;
	u8 FIX;
	u8 _PAD2[3];
REG_END2
	// opaque => output will be Cs/As
	__forceinline bool IsOpaque() const { return ((A == B || (C == 2 && FIX == 0)) && D == 0) || (A == 0 && B == D && C == 2 && FIX == 0x80); }
	__forceinline bool IsOpaque(int amin, int amax) const { return ((A == B || amax == 0) && D == 0) || (A == 0 && B == D && amin == 0x80 && amax == 0x80); }
	__forceinline bool IsCd() { return (A == B) && (D == 1); }
REG_END2

REG64_(GIFReg, BITBLTBUF)
	u32 SBP   : 14;
	u32 _PAD1 :  2;
	u32 SBW   :  6;
	u32 _PAD2 :  2;
	u32 SPSM  :  6;
	u32 _PAD3 :  2;
	u32 DBP   : 14;
	u32 _PAD4 :  2;
	u32 DBW   :  6;
	u32 _PAD5 :  2;
	u32 DPSM  :  6;
	u32 _PAD6 :  2;
REG_END

REG64_(GIFReg, CLAMP)
union
{
	struct
	{
		u32 WMS   :  2;
		u32 WMT   :  2;
		u32 MINU  : 10;
		u32 MAXU  : 10;
		u32 _PAD1 :  8;
		u32 _PAD2 :  2;
		u32 MAXV  : 10;
		u32 _PAD3 : 20;
	};

	struct
	{
		u64 _PAD4 : 24;
		u64 MINV  : 10;
		u64 _PAD5 : 30;
	};
};
REG_END

REG64_(GIFReg, COLCLAMP)
	u32 CLAMP :  1;
	u32 _PAD1 : 31;
	u32 _PAD2 : 32;
REG_END

REG64_(GIFReg, DIMX)
	s32   DM00 : 3;
	s32 _PAD00 : 1;
	s32   DM01 : 3;
	s32 _PAD01 : 1;
	s32   DM02 : 3;
	s32 _PAD02 : 1;
	s32   DM03 : 3;
	s32 _PAD03 : 1;
	s32   DM10 : 3;
	s32 _PAD10 : 1;
	s32   DM11 : 3;
	s32 _PAD11 : 1;
	s32   DM12 : 3;
	s32 _PAD12 : 1;
	s32   DM13 : 3;
	s32 _PAD13 : 1;
	s32   DM20 : 3;
	s32 _PAD20 : 1;
	s32   DM21 : 3;
	s32 _PAD21 : 1;
	s32   DM22 : 3;
	s32 _PAD22 : 1;
	s32   DM23 : 3;
	s32 _PAD23 : 1;
	s32   DM30 : 3;
	s32 _PAD30 : 1;
	s32   DM31 : 3;
	s32 _PAD31 : 1;
	s32   DM32 : 3;
	s32 _PAD32 : 1;
	s32   DM33 : 3;
	s32 _PAD33 : 1;
REG_END

REG64_(GIFReg, DTHE)
	u32 DTHE  :  1;
	u32 _PAD1 : 31;
	u32 _PAD2 : 32;
REG_END

REG64_(GIFReg, FBA)
	u32 FBA   :  1;
	u32 _PAD1 : 31;
	u32 _PAD2 : 32;
REG_END

REG64_(GIFReg, FINISH)
	u32 _PAD1[2];
REG_END

REG64_(GIFReg, FOG)
	u8 _PAD1[7];
	u8 F;
REG_END

REG64_(GIFReg, FOGCOL)
	u8 FCR;
	u8 FCG;
	u8 FCB;
	u8 _PAD1[5];
REG_END

REG64_(GIFReg, FRAME)
	u32 FBP   : 9;
	u32 _PAD1 : 7;
	u32 FBW   : 6;
	u32 _PAD2 : 2;
	u32 PSM   : 6;
	u32 _PAD3 : 2;
	u32 FBMSK;
REG_END2
	u32 Block() const { return FBP << 5; }
REG_END2

REG64_(GIFReg, HWREG)
	u32 DATA_LOWER;
	u32 DATA_UPPER;
REG_END

REG64_(GIFReg, LABEL)
	u32 ID;
	u32 IDMSK;
REG_END

REG64_(GIFReg, MIPTBP1)
	u64 TBP1 : 14;
	u64 TBW1 :  6;
	u64 TBP2 : 14;
	u64 TBW2 :  6;
	u64 TBP3 : 14;
	u64 TBW3 :  6;
	u64 _PAD :  4;
REG_END

REG64_(GIFReg, MIPTBP2)
	u64 TBP4 : 14;
	u64 TBW4 :  6;
	u64 TBP5 : 14;
	u64 TBW5 :  6;
	u64 TBP6 : 14;
	u64 TBW6 :  6;
	u64 _PAD :  4;
REG_END

REG64_(GIFReg, NOP)
	u32 _PAD[2];
REG_END

REG64_(GIFReg, PABE)
	u32 PABE  :  1;
	u32 _PAD1 : 31;
	u32 _PAD2 : 32;
REG_END

REG64_(GIFReg, PRIM)
	u32 PRIM  :  3;
	u32 IIP   :  1;
	u32 TME   :  1;
	u32 FGE   :  1;
	u32 ABE   :  1;
	u32 AA1   :  1;
	u32 FST   :  1;
	u32 CTXT  :  1;
	u32 FIX   :  1;
	u32 _PAD1 : 21;
	u32 _PAD2 : 32;
REG_END

REG64_(GIFReg, PRMODE)
	u32 _PRIM :  3;
	u32 IIP   :  1;
	u32 TME   :  1;
	u32 FGE   :  1;
	u32 ABE   :  1;
	u32 AA1   :  1;
	u32 FST   :  1;
	u32 CTXT  :  1;
	u32 FIX   :  1;
	u32 _PAD2 : 21;
	u32 _PAD3 : 32;
REG_END

REG64_(GIFReg, PRMODECONT)
	u32 AC    :  1;
	u32 _PAD1 : 31;
	u32 _PAD2 : 32;
REG_END

REG64_(GIFReg, RGBAQ)
	u8 R;
	u8 G;
	u8 B;
	u8 A;
	float Q;
REG_END

REG64_(GIFReg, SCANMSK)
	u32 MSK   :  2;
	u32 _PAD1 : 30;
	u32 _PAD2 : 32;
REG_END

REG64_(GIFReg, SCISSOR)
	u32 SCAX0 : 11;
	u32 _PAD1 :  5;
	u32 SCAX1 : 11;
	u32 _PAD2 :  5;
	u32 SCAY0 : 11;
	u32 _PAD3 :  5;
	u32 SCAY1 : 11;
	u32 _PAD4 :  5;
REG_END

REG64_(GIFReg, SIGNAL)
	u32 ID;
	u32 IDMSK;
REG_END

REG64_(GIFReg, ST)
	float S;
	float T;
REG_END

REG64_(GIFReg, TEST)
	u32 ATE   :  1;
	u32 ATST  :  3;
	u32 AREF  :  8;
	u32 AFAIL :  2;
	u32 DATE  :  1;
	u32 DATM  :  1;
	u32 ZTE   :  1;
	u32 ZTST  :  2;
	u32 _PAD1 : 13;
	u32 _PAD2 : 32;
REG_END2
	__forceinline bool DoFirstPass() const { return !ATE || ATST != ATST_NEVER; } // not all pixels fail automatically
	__forceinline bool DoSecondPass() const { return ATE && ATST != ATST_ALWAYS && AFAIL != AFAIL_KEEP; } // pixels may fail, write fb/z
	__forceinline bool NoSecondPass() const { return ATE && ATST != ATST_ALWAYS && AFAIL == AFAIL_KEEP; } // pixels may fail, no output
REG_END2

REG64_(GIFReg, TEX0)
union
{
	struct
	{
		u32 TBP0  : 14;
		u32 TBW   :  6;
		u32 PSM   :  6;
		u32 TW    :  4;
		u32 _PAD1 :  2;
		u32 _PAD2 :  2;
		u32 TCC   :  1;
		u32 TFX   :  2;
		u32 CBP   : 14;
		u32 CPSM  :  4;
		u32 CSM   :  1;
		u32 CSA   :  5;
		u32 CLD   :  3;
	};

	struct
	{
		u64 _PAD3 : 30;
		u64 TH    :  4;
		u64 _PAD4 : 30;
	};
};
REG_END2
	__forceinline bool IsRepeating() const
	{
		if (TBW < 2)
		{
			if (PSM == PSM_PSMT8)
				return TW > 7 || TH > 6;
			if (PSM == PSM_PSMT4)
				return TW > 7 || TH > 7;
		}

		// The recast of TBW seems useless but it avoid tons of warning from GCC...
		return ((u32)TBW << 6u) < (1u << TW);
	}
REG_END2

REG64_(GIFReg, TEX1)
	u32 LCM   :  1;
	u32 _PAD1 :  1;
	u32 MXL   :  3;
	u32 MMAG  :  1;
	u32 MMIN  :  3;
	u32 MTBA  :  1;
	u32 _PAD2 :  9;
	u32 L     :  2;
	u32 _PAD3 : 11;
	s32  K    : 12; // 1:7:4
	u32 _PAD4 : 20;
REG_END2
	bool IsMinLinear() const { return (MMIN == 1) || (MMIN & 4); }
	bool IsMagLinear() const { return MMAG; }
REG_END2

REG64_(GIFReg, TEX2)
	u32 _PAD1 : 20;
	u32 PSM   :  6;
	u32 _PAD2 :  6;
	u32 _PAD3 :  5;
	u32 CBP   : 14;
	u32 CPSM  :  4;
	u32 CSM   :  1;
	u32 CSA   :  5;
	u32 CLD   :  3;
REG_END

REG64_(GIFReg, TEXA)
	u8 TA0;
	u8 _PAD1 : 7;
	u8 AEM   : 1;
	u16 _PAD2;
	u8 TA1   : 8;
	u8 _PAD3[3];
REG_END

REG64_(GIFReg, TEXCLUT)
	u32 CBW   :  6;
	u32 COU   :  6;
	u32 COV   : 10;
	u32 _PAD1 : 10;
	u32 _PAD2 : 32;
REG_END

REG64_(GIFReg, TEXFLUSH)
	u32 _PAD1 : 32;
	u32 _PAD2 : 32;
REG_END

REG64_(GIFReg, TRXDIR)
	u32 XDIR  :  2;
	u32 _PAD1 : 30;
	u32 _PAD2 : 32;
REG_END

REG64_(GIFReg, TRXPOS)
	u32 SSAX  : 11;
	u32 _PAD1 :  5;
	u32 SSAY  : 11;
	u32 _PAD2 :  5;
	u32 DSAX  : 11;
	u32 _PAD3 :  5;
	u32 DSAY  : 11;
	u32 DIRY  :  1;
	u32 DIRX  :  1;
	u32 _PAD4 :  3;
REG_END

REG64_(GIFReg, TRXREG)
	u32 RRW   : 12;
	u32 _PAD1 : 20;
	u32 RRH   : 12;
	u32 _PAD2 : 20;
REG_END

// GSState::GIFPackedRegHandlerUV and GSState::GIFRegHandlerUV will make sure that the _PAD1/2 bits are set to zero

REG64_(GIFReg, UV)
	u16 U;
	// u32 _PAD1 : 2;
	u16 V;
	// u32 _PAD2 : 2;
	u32 _PAD3;
REG_END

// GSState::GIFRegHandlerXYOFFSET will make sure that the _PAD1/2 bits are set to zero

REG64_(GIFReg, XYOFFSET)
	u32 OFX; // : 16; u32 _PAD1 : 16;
	u32 OFY; // : 16; u32 _PAD2 : 16;
REG_END

REG64_(GIFReg, XYZ)
	u16 X;
	u16 Y;
	u32 Z;
REG_END

REG64_(GIFReg, XYZF)
	u16 X;
	u16 Y;
	u32 Z : 24;
	u32 F :  8;
REG_END

REG64_(GIFReg, ZBUF)
	u32 ZBP   :  9;
	u32 _PAD1 : 15;
	// u32 PSM   :  4;
	// u32 _PAD2 :  4;
	u32 PSM   :  6;
	u32 _PAD2 :  2;
	u32 ZMSK  :  1;
	u32 _PAD3 : 31;
REG_END2
	u32 Block() const { return ZBP << 5; }
REG_END2

REG64_SET(GIFReg)
	GIFRegALPHA      ALPHA;
	GIFRegBITBLTBUF  BITBLTBUF;
	GIFRegCLAMP      CLAMP;
	GIFRegCOLCLAMP   COLCLAMP;
	GIFRegDIMX       DIMX;
	GIFRegDTHE       DTHE;
	GIFRegFBA        FBA;
	GIFRegFINISH     FINISH;
	GIFRegFOG        FOG;
	GIFRegFOGCOL     FOGCOL;
	GIFRegFRAME      FRAME;
	GIFRegHWREG      HWREG;
	GIFRegLABEL      LABEL;
	GIFRegMIPTBP1    MIPTBP1;
	GIFRegMIPTBP2    MIPTBP2;
	GIFRegNOP        NOP;
	GIFRegPABE       PABE;
	GIFRegPRIM       PRIM;
	GIFRegPRMODE     PRMODE;
	GIFRegPRMODECONT PRMODECONT;
	GIFRegRGBAQ      RGBAQ;
	GIFRegSCANMSK    SCANMSK;
	GIFRegSCISSOR    SCISSOR;
	GIFRegSIGNAL     SIGNAL;
	GIFRegST         ST;
	GIFRegTEST       TEST;
	GIFRegTEX0       TEX0;
	GIFRegTEX1       TEX1;
	GIFRegTEX2       TEX2;
	GIFRegTEXA       TEXA;
	GIFRegTEXCLUT    TEXCLUT;
	GIFRegTEXFLUSH   TEXFLUSH;
	GIFRegTRXDIR     TRXDIR;
	GIFRegTRXPOS     TRXPOS;
	GIFRegTRXREG     TRXREG;
	GIFRegUV         UV;
	GIFRegXYOFFSET   XYOFFSET;
	GIFRegXYZ        XYZ;
	GIFRegXYZF       XYZF;
	GIFRegZBUF       ZBUF;
REG_SET_END

// GIFPacked

REG128_(GIFPacked, PRIM)
	u32 PRIM  : 11;
	u32 _PAD1 : 21;
	u32 _PAD2[3];
REG_END

REG128_(GIFPacked, RGBA)
	u8 R;
	u8 _PAD1[3];
	u8 G;
	u8 _PAD2[3];
	u8 B;
	u8 _PAD3[3];
	u8 A;
	u8 _PAD4[3];
REG_END

REG128_(GIFPacked, STQ)
	float S;
	float T;
	float Q;
	u32 _PAD1 : 32;
REG_END

REG128_(GIFPacked, UV)
	u32 U     : 14;
	u32 _PAD1 : 18;
	u32 V     : 14;
	u32 _PAD2 : 18;
	u32 _PAD3 : 32;
	u32 _PAD4 : 32;
REG_END

REG128_(GIFPacked, XYZF2)
	u16 X;
	u16 _PAD1;
	u16 Y;
	u16 _PAD2;
	u32 _PAD3 :  4;
	u32 Z     : 24;
	u32 _PAD4 :  4;
	u32 _PAD5 :  4;
	u32 F     :  8;
	u32 _PAD6 :  3;
	u32 ADC   :  1;
	u32 _PAD7 : 16;
REG_END2
u32 Skip() const { return U32[3] & 0x8000; }
REG_END2

REG128_(GIFPacked, XYZ2)
	u16 X;
	u16 _PAD1;
	u16 Y;
	u16 _PAD2;
	u32 Z;
	u32 _PAD3 : 15;
	u32 ADC   :  1;
	u32 _PAD4 : 16;
REG_END2
	u32 Skip() const { return U32[3] & 0x8000; }
REG_END2

REG128_(GIFPacked, FOG)
	u32 _PAD1;
	u32 _PAD2;
	u32 _PAD3;
	u32 _PAD4 :  4;
	u32 F     :  8;
	u32 _PAD5 : 20;
REG_END

REG128_(GIFPacked, A_D)
	u64 DATA;
	u8 ADDR : 8; // enum GIF_A_D_REG
	u8 _PAD1[3 + 4];
REG_END

REG128_(GIFPacked, NOP)
	u32 _PAD1;
	u32 _PAD2;
	u32 _PAD3;
	u32 _PAD4;
REG_END

REG128_SET(GIFPackedReg)
	GIFReg          r;
	GIFPackedPRIM   PRIM;
	GIFPackedRGBA   RGBA;
	GIFPackedSTQ    STQ;
	GIFPackedUV     UV;
	GIFPackedXYZF2  XYZF2;
	GIFPackedXYZ2   XYZ2;
	GIFPackedFOG    FOG;
	GIFPackedA_D    A_D;
	GIFPackedNOP    NOP;
REG_SET_END

struct alignas(32) GIFPath
{
	GIFTag tag;
	u32 nloop;
	u32 nreg;
	u32 reg;
	u32 type;
	GSVector4i regs;

	enum
	{
		TYPE_UNKNOWN,
		TYPE_ADONLY,
		TYPE_STQRGBAXYZF2,
		TYPE_STQRGBAXYZ2
	};

	__forceinline void SetTag(const void* mem)
	{
		const GIFTag* RESTRICT src = (const GIFTag*)mem;

		// the compiler has a hard time not reloading every time a field of src is accessed

		u32 a = src->U32[0];
		u32 b = src->U32[1];

		tag.U32[0] = a;
		tag.U32[1] = b;

		nloop = a & 0x7fff;

		if (nloop == 0)
			return;

		GSVector4i v = GSVector4i::loadl(&src->REGS); // REGS not stored to tag.REGS, only into this->regs, restored before saving the state though

		nreg = (b & 0xf0000000) ? (b >> 28) : 16; // src->NREG
		regs = v.upl8(v >> 4) & GSVector4i::x0f(nreg);
		reg = 0;

		type = TYPE_UNKNOWN;

		if (tag.FLG == GIF_FLG_PACKED)
		{
			if (regs.eq8(GSVector4i(0x0e0e0e0e)).mask() == (1 << nreg) - 1)
			{
				type = TYPE_ADONLY;
			}
			else
			{
				switch (nreg)
				{
					case 1:
						break;
					case 2:
						break;
					case 3:
						// many games, TODO: formats mixed with NOPs (xeno2: 040f010f02, 04010f020f, mgs3: 04010f0f02, 0401020f0f, 04010f020f)
						if (regs.U32[0] == 0x00040102)
							type = TYPE_STQRGBAXYZF2;
						// GoW (has other crazy formats, like ...030503050103)
						if (regs.U32[0] == 0x00050102)
							type = TYPE_STQRGBAXYZ2;
						// TODO: common types with UV instead
						break;
					case 4:
						break;
					case 5:
						break;
					case 6:
						break;
					case 7:
						break;
					case 8:
						break;
					case 9:
						// ffx
						if (regs.U32[0] == 0x02040102 && regs.U32[1] == 0x01020401 && regs.U32[2] == 0x00000004)
						{
							type = TYPE_STQRGBAXYZF2;
							nreg = 3;
							nloop *= 3;
						}
						break;
					case 10:
						break;
					case 11:
						break;
					case 12:
						// dq8 (not many, mostly 040102)
						if (regs.U32[0] == 0x02040102 && regs.U32[1] == 0x01020401 && regs.U32[2] == 0x04010204)
						{
							type = TYPE_STQRGBAXYZF2;
							nreg = 3;
							nloop *= 4;
						}
						break;
					case 13:
						break;
					case 14:
						break;
					case 15:
						break;
					case 16:
						break;
					default:
						__assume(0);
				}
			}
		}
	}

	__forceinline u8 GetReg() const
	{
		return regs.U8[reg];
	}

	__forceinline u8 GetReg(u32 index) const
	{
		return regs.U8[index];
	}

	__forceinline bool StepReg()
	{
		if (++reg == nreg)
		{
			reg = 0;

			if (--nloop == 0)
			{
				return false;
			}
		}

		return true;
	}
};

struct GSPrivRegSet
{
	union
	{
		struct
		{
			GSRegPMODE  PMODE;
			u64         _pad1;
			GSRegSMODE1 SMODE1;
			u64         _pad2;
			GSRegSMODE2 SMODE2;
			u64         _pad3;
			GSRegSRFSH  SRFSH;
			u64         _pad4;
			GSRegSYNCH1 SYNCH1;
			u64         _pad5;
			GSRegSYNCH2 SYNCH2;
			u64         _pad6;
			GSRegSYNCV  SYNCV;
			u64         _pad7;
			struct
			{
				GSRegDISPFB  DISPFB;
				u64          _pad1;
				GSRegDISPLAY DISPLAY;
				u64          _pad2;
			} DISP[2];
			GSRegEXTBUF   EXTBUF;
			u64           _pad8;
			GSRegEXTDATA  EXTDATA;
			u64           _pad9;
			GSRegEXTWRITE EXTWRITE;
			u64           _pad10;
			GSRegBGCOLOR  BGCOLOR;
			u64           _pad11;
		};

		u8 _pad12[0x1000];
	};

	union
	{
		struct
		{
			GSRegCSR      CSR;
			u64           _pad13;
			GSRegIMR      IMR;
			u64           _pad14;
			u64           _unk1[4];
			GSRegBUSDIR   BUSDIR;
			u64           _pad15;
			u64           _unk2[6];
			GSRegSIGLBLID SIGLBLID;
			u64           _pad16;
		};

		u8 _pad17[0x1000];
	};

	void Dump(FILE* fp)
	{
		for (int i = 0; i < 2; i++)
		{
			if (!fp)
				return;

			if (i == 0 && !PMODE.EN1)
				continue;
			if (i == 1 && !PMODE.EN2)
				continue;

			fprintf(fp, "DISPFB[%d] BP=%05x BW=%u PSM=%u DBX=%u DBY=%u\n",
				i,
				DISP[i].DISPFB.Block(),
				DISP[i].DISPFB.FBW,
				DISP[i].DISPFB.PSM,
				DISP[i].DISPFB.DBX,
				DISP[i].DISPFB.DBY);

			fprintf(fp, "DISPLAY[%d] DX=%u DY=%u DW=%u DH=%u MAGH=%u MAGV=%u\n",
				i,
				DISP[i].DISPLAY.DX,
				DISP[i].DISPLAY.DY,
				DISP[i].DISPLAY.DW,
				DISP[i].DISPLAY.DH,
				DISP[i].DISPLAY.MAGH,
				DISP[i].DISPLAY.MAGV);
		}

		fprintf(fp, "PMODE EN1=%u EN2=%u CRTMD=%u MMOD=%u AMOD=%u SLBG=%u ALP=%u\n",
			PMODE.EN1,
			PMODE.EN2,
			PMODE.CRTMD,
			PMODE.MMOD,
			PMODE.AMOD,
			PMODE.SLBG,
			PMODE.ALP);

		fprintf(fp, "SMODE1 CLKSEL=%u CMOD=%u EX=%u GCONT=%u LC=%u NVCK=%u PCK2=%u PEHS=%u PEVS=%u PHS=%u PRST=%u PVS=%u RC=%u SINT=%u SLCK=%u SLCK2=%u SPML=%u T1248=%u VCKSEL=%u VHP=%u XPCK=%u\n",
			SMODE1.CLKSEL,
			SMODE1.CMOD,
			SMODE1.EX,
			SMODE1.GCONT,
			SMODE1.LC,
			SMODE1.NVCK,
			SMODE1.PCK2,
			SMODE1.PEHS,
			SMODE1.PEVS,
			SMODE1.PHS,
			SMODE1.PRST,
			SMODE1.PVS,
			SMODE1.RC,
			SMODE1.SINT,
			SMODE1.SLCK,
			SMODE1.SLCK2,
			SMODE1.SPML,
			SMODE1.T1248,
			SMODE1.VCKSEL,
			SMODE1.VHP,
			SMODE1.XPCK);

		fprintf(fp, "SMODE2 INT=%u FFMD=%u DPMS=%u\n",
			SMODE2.INT,
			SMODE2.FFMD,
			SMODE2.DPMS);

		fprintf(fp, "SRFSH %08x_%08x\n",
			SRFSH.U32[0],
			SRFSH.U32[1]);

		fprintf(fp, "SYNCH1 %08x_%08x\n",
			SYNCH1.U32[0],
			SYNCH1.U32[1]);

		fprintf(fp, "SYNCH2 %08x_%08x\n",
			SYNCH2.U32[0],
			SYNCH2.U32[1]);

		fprintf(fp, "SYNCV VBP=%u VBPE=%u VDP=%u VFP=%u VFPE=%u VS=%u\n",
			SYNCV.VBP,
			SYNCV.VBPE,
			SYNCV.VDP,
			SYNCV.VFP,
			SYNCV.VFPE,
			SYNCV.VS);

		fprintf(fp, "CSR %08x_%08x\n",
			CSR.U32[0],
			CSR.U32[1]);

		fprintf(fp, "BGCOLOR B=%u G=%u R=%u\n",
			BGCOLOR.B,
			BGCOLOR.G,
			BGCOLOR.R);

		fprintf(fp, "EXTBUF BP=0x%x BW=%u FBIN=%u WFFMD=%u EMODA=%u EMODC=%u WDX=%u WDY=%u\n",
			EXTBUF.EXBP, EXTBUF.EXBW, EXTBUF.FBIN, EXTBUF.WFFMD,
			EXTBUF.EMODA, EXTBUF.EMODC, EXTBUF.WDX, EXTBUF.WDY);

		fprintf(fp, "EXTDATA SX=%u SY=%u SMPH=%u SMPV=%u WW=%u WH=%u\n",
			EXTDATA.SX, EXTDATA.SY, EXTDATA.SMPH, EXTDATA.SMPV, EXTDATA.WW, EXTDATA.WH);

		fprintf(fp, "EXTWRITE EN=%u\n", EXTWRITE.WRITE);
	}

	void Dump(const std::string& filename)
	{
		FILE* fp = fopen(filename.c_str(), "wt");
		if (fp)
		{
			Dump(fp);
			fclose(fp);
		}
	}
};

#pragma pack(pop)

