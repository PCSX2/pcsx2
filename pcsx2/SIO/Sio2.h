// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"

#include <array>
#include <deque>

class StateWrapper;

class Sio2
{
public:
	std::array<u32, 16> CmdQueue; // 0x1f808200 - 0x1f80823f
	std::array<u32, 4> PortCtrl0; // 0x1f808240 - 0x1f80825f
	std::array<u32, 4> PortCtrl1; // 0x1f808240 - 0x1f80825f
	u32 dataIn; // 0x1f808260
	u32 dataOut; // 0x1f808264
	u32 ctrl; // 0x1f808268
	u32 CmdStat; // 0x1f80826c
	u32 PortStat; // 0x1f808270
	u32 FifoStat; // 0x1f808274
	u32 FifoTxPos; // 0x1f808278
	u32 FifoRxPos; // 0x1f80827c
	u32 iStat; // 0x1f808280

	u8 port = 0;

	// The current working index of the command queue. The queue is a 16 position
	// array of command descriptors. Each descriptor describes the port the command
	// is targeting, as well as the length of the command in bytes.
	bool queueRead = false;
	size_t queuePosition = 0;
	size_t commandLength = 0;
	size_t processedLength = 0;
	// Tracks the size of a single block of DMA11/DMA12 data. psxDma11 will set this prior
	// to doing writes, and Sio2::SetSend3 will clear this to ensure a non-DMA write into SIO2
	// does not accidentally use dmaBlockSize.
	size_t dmaBlockSize = 0;
	bool queueComplete = false;

	Sio2();
	~Sio2();

	bool Initialize();
	bool Shutdown();

	void SoftReset();
	bool DoState(StateWrapper& sw);

	void Interrupt();

	void SetCtrl(u32 value);
	void SetCmd(size_t position, u32 value);
	void SetCmdStat(u32 value);

	void Pad();
	void Multitap();
	void Infrared();
	void Memcard();

	void Write(u8 data);
	u8 Read();
};

extern std::deque<u8> g_Sio2FifoIn;
extern std::deque<u8> g_Sio2FifoOut;
extern Sio2 g_Sio2;
