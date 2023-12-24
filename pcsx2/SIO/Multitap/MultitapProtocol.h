// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

