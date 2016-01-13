/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"

#include "System.h"
#include "iR5900.h"
#include "Vif.h"
#include "VU.h"
#include "x86emitter/x86emitter.h"
#include "R3000A.h"

using namespace x86Emitter;

// yay sloppy crap needed until we can remove dependency on this hippopotamic
// landmass of shared code. (air)
extern u32 g_psxConstRegs[32];

u16 x86FpuState;
static u16 g_mmxAllocCounter = 0;

// X86 caching
static int g_x86checknext;

// use special x86 register allocation for ia32

void _initX86regs() {
	memzero(x86regs);
	g_x86AllocCounter = 0;
	g_x86checknext = 0;
}

uptr _x86GetAddr(int type, int reg)
{
	uptr ret = 0;

	switch(type&~X86TYPE_VU1)
	{
		case X86TYPE_GPR:
			ret = (uptr)&cpuRegs.GPR.r[reg];
			break;

		case X86TYPE_VI:
			if (type & X86TYPE_VU1)
				ret = (uptr)&VU1.VI[reg];
			else
				ret = (uptr)&VU0.VI[reg];
			break;

		case X86TYPE_MEMOFFSET:
			ret = 0;
			break;

		case X86TYPE_VIMEMOFFSET:
			ret = 0;
			break;

		case X86TYPE_VUQREAD:
			if  (type & X86TYPE_VU1)
				ret = (uptr)&VU1.VI[REG_Q];
			else
				ret = (uptr)&VU0.VI[REG_Q];
			break;

		case X86TYPE_VUPREAD:
			if  (type & X86TYPE_VU1)
				ret = (uptr)&VU1.VI[REG_P];
			else
				ret = (uptr)&VU0.VI[REG_P];
			break;

		case X86TYPE_VUQWRITE:
			if  (type & X86TYPE_VU1)
				ret = (uptr)&VU1.q;
			else
				ret = (uptr)&VU0.q;
			break;

		case X86TYPE_VUPWRITE:
			if  (type & X86TYPE_VU1)
				ret = (uptr)&VU1.p;
			else
				ret = (uptr)&VU0.p;
			break;

		case X86TYPE_PSX:
			ret = (uptr)&psxRegs.GPR.r[reg];
			break;

		case X86TYPE_PCWRITEBACK:
			ret = (uptr)&g_recWriteback;
			break;

		case X86TYPE_VUJUMP:
			ret = (uptr)&g_recWriteback;
			break;

		jNO_DEFAULT;
	}

	return ret;
}

int _getFreeX86reg(int mode)
{
	int tempi = -1;
	u32 bestcount = 0x10000;

	int maxreg = (mode&MODE_8BITREG)?4:iREGCNT_GPR;

	for (uint i=0; i<iREGCNT_GPR; i++) {
		int reg = (g_x86checknext+i)%iREGCNT_GPR;
		if( reg == 0 || reg == esp.GetId() || reg == ebp.GetId() ) continue;
		if( reg >= maxreg ) continue;
		//if( (mode&MODE_NOFRAME) && reg==EBP ) continue;

		if (x86regs[reg].inuse == 0) {
			g_x86checknext = (reg+1)%iREGCNT_GPR;
			return reg;
		}
	}

	for (int i=1; i<maxreg; i++) {
		if( i == esp.GetId()  || i==ebp.GetId()) continue;
		//if( (mode&MODE_NOFRAME) && i==EBP ) continue;

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

	pxFailDev( "x86 register allocation error" );
	throw Exception::FailedToAllocateRegister();
}

void _flushCachedRegs()
{
	_flushConstRegs();
	_flushMMXregs();
	_flushXMMregs();
}

void _flushConstReg(int reg)
{
	if( GPR_IS_CONST1( reg ) && !(g_cpuFlushedConstReg&(1<<reg)) ) {
		xMOV(ptr32[&cpuRegs.GPR.r[reg].UL[0]], g_cpuConstRegs[reg].UL[0]);
		xMOV(ptr32[&cpuRegs.GPR.r[reg].UL[1]], g_cpuConstRegs[reg].UL[1]);
		g_cpuFlushedConstReg |= (1<<reg);
		if (reg == 0) DevCon.Warning("Flushing r0!");
	}
}

void _flushConstRegs()
{
	s32 zero_cnt = 0, minusone_cnt = 0;
	s32 eaxval = 1; // 0, -1
	u32 done[4] = {0, 0, 0, 0};
	u8* rewindPtr;

	// flush constants

	// flush 0 and -1 first
	// ignore r0
	for (int i = 1, j = 0; i < 32; j++ && ++i, j %= 2) {
		if (!GPR_IS_CONST1(i) || g_cpuFlushedConstReg & (1<<i)) continue;
		if (g_cpuConstRegs[i].SL[j] != 0) continue;

		if (eaxval != 0) {
			xXOR(eax, eax);
			eaxval = 0;
		}

		xMOV(ptr[&cpuRegs.GPR.r[i].SL[j]], eax);
		done[j] |= 1<<i;
		zero_cnt++;
	}

	rewindPtr = x86Ptr;

	for (int i = 1, j = 0; i < 32; j++ && ++i, j %= 2) {
		if (!GPR_IS_CONST1(i) || g_cpuFlushedConstReg & (1<<i)) continue;
		if (g_cpuConstRegs[i].SL[j] != -1) continue;

		if (eaxval > 0) {
			xXOR(eax, eax);
			eaxval = 0;
		}
		if (eaxval == 0) {
			xNOT(eax);
			eaxval = -1;
		}

		xMOV(ptr[&cpuRegs.GPR.r[i].SL[j]], eax);
		done[j + 2] |= 1<<i;
		minusone_cnt++;
	}

	if (minusone_cnt == 1 && !zero_cnt) { // not worth it for one byte
		x86SetPtr( rewindPtr );
	} else {
		done[0] |= done[2];
		done[1] |= done[3];
	}

	for (int i = 1; i < 32; ++i) {
		if (GPR_IS_CONST1(i)) {
			if (!(g_cpuFlushedConstReg&(1<<i))) {
				if (!(done[0] & (1<<i)))
					xMOV(ptr32[&cpuRegs.GPR.r[i].UL[0]], g_cpuConstRegs[i].UL[0]);
				if (!(done[1] & (1<<i)))
					xMOV(ptr32[&cpuRegs.GPR.r[i].UL[1]], g_cpuConstRegs[i].UL[1]);

				g_cpuFlushedConstReg |= 1<<i;
			}
		if (g_cpuHasConstReg == g_cpuFlushedConstReg) break;
		}
	}
}

int _allocX86reg(xRegister32 x86reg, int type, int reg, int mode)
{
	uint i;
	pxAssertDev( reg >= 0 && reg < 32, "Register index out of bounds." );
	pxAssertDev( x86reg != esp && x86reg != ebp, "Allocation of ESP/EBP is not allowed!" );

	// don't alloc EAX and ESP,EBP if MODE_NOFRAME
	int oldmode = mode;
	//int noframe = mode & MODE_NOFRAME;
	uint maxreg = (mode & MODE_8BITREG) ? 4 : iREGCNT_GPR;
	mode &= ~(MODE_NOFRAME|MODE_8BITREG);
	int readfromreg = -1;

	if ( type != X86TYPE_TEMP ) {
		if ( maxreg < iREGCNT_GPR ) {
			// make sure reg isn't in the higher regs

			for(i = maxreg; i < iREGCNT_GPR; ++i) {
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
			if ( i == esp.GetId() || i == ebp.GetId() ) continue;
			if (!x86regs[i].inuse || x86regs[i].type != type || x86regs[i].reg != reg) continue;

			if( i >= maxreg ) {
				if (x86regs[i].mode & MODE_READ) readfromreg = i;

				mode |= x86regs[i].mode&MODE_WRITE;
				x86regs[i].inuse = 0;
				break;
			}

			if( !x86reg.IsEmpty() ) {
				// requested specific reg, so return that instead
				if( i != (uint)x86reg.GetId() ) {
					if( x86regs[i].mode & MODE_READ ) readfromreg = i;
					mode |= x86regs[i].mode&MODE_WRITE;
					x86regs[i].inuse = 0;
					break;
				}
			}

			if( type != X86TYPE_TEMP && !(x86regs[i].mode & MODE_READ) && (mode&MODE_READ)) {

				if( type == X86TYPE_GPR ) _flushConstReg(reg);

				if( X86_ISVI(type) && reg < 16 )
					xMOVZX(xRegister32(i), ptr16[(u16*)(_x86GetAddr(type, reg))]);
				else
					xMOV(xRegister32(i), ptr[(void*)(_x86GetAddr(type, reg))]);

				x86regs[i].mode |= MODE_READ;
			}

			x86regs[i].needed = 1;
			x86regs[i].mode|= mode;
			return i;
		}
	}

	if (x86reg.IsEmpty())
		x86reg = xRegister32(_getFreeX86reg(oldmode));
	else
		_freeX86reg(x86reg);

	x86regs[x86reg.GetId()].type = type;
	x86regs[x86reg.GetId()].reg = reg;
	x86regs[x86reg.GetId()].mode = mode;
	x86regs[x86reg.GetId()].needed = 1;
	x86regs[x86reg.GetId()].inuse = 1;

	if( mode & MODE_READ ) {
		if( readfromreg >= 0 )
			xMOV(x86reg, xRegister32(readfromreg));
		else {
			if( type == X86TYPE_GPR ) {

				if( reg == 0 ) {
					xXOR(x86reg, x86reg);
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
					if( reg == 0 )
						xXOR(x86reg, x86reg);
					else
						xMOVZX(x86reg, ptr16[(u16*)(_x86GetAddr(type, reg))]);
				}
				else xMOV(x86reg, ptr[(void*)(_x86GetAddr(type, reg))]);
			}
		}
	}

	// Need to port all the code
	// return x86reg;
	return x86reg.GetId();
}

int _checkX86reg(int type, int reg, int mode)
{
	uint i;

	for (i=0; i<iREGCNT_GPR; i++) {
		if (x86regs[i].inuse && x86regs[i].reg == reg && x86regs[i].type == type) {

			if( !(x86regs[i].mode & MODE_READ) && (mode&MODE_READ) ) {
				if( X86_ISVI(type) )
					xMOVZX(xRegister32(i), ptr16[(u16*)(_x86GetAddr(type, reg))]);
				else
					xMOV(xRegister32(i), ptr[(void*)(_x86GetAddr(type, reg))]);
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
	uint i;

	for (i=0; i<iREGCNT_GPR; i++) {
		if (!x86regs[i].inuse || x86regs[i].reg != reg || x86regs[i].type != type ) continue;

		x86regs[i].counter = g_x86AllocCounter++;
		x86regs[i].needed = 1;
	}
}

void _clearNeededX86regs() {
	uint i;

	for (i=0; i<iREGCNT_GPR; i++) {
		if (x86regs[i].needed ) {
			if( x86regs[i].inuse && (x86regs[i].mode&MODE_WRITE) )
				x86regs[i].mode |= MODE_READ;
		}
		x86regs[i].needed = 0;
	}
}

void _deleteX86reg(int type, int reg, int flush)
{
	uint i;

	for (i=0; i<iREGCNT_GPR; i++) {
		if (x86regs[i].inuse && x86regs[i].reg == reg && x86regs[i].type == type) {
			switch(flush) {
				case 0:
					_freeX86reg(i);
					break;

				case 1:
					if( x86regs[i].mode & MODE_WRITE) {

						if( X86_ISVI(type) && x86regs[i].reg < 16 )
							xMOV(ptr[(void*)(_x86GetAddr(type, x86regs[i].reg))], xRegister16(i));
						else
							xMOV(ptr[(void*)(_x86GetAddr(type, x86regs[i].reg))], xRegister32(i));

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

// Temporary solution to support eax/ebx... type
void _freeX86reg(const x86Emitter::xRegister32& x86reg)
{
	_freeX86reg(x86reg.GetId());
}

void _freeX86reg(int x86reg)
{
	pxAssert( x86reg >= 0 && x86reg < (int)iREGCNT_GPR );

	if( x86regs[x86reg].inuse && (x86regs[x86reg].mode&MODE_WRITE) ) {
		x86regs[x86reg].mode &= ~MODE_WRITE;

		if( X86_ISVI(x86regs[x86reg].type) && x86regs[x86reg].reg < 16 ) {
			xMOV(ptr[(void*)(_x86GetAddr(x86regs[x86reg].type, x86regs[x86reg].reg))], xRegister16(x86reg));
		}
		else
			xMOV(ptr[(void*)(_x86GetAddr(x86regs[x86reg].type, x86regs[x86reg].reg))], xRegister32(x86reg));
	}

	x86regs[x86reg].inuse = 0;
}

void _freeX86regs()
{
	for (uint i=0; i<iREGCNT_GPR; i++)
		_freeX86reg(i);
}

// MMX Caching
_mmxregs mmxregs[8], s_saveMMXregs[8];
static int s_mmxchecknext = 0;

void _initMMXregs()
{
	memzero(mmxregs);
	g_mmxAllocCounter = 0;
	s_mmxchecknext = 0;
}

__fi void* _MMXGetAddr(int reg)
{
	pxAssert( reg != MMX_TEMP );

	if( reg == MMX_LO ) return &cpuRegs.LO;
	if( reg == MMX_HI ) return &cpuRegs.HI;
	if( reg == MMX_FPUACC ) return &fpuRegs.ACC;

	if( reg >= MMX_GPR && reg < MMX_GPR+32 ) return &cpuRegs.GPR.r[reg&31];
	if( reg >= MMX_FPU && reg < MMX_FPU+32 ) return &fpuRegs.fpr[reg&31];
	if( reg >= MMX_COP0 && reg < MMX_COP0+32 ) return &cpuRegs.CP0.r[reg&31];

	pxAssume( false );
	return NULL;
}

int  _getFreeMMXreg()
{
	uint i;
	int tempi = -1;
	u32 bestcount = 0x10000;

	for (i=0; i<iREGCNT_MMX; i++) {
		if (mmxregs[(s_mmxchecknext+i)%iREGCNT_MMX].inuse == 0) {
			int ret = (s_mmxchecknext+i)%iREGCNT_MMX;
			s_mmxchecknext = (s_mmxchecknext+i+1)%iREGCNT_MMX;
			return ret;
		}
	}

	// check for dead regs
	for (i=0; i<iREGCNT_MMX; i++) {
		if (mmxregs[i].needed) continue;
		if (MMX_ISGPR(mmxregs[i].reg)) {
			if( !(g_pCurInstInfo->regs[mmxregs[i].reg-MMX_GPR] & (EEINST_LIVE0)) ) {
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
	for (i=0; i<iREGCNT_MMX; i++) {
		if (mmxregs[i].needed) continue;
		if (MMX_ISGPR(mmxregs[i].reg)) {
			_freeMMXreg(i);
			return i;
		}
	}

	for (i=0; i<iREGCNT_MMX; i++) {
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

	pxFailDev( "mmx register allocation error" );
	throw Exception::FailedToAllocateRegister();
}

int _allocMMXreg(int mmxreg, int reg, int mode)
{
	uint i;

	if( reg != MMX_TEMP ) {
		for (i=0; i<iREGCNT_MMX; i++) {
			if (mmxregs[i].inuse == 0 || mmxregs[i].reg != reg ) continue;

			if( MMX_ISGPR(reg)) {
				pxAssert( _checkXMMreg(XMMTYPE_GPRREG, reg-MMX_GPR, 0) == -1 );
			}

			mmxregs[i].needed = 1;

			if( !(mmxregs[i].mode & MODE_READ) && (mode&MODE_READ) && reg != MMX_TEMP ) {

				SetMMXstate();
				if( reg == MMX_GPR ) {
					// moving in 0s
					xPXOR(xRegisterMMX(i), xRegisterMMX(i));
				}
				else {
					if( MMX_ISGPR(reg) ) _flushConstReg(reg-MMX_GPR);
					if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) )
						xMOVDZX(xRegisterMMX(i), ptr[(_MMXGetAddr(reg))]);
					else
						xMOVQ(xRegisterMMX(i), ptr[(_MMXGetAddr(reg))]);
				}

				mmxregs[i].mode |= MODE_READ;
			}

			mmxregs[i].counter = g_mmxAllocCounter++;
			mmxregs[i].mode|= mode;
			return i;
		}
	}

	if (mmxreg == -1)
		mmxreg = _getFreeMMXreg();

	mmxregs[mmxreg].inuse = 1;
	mmxregs[mmxreg].reg = reg;
	mmxregs[mmxreg].mode = mode&~MODE_READHALF;
	mmxregs[mmxreg].needed = 1;
	mmxregs[mmxreg].counter = g_mmxAllocCounter++;

	SetMMXstate();
	if( reg == MMX_GPR ) {
		// moving in 0s
		xPXOR(xRegisterMMX(mmxreg), xRegisterMMX(mmxreg));
	}
	else {
		int xmmreg;
		if( MMX_ISGPR(reg) && (xmmreg = _checkXMMreg(XMMTYPE_GPRREG, reg-MMX_GPR, 0)) >= 0 ) {
			xMOVH.PS(ptr[(void*)((uptr)_MMXGetAddr(reg)+8)], xRegisterSSE(xmmreg));
			if( mode & MODE_READ )
				xMOVQ(xRegisterMMX(mmxreg), xRegisterSSE(xmmreg));

			if( xmmregs[xmmreg].mode & MODE_WRITE )
				mmxregs[mmxreg].mode |= MODE_WRITE;

			// don't flush
			xmmregs[xmmreg].inuse = 0;
		}
		else {
			if( MMX_ISGPR(reg) ) {
				if(mode&(MODE_READHALF|MODE_READ)) _flushConstReg(reg-MMX_GPR);
			}

			if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) ) {
				xMOVDZX(xRegisterMMX(mmxreg), ptr[(_MMXGetAddr(reg))]);
			}
			else if( mode & MODE_READ ) {
				xMOVQ(xRegisterMMX(mmxreg), ptr[(_MMXGetAddr(reg))]);
			}
		}
	}

	return mmxreg;
}

int _checkMMXreg(int reg, int mode)
{
	uint i;
	for (i=0; i<iREGCNT_MMX; i++) {
		if (mmxregs[i].inuse && mmxregs[i].reg == reg ) {

			if( !(mmxregs[i].mode & MODE_READ) && (mode&MODE_READ) ) {

				if( reg == MMX_GPR ) {
					// moving in 0s
					xPXOR(xRegisterMMX(i), xRegisterMMX(i));
				}
				else {
					if (MMX_ISGPR(reg) && (mode&(MODE_READHALF|MODE_READ))) _flushConstReg(reg-MMX_GPR);
					if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) )
						xMOVDZX(xRegisterMMX(i), ptr[(_MMXGetAddr(reg))]);
					else
						xMOVQ(xRegisterMMX(i), ptr[(_MMXGetAddr(reg))]);
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
	uint i;

	for (i=0; i<iREGCNT_MMX; i++) {
		if (mmxregs[i].inuse == 0) continue;
		if (mmxregs[i].reg != reg) continue;

		mmxregs[i].counter = g_mmxAllocCounter++;
		mmxregs[i].needed = 1;
	}
}

void _clearNeededMMXregs()
{
	uint i;

	for (i=0; i<iREGCNT_MMX; i++) {
		if( mmxregs[i].needed ) {
			// setup read to any just written regs
			if( mmxregs[i].inuse && (mmxregs[i].mode&MODE_WRITE) )
				mmxregs[i].mode |= MODE_READ;
			mmxregs[i].needed = 0;
		}
	}
}

void _deleteMMXreg(int reg, int flush)
{
	uint i;
	for (i=0; i<iREGCNT_MMX; i++) {

		if (mmxregs[i].inuse && mmxregs[i].reg == reg ) {

			switch(flush) {
				case 0: // frees all of the reg
					_freeMMXreg(i);
					break;
				case 1: // flushes all of the reg
					if( mmxregs[i].mode & MODE_WRITE) {
						pxAssert( mmxregs[i].reg != MMX_GPR );

						if( MMX_IS32BITS(reg) )
							xMOVD(ptr[(_MMXGetAddr(mmxregs[i].reg))], xRegisterMMX(i));
						else
							xMOVQ(ptr[(_MMXGetAddr(mmxregs[i].reg))], xRegisterMMX(i));
						SetMMXstate();

						// get rid of MODE_WRITE since don't want to flush again
						mmxregs[i].mode &= ~MODE_WRITE;
						mmxregs[i].mode |= MODE_READ;
					}
					return;
				case 2: // just stops using the reg (no flushing)
					mmxregs[i].inuse = 0;
					break;
			}
			return;
		}
	}
}

int _getNumMMXwrite()
{
	uint num = 0, i;
	for (i=0; i<iREGCNT_MMX; i++) {
		if( mmxregs[i].inuse && (mmxregs[i].mode&MODE_WRITE) ) ++num;
	}

	return num;
}

u8 _hasFreeMMXreg()
{
	uint i;
	for (i=0; i<iREGCNT_MMX; i++) {
		if (!mmxregs[i].inuse) return 1;
	}

	// check for dead regs
	for (i=0; i<iREGCNT_MMX; i++) {
		if (mmxregs[i].needed) continue;
		if (MMX_ISGPR(mmxregs[i].reg)) {
			if( !EEINST_ISLIVE64(mmxregs[i].reg-MMX_GPR) ) {
				return 1;
			}
		}
	}

	// check for dead regs
	for (i=0; i<iREGCNT_MMX; i++) {
		if (mmxregs[i].needed) continue;
		if (MMX_ISGPR(mmxregs[i].reg)) {
			if( !(g_pCurInstInfo->regs[mmxregs[i].reg-MMX_GPR]&EEINST_USED) ) {
				return 1;
			}
		}
	}

	return 0;
}

void _freeMMXreg(u32 mmxreg)
{
	pxAssert( mmxreg < iREGCNT_MMX );
	if (!mmxregs[mmxreg].inuse) return;

	if (mmxregs[mmxreg].mode & MODE_WRITE ) {
		// Not sure if this line is accurate, since if the 32 was 34, it would be MMX_ISGPR.
		if ( /*mmxregs[mmxreg].reg >= MMX_GPR &&*/ mmxregs[mmxreg].reg < MMX_GPR+32 ) // Checking if a u32 is >=0 is pointless.
			pxAssert( !(g_cpuHasConstReg & (1<<(mmxregs[mmxreg].reg-MMX_GPR))) );

		pxAssert( mmxregs[mmxreg].reg != MMX_GPR );

		if( MMX_IS32BITS(mmxregs[mmxreg].reg) )
			xMOVD(ptr[(_MMXGetAddr(mmxregs[mmxreg].reg))], xRegisterMMX(mmxreg));
		else
			xMOVQ(ptr[(_MMXGetAddr(mmxregs[mmxreg].reg))], xRegisterMMX(mmxreg));

		SetMMXstate();
	}

	mmxregs[mmxreg].mode &= ~MODE_WRITE;
	mmxregs[mmxreg].inuse = 0;
}

void _moveMMXreg(int mmxreg)
{
	uint i;
	if( !mmxregs[mmxreg].inuse ) return;

	for (i=0; i<iREGCNT_MMX; i++) {
		if (mmxregs[i].inuse) continue;
		break;
	}

	if( i == iREGCNT_MMX ) {
		_freeMMXreg(mmxreg);
		return;
	}

	// move
	mmxregs[i] = mmxregs[mmxreg];
	mmxregs[mmxreg].inuse = 0;
	xMOVQ(xRegisterMMX(i), xRegisterMMX(mmxreg));
}

// write all active regs
void _flushMMXregs()
{
	uint i;

	for (i=0; i<iREGCNT_MMX; i++) {
		if (mmxregs[i].inuse == 0) continue;

		if( mmxregs[i].mode & MODE_WRITE ) {
			pxAssert( !(g_cpuHasConstReg & (1<<mmxregs[i].reg)) );
			pxAssert( mmxregs[i].reg != MMX_TEMP );
			pxAssert( mmxregs[i].mode & MODE_READ );
			pxAssert( mmxregs[i].reg != MMX_GPR );

			if( MMX_IS32BITS(mmxregs[i].reg) )
				xMOVD(ptr[(_MMXGetAddr(mmxregs[i].reg))], xRegisterMMX(i));
			else
				xMOVQ(ptr[(_MMXGetAddr(mmxregs[i].reg))], xRegisterMMX(i));
			SetMMXstate();

			mmxregs[i].mode &= ~MODE_WRITE;
			mmxregs[i].mode |= MODE_READ;
		}
	}
}

void _freeMMXregs()
{
	uint i;
	for (i=0; i<iREGCNT_MMX; i++) {
		if (mmxregs[i].inuse == 0) continue;

		pxAssert( mmxregs[i].reg != MMX_TEMP );
		pxAssert( mmxregs[i].mode & MODE_READ );

		_freeMMXreg(i);
	}
}

void SetFPUstate() {
	_freeMMXreg(6);
	_freeMMXreg(7);

	if (x86FpuState == MMX_STATE) {
		xEMMS();
		x86FpuState = FPU_STATE;
	}
}

void _signExtendSFtoM(uptr mem)
{
	xLAHF();
	xSAR(ax, 15);
	xCWDE();
	xMOV(ptr[(void*)(mem)], eax);
}

int _signExtendMtoMMX(x86MMXRegType to, uptr mem)
{
	int t0reg = _allocMMXreg(-1, MMX_TEMP, 0);

	xMOVDZX(xRegisterMMX(t0reg), ptr[(void*)(mem)]);
	xMOVQ(xRegisterMMX(to), xRegisterMMX(t0reg));
	xPSRA.D(xRegisterMMX(t0reg), 31);
	xPUNPCK.LDQ(xRegisterMMX(to), xRegisterMMX(t0reg));
	_freeMMXreg(t0reg);

	return to;
}

int _signExtendGPRMMXtoMMX(x86MMXRegType to, u32 gprreg, x86MMXRegType from, u32 gprfromreg)
{
	pxAssert( to >= 0 && from >= 0 );

	if( to == from ) return _signExtendGPRtoMMX(to, gprreg, 0);
	if( !(g_pCurInstInfo->regs[gprfromreg]&EEINST_LASTUSE) ) {
		if( EEINST_ISLIVE64(gprfromreg) ) {
			xMOVQ(xRegisterMMX(to), xRegisterMMX(from));
			return _signExtendGPRtoMMX(to, gprreg, 0);
		}
	}

	// from is free for use
	SetMMXstate();

	xMOVQ(xRegisterMMX(to), xRegisterMMX(from));
	xMOVD(ptr[&cpuRegs.GPR.r[gprreg].UL[0]], xRegisterMMX(from));
	xPSRA.D(xRegisterMMX(from), 31);
	xMOVD(ptr[&cpuRegs.GPR.r[gprreg].UL[1]], xRegisterMMX(from));
	mmxregs[to].inuse = 0;

	return -1;
}

int _signExtendGPRtoMMX(x86MMXRegType to, u32 gprreg, int shift)
{
	pxAssert( to >= 0 && shift >= 0 );

	SetMMXstate();

	if( shift > 0 ) xPSRA.D(xRegisterMMX(to), shift);
	xMOVD(ptr[&cpuRegs.GPR.r[gprreg].UL[0]], xRegisterMMX(to));
	xPSRA.D(xRegisterMMX(to), 31);
	xMOVD(ptr[&cpuRegs.GPR.r[gprreg].UL[1]], xRegisterMMX(to));
	mmxregs[to].inuse = 0;

	return -1;
}

int _allocCheckGPRtoMMX(EEINST* pinst, int reg, int mode)
{
	return _checkMMXreg(MMX_GPR+reg, mode);
}
