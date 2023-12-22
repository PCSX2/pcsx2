// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

//------------------------------------------------------------------
// Micro VU - Pass 2 Functions
//------------------------------------------------------------------

//------------------------------------------------------------------
// Flag Allocators
//------------------------------------------------------------------

__fi static const x32& getFlagReg(uint fInst)
{
	static const x32* const gprFlags[4] = {&gprF0, &gprF1, &gprF2, &gprF3};
	pxAssert(fInst < 4);
	return *gprFlags[fInst];
}

__fi void setBitSFLAG(const x32& reg, const x32& regT, int bitTest, int bitSet)
{
	xTEST(regT, bitTest);
	xForwardJZ8 skip;
	xOR(reg, bitSet);
	skip.SetTarget();
}

__fi void setBitFSEQ(const x32& reg, int bitX)
{
	xTEST(reg, bitX);
	xForwardJump8 skip(Jcc_Zero);
	xOR(reg, bitX);
	skip.SetTarget();
}

__fi void mVUallocSFLAGa(const x32& reg, int fInstance)
{
	xMOV(reg, getFlagReg(fInstance));
}

__fi void mVUallocSFLAGb(const x32& reg, int fInstance)
{
	xMOV(getFlagReg(fInstance), reg);
}

// Normalize Status Flag
__ri void mVUallocSFLAGc(const x32& reg, const x32& regT, int fInstance)
{
	xXOR(reg, reg);
	mVUallocSFLAGa(regT, fInstance);
	setBitSFLAG(reg, regT, 0x0f00, 0x0001); // Z  Bit
	setBitSFLAG(reg, regT, 0xf000, 0x0002); // S  Bit
	setBitSFLAG(reg, regT, 0x000f, 0x0040); // ZS Bit
	setBitSFLAG(reg, regT, 0x00f0, 0x0080); // SS Bit
	xAND(regT, 0xffff0000); // DS/DI/OS/US/D/I/O/U Bits
	xSHR(regT, 14);
	xOR(reg, regT);
}

// Denormalizes Status Flag; destroys tmp1/tmp2
__ri void mVUallocSFLAGd(u32* memAddr, const x32& reg = eax, const x32& tmp1 = ecx, const x32& tmp2 = edx)
{
	xMOV(tmp2, ptr32[memAddr]);
	xMOV(reg, tmp2);
	xSHR(reg, 3);
	xAND(reg, 0x18);

	xMOV(tmp1, tmp2);
	xSHL(tmp1, 11);
	xAND(tmp1, 0x1800);
	xOR(reg, tmp1);

	xSHL(tmp2, 14);
	xAND(tmp2, 0x3cf0000);
	xOR(reg, tmp2);
}

__fi void mVUallocMFLAGa(mV, const x32& reg, int fInstance)
{
	xMOVZX(reg, ptr16[&mVU.macFlag[fInstance]]);
}

__fi void mVUallocMFLAGb(mV, const x32& reg, int fInstance)
{
	//xAND(reg, 0xffff);
	if (fInstance < 4) xMOV(ptr32[&mVU.macFlag[fInstance]], reg);         // microVU
	else               xMOV(ptr32[&mVU.regs().VI[REG_MAC_FLAG].UL], reg); // macroVU
}

__fi void mVUallocCFLAGa(mV, const x32& reg, int fInstance)
{
	if (fInstance < 4) xMOV(reg, ptr32[&mVU.clipFlag[fInstance]]);         // microVU
	else               xMOV(reg, ptr32[&mVU.regs().VI[REG_CLIP_FLAG].UL]); // macroVU
}

__fi void mVUallocCFLAGb(mV, const x32& reg, int fInstance)
{
	if (fInstance < 4) xMOV(ptr32[&mVU.clipFlag[fInstance]], reg);         // microVU
	else               xMOV(ptr32[&mVU.regs().VI[REG_CLIP_FLAG].UL], reg); // macroVU
}

//------------------------------------------------------------------
// VI Reg Allocators
//------------------------------------------------------------------

void microRegAlloc::writeVIBackup(const xRegisterInt& reg)
{
	microVU& mVU = index ? microVU1 : microVU0;
	xMOV(ptr32[&mVU.VIbackup], xRegister32(reg));
}

//------------------------------------------------------------------
// P/Q Reg Allocators
//------------------------------------------------------------------

__fi void getPreg(mV, const xmm& reg)
{
	mVUunpack_xyzw(reg, xmmPQ, (2 + mVUinfo.readP));
}

__fi void getQreg(const xmm& reg, int qInstance)
{
	mVUunpack_xyzw(reg, xmmPQ, qInstance);
}

__ri void writeQreg(const xmm& reg, int qInstance)
{
	if (qInstance)
		xINSERTPS(xmmPQ, reg, _MM_MK_INSERTPS_NDX(0, 1, 0));
	else
		xMOVSS(xmmPQ, reg);
}
