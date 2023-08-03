/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

// Huge thanks to PSI for his work reversing the PS2, his documentation on SIO2 pretty much saved
// this entire implementation. https://psi-rockin.github.io/ps2tek/#sio2registers

#pragma once

#include "SIO/Memcard/MemoryCardFile.h"

struct _mcd
{
	u8 currentCommand;
	u8 term; // terminator value;

	bool goodSector; // xor sector check
	u8 msb;
	u8 lsb;
	u32 sectorAddr;  // read/write sector address
	u32 transferAddr; // Transfer address

	std::vector<u8> buf; // Buffer for reading and writing

	u8 FLAG;  // for PSX;

	u8 port; // port
	u8 slot; // and slot for this memcard

	size_t autoEjectTicks;

	void GetSizeInfo(McdSizeInfo &info)
	{
		FileMcd_GetSizeInfo(port, slot, &info);
	}

	bool IsPSX()
	{
		return FileMcd_IsPSX(port, slot);
	}

	void EraseBlock()
	{
		//DevCon.WriteLn("Memcard Erase (sectorAddr = %08X)", sectorAddr);
		FileMcd_EraseBlock(port, slot, transferAddr);
	}

	// Read from memorycard to dest
	void Read(u8 *dest, int size)
	{
		//DevCon.WriteLn("Memcard Read (sectorAddr = %08X)", sectorAddr);
		FileMcd_Read(port, slot, dest, transferAddr, size);
	}

	// Write to memorycard from src
	void Write(u8 *src, int size)
	{
		//DevCon.WriteLn("Memcard Write (sectorAddr = %08X)", sectorAddr);
		FileMcd_Save(port, slot, src,transferAddr, size);
	}

	bool IsPresent()
	{
		return FileMcd_IsPresent(port, slot);
	}

	u8 DoXor()
	{
		u8 ret = msb ^ lsb;

		for (const u8 byte : buf)
		{
			ret ^= byte;
		}

		return ret;
	}

	u64 GetChecksum()
	{
		return FileMcd_GetCRC(port, slot);
	}

	void NextFrame() {
		FileMcd_NextFrame( port, slot );
	}

	bool ReIndex(const std::string& filter) {
		return FileMcd_ReIndex(port, slot, filter);
	}
};

extern _mcd mcds[2][4];
extern _mcd *mcd;

extern void sioNextFrame();

/// Converts a global pad index to a multitap port and slot.
extern std::tuple<u32, u32> sioConvertPadToPortAndSlot(u32 index);

/// Convert the PS2's port/slot addressing to a single value.
/// Physical ports 0 and 1 still correspond to unified slots 0 and 1.
/// The remaining unified slots are for multitapped slots.
/// Port 0's three multitap slots then occupy unified slots 2, 3 and 4.
/// Port 1's three multitap slots then occupy unified slots 5, 6 and 7.
extern u32 sioConvertPortAndSlotToPad(u32 port, u32 slot);

/// Returns true if the given pad index is a multitap slot.
extern bool sioPadIsMultitapSlot(u32 index);
extern bool sioPortAndSlotIsMultitap(u32 port, u32 slot);
extern void sioSetGameSerial(const std::string& serial);

namespace AutoEject
{
	extern void CountDownTicks();
	extern void Set(size_t port, size_t slot);
	extern void Clear(size_t port, size_t slot);
	extern void SetAll();
	extern void ClearAll();
} // namespace AutoEject
