// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// Micro VU - NEON Reg Loading/Saving/Shuffling/Unpacking/Merging
//------------------------------------------------------------------

// Broadcast a single component to all 4 lanes
// xyzw: 0=X, 1=Y, 2=Z, 3=W
void mVUunpack_xyzw(const a64::VRegister& dstreg, const a64::VRegister& srcreg, int xyzw)
{
	// DUP Vd.4S, Vn.S[lane] — broadcast lane to all 4 slots
	armAsm->Dup(dstreg.V4S(), srcreg.V4S(), xyzw);
}

// Load VF register components from memory.
// xyzw bitmask: 8=X, 4=Y, 2=Z, 1=W. Single-component loads zero-extend.
// For full load (xyzw=0xF or non-single), loads all 128 bits.
void mVUloadReg(const a64::VRegister& reg, const void* ptr, int xyzw)
{
	switch (xyzw)
	{
		case 8: // X only — load 32-bit scalar, upper lanes zeroed
			armLoadPtr(a64::VRegister(reg.GetCode(), 32), ptr);
			break;
		case 4: // Y only
			armLoadPtr(a64::VRegister(reg.GetCode(), 32), (const u8*)ptr + 4);
			break;
		case 2: // Z only
			armLoadPtr(a64::VRegister(reg.GetCode(), 32), (const u8*)ptr + 8);
			break;
		case 1: // W only
			armLoadPtr(a64::VRegister(reg.GetCode(), 32), (const u8*)ptr + 12);
			break;
		default: // Full 128-bit load
			armLoadPtr(reg, ptr);
			break;
	}
}

// base+offset overload — for call sites where the address is reachable as
// `[base, #off]` (e.g. base=gprVUState, off within VURegs). Emits a single
// Ldr per partial lane, no scratch+materialize round-trip. The full-load
// case uses imm12 scaled by 16 (Q-reg) so reaches up to 64KB from base.
__fi void mVUloadReg(const a64::VRegister& reg, const a64::Register& base, int64_t off, int xyzw)
{
	switch (xyzw)
	{
		case 8: armAsm->Ldr(a64::VRegister(reg.GetCode(), 32), a64::MemOperand(base, off));      break;
		case 4: armAsm->Ldr(a64::VRegister(reg.GetCode(), 32), a64::MemOperand(base, off + 4));  break;
		case 2: armAsm->Ldr(a64::VRegister(reg.GetCode(), 32), a64::MemOperand(base, off + 8));  break;
		case 1: armAsm->Ldr(a64::VRegister(reg.GetCode(), 32), a64::MemOperand(base, off + 12)); break;
		default: armAsm->Ldr(reg, a64::MemOperand(base, off)); break;
	}
}

// Load from a runtime address (held in a 64-bit base register) into reg,
// following the regalloc lane convention enforced by writeBackNeon, which
// calls mVUsaveReg(modXYZW=true):
//   - Single-lane partial (X/Y/Z/W): 32-bit load that zeroes upper lanes
//     places the value in lane 0. mVUsaveReg's Y/Z/W cases with modXYZW=true
//     write lane 0 back to the natural byte slot. (X is lane 0 either way.)
//   - Multi-lane partial / full mask: full 16-byte load into natural lanes.
//     Multi-lane mVUsaveReg cases store from natural lanes (modXYZW ignored).
//
// LQ/LQD/LQI must use this — using `Ldr Q, [base]` plus
// mVUmergeRegs(natural-lane) for partial writes leaves single-lane Y/Z/W
// values in their natural lane while the writeback reads lane 0, silently
// clobbering VU memory with whatever happened to be in the recycled
// Q register's lane 0.
__fi void mVUloadMem(const a64::VRegister& reg, const a64::Register& base, int xyzw)
{
	int offset;
	switch (xyzw)
	{
		case 0x8: offset = 0;  break; // X — naturally lane 0
		case 0x4: offset = 4;  break; // Y — into lane 0
		case 0x2: offset = 8;  break; // Z — into lane 0
		case 0x1: offset = 12; break; // W — into lane 0
		default:
			armAsm->Ldr(reg, a64::MemOperand(base));
			return;
	}
	armAsm->Ldr(a64::VRegister(reg.GetCode(), 32), a64::MemOperand(base, offset));
}

// Store VF register components to memory with xyzw mask.
// Handles all 15 non-zero xyzw combinations.
//
// modXYZW semantics (match x86):
//   - modXYZW == true  : value is in lane 0 of `reg` (from SS-path shuffle
//                        in allocReg's clone-write). Write lane 0 to the
//                        target slot. writeBackNeon passes this path.
//   - modXYZW == false : value is at the natural lane of `reg`. Write the
//                        natural lane to the target slot. Used by VIF path
//                        which loads data directly in natural positions.
//
// IMPORTANT: ARM64 ST1-single-lane has NO immediate-offset addressing form —
// VIXL's `St1(V, lane, MemOperand(base, imm))` silently drops `imm` in
// release builds (and asserts in debug). Partial stores here therefore
// advance `x8` with Add before each ST1 at a non-zero slot. `x8` is a
// scratch register (not RSCRATCHADDR), so clobbering it is fine.
// Internal helper — assumes x8 already holds the destination address.
// Both mVUsaveReg overloads route through this to share the case body.
static void mVUsaveRegAtX8(const a64::VRegister& reg, int xyzw, bool modXYZW);

void mVUsaveReg(const a64::VRegister& reg, const void* ptr, int xyzw, bool modXYZW)
{
	armMoveAddressToReg(a64::x8, ptr);
	mVUsaveRegAtX8(reg, xyzw, modXYZW);
}

// base+offset overload — for call sites where the address is reachable as
// `[base, #off]` (e.g. base=gprVUState, off within VURegs). The full-store
// (xyzw=0xF) case bypasses x8 entirely and uses [base, #off] directly,
// saving an extra Add. Partial stores still need x8 because ST1-single-lane
// has no immediate-offset form on aarch64 — see notes on the absolute-ptr
// overload above.
void mVUsaveReg(const a64::VRegister& reg, const a64::Register& base, int64_t off, int xyzw, bool modXYZW)
{
	if (xyzw == 0xF)
	{
		armAsm->Str(reg, a64::MemOperand(base, off));
		return;
	}
	if (off != 0)
		armAsm->Add(a64::x8, base, off);
	else
		armAsm->Mov(a64::x8, base);
	mVUsaveRegAtX8(reg, xyzw, modXYZW);
}

static void mVUsaveRegAtX8(const a64::VRegister& reg, int xyzw, bool modXYZW)
{
	switch (xyzw)
	{
		case 0xF: // XYZW — full store
			armAsm->Str(reg, a64::MemOperand(a64::x8));
			break;
		case 0x8: // X — always lane 0 (X is lane 0 natively)
			armAsm->St1(reg.V4S(), 0, a64::MemOperand(a64::x8));
			break;
		case 0x4: // Y
			armAsm->Add(a64::x8, a64::x8, 4);
			armAsm->St1(reg.V4S(), modXYZW ? 0 : 1, a64::MemOperand(a64::x8));
			break;
		case 0x2: // Z
			armAsm->Add(a64::x8, a64::x8, 8);
			armAsm->St1(reg.V4S(), modXYZW ? 0 : 2, a64::MemOperand(a64::x8));
			break;
		case 0x1: // W
			armAsm->Add(a64::x8, a64::x8, 12);
			armAsm->St1(reg.V4S(), modXYZW ? 0 : 3, a64::MemOperand(a64::x8));
			break;
		case 0xC: // XY
			armAsm->Str(a64::DRegister(reg.GetCode()), a64::MemOperand(a64::x8));
			break;
		case 0x3: // ZW
			armAsm->Add(a64::x8, a64::x8, 8);
			armAsm->St1(reg.V2D(), 1, a64::MemOperand(a64::x8));
			break;
		case 0xE: // XYZ — D-reg store (XY) post-indexes 8 bytes; Z lane store at [x8].
			armAsm->Str(a64::DRegister(reg.GetCode()), a64::MemOperand(a64::x8, 8, a64::PostIndex));
			armAsm->St1(reg.V4S(), 2, a64::MemOperand(a64::x8));
			break;
		case 0x7: // YZW — Y lane post-indexes the V4S element size (4); ZW V2D at [x8].
			armAsm->Add(a64::x8, a64::x8, 4);
			armAsm->St1(reg.V4S(), 1, a64::MemOperand(a64::x8, 4, a64::PostIndex));
			armAsm->St1(reg.V2D(), 1, a64::MemOperand(a64::x8));
			break;
		case 0xD: // XYW — D-reg store (XY) post-indexes 12 bytes; W lane store at [x8].
			armAsm->Str(a64::DRegister(reg.GetCode()), a64::MemOperand(a64::x8, 12, a64::PostIndex));
			armAsm->St1(reg.V4S(), 3, a64::MemOperand(a64::x8));
			break;
		case 0xB: // XZW
			armAsm->St1(reg.V4S(), 0, a64::MemOperand(a64::x8));
			armAsm->Add(a64::x8, a64::x8, 8);
			armAsm->St1(reg.V2D(), 1, a64::MemOperand(a64::x8));
			break;
		case 0xA: // XZ
			armAsm->St1(reg.V4S(), 0, a64::MemOperand(a64::x8));
			armAsm->Add(a64::x8, a64::x8, 8);
			armAsm->St1(reg.V4S(), 2, a64::MemOperand(a64::x8));
			break;
		case 0x9: // XW
			armAsm->St1(reg.V4S(), 0, a64::MemOperand(a64::x8));
			armAsm->Add(a64::x8, a64::x8, 12);
			armAsm->St1(reg.V4S(), 3, a64::MemOperand(a64::x8));
			break;
		case 0x6: // YZ — Y lane post-indexes the V4S element size (4); Z lane at [x8].
			armAsm->Add(a64::x8, a64::x8, 4);
			armAsm->St1(reg.V4S(), 1, a64::MemOperand(a64::x8, 4, a64::PostIndex));
			armAsm->St1(reg.V4S(), 2, a64::MemOperand(a64::x8));
			break;
		case 0x5: // YW
			armAsm->Add(a64::x8, a64::x8, 4);
			armAsm->St1(reg.V4S(), 1, a64::MemOperand(a64::x8));
			armAsm->Add(a64::x8, a64::x8, 8); // x8 now points at +12 (W)
			armAsm->St1(reg.V4S(), 3, a64::MemOperand(a64::x8));
			break;
		default:
			break;
	}
}

// Merge selected components from src into dest.
// xyzw bitmask: 8=X, 4=Y, 2=Z, 1=W.
// Single shared definition for microVU AND the VIF unpack dynarec
// (declared in microVU-arm64.h and Vif_UnpackNEON.h), mirroring x86's
// one helper in microVU_Misc.inl.
//
// modXYZW semantics (match x86): for single-lane masks the value is in
// lane 0 of src (SS-path shuffle convention); multi-lane masks always
// merge natural lanes.
void mVUmergeRegs(const a64::VRegister& dest, const a64::VRegister& src, int xyzw, bool modXYZW)
{
	xyzw &= 0xf;
	if (dest.IsNone() || src.IsNone() || (dest.GetCode() == src.GetCode()) || xyzw == 0)
		return;

	if (xyzw == 0xF)
	{
		armAsm->Mov(dest.V16B(), src.V16B());
		return;
	}

	if (modXYZW)
	{
		// Source has the value in lane 0 — insert into target lane
		switch (xyzw)
		{
			case 0x8: armAsm->Ins(dest.V4S(), 0, src.V4S(), 0); return; // X
			case 0x4: armAsm->Ins(dest.V4S(), 1, src.V4S(), 0); return; // Y
			case 0x2: armAsm->Ins(dest.V4S(), 2, src.V4S(), 0); return; // Z
			case 0x1: armAsm->Ins(dest.V4S(), 3, src.V4S(), 0); return; // W
			default: break; // Multi-lane: natural-lane merge below
		}
	}

	// Natural-lane merge. Reverse the mask into lane order (bit i = lane i)
	// so an X/Y or Z/W pair can move as one 64-bit lane insert.
	int lanes = ((xyzw & 1) << 3) | ((xyzw & 2) << 1) | ((xyzw & 4) >> 1) | ((xyzw & 8) >> 3);

	if ((lanes & 3) == 3) // XY
	{
		armAsm->Mov(dest.V2D(), 0, src.V2D(), 0);
		lanes &= ~3;
	}
	else if ((lanes & 12) == 12) // ZW
	{
		armAsm->Mov(dest.V2D(), 1, src.V2D(), 1);
		lanes &= ~12;
	}

	for (u32 i = 0; i < 4; i++)
	{
		if (lanes & (1u << i))
			armAsm->Ins(dest.V4S(), i, src.V4S(), i);
	}
}

//------------------------------------------------------------------
// Micro VU - Backup/Restore Regs for C Calls
//------------------------------------------------------------------

// Flush all register allocations and save PQ before a C function call.
// The dispatcher already saved callee-saved regs, so once the regalloc is
// flushed the only state that must survive the call is PQ. toMemory/onlyNeeded
// are accepted for signature parity with x86 mVUbackupRegs and ignored here.
__fi void mVUbackupRegs(microVU& mVU, bool toMemory = false, bool onlyNeeded = false)
{
	mVU.regAlloc->flushAll();
	armAsm->Str(qmmPQ, mVUneonBackupMem(qmmPQ.GetCode()));
}

__fi void mVUrestoreRegs(microVU& mVU, bool fromMemory = false, bool onlyNeeded = false)
{
	armAsm->Ldr(qmmPQ, mVUneonBackupMem(qmmPQ.GetCode()));
}

//------------------------------------------------------------------
// Micro VU - VU Memory Address Translation
//------------------------------------------------------------------

static inline u32 branchAddr(const mV)
{
	pxAssumeMsg(islowerOP, "MicroVU: Expected Lower OP code for valid branch addr.");
	return ((((iPC + 2) + (_Imm11_ * 2)) & mVU.progMemMask) * 4);
}

static void mVUwaitMTVU()
{
	if (IsDevBuild)
		DevCon.WriteLn("microVU0: Waiting on VU1 thread to access VU1 regs!");
	vu1Thread.WaitVU();
}

static void mVUTBit()
{
	u32 old = vu1Thread.mtvuInterrupts.fetch_or(VU_Thread::InterruptFlagVUTBit, std::memory_order_release);
	if (old & VU_Thread::InterruptFlagVUTBit)
		DevCon.Warning("Old TBit not registered");
}

static void mVUEBit()
{
	vu1Thread.mtvuInterrupts.fetch_or(VU_Thread::InterruptFlagVUEBit, std::memory_order_release);
}

// Transform VI register address to valid VU0/VU1 memory pointer offset.
// gprReg holds the VI value (address in VU quadwords).
// On exit, gprReg holds byte offset into VU memory.
__fi void mVUaddrFix(mV, const a64::Register& gprReg)
{
	if (isVU1)
	{
		// VU1: mask to 0x3FF quadwords, shift left 4 (x16 bytes)
		armAsm->And(gprReg.W(), gprReg.W(), 0x3ff);
		armAsm->Lsl(gprReg.W(), gprReg.W(), 4);
	}
	else
	{
		// VU0: if addr & 0x400, accessing VU1 register space
		a64::Label notVU1Access, done;

		armAsm->Tst(gprReg.W(), 0x400);
		armAsm->B(&notVU1Access, a64::eq);

		// Accessing VU1 regs from VU0
		if (THREAD_VU1)
		{
			// COP2 macro mode emits this inline into an EE block where the
			// caller-saved EE pins (x12/x13) are live across the call; micro
			// mode runs behind a C boundary whose call site reloads instead.
			// Flush-before is a lazy-dirty no-op in write-through mode.
			if (mVU.cop2)
				armEmitEEClobberedPinFlushForCOP2();
			// Need to wait for VU1 thread
			armEmitCall((void*)mVU.waitMTVU);
			if (mVU.cop2)
				armEmitEEClobberedPinReloadForCOP2();
		}
		armAsm->And(gprReg.W(), gprReg.W(), 0x3f);
		// Add offset in u128 units (x86 mVUaddrFix uses pointer arithmetic so
		// the delta is pre-scaled by sizeof(u128) = 16). The trailing Lsl by
		// 4 below converts both the VI index and the offset to bytes in one
		// shot. Casting to s64 first would produce a raw byte delta whose
		// low bits get truncated by the (correctly 64-bit) shift.
		const s64 vu1Offset = (u128*)VU1.VF - (u128*)VU0.Mem;
		armAsm->Add(gprReg.X(), gprReg.X(), vu1Offset);
		armAsm->B(&done);

		armAsm->Bind(&notVU1Access);
		armAsm->And(gprReg.W(), gprReg.W(), 0xff);

		armAsm->Bind(&done);
		// 64-bit Lsl: the VU0->VU1 path stashed a relative offset in the
		// upper bits via Add(.X()) above; a 32-bit shift would zero them.
		armAsm->Lsl(gprReg.X(), gprReg.X(), 4);
	}
}

// Constant-address fold for loadstores whose base VI is vi00 (always 0). With a
// constant base the whole address is known at compile time, so the runtime
// moveVIToGPR + imm-add + mVUaddrFix (mask/shift) + base-add chain collapses to
// a single Mem-pointer load plus (at most) one immediate add. On a return of
// true gprOutQ holds &VU.Mem[const], ready for the load/store; on false the
// caller emits the normal runtime path. Mirrors x86 mVUoptimizeConstantAddr
// (microVU_Misc.inl). The VU0 cross-VU-register window (addr & 0x400) and the
// IbitHack runtime-reconstruct path are deliberately left to mVUaddrFix.
// Ported 2026-06-23 from upstream 6018936dc (postdates the leak/4248; correct
// and ABI-neutral, so adopted per the "newer-upstream-pattern" rule).
__fi bool mVUoptimizeConstantAddr(mV, u32 srcreg, s32 offset, s32 offsetSS_, const a64::Register& gprOutQ)
{
	if (srcreg != 0 || EmuConfig.Gamefixes.IbitHack)
		return false;

	s32 byteAddr;
	if (isVU1)
	{
		byteAddr = ((offset & 0x3ff) << 4) + offsetSS_;
	}
	else
	{
		if (offset & 0x400)
			return false; // cross-VU-register access — runtime path handles it
		byteAddr = ((offset & 0xff) << 4) + offsetSS_;
	}

	armAsm->Ldr(gprOutQ, mVUstateMem(offsetof(VURegs, Mem)));
	if (byteAddr != 0)
		armAsm->Add(gprOutQ, gprOutQ, byteAddr);
	return true;
}
