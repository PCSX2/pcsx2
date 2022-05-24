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

#include "Common.h"
#include "vtlb.h"

#include "iCore.h"
#include "iR5900.h"
#include "common/Perf.h"

using namespace vtlb_private;
using namespace x86Emitter;

//////////////////////////////////////////////////////////////////////////////////////////
// iAllocRegSSE -- allocates an xmm register.  If no xmm register is available, xmm0 is
// saved into g_globalXMMData and returned as a free register.
//
class iAllocRegSSE
{
protected:
	xRegisterSSE m_reg;
	bool m_free;

public:
	iAllocRegSSE()
		: m_reg(xmm0)
		, m_free(!!_hasFreeXMMreg())
	{
		if (m_free)
			m_reg = xRegisterSSE(_allocTempXMMreg(XMMT_INT, -1));
		else
			xStoreReg(m_reg);
	}

	~iAllocRegSSE()
	{
		if (m_free)
			_freeXMMreg(m_reg.Id);
		else
			xRestoreReg(m_reg);
	}

	operator xRegisterSSE() const { return m_reg; }
};

// Moves 128 bits from point B to point A, using SSE's MOVAPS (or MOVDQA).
// This instruction always uses an SSE register, even if all registers are allocated!  It
// saves an SSE register to memory first, performs the copy, and restores the register.
//
static void iMOV128_SSE(const xIndirectVoid& destRm, const xIndirectVoid& srcRm)
{
	iAllocRegSSE reg;
	xMOVDQA(reg, srcRm);
	xMOVDQA(destRm, reg);
}

// Moves 64 bits of data from point B to point A, using either SSE, or x86 registers
//
static void iMOV64_Smart(const xIndirectVoid& destRm, const xIndirectVoid& srcRm)
{
	if (wordsize == 8)
	{
		xMOV(rax, srcRm);
		xMOV(destRm, rax);
		return;
	}

	if (_hasFreeXMMreg())
	{
		// Move things using MOVLPS:
		xRegisterSSE reg(_allocTempXMMreg(XMMT_INT, -1));
		xMOVL.PS(reg, srcRm);
		xMOVL.PS(destRm, reg);
		_freeXMMreg(reg.Id);
		return;
	}

	xMOV(eax, srcRm);
	xMOV(destRm, eax);
	xMOV(eax, srcRm + 4);
	xMOV(destRm + 4, eax);
}

/*
	// Pseudo-Code For the following Dynarec Implementations -->

	u32 vmv = vmap[addr>>VTLB_PAGE_BITS].raw();
	sptr ppf=addr+vmv;
	if (!(ppf<0))
	{
		data[0]=*reinterpret_cast<DataType*>(ppf);
		if (DataSize==128)
			data[1]=*reinterpret_cast<DataType*>(ppf+8);
		return 0;
	}
	else
	{
		//has to: translate, find function, call function
		u32 hand=(u8)vmv;
		u32 paddr=(ppf-hand) << 1;
		//Console.WriteLn("Translated 0x%08X to 0x%08X",params addr,paddr);
		return reinterpret_cast<TemplateHelper<DataSize,false>::HandlerType*>(RWFT[TemplateHelper<DataSize,false>::sidx][0][hand])(paddr,data);
	}

	// And in ASM it looks something like this -->

	mov eax,ecx;
	shr eax,VTLB_PAGE_BITS;
	mov rax,[rax*wordsize+vmap];
	add rcx,rax;
	js _fullread;

	//these are wrong order, just an example ...
	mov [rax],ecx;
	mov ecx,[rdx];
	mov [rax+4],ecx;
	mov ecx,[rdx+4];
	mov [rax+4+4],ecx;
	mov ecx,[rdx+4+4];
	mov [rax+4+4+4+4],ecx;
	mov ecx,[rdx+4+4+4+4];
	///....

	jmp cont;
	_fullread:
	movzx eax,al;
	sub   ecx,eax;
	call [eax+stuff];
	cont:
	........

*/

namespace vtlb_private
{
	// ------------------------------------------------------------------------
	// Prepares eax, ecx, and, ebx for Direct or Indirect operations.
	// Returns the writeback pointer for ebx (return address from indirect handling)
	//
	static u32* DynGen_PrepRegs()
	{
		// Warning dirty ebx (in case someone got the very bad idea to move this code)
		EE::Profiler.EmitMem();

		xMOV(eax, arg1regd);
		xSHR(eax, VTLB_PAGE_BITS);
		xMOV(rax, ptrNative[xComplexAddress(rbx, vtlbdata.vmap, rax * wordsize)]);
		u32* writeback = xLEA_Writeback(rbx);
		xADD(arg1reg, rax);

		return writeback;
	}

	// ------------------------------------------------------------------------
	static void DynGen_DirectRead(u32 bits, bool sign)
	{
		pxAssert(bits == 8 || bits == 16 || bits == 32);

		switch (bits)
		{
			case 8:
				if (sign)
					xMOVSX(eax, ptr8[arg1reg]);
				else
					xMOVZX(eax, ptr8[arg1reg]);
				break;

			case 16:
				if (sign)
					xMOVSX(eax, ptr16[arg1reg]);
				else
					xMOVZX(eax, ptr16[arg1reg]);
				break;

			case 32:
				xMOV(eax, ptr[arg1reg]);
				break;

			jNO_DEFAULT
		}
	}

	static void DynGen_DirectRead64(u32 bits)
	{
		pxAssert(bits == 64 || bits == 128);

		switch (bits) {
			case 64:
				xMOVQZX(xmm0, ptr64[arg1reg]);
				break;

			case 128:
				xMOVAPS(xmm0, ptr128[arg1reg]);
				break;

			jNO_DEFAULT
		}
	}

	// ------------------------------------------------------------------------
	static void DynGen_DirectWrite(u32 bits)
	{
		// TODO: x86Emitter can't use dil
		switch (bits)
		{
			//8 , 16, 32 : data on EDX
			case 8:
				xMOV(edx, arg2regd);
				xMOV(ptr[arg1reg], dl);
				break;

			case 16:
				xMOV(ptr[arg1reg], xRegister16(arg2reg));
				break;

			case 32:
				xMOV(ptr[arg1reg], arg2regd);
				break;

			case 64:
				iMOV64_Smart(ptr[arg1reg], ptr[arg2reg]);
				break;

			case 128:
				iMOV128_SSE(ptr[arg1reg], ptr[arg2reg]);
				break;
		}
	}
} // namespace vtlb_private

// ------------------------------------------------------------------------
// allocate one page for our naked indirect dispatcher function.
// this *must* be a full page, since we'll give it execution permission later.
// If it were smaller than a page we'd end up allowing execution rights on some
// other vars additionally (bad!).
//
alignas(__pagesize) static u8 m_IndirectDispatchers[__pagesize];

// ------------------------------------------------------------------------
// mode        - 0 for read, 1 for write!
// operandsize - 0 thru 4 represents 8, 16, 32, 64, and 128 bits.
//
static u8* GetIndirectDispatcherPtr(int mode, int operandsize, int sign = 0)
{
	assert(mode || operandsize >= 2 ? !sign : true);

	// Each dispatcher is aligned to 64 bytes.  The actual dispatchers are only like
	// 20-some bytes each, but 64 byte alignment on functions that are called
	// more frequently than a hot sex hotline at 1:15am is probably a good thing.

	// 7*64? 5 widths with two sign extension modes for 8 and 16 bit reads

	// Gregory: a 32 bytes alignment is likely enough and more cache friendly
	const int A = 32;

	return &m_IndirectDispatchers[(mode * (7 * A)) + (sign * 5 * A) + (operandsize * A)];
}

// ------------------------------------------------------------------------
// Generates a JS instruction that targets the appropriate templated instance of
// the vtlb Indirect Dispatcher.
//
static void DynGen_IndirectDispatch(int mode, int bits, bool sign = false)
{
	int szidx = 0;
	switch (bits)
	{
		case   8: szidx = 0; break;
		case  16: szidx = 1; break;
		case  32: szidx = 2; break;
		case  64: szidx = 3; break;
		case 128: szidx = 4; break;
		jNO_DEFAULT;
	}
	xJS(GetIndirectDispatcherPtr(mode, szidx, sign));
}

// ------------------------------------------------------------------------
// Generates the various instances of the indirect dispatchers
// In: arg1reg: vtlb entry, arg2reg: data ptr (if mode >= 64), rbx: function return ptr
// Out: eax: result (if mode < 64)
static void DynGen_IndirectTlbDispatcher(int mode, int bits, bool sign)
{
	xMOVZX(eax, al);
	if (wordsize != 8)
		xSUB(arg1regd, 0x80000000);
	xSUB(arg1regd, eax);

	// jump to the indirect handler, which is a C++ function.
	// [ecx is address, edx is data]
	sptr table = (sptr)vtlbdata.RWFT[bits][mode];
	if (table == (s32)table)
	{
		xFastCall(ptrNative[(rax * wordsize) + table], arg1reg, arg2reg);
	}
	else
	{
		xLEA(arg3reg, ptr[(void*)table]);
		xFastCall(ptrNative[(rax * wordsize) + arg3reg], arg1reg, arg2reg);
	}

	if (!mode)
	{
		if (bits == 0)
		{
			if (sign)
				xMOVSX(eax, al);
			else
				xMOVZX(eax, al);
		}
		else if (bits == 1)
		{
			if (sign)
				xMOVSX(eax, ax);
			else
				xMOVZX(eax, ax);
		}
	}

	xJMP(rbx);
}

// One-time initialization procedure.  Multiple subsequent calls during the lifespan of the
// process will be ignored.
//
void vtlb_dynarec_init()
{
	static bool hasBeenCalled = false;
	if (hasBeenCalled)
		return;
	hasBeenCalled = true;

	// In case init gets called multiple times:
	HostSys::MemProtectStatic(m_IndirectDispatchers, PageAccess_ReadWrite());

	// clear the buffer to 0xcc (easier debugging).
	memset(m_IndirectDispatchers, 0xcc, __pagesize);

	for (int mode = 0; mode < 2; ++mode)
	{
		for (int bits = 0; bits < 5; ++bits)
		{
			for (int sign = 0; sign < (!mode && bits < 2 ? 2 : 1); sign++)
			{
				xSetPtr(GetIndirectDispatcherPtr(mode, bits, !!sign));

				DynGen_IndirectTlbDispatcher(mode, bits, !!sign);
			}
		}
	}

	HostSys::MemProtectStatic(m_IndirectDispatchers, PageAccess_ExecOnly());

	Perf::any.map((uptr)m_IndirectDispatchers, __pagesize, "TLB Dispatcher");
}

static void vtlb_SetWriteback(u32* writeback)
{
	uptr val = (uptr)xGetPtr();
	if (wordsize == 8)
	{
		pxAssertMsg(*((u8*)writeback - 2) == 0x8d, "Expected codegen to be an LEA");
		val -= ((uptr)writeback + 4);
	}
	pxAssertMsg((sptr)val == (s32)val, "Writeback too far away!");
	*writeback = val;
}

//////////////////////////////////////////////////////////////////////////////////////////
//                            Dynarec Load Implementations
int vtlb_DynGenRead64(u32 bits, int gpr)
{
	pxAssume(bits == 64 || bits == 128);

	u32* writeback = DynGen_PrepRegs();

	int reg = gpr == -1 ? _allocTempXMMreg(XMMT_INT, 0) : _allocGPRtoXMMreg(0, gpr, MODE_WRITE); // Handler returns in xmm0
	DynGen_IndirectDispatch(0, bits);
	DynGen_DirectRead64(bits);

	vtlb_SetWriteback(writeback); // return target for indirect's call/ret
	return reg;
}

// ------------------------------------------------------------------------
// Recompiled input registers:
//   ecx - source address to read from
//   Returns read value in eax.
void vtlb_DynGenRead32(u32 bits, bool sign)
{
	pxAssume(bits <= 32);

	u32* writeback = DynGen_PrepRegs();

	DynGen_IndirectDispatch(0, bits, sign && bits < 32);
	DynGen_DirectRead(bits, sign);

	vtlb_SetWriteback(writeback);
}

// ------------------------------------------------------------------------
// TLB lookup is performed in const, with the assumption that the COP0/TLB will clear the
// recompiler if the TLB is changed.
int vtlb_DynGenRead64_Const(u32 bits, u32 addr_const, int gpr)
{
	EE::Profiler.EmitConstMem(addr_const);

	int reg;
	auto vmv = vtlbdata.vmap[addr_const >> VTLB_PAGE_BITS];
	if (!vmv.isHandler(addr_const))
	{
		void* ppf = reinterpret_cast<void*>(vmv.assumePtr(addr_const));
		reg = gpr == -1 ? _allocTempXMMreg(XMMT_INT, -1) : _allocGPRtoXMMreg(-1, gpr, MODE_WRITE);
		switch (bits)
		{
			case 64:
				xMOVQZX(xRegisterSSE(reg), ptr64[ppf]);
				break;

			case 128:
				xMOVAPS(xRegisterSSE(reg), ptr128[ppf]);
				break;

			jNO_DEFAULT
		}
	}
	else
	{
		// has to: translate, find function, call function
		u32 paddr = vmv.assumeHandlerGetPAddr(addr_const);

		int szidx = 0;
		switch (bits)
		{
			case  64: szidx = 3; break;
			case 128: szidx = 4; break;
		}

		iFlushCall(FLUSH_FULLVTLB);
		reg = gpr == -1 ? _allocTempXMMreg(XMMT_INT, 0) : _allocGPRtoXMMreg(0, gpr, MODE_WRITE); // Handler returns in xmm0
		xFastCall(vmv.assumeHandlerGetRaw(szidx, 0), paddr, arg2reg);
	}
	return reg;
}

// ------------------------------------------------------------------------
// Recompiled input registers:
//   ecx - source address to read from
//   Returns read value in eax.
//
// TLB lookup is performed in const, with the assumption that the COP0/TLB will clear the
// recompiler if the TLB is changed.
//
void vtlb_DynGenRead32_Const(u32 bits, bool sign, u32 addr_const)
{
	EE::Profiler.EmitConstMem(addr_const);

	auto vmv = vtlbdata.vmap[addr_const >> VTLB_PAGE_BITS];
	if (!vmv.isHandler(addr_const))
	{
		auto ppf = vmv.assumePtr(addr_const);
		switch (bits)
		{
			case 8:
				if (sign)
					xMOVSX(eax, ptr8[(u8*)ppf]);
				else
					xMOVZX(eax, ptr8[(u8*)ppf]);
				break;

			case 16:
				if (sign)
					xMOVSX(eax, ptr16[(u16*)ppf]);
				else
					xMOVZX(eax, ptr16[(u16*)ppf]);
				break;

			case 32:
				xMOV(eax, ptr32[(u32*)ppf]);
				break;
		}
	}
	else
	{
		// has to: translate, find function, call function
		u32 paddr = vmv.assumeHandlerGetPAddr(addr_const);

		int szidx = 0;
		switch (bits)
		{
			case  8: szidx = 0; break;
			case 16: szidx = 1; break;
			case 32: szidx = 2; break;
		}

		// Shortcut for the INTC_STAT register, which many games like to spin on heavily.
		if ((bits == 32) && !EmuConfig.Speedhacks.IntcStat && (paddr == INTC_STAT))
		{
			xMOV(eax, ptr[&psHu32(INTC_STAT)]);
		}
		else
		{
			iFlushCall(FLUSH_FULLVTLB);
			xFastCall(vmv.assumeHandlerGetRaw(szidx, false), paddr);

			// perform sign extension on the result:

			if (bits == 8)
			{
				if (sign)
					xMOVSX(eax, al);
				else
					xMOVZX(eax, al);
			}
			else if (bits == 16)
			{
				if (sign)
					xMOVSX(eax, ax);
				else
					xMOVZX(eax, ax);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//                            Dynarec Store Implementations

void vtlb_DynGenWrite(u32 sz)
{
	u32* writeback = DynGen_PrepRegs();

	DynGen_IndirectDispatch(1, sz);
	DynGen_DirectWrite(sz);

	vtlb_SetWriteback(writeback);
}


// ------------------------------------------------------------------------
// Generates code for a store instruction, where the address is a known constant.
// TLB lookup is performed in const, with the assumption that the COP0/TLB will clear the
// recompiler if the TLB is changed.
void vtlb_DynGenWrite_Const(u32 bits, u32 addr_const)
{
	EE::Profiler.EmitConstMem(addr_const);

	auto vmv = vtlbdata.vmap[addr_const >> VTLB_PAGE_BITS];
	if (!vmv.isHandler(addr_const))
	{
		// TODO: x86Emitter can't use dil
		auto ppf = vmv.assumePtr(addr_const);
		switch (bits)
		{
			//8 , 16, 32 : data on arg2
			case 8:
				xMOV(edx, arg2regd);
				xMOV(ptr[(void*)ppf], dl);
				break;

			case 16:
				xMOV(ptr[(void*)ppf], xRegister16(arg2reg));
				break;

			case 32:
				xMOV(ptr[(void*)ppf], arg2regd);
				break;

			case 64:
				iMOV64_Smart(ptr[(void*)ppf], ptr[arg2reg]);
				break;

			case 128:
				iMOV128_SSE(ptr[(void*)ppf], ptr[arg2reg]);
				break;
		}
	}
	else
	{
		// has to: translate, find function, call function
		u32 paddr = vmv.assumeHandlerGetPAddr(addr_const);

		int szidx = 0;
		switch (bits)
		{
			case   8: szidx=0; break;
			case  16: szidx=1; break;
			case  32: szidx=2; break;
			case  64: szidx=3; break;
			case 128: szidx=4; break;
		}

		iFlushCall(FLUSH_FULLVTLB);
		xFastCall(vmv.assumeHandlerGetRaw(szidx, true), paddr, arg2reg);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//							Extra Implementations

//   ecx - virtual address
//   Returns physical address in eax.
//   Clobbers edx
void vtlb_DynV2P()
{
	xMOV(eax, ecx);
	xAND(ecx, VTLB_PAGE_MASK); // vaddr & VTLB_PAGE_MASK

	xSHR(eax, VTLB_PAGE_BITS);
	xMOV(eax, ptr[xComplexAddress(rdx, vtlbdata.ppmap, rax * 4)]); // vtlbdata.ppmap[vaddr >> VTLB_PAGE_BITS];

	xOR(eax, ecx);
}
