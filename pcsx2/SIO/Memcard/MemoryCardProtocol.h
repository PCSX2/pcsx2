// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <array>

#include "common/Pcsx2Defs.h"

struct PS1MemoryCardState
{
	size_t currentByte = 2;
	u8 sectorAddrMSB = 0;
	u8 sectorAddrLSB = 0;
	u8 checksum = 0;
	u8 expectedChecksum = 0;
	std::array<u8, 128> buf = {0};
};

// A global class which contains the behavior of each memory card command.
class MemoryCardProtocol
{
private:
	PS1MemoryCardState ps1McState;

	bool PS1Fail();
	void The2bTerminator(size_t length);
	void ReadWriteIncrement(size_t length);
	void RecalculatePS1Addr();

public:
	void ResetPS1State();

	void Probe();
	void UnknownWriteDeleteEnd();
	void SetSector();
	void GetSpecs();
	void SetTerminator();
	void GetTerminator();
	void WriteData();
	void ReadData();
	u8 PS1Read(u8 data);
	u8 PS1State(u8 data);
	u8 PS1Write(u8 data);
	u8 PS1Pocketstation(u8 data);
	void ReadWriteEnd();
	void EraseBlock();
	void UnknownBoot();
	void AuthXor();
	void AuthF3();
	void AuthF7();
};

extern MemoryCardProtocol g_MemoryCardProtocol;
