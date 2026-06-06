// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 microVU — misc emit helpers (Phase 7, task 7.5). VIXL port of the
// emit-coupled tail of pcsx2/x86/microVU_Misc.inl.
//
// Already ported elsewhere:
//   * the arch-neutral reg shufflers (mVUunpack_xyzw/mVUloadReg/mVUsaveReg/
//     mVUmergeRegs) — aVU_IR.h (task 7.2b / 7.4 part 1b);
//   * the branch/T-Bit/E-Bit/waitMTVU C helpers — aVU.cpp.
//
// Deferred:
//   * mVUoptimizeConstantAddr — its return-type contract (how a constant host
//     address is handed to a load/store op) is defined by its only consumer, the
//     Lower load/store handlers in task 7.5b;
//   * the custom SSE arithmetic helpers (MIN_MAX_PS/ADD_SS/SSE_* — task 7.5a FMAC).

// Computes the destination PC (byte address) of a relative VU branch from the
// current lower-op PC + the signed 11-bit immediate. x86: microVU_Misc.inl
// branchAddr. Pure (no emit) — used by the branch op handlers (B/BAL/IBxx) and
// the branch drivers in aVU_Branch.inl.
static inline u32 branchAddr(const mV)
{
	pxAssumeMsg(islowerOP, "MicroVU: Expected Lower OP code for valid branch addr.");
	return ((((iPC + 2) + (_Imm11_ * 2)) & mVU.progMemMask) * 4);
}

//------------------------------------------------------------------
// Small absolute-address memory-access helpers (shared by Tables/Flags/Branch)
//------------------------------------------------------------------
// x86 folded an absolute &global into a ptr32/ptr128 memory operand; ARM64 must
// first materialize the address. armMoveAddressToReg clobbers RSCRATCH (x16/x17),
// so the value regs passed in must never be x16/x17 (callers use w9/w10 or the
// flag GPRs / NEON scratch).

static inline void mvuStr32(const void* addr, const a64::Register& wreg)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Str(wreg.W(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuLdr32(const a64::Register& wreg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(wreg.W(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuStrImm32(const void* addr, u32 imm, const a64::Register& tmp)
{
	armAsm->Mov(tmp.W(), imm);
	mvuStr32(addr, tmp);
}

static inline void mvuStrSS(const void* addr, const a64::VRegister& vreg)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Str(vreg.S(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuLdrSS(const a64::VRegister& vreg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(vreg.S(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuLdrQ(const a64::VRegister& vreg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(vreg.Q(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuStrQ(const void* addr, const a64::VRegister& vreg)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Str(vreg.Q(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuMemAndImm32(const void* addr, u32 imm, const a64::Register& tmp)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(tmp.W(), a64::MemOperand(RSCRATCHADDR));
	armAsm->And(tmp.W(), tmp.W(), imm);
	armAsm->Str(tmp.W(), a64::MemOperand(RSCRATCHADDR));
}

// Transforms the VU address (a quadword index) in gprReg into a byte offset into
// VU memory, applying the VU0/VU1 wrap and the VU0->VU1 register-window remap.
// x86: mVUaddrFix. Modifies gprReg in place (and tmpReg on the far-offset path).
// gprReg/tmpReg are 64-bit (X) registers; the masking steps use the 32-bit view
// (W-writes zero bits [63:32], matching the x86 32-bit AND).
__fi void mVUaddrFix(mV, const a64::Register& gprReg, const a64::Register& tmpReg)
{
	if (isVU1)
	{
		armAsm->And(gprReg.W(), gprReg.W(), 0x3ff); // wrap around (1024 quadwords)
		armAsm->Lsl(gprReg.W(), gprReg.W(), 4);     // * 16 -> byte offset
	}
	else
	{
		a64::Label jmpA, jmpB;
		armAsm->Tst(gprReg.W(), 0x400);
		armAsm->B(&jmpA, a64::ne); // if (addr & 0x400): reads VU1's VF/VI regs
			armAsm->And(gprReg.W(), gprReg.W(), 0xff); // else wrap (256 quadwords)
			armAsm->B(&jmpB);
		armAsm->Bind(&jmpA);
			if (THREAD_VU1)
				armEmitCall(reinterpret_cast<const void*>(mVU.waitMTVU));
			armAsm->And(gprReg.W(), gprReg.W(), 0x3f); // ToDo: VU0 may override VU1's VF0/VI0
			const sptr offset = (u128*)VU1.VF - (u128*)VU0.Mem;
			if (offset == static_cast<s32>(offset))
			{
				armAsm->Add(gprReg, gprReg, static_cast<s32>(offset));
			}
			else
			{
				armAsm->Mov(tmpReg, offset);
				armAsm->Add(gprReg, gprReg, tmpReg);
			}
		armAsm->Bind(&jmpB);
		armAsm->Lsl(gprReg, gprReg, 4); // * 16 -> byte offset (64-bit)
	}
}
