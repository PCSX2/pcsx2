/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
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

#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#include "PsxCommon.h"
#include "VU.h"
#include "iCore.h"
#include "ir3000A.h"

extern u32 g_psxMaxRecMem;
int g_psxWriteOk=1;
static u32 writectrl;

#ifdef WIN32_VIRTUAL_MEM

int psxMemInit()
{
	// all mem taken care by memInit
	return 0;
}

void psxMemReset()
{
	memset(psxM, 0, 0x00200000);
}

void psxMemShutdown()
{
}

#ifdef _DEBUG

#define ASSERT_WRITEOK \
{ \
	__asm cmp g_psxWriteOk, 1 \
	__asm je WriteOk \
	__asm int 10 \
} \
WriteOk: \

#else
#define ASSERT_WRITEOK
#endif

u8 psxMemRead8(u32 mem)
{
	u32 t = (mem >> 16) & 0x1fff;
	
	switch(t) {
		case 0x1f80:
			mem&= 0x1fffffff;
			if (mem < 0x1f801000)
				return psxHu8(mem);
			else
				return psxHwRead8(mem);
			break;

#ifdef _DEBUG
		case 0x1d00: assert(0);
#endif

		case 0x1f40:
			mem &= 0x1fffffff;
			return psxHw4Read8(mem);

		case 0x1000: return DEV9read8(mem & 0x1FFFFFFF);

		default:
			assert( g_psxWriteOk );
			return *(u8*)PSXM(mem);
	}
}

__declspec(naked) void psxRecMemRead8()
{
	__asm {
		mov edx, ecx
		shr edx, 16
		cmp dx, 0x1f80
		je hwread
		cmp dx, 0x1f40
		je hw4read
		cmp dx, 0x1000
		je devread
		cmp dx, 0x1f00
		je spuread
	}

	ASSERT_WRITEOK

	__asm {
memread:
		// rom reads, has to be PS2MEM_BASE_
		mov eax, dword ptr [ecx+PS2MEM_BASE_]
		ret

hwread:
		cmp cx, 0x1000
		jb memread

		push ecx
		call psxHwRead8
		add esp, 4
		ret

hw4read:
		push ecx
		call psxHw4Read8
		add esp, 4
		ret

devread:
		push ecx
		call DEV9read8
		// stack already incremented
		ret

spuread:
		push ecx
		call SPU2read
		// stack already incremented
		ret
	}
}

int psxRecMemConstRead8(u32 x86reg, u32 mem, u32 sign)
{
	u32 t = (mem >> 16) & 0x1fff;
	
	switch(t) {
		case 0x1f80:
			return psxHwConstRead8(x86reg, mem&0x1fffffff, sign);

#ifdef _DEBUG
		case 0x1d00: assert(0);
#endif

		case 0x1f40:
			return psxHw4ConstRead8(x86reg, mem&0x1fffffff, sign);

		case 0x1000:
			PUSH32I(mem&0x1fffffff);
			CALLFunc((u32)DEV9read8);
			if( sign ) MOVSX32R8toR(x86reg, EAX);
			else MOVZX32R8toR(x86reg, EAX);
			return 0;

		default:
			_eeReadConstMem8(x86reg, (u32)PSXM(mem), sign);
			return 0;
	}
}

u16 psxMemRead16(u32 mem)
{
	u32 t = (mem >> 16) & 0x1fff;

	switch(t) {
		case 0x1f80:
			mem&= 0x1fffffff;
			if (mem < 0x1f801000)
				return psxHu16(mem);
			else
				return psxHwRead16(mem);
			break;

		case 0x1d00:
#ifdef SIF_LOG
			SIF_LOG("Sif reg read %x value %x\n", mem, psxHu16(mem));
#endif
			switch(mem & 0xF0)
			{
				case 0x40: return psHu16(0x1000F240) | 0x0002;
				case 0x60: return 0;
				default: return *(u16*)(PS2MEM_HW+0xf200+(mem&0xf0));
			}
			break;

		case 0x1f90:
			return SPU2read(mem & 0x1FFFFFFF);
		case 0x1000:
			return DEV9read16(mem & 0x1FFFFFFF);

		default:
			assert( g_psxWriteOk );
			return *(u16*)PSXM(mem);
	}
}

__declspec(naked) void psxRecMemRead16()
{
	__asm {
		mov edx, ecx
		shr edx, 16
		cmp dx, 0x1f80
		je hwread
		cmp dx, 0x1f90
		je spuread
		cmp dx, 0x1d00
		je sifread
		cmp dx, 0x1000
		je devread
	}

	ASSERT_WRITEOK

	__asm {
memread:
		// rom reads, has to be PS2MEM_BASE_
		mov eax, dword ptr [ecx+PS2MEM_BASE_]
		ret

hwread:
		cmp cx, 0x1000
		jb memread

		push ecx
		call psxHwRead16
		add esp, 4
		ret

sifread:
		mov edx, ecx
		and edx, 0xf0
		cmp dl, 0x60
		je Sif60
		

		mov eax, dword ptr [edx+PS2MEM_BASE_+0x1000f200]

		cmp dl, 0x40
		jne End

		// 0x40
		or eax, 2
		jmp End
Sif60:
		xor eax, eax
		jmp End

spuread:
		push ecx
		call SPU2read
		// stack already incremented

End:
		ret

devread:
		push ecx
		call DEV9read16
		// stack already incremented
		ret
	}
}

int psxRecMemConstRead16(u32 x86reg, u32 mem, u32 sign)
{
	u32 t = (mem >> 16) & 0x1fff;

	switch(t) {
		case 0x1f80: return psxHwConstRead16(x86reg, mem&0x1fffffff, sign);

		case 0x1d00:

			switch(mem & 0xF0)
			{
				case 0x40:
					_eeReadConstMem16(x86reg, (u32)PS2MEM_HW+0xF240, sign);
					OR32ItoR(x86reg, 0x0002);
					break;
				case 0x60:
					XOR32RtoR(x86reg, x86reg);
					break;
				default:
					_eeReadConstMem16(x86reg, (u32)PS2MEM_HW+0xf200+(mem&0xf0), sign);
					break;
			}
			return 0;

		case 0x1f90:
			PUSH32I(mem&0x1fffffff);
			CALLFunc((u32)SPU2read);
			if( sign ) MOVSX32R16toR(x86reg, EAX);
			else MOVZX32R16toR(x86reg, EAX);
			return 0;

		case 0x1000:
			PUSH32I(mem&0x1fffffff);
			CALLFunc((u32)DEV9read16);
			if( sign ) MOVSX32R16toR(x86reg, EAX);
			else MOVZX32R16toR(x86reg, EAX);
			return 0;

		default:
			assert( g_psxWriteOk );
			_eeReadConstMem16(x86reg, (u32)PSXM(mem), sign);
			return 0;
	}

	return 0;
}

u32 psxMemRead32(u32 mem)
{
	u32 t = (mem >> 16) & 0x1fff;

	switch(t) {
		case 0x1f80:
			mem&= 0x1fffffff;
			if (mem < 0x1f801000)
				return psxHu32(mem);
			else
				return psxHwRead32(mem);
			break;

		case 0x1d00:
#ifdef SIF_LOG
			SIF_LOG("Sif reg read %x value %x\n", mem, psxHu32(mem));
#endif
			switch(mem & 0xF0)
			{
				case 0x40: return psHu32(0x1000F240) | 0xF0000002;
				case 0x60: return 0;
				default: return *(u32*)(PS2MEM_HW+0xf200+(mem&0xf0));
			}
			break;

		case 0x1fff: return g_psxWriteOk;
		case 0x1000:
			return DEV9read32(mem & 0x1FFFFFFF);

		default:
			assert(g_psxWriteOk);
			if( mem == 0xfffe0130 )
				return writectrl;
			else if( mem == 0xffffffff )
				return writectrl;
			else if( g_psxWriteOk )
				return *(u32*)PSXM(mem);
			else return 0;
	}
}

__declspec(naked) void psxRecMemRead32()
{
	__asm {
		mov edx, ecx
		shr edx, 16
		cmp dx, 0x1f80
		je hwread
		cmp dx, 0x1d00
		je sifread
		cmp dx, 0x1000
		je devread
		cmp ecx, 0x1ffe0130
		je WriteCtrlRead
	}

	ASSERT_WRITEOK

	__asm {
memread:
		// rom reads, has to be PS2MEM_BASE_
		mov eax, dword ptr [ecx+PS2MEM_BASE_]
		ret

hwread:
		cmp cx, 0x1000
		jb memread

		push ecx
		call psxHwRead32
		add esp, 4
		ret

sifread:
		mov edx, ecx
		and edx, 0xf0
		cmp dl, 0x60
		je Sif60
		
		// do the read from ps2 mem
		mov eax, dword ptr [edx+PS2MEM_BASE_+0x1000f200]

		cmp dl, 0x40
		jne End

		// 0x40
		or eax, 0xf0000002
		jmp End
Sif60:
		xor eax, eax
End:
		ret

devread:
		push ecx
		call DEV9read32
		// stack already incremented
		ret

WriteCtrlRead:
		mov eax, writectrl
		ret
	}
}

int psxRecMemConstRead32(u32 x86reg, u32 mem)
{
	u32 t = (mem >> 16) & 0x1fff;

	switch(t) {
		case 0x1f80: return psxHwConstRead32(x86reg, mem&0x1fffffff);

		case 0x1d00:
			switch(mem & 0xF0)
			{
				case 0x40:
					_eeReadConstMem32(x86reg, (u32)PS2MEM_HW+0xF240);
					OR32ItoR(x86reg, 0xf0000002);
					break;
				case 0x60:
					XOR32RtoR(x86reg, x86reg);
					break;
				default:
					_eeReadConstMem32(x86reg, (u32)PS2MEM_HW+0xf200+(mem&0xf0));
					break;
			}
			return 0;

		case 0x1000:
			PUSH32I(mem&0x1fffffff);
			CALLFunc((u32)DEV9read32);
			return 1;

		default:
			if( mem == 0xfffe0130 )
				MOV32MtoR(x86reg, (u32)&writectrl);
			else {
				XOR32RtoR(x86reg, x86reg);
				CMP32ItoM((u32)&g_psxWriteOk, 0);
				CMOVNE32MtoR(x86reg, (u32)PSXM(mem));
			}

			return 0;
	}
}

void psxMemWrite8(u32 mem, u8 value)
{
	u32 t = (mem >> 16) & 0x1fff;

	switch(t) {
		case 0x1f80:
			mem&= 0x1fffffff;
			if (mem < 0x1f801000)
				psxHu8(mem) = value;
			else
				psxHwWrite8(mem, value);
			break;

		case 0x1f40:
			mem&= 0x1fffffff;
			psxHw4Write8(mem, value);
			break;

		case 0x1d00:
			SysPrintf("sw8 [0x%08X]=0x%08X\n", mem, value);
			*(u8*)(PS2MEM_HW+0xf200+(mem&0xff)) = value;
			break;

		case 0x1000:
			DEV9write8(mem & 0x1fffffff, value);
			return;

		default:
			assert(g_psxWriteOk);
			*(u8  *)PSXM(mem) = value;
			psxCpu->Clear(mem&~3, 1);
			break;
	}
}

__declspec(naked) void psxRecMemWrite8()
{
	__asm {
		mov edx, ecx
		shr edx, 16
		cmp dx, 0x1f80
		je hwwrite
		cmp dx, 0x1f40
		je hw4write
		cmp dx, 0x1000
		je devwrite
	}

	ASSERT_WRITEOK

	__asm {
memwrite:
		// rom writes, has to be PS2MEM_BASE_
		mov byte ptr [ecx+PS2MEM_BASE_], al
		ret

hwwrite:
		cmp cx, 0x1000
		jb memwrite

		push eax
		push ecx
		call psxHwWrite8
		add esp, 8
		ret

hw4write:
		push eax
		push ecx
		call psxHw4Write8
		add esp, 8
		ret

devwrite:
		push eax
		push ecx
		call DEV9write8
		// stack alwritey incremented
		ret
	}
}

int psxRecMemConstWrite8(u32 mem, int mmreg)
{
	u32 t = (mem >> 16) & 0x1fff;

	switch(t) {
		case 0x1f80:
			psxHwConstWrite8(mem&0x1fffffff, mmreg);
			return 0;
		case 0x1f40:
			psxHw4ConstWrite8(mem&0x1fffffff, mmreg);
			return 0;

		case 0x1d00:
			assert(0);
			_eeWriteConstMem8((u32)(PS2MEM_HW+0xf200+(mem&0xff)), mmreg);
			return 0;

		case 0x1000:
			_recPushReg(mmreg);
			PUSH32I(mem&0x1fffffff);
			CALLFunc((u32)DEV9write8);
			return 0;

		default:
			_eeWriteConstMem8((u32)PSXM(mem), mmreg);
			return 1;
	}
}

void psxMemWrite16(u32 mem, u16 value)
{
	u32 t = (mem >> 16) & 0x1fff;
	switch(t) {
		case 0x1f80:
			mem&= 0x1fffffff;
			if (mem < 0x1f801000)
				psxHu16(mem) = value;
			else
				psxHwWrite16(mem, value);
			break;

		case 0x1d00:
			switch (mem & 0xf0) {
				case 0x10:
					// write to ps2 mem
					psHu16(0x1000F210) = value;
					return;
				case 0x40:
				{
					u32 temp = value & 0xF0;
					// write to ps2 mem
					if(value & 0x20 || value & 0x80)
					{
						psHu16(0x1000F240) &= ~0xF000;
						psHu16(0x1000F240) |= 0x2000;
					}
						
					if(psHu16(0x1000F240) & temp) psHu16(0x1000F240) &= ~temp;
					else psHu16(0x1000F240) |= temp;
					return;
				}
				case 0x60:
					psHu32(0x1000F260) = 0;
					return;
				default:
					assert(0);
			}
			return;

		case 0x1f90:
			SPU2write(mem & 0x1FFFFFFF, value); return;
			
		case 0x1000:
			DEV9write16(mem & 0x1fffffff, value); return;
		default:
			assert( g_psxWriteOk );
			*(u16 *)PSXM(mem) = value;
			psxCpu->Clear(mem&~3, 1);
			break;
	}
}

__declspec(naked) void psxRecMemWrite16()
{
	__asm {
		mov edx, ecx
		shr edx, 16
		cmp dx, 0x1f80
		je hwwrite
		cmp dx, 0x1f90
		je spuwrite
		cmp dx, 0x1d00
		je sifwrite
		cmp dx, 0x1000
		je devwrite
	}

	ASSERT_WRITEOK

	__asm {
memwrite:
		// rom writes, has to be PS2MEM_BASE_
		mov word ptr [ecx+PS2MEM_BASE_], ax
		ret

hwwrite:
		cmp cx, 0x1000
		jb memwrite

		push eax
		push ecx
		call psxHwWrite16
		add esp, 8
		ret

sifwrite:
		mov edx, ecx
		and edx, 0xf0
		cmp dl, 0x60
		je Sif60
		cmp dl, 0x40
		je Sif40
		
		mov word ptr [edx+PS2MEM_BASE_+0x1000f200], ax
		ret

Sif40:
		mov bx, word ptr [edx+PS2MEM_BASE_+0x1000f200]
		test ax, 0xa0
		jz Sif40_2
		// psHu16(0x1000F240) &= ~0xF000;
		// psHu16(0x1000F240) |= 0x2000;
		and bx, 0x0fff
		or bx, 0x2000
		
Sif40_2:
		// if(psHu16(0x1000F240) & temp) psHu16(0x1000F240) &= ~temp;
		// else psHu16(0x1000F240) |= temp;
		and ax, 0xf0
		test bx, ax
		jz Sif40_3

		not ax
		and bx, ax
		jmp Sif40_4
Sif40_3:
		or bx, ax
Sif40_4:
		mov word ptr [edx+PS2MEM_BASE_+0x1000f200], bx
		ret

Sif60:
		mov word ptr [edx+PS2MEM_BASE_+0x1000f200], 0
		ret

spuwrite:
		push eax
		push ecx
		call SPU2write
		// stack alwritey incremented
		ret

devwrite:
		push eax
		push ecx
		call DEV9write16
		// stack alwritey incremented
		ret
	}
}

int psxRecMemConstWrite16(u32 mem, int mmreg)
{
	u32 t = (mem >> 16) & 0x1fff;
	switch(t) {
		case 0x1f80:
			psxHwConstWrite16(mem&0x1fffffff, mmreg);
			return 0;

		case 0x1d00:
			switch (mem & 0xf0) {
				case 0x10:
					// write to ps2 mem
					_eeWriteConstMem16((u32)(PS2MEM_HW+0xf210), mmreg);
					return 0;
				case 0x40:
				{
					// delete x86reg
					_eeMoveMMREGtoR(EAX, mmreg);

					assert( mmreg != EBX );
					MOV16MtoR(EBX, (u32)PS2MEM_HW+0xf240);
					TEST16ItoR(EAX, 0xa0);
					j8Ptr[0] = JZ8(0);

					AND16ItoR(EBX, 0x0fff);
					OR16ItoR(EBX, 0x2000);

					x86SetJ8(j8Ptr[0]);

					AND16ItoR(EAX, 0xf0);
					TEST16RtoR(EAX, 0xf0);
					j8Ptr[0] = JZ8(0);

					NOT32R(EAX);
					AND16RtoR(EBX, EAX);
					j8Ptr[1] = JMP8(0);

					x86SetJ8(j8Ptr[0]);
					OR16RtoR(EBX, EAX);

					x86SetJ8(j8Ptr[1]);

					MOV16RtoM((u32)PS2MEM_HW+0xf240, EBX);

					return 0;
				}
				case 0x60:
					MOV32ItoM((u32)(PS2MEM_HW+0xf260), 0);
					return 0;
				default:
					assert(0);
			}
			return 0;

		case 0x1f90:
			_recPushReg(mmreg);
			PUSH32I(mem&0x1fffffff);
			CALLFunc((u32)SPU2write);
			return 0;
			
		case 0x1000:
			_recPushReg(mmreg);
			PUSH32I(mem&0x1fffffff);
			CALLFunc((u32)DEV9write16);
			return 0;

		default:
			_eeWriteConstMem16((u32)PSXM(mem), mmreg);
			return 1;
	}
}

void psxMemWrite32(u32 mem, u32 value)
{
	u32 t = (mem >> 16) & 0x1fff;
	switch(t) {
		case 0x1f80:
			mem&= 0x1fffffff;
			if (mem < 0x1f801000)
				psxHu32(mem) = value;
			else
				psxHwWrite32(mem, value);
			break;

		case 0x1d00:
			switch (mem & 0xf0) {
				case 0x10:
					// write to ps2 mem
					psHu32(0x1000F210) = value;
					return;
				case 0x20:
					// write to ps2 mem
					psHu32(0x1000F220) &= ~value;
					return;
				case 0x30:
					// write to ps2 mem
					psHu32(0x1000F230) |= value;
					return;
				case 0x40:
				{
					u32 temp = value & 0xF0;
					// write to ps2 mem
					if(value & 0x20 || value & 0x80)
					{
						psHu32(0x1000F240) &= ~0xF000;
						psHu32(0x1000F240) |= 0x2000;
					}

					
					if(psHu32(0x1000F240) & temp) psHu32(0x1000F240) &= ~temp;
					else psHu32(0x1000F240) |= temp;
					return;
				}
				case 0x60:
					psHu32(0x1000F260) = 0;
					return;

				default:
					*(u32*)(PS2MEM_HW+0xf200+(mem&0xf0)) = value;
			}
				
			return;

		case 0x1000:
			DEV9write32(mem & 0x1fffffff, value);
			return;

		case 0x1ffe:
			if( mem == 0xfffe0130 ) {
				writectrl = value;
				switch (value) {
					case 0x800: case 0x804:
					case 0xc00: case 0xc04:
					case 0xcc0: case 0xcc4:
					case 0x0c4:
						g_psxWriteOk = 0;
#ifdef PSXMEM_LOG
//						PSXMEM_LOG("writectrl: writenot ok\n");
#endif
						break;
					case 0x1e988:
					case 0x1edd8:
						g_psxWriteOk = 1;
#ifdef PSXMEM_LOG
//						PSXMEM_LOG("writectrl: write ok\n");
#endif
						break;
					default:
#ifdef PSXMEM_LOG
						PSXMEM_LOG("unk %8.8lx = %x\n", mem, value);
#endif
						break;
				}
			}
			break;

		default:
			
			if( g_psxWriteOk ) {
				*(u32 *)PSXM(mem) = value;
				psxCpu->Clear(mem&~3, 1);
			}

			break;
	}
}

__declspec(naked) void psxRecMemWrite32()
{
	__asm {
		mov edx, ecx
		shr edx, 16
		cmp dx, 0x1f80
		je hwwrite
		cmp dx, 0x1d00
		je sifwrite
		cmp dx, 0x1000
		je devwrite
		cmp dx, 0x1ffe
		je WriteCtrl
	}

	__asm {
		// rom writes, has to be PS2MEM_BASE_
		test g_psxWriteOk, 1
		jz endwrite

memwrite:
		mov dword ptr [ecx+PS2MEM_BASE_], eax
endwrite:
		ret

hwwrite:
		cmp cx, 0x1000
		jb memwrite

		push eax
		push ecx
		call psxHwWrite32
		add esp, 8
		ret

sifwrite:
		mov edx, ecx
		and edx, 0xf0
		cmp dl, 0x60
		je Sif60
		cmp dl, 0x40
		je Sif40
		cmp dl, 0x30
		je Sif30
		cmp dl, 0x20
		je Sif20
		
		mov word ptr [edx+PS2MEM_BASE_+0x1000f200], ax
		ret

Sif40:
		mov bx, word ptr [edx+PS2MEM_BASE_+0x1000f200]
		test ax, 0xa0
		jz Sif40_2
		// psHu16(0x1000F240) &= ~0xF000;
		// psHu16(0x1000F240) |= 0x2000;
		and bx, 0x0fff
		or bx, 0x2000
		
Sif40_2:
		// if(psHu16(0x1000F240) & temp) psHu16(0x1000F240) &= ~temp;
		// else psHu16(0x1000F240) |= temp;
		and ax, 0xf0
		test bx, ax
		jz Sif40_3

		not ax
		and bx, ax
		jmp Sif40_4
Sif40_3:
		or bx, ax
Sif40_4:
		mov word ptr [edx+PS2MEM_BASE_+0x1000f200], bx
		ret

Sif30:
		or dword ptr [edx+PS2MEM_BASE_+0x1000f200], eax
		ret
Sif20:
		not eax
		and dword ptr [edx+PS2MEM_BASE_+0x1000f200], eax
		ret
Sif60:
		mov dword ptr [edx+PS2MEM_BASE_+0x1000f200], 0
		ret

devwrite:
		push eax
		push ecx
		call DEV9write32
		// stack alwritey incremented
		ret

WriteCtrl:
		cmp ecx, 0x1ffe0130
		jne End

		mov writectrl, eax

		cmp eax, 0x800
		je SetWriteNotOk
		cmp eax, 0x804
		je SetWriteNotOk
		cmp eax, 0xc00
		je SetWriteNotOk
		cmp eax, 0xc04
		je SetWriteNotOk
		cmp eax, 0xcc0
		je SetWriteNotOk
		cmp eax, 0xcc4
		je SetWriteNotOk
		cmp eax, 0x0c4
		je SetWriteNotOk

		// test ok
		cmp eax, 0x1e988
		je SetWriteOk
		cmp eax, 0x1edd8
		je SetWriteOk

End:
		ret

SetWriteNotOk:
		mov g_psxWriteOk, 0
		ret
SetWriteOk:
		mov g_psxWriteOk, 1
		ret
	}
}

int psxRecMemConstWrite32(u32 mem, int mmreg)
{
	u32 t = (mem >> 16) & 0x1fff;
	switch(t) {
		case 0x1f80:
			psxHwConstWrite32(mem&0x1fffffff, mmreg);
			return 0;

		case 0x1d00:
			switch (mem & 0xf0) {
				case 0x10:
					// write to ps2 mem
					_eeWriteConstMem32((u32)PS2MEM_HW+0xf210, mmreg);
					return 0;
				case 0x20:
					// write to ps2 mem
					// delete x86reg
					if( IS_PSXCONSTREG(mmreg) ) {
						AND32ItoM((u32)PS2MEM_HW+0xf220, ~g_psxConstRegs[(mmreg>>16)&0x1f]);
					}
					else {
						NOT32R(mmreg);
						AND32RtoM((u32)PS2MEM_HW+0xf220, mmreg);
					}
					return 0;
				case 0x30:
					// write to ps2 mem
					_eeWriteConstMem32OP((u32)PS2MEM_HW+0xf230, mmreg, 1);
					return 0;
				case 0x40:
				{
					// delete x86reg
					assert( mmreg != EBX );

					_eeMoveMMREGtoR(EAX, mmreg);

					MOV16MtoR(EBX, (u32)PS2MEM_HW+0xf240);
					TEST16ItoR(EAX, 0xa0);
					j8Ptr[0] = JZ8(0);

					AND16ItoR(EBX, 0x0fff);
					OR16ItoR(EBX, 0x2000);

					x86SetJ8(j8Ptr[0]);

					AND16ItoR(EAX, 0xf0);
					TEST16RtoR(EAX, 0xf0);
					j8Ptr[0] = JZ8(0);

					NOT32R(EAX);
					AND16RtoR(EBX, EAX);
					j8Ptr[1] = JMP8(0);

					x86SetJ8(j8Ptr[0]);
					OR16RtoR(EBX, EAX);

					x86SetJ8(j8Ptr[1]);

					MOV16RtoM((u32)PS2MEM_HW+0xf240, EBX);

					return 0;
				}
				case 0x60:
					MOV32ItoM((u32)(PS2MEM_HW+0xf260), 0);
					return 0;
				default:
					assert(0);
			}
			return 0;

		case 0x1000:
			_recPushReg(mmreg);
			PUSH32I(mem&0x1fffffff);
			CALLFunc((u32)DEV9write32);
			return 0;

		case 0x1ffe:
			if( mem == 0xfffe0130 ) {
				u8* ptrs[9];

				_eeWriteConstMem32((u32)&writectrl, mmreg);
		
				if( IS_PSXCONSTREG(mmreg) ) {
					switch (g_psxConstRegs[(mmreg>>16)&0x1f]) {
						case 0x800: case 0x804:
						case 0xc00: case 0xc04:
						case 0xcc0: case 0xcc4:
						case 0x0c4:
							MOV32ItoM((u32)&g_psxWriteOk, 0);
							break;
						case 0x1e988:
						case 0x1edd8:
							MOV32ItoM((u32)&g_psxWriteOk, 1);
							break;
						default:
							assert(0);
					}
				}
				else {
					// not ok
					CMP32ItoR(mmreg, 0x800);
					ptrs[0] = JE8(0);
					CMP32ItoR(mmreg, 0x804);
					ptrs[1] = JE8(0);
					CMP32ItoR(mmreg, 0xc00);
					ptrs[2] = JE8(0);
					CMP32ItoR(mmreg, 0xc04);
					ptrs[3] = JE8(0);
					CMP32ItoR(mmreg, 0xcc0);
					ptrs[4] = JE8(0);
					CMP32ItoR(mmreg, 0xcc4);
					ptrs[5] = JE8(0);
					CMP32ItoR(mmreg, 0x0c4);
					ptrs[6] = JE8(0);

					// ok
					CMP32ItoR(mmreg, 0x1e988);
					ptrs[7] = JE8(0);
					CMP32ItoR(mmreg, 0x1edd8);
					ptrs[8] = JE8(0);

					x86SetJ8(ptrs[0]);
					x86SetJ8(ptrs[1]);
					x86SetJ8(ptrs[2]);
					x86SetJ8(ptrs[3]);
					x86SetJ8(ptrs[4]);
					x86SetJ8(ptrs[5]);
					x86SetJ8(ptrs[6]);
					MOV32ItoM((u32)&g_psxWriteOk, 0);
					ptrs[0] = JMP8(0);

					x86SetJ8(ptrs[7]);
					x86SetJ8(ptrs[8]);
					MOV32ItoM((u32)&g_psxWriteOk, 1);

					x86SetJ8(ptrs[0]);
				}
			}
			return 0;

		default:
			TEST8ItoM((u32)&g_psxWriteOk, 1);
			j8Ptr[0] = JZ8(0);
			_eeWriteConstMem32((u32)PSXM(mem), mmreg);
			x86SetJ8(j8Ptr[0]);
			return 1;
	}
}

#else

// TLB functions

s8 *psxM;
s8 *psxP;
s8 *psxH;
s8 *psxS;
uptr *psxMemWLUT;
uptr *psxMemRLUT;

int psxMemInit()
{
	int i;

	psxMemRLUT = (uptr*)_aligned_malloc(0x10000 * sizeof(uptr),16);
	psxMemWLUT = (uptr*)_aligned_malloc(0x10000 * sizeof(uptr),16);
	memset(psxMemRLUT, 0, 0x10000 * sizeof(uptr));
	memset(psxMemWLUT, 0, 0x10000 * sizeof(uptr));

	psxM = (char*)_aligned_malloc(0x00200000,16);
	psxP = (char*)_aligned_malloc(0x00010000,16);
	psxH = (char*)_aligned_malloc(0x00010000,16);
	psxS = (char*)_aligned_malloc(0x00010000,16);
	if (psxMemRLUT == NULL || psxMemWLUT == NULL || 
		psxM == NULL || psxP == NULL || psxH == NULL) {
		SysMessage(_("Error allocating memory")); return -1;
	}

	memset(psxM, 0, 0x00200000);
	memset(psxP, 0, 0x00010000);
	memset(psxH, 0, 0x00010000);
	memset(psxS, 0, 0x00010000);


// MemR
	for (i=0; i<0x0080; i++) psxMemRLUT[i + 0x0000] = (uptr)&psxM[(i & 0x1f) << 16];
	for (i=0; i<0x0080; i++) psxMemRLUT[i + 0x8000] = (uptr)&psxM[(i & 0x1f) << 16];
	for (i=0; i<0x0080; i++) psxMemRLUT[i + 0xa000] = (uptr)&psxM[(i & 0x1f) << 16];

	for (i=0; i<0x0001; i++) psxMemRLUT[i + 0x1f00] = (uptr)&psxP[i << 16];

	for (i=0; i<0x0001; i++) psxMemRLUT[i + 0x1f80] = (uptr)&psxH[i << 16];
	for (i=0; i<0x0001; i++) psxMemRLUT[i + 0xbf80] = (uptr)&psxH[i << 16];

	for (i=0; i<0x0040; i++) psxMemRLUT[i + 0x1fc0] = (uptr)&PS2MEM_ROM[i << 16];
	for (i=0; i<0x0040; i++) psxMemRLUT[i + 0x9fc0] = (uptr)&PS2MEM_ROM[i << 16];
	for (i=0; i<0x0040; i++) psxMemRLUT[i + 0xbfc0] = (uptr)&PS2MEM_ROM[i << 16];

	for (i=0; i<0x0004; i++) psxMemRLUT[i + 0x1e00] = (uptr)&PS2MEM_ROM1[i << 16];
	for (i=0; i<0x0004; i++) psxMemRLUT[i + 0x9e00] = (uptr)&PS2MEM_ROM1[i << 16];
	for (i=0; i<0x0004; i++) psxMemRLUT[i + 0xbe00] = (uptr)&PS2MEM_ROM1[i << 16];

	for (i=0; i<0x0001; i++) psxMemRLUT[i + 0x1d00] = (uptr)&psxS[i << 16];
	for (i=0; i<0x0001; i++) psxMemRLUT[i + 0xbd00] = (uptr)&psxS[i << 16];

// MemW
	for (i=0; i<0x0080; i++) psxMemWLUT[i + 0x0000] = (uptr)&psxM[(i & 0x1f) << 16];
	for (i=0; i<0x0080; i++) psxMemWLUT[i + 0x8000] = (uptr)&psxM[(i & 0x1f) << 16];
	for (i=0; i<0x0080; i++) psxMemWLUT[i + 0xa000] = (uptr)&psxM[(i & 0x1f) << 16];

	for (i=0; i<0x0001; i++) psxMemWLUT[i + 0x1f00] = (uptr)&psxP[i << 16];

	for (i=0; i<0x0001; i++) psxMemWLUT[i + 0x1f80] = (uptr)&psxH[i << 16];
	for (i=0; i<0x0001; i++) psxMemWLUT[i + 0xbf80] = (uptr)&psxH[i << 16];

//	for (i=0; i<0x0008; i++) psxMemWLUT[i + 0xbfc0] = (uptr)&psR[i << 16];

//	for (i=0; i<0x0001; i++) psxMemWLUT[i + 0x1d00] = (uptr)&psxS[i << 16];
//	for (i=0; i<0x0001; i++) psxMemWLUT[i + 0xbd00] = (uptr)&psxS[i << 16];

	return 0;
}

void psxMemReset() {
	memset(psxM, 0, 0x00200000);
	memset(psxP, 0, 0x00010000);
	//memset(psxS, 0, 0x00010000);
}

void psxMemShutdown()
{
	_aligned_free(psxM);
	_aligned_free(psxP);
	_aligned_free(psxH);
	_aligned_free(psxMemRLUT);
	_aligned_free(psxMemWLUT);
}

u8 psxMemRead8(u32 mem) {
	char *p;
	u32 t;

	t = (mem >> 16) & 0x1fff;
	if (t == 0x1f80) {
		mem&= 0x1fffffff;
		if (mem < 0x1f801000)
			return psxHu8(mem);
		else
			return psxHwRead8(mem);
	} else
	if (t == 0x1f40) {
		mem&= 0x1fffffff;
		return psxHw4Read8(mem);
	} else {
		p = (char *)(psxMemRLUT[mem >> 16]);
		if (p != NULL) {
			return *(u8 *)(p + (mem & 0xffff));
		} else {
			if (t == 0x1000) return DEV9read8(mem & 0x1FFFFFFF);
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err lb %8.8lx\n", mem);
#endif
			return 0;
		}
	}
}

u16 psxMemRead16(u32 mem) {
	char *p;
	u32 t;

	t = (mem >> 16) & 0x1fff;
	if (t == 0x1f80) {
		mem&= 0x1fffffff;
		if (mem < 0x1f801000)
			return psxHu16(mem);
		else
			return psxHwRead16(mem);
	} else {
		p = (char *)(psxMemRLUT[mem >> 16]);
		if (p != NULL) {
			if (t == 0x1d00) {
				u16 ret;
				switch(mem & 0xF0)
				{
				case 0x00:
					ret= psHu16(0x1000F200);
					break;
				case 0x10:
					ret= psHu16(0x1000F210);
					break;
				case 0x40:
					ret= psHu16(0x1000F240) | 0x0002;
					break;
				case 0x60:
					ret = 0;
					break;
				default:
					ret = psxHu16(mem);
					break;
				}
#ifdef SIF_LOG
				SIF_LOG("Sif reg read %x value %x\n", mem, ret);			
#endif
				return ret;
			}
			return *(u16 *)(p + (mem & 0xffff));
		} else {
			if (t == 0x1F90)
				return SPU2read(mem & 0x1FFFFFFF);
			if (t == 0x1000) return DEV9read16(mem & 0x1FFFFFFF);
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err lh %8.8lx\n", mem);
#endif
			return 0;
		}
	}
}

u32 psxMemRead32(u32 mem) {
	char *p;
	u32 t;
	t = (mem >> 16) & 0x1fff;
	if (t == 0x1f80) {
		mem&= 0x1fffffff;
		if (mem < 0x1f801000)
			return psxHu32(mem);
		else
			return psxHwRead32(mem);
	} else {
		//see also Hw.c
		p = (char *)(psxMemRLUT[mem >> 16]);
		if (p != NULL) {
			if (t == 0x1d00) {
				u32 ret;
				switch(mem & 0xF0)
				{
				case 0x00:
					ret= psHu32(0x1000F200);
					break;
				case 0x10:
					ret= psHu32(0x1000F210);
					break;
				case 0x20:
					ret= psHu32(0x1000F220);
					break;
				case 0x30:	// EE Side
					ret= psHu32(0x1000F230);
					break;
				case 0x40:
					ret= psHu32(0x1000F240) | 0xF0000002;
					break;
				case 0x60:
					ret = 0;
					break;
				default:
					ret = psxHu32(mem);
					break;
				}
#ifdef SIF_LOG
				SIF_LOG("Sif reg read %x value %x\n", mem, ret);			
#endif
				return ret;
			}
			return *(u32 *)(p + (mem & 0xffff));
		} else {
			if (t == 0x1000) return DEV9read32(mem & 0x1FFFFFFF);
			
			if (mem != 0xfffe0130) {
#ifdef PSXMEM_LOG
				if (g_psxWriteOk) PSXMEM_LOG("err lw %8.8lx\n", mem);
#endif
			} else {
				return writectrl;
			}
			return 0;
		}
	}
}

void psxMemWrite8(u32 mem, u8 value) {
	char *p;
	u32 t;

	t = (mem >> 16) & 0x1fff;
	if (t == 0x1f80) {
		mem&= 0x1fffffff;
		if (mem < 0x1f801000)
			psxHu8(mem) = value;
		else
			psxHwWrite8(mem, value);
	} else
	if (t == 0x1f40) {
		mem&= 0x1fffffff;
		psxHw4Write8(mem, value);
	} else {
		p = (char *)(psxMemWLUT[mem >> 16]);
		if (p != NULL) {
			*(u8  *)(p + (mem & 0xffff)) = value;
			psxCpu->Clear(mem&~3, 1);
		} else {
			if ((t & 0x1FFF)==0x1D00) SysPrintf("sw8 [0x%08X]=0x%08X\n", mem, value);
			if (t == 0x1d00) {
				psxSu8(mem) = value; return;
			}
			if (t == 0x1000) {
				DEV9write8(mem & 0x1fffffff, value); return;
			}
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err sb %8.8lx = %x\n", mem, value);
#endif
		}
	}
}

void psxMemWrite16(u32 mem, u16 value) {
	char *p;
	u32 t;

	t = (mem >> 16) & 0x1fff;
	if (t == 0x1f80) {
		mem&= 0x1fffffff;
		if (mem < 0x1f801000)
			psxHu16(mem) = value;
		else
			psxHwWrite16(mem, value);
	} else {
		p = (char *)(psxMemWLUT[mem >> 16]);
		if (p != NULL) {
			if ((t & 0x1FFF)==0x1D00) SysPrintf("sw16 [0x%08X]=0x%08X\n", mem, value);
			*(u16 *)(p + (mem & 0xffff)) = value;
			psxCpu->Clear(mem&~3, 1);
		} else {
			if (t == 0x1d00) {
					switch (mem & 0xf0) {
						case 0x10:
							// write to ps2 mem
							psHu16(0x1000F210) = value;
							return;
						case 0x40:
						{
							u32 temp = value & 0xF0;
							// write to ps2 mem
							if(value & 0x20 || value & 0x80)
							{
								psHu16(0x1000F240) &= ~0xF000;
								psHu16(0x1000F240) |= 0x2000;
							}

							
							if(psHu16(0x1000F240) & temp) psHu16(0x1000F240) &= ~temp;
							else psHu16(0x1000F240) |= temp;
							return;
						}
						case 0x60:
							psHu32(0x1000F260) = 0;
							return;

					}
				psxSu16(mem) = value; return;
			}
			if (t == 0x1F90) {
				SPU2write(mem & 0x1FFFFFFF, value); return;
			}
			if (t == 0x1000) {
				DEV9write16(mem & 0x1fffffff, value); return;
			}
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err sh %8.8lx = %x\n", mem, value);
#endif
		}
	}
}

void psxMemWrite32(u32 mem, u32 value) {
	char *p;
	u32 t;
	
	t = (mem >> 16) & 0x1fff;
	if (t == 0x1f80) {
		mem&= 0x1fffffff;
		if (mem < 0x1f801000)
			psxHu32(mem) = value;
		else
			psxHwWrite32(mem, value);
	} else {
		//see also Hw.c
		p = (char *)(psxMemWLUT[mem >> 16]);
		if (p != NULL) {
			*(u32 *)(p + (mem & 0xffff)) = value;
			psxCpu->Clear(mem&~3, 1);
		} else {
			if (mem != 0xfffe0130) {
				if (t == 0x1d00) {
#ifdef MEM_LOG
				MEM_LOG("iop Sif reg write %x value %x\n", mem, value);			
#endif
					switch (mem & 0xf0) {
						case 0x10:
							// write to ps2 mem
							psHu32(0x1000F210) = value;
							return;
						case 0x20:
							// write to ps2 mem
							psHu32(0x1000F220) &= ~value;
							return;
						case 0x30:
							// write to ps2 mem
							psHu32(0x1000F230) |= value;
							return;
						case 0x40:
						{
							u32 temp = value & 0xF0;
							// write to ps2 mem
							if(value & 0x20 || value & 0x80)
							{
								psHu32(0x1000F240) &= ~0xF000;
								psHu32(0x1000F240) |= 0x2000;
							}

							
							if(psHu32(0x1000F240) & temp) psHu32(0x1000F240) &= ~temp;
							else psHu32(0x1000F240) |= temp;
							return;
						}
						case 0x60:
							psHu32(0x1000F260) = 0;
							return;

					}
					psxSu32(mem) = value; 

					// write to ps2 mem
					if( (mem & 0xf0) != 0x60 )
						*(u32*)(PS2MEM_HW+0xf200+(mem&0xf0)) = value;
					return;   
				}
				if (t == 0x1000) {
					DEV9write32(mem & 0x1fffffff, value); return;
				}

				//if (!g_psxWriteOk) psxCpu->Clear(mem&~3, 1);
#ifdef PSXMEM_LOG
				if (g_psxWriteOk) { PSXMEM_LOG("err sw %8.8lx = %x\n", mem, value); }
#endif
			} else {
				int i;

				writectrl = value;
				switch (value) {
					case 0x800: case 0x804:
					case 0xc00: case 0xc04:
					case 0xcc0: case 0xcc4:
					case 0x0c4:
						if (g_psxWriteOk == 0) break;
						g_psxWriteOk = 0;
						memset(psxMemWLUT + 0x0000, 0, 0x80 * sizeof(uptr));
						memset(psxMemWLUT + 0x8000, 0, 0x80 * sizeof(uptr));
						memset(psxMemWLUT + 0xa000, 0, 0x80 * sizeof(uptr));
#ifdef PSXMEM_LOG
//						PSXMEM_LOG("writectrl: writenot ok\n");
#endif
						break;
					case 0x1e988:
					case 0x1edd8:
						if (g_psxWriteOk == 1) break;
						g_psxWriteOk = 1;
						for (i=0; i<0x0080; i++) psxMemWLUT[i + 0x0000] = (uptr)&psxM[(i & 0x1f) << 16];
						for (i=0; i<0x0080; i++) psxMemWLUT[i + 0x8000] = (uptr)&psxM[(i & 0x1f) << 16];
						for (i=0; i<0x0080; i++) psxMemWLUT[i + 0xa000] = (uptr)&psxM[(i & 0x1f) << 16];
#ifdef PSXMEM_LOG
//						PSXMEM_LOG("writectrl: write ok\n");
#endif
						break;
					default:
#ifdef PSXMEM_LOG
						PSXMEM_LOG("unk %8.8lx = %x\n", mem, value);
#endif
						break;
				}
			}
		}
	}
}

#endif