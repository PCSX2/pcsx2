// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#include "newVif_UnpackNEON.h"
#include "common/Perf.h"

namespace a64 = vixl::aarch64;

// =====================================================================================================
//  VifUnpackSSE_Base Section
// =====================================================================================================
VifUnpackNEON_Base::VifUnpackNEON_Base()
	: usn(false)
	, doMask(false)
	, UnpkLoopIteration(0)
	, UnpkNoOfIterations(0)
	, IsAligned(0)
	, dstIndirect(a64::MemOperand(RXARG1))
	, srcIndirect(a64::MemOperand(RXARG2))
	, workReg(a64::q1)
	, destReg(a64::q0)
	, workGprW(a64::w4)
{
}

void VifUnpackNEON_Base::xMovDest() const
{
	if (!IsWriteProtectedOp())
	{
		if (IsUnmaskedOp())
			armAsm->Str(destReg, dstIndirect);
		else
			doMaskWrite(destReg);
	}
}

void VifUnpackNEON_Base::xShiftR(const vixl::aarch64::VRegister& regX, int n) const
{
	if (usn)
		armAsm->Ushr(regX.V4S(), regX.V4S(), n);
	else
		armAsm->Sshr(regX.V4S(), regX.V4S(), n);
}

void VifUnpackNEON_Base::xPMOVXX8(const vixl::aarch64::VRegister& regX) const
{
	// TODO(Stenzek): Check this
	armAsm->Ldr(regX.S(), srcIndirect);

	if (usn)
	{
		armAsm->Ushll(regX.V8H(), regX.V8B(), 0);
		armAsm->Ushll(regX.V4S(), regX.V4H(), 0);
	}
	else
	{
		armAsm->Sshll(regX.V8H(), regX.V8B(), 0);
		armAsm->Sshll(regX.V4S(), regX.V4H(), 0);
	}
}

void VifUnpackNEON_Base::xPMOVXX16(const vixl::aarch64::VRegister& regX) const
{
	armAsm->Ldr(regX.D(), srcIndirect);

	if (usn)
		armAsm->Ushll(regX.V4S(), regX.V4H(), 0);
	else
		armAsm->Sshll(regX.V4S(), regX.V4H(), 0);
}

void VifUnpackNEON_Base::xUPK_S_32() const
{
	if (UnpkLoopIteration == 0)
		armAsm->Ldr(workReg, srcIndirect);

	if (IsInputMasked())
		return;

	switch (UnpkLoopIteration)
	{
		case 0:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 0);
			break;
		case 1:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 1);
			break;
		case 2:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 2);
			break;
		case 3:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 3);
			break;
	}
}

void VifUnpackNEON_Base::xUPK_S_16() const
{
	if (UnpkLoopIteration == 0)
		xPMOVXX16(workReg);

	if (IsInputMasked())
		return;

	switch (UnpkLoopIteration)
	{
		case 0:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 0);
			break;
		case 1:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 1);
			break;
		case 2:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 2);
			break;
		case 3:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 3);
			break;
	}
}

void VifUnpackNEON_Base::xUPK_S_8() const
{
	if (UnpkLoopIteration == 0)
		xPMOVXX8(workReg);

	if (IsInputMasked())
		return;

	switch (UnpkLoopIteration)
	{
		case 0:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 0);
			break;
		case 1:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 1);
			break;
		case 2:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 2);
			break;
		case 3:
			armAsm->Dup(destReg.V4S(), workReg.V4S(), 3);
			break;
	}
}

// The V2 + V3 unpacks have freaky behaviour, the manual claims "indeterminate".
// After testing on the PS2, it's very much determinate in 99% of cases
// and games like Lemmings, And1 Streetball rely on this data to be like this!
// I have commented after each shuffle to show what data is going where - Ref

void VifUnpackNEON_Base::xUPK_V2_32() const
{
	if (UnpkLoopIteration == 0)
	{
		armAsm->Ldr(workReg, srcIndirect);

		if (IsInputMasked())
			return;

		armAsm->Dup(destReg.V2D(), workReg.V2D(), 0); //v1v0v1v0
		if (IsAligned)
			armAsm->Ins(destReg.V4S(), 3, a64::wzr); //zero last word - tested on ps2
	}
	else
	{
		if (IsInputMasked())
			return;

		armAsm->Dup(destReg.V2D(), workReg.V2D(), 1); //v3v2v3v2
		if (IsAligned)
			armAsm->Ins(destReg.V4S(), 3, a64::wzr); //zero last word - tested on ps2
	}
}

void VifUnpackNEON_Base::xUPK_V2_16() const
{
	if (UnpkLoopIteration == 0)
	{
		xPMOVXX16(workReg);

		if (IsInputMasked())
			return;

		armAsm->Dup(destReg.V2D(), workReg.V2D(), 0); //v1v0v1v0
	}
	else
	{
		if (IsInputMasked())
			return;

		armAsm->Dup(destReg.V2D(), workReg.V2D(), 1); //v3v2v3v2
	}
}

void VifUnpackNEON_Base::xUPK_V2_8() const
{
	if (UnpkLoopIteration == 0)
	{
		xPMOVXX8(workReg);

		if (IsInputMasked())
			return;

		armAsm->Dup(destReg.V2D(), workReg.V2D(), 0); //v1v0v1v0
	}
	else
	{
		if (IsInputMasked())
			return;

		armAsm->Dup(destReg.V2D(), workReg.V2D(), 1); //v3v2v3v2
	}
}

void VifUnpackNEON_Base::xUPK_V3_32() const
{
	if (IsInputMasked())
		return;

	armAsm->Ldr(destReg, srcIndirect);
	if (UnpkLoopIteration != IsAligned)
		armAsm->Ins(destReg.V4S(), 3, a64::wzr);
}

void VifUnpackNEON_Base::xUPK_V3_16() const
{
	if (IsInputMasked())
		return;

	xPMOVXX16(destReg);

	//With V3-16, it takes the first vector from the next position as the W vector
	//However - IF the end of this iteration of the unpack falls on a quadword boundary, W becomes 0
	//IsAligned is the position through the current QW in the vif packet
	//Iteration counts where we are in the packet.
	int result = (((UnpkLoopIteration / 4) + 1 + (4 - IsAligned)) & 0x3);

	if ((UnpkLoopIteration & 0x1) == 0 && result == 0)
		armAsm->Ins(destReg.V4S(), 3, a64::wzr); //zero last word on QW boundary if whole 32bit word is used - tested on ps2
}

void VifUnpackNEON_Base::xUPK_V3_8() const
{
	if (IsInputMasked())
		return;

	xPMOVXX8(destReg);
	if (UnpkLoopIteration != IsAligned)
		armAsm->Ins(destReg.V4S(), 3, a64::wzr);
}

void VifUnpackNEON_Base::xUPK_V4_32() const
{
	if (IsInputMasked())
		return;

	armAsm->Ldr(destReg.Q(), a64::MemOperand(srcIndirect));
}

void VifUnpackNEON_Base::xUPK_V4_16() const
{
	if (IsInputMasked())
		return;

	xPMOVXX16(destReg);
}

void VifUnpackNEON_Base::xUPK_V4_8() const
{
	if (IsInputMasked())
		return;

	xPMOVXX8(destReg);
}

void VifUnpackNEON_Base::xUPK_V4_5() const
{
	if (IsInputMasked())
		return;

	armAsm->Ldrh(workGprW, srcIndirect);
	armAsm->Lsl(workGprW, workGprW, 3); // ABG|R5.000
	armAsm->Dup(destReg.V4S(), workGprW); // x|x|x|R
	armAsm->Lsr(workGprW, workGprW, 8); // ABG
	armAsm->Lsl(workGprW, workGprW, 3); // AB|G5.000
	armAsm->Ins(destReg.V4S(), 1, workGprW); // x|x|G|R
	armAsm->Lsr(workGprW, workGprW, 8); // AB
	armAsm->Lsl(workGprW, workGprW, 3); // A|B5.000
	armAsm->Ins(destReg.V4S(), 2, workGprW); // x|B|G|R
	armAsm->Lsr(workGprW, workGprW, 8); // A
	armAsm->Lsl(workGprW, workGprW, 7); // A.0000000
	armAsm->Ins(destReg.V4S(), 3, workGprW); // A|B|G|R
	armAsm->Shl(destReg.V4S(), destReg.V4S(), 24); // can optimize to
	armAsm->Ushr(destReg.V4S(), destReg.V4S(), 24); // single AND...
}

void VifUnpackNEON_Base::xUnpack(int upknum) const
{
	switch (upknum)
	{
		case 0:
			xUPK_S_32();
			break;
		case 1:
			xUPK_S_16();
			break;
		case 2:
			xUPK_S_8();
			break;

		case 4:
			xUPK_V2_32();
			break;
		case 5:
			xUPK_V2_16();
			break;
		case 6:
			xUPK_V2_8();
			break;

		case 8:
			xUPK_V3_32();
			break;
		case 9:
			xUPK_V3_16();
			break;
		case 10:
			xUPK_V3_8();
			break;

		case 12:
			xUPK_V4_32();
			break;
		case 13:
			xUPK_V4_16();
			break;
		case 14:
			xUPK_V4_8();
			break;
		case 15:
			xUPK_V4_5();
			break;

		case 3:
		case 7:
		case 11:
			pxFailRel(fmt::format("Vpu/Vif - Invalid Unpack! [{}]", upknum).c_str());
			break;
	}
}

// =====================================================================================================
//  VifUnpackSSE_Simple
// =====================================================================================================

VifUnpackNEON_Simple::VifUnpackNEON_Simple(bool usn_, bool domask_, int curCycle_)
{
	curCycle = curCycle_;
	usn = usn_;
	doMask = domask_;
	IsAligned = true;
}

void VifUnpackNEON_Simple::doMaskWrite(const vixl::aarch64::VRegister& regX) const
{
	armAsm->Ldr(a64::q7, dstIndirect);

	int offX = std::min(curCycle, 3);
	armMoveAddressToReg(RXVIXLSCRATCH, nVifMask);
	armAsm->Ldr(a64::q29, a64::MemOperand(RXVIXLSCRATCH, reinterpret_cast<const u8*>(nVifMask[0][offX]) - reinterpret_cast<const u8*>(nVifMask)));
	armAsm->Ldr(a64::q30, a64::MemOperand(RXVIXLSCRATCH, reinterpret_cast<const u8*>(nVifMask[1][offX]) - reinterpret_cast<const u8*>(nVifMask)));
	armAsm->Ldr(a64::q31, a64::MemOperand(RXVIXLSCRATCH, reinterpret_cast<const u8*>(nVifMask[2][offX]) - reinterpret_cast<const u8*>(nVifMask)));
	armAsm->And(regX.V16B(), regX.V16B(), a64::q29.V16B());
	armAsm->And(a64::q7.V16B(), a64::q7.V16B(), a64::q30.V16B());
	armAsm->Orr(regX.V16B(), regX.V16B(), a64::q31.V16B());
	armAsm->Orr(regX.V16B(), regX.V16B(), a64::q7.V16B());
	armAsm->Str(regX, dstIndirect);
}

// ecx = dest, edx = src
static void nVifGen(int usn, int mask, int curCycle)
{

	int usnpart = usn * 2 * 16;
	int maskpart = mask * 16;

	VifUnpackNEON_Simple vpugen(!!usn, !!mask, curCycle);

	for (int i = 0; i < 16; ++i)
	{
		nVifCall& ucall(nVifUpk[((usnpart + maskpart + i) * 4) + curCycle]);
		ucall = NULL;
		if (nVifT[i] == 0)
			continue;

		ucall = (nVifCall)armStartBlock();
		vpugen.xUnpack(i);
		vpugen.xMovDest();
		armAsm->Ret();
		armEndBlock();
	}
}

void VifUnpackSSE_Init()
{
	DevCon.WriteLn("Generating NEON-optimized unpacking functions for VIF interpreters...");

	HostSys::BeginCodeWrite();
	armSetAsmPtr(SysMemory::GetVIFUnpackRec(), SysMemory::GetVIFUnpackRecEnd() - SysMemory::GetVIFUnpackRec(), nullptr);

	for (int a = 0; a < 2; a++)
	{
		for (int b = 0; b < 2; b++)
		{
			for (int c = 0; c < 4; c++)
			{
				nVifGen(a, b, c);
			}
		}
	}

	Perf::any.Register(SysMemory::GetVIFUnpackRec(), armGetAsmPtr() - SysMemory::GetVIFUnpackRec(), "VIF Unpack");
	HostSys::EndCodeWrite();
}
