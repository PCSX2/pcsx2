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

/*
 * ix86 core v0.9.1
 *
 * Original Authors (v0.6.2 and prior):
 *		linuzappz <linuzappz@pcsx.net>
 *		alexey silinov
 *		goldfinger
 *		zerofrog(@gmail.com)
 *
 * Authors of v0.9.1:
 *		Jake.Stine(@gmail.com)
 *		cottonvibes(@gmail.com)
 *		sudonim(1@gmail.com)
 */

#include "common/emitter/internal.h"
#include "common/emitter/tools.h"
#include <functional>

// ------------------------------------------------------------------------
// Notes on Thread Local Storage:
//  * TLS is pretty simple, and "just works" from a programmer perspective, with only
//    some minor additional computational overhead (see performance notes below).
//
//  * MSVC and GCC handle TLS differently internally, but behavior to the programmer is
//    generally identical.
//
// Performance Considerations:
//  * GCC's implementation involves an extra dereference from normal storage (possibly
//    applies to x86-32 only -- x86-64 is untested).
//
//  * MSVC's implementation involves *two* extra dereferences from normal storage because
//    it has to look up the TLS heap pointer from the Windows Thread Storage Area.  (in
//    generated ASM code, this dereference is denoted by access to the fs:[2ch] address),
//
//  * However, in either case, the optimizer usually optimizes it to a register so the
//    extra overhead is minimal over a series of instructions.
//
// MSVC Notes:
//  * Important!! the Full Optimization [/Ox] option effectively disables TLS optimizations
//    in MSVC 2008 and earlier, causing generally significant code bloat.  Not tested in
//    VC2010 yet.
//
//  * VC2010 generally does a superior job of optimizing TLS across inlined functions and
//    class methods, compared to predecessors.
//


thread_local u8* x86Ptr;
thread_local XMMSSEType g_xmmtypes[iREGCNT_XMM] = {XMMT_INT};

namespace x86Emitter
{

	template void xWrite<u8>(u8 val);
	template void xWrite<u16>(u16 val);
	template void xWrite<u32>(u32 val);
	template void xWrite<u64>(u64 val);
	template void xWrite<u128>(u128 val);

	__fi void xWrite8(u8 val)
	{
		xWrite(val);
	}

	__fi void xWrite16(u16 val)
	{
		xWrite(val);
	}

	__fi void xWrite32(u32 val)
	{
		xWrite(val);
	}

	__fi void xWrite64(u64 val)
	{
		xWrite(val);
	}

	// Empty initializers are due to frivolously pointless GCC errors (it demands the
	// objects be initialized even though they have no actual variable members).

	const xAddressIndexer<xIndirectVoid> ptr = {};
	const xAddressIndexer<xIndirectNative> ptrNative = {};
	const xAddressIndexer<xIndirect128> ptr128 = {};
	const xAddressIndexer<xIndirect64> ptr64 = {};
	const xAddressIndexer<xIndirect32> ptr32 = {};
	const xAddressIndexer<xIndirect16> ptr16 = {};
	const xAddressIndexer<xIndirect8> ptr8 = {};

	// ------------------------------------------------------------------------

	const xRegisterEmpty xEmptyReg = {};

	// clang-format off

const xRegisterSSE
    xmm0(0), xmm1(1),
    xmm2(2), xmm3(3),
    xmm4(4), xmm5(5),
    xmm6(6), xmm7(7),
    xmm8(8), xmm9(9),
    xmm10(10), xmm11(11),
    xmm12(12), xmm13(13),
    xmm14(14), xmm15(15);

const xAddressReg
    rax(0), rbx(3),
    rcx(1), rdx(2),
    rsp(4), rbp(5),
    rsi(6), rdi(7),
    r8(8), r9(9),
    r10(10), r11(11),
    r12(12), r13(13),
    r14(14), r15(15);

const xRegister32
    eax(0), ebx(3),
    ecx(1), edx(2),
    esp(4), ebp(5),
    esi(6), edi(7),
    r8d(8), r9d(9),
    r10d(10), r11d(11),
    r12d(12), r13d(13),
    r14d(14), r15d(15);

const xRegister16
    ax(0), bx(3),
    cx(1), dx(2),
    sp(4), bp(5),
    si(6), di(7);

const xRegister8
    al(0),
    dl(2), bl(3),
    ah(4), ch(5),
    dh(6), bh(7);

#if defined(_WIN32)
const xAddressReg
    arg1reg = rcx,
    arg2reg = rdx,
    arg3reg = r8,
    arg4reg = r9,
    calleeSavedReg1 = rdi,
    calleeSavedReg2 = rsi;

const xRegister32
    arg1regd = ecx,
    arg2regd = edx,
    calleeSavedReg1d = edi,
    calleeSavedReg2d = esi;
#else
const xAddressReg
    arg1reg = rdi,
    arg2reg = rsi,
    arg3reg = rdx,
    arg4reg = rcx,
    calleeSavedReg1 = r12,
    calleeSavedReg2 = r13;

const xRegister32
    arg1regd = edi,
    arg2regd = esi,
    calleeSavedReg1d = r12d,
    calleeSavedReg2d = r13d;
#endif

	// clang-format on

	const xRegisterCL cl;

	const char* const x86_regnames_gpr8[] =
	{
		"al", "cl", "dl", "bl",
		"ah", "ch", "dh", "bh",
		"b8", "b9", "b10", "b11",
		"b12", "b13", "b14", "b15"
	};

	const char* const x86_regnames_gpr16[] =
	{
		"ax", "cx", "dx", "bx",
		"sp", "bp", "si", "di",
		"h8", "h9", "h10", "h11",
		"h12", "h13", "h14", "h15"
	};

	const char* const x86_regnames_gpr32[] =
	{
		"eax", "ecx", "edx", "ebx",
		"esp", "ebp", "esi", "edi",
		"e8", "e9", "e10", "e11",
		"e12", "e13", "e14", "e15"
	};

	const char* const x86_regnames_gpr64[] =
	{
		"rax", "rcx", "rdx", "rbx",
		"rsp", "rbp", "rsi", "rdi",
		"r8", "r9", "r10", "r11",
		"r12", "r13", "r14", "r15"
	};

	const char* const x86_regnames_sse[] =
	{
		"xmm0", "xmm1", "xmm2", "xmm3",
		"xmm4", "xmm5", "xmm6", "xmm7",
		"xmm8", "xmm9", "xmm10", "xmm11",
		"xmm12", "xmm13", "xmm14", "xmm15"
	};

	const char* xRegisterBase::GetName()
	{
		if (Id == xRegId_Invalid)
			return "invalid";
		if (Id == xRegId_Empty)
			return "empty";

		// bad error?  Return a "big" error string.  Might break formatting of register tables
		// but that's the least of your worries if you see this baby.
		if (Id >= (int)iREGCNT_GPR || Id < 0)
			return "!Register index out of range!";

		switch (GetOperandSize())
		{
			case 1:
				return x86_regnames_gpr8[Id];
			case 2:
				return x86_regnames_gpr16[Id];
			case 4:
				return x86_regnames_gpr32[Id];
			case 8:
				return x86_regnames_gpr64[Id];
			case 16:
				return x86_regnames_sse[Id];
		}

		return "oops?";
	}

	//////////////////////////////////////////////////////////////////////////////////////////
	// Performance note: VC++ wants to use byte/word register form for the following
	// ModRM/SibSB constructors when we use xWrite<u8>, and furthermore unrolls the
	// the shift using a series of ADDs for the following results:
	//   add cl,cl
	//   add cl,cl
	//   add cl,cl
	//   or  cl,bl
	//   add cl,cl
	//  ... etc.
	//
	// This is unquestionably bad optimization by Core2 standard, an generates tons of
	// register aliases and false dependencies. (although may have been ideal for early-
	// brand P4s with a broken barrel shifter?).  The workaround is to do our own manual
	// x86Ptr access and update using a u32 instead of u8.  Thanks to little endianness,
	// the same end result is achieved and no false dependencies are generated.  The draw-
	// back is that it clobbers 3 bytes past the end of the write, which could cause a
	// headache for someone who himself is doing some kind of headache-inducing amount of
	// recompiler SMC.  So we don't do a work-around, and just hope for the compiler to
	// stop sucking someday instead. :)
	//
	// (btw, I know this isn't a critical performance item by any means, but it's
	//  annoying simply because it *should* be an easy thing to optimize)

	static __fi void ModRM(uint mod, uint reg, uint rm)
	{
		xWrite8((mod << 6) | (reg << 3) | rm);
	}

	static __fi void SibSB(u32 ss, u32 index, u32 base)
	{
		xWrite8((ss << 6) | (index << 3) | base);
	}

	void EmitSibMagic(uint regfield, const void* address, int extraRIPOffset)
	{
		sptr displacement = (sptr)address;
		sptr ripRelative = (sptr)address - ((sptr)x86Ptr + sizeof(s8) + sizeof(s32) + extraRIPOffset);
		// Can we use a rip-relative address?  (Prefer this over eiz because it's a byte shorter)
		if (ripRelative == (s32)ripRelative)
		{
			ModRM(0, regfield, ModRm_UseDisp32);
			displacement = ripRelative;
		}
		else
		{
			pxAssertDev(displacement == (s32)displacement, "SIB target is too far away, needs an indirect register");
			ModRM(0, regfield, ModRm_UseSib);
			SibSB(0, Sib_EIZ, Sib_UseDisp32);
		}

		xWrite<s32>((s32)displacement);
	}

	//////////////////////////////////////////////////////////////////////////////////////////
	// returns TRUE if this instruction requires SIB to be encoded, or FALSE if the
	// instruction ca be encoded as ModRm alone.
	static __fi bool NeedsSibMagic(const xIndirectVoid& info)
	{
		// no registers? no sibs!
		// (xIndirectVoid::Reduce always places a register in Index, and optionally leaves
		// Base empty if only register is specified)
		if (info.Index.IsEmpty())
			return false;

		// A scaled register needs a SIB
		if (info.Scale != 0)
			return true;

		// two registers needs a SIB
		if (!info.Base.IsEmpty())
			return true;

		return false;
	}

	//////////////////////////////////////////////////////////////////////////////////////////
	// Conditionally generates Sib encoding information!
	//
	// regfield - register field to be written to the ModRm.  This is either a register specifier
	//   or an opcode extension.  In either case, the instruction determines the value for us.
	//
	void EmitSibMagic(uint regfield, const xIndirectVoid& info, int extraRIPOffset)
	{
		// 3 bits also on x86_64 (so max is 8)
		// We might need to mask it on x86_64
		pxAssertDev(regfield < 8, "Invalid x86 register identifier.");
		int displacement_size = (info.Displacement == 0) ? 0 :
                                                           ((info.IsByteSizeDisp()) ? 1 : 2);

		pxAssert(!info.Base.IsEmpty() || !info.Index.IsEmpty() || displacement_size == 2);
		// Displacement is only 64 bits for rip-relative addressing
		pxAssert(info.Displacement == (s32)info.Displacement || (info.Base.IsEmpty() && info.Index.IsEmpty()));

		if (!NeedsSibMagic(info))
		{
			// Use ModRm-only encoding, with the rm field holding an index/base register, if
			// one has been specified.  If neither register is specified then use Disp32 form,
			// which is encoded as "EBP w/o displacement" (which is why EBP must always be
			// encoded *with* a displacement of 0, if it would otherwise not have one).

			if (info.Index.IsEmpty())
			{
				EmitSibMagic(regfield, (void*)info.Displacement, extraRIPOffset);
				return;
			}
			else
			{
				if (info.Index == rbp && displacement_size == 0)
					displacement_size = 1; // forces [ebp] to be encoded as [ebp+0]!

				ModRM(displacement_size, regfield, info.Index.Id & 7);
			}
		}
		else
		{
			// In order to encode "just" index*scale (and no base), we have to encode
			// it as a special [index*scale + displacement] form, which is done by
			// specifying EBP as the base register and setting the displacement field
			// to zero. (same as ModRm w/o SIB form above, basically, except the
			// ModRm_UseDisp flag is specified in the SIB instead of the ModRM field).

			if (info.Base.IsEmpty())
			{
				ModRM(0, regfield, ModRm_UseSib);
				SibSB(info.Scale, info.Index.Id, Sib_UseDisp32);
				xWrite<s32>(info.Displacement);
				return;
			}
			else
			{
				if (info.Base == rbp && displacement_size == 0)
					displacement_size = 1; // forces [ebp] to be encoded as [ebp+0]!

				ModRM(displacement_size, regfield, ModRm_UseSib);
				SibSB(info.Scale, info.Index.Id & 7, info.Base.Id & 7);
			}
		}

		if (displacement_size != 0)
		{
			if (displacement_size == 1)
				xWrite<s8>(info.Displacement);
			else
				xWrite<s32>(info.Displacement);
		}
	}

	// Writes a ModRM byte for "Direct" register access forms, which is used for all
	// instructions taking a form of [reg,reg].
	void EmitSibMagic(uint reg1, const xRegisterBase& reg2, int)
	{
		xWrite8((Mod_Direct << 6) | (reg1 << 3) | (reg2.Id & 7));
	}

	void EmitSibMagic(const xRegisterBase& reg1, const xRegisterBase& reg2, int)
	{
		xWrite8((Mod_Direct << 6) | ((reg1.Id & 7) << 3) | (reg2.Id & 7));
	}

	void EmitSibMagic(const xRegisterBase& reg1, const void* src, int extraRIPOffset)
	{
		EmitSibMagic(reg1.Id & 7, src, extraRIPOffset);
	}

	void EmitSibMagic(const xRegisterBase& reg1, const xIndirectVoid& sib, int extraRIPOffset)
	{
		EmitSibMagic(reg1.Id & 7, sib, extraRIPOffset);
	}

	//////////////////////////////////////////////////////////////////////////////////////////
	__emitinline static void EmitRex(bool w, bool r, bool x, bool b)
	{
		const u8 rex = 0x40 | (w << 3) | (r << 2) | (x << 1) | (u8)b;
		if (rex != 0x40)
			xWrite8(rex);
	}

	void EmitRex(uint regfield, const void* address)
	{
		pxAssert(0);
		bool w = false;
		bool r = false;
		bool x = false;
		bool b = false;
		EmitRex(w, r, x, b);
	}

	void EmitRex(uint regfield, const xIndirectVoid& info)
	{
		bool w = info.IsWide();
		bool r = false;
		bool x = info.Index.IsExtended();
		bool b = info.Base.IsExtended();
		if (!NeedsSibMagic(info))
		{
			b = x;
			x = false;
		}
		EmitRex(w, r, x, b);
	}

	void EmitRex(uint reg1, const xRegisterBase& reg2)
	{
		bool w = reg2.IsWide();
		bool r = false;
		bool x = false;
		bool b = reg2.IsExtended();
		EmitRex(w, r, x, b);
	}

	void EmitRex(const xRegisterBase& reg1, const xRegisterBase& reg2)
	{
		bool w = reg1.IsWide();
		bool r = reg1.IsExtended();
		bool x = false;
		bool b = reg2.IsExtended();
		EmitRex(w, r, x, b);
	}

	void EmitRex(const xRegisterBase& reg1, const void* src)
	{
		pxAssert(0); //see fixme
		bool w = reg1.IsWide();
		bool r = reg1.IsExtended();
		bool x = false;
		bool b = false; // FIXME src.IsExtended();
		EmitRex(w, r, x, b);
	}

	void EmitRex(const xRegisterBase& reg1, const xIndirectVoid& sib)
	{
		bool w = reg1.IsWide();
		bool r = reg1.IsExtended();
		bool x = sib.Index.IsExtended();
		bool b = sib.Base.IsExtended();
		if (!NeedsSibMagic(sib))
		{
			b = x;
			x = false;
		}
		EmitRex(w, r, x, b);
	}

	// For use by instructions that are implicitly wide
	void EmitRexImplicitlyWide(const xRegisterBase& reg)
	{
		bool w = false;
		bool r = false;
		bool x = false;
		bool b = reg.IsExtended();
		EmitRex(w, r, x, b);
	}

	void EmitRexImplicitlyWide(const xIndirectVoid& sib)
	{
		bool w = false;
		bool r = false;
		bool x = sib.Index.IsExtended();
		bool b = sib.Base.IsExtended();
		if (!NeedsSibMagic(sib))
		{
			b = x;
			x = false;
		}
		EmitRex(w, r, x, b);
	}


	// --------------------------------------------------------------------------------------
	//  xSetPtr / xAlignPtr / xGetPtr / xAdvancePtr
	// --------------------------------------------------------------------------------------

	// Assigns the current emitter buffer target address.
	// This is provided instead of using x86Ptr directly, since we may in the future find
	// a need to change the storage class system for the x86Ptr 'under the hood.'
	__emitinline void xSetPtr(void* ptr)
	{
		x86Ptr = (u8*)ptr;
	}

	// Retrieves the current emitter buffer target address.
	// This is provided instead of using x86Ptr directly, since we may in the future find
	// a need to change the storage class system for the x86Ptr 'under the hood.'
	__emitinline u8* xGetPtr()
	{
		return x86Ptr;
	}

	__emitinline void xAlignPtr(uint bytes)
	{
		// forward align
		x86Ptr = (u8*)(((uptr)x86Ptr + bytes - 1) & ~(uptr)(bytes - 1));
	}

	// Performs best-case alignment for the target CPU, for use prior to starting a new
	// function.  This is not meant to be used prior to jump targets, since it doesn't
	// add padding (additionally, speed benefit from jump alignment is minimal, and often
	// a loss).
	__emitinline void xAlignCallTarget()
	{
		// Core2/i7 CPUs prefer unaligned addresses.  Checking for SSSE3 is a decent filter.
		// (also align in debug modes for disasm convenience)

		if (IsDebugBuild || !x86caps.hasSupplementalStreamingSIMD3Extensions)
		{
			// - P4's and earlier prefer 16 byte alignment.
			// - AMD Athlons and Phenoms prefer 8 byte alignment, but I don't have an easy
			//   heuristic for it yet.
			// - AMD Phenom IIs are unknown (either prefer 8 byte, or unaligned).

			xAlignPtr(16);
		}
	}

	__emitinline u8* xGetAlignedCallTarget()
	{
		xAlignCallTarget();
		return x86Ptr;
	}

	__emitinline void xAdvancePtr(uint bytes)
	{
		if (IsDevBuild)
		{
			// common debugger courtesy: advance with INT3 as filler.
			for (uint i = 0; i < bytes; i++)
				xWrite8(0xcc);
		}
		else
			x86Ptr += bytes;
	}

	// --------------------------------------------------------------------------------------
	//  xRegisterInt  (method implementations)
	// --------------------------------------------------------------------------------------
	xRegisterInt xRegisterInt::MatchSizeTo(xRegisterInt other) const
	{
		return other.GetOperandSize() == 1 ? xRegisterInt(xRegister8(*this)) : xRegisterInt(other.GetOperandSize(), Id);
	}

	// --------------------------------------------------------------------------------------
	//  xAddressReg  (operator overloads)
	// --------------------------------------------------------------------------------------
	xAddressVoid xAddressReg::operator+(const xAddressReg& right) const
	{
		pxAssertMsg(right.Id != -1 || Id != -1, "Uninitialized x86 register.");
		return xAddressVoid(*this, right);
	}

	xAddressVoid xAddressReg::operator+(sptr right) const
	{
		pxAssertMsg(Id != -1, "Uninitialized x86 register.");
		return xAddressVoid(*this, right);
	}

	xAddressVoid xAddressReg::operator+(const void* right) const
	{
		pxAssertMsg(Id != -1, "Uninitialized x86 register.");
		return xAddressVoid(*this, (sptr)right);
	}

	xAddressVoid xAddressReg::operator-(sptr right) const
	{
		pxAssertMsg(Id != -1, "Uninitialized x86 register.");
		return xAddressVoid(*this, -right);
	}

	xAddressVoid xAddressReg::operator-(const void* right) const
	{
		pxAssertMsg(Id != -1, "Uninitialized x86 register.");
		return xAddressVoid(*this, -(sptr)right);
	}

	xAddressVoid xAddressReg::operator*(int factor) const
	{
		pxAssertMsg(Id != -1, "Uninitialized x86 register.");
		return xAddressVoid(xEmptyReg, *this, factor);
	}

	xAddressVoid xAddressReg::operator<<(u32 shift) const
	{
		pxAssertMsg(Id != -1, "Uninitialized x86 register.");
		return xAddressVoid(xEmptyReg, *this, 1 << shift);
	}


	// --------------------------------------------------------------------------------------
	//  xAddressVoid  (method implementations)
	// --------------------------------------------------------------------------------------

	xAddressVoid::xAddressVoid(const xAddressReg& base, const xAddressReg& index, int factor, sptr displacement)
	{
		Base = base;
		Index = index;
		Factor = factor;
		Displacement = displacement;

		pxAssertMsg(base.Id != xRegId_Invalid, "Uninitialized x86 register.");
		pxAssertMsg(index.Id != xRegId_Invalid, "Uninitialized x86 register.");
	}

	xAddressVoid::xAddressVoid(const xAddressReg& index, sptr displacement)
	{
		Base = xEmptyReg;
		Index = index;
		Factor = 0;
		Displacement = displacement;

		pxAssertMsg(index.Id != xRegId_Invalid, "Uninitialized x86 register.");
	}

	xAddressVoid::xAddressVoid(sptr displacement)
	{
		Base = xEmptyReg;
		Index = xEmptyReg;
		Factor = 0;
		Displacement = displacement;
	}

	xAddressVoid::xAddressVoid(const void* displacement)
	{
		Base = xEmptyReg;
		Index = xEmptyReg;
		Factor = 0;
		Displacement = (sptr)displacement;
	}

	xAddressVoid& xAddressVoid::Add(const xAddressReg& src)
	{
		if (src == Index)
		{
			Factor++;
		}
		else if (src == Base)
		{
			// Compound the existing register reference into the Index/Scale pair.
			Base = xEmptyReg;

			if (src == Index)
				Factor++;
			else
			{
				pxAssertDev(Index.IsEmpty(), "x86Emitter: Only one scaled index register is allowed in an address modifier.");
				Index = src;
				Factor = 2;
			}
		}
		else if (Base.IsEmpty())
			Base = src;
		else if (Index.IsEmpty())
			Index = src;
		else
			pxAssumeDev(false, "x86Emitter: address modifiers cannot have more than two index registers."); // oops, only 2 regs allowed per ModRm!

		return *this;
	}

	xAddressVoid& xAddressVoid::Add(const xAddressVoid& src)
	{
		Add(src.Base);
		Add(src.Displacement);

		// If the factor is 1, we can just treat index like a base register also.
		if (src.Factor == 1)
		{
			Add(src.Index);
		}
		else if (Index.IsEmpty())
		{
			Index = src.Index;
			Factor = src.Factor;
		}
		else if (Index == src.Index)
		{
			Factor += src.Factor;
		}
		else
			pxAssumeDev(false, "x86Emitter: address modifiers cannot have more than two index registers."); // oops, only 2 regs allowed per ModRm!

		return *this;
	}

	xIndirectVoid::xIndirectVoid(const xAddressVoid& src)
	{
		Base = src.Base;
		Index = src.Index;
		Scale = src.Factor;
		Displacement = src.Displacement;

		Reduce();
	}

	xIndirectVoid::xIndirectVoid(sptr disp)
	{
		Base = xEmptyReg;
		Index = xEmptyReg;
		Scale = 0;
		Displacement = disp;

		// no reduction necessary :D
	}

	xIndirectVoid::xIndirectVoid(xAddressReg base, xAddressReg index, int scale, sptr displacement)
	{
		Base = base;
		Index = index;
		Scale = scale;
		Displacement = displacement;

		Reduce();
	}

	// Generates a 'reduced' ModSib form, which has valid Base, Index, and Scale values.
	// Necessary because by default ModSib compounds registers into Index when possible.
	//
	// If the ModSib is in illegal form ([Base + Index*5] for example) then an assertion
	// followed by an InvalidParameter Exception will be tossed around in haphazard
	// fashion.
	//
	// Optimization Note: Currently VC does a piss poor job of inlining this, even though
	// constant propagation *should* resove it to little or no code (VC's constprop fails
	// on C++ class initializers).  There is a work around [using array initializers instead]
	// but it's too much trouble for code that isn't performance critical anyway.
	// And, with luck, maybe VC10 will optimize it better and make it a non-issue. :D
	//
	void xIndirectVoid::Reduce()
	{
		if (Index.IsStackPointer())
		{
			// esp cannot be encoded as the index, so move it to the Base, if possible.
			// note: intentionally leave index assigned to esp also (generates correct
			// encoding later, since ESP cannot be encoded 'alone')

			pxAssert(Scale == 0); // esp can't have an index modifier!
			pxAssert(Base.IsEmpty()); // base must be empty or else!

			Base = Index;
			return;
		}

		// If no index reg, then load the base register into the index slot.
		if (Index.IsEmpty())
		{
			Index = Base;
			Scale = 0;
			if (!Base.IsStackPointer()) // prevent ESP from being encoded 'alone'
				Base = xEmptyReg;
			return;
		}

		// The Scale has a series of valid forms, all shown here:

		switch (Scale)
		{
			case 0:
				break;
			case 1:
				Scale = 0;
				break;
			case 2:
				Scale = 1;
				break;

			case 3: // becomes [reg*2+reg]
				pxAssertDev(Base.IsEmpty(), "Cannot scale an Index register by 3 when Base is not empty!");
				Base = Index;
				Scale = 1;
				break;

			case 4:
				Scale = 2;
				break;

			case 5: // becomes [reg*4+reg]
				pxAssertDev(Base.IsEmpty(), "Cannot scale an Index register by 5 when Base is not empty!");
				Base = Index;
				Scale = 2;
				break;

			case 6: // invalid!
				pxAssumeDev(false, "x86 asm cannot scale a register by 6.");
				break;

			case 7: // so invalid!
				pxAssumeDev(false, "x86 asm cannot scale a register by 7.");
				break;

			case 8:
				Scale = 3;
				break;
			case 9: // becomes [reg*8+reg]
				pxAssertDev(Base.IsEmpty(), "Cannot scale an Index register by 9 when Base is not empty!");
				Base = Index;
				Scale = 3;
				break;

				jNO_DEFAULT
		}
	}

	xIndirectVoid& xIndirectVoid::Add(sptr imm)
	{
		Displacement += imm;
		return *this;
	}

	// ------------------------------------------------------------------------
	// Internal implementation of EmitSibMagic which has been custom tailored
	// to optimize special forms of the Lea instructions accordingly, such
	// as when a LEA can be replaced with a "MOV reg,imm" or "MOV reg,reg".
	//
	// preserve_flags - set to ture to disable use of SHL on [Index*Base] forms
	// of LEA, which alters flags states.
	//
	static void EmitLeaMagic(const xRegisterInt& to, const xIndirectVoid& src, bool preserve_flags)
	{
		int displacement_size = (src.Displacement == 0) ? 0 :
                                                          ((src.IsByteSizeDisp()) ? 1 : 2);

		// See EmitSibMagic for commenting on SIB encoding.

		if (!NeedsSibMagic(src) && src.Displacement == (s32)src.Displacement)
		{
			// LEA Land: means we have either 1-register encoding or just an offset.
			// offset is encodable as an immediate MOV, and a register is encodable
			// as a register MOV.

			if (src.Index.IsEmpty())
			{
				xMOV(to, src.Displacement);
				return;
			}
			else if (displacement_size == 0)
			{
				_xMovRtoR(to, src.Index.MatchSizeTo(to));
				return;
			}
			else if (!preserve_flags)
			{
				// encode as MOV and ADD combo.  Make sure to use the immediate on the
				// ADD since it can encode as an 8-bit sign-extended value.

				_xMovRtoR(to, src.Index.MatchSizeTo(to));
				xADD(to, src.Displacement);
				return;
			}
		}
		else
		{
			if (src.Base.IsEmpty())
			{
				if (!preserve_flags && (displacement_size == 0))
				{
					// Encode [Index*Scale] as a combination of Mov and Shl.
					// This is more efficient because of the bloated LEA format which requires
					// a 32 bit displacement, and the compact nature of the alternative.
					//
					// (this does not apply to older model P4s with the broken barrel shifter,
					//  but we currently aren't optimizing for that target anyway).

					_xMovRtoR(to, src.Index);
					xSHL(to, src.Scale);
					return;
				}
			}
			else
			{
				if (src.Scale == 0)
				{
					if (!preserve_flags)
					{
						if (src.Index == rsp)
						{
							// ESP is not encodable as an index (ix86 ignores it), thus:
							_xMovRtoR(to, src.Base.MatchSizeTo(to)); // will do the trick!
							if (src.Displacement)
								xADD(to, src.Displacement);
							return;
						}
						else if (src.Displacement == 0)
						{
							_xMovRtoR(to, src.Base.MatchSizeTo(to));
							_g1_EmitOp(G1Type_ADD, to, src.Index.MatchSizeTo(to));
							return;
						}
					}
					else if ((src.Index == rsp) && (src.Displacement == 0))
					{
						// special case handling of ESP as Index, which is replaceable with
						// a single MOV even when preserve_flags is set! :D

						_xMovRtoR(to, src.Base.MatchSizeTo(to));
						return;
					}
				}
			}
		}

		xOpWrite(0, 0x8d, to, src);
	}

	__emitinline void xLEA(xRegister64 to, const xIndirectVoid& src, bool preserve_flags)
	{
		EmitLeaMagic(to, src, preserve_flags);
	}

	__emitinline void xLEA(xRegister32 to, const xIndirectVoid& src, bool preserve_flags)
	{
		EmitLeaMagic(to, src, preserve_flags);
	}

	__emitinline void xLEA(xRegister16 to, const xIndirectVoid& src, bool preserve_flags)
	{
		xWrite8(0x66);
		EmitLeaMagic(to, src, preserve_flags);
	}

	__emitinline u32* xLEA_Writeback(xAddressReg to)
	{
		xOpWrite(0, 0x8d, to, ptr[(void*)(0xdcdcdcd + (uptr)xGetPtr() + 7)]);

		return (u32*)xGetPtr() - 1;
	}

	// =====================================================================================================
	//  TEST / INC / DEC
	// =====================================================================================================
	void xImpl_Test::operator()(const xRegisterInt& to, const xRegisterInt& from) const
	{
		pxAssert(to.GetOperandSize() == from.GetOperandSize());
		xOpWrite(to.GetPrefix16(), to.Is8BitOp() ? 0x84 : 0x85, from, to);
	}

	void xImpl_Test::operator()(const xIndirect64orLess& dest, int imm) const
	{
		xOpWrite(dest.GetPrefix16(), dest.Is8BitOp() ? 0xf6 : 0xf7, 0, dest, dest.GetImmSize());
		dest.xWriteImm(imm);
	}

	void xImpl_Test::operator()(const xRegisterInt& to, int imm) const
	{
		if (to.IsAccumulator())
		{
			xOpAccWrite(to.GetPrefix16(), to.Is8BitOp() ? 0xa8 : 0xa9, 0, to);
		}
		else
		{
			xOpWrite(to.GetPrefix16(), to.Is8BitOp() ? 0xf6 : 0xf7, 0, to);
		}
		to.xWriteImm(imm);
	}

	void xImpl_BitScan::operator()(const xRegister16or32or64& to, const xRegister16or32or64& from) const
	{
		pxAssert(to->GetOperandSize() == from->GetOperandSize());
		xOpWrite0F(from->GetPrefix16(), Opcode, to, from);
	}
	void xImpl_BitScan::operator()(const xRegister16or32or64& to, const xIndirectVoid& sibsrc) const
	{
		xOpWrite0F(to->GetPrefix16(), Opcode, to, sibsrc);
	}

	void xImpl_IncDec::operator()(const xRegisterInt& to) const
	{
		if (to.Is8BitOp())
		{
			u8 regfield = isDec ? 1 : 0;
			xOpWrite(to.GetPrefix16(), 0xfe, regfield, to);
		}
		else
		{
			xOpWrite(to.GetPrefix16(), 0xff, isDec ? 1 : 0, to);
		}
	}

	void xImpl_IncDec::operator()(const xIndirect64orLess& to) const
	{
		to.prefix16();
		xWrite8(to.Is8BitOp() ? 0xfe : 0xff);
		EmitSibMagic(isDec ? 1 : 0, to);
	}

	void xImpl_DwordShift::operator()(const xRegister16or32or64& to, const xRegister16or32or64& from, const xRegisterCL& /* clreg */) const
	{
		pxAssert(to->GetOperandSize() == from->GetOperandSize());
		xOpWrite0F(from->GetPrefix16(), OpcodeBase + 1, to, from);
	}

	void xImpl_DwordShift::operator()(const xRegister16or32or64& to, const xRegister16or32or64& from, u8 shiftcnt) const
	{
		pxAssert(to->GetOperandSize() == from->GetOperandSize());
		if (shiftcnt != 0)
			xOpWrite0F(from->GetPrefix16(), OpcodeBase, to, from, shiftcnt);
	}

	void xImpl_DwordShift::operator()(const xIndirectVoid& dest, const xRegister16or32or64& from, const xRegisterCL& /* clreg */) const
	{
		xOpWrite0F(from->GetPrefix16(), OpcodeBase + 1, from, dest);
	}

	void xImpl_DwordShift::operator()(const xIndirectVoid& dest, const xRegister16or32or64& from, u8 shiftcnt) const
	{
		if (shiftcnt != 0)
			xOpWrite0F(from->GetPrefix16(), OpcodeBase, from, dest, shiftcnt);
	}

	const xImpl_Test xTEST = {};

	const xImpl_BitScan xBSF = {0xbc};
	const xImpl_BitScan xBSR = {0xbd};

	const xImpl_IncDec xINC = {false};
	const xImpl_IncDec xDEC = {true};

	const xImpl_DwordShift xSHLD = {0xa4};
	const xImpl_DwordShift xSHRD = {0xac};

	//////////////////////////////////////////////////////////////////////////////////////////
	// Push / Pop Emitters
	//
	// Note: pushad/popad implementations are intentionally left out.  The instructions are
	// invalid in x64, and are super slow on x32.  Use multiple Push/Pop instructions instead.

	__emitinline void xPOP(const xIndirectVoid& from)
	{
		EmitRexImplicitlyWide(from);
		xWrite8(0x8f);
		EmitSibMagic(0, from);
	}

	__emitinline void xPUSH(const xIndirectVoid& from)
	{
		EmitRexImplicitlyWide(from);
		xWrite8(0xff);
		EmitSibMagic(6, from);
	}

	__fi void xPOP(xRegister32or64 from)
	{
		EmitRexImplicitlyWide(from);
		xWrite8(0x58 | (from->Id & 7));
	}

	__fi void xPUSH(u32 imm)
	{
		if (is_s8(imm))
		{
			xWrite8(0x6a);
			xWrite8(imm);
		}
		else
		{
			xWrite8(0x68);
			xWrite32(imm);
		}
	}
	__fi void xPUSH(xRegister32or64 from)
	{
		EmitRexImplicitlyWide(from);
		xWrite8(0x50 | (from->Id & 7));
	}

	// pushes the EFLAGS register onto the stack
	__fi void xPUSHFD() { xWrite8(0x9C); }
	// pops the EFLAGS register from the stack
	__fi void xPOPFD() { xWrite8(0x9D); }


	//////////////////////////////////////////////////////////////////////////////////////////
	//

	__fi void xLEAVE() { xWrite8(0xC9); }
	__fi void xRET() { xWrite8(0xC3); }
	__fi void xCBW() { xWrite16(0x9866); }
	__fi void xCWD() { xWrite8(0x98); }
	__fi void xCDQ() { xWrite8(0x99); }
	__fi void xCWDE() { xWrite8(0x98); }
	__fi void xCDQE() { xWrite16(0x9848); }

	__fi void xLAHF() { xWrite8(0x9f); }
	__fi void xSAHF() { xWrite8(0x9e); }

	__fi void xSTC() { xWrite8(0xF9); }
	__fi void xCLC() { xWrite8(0xF8); }

	// NOP 1-byte
	__fi void xNOP() { xWrite8(0x90); }

	__fi void xINT(u8 imm)
	{
		if (imm == 3)
			xWrite8(0xcc);
		else
		{
			xWrite8(0xcd);
			xWrite8(imm);
		}
	}

	__fi void xINTO() { xWrite8(0xce); }

	__emitinline void xBSWAP(const xRegister32or64& to)
	{
		xWrite8(0x0F);
		xWrite8(0xC8 | to->Id);
	}

	alignas(16) static u64 xmm_data[iREGCNT_XMM * 2];

	__emitinline void xStoreReg(const xRegisterSSE& src)
	{
		xMOVDQA(ptr[&xmm_data[src.Id * 2]], src);
	}

	__emitinline void xRestoreReg(const xRegisterSSE& dest)
	{
		xMOVDQA(dest, ptr[&xmm_data[dest.Id * 2]]);
	}

//////////////////////////////////////////////////////////////////////////////////////////
// Helper object to handle ABI frame
// All x86-64 calling conventions ensure/require stack to be 16 bytes aligned
// I couldn't find documentation on when, but compilers would indicate it's before the call: https://gcc.godbolt.org/z/KzTfsz
#define ALIGN_STACK(v) xADD(rsp, v)

	static void stackAlign(int offset, bool moveDown)
	{
		int needed = (16 - (offset % 16)) % 16;
		if (moveDown)
		{
			needed = -needed;
		}
		ALIGN_STACK(needed);
	}

	xScopedStackFrame::xScopedStackFrame(bool base_frame, bool save_base_pointer, int offset)
	{
		m_base_frame = base_frame;
		m_save_base_pointer = save_base_pointer;
		m_offset = offset;

		m_offset += sizeof(void*); // Call stores the return address (4 bytes)

		// Note rbp can surely be optimized in 64 bits
		if (m_base_frame)
		{
			xPUSH(rbp);
			xMOV(rbp, rsp);
			m_offset += sizeof(void*);
		}
		else if (m_save_base_pointer)
		{
			xPUSH(rbp);
			m_offset += sizeof(void*);
		}

		xPUSH(rbx);
		xPUSH(r12);
		xPUSH(r13);
		xPUSH(r14);
		xPUSH(r15);
		m_offset += 40;
#ifdef _WIN32
		xPUSH(rdi);
		xPUSH(rsi);
		xSUB(rsp, 32); // Windows calling convention specifies additional space for the callee to spill registers
		m_offset += 48;
#endif

		stackAlign(m_offset, true);
	}

	xScopedStackFrame::~xScopedStackFrame()
	{
		stackAlign(m_offset, false);

		// Restore the register context
#ifdef _WIN32
		xADD(rsp, 32);
		xPOP(rsi);
		xPOP(rdi);
#endif
		xPOP(r15);
		xPOP(r14);
		xPOP(r13);
		xPOP(r12);
		xPOP(rbx);

		// Destroy the frame
		if (m_base_frame)
		{
			xLEAVE();
		}
		else if (m_save_base_pointer)
		{
			xPOP(rbp);
		}
	}

	xScopedSavedRegisters::xScopedSavedRegisters(std::initializer_list<std::reference_wrapper<const xAddressReg>> regs)
		: regs(regs)
	{
		for (auto reg : regs)
		{
			const xAddressReg& regRef = reg;
			xPUSH(regRef);
		}
		stackAlign(regs.size() * wordsize, true);
	}

	xScopedSavedRegisters::~xScopedSavedRegisters()
	{
		stackAlign(regs.size() * wordsize, false);
		for (auto it = regs.rbegin(); it < regs.rend(); ++it)
		{
			const xAddressReg& regRef = *it;
			xPOP(regRef);
		}
	}

	xAddressVoid xComplexAddress(const xAddressReg& tmpRegister, void* base, const xAddressVoid& offset)
	{
		if ((sptr)base == (s32)(sptr)base)
		{
			return offset + base;
		}
		else
		{
			xLEA(tmpRegister, ptr[base]);
			return offset + tmpRegister;
		}
	}

	void xLoadFarAddr(const xAddressReg& dst, void* addr)
	{
		sptr iaddr = (sptr)addr;
		sptr rip = (sptr)xGetPtr() + 7; // LEA will be 7 bytes
		sptr disp = iaddr - rip;
		if (disp == (s32)disp)
		{
			xLEA(dst, ptr[addr]);
		}
		else
		{
			xMOV64(dst, iaddr);
		}
	}

	void xWriteImm64ToMem(u64* addr, const xAddressReg& tmp, u64 imm)
	{
		xImm64Op(xMOV, ptr64[addr], tmp, imm);
	}

} // End namespace x86Emitter
