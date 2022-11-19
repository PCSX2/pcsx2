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

// Huge thanks to PSI for his work reversing the PS2, his documentation on SIO2 pretty much saved
// this entire implementation. https://psi-rockin.github.io/ps2tek/#sio2registers

#pragma once

#include "SioTypes.h"
#include "MemoryCardFile.h"
#include <array>
#include <deque>

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

class Sio0
{
private:
	u32 txData; // 0x1f801040
	u32 rxData; // 0x1f801040
	u32 stat; // 0x1f801044
	u16 mode; // 0x1f801048
	u16 ctrl; // 0x1f80104a
	u16 baud; // 0x1f80104e

	void ClearStatAcknowledge();

public:
	u8 flag = 0;

	SioStage sioStage = SioStage::IDLE;
	u8 sioMode = SioMode::NOT_SET;
	u8 sioCommand = 0;
	bool padStarted = false;
	bool rxDataSet = false;

	u8 port = 0;
	u8 slot = 0;

	Sio0();
	~Sio0();

	void SoftReset();
	void FullReset();

	void Acknowledge();
	void Interrupt(Sio0Interrupt sio0Interrupt);

	u8 GetTxData();
	u8 GetRxData();
	u32 GetStat();
	u16 GetMode();
	u16 GetCtrl();
	u16 GetBaud();

	void SetTxData(u8 value);
	void SetRxData(u8 value);
	void SetStat(u32 value);
	void SetMode(u16 value);
	void SetCtrl(u16 value);
	void SetBaud(u16 value);

	bool IsPadCommand(u8 command);
	bool IsMemcardCommand(u8 command);
	bool IsPocketstationCommand(u8 command);

	u8 Pad(u8 value);
	u8 Memcard(u8 value);
};

class Sio2
{
private:
	void UpdateInputRecording(u8& dataIn, u8& dataOut);

public:
	std::array<u32, 16> send3;	// 0x1f808200 - 0x1f80823f
	// SEND1 and SEND2 are an unusual bunch. It's not entirely clear just from
	// documentation but these registers almost seem like they are the same thing;
	// when bit 2 is set, SEND2 is being read/written. When bit 2 isn't set, it is
	// SEND1. Their use is not really known, either.
	std::array<u32, 4> send1;	// 0x1f808240 - 0x1f80825f
	std::array<u32, 4> send2;	// 0x1f808240 - 0x1f80825f
	u32 dataIn;					// 0x1f808260
	u32 dataOut;				// 0x1f808264
	u32 ctrl;					// 0x1f808268
	u32 recv1;					// 0x1f80826c
	u32 recv2;					// 0x1f808270
	u32 recv3;					// 0x1f808274
	u32 unknown1;				// 0x1f808278
	u32 unknown2;				// 0x1f80827c
	u32 iStat;					// 0x1f808280

	u8 port = 0;
	u8 slot = 0;

	// The current working index of SEND3. The SEND3 register is a 16 position
	// array of command descriptors. Each descriptor describes the port the command
	// is targeting, as well as the length of the command in bytes.
	bool send3Read = false;
	size_t send3Position = 0;
	size_t commandLength = 0;
	size_t processedLength = 0;
	// Tracks the size of a single block of DMA11/DMA12 data. psxDma11 will set this prior
	// to doing writes, and Sio2::SetSend3 will clear this to ensure a non-DMA write into SIO2
	// does not accidentally use dmaBlockSize.
	size_t dmaBlockSize = 0;
	bool send3Complete = false;

	Sio2();
	~Sio2();

	void SoftReset();
	void FullReset();

	void Interrupt();

	void SetCtrl(u32 value);
	void SetSend3(size_t position, u32 value);
	void SetRecv1(u32 value);

	void Pad();
	void Multitap();
	void Infrared();
	void Memcard();
	
	void Write(u8 data);
	u8 Read();
};

extern std::deque<u8> fifoIn;
extern std::deque<u8> fifoOut;

extern Sio0 sio0;
extern Sio2 sio2;

extern _mcd mcds[2][4];
extern _mcd *mcd;

extern void sioNextFrame();

/// Converts a global pad index to a multitap port and slot.
extern std::tuple<u32, u32> sioConvertPadToPortAndSlot(u32 index);

/// Converts a multitap port and slot to a global pad index.
extern u32 sioConvertPortAndSlotToPad(u32 port, u32 slot);

/// Returns true if the given pad index is a multitap slot.
extern bool sioPadIsMultitapSlot(u32 index);
extern bool sioPortAndSlotIsMultitap(u32 port, u32 slot);
extern void sioSetGameSerial(const std::string& serial);

namespace AutoEject
{
	extern void Set(size_t port, size_t slot);
	extern void Clear(size_t port, size_t slot);
	extern void SetAll();
	extern void ClearAll();
}