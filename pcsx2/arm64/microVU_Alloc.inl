// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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
	a64::Label skip;
	armAsm->Tst(regT.W(), bitTest);
	armAsm->B(&skip, a64::eq);
	armAsm->Orr(reg.W(), reg.W(), bitSet);
	armAsm->Bind(&skip);
}

__fi void setBitFSEQ(const x32& reg, int bitX)
{
	a64::Label skip;
	armAsm->Tst(reg.W(), bitX);
	armAsm->B(&skip, a64::eq);
	armAsm->Orr(reg.W(), reg.W(), bitX);
	armAsm->Bind(&skip);
}

__fi void mVUallocSFLAGa(const x32& reg, int fInstance)
{
	if (reg != getFlagReg(fInstance))
		armAsm->Mov(reg.W(), getFlagReg(fInstance).W());
}

__fi void mVUallocSFLAGb(const x32& reg, int fInstance)
{
	if (reg != getFlagReg(fInstance))
		armAsm->Mov(getFlagReg(fInstance).W(), reg.W());
}

// Normalize Status Flag
__ri void mVUallocSFLAGc(const x32& reg, const x32& regT, int fInstance)
{
	armAsm->Mov(reg.W(), a64::wzr);
	mVUallocSFLAGa(regT, fInstance);
	setBitSFLAG(reg, regT, 0x0f00, 0x0001); // Z  Bit
	setBitSFLAG(reg, regT, 0xf000, 0x0002); // S  Bit
	setBitSFLAG(reg, regT, 0x000f, 0x0040); // ZS Bit
	setBitSFLAG(reg, regT, 0x00f0, 0x0080); // SS Bit
	armAsm->And(regT.W(), regT.W(), 0xffff0000); // DS/DI/OS/US/D/I/O/U Bits
	armAsm->Lsr(regT.W(), regT.W(), 14);
	armAsm->Orr(reg.W(), reg.W(), regT.W());
}

__fi void mVUallocMFLAGa(mV, const x32& reg, int fInstance)
{
	armAsm->Ldrh(reg.W(), mVUmem(&mVU.macFlag[fInstance]));
}

__fi void mVUallocMFLAGb(mV, const x32& reg, int fInstance)
{
	//xAND(reg, 0xffff);
	if (fInstance < 4) armAsm->Str(reg.W(), mVUmem(&mVU.macFlag[fInstance]));         // microVU
	else               armAsm->Str(reg.W(), mVUmem(&mVU.regs().VI[REG_MAC_FLAG].UL)); // macroVU
}

__fi void mVUallocCFLAGa(mV, const x32& reg, int fInstance)
{
	if (fInstance < 4) armAsm->Ldr(reg.W(), mVUmem(&mVU.clipFlag[fInstance]));         // microVU
	else               armAsm->Ldr(reg.W(), mVUmem(&mVU.regs().VI[REG_CLIP_FLAG].UL)); // macroVU
}

__fi void mVUallocCFLAGb(mV, const x32& reg, int fInstance)
{
	if (fInstance < 4) armAsm->Str(reg.W(), mVUmem(&mVU.clipFlag[fInstance]));         // microVU
	else               armAsm->Str(reg.W(), mVUmem(&mVU.regs().VI[REG_CLIP_FLAG].UL)); // macroVU
}

//------------------------------------------------------------------
// VI Reg Allocators
//------------------------------------------------------------------

void microRegAlloc::writeVIBackup(const x32& reg)
{
	microVU& mVU = index ? microVU1 : microVU0;
	armAsm->Str(reg.W(), mVUmem(&mVU.VIbackup));
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
	mVUinsLane(xmmPQ, qInstance ? 1 : 0, reg, 0);
}
