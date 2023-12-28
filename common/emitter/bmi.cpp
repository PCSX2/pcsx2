// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/emitter/internal.h"

namespace x86Emitter
{

	const xImplBMI_RVM xMULX = {0xF2, 0x38, 0xF6};
	const xImplBMI_RVM xPDEP = {0xF2, 0x38, 0xF5};
	const xImplBMI_RVM xPEXT = {0xF3, 0x38, 0xF5};
	const xImplBMI_RVM xANDN_S = {0x00, 0x38, 0xF2};

	void xImplBMI_RVM::operator()(const xRegisterInt& to, const xRegisterInt& from1, const xRegisterInt& from2) const
	{
		xOpWriteC4(Prefix, MbPrefix, Opcode, to, from1, from2);
	}
	void xImplBMI_RVM::operator()(const xRegisterInt& to, const xRegisterInt& from1, const xIndirectVoid& from2) const
	{
		xOpWriteC4(Prefix, MbPrefix, Opcode, to, from1, from2);
	}
} // namespace x86Emitter
