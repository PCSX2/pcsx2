/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#pragma once

#include "SIO/SioTypes.h"

#include <array>

class StateWrapper;

enum class MultitapMode
{
	NOT_SET = 0xff,
	PAD_SUPPORT_CHECK = 0x12,
	MEMCARD_SUPPORT_CHECK = 0x13,
	SELECT_PAD = 0x21,
	SELECT_MEMCARD = 0x22,
};

class MultitapProtocol
{
private:
	u8 currentPadSlot = 0;
	u8 currentMemcardSlot = 0;

	void SupportCheck();
	void Select(MultitapMode mode);

public:
	MultitapProtocol();
	~MultitapProtocol();

	void SoftReset();
	void FullReset();
	bool DoState(StateWrapper& sw);

	u8 GetPadSlot();
	u8 GetMemcardSlot();

	void SendToMultitap();
};

extern std::array<MultitapProtocol, SIO::PORTS> g_MultitapArr;

