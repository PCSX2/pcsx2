/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2005  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <math.h>
#include <string.h>

#include "Common.h"
#include "Vif.h"
#include "VUmicro.h"
#include "VifDma.h" 

#include <assert.h>

#define gif ((DMACh*)&PS2MEM_HW[0xA000])

// Extern variables
extern VIFregisters *_vifRegs;
extern vifStruct *_vif;
extern u32* _vifMaskRegs;
extern u32 g_vifRow0[4], g_vifCol0[4], g_vifRow1[4], g_vifCol1[4];
extern u32* _vifRow;

extern u32 g_vif1Masks[48], g_vif0Masks[48];
extern u32 g_vif1HasMask3[4], g_vif0HasMask3[4];

#if (defined(__i386__) || defined(__x86_64__))
#include <xmmintrin.h>
#include <emmintrin.h>
#endif

// Generic constants
static const unsigned int VIF0intc = 4;
static const unsigned int VIF1intc = 5;
static const unsigned int VIF0dmanum = 0;
static const unsigned int VIF1dmanum = 1;

static int cycles;
extern HANDLE g_hGsEvent;
extern void * memcpy_amd(void *dest, const void *src, size_t n);

typedef void (*UNPACKFUNCTYPE)( u32 *dest, u32 *data );
typedef int  (*UNPACKPARTFUNCTYPE)( u32 *dest, u32 *data, int size );

typedef struct {
	UNPACKFUNCTYPE       funcU;
	UNPACKFUNCTYPE       funcS;
	UNPACKPARTFUNCTYPE   funcUpart;
	UNPACKPARTFUNCTYPE   funcSpart;

	int bsize; // total byte size of compressed data
	int dsize; // byte size of one channel
	int gsize; // repeat count
	int qsize; // bytes of compressed size of 1 decompressed qword
} VIFUnpackFuncTable;

/* block size; data size; group size; qword size; */
#define _UNPACK_TABLE32(name, bsize, dsize, gsize, qsize) \
   { UNPACK_##name,         UNPACK_##name, \
     UNPACK_##name##part,   UNPACK_##name##part, \
	 bsize, dsize, gsize, qsize },

#define _UNPACK_TABLE(name, bsize, dsize, gsize, qsize) \
   { UNPACK_##name##u,      UNPACK_##name##s, \
     UNPACK_##name##upart,  UNPACK_##name##spart, \
	 bsize, dsize, gsize, qsize },

// Main table for function unpacking
static const VIFUnpackFuncTable VIFfuncTable[16] = {
	_UNPACK_TABLE32(S_32, 12, 4, 4, 4)		// 0x0 - S-32
	_UNPACK_TABLE(S_16, 6, 2, 4, 2)			// 0x1 - S-16
	_UNPACK_TABLE(S_8, 3, 1, 4, 1)			// 0x2 - S-8
	{ NULL, NULL, NULL, NULL, 0, 0, 0, 0 },	// 0x3

	_UNPACK_TABLE32(V2_32, 24, 4, 1, 8)		// 0x4 - V2-32
	_UNPACK_TABLE(V2_16, 12, 2, 1, 4)		// 0x5 - V2-16
	_UNPACK_TABLE(V2_8, 6, 1, 1, 2)			// 0x6 - V2-8
	{ NULL, NULL, NULL, NULL, 0, 0, 0, 0 },	// 0x7
	
	_UNPACK_TABLE32(V3_32, 36, 4, 1, 12)	// 0x8 - V3-32
	_UNPACK_TABLE(V3_16, 18, 2, 1, 6)		// 0x9 - V3-16
	_UNPACK_TABLE(V3_8, 9, 1, 1, 3)			// 0xA - V3-8
	{ NULL, NULL, NULL, NULL, 0, 0, 0, 0 },	// 0xB

	_UNPACK_TABLE32(V4_32, 48, 4, 1, 16)	// 0xC - V4-32
	_UNPACK_TABLE(V4_16, 24, 2, 1, 8)		// 0xD - V4-16
	_UNPACK_TABLE(V4_8, 12, 1, 1, 4)		// 0xE - V4-8
	_UNPACK_TABLE32(V4_5, 6, 2, 1, 8)		// 0xF - V4-5
};

#if (defined(__i386__) || defined(__x86_64__))

typedef struct {
	// regular 0, 1, 2; mask 0, 1, 2
	UNPACKPARTFUNCTYPE       funcU[9], funcS[9];
} VIFSSEUnpackTable;

#define DECL_UNPACK_TABLE_SSE(name, sign) \
extern int UNPACK_SkippingWrite_##name##_##sign##_Regular_0(u32* dest, u32* data, int dmasize); \
extern int UNPACK_SkippingWrite_##name##_##sign##_Regular_1(u32* dest, u32* data, int dmasize); \
extern int UNPACK_SkippingWrite_##name##_##sign##_Regular_2(u32* dest, u32* data, int dmasize); \
extern int UNPACK_SkippingWrite_##name##_##sign##_Mask_0(u32* dest, u32* data, int dmasize); \
extern int UNPACK_SkippingWrite_##name##_##sign##_Mask_1(u32* dest, u32* data, int dmasize); \
extern int UNPACK_SkippingWrite_##name##_##sign##_Mask_2(u32* dest, u32* data, int dmasize); \
extern int UNPACK_SkippingWrite_##name##_##sign##_WriteMask_0(u32* dest, u32* data, int dmasize); \
extern int UNPACK_SkippingWrite_##name##_##sign##_WriteMask_1(u32* dest, u32* data, int dmasize); \
extern int UNPACK_SkippingWrite_##name##_##sign##_WriteMask_2(u32* dest, u32* data, int dmasize); \

#define _UNPACK_TABLE_SSE(name, sign) \
	UNPACK_SkippingWrite_##name##_##sign##_Regular_0, \
	UNPACK_SkippingWrite_##name##_##sign##_Regular_1, \
	UNPACK_SkippingWrite_##name##_##sign##_Regular_2, \
	UNPACK_SkippingWrite_##name##_##sign##_Mask_0, \
	UNPACK_SkippingWrite_##name##_##sign##_Mask_1, \
	UNPACK_SkippingWrite_##name##_##sign##_Mask_2, \
	UNPACK_SkippingWrite_##name##_##sign##_WriteMask_0, \
	UNPACK_SkippingWrite_##name##_##sign##_WriteMask_1, \
	UNPACK_SkippingWrite_##name##_##sign##_WriteMask_2 \

#define _UNPACK_TABLE_SSE_NULL \
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL

// Main table for function unpacking
DECL_UNPACK_TABLE_SSE(S_32, u);
DECL_UNPACK_TABLE_SSE(S_16, u);
DECL_UNPACK_TABLE_SSE(S_8, u);
DECL_UNPACK_TABLE_SSE(S_16, s);
DECL_UNPACK_TABLE_SSE(S_8, s);

DECL_UNPACK_TABLE_SSE(V2_32, u);
DECL_UNPACK_TABLE_SSE(V2_16, u);
DECL_UNPACK_TABLE_SSE(V2_8, u);
DECL_UNPACK_TABLE_SSE(V2_16, s);
DECL_UNPACK_TABLE_SSE(V2_8, s);

DECL_UNPACK_TABLE_SSE(V3_32, u);
DECL_UNPACK_TABLE_SSE(V3_16, u);
DECL_UNPACK_TABLE_SSE(V3_8, u);
DECL_UNPACK_TABLE_SSE(V3_16, s);
DECL_UNPACK_TABLE_SSE(V3_8, s);

DECL_UNPACK_TABLE_SSE(V4_32, u);
DECL_UNPACK_TABLE_SSE(V4_16, u);
DECL_UNPACK_TABLE_SSE(V4_8, u);
DECL_UNPACK_TABLE_SSE(V4_16, s);
DECL_UNPACK_TABLE_SSE(V4_8, s);
DECL_UNPACK_TABLE_SSE(V4_5, u);

static const VIFSSEUnpackTable VIFfuncTableSSE[16] = {
	{ _UNPACK_TABLE_SSE(S_32, u), _UNPACK_TABLE_SSE(S_32, u) },
	{ _UNPACK_TABLE_SSE(S_16, u), _UNPACK_TABLE_SSE(S_16, s) },
	{ _UNPACK_TABLE_SSE(S_8, u), _UNPACK_TABLE_SSE(S_8, s) },
	{ _UNPACK_TABLE_SSE_NULL, _UNPACK_TABLE_SSE_NULL  },

	{ _UNPACK_TABLE_SSE(V2_32, u), _UNPACK_TABLE_SSE(V2_32, u) },
	{ _UNPACK_TABLE_SSE(V2_16, u), _UNPACK_TABLE_SSE(V2_16, s) },
	{ _UNPACK_TABLE_SSE(V2_8, u), _UNPACK_TABLE_SSE(V2_8, s) },
	{ _UNPACK_TABLE_SSE_NULL, _UNPACK_TABLE_SSE_NULL },

	{ _UNPACK_TABLE_SSE(V3_32, u), _UNPACK_TABLE_SSE(V3_32, u) },
	{ _UNPACK_TABLE_SSE(V3_16, u), _UNPACK_TABLE_SSE(V3_16, s) },
	{ _UNPACK_TABLE_SSE(V3_8, u), _UNPACK_TABLE_SSE(V3_8, s) },
	{ _UNPACK_TABLE_SSE_NULL, _UNPACK_TABLE_SSE_NULL },

	{ _UNPACK_TABLE_SSE(V4_32, u), _UNPACK_TABLE_SSE(V4_32, u) },
	{ _UNPACK_TABLE_SSE(V4_16, u), _UNPACK_TABLE_SSE(V4_16, s) },
	{ _UNPACK_TABLE_SSE(V4_8, u), _UNPACK_TABLE_SSE(V4_8, s) },
	{ _UNPACK_TABLE_SSE(V4_5, u), _UNPACK_TABLE_SSE(V4_5, u) },
};

#endif


void vif0FLUSH();
void vif1FLUSH();

void vifDmaInit() {
}

__inline static int _limit( int a, int max ) {
   return ( a > max ? max : a );
}

static void VIFunpack(u32 *data, vifCode *v, int size, const unsigned int VIFdmanum) {
	u32 *dest;
	unsigned int unpackType;
	UNPACKFUNCTYPE func;
	UNPACKPARTFUNCTYPE funcP;
	const VIFUnpackFuncTable *ft;
	vifStruct *vif;
	VIFregisters *vifRegs;
	VURegs * VU;
	u8 *cdata = (u8*)data;
	int memsize;

	_mm_prefetch((char*)data, _MM_HINT_NTA);

	if (VIFdmanum == 0) {
		VU = &VU0;
		vif = &vif0;
		vifRegs = vif0Regs;
		memsize = 0x1000;
		assert( v->addr < 0x4000 );
		v->addr &= 0xfff;
	} else {
		VU = &VU1;
		vif = &vif1;
		vifRegs = vif1Regs;
		memsize = 0x4000;
		v->addr &= 0x3fff;
	}

	dest = (u32*)(VU->Mem + v->addr);
  
#ifdef VIF_LOG
	VIF_LOG("VIF%d UNPACK: Mode=%x, v->size=%d, size=%d, v->addr=%x\n", 
            VIFdmanum, v->cmd & 0xf, v->size, size, v->addr );
#endif

/*	if (vifRegs->cycle.cl > vifRegs->cycle.wl) {
	   SysPrintf( "VIF%d UNPACK: Mode=%x, v->size=%d, size=%d, v->addr=%x\n", 
            VIFdmanum, v->cmd & 0xf, v->size, size, v->addr );
	}*/
#ifdef _DEBUG
	if (v->size != size) {
#ifdef VIF_LOG
		VIF_LOG("*PCSX2*: warning v->size != size\n");
#endif
	}
	if ((v->addr+size*4) > memsize) {
		SysPrintf("*PCSX2*: fixme unpack overflow\n");
		SysPrintf( "VIF%d UNPACK: Mode=%x, v->size=%d, size=%d, v->addr=%x\n", 
            VIFdmanum, v->cmd & 0xf, v->size, size, v->addr );
	}
#endif

	if (size == 0) {
		//SysPrintf("*PCSX2*: Unpack with size 0!!\n");
		return;
	}

	// The unpack type
	unpackType = v->cmd & 0xf;

	if(unpackType == 0xC && vifRegs->mode == 0 && !(vifRegs->code & 0x10000000) && vifRegs->cycle.cl == vifRegs->cycle.wl) {
		// v4-32
		memcpy_amd((u8*)dest, cdata, size << 2);
		size = 0;
		return;
	}

	_mm_prefetch((char*)data+128, _MM_HINT_NTA);
	_vifRegs = (VIFregisters*)vifRegs;
	_vifMaskRegs = VIFdmanum ? g_vif1Masks : g_vif0Masks;
    _vif = vif;
	_vifRow = VIFdmanum ? g_vifRow1 : g_vifRow0;

	// Unpacking
	vif->wl = 0; vif->cl = 0;
	//memsize = size;
	size*= 4;
	if (vifRegs->cycle.cl >= vifRegs->cycle.wl) { // skipping write

		if( !(v->addr&0xf) && cpucaps.hasStreamingSIMD2Extensions ) {
			const UNPACKPARTFUNCTYPE* pfn;
			int writemask;
			//static LARGE_INTEGER lbase, lfinal;
			//QueryPerformanceCounter(&lbase);
			u32 oldcycle = -1;
			FreezeXMMRegs(1);

//			u16 tempdata[4] = { 0x8000, 0x7fff, 0x1010, 0xd0d0 };
//			vifRegs->cycle.cl = 4;
//			vifRegs->cycle.wl = 1;
//			SetNewMask(g_vif1Masks, g_vif1HasMask3, 0x3f, ~0x3f);
//			memset(dest, 0xcd, 64*4);
//			VIFfuncTableSSE[1].funcS[6](dest, (u32*)tempdata, 8);

			if( VIFdmanum ) {
				__asm movaps XMM_ROW, qword ptr [g_vifRow1]
				__asm movaps XMM_COL, qword ptr [g_vifCol1]
			}
			else {
				__asm movaps XMM_ROW, qword ptr [g_vifRow0]
				__asm movaps XMM_COL, qword ptr [g_vifCol0]
			}

			if( vifRegs->cycle.cl == 0 || vifRegs->cycle.wl == 0 || (vifRegs->cycle.cl == vifRegs->cycle.wl && !(vifRegs->code&0x10000000)) ) {
				oldcycle = *(u32*)&vifRegs->cycle;
				vifRegs->cycle.cl = vifRegs->cycle.wl = 1;
			}

			pfn = vif->usn ? VIFfuncTableSSE[unpackType].funcU: VIFfuncTableSSE[unpackType].funcS;
			writemask = VIFdmanum ? g_vif1HasMask3[min(vifRegs->cycle.wl-1,3)] : g_vif0HasMask3[min(vifRegs->cycle.wl-1,3)];
			writemask = pfn[(((vifRegs->code & 0x10000000)>>28)<<writemask)*3+vifRegs->mode](dest, (u32*)cdata, size);

			if( oldcycle != -1 ) *(u32*)&vifRegs->cycle = oldcycle;

			// if size is left over, update the src,dst pointers
			if( writemask > 0 ) {
				int left;
				ft = &VIFfuncTable[ unpackType ];
				left = (size-writemask)/ft->qsize;
				cdata += size-writemask;
				dest = (u32*)((u8*)dest + ((left/vifRegs->cycle.wl)*vifRegs->cycle.cl + left%vifRegs->cycle.wl)*16);
			}

			size = writemask;

			//QueryPerformanceCounter(&lfinal);
			//((LARGE_INTEGER*)g_nCounters)->QuadPart += lfinal.QuadPart - lbase.QuadPart;
		}
		else {
			int incdest, wl, chans;
			ft = &VIFfuncTable[ unpackType ];
			// Assigning the normal upack function, the part type is assigned later
			func = vif->usn ? ft->funcU : ft->funcS;
			funcP = vif->usn ? ft->funcUpart : ft->funcSpart;

			incdest = ((vifRegs->cycle.cl - vifRegs->cycle.wl)<<2) + 4;
			wl = vifRegs->cycle.wl-1;
			chans = (ft->qsize/ft->dsize)*ft->gsize;

			//SysPrintf("slow vif\n");

			while (size >= ft->qsize) {
				funcP( dest, (u32*)cdata, chans);
				cdata += ft->qsize;
				size -= ft->qsize;

				if (vif->cl >= wl) {
					dest += incdest;
					vif->cl = 0;
				}
				else {
					dest += 4;
					vif->cl++;
				}
			}
		}

		// used for debugging vif
//		{
//			int i, j;
//			u32* curdest = (u32*)(VU->Mem + v->addr);
//			FILE* ftemp = fopen("temp.txt", "a+");
//			fprintf(ftemp, "%x %x %x\n", vifRegs->code>>24, vifRegs->mode, *(u32*)&vifRegs->cycle);
//
//			
//			for(i = 0; i < memsize; ) {
//				for(j = 0; j <= ((vifRegs->code>>26)&3); ++j) {
//					fprintf(ftemp, "%x ", curdest[j]);
//				}
//				fprintf(ftemp, "\n");
//				curdest += 4*vifRegs->cycle.cl;
//				i += j;
//			}
//			fclose(ftemp);
//		}

#ifdef VIF_LOG
		VIF_LOG("remaining %d\n", size);
#endif

		if( size > 0 ) {
#ifdef VIF_LOG
			VIF_LOG("warning, end with size = %d\n", size);
#endif
			// SSE doesn't handle such small data
			ft = &VIFfuncTable[ unpackType ];
			func = vif->usn ? ft->funcU : ft->funcS;
			funcP = vif->usn ? ft->funcUpart : ft->funcSpart;

			while (size >= ft->dsize) {
				if (vif->cl < vifRegs->cycle.wl) { /* unpack one qword */
					size-= funcP(dest, (u32*)cdata, (size/ft->dsize)*ft->gsize);
					break;
				}
				dest += 4;
				vif->cl++;
				if (vif->cl == vifRegs->cycle.cl) {
		    		vif->cl = 0;
				}
			}
		}
	} else
	if (vifRegs->cycle.cl < vifRegs->cycle.wl) { /* filling write */
#ifdef VIF_LOG
		VIF_LOG("*PCSX2*: filling write\n");
#endif
		ft = &VIFfuncTable[ unpackType ];
		func = vif->usn ? ft->funcU : ft->funcS;
		funcP = vif->usn ? ft->funcUpart : ft->funcSpart;

		SysPrintf("filling write\n");

		while (size >= ft->bsize) {
			if (vif->wl == vifRegs->cycle.wl) {
         		vif->wl = 0;
			}
			func(dest, (u32*)cdata);
			if (vif->wl < vifRegs->cycle.cl) { /* unpack one qword */
				cdata += ft->bsize;
				size -= ft->bsize;
				vif->cl++;
				if (vif->cl == vifRegs->cycle.cl) {
	   	      		vif->cl = 0;
				}
			}
			dest += 4;
			vif->wl++;
		}
	} else {

		// why is this code here?
		assert(0);
//		while ( size >= ft->bsize ) {
//			func(dest, (u32*)cdata);
//			dest += 12;
//			cdata += ft->bsize;
//			size -= ft->bsize;
//			vif->cl++; vif->wl++;
//			if (vif->cl == vifRegs->cycle.cl) {
//				vif->cl = 0;
//			}
//			if (vif->wl == vifRegs->cycle.wl) {
//				vif->wl = 0;
//			}
//		}
//		while (size >= ft->dsize) {
//			if (vif->cl < vifRegs->cycle.wl) { /* unpack one qword */
//				funcP(dest, (u32*)cdata, (size/ft->dsize)*ft->gsize);
//				break;
//			}
//			dest += 4;
//			vif->cl++;
//			if (vif->cl == vifRegs->cycle.cl) {
//            	vif->cl = 0;
//			}
//		}
	}

	if (size > 0) {
#ifdef VIF_LOG
		VIF_LOG("*PCSX2*: warning size(%d) > 0 after unpack\n", size);
#endif
	}
}

static void vuExecMicro( u32 addr, const unsigned int VIFdmanum ) {

	VURegs * VU;
	//void (*_vuExecMicro)();

//	MessageBox(NULL, "3d doesn't work\n", "Query", MB_OK);
//	return;

	if (VIFdmanum == 0) {
		//_vuExecMicro = Cpu->ExecuteVU0Block;
		VU = &VU0;
		vif0FLUSH();
	} else {
		//_vuExecMicro = Cpu->ExecuteVU1Block;
		VU = &VU1;
		vif1FLUSH();
	}

	VU->vifRegs->itop = VU->vifRegs->itops;

	if (VIFdmanum == 1) {
		/* in case we're handling a VIF1 execMicro 
		   set the top with the tops value */
		VU->vifRegs->top = VU->vifRegs->tops;

		/* is DBF flag set in VIF_STAT? */
		if (VU->vifRegs->stat & 0x80) {
			/* it is, so set tops with base + ofst 
			   and clear stat DBF flag */
			VU->vifRegs->tops = VU->vifRegs->base;
			VU->vifRegs->stat &= ~0x80;
		} else {
			/* it is not, so set tops with base
			   and set the stat DBF flag */
			VU->vifRegs->tops = VU->vifRegs->base + VU->vifRegs->ofst;
			VU->vifRegs->stat |= 0x80;
		}
	}

	if (VIFdmanum == 0) {
		vu0ExecMicro(addr);
	} else {
		vu1ExecMicro(addr);
	}
}

void vif0Init() {
	u32 i;
	extern u8 s_maskwrite[256];

	for(i = 0; i < 256; ++i ) {
		s_maskwrite[i] = ((i&3)==3)||((i&0xc)==0xc)||((i&0x30)==0x30)||((i&0xc0)==0xc0);
	}

	SetNewMask(g_vif0Masks, g_vif0HasMask3, 0, 0xffffffff);
}

void vif0FLUSH() {
	int _cycles;
	_cycles = VU0.cycle;

	vu0Finish();
	cycles+= (VU0.cycle - _cycles)*BIAS;
}

void vif0UNPACK(u32 *data) {
	int vifNum;
    int vl, vn;
    int len;

	vif0FLUSH();

    vl = (vif0.cmd     ) & 0x3;
    vn = (vif0.cmd >> 2) & 0x3;
    vif0.tag.addr = (data[0] & 0x3ff) << 4;
    vif0.usn = (data[0] >> 14) & 0x1;
    vifNum = (data[0] >> 16) & 0xff;
    if ( vifNum == 0 ) vifNum = 256;

    if ( vif0Regs->cycle.wl <= vif0Regs->cycle.cl ) {
        len = ((( 32 >> vl ) * ( vn + 1 )) * vifNum + 31) >> 5;
    } else {
        int n = vif0Regs->cycle.cl * (vifNum / vif0Regs->cycle.wl) + 
                _limit( vifNum % vif0Regs->cycle.wl, vif0Regs->cycle.cl );

		len = ( ((( 32 >> vl ) * ( vn + 1 )) * n) + 31 ) >> 5;
    }
   
    vif0.tag.cmd  = vif0.cmd;
    vif0.tag.size = len;
    vif0Regs->offset = 0;
}

void _vif0mpgTransfer(u32 addr, u32 *data, int size) {
/*	SysPrintf("_vif0mpgTransfer addr=%x; size=%x\n", addr, size);
	{
		FILE *f = fopen("vu1.raw", "wb");
		fwrite(data, 1, size*4, f);
		fclose(f);
	}*/
	if (memcmp(VU0.Micro + addr, data, size << 2)) {
		memcpy_amd(VU0.Micro + addr, data, size << 2);
		Cpu->ClearVU0(addr, size);
	}
}

int vif0transferData(u32 *data, int size) {
	int ret=0;

#ifdef VIF_LOG 
	VIF_LOG("VIFtransferData: cmd %x, size %x, vif0.tag.size %x\n", vif0.cmd, size, vif0.tag.size);
#endif
	if ((vif0.cmd & 0x60) == 0x60) { // UNPACK
#ifdef VIF_LOG 
		VIF_LOG("UNPACKData: cmd %x, size %x, vif0.tag.size %x\n", vif0.cmd, size, vif0.tag.size);
#endif
		if (size < vif0.tag.size) {
			VIFunpack(data, &vif0.tag, size, VIF0dmanum);
		//	cycles+= size >> 1;
			vif0.tag.addr += size << 2;
			vif0.tag.size -= size; 
			ret = size;
		} else {
			VIFunpack(data, &vif0.tag, vif0.tag.size, VIF0dmanum);
		//	cycles+= vif0.tag.size >> 1;
			ret = vif0.tag.size;
			vif0.tag.size = 0;
		}
	} else {
		switch (vif0.cmd) {
			case 0x20: // STMASK
				SetNewMask(g_vif0Masks, g_vif0HasMask3, data[0], vif0Regs->mask);
				vif0Regs->mask = data[0];
#ifdef VIF_LOG
				VIF_LOG("STMASK == %x\n", vif0Regs->mask);
#endif
				ret = 1;
                vif0.tag.size = 0;
                break;

            case 0x30: // STROW
			{
                u32* pmem = &vif0Regs->r0+(vif0.tag.addr<<2);
				u32* pmem2 = g_vifRow0+vif0.tag.addr;
				assert( vif0.tag.addr < 4 );
				ret = min(4-vif0.tag.addr, size);
				assert( ret > 0 );
				switch(ret) {
					case 4: pmem[3] = data[3]; pmem2[3] = data[3];
					case 3: pmem[2] = data[2]; pmem2[2] = data[2];
					case 2: pmem[1] = data[1]; pmem2[1] = data[1];
					case 1: pmem[0] = data[0]; pmem2[0] = data[0]; break;
					default: __assume(0);
				}
                vif0.tag.addr += ret;
                vif0.tag.size -= ret;
                break;
			}
            case 0x31: // STCOL
            {
				u32* pmem = &vif0Regs->c0+(vif0.tag.addr<<2);
				u32* pmem2 = g_vifCol0+vif0.tag.addr;
				ret = min(4-vif0.tag.addr, size);
                switch(ret) {
					case 4: pmem[3] = data[3]; pmem2[3] = data[3];
					case 3: pmem[2] = data[2]; pmem2[2] = data[2];
					case 2: pmem[1] = data[1]; pmem2[1] = data[1];
					case 1: pmem[0] = data[0]; pmem2[0] = data[0]; break;
					default: __assume(0);
				}
				vif0.tag.addr += ret;
                vif0.tag.size -= ret;
                break;
			}
            case 0x4A: // MPG
                if (size < vif0.tag.size) {
					_vif0mpgTransfer(vif0.tag.addr, data, size);
                    vif0.tag.addr += size << 2;
                    vif0.tag.size -= size; 
                    ret = size;
                } else {
					_vif0mpgTransfer(vif0.tag.addr, data, vif0.tag.size);
					ret = vif0.tag.size;
                    vif0.tag.size = 0;
                }
                break;
		}
    }
	if (vif0.tag.size <= 0) {
		vif0.cmd = 0;
	}

	return ret;
}

void vif0CMD(u32 *data, int size) {
	int vifNum;

	switch ( vif0.cmd & 0x7F ) {
		case 0x00: // NOP
        	vif0.cmd = 0;
            break;

        case 0x01: // STCYCL
            vif0Regs->cycle.cl =  data[0] & 0xff;
            vif0Regs->cycle.wl = (data[0] >> 8) & 0xff;
            vif0.cmd = 0;
            break;

        case 0x04: // ITOP
            vif0Regs->itops = data[0] & 0x3ff;
            vif0.cmd = 0;
            break;

        case 0x05: // STMOD
            vif0Regs->mode = data[0] & 0x3;
            vif0.cmd = 0;
            break;

        case 0x07: // MARK
            vif0Regs->mark = (u16)data[0];
			vif0Regs->stat |= VIF0_STAT_MRK;
            vif0.cmd = 0;
            break;

        case 0x10: // FLUSHE
			vif0FLUSH();
            vif0.cmd = 0;
            break;

        case 0x14: // MSCAL
        case 0x15: // MSCALF
            vuExecMicro( (u16)( data[0]) << 3, VIF0dmanum );
            vif0.cmd = 0;
            break;

        case 0x17: // MSCNT
            vuExecMicro( -1, VIF0dmanum );
            vif0.cmd = 0;
            break;

        case 0x20: // STMASK
            break;

        case 0x30: // STROW
            vif0.tag.addr = 0;
            vif0.tag.size = 4;
            break;

        case 0x31: // STCOL
            vif0.tag.addr = 0;
            vif0.tag.size = 4;
            break;

        case 0x4A: // MPG
			vif0FLUSH();
            vifNum = (data[0] >> 16) & 0xff;
            if (vifNum == 0) vifNum = 256;
            vif0.tag.addr = (u16)(data[0]) << 3;
            vif0.tag.size = vifNum << 1;
            break;

        default:			
            vif0.cmd = 0;
            if ((vif0Regs->err & 0x6) == 0) {  //Mask Vifcode and DMA tag mismatch errors
				SysPrintf( "UNKNOWN VifCmd: %x\n", vif0.cmd );
            	vif0Regs->stat |= 1 << 13;
            }
            break;
	}
}

int VIF0transfer(u32 *data, int size, int istag) {
	int ret;
	int transferred=vif0.vifstalled ? vif0.irqoffset : 0;
	
#ifdef VIF_LOG 
	VIF_LOG( "VIF0transfer: size %x (vif0.cmd %x)\n", size, vif0.cmd );
#endif

/*	{
		int i;
		for (i=0; i<size; i++) {
#ifdef VIF_LOG 
		   VIF_LOG( "[%d]: %x\n", i, data[i] );
#endif
		}
	}*/

	//vif0.irq = 0;
	
	while (size > 0) {
		
		if (vif0.cmd) {			
			//vif0Regs->stat |= VIF0_STAT_VPS_T;
			ret = vif0transferData(data, size);
			data+= ret; size-= ret;
			transferred+= ret;
			//vif0Regs->stat &= ~VIF0_STAT_VPS_T;
			continue;
		}

		vif0Regs->stat &= ~VIF0_STAT_VPS_W;

		vif0.cmd = (data[0] >> 24);
		vif0Regs->code = data[0];
		if(vif0.irq && vif0.cmd != 0x7/* && size > 1*/) {
			break;
		}

#ifdef VIF_LOG 
		VIF_LOG( "VIFtransfer: cmd %x, num %x, imm %x, size %x\n", vif0.cmd, (data[0] >> 16) & 0xff, data[0] & 0xffff, size );
#endif

		if ((vif0.cmd & 0x80) && !(vif0Regs->err & 0x1)) { //i bit on vifcode and not masked by VIF0_ERR
#ifdef VIF_LOG
			VIF_LOG( "Interrupt on VIFcmd: %x (INTC_MASK = %x)\n", vif0.cmd, psHu32(INTC_MASK) );
#endif
			vif0.irq++;	
		} 

		//vif0Regs->stat |= VIF0_STAT_VPS_D;
		if ((vif0.cmd & 0x60) == 0x60) {
			vif0UNPACK(data);
		} else {
			vif0CMD(data, size);
		}
		//vif0Regs->stat &= ~VIF0_STAT_VPS_D;
		if(vif0.tag.size > 0) vif0Regs->stat |= VIF0_STAT_VPS_W;
		data++; 
		size--;
		transferred++;
		
	}

	if( !vif0.cmd )
		vif0Regs->stat &= ~VIF0_STAT_VPS_W;

	if (vif0.irq > 0) {
		vif0.irq--;
		
		if( istag ) {
			hwIntcIrq(VIF0intc);
			vif0Regs->stat|= VIF0_STAT_INT;
			return -2;
		}

		vif0.irqoffset = transferred%4; // cannot lose the offset

		transferred = transferred >> 2;
		vif0ch->madr+= (transferred << 4);
		vif0ch->qwc-= transferred;
		//SysPrintf("Stall on vif0, FromSPR = %x, Vif0MADR = %x Sif0MADR = %x STADR = %x\n", psHu32(0x1000d010), vif0ch->madr, psHu32(0x1000c010), psHu32(DMAC_STADR));
		hwIntcIrq(VIF0intc);
		vif0Regs->stat|= VIF0_STAT_INT;
		//if(size > 0) SysPrintf("VIF0 Remaining size %x, data %x_%x_%x_%x\n", size, data[3], data[2], data[1], data[0]);
		return -2;
	}

	if( !istag ) {
		transferred = transferred >> 2;
		vif0ch->madr+= (transferred << 4);
		vif0ch->qwc-= transferred;
	}

	return 0;
}

int  _VIF0chain() {
	u32 *pMem;
	u32 qwc = vif0ch->qwc;
	u32 ret;

	if (vif0ch->qwc == 0) return 0;

	pMem = (u32*)dmaGetAddr(vif0ch->madr);
	if (pMem == NULL) {
		SysPrintf("VIF0chain bad madr %x", vif0ch->madr);
		return -1;
	}

	if( vif0.vifstalled ) {
		ret = VIF0transfer(pMem+vif0.irqoffset, vif0ch->qwc*4-vif0.irqoffset, 0);
	}
	else {
		ret = VIF0transfer(pMem, vif0ch->qwc*4, 0);
	}
	cycles+= (qwc-vif0ch->qwc)*BIAS; /* guessing */
	return ret;
}

int _chainVIF0() {
	u32 *ptag;
	int id;
	int done=0;
	int ret;
	
	ptag = (u32*)dmaGetAddr(vif0ch->tadr); //Set memory pointer to TADR
	if (ptag == NULL) {						//Is ptag empty?
		psHu32(DMAC_STAT)|= 1<<15;          //If yes, set BEIS (BUSERR) in DMAC_STAT register
		return -1;						   //Return -1 as an error has occurred
	}
	
	id        = (ptag[0] >> 28) & 0x7; //ID for DmaChain copied from bit 28 of the tag
	vif0ch->qwc  = (u16)ptag[0];       //QWC set to lower 16bits of the tag
	vif0ch->madr = ptag[1];            //MADR = ADDR field
	cycles+=1; // Add 1 cycles from the QW read for the tag
#ifdef VIF_LOG
	VIF_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, madr=%lx, tadr=%lx\n",
			ptag[0], ptag[1], vif0ch->qwc, id, vif0ch->madr, vif0ch->tadr);
#endif

	vif0ch->chcr = ( vif0ch->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 ); //Transfer upper part of tag to CHCR bits 31-15
	// Transfer dma tag if tte is set
	done |= hwDmacSrcChainWithStack(vif0ch, id);
	if (vif0ch->chcr & 0x40) {
		ret = VIF0transfer(ptag+2, 2, 1);  //Transfer Tag
		if (ret == -1)
			return -1;       //There has been an error
		if (ret == -2) {
			vif0.vifstalled = 1;
			return done;        //IRQ set by VIFTransfer
		}
	}
	
	ret = _VIF0chain();											   //Transfers the data set by the switch
	if (ret == -1) { return -1; }									   //There's been an error
	if (ret == -2) { 							   //IRQ has been set by VifTransfer
		vif0.vifstalled = 1;
		return done;
	}

	//if(id == 7)vif0ch->tadr = vif0ch->madr;

	vif0.vifstalled = 0;

	if ((vif0ch->chcr & 0x80) && (ptag[0] >> 31)) {			       //Check TIE bit of CHCR and IRQ bit of tag
#ifdef VIF_LOG
		VIF_LOG( "dmaIrq Set\n" );
#endif
		//SysPrintf("VIF0 TIE\n");
		//SysPrintf( "VIF0dmaIrq Set\n" );
		vif0ch->qwc = 0;
		vif0Regs->stat|= VIF0_STAT_VIS;							   //Set the Tag Interrupt flag of VIF0_STAT
		return 1;												   //End Transfer
	}
	return done;												   //Return Done
}

int  vif0Interrupt() {
	int ret;

#ifdef VIF_LOG 
	VIF_LOG("vif0Interrupt: %8.8x\n", cpuRegs.cycle);
#endif

	// need to check DMAC_CTRL
	if ((vif0ch->chcr & 0x4) && vif0.done == 0 && vif0.vifstalled == 0 ) {

		if( !(psHu32(DMAC_CTRL) & 0x1) ) {
			SysPrintf("vif0 dma masked\n");
			return 0;
		}

		cycles = 0;
		ret = _chainVIF0();
		if (ret != 0) vif0.done = 1;
		INT(0, cycles);
		return 0;
	}
	if(vif0.vifstalled == 1) {
	// vif0.done = 0;
	 return 0;
	}

	// hack?
	vif0.tag.size = 0;
	vif0.cmd = 0;
	vif0Regs->stat &= ~VIF0_STAT_VPS;
	// hack?

	vif0ch->chcr &= ~0x100;
	hwDmacIrq(DMAC_VIF0);
	vif0Regs->stat&= ~0xF000000; // FQC=0
	
	return 1;
}

void dmaVIF0() {
#ifdef VIF_LOG
	VIF_LOG("dmaVIF0 chcr = %lx, madr = %lx, qwc  = %lx\n"
			"        tadr = %lx, asr0 = %lx, asr1 = %lx\n",
			vif0ch->chcr, vif0ch->madr, vif0ch->qwc,
			vif0ch->tadr, vif0ch->asr0, vif0ch->asr1 );
#endif
	/*if (vif0.irq > 0) {
		vif0.irq--;
		hwIntcIrq(VIF0intc);
		return;
	}*/

	cycles = 0;
//	if(vif0ch->qwc > 0) {
//		 _VIF0chain();
//		 INT(0, cycles);
//	}
	vif0Regs->stat|= 0x8000000; // FQC=8

	if (!(vif0ch->chcr & 0x4)) { // Normal Mode 
		_VIF0chain();
		INT(0, cycles);
		FreezeXMMRegs(0);
		FreezeMMXRegs(0);
		return;
	}

	if (_VIF0chain() != 0) {
		INT(0, cycles);
		FreezeXMMRegs(0);
		FreezeMMXRegs(0);
		return;
	}

	// Chain Mode
	vif0.done = 0;
	INT(0, cycles);
	FreezeXMMRegs(0);
	FreezeMMXRegs(0);
}

void vif0Write32(u32 mem, u32 value) {
	if (mem == 0x10003830) { // MARK
		vif0Regs->stat&= ~0x40;
		vif0Regs->mark = value;
	} else
	if (mem == 0x10003c10) { // FBRST
#ifdef VIF_LOG
		VIF_LOG("VIF0_FBRST write32 0x%8.8x\n", value);
#endif
		if (value & 0x1) {
			/* Reset VIF */
			memset(&vif0, 0, sizeof(vif0));
			vif0ch->qwc = 0;
			vif0Regs->err = 0;
			vif0.done = 1;
			//vif0Reset();
			vif0Regs->stat&= ~0x0F000000; // FQC=0
			
		}
		if (value & 0x2) {
			/* Force Break the VIF */
			/* I guess we should stop the VIF dma here 
			   but not 100% sure (linuz) */
			vif0Regs->stat |= VIF0_STAT_VFS;
		}
		if (value & 0x4) {
			/* Stop VIF */
			/* Not completly sure about this, can't remember what game 
			   used this, but 'draining' the VIF helped it, instead of 
			   just stoppin the VIF (linuz) */
			
			vif0Regs->stat |= VIF0_STAT_VSS;
			//dmaVIF0();	// Drain the VIF  --- VIF Stops as not to outstrip dma source (refraction)
			//FreezeXMMRegs(0);
			//FreezeMMXRegs(0);
		}
		if (value & 0x8) {
			int cancel = 0;

			/* Cancel stall, first check if there is a stall to cancel, 
			   and then clear VIF0_STAT VSS|VFS|VIS|INT|ER0|ER1 bits */
			if (vif0Regs->stat & (VIF0_STAT_INT|VIF0_STAT_VSS|VIF0_STAT_VIS|VIF0_STAT_VFS)) {
				cancel = 1;
			}
			vif0Regs->stat &= ~(VIF0_STAT_VSS | VIF0_STAT_VFS | VIF0_STAT_VIS |
								VIF0_STAT_INT | VIF0_STAT_ER0 | VIF0_STAT_ER1);
			if (cancel) {
				//SysPrintf("VIF0 Stall Resume\n");
				if( vif0.vifstalled ) {
					// only reset if no further stalls
					if( _VIF0chain() != -2 ) 
						vif0.vifstalled = 0;
				}			
			}
			INT(0,0);
		}
	} else
	if (mem == 0x10003820) { // ERR
		vif0Regs->err = value;
	}
	else if (mem == 0x10003800) { // STAT
#ifdef VIF_LOG
		VIF_LOG("VIF0_STAT write32 0x%8.8x\n", value);
#endif
//		vif0ch->qwc = 0;
//		vif0.vifstalled = 0;
//		vif0.done = 1;
//		vif0Regs->stat&= ~0x0F000000; // FQC=0
	}
	else if( mem >= 0x10003900 && mem < 0x10003980 ) {
		assert( (mem&0xf) == 0 );
		if( mem < 0x10003940 ) g_vifRow0[(mem>>4)&3] = value;
		else g_vifCol0[(mem>>4)&3] = value;
	}
}

void vif0Reset() {
	/* Reset the whole VIF, meaning the internal pcsx2 vars
	   and all the registers */
	memset(&vif0, 0, sizeof(vif0));
	memset(vif0Regs, 0, sizeof(vif0Regs));

	SetNewMask(g_vif0Masks, g_vif0HasMask3, vif0Regs->mask, ~vif0Regs->mask);
	FreezeXMMRegs(0);
	FreezeMMXRegs(0);
}

int  vif0Freeze(gzFile f, int Mode) {
	gzfreeze(&vif0, sizeof(vif0));
	
	if (Mode == 0)
		SetNewMask(g_vif0Masks, g_vif0HasMask3, vif0Regs->mask, ~vif0Regs->mask);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////


void vif1Init() {
	SetNewMask(g_vif1Masks, g_vif1HasMask3, 0, 0xffffffff);
}

void vif1FLUSH() {
	int _cycles;
	_cycles = VU1.cycle;

	if( VU0.VI[REG_VPU_STAT].UL & 0x100 ) {
		FreezeXMMRegs(1);
		do {
			Cpu->ExecuteVU1Block();
		} while(VU0.VI[REG_VPU_STAT].UL & 0x100);

//		FreezeXMMRegs(0);
//		FreezeMMXRegs(0);

		cycles+= (VU1.cycle - _cycles)*BIAS;
	}
}

void vif1UNPACK(u32 *data) {
	int vifNum;
    int vl, vn;
    int len;

	vif1FLUSH();

    vl = (vif1.cmd     ) & 0x3;
    vn = (vif1.cmd >> 2) & 0x3;
    vif1.tag.addr = (data[0] & 0x3ff);
    vif1.usn = (data[0] >> 14) & 0x1;
    vifNum = (data[0] >> 16) & 0xff;
    if ( vifNum == 0 ) vifNum = 256;

    if ( vif1Regs->cycle.wl <= vif1Regs->cycle.cl ) {
        len = ((( 32 >> vl ) * ( vn + 1 )) * vifNum + 31) >> 5;
    } else {
        int n = vif1Regs->cycle.cl * (vifNum / vif1Regs->cycle.wl) + 
                _limit( vifNum % vif1Regs->cycle.wl, vif1Regs->cycle.cl );
        len = ( ((( 32 >> vl ) * ( vn + 1 )) * n) + 31 ) >> 5;
    }
   if ( ( data[0] >> 15) & 0x1 ) {
        vif1.tag.addr += vif1Regs->tops;
    }    

    vif1.tag.addr <<= 4;
	vif1.tag.addr &= 0x3fff;
    vif1.tag.cmd  = vif1.cmd;
	vif1.tag.size = len;
    vif1Regs->offset = 0;
}

void _vif1mpgTransfer(u32 addr, u32 *data, int size) {
/*	SysPrintf("_vif1mpgTransfer addr=%x; size=%x\n", addr, size);
	{
		FILE *f = fopen("vu1.raw", "wb");
		fwrite(data, 1, size*4, f);
		fclose(f);
	}*/
	if (memcmp(VU1.Micro + addr, data, size << 2)) {
		memcpy_amd(VU1.Micro + addr, data, size << 2);
		Cpu->ClearVU1(addr, size);
	}
}

int vif1transferData(u32 *data, int size) {
	int ret=0;

#ifdef VIF_LOG 
	VIF_LOG("VIFtransferData: cmd %x, size %x, vif1.tag.size %x\n", vif1.cmd, size, vif1.tag.size);
#endif
	if ((vif1.cmd & 0x60) == 0x60) { // UNPACK
#ifdef VIF_LOG 
		VIF_LOG("UNPACKData: cmd %x, size %x, vif1.tag.size %x\n", vif1.cmd, size, vif1.tag.size);
#endif
		if (size < vif1.tag.size) {
			/* size is less that the total size, transfer is 
			   'in pieces' */
			VIFunpack(data, &vif1.tag, size, VIF1dmanum);
		//	cycles+= size >> 1;
			vif1.tag.addr += size << 2;
			vif1.tag.size -= size; 
			ret = size;
		} else {
			/* we got all the data, transfer it fully */
			VIFunpack(data, &vif1.tag, vif1.tag.size, VIF1dmanum);
			//cycles+= vif1.tag.size >> 1;
			ret = vif1.tag.size;
			vif1.tag.size = 0;
		}
	} else {
		switch (vif1.cmd) {
			case 0x20: // STMASK
				SetNewMask(g_vif1Masks, g_vif1HasMask3, data[0], vif1Regs->mask);
				vif1Regs->mask = data[0];
#ifdef VIF_LOG
				VIF_LOG("STMASK == %x\n", vif1Regs->mask);
#endif
				ret = 1;
                vif1.tag.size = 0;
                break;

            case 0x30: // STROW
			{
				u32* pmem = &vif1Regs->r0+(vif1.tag.addr<<2);
				u32* pmem2 = g_vifRow1+vif1.tag.addr;
				assert( vif1.tag.addr < 4 );
				ret = min(4-vif1.tag.addr, size);
				assert( ret > 0 );
				switch(ret) {
					case 4: pmem[12] = data[3]; pmem2[3] = data[3];
					case 3: pmem[8] = data[2]; pmem2[2] = data[2];
					case 2: pmem[4] = data[1]; pmem2[1] = data[1];
					case 1: pmem[0] = data[0]; pmem2[0] = data[0]; break;
					default: __assume(0);
				}
                vif1.tag.addr += ret;
                vif1.tag.size -= ret;
                break;
			}
            case 0x31: // STCOL
			{
				u32* pmem = &vif1Regs->c0+(vif1.tag.addr<<2);
				u32* pmem2 = g_vifCol1+vif1.tag.addr;
				ret = min(4-vif1.tag.addr, size);
                switch(ret) {
					case 4: pmem[12] = data[3]; pmem2[3] = data[3];
					case 3: pmem[8] = data[2]; pmem2[2] = data[2];
					case 2: pmem[4] = data[1]; pmem2[1] = data[1];
					case 1: pmem[0] = data[0]; pmem2[0] = data[0]; break;
					default: __assume(0);
				}
				vif1.tag.addr += ret;
                vif1.tag.size -= ret;
                break;
			}
            case 0x4A: // MPG
                if (size < vif1.tag.size) {
					_vif1mpgTransfer(vif1.tag.addr, data, size);
                    vif1.tag.addr += size << 2;
                    vif1.tag.size -= size; 
                    ret = size;
                } else {
					_vif1mpgTransfer(vif1.tag.addr, data, vif1.tag.size);
					ret = vif1.tag.size;
                    vif1.tag.size = 0;
                }
				
                break;

            case 0x50: // DIRECT
            case 0x51: // DIRECTHL
				
				if (size < vif1.tag.size) {
                    vif1.tag.size-= size;
					ret = size;
                } else {
                    ret = vif1.tag.size;
                    vif1.tag.size = 0;
                }

				if( CHECK_MULTIGS ) {
					u8* gsmem = GSRingBufCopy(data, ret<<2, GS_RINGTYPE_P2);
					if( gsmem != NULL ) {
						memcpy_amd(gsmem, data, ret<<2);
						GSRINGBUF_DONECOPY(gsmem, ret<<2);
						GSgifTransferDummy(1, data, ret>>2);
					}

					if( !CHECK_DUALCORE  )
						SetEvent(g_hGsEvent);
				}
				else {
#ifdef GSCAPTURE
					extern u32 g_loggs, g_gstransnum, g_gsfinalnum;

					if( !g_loggs || (g_loggs && g_gstransnum++ < g_gsfinalnum)) {
						// call directly
						FreezeMMXRegs(1);
						FreezeXMMRegs(1);
						GSgifTransfer2(data, (ret >> 2) - GSgifTransferDummy(1, data, ret>>2));
					}
#else
					FreezeMMXRegs(1);
					FreezeXMMRegs(1);
					GSgifTransfer2(data, (ret >> 2));
#endif
				}
				
				//GSCSRr |= 2; // set finish?

				break;
		}
    }
	if (vif1.tag.size <= 0) {
		vif1.cmd = 0;
	}

	return ret;
}

void vif1CMD(u32 *data, int size) {
	int vifNum;
	int vifImm;

	switch ( vif1.cmd & 0x7F ) {
		case 0x00: // NOP
        	vif1.cmd = 0;
            break;

        case 0x01: // STCYCL
            vif1Regs->cycle.cl =  (u8)data[0];
            vif1Regs->cycle.wl = (u8)(data[0] >> 8);
            vif1.cmd = 0;
            break;

        case 0x02: // OFFSET
            vif1Regs->ofst  = data[0] & 0x3ff;
            vif1Regs->stat &= ~0x80;
            vif1Regs->tops  = vif1Regs->base;
            vif1.cmd = 0;
            break;

        case 0x03: // BASE
            vif1Regs->base = data[0] & 0x3ff;
            vif1.cmd = 0;
            break;

        case 0x04: // ITOP
            vif1Regs->itops = data[0] & 0x3ff;
            vif1.cmd = 0;
            break;

        case 0x05: // STMOD
            vif1Regs->mode = data[0] & 0x3;
            vif1.cmd = 0;
            break;

        case 0x06: // MSKPATH3
            vif1Regs->mskpath3 = (data[0] >> 15) & 0x1; 
            if ( vif1Regs->mskpath3 ) {
				if(gif->qwc) _GIFchain();		// Finish the transfer first
                psHu32(GIF_STAT) |= 0x2;
            } else {
				psHu32(GIF_STAT) &= ~0x2;
				if(gif->qwc) _GIFchain();		// Finish the transfer first
            }
            vif1.cmd = 0;
            break;

        case 0x07: // MARK
            vif1Regs->mark = (u16)data[0];
			vif1Regs->stat |= VIF1_STAT_MRK;
            vif1.cmd = 0;
            break;

        case 0x10: // FLUSHE
        case 0x11: // FLUSH
        case 0x13: // FLUSHA
			vif1FLUSH();
            vif1.cmd = 0;
            break;

        case 0x14: // MSCAL
        case 0x15: // MSCALF
            vuExecMicro( (u16)(data[0]) << 3, VIF1dmanum );
            vif1.cmd = 0;
            break;

        case 0x17: // MSCNT
            vuExecMicro( -1, VIF1dmanum );
            vif1.cmd = 0;
            break;

        case 0x20: // STMASK
            break;

        case 0x30: // STROW
            vif1.tag.addr = 0;
            vif1.tag.size = 4;
            break;

        case 0x31: // STCOL
            vif1.tag.addr = 0;
            vif1.tag.size = 4;
            break;

        case 0x4A: // MPG
			vif1FLUSH();
            vifNum = (u8)(data[0] >> 16);
            if (vifNum == 0) vifNum = 256;
            vif1.tag.addr = (u16)(data[0]) << 3;
            vif1.tag.size = vifNum * 2;
            break;

        case 0x50: // DIRECT
        case 0x51: // DIRECTHL
			vifImm = (u16)data[0];
            if (vifImm == 0) {
				vif1.tag.size = 65536 << 2;
			} else {
				vif1.tag.size = vifImm << 2;
			}
            break;

        default:			
            vif1.cmd = 0;
            if ((vif1Regs->err & 0x6) == 0) {  //Ignore vifcode and tag mismatch error
				SysPrintf( "UNKNOWN VifCmd: %x\n", vif1.cmd );
            	vif1Regs->stat |= 1 << 13;
            }
            break;
	}
}


int VIF1transfer(u32 *data, int size, int istag) {
	int ret;
	int transferred=vif1.vifstalled ? vif1.irqoffset : 0; // irqoffset necessary to add up the right qws, or else will spin (spiderman)

#ifdef VIF_LOG 
	VIF_LOG( "VIF1transfer: size %x (vif1.cmd %x)\n", size, vif1.cmd );
#endif

	//return 0;
	
	//vif1.irq = 0;
	while (size > 0) {

		if (vif1.cmd) {			
			//vif1Regs->stat |= VIF1_STAT_VPS_T;
			ret = vif1transferData(data, size);
			data+= ret; size-= ret;
			transferred+= ret;
			//vif1Regs->stat &= ~VIF1_STAT_VPS_T;
			continue;
		}
		
		vif1Regs->stat &= ~VIF1_STAT_VPS_W;

		vif1.cmd = (data[0] >> 24);
		vif1Regs->code = data[0];
		if(vif1.irq && vif1.cmd != 0x7/* && size > 1*/) {
			break;
		}
#ifdef VIF_LOG 
		VIF_LOG( "VIFtransfer: cmd %x, num %x, imm %x, size %x\n", vif1.cmd, (data[0] >> 16) & 0xff, data[0] & 0xffff, size );
#endif

		if ((vif1.cmd & 0x80) && !(vif1Regs->err & 0x1)) { //i bit on vifcode and not masked by VIF1_ERR
#ifdef VIF_LOG
			VIF_LOG( "Interrupt on VIFcmd: %x (INTC_MASK = %x)\n", vif1.cmd, psHu32(INTC_MASK) );
#endif
			
			//vif1Regs->stat|= VIF1_STAT_VIS;
			vif1.irq++;
		} 

		//vif1Regs->stat |= VIF1_STAT_VPS_D;
		if ((vif1.cmd & 0x60) == 0x60) {
			vif1UNPACK(data);
		} else {
			vif1CMD(data, size);
		}
		//vif1Regs->stat &= ~VIF1_STAT_VPS_D;
		if(vif1.tag.size > 0) vif1Regs->stat |= VIF1_STAT_VPS_W;
		data++; 
		size--;
		transferred++;
		
	}

	if( !vif1.cmd )
		vif1Regs->stat &= ~VIF1_STAT_VPS_W;
		
	if (vif1.irq > 0) {
		vif1.irq--;

		if( istag ) {
			hwIntcIrq(VIF1intc);
			vif1Regs->stat|= VIF1_STAT_INT;
			return -2;
		}

		// spiderman doesn't break on qw boundaries
		vif1.irqoffset = transferred%4; // cannot lose the offset

		transferred = transferred >> 2;
		vif1ch->madr+= (transferred << 4);
		vif1ch->qwc-= transferred;
		//SysPrintf("Stall on vif1, FromSPR = %x, Vif1MADR = %x Sif0MADR = %x STADR = %x\n", psHu32(0x1000d010), vif1ch->madr, psHu32(0x1000c010), psHu32(DMAC_STADR));
		hwIntcIrq(VIF1intc);
		vif1Regs->stat|= VIF1_STAT_INT;
		if(size > 0) {
			//SysPrintf("VIF1 Remaining size %x, data %x_%x_%x_%x\n", size, data[3], data[2], data[1], data[0]);
		}
		return -2;
	}

	if( !istag ) {
		transferred = transferred >> 2;
		vif1ch->madr+= (transferred << 4);
		vif1ch->qwc-= transferred;
	}

	return 0;
}

int  _VIF1chain() {
	u32 *pMem;
	u32 qwc = vif1ch->qwc;
	u32 ret;

	if (vif1ch->qwc == 0) return 0;

	pMem = (u32*)dmaGetAddr(vif1ch->madr);
	if (pMem == NULL)
		return -1;

	if( vif1.vifstalled ) {
		ret = VIF1transfer(pMem+vif1.irqoffset, vif1ch->qwc*4-vif1.irqoffset, 0);
	}
	else {
		ret = VIF1transfer(pMem, vif1ch->qwc*4, 0);
	}
	/*vif1ch->madr+= (vif1ch->qwc << 4);
	vif1ch->qwc-= qwc;*/
	cycles+= (qwc-vif1ch->qwc)*BIAS; /* guessing */
	return ret;
}

int _chainVIF1() {
	u32 *ptag;
	int id;
	int done=0;
	int ret;
	
	ptag = (u32*)dmaGetAddr(vif1ch->tadr); //Set memory pointer to TADR
	if (ptag == NULL) {						//Is ptag empty?
			psHu32(DMAC_STAT)|= 1<<15;          //If yes, set BEIS (BUSERR) in DMAC_STAT register
			return -1;						   //Return -1 as an error has occurred
		
	}
	
	id        = (ptag[0] >> 28) & 0x7; //ID for DmaChain copied from bit 28 of the tag
	vif1ch->qwc  = (u16)ptag[0];       //QWC set to lower 16bits of the tag
	vif1ch->madr = ptag[1];            //MADR = ADDR field
	cycles+=1; // Add 1 cycles from the QW read for the tag
#ifdef VIF_LOG
	VIF_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, madr=%lx, tadr=%lx\n",
			ptag[1], ptag[0], vif1ch->qwc, id, vif1ch->madr, vif1ch->tadr);
#endif

	vif1ch->chcr = ( vif1ch->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 ); //Transfer upper part of tag to CHCR bits 31-15
	// Transfer dma tag if tte is set
	done |= hwDmacSrcChainWithStack(vif1ch, id);
	if (vif1ch->chcr & 0x40) {
		ret = VIF1transfer(ptag+2, 2, 1);  //Transfer Tag
		if (ret == -1) return -1;       //There has been an error
		if (ret == -2) {
			vif1.vifstalled = 1;
			return done;        //IRQ set by VIFTransfer
		}
	}
	
#ifdef VIF_LOG
		VIF_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, madr=%lx, tadr=%lx\n",
				ptag[1], ptag[0], vif1ch->qwc, id, vif1ch->madr, vif1ch->tadr);
#endif

	//done |= hwDmacSrcChainWithStack(vif1ch, id);
	ret = _VIF1chain();											   //Transfers the data set by the switch
	if (ret == -1) { return -1; }									   //There's been an error
	if (ret == -2) { 							   //IRQ has been set by VifTransfer
		vif1.vifstalled = 1;
		return done;
	}

	//if(id == 7)vif1ch->tadr = vif1ch->madr;

	vif1.vifstalled = 0;

	if ((vif1ch->chcr & 0x80) && (ptag[0] >> 31)) {			       //Check TIE bit of CHCR and IRQ bit of tag
#ifdef VIF_LOG
		VIF_LOG( "dmaIrq Set\n" );
#endif
		//SysPrintf("VIF1 TIE\n");
		//SysPrintf( "VIF1dmaIrq Set\n" );
		vif1ch->qwc = 0;
		vif1Regs->stat|= VIF1_STAT_VIS;							   //Set the Tag Interrupt flag of VIF1_STAT
		return 1;												   //End Transfer
	}
	return done;												   //Return Done
}

int _vif1Interrupt() {
	int ret;

#ifdef VIF_LOG 
	VIF_LOG("vif1Interrupt: %8.8x\n", cpuRegs.cycle);
#endif

	if(vif1.vifstalled == 1) {
		return 1;
	}
	if (vif1ch->chcr & 0x4 && vif1.done == 0 && vif1.vifstalled == 0) {

		if( !(psHu32(DMAC_CTRL) & 0x1) ) {
			SysPrintf("vif1 dma masked\n");
			return 0;
		}

		cycles = 0;
		ret = _chainVIF1();
		if (ret != 0) vif1.done = 1;
		INT(1, cycles);
		return 0;
	}
	

	// hack?
	vif1.tag.size = 0;
	vif1.cmd = 0;
	vif1Regs->stat &= ~VIF1_STAT_VPS;
	// hack?


	vif1ch->chcr &= ~0x100;
	hwDmacIrq(DMAC_VIF1);
	vif1Regs->stat&= ~0x1F000000; // FQC=0

	return 1;
}

int  vif1Interrupt() {

	int ret;

	ret = _vif1Interrupt();
	//FreezeXMMRegs(0);
	//FreezeMMXRegs(0);

	return ret;
}

void _dmaVIF1() {
#ifdef VIF_LOG
	VIF_LOG("dmaVIF1 chcr = %lx, madr = %lx, qwc  = %lx\n"
			"        tadr = %lx, asr0 = %lx, asr1 = %lx\n",
			vif1ch->chcr, vif1ch->madr, vif1ch->qwc,
			vif1ch->tadr, vif1ch->asr0, vif1ch->asr1 );
#endif

	/* Check if there is a pending irq */
	/*if (vif1.irq > 0) {
		vif1.irq--;
		hwIntcIrq(VIF1intc);
		return;
	}*/
//	if(vif1ch->qwc > 0) {
//		 _VIF1chain();
//		 INT(1, cycles);
//	}

	if (((psHu32(DMAC_CTRL) & 0xC) == 0x8)) { // VIF MFIFO
		return;
	}

#ifdef PCSX2_DEVBUILD
	if ((psHu32(DMAC_CTRL) & 0xC0) == 0x40) { // STD == VIF1
		SysPrintf("vif1 drain stall %d\n", (psHu32(DMAC_CTRL)>>4)&3);
		//return;
	}
#endif

	cycles = 0;
	vif1Regs->stat|= 0x10000000; // FQC=16

	if (!(vif1ch->chcr & 0x4)) { // Normal Mode 
		if ((vif1ch->chcr & 0x1)) { // to Memory
			_VIF1chain();
			INT(1, cycles);
		} else { // from Memory

			if( CHECK_MULTIGS ) {
				u8* pMem = GSRingBufCopy(NULL, 0, GS_RINGTYPE_VIFFIFO);
				assert( vif1ch->qwc < 0x10000 );
				*(u32*)(pMem-16) = GS_RINGTYPE_VIFFIFO|(vif1ch->qwc<<16); // hack
				*(u32*)(pMem-12) = vif1ch->madr;
				*(u32*)(pMem-8) = cpuRegs.cycle;

				GSRINGBUF_DONECOPY(pMem, 0);
				SetEvent(g_hGsEvent);
			}
			else {

				int size;
				u64* pMem = (u64*)dmaGetAddr(vif1ch->madr);
				if (pMem == NULL) {
					psHu32(DMAC_STAT)|= 1<<15;
					return;
				}

				if( GSreadFIFO2 == NULL ) {
					for (size=vif1ch->qwc; size>0; size--) {
						if (size > 1) GSreadFIFO((u64*)&PS2MEM_HW[0x5000]);
						pMem[0] = psHu64(0x5000);
						pMem[1] = psHu64(0x5008); pMem+= 2;
					}
				}
				else {
					GSreadFIFO2(pMem, vif1ch->qwc);

					// set incase read
					psHu64(0x5000) = pMem[2*vif1ch->qwc-2];
					psHu64(0x5008) = pMem[2*vif1ch->qwc-1];
				}

				vif1Regs->stat&= ~0x1f000000;
				vif1ch->qwc = 0;
				INT(1, cycles);
			}
		}
		vif1.done = 1;
		return;
	}

/*	if (_VIF1chain() != 0) {
		INT(1, cycles);
		return;
	}*/

	// Chain Mode
	vif1.done = 0;
	INT(1, cycles);
}

void dmaVIF1()
{
	_dmaVIF1();
	FreezeXMMRegs(0);
	FreezeMMXRegs(0);
}

void vif1Write32(u32 mem, u32 value) {
	if (mem == 0x10003c30) { // MARK
#ifdef VIF_LOG
		VIF_LOG("VIF1_MARK write32 0x%8.8x\n", value);
#endif
		/* Clear mark flag in VIF1_STAT and set mark with 'value' */
		vif1Regs->stat&= ~VIF1_STAT_MRK;
		vif1Regs->mark = value;
	} else
	if (mem == 0x10003c10) { // FBRST
#ifdef VIF_LOG
		VIF_LOG("VIF1_FBRST write32 0x%8.8x\n", value);
#endif
		if (value & 0x1) {
			/* Reset VIF */
			//SysPrintf("Vif1 Reset\n");
			memset(&vif1, 0, sizeof(vif1));
			vif1ch->qwc = 0; //?
			psHu64(0x10005000) = 0;
			psHu64(0x10005008) = 0;
			vif1.done = 1;
			vif1Regs->err = 0;
			vif1Regs->stat&= ~0x1F000000; // FQC=0
		}
		if (value & 0x2) {
			/* Force Break the VIF */
			/* I guess we should stop the VIF dma here 
			   but not 100% sure (linuz) */
			vif1Regs->stat |= VIF1_STAT_VFS;
			SysPrintf("vif1 force break\n");
		}
		if (value & 0x4) {
			/* Stop VIF */
			/* Not completly sure about this, can't remember what game 
			   used this, but 'draining' the VIF helped it, instead of 
			   just stoppin the VIF (linuz) */
			vif1Regs->stat |= VIF1_STAT_VSS;
			//SysPrintf("Vif1 Stop\n");
			//dmaVIF1();	// Drain the VIF  --- VIF Stops as not to outstrip dma source (refraction)
			//FreezeXMMRegs(0);
		}
		if (value & 0x8) {
			int cancel = 0;

			/* Cancel stall, first check if there is a stall to cancel, 
			   and then clear VIF1_STAT VSS|VFS|VIS|INT|ER0|ER1 bits */
			if (vif1Regs->stat & (VIF1_STAT_INT|VIF1_STAT_VSS|VIF1_STAT_VIS|VIF1_STAT_VFS)) {
				cancel = 1;
			}

			vif1Regs->stat &= ~(VIF1_STAT_VSS | VIF1_STAT_VFS | VIF1_STAT_VIS |
								VIF1_STAT_INT | VIF1_STAT_ER0 | VIF1_STAT_ER1);
			if (cancel) {
				//SysPrintf("VIF1 Stall Resume\n");
				if( vif1.vifstalled ) {
					// loop necessary for spiderman
					if ( _VIF1chain() != -2 )
						vif1.vifstalled = 0;

					FreezeXMMRegs(0);
					FreezeMMXRegs(0);
				}
			}

			INT(1, 0); // If vif is stopped/stall cancelled/ or force break, we need to make sure the dma ends.
		}			
	} else
	if (mem == 0x10003c20) { // ERR
#ifdef VIF_LOG
		VIF_LOG("VIF1_ERR write32 0x%8.8x\n", value);
#endif
		/* Set VIF1_ERR with 'value' */
		vif1Regs->err = value;
	} else
	if (mem == 0x10003c00) { // STAT
#ifdef VIF_LOG
		VIF_LOG("VIF1_STAT write32 0x%8.8x\n", value);
#endif

#ifdef PCSX2_DEVBUILD
		/* Only FDR bit is writable, so mask the rest */
		if( (vif1Regs->stat & VIF1_STAT_FDR) ^ (value & VIF1_STAT_FDR) ) {
			// different so can't be stalled
			if (vif1Regs->stat & (VIF1_STAT_INT|VIF1_STAT_VSS|VIF1_STAT_VIS|VIF1_STAT_VFS)) {
				SysPrintf("changing dir when vif1 fifo stalled\n");
			}
		}
#endif

		vif1Regs->stat = (vif1Regs->stat & ~VIF1_STAT_FDR) | (value & VIF1_STAT_FDR);
		if (vif1Regs->stat & VIF1_STAT_FDR) {
			vif1Regs->stat|= 0x01000000;
		} else {
			vif1ch->qwc = 0;
			vif1.vifstalled = 0;
			vif1.done = 1;
			vif1Regs->stat&= ~0x1F000000; // FQC=0
		}
	}
	else if( mem >= 0x10003d00 && mem < 0x10003d80 ) {
		assert( (mem&0xf) == 0 );
		if( mem < 0x10003d40 ) g_vifRow1[(mem>>4)&3] = value;
		else g_vifCol1[(mem>>4)&3] = value;
	}

	/* Other registers are read-only so do nothing for them */
}

void vif1Reset() {
	/* Reset the whole VIF, meaning the internal pcsx2 vars
	   and all the registers */
	memset(&vif1, 0, sizeof(vif1));
	memset(vif1Regs, 0, sizeof(vif1Regs));
	SetNewMask(g_vif1Masks, g_vif1HasMask3, 0, 0xffffffff);
	psHu64(0x10005000) = 0;
	psHu64(0x10005008) = 0;
	vif1.done = 1;
	vif1Regs->stat&= ~0x1F000000; // FQC=0
	FreezeXMMRegs(0);
	FreezeMMXRegs(0);
}

int vif1Freeze(gzFile f, int Mode) {
	gzfreeze(&vif1, sizeof(vif1));
	if (Mode == 0)
		SetNewMask(g_vif1Masks, g_vif1HasMask3, vif1Regs->mask, ~vif1Regs->mask);

	return 0;
}