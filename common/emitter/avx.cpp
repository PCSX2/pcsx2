// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/emitter/internal.h"

// warning: suggest braces around initialization of subobject [-Wmissing-braces]
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif

namespace x86Emitter
{
	const xImplAVX_Move xVMOVAPS = {0x00, 0x28, 0x29};
	const xImplAVX_Move xVMOVUPS = {0x00, 0x10, 0x11};

	const xImplAVX_ArithFloat xVADD = {
		{0x00, 0x58}, // VADDPS
		{0x66, 0x58}, // VADDPD
		{0xF3, 0x58}, // VADDSS
		{0xF2, 0x58}, // VADDSD
	};
	const xImplAVX_ArithFloat xVSUB = {
		{0x00, 0x5C}, // VSUBPS
		{0x66, 0x5C}, // VSUBPD
		{0xF3, 0x5C}, // VSUBSS
		{0xF2, 0x5C}, // VSUBSD
	};
	const xImplAVX_ArithFloat xVMUL = {
		{0x00, 0x59}, // VMULPS
		{0x66, 0x59}, // VMULPD
		{0xF3, 0x59}, // VMULSS
		{0xF2, 0x59}, // VMULSD
	};
	const xImplAVX_ArithFloat xVDIV = {
		{0x00, 0x5E}, // VDIVPS
		{0x66, 0x5E}, // VDIVPD
		{0xF3, 0x5E}, // VDIVSS
		{0xF2, 0x5E}, // VDIVSD
	};
	const xImplAVX_CmpFloat xVCMP = {
		{SSE2_Equal},
		{SSE2_Less},
		{SSE2_LessOrEqual},
		{SSE2_Unordered},
		{SSE2_NotEqual},
		{SSE2_NotLess},
		{SSE2_NotLessOrEqual},
		{SSE2_Ordered},
	};
	const xImplAVX_ThreeArgYMM xVPAND = {0x66, 0xDB};
	const xImplAVX_ThreeArgYMM xVPANDN = {0x66, 0xDF};
	const xImplAVX_ThreeArgYMM xVPOR = {0x66, 0xEB};
	const xImplAVX_ThreeArgYMM xVPXOR = {0x66, 0xEF};
	const xImplAVX_CmpInt xVPCMP = {
		{0x66, 0x74}, // VPCMPEQB
		{0x66, 0x75}, // VPCMPEQW
		{0x66, 0x76}, // VPCMPEQD
		{0x66, 0x64}, // VPCMPGTB
		{0x66, 0x65}, // VPCMPGTW
		{0x66, 0x66}, // VPCMPGTD
	};

	void xVPMOVMSKB(const xRegister32& to, const xRegisterSSE& from)
	{
		xOpWriteC5(0x66, 0xd7, to, xRegister32(), from);
	}

	void xVMOVMSKPS(const xRegister32& to, const xRegisterSSE& from)
	{
		xOpWriteC5(0x00, 0x50, to, xRegister32(), from);
	}

	void xVMOVMSKPD(const xRegister32& to, const xRegisterSSE& from)
	{
		xOpWriteC5(0x66, 0x50, to, xRegister32(), from);
	}

	void xVZEROUPPER()
	{
		// rather than dealing with nonexistant operands..
		xWrite8(0xc5);
		xWrite8(0xf8);
		xWrite8(0x77);
	}

	void xImplAVX_Move::operator()(const xRegisterSSE& to, const xRegisterSSE& from) const
	{
		if (to != from)
			xOpWriteC5(Prefix, LoadOpcode, to, xRegisterSSE(), from);
	}

	void xImplAVX_Move::operator()(const xRegisterSSE& to, const xIndirectVoid& from) const
	{
		xOpWriteC5(Prefix, LoadOpcode, to, xRegisterSSE(), from);
	}

	void xImplAVX_Move::operator()(const xIndirectVoid& to, const xRegisterSSE& from) const
	{
		xOpWriteC5(Prefix, StoreOpcode, from, xRegisterSSE(), to);
	}

	void xImplAVX_ThreeArg::operator()(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const
	{
		pxAssert(!to.IsWideSIMD() && !from1.IsWideSIMD() && !from2.IsWideSIMD());
		xOpWriteC5(Prefix, Opcode, to, from1, from2);
	}

	void xImplAVX_ThreeArg::operator()(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const
	{
		pxAssert(!to.IsWideSIMD() && !from1.IsWideSIMD());
		xOpWriteC5(Prefix, Opcode, to, from1, from2);
	}

	void xImplAVX_ThreeArgYMM::operator()(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const
	{
		xOpWriteC5(Prefix, Opcode, to, from1, from2);
	}

	void xImplAVX_ThreeArgYMM::operator()(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const
	{
		xOpWriteC5(Prefix, Opcode, to, from1, from2);
	}

	void xImplAVX_CmpFloatHelper::PS(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const
	{
		xOpWriteC5(0x00, 0xC2, to, from1, from2);
		xWrite8(static_cast<u8>(CType));
	}

	void xImplAVX_CmpFloatHelper::PS(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const
	{
		xOpWriteC5(0x00, 0xC2, to, from1, from2);
		xWrite8(static_cast<u8>(CType));
	}

	void xImplAVX_CmpFloatHelper::PD(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const
	{
		xOpWriteC5(0x66, 0xC2, to, from1, from2);
		xWrite8(static_cast<u8>(CType));
	}

	void xImplAVX_CmpFloatHelper::PD(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const
	{
		xOpWriteC5(0x66, 0xC2, to, from1, from2);
		xWrite8(static_cast<u8>(CType));
	}

	void xImplAVX_CmpFloatHelper::SS(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const
	{
		xOpWriteC5(0xF3, 0xC2, to, from1, from2);
		xWrite8(static_cast<u8>(CType));
	}

	void xImplAVX_CmpFloatHelper::SS(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const
	{
		xOpWriteC5(0xF3, 0xC2, to, from1, from2);
		xWrite8(static_cast<u8>(CType));
	}

	void xImplAVX_CmpFloatHelper::SD(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const
	{
		xOpWriteC5(0xF2, 0xC2, to, from1, from2);
		xWrite8(static_cast<u8>(CType));
	}

	void xImplAVX_CmpFloatHelper::SD(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const
	{
		xOpWriteC5(0xF2, 0xC2, to, from1, from2);
		xWrite8(static_cast<u8>(CType));
	}
} // namespace x86Emitter
