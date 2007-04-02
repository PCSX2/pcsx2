#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

extern "C" {

#if defined(__WIN32__)
#include <windows.h>
#endif

#include "PS2Etypes.h"
#include "System.h"
#include "R5900.h"
#include "Vif.h"
#include "VU.h"
#include "ix86/ix86.h"
#include "iCore.h"
#include "R3000A.h"

u16 g_x86AllocCounter = 0;
u16 g_xmmAllocCounter = 0;
u16 g_mmxAllocCounter = 0;

u16 x86FpuState, iCWstate;
EEINST* g_pCurInstInfo = NULL;

u32 g_cpuRegHasLive1 = 0, g_cpuPrevRegHasLive1 = 0; // set if upper 32 bits are live
u32 g_cpuRegHasSignExt = 0, g_cpuPrevRegHasSignExt = 0; // set if upper 32 bits are the sign extension of the lower integer

// used to make sure regs don't get changed while in recompiler
// use FreezeMMXRegs, FreezeXMMRegs
u8 g_globalMMXSaved = 0, g_globalXMMSaved = 0;
u32 g_recWriteback = 0;

#ifdef _DEBUG
char g_globalMMXLocked = 0, g_globalXMMLocked = 0;
#endif

_xmmregs xmmregs[XMMREGS], s_saveXMMregs[XMMREGS];

__declspec(align(16)) u64 g_globalMMXData[8];
__declspec(align(16)) u64 g_globalXMMData[2*XMMREGS];

// X86 caching
_x86regs x86regs[X86REGS], s_saveX86regs[X86REGS];
static int s_x86checknext = 0;

} // end extern "C"

#include <vector>

void _initX86regs() {
	memset(x86regs, 0, sizeof(x86regs));
	g_x86AllocCounter = 0;
	s_x86checknext = 0;
}

__forceinline u32 _x86GetAddr(int type, int reg)
{
	switch(type&~X86TYPE_VU1) {
		case X86TYPE_GPR: return (u32)&cpuRegs.GPR.r[reg];
		case X86TYPE_VI: {
			//assert( reg < 16 || reg == REG_R );
			return (type&X86TYPE_VU1)?(u32)&VU1.VI[reg]:(u32)&VU0.VI[reg];
		}
		case X86TYPE_MEMOFFSET: return 0;
		case X86TYPE_VIMEMOFFSET: return 0;
		case X86TYPE_VUQREAD: return (type&X86TYPE_VU1)?(u32)&VU1.VI[REG_P]:(u32)&VU0.VI[REG_Q];
		case X86TYPE_VUPREAD: return (type&X86TYPE_VU1)?(u32)&VU1.VI[REG_P]:(u32)&VU0.VI[REG_Q];
		case X86TYPE_VUQWRITE: return (type&X86TYPE_VU1)?(u32)&VU1.q:(u32)&VU0.q;
		case X86TYPE_VUPWRITE: return (type&X86TYPE_VU1)?(u32)&VU1.p:(u32)&VU0.p;
		case X86TYPE_PSX: return (u32)&psxRegs.GPR.r[reg];
		case X86TYPE_PCWRITEBACK:
			return (u32)&g_recWriteback;
		case X86TYPE_VUJUMP:
			return (u32)&g_recWriteback;
		default: assert(0);
	}

	return 0;
}

int _getFreeX86reg(int mode)
{
	int i, tempi;
	u32 bestcount = 0x10000;

	int maxreg = (mode&MODE_8BITREG)?4:X86REGS;

	for (i=0; i<X86REGS; i++) {
		int reg = (s_x86checknext+i)%X86REGS;
		if( reg == 0 || reg == ESP ) continue;
		if( reg >= maxreg ) continue;
		if( (mode&MODE_NOFRAME) && reg==EBP ) continue;

		if (x86regs[reg].inuse == 0) {
			s_x86checknext = (reg+1)%X86REGS;
			return reg;
		}
	}

	tempi = -1;
	for (i=1; i<maxreg; i++) {
		if( i == ESP ) continue;
		if( (mode&MODE_NOFRAME) && i==EBP ) continue;

		if (x86regs[i].needed) continue;
		if (x86regs[i].type != X86TYPE_TEMP) {

			if( x86regs[i].counter < bestcount ) {
				tempi = i;
				bestcount = x86regs[i].counter;
			}
			continue;
		}

		_freeX86reg(i);
		return i;
	}

	if( tempi != -1 ) {
		_freeX86reg(tempi);
		return tempi;
	}
	SysPrintf("*PCSX2*: x86 error\n");
	assert(0);

	return -1;
}

int _allocX86reg(int x86reg, int type, int reg, int mode)
{
	int i;
	assert( reg >= 0 && reg < 32 );

//	if( X86_ISVI(type) )
//		assert( reg < 16 || reg == REG_R );
	 
	// don't alloc EAX and ESP,EBP if MODE_NOFRAME
	int oldmode = mode;
	int noframe = mode&MODE_NOFRAME;
	int maxreg = (mode&MODE_8BITREG)?4:X86REGS;
	mode &= ~(MODE_NOFRAME|MODE_8BITREG);
	int readfromreg = -1;

	if( type != X86TYPE_TEMP ) {

		if( maxreg < X86REGS ) {
			// make sure reg isn't in the higher regs
			
			for(i = maxreg; i < X86REGS; ++i) {
				if (!x86regs[i].inuse || x86regs[i].type != type || x86regs[i].reg != reg) continue;

				if( mode & MODE_READ ) {
					readfromreg = i;
					x86regs[i].inuse = 0;
					break;
				}
				else if( mode & MODE_WRITE ) {
					x86regs[i].inuse = 0;
					break;
				}
			}
		}

		for (i=1; i<maxreg; i++) {
			if( i == ESP ) continue;

			if (!x86regs[i].inuse || x86regs[i].type != type || x86regs[i].reg != reg) continue;

			if( (noframe && i == EBP) || (i >= maxreg) ) {
				if( x86regs[i].mode & MODE_READ )
					readfromreg = i;
				//if( xmmregs[i].mode & MODE_WRITE ) mode |= MODE_WRITE;
				mode |= x86regs[i].mode&MODE_WRITE;
				x86regs[i].inuse = 0;
				break;
			}

			if( x86reg >= 0 ) {
				// requested specific reg, so return that instead
				if( i != x86reg ) {
					if( x86regs[i].mode & MODE_READ ) readfromreg = i;
					//if( x86regs[i].mode & MODE_WRITE ) mode |= MODE_WRITE;
					mode |= x86regs[i].mode&MODE_WRITE;
					x86regs[i].inuse = 0;
					break;
				}
			}

			if( type != X86TYPE_TEMP && !(x86regs[i].mode & MODE_READ) && (mode&MODE_READ)) {

				if( type == X86TYPE_GPR ) _flushConstReg(reg);
				
				if( X86_ISVI(type) && reg < 16 ) MOVZX32M16toR(i, _x86GetAddr(type, reg));
				else MOV32MtoR(i, _x86GetAddr(type, reg));

				x86regs[i].mode |= MODE_READ;
			}

			x86regs[i].needed = 1;
			x86regs[i].mode|= mode;
			return i;
		}
	}

	if (x86reg == -1) {
		x86reg = _getFreeX86reg(oldmode);
	}
	else {
		_freeX86reg(x86reg);
	}

	x86regs[x86reg].type = type;
	x86regs[x86reg].reg = reg;
	x86regs[x86reg].mode = mode;
	x86regs[x86reg].needed = 1;
	x86regs[x86reg].inuse = 1;

	if( mode & MODE_READ ) {
		if( readfromreg >= 0 ) MOV32RtoR(x86reg, readfromreg);
		else {
			if( type == X86TYPE_GPR ) {

				if( reg == 0 ) {
					XOR32RtoR(x86reg, x86reg);
				}
				else {
					_flushConstReg(reg);
					_deleteMMXreg(MMX_GPR+reg, 1);
					_deleteGPRtoXMMreg(reg, 1);
					
					_eeMoveGPRtoR(x86reg, reg);
					
					_deleteMMXreg(MMX_GPR+reg, 0);
					_deleteGPRtoXMMreg(reg, 0);
				}
			}
			else {
				if( X86_ISVI(type) && reg < 16 ) {
					if( reg == 0 ) XOR32RtoR(x86reg, x86reg);
					else MOVZX32M16toR(x86reg, _x86GetAddr(type, reg));
				}
				else MOV32MtoR(x86reg, _x86GetAddr(type, reg));
			}
		}
}

	return x86reg;
}

int _checkX86reg(int type, int reg, int mode)
{
	int i;

	for (i=0; i<X86REGS; i++) {
		if (x86regs[i].inuse && x86regs[i].reg == reg && x86regs[i].type == type) {

			if( !(x86regs[i].mode & MODE_READ) && (mode&MODE_READ) ) {
				if( X86_ISVI(type) ) MOVZX32M16toR(i, _x86GetAddr(type, reg));
				else MOV32MtoR(i, _x86GetAddr(type, reg));
			}

			x86regs[i].mode |= mode;
			x86regs[i].counter = g_x86AllocCounter++;
			x86regs[i].needed = 1;
			return i;
		}
	}

	return -1;
}

void _addNeededX86reg(int type, int reg)
{
	int i;

	for (i=0; i<X86REGS; i++) {
		if (!x86regs[i].inuse || x86regs[i].reg != reg || x86regs[i].type != type ) continue;

		x86regs[i].counter = g_x86AllocCounter++;
		x86regs[i].needed = 1;
	}
}

void _clearNeededX86regs() {
	int i;

	for (i=0; i<X86REGS; i++) {
		if (x86regs[i].needed ) {
			if( x86regs[i].inuse && (x86regs[i].mode&MODE_WRITE) )
				x86regs[i].mode |= MODE_READ;
		}
		x86regs[i].needed = 0;
	}
}

void _deleteX86reg(int type, int reg, int flush)
{
	int i;

	for (i=0; i<X86REGS; i++) {
		if (x86regs[i].inuse && x86regs[i].reg == reg && x86regs[i].type == type) {
			switch(flush) {
				case 0:
					_freeX86reg(i);
					break;
				case 1:
					if( x86regs[i].mode & MODE_WRITE) {

						if( X86_ISVI(type) && x86regs[i].reg < 16 ) MOV16RtoM(_x86GetAddr(type, x86regs[i].reg), i);
						else MOV32RtoM(_x86GetAddr(type, x86regs[i].reg), i);
						
						// get rid of MODE_WRITE since don't want to flush again
						x86regs[i].mode &= ~MODE_WRITE;
						x86regs[i].mode |= MODE_READ;
					}
					return;
				case 2:
					x86regs[i].inuse = 0;
					break;
			}
		}
	}
}

void _freeX86reg(int x86reg)
{
	assert( x86reg >= 0 && x86reg < X86REGS );

	if( x86regs[x86reg].inuse && (x86regs[x86reg].mode&MODE_WRITE) ) {
		x86regs[x86reg].mode &= ~MODE_WRITE;

		if( X86_ISVI(x86regs[x86reg].type) && x86regs[x86reg].reg < 16 ) {
			MOV16RtoM(_x86GetAddr(x86regs[x86reg].type, x86regs[x86reg].reg), x86reg);
		}
		else
			MOV32RtoM(_x86GetAddr(x86regs[x86reg].type, x86regs[x86reg].reg), x86reg);
	}

	x86regs[x86reg].inuse = 0;
}

void _freeX86regs() {
	int i;

	for (i=0; i<X86REGS; i++) {
		if (!x86regs[i].inuse) continue;

		_freeX86reg(i);
	}
}

void _eeSetLoadStoreReg(int gprreg, u32 offset, int x86reg)
{
	int regs[2] = {ESI, EDI};

	int i = _checkX86reg(X86TYPE_MEMOFFSET, gprreg, MODE_WRITE);
	if( i < 0 ) {
		for(i = 0; i < 2; ++i) {
			if( !x86regs[regs[i]].inuse ) break;
		}

		assert( i < 2 );
		i = regs[i];
	}

	if( i != x86reg ) MOV32RtoR(x86reg, i);
	x86regs[i].extra = offset;
}

int _eeGeLoadStoreReg(int gprreg, int* poffset)
{
	int i = _checkX86reg(X86TYPE_MEMOFFSET, gprreg, MODE_READ);
	if( i >= 0 ) return -1;

	if( poffset ) *poffset = x86regs[i].extra;
	return i;
}

// MMX Caching
_mmxregs mmxregs[8], s_saveMMXregs[8];
static int s_mmxchecknext = 0;

void _initMMXregs()
{
	memset(mmxregs, 0, sizeof(mmxregs));
	g_mmxAllocCounter = 0;
	s_mmxchecknext = 0;
}

__forceinline void* _MMXGetAddr(int reg)
{
	assert( reg != MMX_TEMP );
	
	if( reg == MMX_LO ) return &cpuRegs.LO;
	if( reg == MMX_HI ) return &cpuRegs.HI;
	if( reg == MMX_FPUACC ) return &fpuRegs.ACC;

	if( reg >= MMX_GPR && reg < MMX_GPR+32 ) return &cpuRegs.GPR.r[reg&31];
	if( reg >= MMX_FPU && reg < MMX_FPU+32 ) return &fpuRegs.fpr[reg&31];
	if( reg >= MMX_COP0 && reg < MMX_COP0+32 ) return &cpuRegs.CP0.r[reg&31];
	
	assert( 0 );
	return NULL;
}

int  _getFreeMMXreg()
{
	int i, tempi;
	u32 bestcount = 0x10000;

	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[(s_mmxchecknext+i)%MMXREGS].inuse == 0) {
			int ret = (s_mmxchecknext+i)%MMXREGS;
			s_mmxchecknext = (s_mmxchecknext+i+1)%MMXREGS;
			return ret;
		}
	}

	// check for dead regs
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].needed) continue;
		if (mmxregs[i].reg >= MMX_GPR && mmxregs[i].reg < MMX_GPR+34 ) {
			if( !(g_pCurInstInfo->regs[mmxregs[i].reg-MMX_GPR] & (EEINST_LIVE0|EEINST_LIVE1)) ) {
				_freeMMXreg(i);
				return i;
			}
			if( !(g_pCurInstInfo->regs[mmxregs[i].reg-MMX_GPR]&EEINST_USED) ) {
				_freeMMXreg(i);
				return i;
			}
		}
	}

	// check for future xmm usage
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].needed) continue;
		if (mmxregs[i].reg >= MMX_GPR && mmxregs[i].reg < MMX_GPR+34 ) {
			if( !(g_pCurInstInfo->regs[mmxregs[i].reg] & EEINST_MMX) ) {
				_freeMMXreg(i);
				return i;
			}
		}
	}

	tempi = -1;
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].needed) continue;
		if (mmxregs[i].reg != MMX_TEMP) {

			if( mmxregs[i].counter < bestcount ) {
				tempi = i;
				bestcount = mmxregs[i].counter;
			}
			continue;
		}

		_freeMMXreg(i);
		return i;
	}

	if( tempi != -1 ) {
		_freeMMXreg(tempi);
		return tempi;
	}
	SysPrintf("*PCSX2*: mmx error\n");
	assert(0);

	return -1;
}

int _allocMMXreg(int mmxreg, int reg, int mode)
{
	int i;

	if( reg != MMX_TEMP ) {
		for (i=0; i<MMXREGS; i++) {
			if (mmxregs[i].inuse == 0 || mmxregs[i].reg != reg ) continue;

			if( MMX_ISGPR(reg)) {
				assert( _checkXMMreg(XMMTYPE_GPRREG, reg-MMX_GPR, 0) == -1 );
			}

			mmxregs[i].needed = 1;

			if( !(mmxregs[i].mode & MODE_READ) && (mode&MODE_READ) && reg != MMX_TEMP ) {

				SetMMXstate();
				if( reg == MMX_GPR ) {
					// moving in 0s
					PXORRtoR(i, i);
				}
				else {
					if( MMX_ISGPR(reg) ) _flushConstReg(reg-MMX_GPR);
					if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) )
						MOVDMtoMMX(i, (u32)_MMXGetAddr(reg));
					else {
						MOVQMtoR(i, (u32)_MMXGetAddr(reg));
					}
				}

				mmxregs[i].mode |= MODE_READ;
			}

			mmxregs[i].counter = g_mmxAllocCounter++;
			mmxregs[i].mode|= mode;
			return i;
		}
	}

	if (mmxreg == -1) {
		mmxreg = _getFreeMMXreg();
	}

	mmxregs[mmxreg].inuse = 1;
	mmxregs[mmxreg].reg = reg;
	mmxregs[mmxreg].mode = mode&~MODE_READHALF;
	mmxregs[mmxreg].needed = 1;
	mmxregs[mmxreg].counter = g_mmxAllocCounter++;

	SetMMXstate();
	if( reg == MMX_GPR ) {
		// moving in 0s
		PXORRtoR(mmxreg, mmxreg);
	}
	else {
		int xmmreg;
		if( MMX_ISGPR(reg) && (xmmreg = _checkXMMreg(XMMTYPE_GPRREG, reg-MMX_GPR, 0)) >= 0 ) {
			if (cpucaps.hasStreamingSIMD2Extensions) {
				SSE_MOVHPS_XMM_to_M64((u32)_MMXGetAddr(reg)+8, xmmreg);
				if( mode & MODE_READ )
					SSE2_MOVDQ2Q_XMM_to_MM(mmxreg, xmmreg);

				if( xmmregs[xmmreg].mode & MODE_WRITE )
					mmxregs[mmxreg].mode |= MODE_WRITE;

				// don't flush
				xmmregs[xmmreg].inuse = 0;
			}
			else {
				_freeXMMreg(xmmreg);

				if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) ) {
					MOVDMtoMMX(mmxreg, (u32)_MMXGetAddr(reg));
				}
				else if( mode & MODE_READ ) {
					MOVQMtoR(mmxreg, (u32)_MMXGetAddr(reg));
				}

			}
		}
		else {
			if( MMX_ISGPR(reg) ) {
				if(mode&(MODE_READHALF|MODE_READ)) _flushConstReg(reg-MMX_GPR);
			}

			if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) ) {
				MOVDMtoMMX(mmxreg, (u32)_MMXGetAddr(reg));
			}
			else if( mode & MODE_READ ) {
				MOVQMtoR(mmxreg, (u32)_MMXGetAddr(reg));
			}
		}
	}

	return mmxreg;
}

int _checkMMXreg(int reg, int mode)
{
	int i;
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].inuse && mmxregs[i].reg == reg ) {

			if( !(mmxregs[i].mode & MODE_READ) && (mode&MODE_READ) ) {

				if( reg == MMX_GPR ) {
					// moving in 0s
					PXORRtoR(i, i);
				}
				else {
					if( MMX_ISGPR(reg) && (mode&(MODE_READHALF|MODE_READ)) ) _flushConstReg(reg-MMX_GPR);
					if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) )
						MOVDMtoMMX(i, (u32)_MMXGetAddr(reg));
					else
						MOVQMtoR(i, (u32)_MMXGetAddr(reg));
				}
				SetMMXstate();
			}

			mmxregs[i].mode |= mode;
			mmxregs[i].counter = g_mmxAllocCounter++;
			mmxregs[i].needed = 1;
			return i;
		}
	}

	return -1;
}

void _addNeededMMXreg(int reg)
{
	int i;

	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].inuse == 0) continue;
		if (mmxregs[i].reg != reg) continue;

		mmxregs[i].counter = g_mmxAllocCounter++;
		mmxregs[i].needed = 1;
	}
}

void _clearNeededMMXregs()
{
	int i;

	for (i=0; i<MMXREGS; i++) {

		if( mmxregs[i].needed ) {

			// setup read to any just written regs
			if( mmxregs[i].inuse && (mmxregs[i].mode&MODE_WRITE) )
				mmxregs[i].mode |= MODE_READ;
			mmxregs[i].needed = 0;
		}
	}
}

// when flush is 0 - frees all of the reg, when flush is 1, flushes all of the reg, when
// it is 2, just stops using the reg (no flushing)
void _deleteMMXreg(int reg, int flush)
{
	int i;
	for (i=0; i<MMXREGS; i++) {

		if (mmxregs[i].inuse && mmxregs[i].reg == reg ) {

			switch(flush) {
				case 0:
					_freeMMXreg(i);
					break;
				case 1:
					if( mmxregs[i].mode & MODE_WRITE) {
						assert( mmxregs[i].reg != MMX_GPR );

						if( MMX_IS32BITS(reg) )
							MOVDMMXtoM((u32)_MMXGetAddr(mmxregs[i].reg), i);
						else
							MOVQRtoM((u32)_MMXGetAddr(mmxregs[i].reg), i);
						SetMMXstate();

						// get rid of MODE_WRITE since don't want to flush again
						mmxregs[i].mode &= ~MODE_WRITE;
						mmxregs[i].mode |= MODE_READ;
					}
					return;
				case 2:
					mmxregs[i].inuse = 0;
					break;
			}

			
			return;
		}
	}
}

int _getNumMMXwrite()
{
	int num = 0, i;
	for (i=0; i<MMXREGS; i++) {
		if( mmxregs[i].inuse && (mmxregs[i].mode&MODE_WRITE) ) ++num;
	}

	return num;
}

u8 _hasFreeMMXreg()
{
	int i;
	for (i=0; i<MMXREGS; i++) {
		if (!mmxregs[i].inuse) return 1;
	}

	// check for dead regs
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].needed) continue;
		if (mmxregs[i].reg >= MMX_GPR && mmxregs[i].reg < MMX_GPR+34 ) {
			if( !EEINST_ISLIVEMMX(mmxregs[i].reg-MMX_GPR) ) {
				return 1;
			}
		}
	}

	// check for dead regs
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].needed) continue;
		if (mmxregs[i].reg >= MMX_GPR && mmxregs[i].reg < MMX_GPR+34 ) {
			if( !(g_pCurInstInfo->regs[mmxregs[i].reg-MMX_GPR]&EEINST_USED) ) {
				return 1;
			}
		}
	}

	return 0;
}

void _freeMMXreg(int mmxreg)
{
	assert( mmxreg < MMXREGS );
	if (!mmxregs[mmxreg].inuse) return;
	
	if (mmxregs[mmxreg].mode & MODE_WRITE ) {

		if( mmxregs[mmxreg].reg >= MMX_GPR && mmxregs[mmxreg].reg < MMX_GPR+32 )
			assert( !(g_cpuHasConstReg & (1<<(mmxregs[mmxreg].reg-MMX_GPR))) );

		assert( mmxregs[mmxreg].reg != MMX_GPR );
		
		if( MMX_IS32BITS(mmxregs[mmxreg].reg) )
			MOVDMMXtoM((u32)_MMXGetAddr(mmxregs[mmxreg].reg), mmxreg);
		else
			MOVQRtoM((u32)_MMXGetAddr(mmxregs[mmxreg].reg), mmxreg);

		SetMMXstate();
	}

	mmxregs[mmxreg].mode &= ~MODE_WRITE;
	mmxregs[mmxreg].inuse = 0;
}

void _moveMMXreg(int mmxreg)
{
	int i;
	if( !mmxregs[mmxreg].inuse ) return;

	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].inuse) continue;
		break;
	}

	if( i == MMXREGS ) {
		_freeMMXreg(mmxreg);
		return;
	}

	// move
	mmxregs[i] = mmxregs[mmxreg];
	mmxregs[mmxreg].inuse = 0;
	MOVQRtoR(i, mmxreg);
}

// write all active regs
void _flushMMXregs()
{
	int i;

	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].inuse == 0) continue;

		if( mmxregs[i].mode & MODE_WRITE ) {

			assert( !(g_cpuHasConstReg & (1<<mmxregs[i].reg)) );

			assert( mmxregs[i].reg != MMX_TEMP );
			assert( mmxregs[i].mode & MODE_READ );
			assert( mmxregs[i].reg != MMX_GPR );

			if( MMX_IS32BITS(mmxregs[i].reg) )
				MOVDMMXtoM((u32)_MMXGetAddr(mmxregs[i].reg), i);
			else
				MOVQRtoM((u32)_MMXGetAddr(mmxregs[i].reg), i);
			SetMMXstate();

			mmxregs[i].mode &= ~MODE_WRITE;
			mmxregs[i].mode |= MODE_READ;
		}
	}
}

void _freeMMXregs()
{
	int i;
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].inuse == 0) continue;

		assert( mmxregs[i].reg != MMX_TEMP );
		assert( mmxregs[i].mode & MODE_READ );

		_freeMMXreg(i);
	}
}

void FreezeMMXRegs_(int save)
{
	assert( g_EEFreezeRegs );

	if( save ) {
		if( g_globalMMXSaved )
			return;
		g_globalMMXSaved = 1;

		__asm {
			movntq mmword ptr [g_globalMMXData + 0], mm0
			movntq mmword ptr [g_globalMMXData + 8], mm1
			movntq mmword ptr [g_globalMMXData + 16], mm2
			movntq mmword ptr [g_globalMMXData + 24], mm3
			movntq mmword ptr [g_globalMMXData + 32], mm4
			movntq mmword ptr [g_globalMMXData + 40], mm5
			movntq mmword ptr [g_globalMMXData + 48], mm6
			movntq mmword ptr [g_globalMMXData + 56], mm7
			emms
		}
	}
	else {
		if( !g_globalMMXSaved )
			return;
		g_globalMMXSaved = 0;

		__asm {
			movq mm0, mmword ptr [g_globalMMXData + 0]
			movq mm1, mmword ptr [g_globalMMXData + 8]
			movq mm2, mmword ptr [g_globalMMXData + 16]
			movq mm3, mmword ptr [g_globalMMXData + 24]
			movq mm4, mmword ptr [g_globalMMXData + 32]
			movq mm5, mmword ptr [g_globalMMXData + 40]
			movq mm6, mmword ptr [g_globalMMXData + 48]
			movq mm7, mmword ptr [g_globalMMXData + 56]
			emms
		}
	}
}

#ifdef _DEBUG
void checkconstreg()
{
	SysPrintf("const regs not the same!\n");
	assert(0);
}
#endif

void SetMMXstate() {
	x86FpuState = MMX_STATE;
}

void SetFPUstate() {
	_freeMMXreg(6);
	_freeMMXreg(7);

	if (x86FpuState==MMX_STATE) {
		if (cpucaps.has3DNOWInstructionExtensions) FEMMS();
		else EMMS();
		x86FpuState=FPU_STATE;
	}
}

// XMM Caching
#define VU_VFx_ADDR(x)  (u32)&VU->VF[x].UL[0]
#define VU_ACCx_ADDR    (u32)&VU->ACC.UL[0]

static int s_xmmchecknext = 0;

void _initXMMregs() {
	memset(xmmregs, 0, sizeof(xmmregs));
	g_xmmAllocCounter = 0;
	s_xmmchecknext = 0;
}

__forceinline void* _XMMGetAddr(int type, int reg, VURegs *VU)
{
	if (type == XMMTYPE_VFREG ) return (void*)VU_VFx_ADDR(reg);
	else if (type == XMMTYPE_ACC ) return (void*)VU_ACCx_ADDR;
	else if (type == XMMTYPE_GPRREG) {
		if( reg < 32 )
			assert( !(g_cpuHasConstReg & (1<<reg)) || (g_cpuFlushedConstReg & (1<<reg)) );
		return &cpuRegs.GPR.r[reg].UL[0];
	}
	else if (type == XMMTYPE_FPREG ) return &fpuRegs.fpr[reg];
	else if (type == XMMTYPE_FPACC ) return &fpuRegs.ACC.f;

	assert(0);
	return NULL;
}

int  _getFreeXMMreg()
{
	int i, tempi;
	u32 bestcount = 0x10000;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[(i+s_xmmchecknext)%XMMREGS].inuse == 0) {
			int ret = (s_xmmchecknext+i)%XMMREGS;
			s_xmmchecknext = (s_xmmchecknext+i+1)%XMMREGS;
			return ret;
		}
	}

	// check for dead regs
	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].needed) continue;
		if (xmmregs[i].type == XMMTYPE_GPRREG ) {
			if( !(g_pCurInstInfo->regs[xmmregs[i].reg] & (EEINST_LIVE0|EEINST_LIVE1|EEINST_LIVE2)) ) {
				_freeXMMreg(i);
				return i;
			}
		}
	}

	// check for future xmm usage
	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].needed) continue;
		if (xmmregs[i].type == XMMTYPE_GPRREG ) {
			if( !(g_pCurInstInfo->regs[xmmregs[i].reg] & EEINST_XMM) ) {
				_freeXMMreg(i);
				return i;
			}
		}
	}

	tempi = -1;
	bestcount = 0xffff;
	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].needed) continue;
		if (xmmregs[i].type != XMMTYPE_TEMP) {

			if( xmmregs[i].counter < bestcount ) {
				tempi = i;
				bestcount = xmmregs[i].counter;
			}
			continue;
		}

		_freeXMMreg(i);
		return i;
	}

	if( tempi != -1 ) {
		_freeXMMreg(tempi);
		return tempi;
	}
	SysPrintf("*PCSX2*: VUrec ERROR\n");

	return -1;
}

int _allocTempXMMreg(XMMSSEType type, int xmmreg) {
	if (xmmreg == -1) {
		xmmreg = _getFreeXMMreg();
	}
	else {
		_freeXMMreg(xmmreg);
	}

	xmmregs[xmmreg].inuse = 1;
	xmmregs[xmmreg].type = XMMTYPE_TEMP;
	xmmregs[xmmreg].needed = 1;
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;
	g_xmmtypes[xmmreg] = type;

	return xmmreg;
}

int _allocVFtoXMMreg(VURegs *VU, int xmmreg, int vfreg, int mode) {
	int i;
	int readfromreg = -1;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0 || xmmregs[i].type != XMMTYPE_VFREG || xmmregs[i].reg != vfreg || xmmregs[i].VU != XMM_CONV_VU(VU) ) continue;

		if( xmmreg >= 0 ) {
			// requested specific reg, so return that instead
			if( i != xmmreg ) {
				if( xmmregs[i].mode & MODE_READ ) readfromreg = i;
				//if( xmmregs[i].mode & MODE_WRITE ) mode |= MODE_WRITE;
				mode |= xmmregs[i].mode&MODE_WRITE;
				xmmregs[i].inuse = 0;
				break;
			}
		}

		xmmregs[i].needed = 1;

		if( !(xmmregs[i].mode & MODE_READ) && (mode&MODE_READ) ) {
			SSE_MOVAPS_M128_to_XMM(i, VU_VFx_ADDR(vfreg));
			xmmregs[i].mode |= MODE_READ;
		}

		g_xmmtypes[i] = XMMT_FPS;
		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].mode|= mode;
		return i;
	}

	if (xmmreg == -1) {
		xmmreg = _getFreeXMMreg();
	}
	else {
		_freeXMMreg(xmmreg);
	}

	g_xmmtypes[xmmreg] = XMMT_FPS;
	xmmregs[xmmreg].inuse = 1;
	xmmregs[xmmreg].type = XMMTYPE_VFREG;
	xmmregs[xmmreg].reg = vfreg;
	xmmregs[xmmreg].mode = mode;
	xmmregs[xmmreg].needed = 1;
	xmmregs[xmmreg].VU = XMM_CONV_VU(VU);
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;
	if (mode & MODE_READ) {
		if( readfromreg >= 0 ) SSE_MOVAPS_XMM_to_XMM(xmmreg, readfromreg);
		else SSE_MOVAPS_M128_to_XMM(xmmreg, VU_VFx_ADDR(xmmregs[xmmreg].reg));
	}

	return xmmreg;
}

int _checkXMMreg(int type, int reg, int mode)
{
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse && xmmregs[i].type == (type&0xff) && xmmregs[i].reg == reg ) {

			if( !(xmmregs[i].mode & MODE_READ) && (mode&(MODE_READ|MODE_READHALF)) ) {
				if(mode&MODE_READ)
					SSEX_MOVDQA_M128_to_XMM(i, (u32)_XMMGetAddr(xmmregs[i].type, xmmregs[i].reg, xmmregs[i].VU ? &VU1 : &VU0));
				else {
					if( cpucaps.hasStreamingSIMD2Extensions && g_xmmtypes[i]==XMMT_INT )
						SSE2_MOVQ_M64_to_XMM(i, (u32)_XMMGetAddr(xmmregs[i].type, xmmregs[i].reg, xmmregs[i].VU ? &VU1 : &VU0));
					else
						SSE_MOVLPS_M64_to_XMM(i, (u32)_XMMGetAddr(xmmregs[i].type, xmmregs[i].reg, xmmregs[i].VU ? &VU1 : &VU0));
				}
			}

			xmmregs[i].mode |= mode&~MODE_READHALF;
			xmmregs[i].counter = g_xmmAllocCounter++; // update counter
			xmmregs[i].needed = 1;
			return i;
		}
	}

	return -1;
}

int _allocACCtoXMMreg(VURegs *VU, int xmmreg, int mode) {
	int i;
	int readfromreg = -1;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;
		if (xmmregs[i].type != XMMTYPE_ACC) continue;
		if (xmmregs[i].VU != XMM_CONV_VU(VU) ) continue;

		if( xmmreg >= 0 ) {
			// requested specific reg, so return that instead
			if( i != xmmreg ) {
				if( xmmregs[i].mode & MODE_READ ) readfromreg = i;
				//if( xmmregs[i].mode & MODE_WRITE ) mode |= MODE_WRITE;
				mode |= xmmregs[i].mode&MODE_WRITE;
				xmmregs[i].inuse = 0;
				break;
			}
		}

		if( !(xmmregs[i].mode & MODE_READ) && (mode&MODE_READ)) {
			SSE_MOVAPS_M128_to_XMM(i, VU_ACCx_ADDR);
			xmmregs[i].mode |= MODE_READ;
		}

		g_xmmtypes[i] = XMMT_FPS;
		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		xmmregs[i].mode|= mode;
		return i;
	}

	if (xmmreg == -1) {
		xmmreg = _getFreeXMMreg();
	}
	else {
		_freeXMMreg(xmmreg);
	}

	g_xmmtypes[xmmreg] = XMMT_FPS;
	xmmregs[xmmreg].inuse = 1;
	xmmregs[xmmreg].type = XMMTYPE_ACC;
	xmmregs[xmmreg].mode = mode;
	xmmregs[xmmreg].needed = 1;
	xmmregs[xmmreg].VU = XMM_CONV_VU(VU);
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;
	xmmregs[xmmreg].reg = 0;

	if (mode & MODE_READ) {
		if( readfromreg >= 0 ) SSE_MOVAPS_XMM_to_XMM(xmmreg, readfromreg);
		else SSE_MOVAPS_M128_to_XMM(xmmreg, VU_ACCx_ADDR);
	}

	return xmmreg;
}

int _allocFPtoXMMreg(int xmmreg, int fpreg, int mode) {
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;
		if (xmmregs[i].type != XMMTYPE_FPREG) continue;
		if (xmmregs[i].reg != fpreg) continue;

		if( !(xmmregs[i].mode & MODE_READ) && (mode&MODE_READ)) {
			SSE_MOVSS_M32_to_XMM(i, (u32)&fpuRegs.fpr[fpreg].f);
			xmmregs[i].mode |= MODE_READ;
		}

		g_xmmtypes[i] = XMMT_FPS;
		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		xmmregs[i].mode|= mode;
		return i;
	}

	if (xmmreg == -1) {
		xmmreg = _getFreeXMMreg();
	}

	g_xmmtypes[xmmreg] = XMMT_FPS;
	xmmregs[xmmreg].inuse = 1;
	xmmregs[xmmreg].type = XMMTYPE_FPREG;
	xmmregs[xmmreg].reg = fpreg;
	xmmregs[xmmreg].mode = mode;
	xmmregs[xmmreg].needed = 1;
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;

	if (mode & MODE_READ) {
		SSE_MOVSS_M32_to_XMM(xmmreg, (u32)&fpuRegs.fpr[fpreg].f);
	}

	return xmmreg;
}

int _allocGPRtoXMMreg(int xmmreg, int gprreg, int mode) {
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;
		if (xmmregs[i].type != XMMTYPE_GPRREG) continue;
		if (xmmregs[i].reg != gprreg) continue;

		assert( _checkMMXreg(MMX_GPR|gprreg, mode) == -1 );
		g_xmmtypes[i] = XMMT_INT;

		if( !(xmmregs[i].mode & MODE_READ) && (mode&MODE_READ)) {
			if( gprreg == 0 ) {
				SSEX_PXOR_XMM_to_XMM(i, i);
			}
			else {
				//assert( !(g_cpuHasConstReg & (1<<gprreg)) || (g_cpuFlushedConstReg & (1<<gprreg)) );
				_flushConstReg(gprreg);
				SSEX_MOVDQA_M128_to_XMM(i, (u32)&cpuRegs.GPR.r[gprreg].UL[0]);
			}
			xmmregs[i].mode |= MODE_READ;
		}

		if( (mode & MODE_WRITE) && gprreg < 32 ) {
			g_cpuHasConstReg &= ~(1<<gprreg);
			//assert( !(g_cpuHasConstReg & (1<<gprreg)) );
		}

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		xmmregs[i].mode|= mode;
		return i;
	}

	// currently only gpr regs are const
	if( (mode & MODE_WRITE) && gprreg < 32 ) {
		//assert( !(g_cpuHasConstReg & (1<<gprreg)) );
		g_cpuHasConstReg &= ~(1<<gprreg);
	}

	if (xmmreg == -1) {
		xmmreg = _getFreeXMMreg();
	}

	g_xmmtypes[xmmreg] = XMMT_INT;
	xmmregs[xmmreg].inuse = 1;
	xmmregs[xmmreg].type = XMMTYPE_GPRREG;
	xmmregs[xmmreg].reg = gprreg;
	xmmregs[xmmreg].mode = mode;
	xmmregs[xmmreg].needed = 1;
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;

	if (mode & MODE_READ) {
		if( gprreg == 0 ) {
			SSEX_PXOR_XMM_to_XMM(xmmreg, xmmreg);
		}
		else {
			// DOX86
			int mmxreg;
			if( (mode&MODE_READ) ) _flushConstReg(gprreg);

			if( (mmxreg = _checkMMXreg(MMX_GPR+gprreg, 0)) >= 0 ) {
				// transfer
				if (cpucaps.hasStreamingSIMD2Extensions ) {

					SetMMXstate();
					SSE2_MOVQ2DQ_MM_to_XMM(xmmreg, mmxreg);
					SSE2_PUNPCKLQDQ_XMM_to_XMM(xmmreg, xmmreg);
					SSE2_PUNPCKHQDQ_M128_to_XMM(xmmreg, (u32)&cpuRegs.GPR.r[gprreg].UL[0]);

					if( mmxregs[mmxreg].mode & MODE_WRITE ) {

						// instead of setting to write, just flush to mem
						if( !(mode & MODE_WRITE) ) {
							SetMMXstate();
							MOVQRtoM((u32)&cpuRegs.GPR.r[gprreg].UL[0], mmxreg);
						}
						//xmmregs[xmmreg].mode |= MODE_WRITE;
					}
					
					// don't flush
					mmxregs[mmxreg].inuse = 0;
				}
				else {
					_freeMMXreg(mmxreg);
					SSEX_MOVDQA_M128_to_XMM(xmmreg, (u32)&cpuRegs.GPR.r[gprreg].UL[0]);
				}
			}
			else {
				SSEX_MOVDQA_M128_to_XMM(xmmreg, (u32)&cpuRegs.GPR.r[gprreg].UL[0]);
			}
		}
	}
	else {
		_deleteMMXreg(MMX_GPR+gprreg, 0);
	}

	return xmmreg;
}

int _allocFPACCtoXMMreg(int xmmreg, int mode) {
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;
		if (xmmregs[i].type != XMMTYPE_FPACC) continue;

		if( !(xmmregs[i].mode & MODE_READ) && (mode&MODE_READ)) {
			SSE_MOVSS_M32_to_XMM(i, (u32)&fpuRegs.ACC.f);
			xmmregs[i].mode |= MODE_READ;
		}

		g_xmmtypes[i] = XMMT_FPS;
		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		xmmregs[i].mode|= mode;
		return i;
	}

	if (xmmreg == -1) {
		xmmreg = _getFreeXMMreg();
	}

	g_xmmtypes[xmmreg] = XMMT_FPS;
	xmmregs[xmmreg].inuse = 1;
	xmmregs[xmmreg].type = XMMTYPE_FPACC;
	xmmregs[xmmreg].mode = mode;
	xmmregs[xmmreg].needed = 1;
	xmmregs[xmmreg].reg = 0;
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;

	if (mode & MODE_READ) {
		SSE_MOVSS_M32_to_XMM(xmmreg, (u32)&fpuRegs.ACC.f);
	}

	return xmmreg;
}

void _addNeededVFtoXMMreg(int vfreg) {
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;
		if (xmmregs[i].type != XMMTYPE_VFREG) continue;
		if (xmmregs[i].reg != vfreg) continue;

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
	}
}

void _addNeededGPRtoXMMreg(int gprreg)
{
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;
		if (xmmregs[i].type != XMMTYPE_GPRREG) continue;
		if (xmmregs[i].reg != gprreg) continue;

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		break;
	}
}

void _addNeededACCtoXMMreg() {
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;
		if (xmmregs[i].type != XMMTYPE_ACC) continue;

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		break;
	}
}

void _addNeededFPtoXMMreg(int fpreg) {
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;
		if (xmmregs[i].type != XMMTYPE_FPREG) continue;
		if (xmmregs[i].reg != fpreg) continue;

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		break;
	}
}

void _addNeededFPACCtoXMMreg() {
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;
		if (xmmregs[i].type != XMMTYPE_FPACC) continue;

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		break;
	}
}

void _clearNeededXMMregs() {
	int i;

	for (i=0; i<XMMREGS; i++) {

		if( xmmregs[i].needed ) {

			// setup read to any just written regs
			if( xmmregs[i].inuse && (xmmregs[i].mode&MODE_WRITE) )
				xmmregs[i].mode |= MODE_READ;
			xmmregs[i].needed = 0;
		}

		if( xmmregs[i].inuse ) {
			assert( xmmregs[i].type != XMMTYPE_TEMP );
		}
	}
}

void _deleteVFtoXMMreg(int reg, int vu, int flush)
{
	int i;
	VURegs *VU = vu ? &VU1 : &VU0;
	
	for (i=0; i<XMMREGS; i++) {

		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_VFREG && xmmregs[i].reg == reg && xmmregs[i].VU == vu) {

			switch(flush) {
				case 0:
					_freeXMMreg(i);
					break;
				case 1:
				case 2:
					if( xmmregs[i].mode & MODE_WRITE ) {
						assert( reg != 0 );

						if( xmmregs[i].mode & MODE_VUXYZ ) {

							if( xmmregs[i].mode & MODE_VUZ ) {
								// xyz, don't destroy w
								int t0reg;
								for(t0reg = 0; t0reg < XMMREGS; ++t0reg ) {
									if( !xmmregs[t0reg].inuse ) break;
								}

								if( t0reg < XMMREGS ) {
									SSE_MOVHLPS_XMM_to_XMM(t0reg, i);
									SSE_MOVLPS_XMM_to_M64(VU_VFx_ADDR(xmmregs[i].reg), i);
									SSE_MOVSS_XMM_to_M32(VU_VFx_ADDR(xmmregs[i].reg)+8, t0reg);
								}
								else {
									// no free reg
									SSE_MOVLPS_XMM_to_M64(VU_VFx_ADDR(xmmregs[i].reg), i);
									SSE_SHUFPS_XMM_to_XMM(i, i, 0xc6);
									SSE_MOVSS_XMM_to_M32(VU_VFx_ADDR(xmmregs[i].reg)+8, i);
									SSE_SHUFPS_XMM_to_XMM(i, i, 0xc6);
								}
							}
							else {
								// xy
								SSE_MOVLPS_XMM_to_M64(VU_VFx_ADDR(xmmregs[i].reg), i);
							}
						}
						else SSE_MOVAPS_XMM_to_M128(VU_VFx_ADDR(xmmregs[i].reg), i);
						
						// get rid of MODE_WRITE since don't want to flush again
						xmmregs[i].mode &= ~MODE_WRITE;
						xmmregs[i].mode |= MODE_READ;
					}

					if( flush == 2 )
						xmmregs[i].inuse = 0;
					break;
			}
				
			return;
		}
	}
}

void _deleteACCtoXMMreg(int vu, int flush)
{
	int i;
	VURegs *VU = vu ? &VU1 : &VU0;
	
	for (i=0; i<XMMREGS; i++) {
	
		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_ACC && xmmregs[i].VU == vu) {

			switch(flush) {
				case 0:
					_freeXMMreg(i);
					break;
				case 1:
				case 2:
					if( xmmregs[i].mode & MODE_WRITE ) {

						if( xmmregs[i].mode & MODE_VUXYZ ) {

							if( xmmregs[i].mode & MODE_VUZ ) {
								// xyz, don't destroy w
								int t0reg;
								for(t0reg = 0; t0reg < XMMREGS; ++t0reg ) {
									if( !xmmregs[t0reg].inuse ) break;
								}

								if( t0reg < XMMREGS ) {
									SSE_MOVHLPS_XMM_to_XMM(t0reg, i);
									SSE_MOVLPS_XMM_to_M64(VU_ACCx_ADDR, i);
									SSE_MOVSS_XMM_to_M32(VU_ACCx_ADDR+8, t0reg);
								}
								else {
									// no free reg
									SSE_MOVLPS_XMM_to_M64(VU_ACCx_ADDR, i);
									SSE_SHUFPS_XMM_to_XMM(i, i, 0xc6);
									//SSE_MOVHLPS_XMM_to_XMM(i, i);
									SSE_MOVSS_XMM_to_M32(VU_ACCx_ADDR+8, i);
									SSE_SHUFPS_XMM_to_XMM(i, i, 0xc6);
								}
							}
							else {
								// xy
								SSE_MOVLPS_XMM_to_M64(VU_ACCx_ADDR, i);
							}
						}
						else SSE_MOVAPS_XMM_to_M128(VU_ACCx_ADDR, i);
						
						// get rid of MODE_WRITE since don't want to flush again
						xmmregs[i].mode &= ~MODE_WRITE;
						xmmregs[i].mode |= MODE_READ;
					}

					if( flush == 2 )
						xmmregs[i].inuse = 0;
					break;
			}
				
			return;
		}
	}
}

// when flush is 1 or 2, only commits the reg to mem (still leave its xmm entry)
void _deleteGPRtoXMMreg(int reg, int flush)
{
	int i;
	for (i=0; i<XMMREGS; i++) {

		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_GPRREG && xmmregs[i].reg == reg ) {

			switch(flush) {
				case 0:
					_freeXMMreg(i);
					break;
				case 1:
				case 2:
					if( xmmregs[i].mode & MODE_WRITE ) {
						assert( reg != 0 );

						//assert( g_xmmtypes[i] == XMMT_INT );
						SSEX_MOVDQA_XMM_to_M128((u32)&cpuRegs.GPR.r[reg].UL[0], i);
						
						// get rid of MODE_WRITE since don't want to flush again
						xmmregs[i].mode &= ~MODE_WRITE;
						xmmregs[i].mode |= MODE_READ;
					}

					if( flush == 2 )
						xmmregs[i].inuse = 0;
					break;
			}
				
			return;
		}
	}
}

void _deleteFPtoXMMreg(int reg, int flush)
{
	int i;
	for (i=0; i<XMMREGS; i++) {

		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_FPREG && xmmregs[i].reg == reg ) {

			switch(flush) {
				case 0:
					_freeXMMreg(i);
					break;
				case 1:
				case 2:
					if( flush == 1 && (xmmregs[i].mode & MODE_WRITE) ) {
						assert( reg != 0 );

						SSE_MOVSS_XMM_to_M32((u32)&fpuRegs.fpr[reg].UL, i);
						// get rid of MODE_WRITE since don't want to flush again
						xmmregs[i].mode &= ~MODE_WRITE;
						xmmregs[i].mode |= MODE_READ;
					}

					if( flush == 2 )
						xmmregs[i].inuse = 0;
					break;
			}
				
			return;
		}
	}
}

void _freeXMMreg(int xmmreg) {
	VURegs *VU = xmmregs[xmmreg].VU ? &VU1 : &VU0;
	assert( xmmreg < XMMREGS );

	if (!xmmregs[xmmreg].inuse) return;
	
	if (xmmregs[xmmreg].type == XMMTYPE_VFREG && (xmmregs[xmmreg].mode & MODE_WRITE) ) {
		if( xmmregs[xmmreg].mode & MODE_VUXYZ ) {

			if( xmmregs[xmmreg].mode & MODE_VUZ ) {
				// don't destroy w
				int t0reg;
				for(t0reg = 0; t0reg < XMMREGS; ++t0reg ) {
					if( !xmmregs[t0reg].inuse ) break;
				}

				if( t0reg < XMMREGS ) {
					SSE_MOVHLPS_XMM_to_XMM(t0reg, xmmreg);
					SSE_MOVLPS_XMM_to_M64(VU_VFx_ADDR(xmmregs[xmmreg].reg), xmmreg);
					SSE_MOVSS_XMM_to_M32(VU_VFx_ADDR(xmmregs[xmmreg].reg)+8, t0reg);
				}
				else {
					// no free reg
					SSE_MOVLPS_XMM_to_M64(VU_VFx_ADDR(xmmregs[xmmreg].reg), xmmreg);
					SSE_SHUFPS_XMM_to_XMM(xmmreg, xmmreg, 0xc6);
					//SSE_MOVHLPS_XMM_to_XMM(xmmreg, xmmreg);
					SSE_MOVSS_XMM_to_M32(VU_VFx_ADDR(xmmregs[xmmreg].reg)+8, xmmreg);
					SSE_SHUFPS_XMM_to_XMM(xmmreg, xmmreg, 0xc6);
				}
			}
			else {
				SSE_MOVLPS_XMM_to_M64(VU_VFx_ADDR(xmmregs[xmmreg].reg), xmmreg);
			}
		}
		else SSE_MOVAPS_XMM_to_M128(VU_VFx_ADDR(xmmregs[xmmreg].reg), xmmreg);
	}
	else if (xmmregs[xmmreg].type == XMMTYPE_ACC && (xmmregs[xmmreg].mode & MODE_WRITE) ) {
		if( xmmregs[xmmreg].mode & MODE_VUXYZ ) {

			if( xmmregs[xmmreg].mode & MODE_VUZ ) {
				// don't destroy w
				int t0reg;
				for(t0reg = 0; t0reg < XMMREGS; ++t0reg ) {
					if( !xmmregs[t0reg].inuse ) break;
				}

				if( t0reg < XMMREGS ) {
					SSE_MOVHLPS_XMM_to_XMM(t0reg, xmmreg);
					SSE_MOVLPS_XMM_to_M64(VU_ACCx_ADDR, xmmreg);
					SSE_MOVSS_XMM_to_M32(VU_ACCx_ADDR+8, t0reg);
				}
				else {
					// no free reg
					SSE_MOVLPS_XMM_to_M64(VU_ACCx_ADDR, xmmreg);
					SSE_SHUFPS_XMM_to_XMM(xmmreg, xmmreg, 0xc6);
					//SSE_MOVHLPS_XMM_to_XMM(xmmreg, xmmreg);
					SSE_MOVSS_XMM_to_M32(VU_ACCx_ADDR+8, xmmreg);
					SSE_SHUFPS_XMM_to_XMM(xmmreg, xmmreg, 0xc6);
				}
			}
			else {
				SSE_MOVLPS_XMM_to_M64(VU_ACCx_ADDR, xmmreg);
			}
		}
		else SSE_MOVAPS_XMM_to_M128(VU_ACCx_ADDR, xmmreg);
	}
	else if (xmmregs[xmmreg].type == XMMTYPE_GPRREG && (xmmregs[xmmreg].mode & MODE_WRITE) ) {
		assert( xmmregs[xmmreg].reg != 0 );
		//assert( g_xmmtypes[xmmreg] == XMMT_INT );
		SSEX_MOVDQA_XMM_to_M128((u32)&cpuRegs.GPR.r[xmmregs[xmmreg].reg].UL[0], xmmreg);
	}
	else if (xmmregs[xmmreg].type == XMMTYPE_FPREG && (xmmregs[xmmreg].mode & MODE_WRITE)) {
		SSE_MOVSS_XMM_to_M32((u32)&fpuRegs.fpr[xmmregs[xmmreg].reg], xmmreg);
	}
	else if (xmmregs[xmmreg].type == XMMTYPE_FPACC && (xmmregs[xmmreg].mode & MODE_WRITE)) {
		SSE_MOVSS_XMM_to_M32((u32)&fpuRegs.ACC.f, xmmreg);
	}

	xmmregs[xmmreg].mode &= ~(MODE_WRITE|MODE_VUXYZ);
	xmmregs[xmmreg].inuse = 0;
}

int _getNumXMMwrite()
{
	int num = 0, i;
	for (i=0; i<XMMREGS; i++) {
		if( xmmregs[i].inuse && (xmmregs[i].mode&MODE_WRITE) ) ++num;
	}

	return num;
}

u8 _hasFreeXMMreg()
{
	int i;
	for (i=0; i<XMMREGS; i++) {
		if (!xmmregs[i].inuse) return 1;
	}

	// check for dead regs
	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].needed) continue;
		if (xmmregs[i].type == XMMTYPE_GPRREG ) {
			if( !EEINST_ISLIVEXMM(xmmregs[i].reg) ) {
				return 1;
			}
		}
	}

	// check for dead regs
	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].needed) continue;
		if (xmmregs[i].type == XMMTYPE_GPRREG  ) {
			if( !(g_pCurInstInfo->regs[xmmregs[i].reg]&EEINST_USED) ) {
				return 1;
			}
		}
	}
	return 0;
}

void _moveXMMreg(int xmmreg)
{
	int i;
	if( !xmmregs[xmmreg].inuse ) return;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse) continue;
		break;
	}

	if( i == XMMREGS ) {
		_freeXMMreg(xmmreg);
		return;
	}

	// move
	xmmregs[i] = xmmregs[xmmreg];
	xmmregs[xmmreg].inuse = 0;
	SSEX_MOVDQA_XMM_to_XMM(i, xmmreg);
}

void _flushXMMregs()
{
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;

		assert( xmmregs[i].type != XMMTYPE_TEMP );
		assert( xmmregs[i].mode & (MODE_READ|MODE_WRITE) );

		_freeXMMreg(i);
		xmmregs[i].inuse = 1;
		xmmregs[i].mode &= ~MODE_WRITE;
		xmmregs[i].mode |= MODE_READ;
	}
}

void _freeXMMregs()
{
	int i;

	for (i=0; i<XMMREGS; i++) {
		if (xmmregs[i].inuse == 0) continue;

		assert( xmmregs[i].type != XMMTYPE_TEMP );
		//assert( xmmregs[i].mode & (MODE_READ|MODE_WRITE) );

		_freeXMMreg(i);
	}
}

void FreezeXMMRegs_(int save)
{
	assert( g_EEFreezeRegs );

	if( save ) {
		if( g_globalXMMSaved )
			return;
		g_globalXMMSaved = 1;

		__asm {
			movaps xmmword ptr [g_globalXMMData + 0], xmm0
			movaps xmmword ptr [g_globalXMMData + 16], xmm1
			movaps xmmword ptr [g_globalXMMData + 32], xmm2
			movaps xmmword ptr [g_globalXMMData + 48], xmm3
			movaps xmmword ptr [g_globalXMMData + 64], xmm4
			movaps xmmword ptr [g_globalXMMData + 80], xmm5
			movaps xmmword ptr [g_globalXMMData + 96], xmm6
			movaps xmmword ptr [g_globalXMMData + 112], xmm7
		}
	}
	else {
		if( !g_globalXMMSaved )
			return;

		g_globalXMMSaved = 0;

		__asm {
			movaps xmm0, xmmword ptr [g_globalXMMData + 0]
			movaps xmm1, xmmword ptr [g_globalXMMData + 16]
			movaps xmm2, xmmword ptr [g_globalXMMData + 32]
			movaps xmm3, xmmword ptr [g_globalXMMData + 48]
			movaps xmm4, xmmword ptr [g_globalXMMData + 64]
			movaps xmm5, xmmword ptr [g_globalXMMData + 80]
			movaps xmm6, xmmword ptr [g_globalXMMData + 96]
			movaps xmm7, xmmword ptr [g_globalXMMData + 112]
		}
	}
}

// EE
void _eeMoveGPRtoR(x86IntRegType to, int fromgpr)
{
	if( GPR_IS_CONST1(fromgpr) )
		MOV32ItoR( to, g_cpuConstRegs[fromgpr].UL[0] );
	else {
		int mmreg;
		
		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 && (xmmregs[mmreg].mode&MODE_WRITE)) {
			SSE2_MOVD_XMM_to_R(to, mmreg);
		}
		else if( (mmreg = _checkMMXreg(MMX_GPR+fromgpr, MODE_READ)) >= 0 && (mmxregs[mmreg].mode&MODE_WRITE) ) {
			MOVD32MMXtoR(to, mmreg);
			SetMMXstate();
		}
		else {
			MOV32MtoR(to, (int)&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] );
		}
	}
}

void _eeMoveGPRtoM(u32 to, int fromgpr)
{
	if( GPR_IS_CONST1(fromgpr) )
		MOV32ItoM( to, g_cpuConstRegs[fromgpr].UL[0] );
	else {
		int mmreg;
		
		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 ) {
			SSEX_MOVD_XMM_to_M32(to, mmreg);
		}
		else if( (mmreg = _checkMMXreg(MMX_GPR+fromgpr, MODE_READ)) >= 0 ) {
			MOVDMMXtoM(to, mmreg);
			SetMMXstate();
		}
		else {
			MOV32MtoR(EAX, (int)&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] );
			MOV32RtoM(to, EAX );
		}
	}
}

void _eeMoveGPRtoRm(x86IntRegType to, int fromgpr)
{
	if( GPR_IS_CONST1(fromgpr) )
		MOV32ItoRmOffset( to, g_cpuConstRegs[fromgpr].UL[0], 0 );
	else {
		int mmreg;
		
		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 ) {
			SSEX_MOVD_XMM_to_Rm(to, mmreg);
		}
		else if( (mmreg = _checkMMXreg(MMX_GPR+fromgpr, MODE_READ)) >= 0 ) {
			MOVD32MMXtoRm(to, mmreg);
			SetMMXstate();
		}
		else {
			MOV32MtoR(EAX, (int)&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] );
			MOV32RtoRm(to, EAX );
		}
	}
}

// PSX 
void _psxMoveGPRtoR(x86IntRegType to, int fromgpr)
{
	if( PSX_IS_CONST1(fromgpr) )
		MOV32ItoR( to, g_psxConstRegs[fromgpr] );
	else {
		// check x86
		MOV32MtoR(to, (int)&psxRegs.GPR.r[ fromgpr ] );
	}
}

void _psxMoveGPRtoM(u32 to, int fromgpr)
{
	if( PSX_IS_CONST1(fromgpr) )
		MOV32ItoM( to, g_psxConstRegs[fromgpr] );
	else {
		// check x86
		MOV32MtoR(EAX, (int)&psxRegs.GPR.r[ fromgpr ] );
		MOV32RtoM(to, EAX );
	}
}

void _psxMoveGPRtoRm(x86IntRegType to, int fromgpr)
{
	if( PSX_IS_CONST1(fromgpr) )
		MOV32ItoRmOffset( to, g_psxConstRegs[fromgpr], 0 );
	else {
		// check x86
		MOV32MtoR(EAX, (int)&psxRegs.GPR.r[ fromgpr ] );
		MOV32RtoRm(to, EAX );
	}
}

void _recPushReg(int mmreg)
{
	if( IS_XMMREG(mmreg) ) {
		SUB32ItoR(ESP, 4);
		SSEX_MOVD_XMM_to_Rm(ESP, mmreg&0xf);
	}
	else if( IS_MMXREG(mmreg) ) {
		SUB32ItoR(ESP, 4);
		MOVD32MMXtoRm(ESP, mmreg&0xf);
	}
	else if( IS_CONSTREG(mmreg) ) {
		PUSH32I(g_cpuConstRegs[(mmreg>>16)&0x1f].UL[0]);
	}
	else if( IS_PSXCONSTREG(mmreg) ) {
		PUSH32I(g_psxConstRegs[(mmreg>>16)&0x1f]);
	}
	else PUSH32R(mmreg);
}

void _signExtendSFtoM(u32 mem)
{
	LAHF();
	SAR16ItoR(EAX, 15);
	CWDE();
	MOV32RtoM(mem, EAX );
}

int _signExtendMtoMMX(x86MMXRegType to, u32 mem)
{
	int t0reg = _allocMMXreg(-1, MMX_TEMP, 0);
	MOVDMtoMMX(t0reg, mem);
	MOVQRtoR(to, t0reg);
	PSRADItoR(t0reg, 31);
	PUNPCKLDQRtoR(to, t0reg);
	_freeMMXreg(t0reg);
	return to;
}

int _signExtendGPRMMXtoMMX(x86MMXRegType to, u32 gprreg, x86MMXRegType from, u32 gprfromreg)
{
	assert( to >= 0 && from >= 0 );
	if( !EEINST_ISLIVE1(gprreg) ) {
		EEINST_RESETHASLIVE1(gprreg);
		if( to != from ) MOVQRtoR(to, from);
		return to;
	}

	if( to == from ) return _signExtendGPRtoMMX(to, gprreg, 0);
	if( !(g_pCurInstInfo->regs[gprfromreg]&EEINST_LASTUSE) ) {
		if( EEINST_ISLIVEMMX(gprfromreg) ) {
			MOVQRtoR(to, from);
			return _signExtendGPRtoMMX(to, gprreg, 0);
		}
	}

	// from is free for use
	SetMMXstate();

	if( g_pCurInstInfo->regs[gprreg] & EEINST_MMX ) {
		
		if( EEINST_ISLIVEMMX(gprfromreg) ) {
			_freeMMXreg(from);
		}

		MOVQRtoR(to, from);
		PSRADItoR(from, 31);
		PUNPCKLDQRtoR(to, from);
		return to;
	}
	else {
		MOVQRtoR(to, from);
		MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[0], from);
		PSRADItoR(from, 31);
		MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[1], from);
		mmxregs[to].inuse = 0;
		return -1;
	}

	assert(0);
}

int _signExtendGPRtoMMX(x86MMXRegType to, u32 gprreg, int shift)
{
	assert( to >= 0 && shift >= 0 );
	if( !EEINST_ISLIVE1(gprreg) ) {
		if( shift > 0 ) PSRADItoR(to, shift);
		EEINST_RESETHASLIVE1(gprreg);
		return to;
	}

	SetMMXstate();

	if( g_pCurInstInfo->regs[gprreg] & EEINST_MMX ) {
		if( _hasFreeMMXreg() ) {
			int t0reg = _allocMMXreg(-1, MMX_TEMP, 0);
			MOVQRtoR(t0reg, to);
			PSRADItoR(to, 31);
			if( shift > 0 ) PSRADItoR(t0reg, shift);
			PUNPCKLDQRtoR(t0reg, to);

			// swap mmx regs.. don't ask
			mmxregs[t0reg] = mmxregs[to];
			mmxregs[to].inuse = 0;
			return t0reg;
		}
		else {
			// will be used in the future as mmx
			if( shift > 0 ) PSRADItoR(to, shift);
			MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[0], to);
			PSRADItoR(to, 31);
			MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[1], to);

			// read again
			MOVQMtoR(to, (u32)&cpuRegs.GPR.r[gprreg].UL[0]);
			mmxregs[to].mode &= ~MODE_WRITE;
			return to;
		}
	}
	else {
		if( shift > 0 ) PSRADItoR(to, shift);
		MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[0], to);
		PSRADItoR(to, 31);
		MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[1], to);
		mmxregs[to].inuse = 0;
		return -1;
	}

	assert(0);
}

__declspec(align(16)) u32 s_zeros[4] = {0};
int _signExtendXMMtoM(u32 to, x86SSERegType from, int candestroy)
{
	int t0reg;
//	if( g_xmmtypes[from] == XMMT_FPS && (!candestroy || _hasFreeXMMreg()) ) {
//		// special floating point implementation
//		// NOTE: doesn't sign extend 0x80000000
//		xmmregs[from].needed = 1;
//		t0reg = _allocTempXMMreg(XMMT_FPS, -1);
//		SSE_XORPS_XMM_to_XMM(t0reg, t0reg);
//		SSE_MOVSS_XMM_to_M32(to, from);
//		SSE_CMPNLESS_XMM_to_XMM(t0reg, from);
//		SSE_MOVSS_XMM_to_M32(to+4, t0reg);
//		_freeXMMreg(t0reg);
//		return 0;
//	}
//	else {
	g_xmmtypes[from] = XMMT_INT;
		if( candestroy ) {
			if( g_xmmtypes[from] == XMMT_FPS || !cpucaps.hasStreamingSIMD2Extensions ) SSE_MOVSS_XMM_to_M32(to, from);
			else SSE2_MOVD_XMM_to_M32(to, from);

//			if( g_xmmtypes[from] == XMMT_FPS ) {
//				SSE_CMPLTSS_M32_to_XMM(from, (u32)&s_zeros[0]);
//				SSE_MOVSS_XMM_to_M32(to+4, from);
//				return 1;
//			}
//			else {
				if( cpucaps.hasStreamingSIMD2Extensions ) {
					SSE2_PSRAD_I8_to_XMM(from, 31);
					SSE2_MOVD_XMM_to_M32(to+4, from);
					return 1;
				}
				else {
					SSE_MOVSS_XMM_to_M32(to+4, from);
					SAR32ItoM(to+4, 31);
					return 0;
				}
//			}
		}
		else {
			// can't destroy and type is int
			assert( g_xmmtypes[from] == XMMT_INT );

			if( cpucaps.hasStreamingSIMD2Extensions ) {
				if( _hasFreeXMMreg() ) {
					xmmregs[from].needed = 1;
					t0reg = _allocTempXMMreg(XMMT_INT, -1);
					SSEX_MOVDQA_XMM_to_XMM(t0reg, from);
					SSE2_PSRAD_I8_to_XMM(from, 31);
					SSE2_MOVD_XMM_to_M32(to, t0reg);
					SSE2_MOVD_XMM_to_M32(to+4, from);

					// swap xmm regs.. don't ask
					xmmregs[t0reg] = xmmregs[from];
					xmmregs[from].inuse = 0;
				}
				else {
					SSE2_MOVD_XMM_to_M32(to+4, from);
					SSE2_MOVD_XMM_to_M32(to, from);
					SAR32ItoM(to+4, 31);
				}
			}
			else {
				SSE_MOVSS_XMM_to_M32(to+4, from);
				SSE_MOVSS_XMM_to_M32(to, from);
				SAR32ItoM(to+4, 31);
			}

			return 0;
		}
	//}

	assert(0);
}

int _allocCheckGPRtoXMM(EEINST* pinst, int gprreg, int mode)
{
	if( pinst->regs[gprreg] & EEINST_XMM ) return _allocGPRtoXMMreg(-1, gprreg, mode);
	
	return _checkXMMreg(XMMTYPE_GPRREG, gprreg, mode);
}

int _allocCheckFPUtoXMM(EEINST* pinst, int fpureg, int mode)
{
	if( pinst->fpuregs[fpureg] & EEINST_XMM ) return _allocFPtoXMMreg(-1, fpureg, mode);
	
	return _checkXMMreg(XMMTYPE_FPREG, fpureg, mode);
}

int _allocCheckGPRtoMMX(EEINST* pinst, int reg, int mode)
{
	if( pinst->regs[reg] & EEINST_MMX ) return _allocMMXreg(-1, MMX_GPR+reg, mode);
		
	return _checkMMXreg(MMX_GPR+reg, mode);
}

void _recClearInst(EEINST* pinst)
{
	memset(&pinst->regs[0], EEINST_LIVE0|EEINST_LIVE1|EEINST_LIVE2, sizeof(pinst->regs));
	memset(&pinst->fpuregs[0], EEINST_LIVE0, sizeof(pinst->fpuregs));
	memset(&pinst->info, 0, sizeof(EEINST)-sizeof(pinst->regs)-sizeof(pinst->fpuregs));
}

// returns nonzero value if reg has been written between [startpc, endpc-4]
int _recIsRegWritten(EEINST* pinst, int size, u8 xmmtype, u8 reg)
{
	int i, inst = 1;
	while(size-- > 0) {
		for(i = 0; i < ARRAYSIZE(pinst->writeType); ++i) {
			if( pinst->writeType[i] == xmmtype && pinst->writeReg[i] == reg )
				return inst;
		}
		++inst;
		pinst++;
	}

	return 0;
}

int _recIsRegUsed(EEINST* pinst, int size, u8 xmmtype, u8 reg)
{
	int i, inst = 1;
	while(size-- > 0) {
		for(i = 0; i < ARRAYSIZE(pinst->writeType); ++i) {
			if( pinst->writeType[i] == xmmtype && pinst->writeReg[i] == reg )
				return inst;
		}
		for(i = 0; i < ARRAYSIZE(pinst->readType); ++i) {
			if( pinst->readType[i] == xmmtype && pinst->readReg[i] == reg )
				return inst;
		}
		++inst;
		pinst++;
	}

	return 0;
}

void _recFillRegister(EEINST* pinst, int type, int reg, int write)
{
	int i = 0;
	if( write ) {
		for(i = 0; i < ARRAYSIZE(pinst->writeType); ++i) {
			if( pinst->writeType[i] == XMMTYPE_TEMP ) {
				pinst->writeType[i] = type;
				pinst->writeReg[i] = reg;
				return;
			}
		}
		assert(0);
	}
	else {
		for(i = 0; i < ARRAYSIZE(pinst->readType); ++i) {
			if( pinst->readType[i] == XMMTYPE_TEMP ) {
				pinst->readType[i] = type;
				pinst->readReg[i] = reg;
				return;
			}
		}
		assert(0);
	}
}

// Writebacks //
void _recClearWritebacks()
{
}

void _recAddWriteBack(int cycle, u32 viwrite, EEINST* parent)
{
}

EEINSTWRITEBACK* _recCheckWriteBack(int cycle)
{
	return NULL;
}

__declspec(align(16)) static u32 s_ones[2] = {0xffffffff, 0xffffffff};

void LogicalOpRtoR(x86MMXRegType to, x86MMXRegType from, int op)
{
	switch(op) {
		case 0: PANDRtoR(to, from); break;
		case 1: PORRtoR(to, from); break;
		case 2: PXORRtoR(to, from); break;
		case 3:
			PORRtoR(to, from);
			PXORMtoR(to, (u32)&s_ones[0]);
			break;
	}
}

void LogicalOpMtoR(x86MMXRegType to, u32 from, int op)
{
	switch(op) {
		case 0: PANDMtoR(to, from); break;
		case 1: PORMtoR(to, from); break;
		case 2: PXORMtoR(to, from); break;
		case 3:
			PORRtoR(to, from);
			PXORMtoR(to, (u32)&s_ones[0]);
			break;
	}
}

void LogicalOp32RtoM(u32 to, x86IntRegType from, int op)
{
	switch(op) {
		case 0: AND32RtoM(to, from); break;
		case 1: OR32RtoM(to, from); break;
		case 2: XOR32RtoM(to, from); break;
		case 3: OR32RtoM(to, from); break;
	}
}

void LogicalOp32MtoR(x86IntRegType to, u32 from, int op)
{
	switch(op) {
		case 0: AND32MtoR(to, from); break;
		case 1: OR32MtoR(to, from); break;
		case 2: XOR32MtoR(to, from); break;
		case 3: OR32MtoR(to, from); break;
	}
}

void LogicalOp32ItoR(x86IntRegType to, u32 from, int op)
{
	switch(op) {
		case 0: AND32ItoR(to, from); break;
		case 1: OR32ItoR(to, from); break;
		case 2: XOR32ItoR(to, from); break;
		case 3: OR32ItoR(to, from); break;
	}
}

void LogicalOp32ItoM(u32 to, u32 from, int op)
{
	switch(op) {
		case 0: AND32ItoM(to, from); break;
		case 1: OR32ItoM(to, from); break;
		case 2: XOR32ItoM(to, from); break;
		case 3: OR32ItoM(to, from); break;
	}
}

using namespace std;

struct BASEBLOCKS
{
	// 0 - ee, 1 - iop
	inline void Add(BASEBLOCKEX*);
	inline void Remove(BASEBLOCKEX*);
	inline int Get(u32 startpc);
	inline void Reset();

	inline BASEBLOCKEX** GetAll(int* pnum);

	vector<BASEBLOCKEX*> blocks;
};

void BASEBLOCKS::Add(BASEBLOCKEX* pex)
{
	assert( pex != NULL );

	switch(blocks.size()) {
		case 0:
			blocks.push_back(pex);
			return;
		case 1:
			assert( blocks.front()->startpc != pex->startpc );

			if( blocks.front()->startpc < pex->startpc ) {
				blocks.push_back(pex);
			}
			else blocks.insert(blocks.begin(), pex);

			return;

		default:
		{
			int imin = 0, imax = blocks.size(), imid;

			while(imin < imax) {
				imid = (imin+imax)>>1;

				if( blocks[imid]->startpc > pex->startpc ) imax = imid;
				else imin = imid+1;
			}

			assert( imin == blocks.size() || blocks[imin]->startpc > pex->startpc );
			if( imin > 0 ) assert( blocks[imin-1]->startpc < pex->startpc );
			blocks.insert(blocks.begin()+imin, pex);

			return;
		}
	}
}

int BASEBLOCKS::Get(u32 startpc)
{
	switch(blocks.size()) {
		case 1:
			return 0;
		case 2:
			return blocks.front()->startpc < startpc;

		default:
		{
			int imin = 0, imax = blocks.size()-1, imid;

			while(imin < imax) {
				imid = (imin+imax)>>1;

				if( blocks[imid]->startpc > startpc ) imax = imid;
				else if( blocks[imid]->startpc == startpc ) return imid;
				else imin = imid+1;
			}

			assert( blocks[imin]->startpc == startpc );
			return imin;
		}
	}
}

void BASEBLOCKS::Remove(BASEBLOCKEX* pex)
{
	assert( pex != NULL );
	int i = Get(pex->startpc);
	assert( blocks[i] == pex ); 
	blocks.erase(blocks.begin()+i);
}

void BASEBLOCKS::Reset()
{
	blocks.resize(0);
	blocks.reserve(512);
}

BASEBLOCKEX** BASEBLOCKS::GetAll(int* pnum)
{
	assert( pnum != NULL );
	*pnum = blocks.size();
	return &blocks[0];
}

static BASEBLOCKS s_vecBaseBlocksEx[2];

void AddBaseBlockEx(BASEBLOCKEX* pex, int cpu)
{
	s_vecBaseBlocksEx[cpu].Add(pex);
}

BASEBLOCKEX* GetBaseBlockEx(u32 startpc, int cpu)
{
	return s_vecBaseBlocksEx[cpu].blocks[s_vecBaseBlocksEx[cpu].Get(startpc)];
}

void RemoveBaseBlockEx(BASEBLOCKEX* pex, int cpu)
{
	s_vecBaseBlocksEx[cpu].Remove(pex);
}

void ResetBaseBlockEx(int cpu)
{
	s_vecBaseBlocksEx[cpu].Reset();
}

BASEBLOCKEX** GetAllBaseBlocks(int* pnum, int cpu)
{
	return s_vecBaseBlocksEx[cpu].GetAll(pnum);
}
