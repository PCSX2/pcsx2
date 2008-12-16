/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

// stop compiling if NORECBUILD build (only for Visual Studio)
#if !(defined(_MSC_VER) && defined(PCSX2_NORECBUILD))

#if defined(_WIN32)
#include <windows.h>
#endif

#include <assert.h>
#include <vector>
#include <list>

using namespace std;

#include "Common.h"
#include "VU.h"

#include "ix86/ix86.h"
#include "iR5900.h"

#include "GS.h"
#include "DebugTools/Debug.h"
	
extern u32 CSRw;

#ifdef PCSX2_VIRTUAL_MEM
#define PS2GS_BASE(mem) ((PS2MEM_BASE+0x12000000)+(mem&0x13ff))
#else
extern u8 g_RealGSMem[0x2000];
#define PS2GS_BASE(mem) (g_RealGSMem+(mem&0x13ff))
#endif

void gsConstWrite8(u32 mem, int mmreg)
{
	switch (mem&~3) {
		case 0x12001000: // GS_CSR
			_eeMoveMMREGtoR(EAX, mmreg);
			iFlushCall(0);
			MOV32MtoR(ECX, (uptr)&CSRw);
			AND32ItoR(EAX, 0xff<<(mem&3)*8);
			AND32ItoR(ECX, ~(0xff<<(mem&3)*8));
			OR32ItoR(EAX, ECX);
			_callFunctionArg1((uptr)gsCSRwrite, EAX|MEM_X86TAG, 0);
			break;
		default:
			_eeWriteConstMem8( (uptr)PS2GS_BASE(mem), mmreg );

			if( CHECK_MULTIGS ) {
                iFlushCall(0);

                _callFunctionArg3((uptr)GSRingBufSimplePacket, MEM_CONSTTAG, MEM_CONSTTAG, mmreg,
                                  GS_RINGTYPE_MEMWRITE8, mem&0x13ff, 0);
			}
			break;
	}
}

void gsConstWrite16(u32 mem, int mmreg)
{	
	switch (mem&~3) {

		case 0x12000010: // GS_SMODE1
		case 0x12000020: // GS_SMODE2
			// SMODE1 and SMODE2 fall back on the gsWrite library.
			iFlushCall(0);
			_callFunctionArg2((uptr)gsWrite16, MEM_CONSTTAG, mmreg, mem, 0 );
			break;

		case 0x12001000: // GS_CSR

			assert( !(mem&2) );
			_eeMoveMMREGtoR(EAX, mmreg);
			iFlushCall(0);

			MOV32MtoR(ECX, (uptr)&CSRw);
			AND32ItoR(EAX, 0xffff<<(mem&2)*8);
			AND32ItoR(ECX, ~(0xffff<<(mem&2)*8));
			OR32ItoR(EAX, ECX);
			_callFunctionArg1((uptr)gsCSRwrite, EAX|MEM_X86TAG, 0);
			break;

		default:
			_eeWriteConstMem16( (uptr)PS2GS_BASE(mem), mmreg );

			if( CHECK_MULTIGS ) {
				iFlushCall(0);
                _callFunctionArg3((uptr)GSRingBufSimplePacket, MEM_CONSTTAG, MEM_CONSTTAG, mmreg,
                                  GS_RINGTYPE_MEMWRITE16, mem&0x13ff, 0);
			}

			break;
	}
}

// (value&0x1f00)|0x6000
void gsConstWriteIMR(int mmreg)
{
	const u32 mem = 0x12001010;
	if( mmreg & MEM_XMMTAG ) {
		SSE2_MOVD_XMM_to_M32((uptr)PS2GS_BASE(mem), mmreg&0xf);
		AND32ItoM((uptr)PS2GS_BASE(mem), 0x1f00);
		OR32ItoM((uptr)PS2GS_BASE(mem), 0x6000);
	}
#ifndef __x86_64__
	else if( mmreg & MEM_MMXTAG ) {
		SetMMXstate();
		MOVDMMXtoM((uptr)PS2GS_BASE(mem), mmreg&0xf);
		AND32ItoM((uptr)PS2GS_BASE(mem), 0x1f00);
		OR32ItoM((uptr)PS2GS_BASE(mem), 0x6000);
	}
#endif
	else if( mmreg & MEM_EECONSTTAG ) {
		MOV32ItoM( (uptr)PS2GS_BASE(mem), (g_cpuConstRegs[(mmreg>>16)&0x1f].UL[0]&0x1f00)|0x6000);
	}
	else {
		AND32ItoR(mmreg, 0x1f00);
		OR32ItoR(mmreg, 0x6000);
		MOV32RtoM( (uptr)PS2GS_BASE(mem), mmreg );
	}

	// IMR doesn't need to be updated in MTGS mode
}

void gsConstWrite32(u32 mem, int mmreg) {

	switch (mem) {

		case 0x12000010: // GS_SMODE1
		case 0x12000020: // GS_SMODE2
			// SMODE1 and SMODE2 fall back on the gsWrite library.
			iFlushCall(0);
			_callFunctionArg2((uptr)gsWrite32, MEM_CONSTTAG, mmreg, mem, 0 );
			break;

		case 0x12001000: // GS_CSR
			iFlushCall(0);
            _callFunctionArg1((uptr)gsCSRwrite, mmreg, 0);
			break;

		case 0x12001010: // GS_IMR
			gsConstWriteIMR(mmreg);
			break;
		default:
			_eeWriteConstMem32( (uptr)PS2GS_BASE(mem), mmreg );

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				_callFunctionArg3((uptr)GSRingBufSimplePacket, MEM_CONSTTAG, MEM_CONSTTAG, mmreg,
                                  GS_RINGTYPE_MEMWRITE32, mem&0x13ff, 0);                
			}

			break;
	}
}

void gsConstWrite64(u32 mem, int mmreg)
{
	switch (mem) {
		case 0x12000010: // GS_SMODE1
		case 0x12000020: // GS_SMODE2
			// SMODE1 and SMODE2 fall back on the gsWrite library.
			// the low 32 bit dword is all the SMODE regs care about.
			iFlushCall(0);
			_callFunctionArg2((uptr)gsWrite32, MEM_CONSTTAG, mmreg, mem, 0 );
			break;

		case 0x12001000: // GS_CSR
			iFlushCall(0);
            _callFunctionArg1((uptr)gsCSRwrite, mmreg, 0);
			break;

		case 0x12001010: // GS_IMR
			gsConstWriteIMR(mmreg);
			break;

		default:
			_eeWriteConstMem64((uptr)PS2GS_BASE(mem), mmreg);

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

                _callPushArg(MEM_MEMORYTAG, (uptr)PS2GS_BASE(mem)+4, X86ARG4);
                _callPushArg(MEM_MEMORYTAG, (uptr)PS2GS_BASE(mem), X86ARG3);
                _callPushArg(MEM_CONSTTAG, mem&0x13ff, X86ARG2);
                _callPushArg(MEM_CONSTTAG, GS_RINGTYPE_MEMWRITE64, X86ARG1);
				CALLFunc((uptr)GSRingBufSimplePacket);
#ifndef __x86_64__
				ADD32ItoR(ESP, 16);
#endif
			}

			break;
	}
}

void gsConstWrite128(u32 mem, int mmreg)
{
	switch (mem) {
		case 0x12000010: // GS_SMODE1
		case 0x12000020: // GS_SMODE2
			// SMODE1 and SMODE2 fall back on the gsWrite library.
			// the low 32 bit dword is all the SMODE regs care about.
			iFlushCall(0);
			_callFunctionArg2((uptr)gsWrite32, MEM_CONSTTAG, mmreg, mem, 0 );
			break;

		case 0x12001000: // GS_CSR
			iFlushCall(0);
            _callFunctionArg1((uptr)gsCSRwrite, mmreg, 0);
			break;

		case 0x12001010: // GS_IMR
			// (value&0x1f00)|0x6000
			gsConstWriteIMR(mmreg);
			break;

		default:
			_eeWriteConstMem128( (uptr)PS2GS_BASE(mem), mmreg);

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

                _callPushArg(MEM_MEMORYTAG, (uptr)PS2GS_BASE(mem)+4, X86ARG4);
                _callPushArg(MEM_MEMORYTAG, (uptr)PS2GS_BASE(mem), X86ARG3);
                _callPushArg(MEM_CONSTTAG, mem&0x13ff, X86ARG2);
                _callPushArg(MEM_CONSTTAG, GS_RINGTYPE_MEMWRITE64, X86ARG1);
				CALLFunc((uptr)GSRingBufSimplePacket);
#ifndef __x86_64__
				ADD32ItoR(ESP, 16);
#endif
                
                _callPushArg(MEM_MEMORYTAG, (uptr)PS2GS_BASE(mem)+12, X86ARG4);
                _callPushArg(MEM_MEMORYTAG, (uptr)PS2GS_BASE(mem)+8, X86ARG3);
                _callPushArg(MEM_CONSTTAG, (mem&0x13ff)+8, X86ARG2);
                _callPushArg(MEM_CONSTTAG, GS_RINGTYPE_MEMWRITE64, X86ARG1);
				CALLFunc((uptr)GSRingBufSimplePacket);
#ifndef __x86_64__
				ADD32ItoR(ESP, 16);
#endif
			}

			break;
	}
}

int gsConstRead8(u32 x86reg, u32 mem, u32 sign)
{
	GIF_LOG("GS read 8 %8.8lx (%8.8x), at %8.8lx\n", (uptr)PS2GS_BASE(mem), mem);
	_eeReadConstMem8(x86reg, (uptr)PS2GS_BASE(mem), sign);
	return 0;
}

int gsConstRead16(u32 x86reg, u32 mem, u32 sign)
{
	GIF_LOG("GS read 16 %8.8lx (%8.8x), at %8.8lx\n", (uptr)PS2GS_BASE(mem), mem);
	_eeReadConstMem16(x86reg, (uptr)PS2GS_BASE(mem), sign);
	return 0;
}

int gsConstRead32(u32 x86reg, u32 mem)
{
	GIF_LOG("GS read 32 %8.8lx (%8.8x), at %8.8lx\n", (uptr)PS2GS_BASE(mem), mem);
	_eeReadConstMem32(x86reg, (uptr)PS2GS_BASE(mem));
	return 0;
}

void gsConstRead64(u32 mem, int mmreg)
{
	GIF_LOG("GS read 64 %8.8lx (%8.8x), at %8.8lx\n", (uptr)PS2GS_BASE(mem), mem);
	if( IS_XMMREG(mmreg) ) SSE_MOVLPS_M64_to_XMM(mmreg&0xff, (uptr)PS2GS_BASE(mem));
	else {
#ifndef __x86_64__
		MOVQMtoR(mmreg, (uptr)PS2GS_BASE(mem));
		SetMMXstate();
#else
        assert(0);
#endif
	}
}

void gsConstRead128(u32 mem, int xmmreg)
{
	GIF_LOG("GS read 128 %8.8lx (%8.8x), at %8.8lx\n", (uptr)PS2GS_BASE(mem), mem);
	_eeReadConstMem128( xmmreg, (uptr)PS2GS_BASE(mem));
}

#endif // PCSX2_NORECBUILD
