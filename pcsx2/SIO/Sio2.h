// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <array>
#include <deque>

class StateWrapper;

class Sio2
{
public:
	std::array<u32, 16> send3; // 0x1f808200 - 0x1f80823f
	// SEND1 and SEND2 are an unusual bunch. It's not entirely clear just from
	// documentation but these registers almost seem like they are the same thing;
	// when bit 2 is set, SEND2 is being read/written. When bit 2 isn't set, it is
	// SEND1. Their use is not really known, either.
	std::array<u32, 4> send1; // 0x1f808240 - 0x1f80825f
	std::array<u32, 4> send2; // 0x1f808240 - 0x1f80825f
	u32 dataIn; // 0x1f808260
	u32 dataOut; // 0x1f808264
	u32 ctrl; // 0x1f808268
	u32 recv1; // 0x1f80826c
	u32 recv2; // 0x1f808270
	u32 recv3; // 0x1f808274
	u32 unknown1; // 0x1f808278
	u32 unknown2; // 0x1f80827c
	u32 iStat; // 0x1f808280

	u8 port = 0;

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

	bool Initialize();
	bool Shutdown();

	void SoftReset();
	bool DoState(StateWrapper& sw);

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

extern std::deque<u8> g_Sio2FifoIn;
extern std::deque<u8> g_Sio2FifoOut;
extern Sio2 g_Sio2;
