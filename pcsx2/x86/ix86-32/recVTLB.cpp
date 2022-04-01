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

// we need enough for a 32-bit jump forwards (5 bytes)
static constexpr u32 LOADSTORE_PADDING = 5;

static u32 GetAllocatedGPRBitmask()
{
	u32 mask = 0;
	for (u32 i = 0; i < iREGCNT_GPR; i++)
	{
		if (x86regs[i].inuse)
			mask |= (1u << i);
	}
	return mask;
}

static u32 GetAllocatedXMMBitmask()
{
	u32 mask = 0;
	for (u32 i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse)
			mask |= (1u << i);
	}
	return mask;
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
		pxAssert(bits == 8 || bits == 16 || bits == 32 || bits == 64 || bits == 128);

		switch (bits)
		{
			case 8:
				if (sign)
					xMOVSX(rax, ptr8[arg1reg]);
				else
					xMOVZX(rax, ptr8[arg1reg]);
				break;

			case 16:
				if (sign)
					xMOVSX(rax, ptr16[arg1reg]);
				else
					xMOVZX(rax, ptr16[arg1reg]);
				break;

			case 32:
				if (sign)
					xMOVSX(rax, ptr32[arg1reg]);
				else
					xMOV(eax, ptr32[arg1reg]);
				break;

			case 64:
				xMOV(rax, ptr64[arg1reg]);
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
				xMOV(ptr[arg1reg], arg2reg);
				break;

			case 128:
				xMOVAPS(ptr[arg1reg], xmm1);
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
	assert(mode || operandsize >= 3 ? !sign : true);

	// Each dispatcher is aligned to 64 bytes.  The actual dispatchers are only like
	// 20-some bytes each, but 64 byte alignment on functions that are called
	// more frequently than a hot sex hotline at 1:15am is probably a good thing.

	// 7*64? 5 widths with two sign extension modes for 8 and 16 bit reads

	// Gregory: a 32 bytes alignment is likely enough and more cache friendly
	const int A = 32;

	return &m_IndirectDispatchers[(mode * (8 * A)) + (sign * 5 * A) + (operandsize * A)];
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

	// jump to the indirect handler, which is a __fastcall C++ function.
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
				xMOVSX(rax, al);
			else
				xMOVZX(rax, al);
		}
		else if (bits == 1)
		{
			if (sign)
				xMOVSX(rax, ax);
			else
				xMOVZX(rax, ax);
		}
		else if (bits == 2)
		{
			if (sign)
				xCDQE();
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
			for (int sign = 0; sign < (!mode && bits < 3 ? 2 : 1); sign++)
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
int vtlb_DynGenReadQuad(u32 bits, int gpr)
{
	pxAssume(bits == 128);

	if (!CHECK_FASTMEM || vtlb_IsFaultingPC(pc))
	{
		u32* writeback = DynGen_PrepRegs();

		const int reg = gpr == -1 ? _allocTempXMMreg(XMMT_INT, 0) : _allocGPRtoXMMreg(0, gpr, MODE_WRITE); // Handler returns in xmm0
		DynGen_IndirectDispatch(0, bits);
		DynGen_DirectRead(bits, false);

		vtlb_SetWriteback(writeback); // return target for indirect's call/ret

		return reg;
	}

	const int reg = gpr == -1 ? _allocTempXMMreg(XMMT_INT, 0) : _allocGPRtoXMMreg(0, gpr, MODE_WRITE); // Handler returns in xmm0
	const u8* codeStart = x86Ptr;

	xMOVAPS(xRegisterSSE(reg), ptr128[RFASTMEMBASE + arg1reg]);

	const u32 padding = LOADSTORE_PADDING - std::min<u32>(static_cast<u32>(x86Ptr - codeStart), 5);
	for (u32 i = 0; i < padding; i++)
		xNOP();

	vtlb_AddLoadStoreInfo((uptr)codeStart, static_cast<u32>(x86Ptr - codeStart),
		pc, GetAllocatedGPRBitmask(), GetAllocatedXMMBitmask(),
		static_cast<u8>(arg1reg.GetId()), static_cast<u8>(reg),
		static_cast<u8>(bits), false, true, true);

	return reg;
}

// ------------------------------------------------------------------------
// Recompiled input registers:
//   ecx - source address to read from
//   Returns read value in eax.
void vtlb_DynGenReadNonQuad(u32 bits, bool sign)
{
	pxAssume(bits <= 64);

	if (!CHECK_FASTMEM || vtlb_IsFaultingPC(pc))
	{
		u32* writeback = DynGen_PrepRegs();

		DynGen_IndirectDispatch(0, bits, sign && bits < 64);
		DynGen_DirectRead(bits, sign);

		vtlb_SetWriteback(writeback);
		return;
	}

	const u8* codeStart = x86Ptr;

	switch (bits)
	{
	case 8:
		sign ? xMOVSX(rax, ptr8[RFASTMEMBASE + arg1reg]) : xMOVZX(rax, ptr8[RFASTMEMBASE + arg1reg]);
		break;
	case 16:
		sign ? xMOVSX(rax, ptr16[RFASTMEMBASE + arg1reg]) : xMOVZX(rax, ptr16[RFASTMEMBASE + arg1reg]);
		break;
	case 32:
		sign ? xMOVSX(rax, ptr32[RFASTMEMBASE + arg1reg]) : xMOV(eax, ptr32[RFASTMEMBASE + arg1reg]);
		break;
	case 64:
		xMOV(rax, ptr64[RFASTMEMBASE + arg1reg]);
		break;

	jNO_DEFAULT
	}

	const u32 padding = LOADSTORE_PADDING - std::min<u32>(static_cast<u32>(x86Ptr - codeStart), 5);
	for (u32 i = 0; i < padding; i++)
		xNOP();

	vtlb_AddLoadStoreInfo((uptr)codeStart, static_cast<u32>(x86Ptr - codeStart),
		pc, GetAllocatedGPRBitmask(), GetAllocatedXMMBitmask(),
		static_cast<u8>(arg1reg.GetId()), static_cast<u8>(eax.GetId()),
		static_cast<u8>(bits), sign, true, false);
}

// ------------------------------------------------------------------------
// TLB lookup is performed in const, with the assumption that the COP0/TLB will clear the
// recompiler if the TLB is changed.
int vtlb_DynGenReadQuad_Const(u32 bits, u32 addr_const, int gpr)
{
	pxAssert(bits == 128);

	EE::Profiler.EmitConstMem(addr_const);

	int reg;
	auto vmv = vtlbdata.vmap[addr_const >> VTLB_PAGE_BITS];
	if (!vmv.isHandler(addr_const))
	{
		void* ppf = reinterpret_cast<void*>(vmv.assumePtr(addr_const));
		reg = gpr == -1 ? _allocTempXMMreg(XMMT_INT, -1) : _allocGPRtoXMMreg(-1, gpr, MODE_WRITE);
		xMOVAPS(xRegisterSSE(reg), ptr128[ppf]);
	}
	else
	{
		// has to: translate, find function, call function
		u32 paddr = vmv.assumeHandlerGetPAddr(addr_const);

		const int szidx = 4;
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
void vtlb_DynGenReadNonQuad_Const(u32 bits, bool sign, u32 addr_const)
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
					xMOVSX(rax, ptr8[(u8*)ppf]);
				else
					xMOVZX(rax, ptr8[(u8*)ppf]);
				break;

			case 16:
				if (sign)
					xMOVSX(rax, ptr16[(u16*)ppf]);
				else
					xMOVZX(rax, ptr16[(u16*)ppf]);
				break;

			case 32:
				if (sign)
					xMOVSX(rax, ptr32[(u32*)ppf]);
				else
					xMOV(eax, ptr32[(u32*)ppf]);
				break;

			case 64:
				xMOV(rax, ptr64[(u64*)ppf]);
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
			case 64: szidx = 3; break;
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
					xMOVSX(rax, al);
				else
					xMOVZX(rax, al);
			}
			else if (bits == 16)
			{
				if (sign)
					xMOVSX(rax, ax);
				else
					xMOVZX(rax, ax);
			}
			else if (bits == 32)
			{
				if (sign)
					xCDQE();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//                            Dynarec Store Implementations

void vtlb_DynGenWrite(u32 sz)
{
	if (!CHECK_FASTMEM || vtlb_IsFaultingPC(pc))
	{
		u32* writeback = DynGen_PrepRegs();

		DynGen_IndirectDispatch(1, sz);
		DynGen_DirectWrite(sz);

		vtlb_SetWriteback(writeback);
		return;
	}

	const u8* codeStart = x86Ptr;

	int data_register = 0;
	switch (sz)
	{
	case 8:
		xMOV(edx, arg2regd);
		xMOV(ptr8[RFASTMEMBASE + arg1reg], dl);
		data_register = edx.GetId();
		break;
	case 16:
		xMOV(ptr16[RFASTMEMBASE + arg1reg], xRegister16(arg2reg.GetId()));
		data_register = arg2reg.GetId();
		break;
	case 32:
		xMOV(ptr32[RFASTMEMBASE + arg1reg], arg2regd);
		data_register = arg2reg.GetId();
		break;
	case 64:
		xMOV(ptr64[RFASTMEMBASE + arg1reg], arg2reg);
		data_register = arg2reg.GetId();
		break;
	case 128:
		xMOVAPS(ptr128[RFASTMEMBASE + arg1reg], xmm1);
		data_register = xmm1.GetId();
		break;

		jNO_DEFAULT
	}

	const u32 padding = LOADSTORE_PADDING - std::min<u32>(static_cast<u32>(x86Ptr - codeStart), 5);
	for (u32 i = 0; i < padding; i++)
		xNOP();

	vtlb_AddLoadStoreInfo((uptr)codeStart, static_cast<u32>(x86Ptr - codeStart),
		pc, GetAllocatedGPRBitmask(), GetAllocatedXMMBitmask(),
		static_cast<u8>(arg1reg.GetId()), static_cast<u8>(data_register),
		static_cast<u8>(sz), false, false, (sz == 128));
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
				xMOV(ptr64[(void*)ppf], arg2reg);
				break;

			case 128:
				xMOVAPS(ptr128[(void*)ppf], xmm1);
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

static bool IsCallerSavedGPR(int i)
{
	return (i != calleeSavedReg1.GetId() && i != calleeSavedReg2.GetId());
}

void vtlb_DynBackpatchLoadStore(uptr code_address, u32 code_size, u32 guest_pc, u32 guest_addr,
	u32 gpr_bitmask, u32 fpr_bitmask, u8 address_register, u8 data_register,
	u8 size_in_bits, bool is_signed, bool is_load, bool is_fpr)
{
	static constexpr u32 GPR_SIZE = 8;
	static constexpr u32 XMM_SIZE = 16;

	// on win32, we need to reserve an additional 32 bytes shadow space when calling out to C
#ifdef _WIN32
	static constexpr u32 SHADOW_SIZE = 32;
#else
	static constexpr u32 SHADOW_SIZE = 0;
#endif

	DevCon.WriteLn("Backpatching %s at %p[%u] (pc %08X vaddr %08X): Bitmask %08X %08X Addr %u Data %u Size %u Flags %02X %02X",
		is_load ? "load" : "store", (void*)code_address, code_size, guest_pc, guest_addr, gpr_bitmask, fpr_bitmask,
		address_register, data_register, size_in_bits, is_signed, is_load);

	u8* thunk = recBeginThunk();

	// save regs
	u32 num_gprs = 0;
	u32 num_fprs = 0;
	for (u32 i = 0; i < iREGCNT_GPR; i++)
	{
		if ((gpr_bitmask & (1u << i)) && IsCallerSavedGPR(i) && (is_fpr || data_register != i))
			num_gprs++;
	}
	for (u32 i = 0; i < iREGCNT_XMM; i++)
	{
		if (fpr_bitmask & (1u << i))
			num_fprs++;
	}

	const u32 stack_size = (((num_gprs + 1) & ~1u) * GPR_SIZE) + (num_fprs * XMM_SIZE) + SHADOW_SIZE;

	if (stack_size > 0)
	{
		xSUB(rsp, stack_size);

		u32 stack_offset = SHADOW_SIZE;
		for (u32 i = 0; i < iREGCNT_XMM; i++)
		{
			if (fpr_bitmask & (1u << i) && (!is_load || !is_fpr || data_register != i))
			{
				xMOVAPS(ptr128[rsp + stack_offset], xRegisterSSE(i));
				stack_offset += XMM_SIZE;
			}
		}

		for (u32 i = 0; i < iREGCNT_GPR; i++)
		{
			if ((gpr_bitmask & (1u << i)) && IsCallerSavedGPR(i) && (!is_load || is_fpr || data_register != i))
			{
				xMOV(ptr64[rsp + stack_offset], xRegister64(i));
				stack_offset += GPR_SIZE;
			}
		}
	}

	if (is_load)
	{
		if (address_register != arg1reg.GetId())
			xMOV(arg1regd, xRegister32(address_register));

		u32* writeback = DynGen_PrepRegs();

		DynGen_IndirectDispatch(0, size_in_bits, is_signed && size_in_bits < 32);
		DynGen_DirectRead(size_in_bits, is_signed);

		vtlb_SetWriteback(writeback);

		if (size_in_bits == 128)
		{
			if (data_register != xmm0.GetId())
				xMOVAPS(xRegisterSSE(data_register), xmm0);
		}
		else
		{
			if (is_fpr)
			{
				xMOVDZX(xRegisterSSE(data_register), rax);
			}
			else
			{
				if (data_register != eax.GetId())
					xMOV(xRegister64(data_register), rax);
			}
		}
	}
	else
	{
		if (address_register != arg1reg.GetId())
			xMOV(arg1regd, xRegister32(address_register));

		if (size_in_bits == 128)
		{
			if (data_register != xmm1.GetId())
				xMOVAPS(xmm1, xRegisterSSE(data_register));
		}
		else
		{
			if (is_fpr)
			{
				xMOVD(arg2reg, xRegisterSSE(data_register));
			}
			else
			{
				if (data_register != arg2reg.GetId())
					xMOV(arg2reg, xRegister64(data_register));
			}
		}

		u32* writeback = DynGen_PrepRegs();

		DynGen_IndirectDispatch(1, size_in_bits);
		DynGen_DirectWrite(size_in_bits);

		vtlb_SetWriteback(writeback);
	}

	// restore regs
	if (stack_size > 0)
	{
		u32 stack_offset = SHADOW_SIZE;
		for (u32 i = 0; i < iREGCNT_XMM; i++)
		{
			if (fpr_bitmask & (1u << i) && (!is_load || !is_fpr || data_register != i))
			{
				xMOVAPS(xRegisterSSE(i), ptr128[rsp + stack_offset]);
				stack_offset += XMM_SIZE;
			}
		}

		for (u32 i = 0; i < iREGCNT_GPR; i++)
		{
			if ((gpr_bitmask & (1u << i)) && IsCallerSavedGPR(i) && (!is_load || is_fpr || data_register != i))
			{
				xMOV(xRegister64(i), ptr64[rsp + stack_offset]);
				stack_offset += GPR_SIZE;
			}
		}

		xADD(rsp, stack_size);
	}

	xJMP((void*)(code_address + code_size));

	recEndThunk();

	// backpatch to a jump to the slowmem handler
	x86Ptr = (u8*)code_address;
	xJMP(thunk);

	// fill the rest of it with nops, if any
	pxAssertRel(static_cast<u32>((uptr)x86Ptr - code_address) <= code_size, "Overflowed when backpatching");
	for (u32 i = static_cast<u32>((uptr)x86Ptr - code_address); i < code_size; i++)
		xNOP();
}
