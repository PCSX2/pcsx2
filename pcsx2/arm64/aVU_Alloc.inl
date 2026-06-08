// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 microVU recompiler — Pass-2 flag allocators (Phase 7, task 7.4/7.5).
//
// ARM64 counterpart to the flag-allocator half of pcsx2/x86/microVU_Alloc.inl:
// getFlagReg + the Status/Mac/Clip flag normalize/denormalize emit helpers.
// Translations vs the x86 original:
//   * x86 GPRs (x32 = eax/ecx/r12d..) -> ARM64 w-regs (gprT1/gprT2/gprF0..3,
//     the name macros in aVU_IR.h).
//   * x86 `xTEST + xForwardJZ8 + xOR` (set a bit conditionally) -> VIXL
//     `Tst + B(eq) + Orr` with a local label.
//   * x86 absolute `ptr16/ptr32[&...]` loads/stores -> armMoveAddressToReg +
//     VIXL Ldrh/Ldr/Str. The mac/clip flags live in the mVU.macFlag/clipFlag
//     host arrays (addressed absolutely, exactly like x86).
//   * VIXL's MacroAssembler materialises any non-encodable logical immediate
//     into its internal scratch (x16 = RXVIXLSCRATCH), so And/Orr/Tst take the
//     x86 masks (0x0f00/0xffff0000/0x3cf0000/..) verbatim.
//
// writeVIBackup (the VI allocator member) is already defined inline in aVU_IR.h.
//
// The P/Q register allocators (getPreg/getQreg/writeQreg) live at the bottom of
// this file. They use mVUunpack_xyzw — the NEON lane-broadcast from aVU_IR.h
// (x86 microVU_Misc.inl) — and the PQ latency NEON reg mVU_xmmPQ (v24). They are
// used only by the Upper/Lower opcode handlers (task 7.5).

//------------------------------------------------------------------
// Flag Allocators
//------------------------------------------------------------------

// Returns the live w-register holding Status-flag instance fInst (x86: getFlagReg).
__fi static const a64::Register& getFlagReg(uint fInst)
{
	static const a64::Register* const gprFlags[4] = {&gprF0, &gprF1, &gprF2, &gprF3};
	pxAssert(fInst < 4);
	return *gprFlags[fInst];
}

// if (regT & bitTest) reg |= bitSet;   (x86: xTEST + xForwardJZ8 + xOR)
__fi void setBitSFLAG(const a64::Register& reg, const a64::Register& regT, int bitTest, int bitSet)
{
	armAsm->Tst(regT, bitTest);
	a64::Label skip;
	armAsm->B(&skip, a64::eq);
	armAsm->Orr(reg, reg, bitSet);
	armAsm->Bind(&skip);
}

// if (reg & bitX) reg |= bitX;   (x86: setBitFSEQ — used by FSEQ in Lower)
__fi void setBitFSEQ(const a64::Register& reg, int bitX)
{
	armAsm->Tst(reg, bitX);
	a64::Label skip;
	armAsm->B(&skip, a64::eq);
	armAsm->Orr(reg, reg, bitX);
	armAsm->Bind(&skip);
}

__fi void mVUallocSFLAGa(const a64::Register& reg, int fInstance)
{
	armAsm->Mov(reg, getFlagReg(fInstance));
}

__fi void mVUallocSFLAGb(const a64::Register& reg, int fInstance)
{
	armAsm->Mov(getFlagReg(fInstance), reg);
}

// Normalize Status Flag
__ri void mVUallocSFLAGc(const a64::Register& reg, const a64::Register& regT, int fInstance)
{
	armAsm->Mov(reg, a64::wzr); // xXOR(reg, reg)
	mVUallocSFLAGa(regT, fInstance);
	setBitSFLAG(reg, regT, 0x0f00, 0x0001); // Z  Bit
	setBitSFLAG(reg, regT, 0xf000, 0x0002); // S  Bit
	setBitSFLAG(reg, regT, 0x000f, 0x0040); // ZS Bit
	setBitSFLAG(reg, regT, 0x00f0, 0x0080); // SS Bit
	armAsm->And(regT, regT, 0xffff0000);    // DS/DI/OS/US/D/I/O/U Bits
	armAsm->Lsr(regT, regT, 14);
	armAsm->Orr(reg, reg, regT);
}

// Denormalizes Status Flag; destroys reg/tmp1/tmp2 (x86: mVUallocSFLAGd, whose
// eax/ecx/edx defaults are the macro-mode path we drop — callers pass regs).
__ri void mVUallocSFLAGd(u32* memAddr, const a64::Register& reg, const a64::Register& tmp1, const a64::Register& tmp2)
{
	armMoveAddressToReg(RSCRATCHADDR, memAddr);
	armAsm->Ldr(tmp2, a64::MemOperand(RSCRATCHADDR));
	armAsm->Mov(reg, tmp2);
	armAsm->Lsr(reg, reg, 3);
	armAsm->And(reg, reg, 0x18);

	armAsm->Mov(tmp1, tmp2);
	armAsm->Lsl(tmp1, tmp1, 11);
	armAsm->And(tmp1, tmp1, 0x1800);
	armAsm->Orr(reg, reg, tmp1);

	armAsm->Lsl(tmp2, tmp2, 14);
	armAsm->And(tmp2, tmp2, 0x3cf0000);
	armAsm->Orr(reg, reg, tmp2);
}

__fi void mVUallocMFLAGa(mV, const a64::Register& reg, int fInstance)
{
	armMoveAddressToReg(RSCRATCHADDR, &mVU.macFlag[fInstance]);
	armAsm->Ldrh(reg, a64::MemOperand(RSCRATCHADDR)); // zero-extending 16-bit load (xMOVZX)
}

__fi void mVUallocMFLAGb(mV, const a64::Register& reg, int fInstance)
{
	if (fInstance < 4)
		armMoveAddressToReg(RSCRATCHADDR, &mVU.macFlag[fInstance]);          // microVU
	else
		armMoveAddressToReg(RSCRATCHADDR, &mVU.regs().VI[REG_MAC_FLAG].UL);  // macroVU
	armAsm->Str(reg, a64::MemOperand(RSCRATCHADDR));
}

__fi void mVUallocCFLAGa(mV, const a64::Register& reg, int fInstance)
{
	if (fInstance < 4)
		armMoveAddressToReg(RSCRATCHADDR, &mVU.clipFlag[fInstance]);         // microVU
	else
		armMoveAddressToReg(RSCRATCHADDR, &mVU.regs().VI[REG_CLIP_FLAG].UL); // macroVU
	armAsm->Ldr(reg, a64::MemOperand(RSCRATCHADDR));
}

__fi void mVUallocCFLAGb(mV, const a64::Register& reg, int fInstance)
{
	if (fInstance < 4)
		armMoveAddressToReg(RSCRATCHADDR, &mVU.clipFlag[fInstance]);         // microVU
	else
		armMoveAddressToReg(RSCRATCHADDR, &mVU.regs().VI[REG_CLIP_FLAG].UL); // macroVU
	armAsm->Str(reg, a64::MemOperand(RSCRATCHADDR));
}

//------------------------------------------------------------------
// P/Q Reg Allocators
//------------------------------------------------------------------
// The PQ-latency pair lives in one NEON reg (mVU_xmmPQ = v24, x86: xmmPQ) with
// the x86 lane layout: Q in lanes 0/1 (instance 0/1), P in lanes 2/3 (instance
// 0/1). Reads broadcast the live lane out via mVUunpack_xyzw; writeQreg inserts
// lane0 of the source into the Q slot for the requested instance.

__fi void getPreg(mV, const a64::VRegister& reg)
{
	mVUunpack_xyzw(reg, mVU_xmmPQ, (2 + mVUinfo.readP));
}

__fi void getQreg(const a64::VRegister& reg, int qInstance)
{
	mVUunpack_xyzw(reg, mVU_xmmPQ, qInstance);
}

__ri void writeQreg(const a64::VRegister& reg, int qInstance)
{
	// x86: qInstance ? xINSERTPS(xmmPQ, reg, NDX(0,1,0)) : xMOVSS(xmmPQ, reg).
	// Both copy the source's lane0 into Q lane qInstance, leaving the rest of PQ.
	armAsm->Ins(mVU_xmmPQ.V4S(), qInstance, reg.V4S(), 0);
}
