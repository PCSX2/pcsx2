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

#include <array>

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
