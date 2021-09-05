/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2015  PCSX2 Dev Team
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

#include "common/emitter/internal.h"
#include "common/emitter/tools.h"

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
