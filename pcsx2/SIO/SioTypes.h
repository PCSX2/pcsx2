// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

enum class SioStage
{
	IDLE,
	WAITING_COMMAND,
	WORKING
};

namespace SioMode
{
	static constexpr u8 NOT_SET = 0x00;
	static constexpr u8 PAD = 0x01;
	static constexpr u8 MULTITAP = 0x21;
	static constexpr u8 INFRARED = 0x61;
	static constexpr u8 MEMCARD = 0x81;
} // namespace SioMode

/*
namespace PadCommand
{
	static constexpr u8 UNK_0 = 0x40;
	static constexpr u8 QUERY_BUTTONS = 0x41;
	static constexpr u8 POLL = 0x42;
	static constexpr u8 CONFIG = 0x43;
	static constexpr u8 MODE_SWITCH = 0x44;
	static constexpr u8 STATUS = 0x45;
	static constexpr u8 CONST_1 = 0x46;
	static constexpr u8 CONST_2 = 0x47;
	static constexpr u8 UNK_8 = 0x48;
	static constexpr u8 UNK_9 = 0x49;
	static constexpr u8 UNK_A = 0x4a;
	static constexpr u8 UNK_B = 0x4b;
	static constexpr u8 CONST_3 = 0x4c;
	static constexpr u8 VIBRATION = 0x4d;
	static constexpr u8 UNK_E = 0x4e;
	static constexpr u8 ANALOG = 0x4f;
} // namespace PadCommand
*/

namespace MemcardCommand
{
	static constexpr u8 NOT_SET = 0x00;
	static constexpr u8 PROBE = 0x11;
	static constexpr u8 UNKNOWN_WRITE_DELETE_END = 0x12;
	static constexpr u8 SET_ERASE_SECTOR = 0x21;
	static constexpr u8 SET_WRITE_SECTOR = 0x22;
	static constexpr u8 SET_READ_SECTOR = 0x23;
	static constexpr u8 GET_SPECS = 0x26;
	static constexpr u8 SET_TERMINATOR = 0x27;
	static constexpr u8 GET_TERMINATOR = 0x28;
	static constexpr u8 WRITE_DATA = 0x42;
	static constexpr u8 READ_DATA = 0x43;
	static constexpr u8 PS1_READ = 0x52;
	static constexpr u8 PS1_STATE = 0x53;
	static constexpr u8 PS1_WRITE = 0x57;
	static constexpr u8 PS1_POCKETSTATION = 0x58;
	static constexpr u8 READ_WRITE_END = 0x81;
	static constexpr u8 ERASE_BLOCK = 0x82;
	static constexpr u8 UNKNOWN_BOOT = 0xbf;
	static constexpr u8 AUTH_XOR = 0xf0;
	static constexpr u8 AUTH_F3 = 0xf3;
	static constexpr u8 AUTH_F7 = 0xf7;
} // namespace MemcardCommand

enum class Sio0Interrupt
{
	TEST_EVENT,
	STAT_READ,
	TX_DATA_WRITE
}; // namespace Sio0Interrupt

namespace SIO
{
	static constexpr u8 PORTS = 2;
	static constexpr u8 SLOTS = 4;
} // namespace SIO

namespace SIO0_STAT
{
	static constexpr u32 TX_READY = 0x01;
	static constexpr u32 RX_FIFO_NOT_EMPTY = 0x02;
	static constexpr u32 TX_EMPTY = 0x04;
	static constexpr u32 RX_PARITY_ERROR = 0x08;
	static constexpr u32 ACK = 0x80;
	static constexpr u32 IRQ = 0x0200;
} // namespace SIO0_STAT

namespace SIO0_CTRL
{
	static constexpr u16 TX_ENABLE = 0x01;
	static constexpr u16 RX_ENABLE = 0x04;
	static constexpr u16 ACK = 0x10;
	static constexpr u16 RESET = 0x40;
	static constexpr u16 RX_INT_MODE_LSB = 0x0100;
	static constexpr u16 RX_INT_MODE_MSB = 0x0200;
	static constexpr u16 TX_INT_ENABLE = 0x0400;
	static constexpr u16 RX_INT_ENABLE = 0x0800;
	static constexpr u16 ACK_INT_ENABLE = 0x1000;
	static constexpr u16 PORT = 0x2000;
} // namespace SIO0_CTRL

namespace Send3
{
	static constexpr u32 PORT = 0x01;
	static constexpr u16 COMMAND_LENGTH_MASK = 0x3ff;
} // namespace Send3

namespace Sio2Ctrl
{
	static constexpr u32 START_TRANSFER = 0x1;
	static constexpr u32 RESET = 0xc;
	static constexpr u32 PORT = 0x2000;
	// The value which SIO2MAN resets SIO2_CTRL to after a system reset.
	static constexpr u32 SIO2MAN_RESET = 0x000003bc;
} // namespace Sio2Ctrl

// TODO: Remove deprecated options once memcards are no longer using them.
namespace Recv1
{
	// Deprecated
	static constexpr u32 DISCONNECTED = 0x1d100;
	// Deprecated
	static constexpr u32 CONNECTED = 0x1100;

	static constexpr u32 NO_DEVICES_MISSING = 0x1000;
	static constexpr u32 PORT_1_MISSING = 0x1D000;
	static constexpr u32 PORT_2_MISSING = 0x2D000;
	static constexpr u32 BOTH_PORTS_MISSING = 0x3D000;
	static constexpr u32 ONE_PORT_OPEN = 0x100;
	static constexpr u32 TWO_PORTS_OPEN = 0x200;

} // namespace Recv1

namespace Recv2
{
	static constexpr u32 DEFAULT = 0xf;
} // namespace Recv2

// Most RECV3 values are mysterious, undocumented, and their purpose
// can only be inferred from how old, mostly incorrect PCSX2 code tried
// to use them. We're going to try and respect these where it seems like
// it may make sense to do so, but these are still largely unknown and
// tests suggest they are not even used at all.
namespace Recv3
{
	static constexpr u32 DEFAULT = 0x0;
	// Set when getting memcard specs
	static constexpr u32 SPECS = 0x83;
	// Set when getting or setting the terminator byte
	static constexpr u32 TERMINATOR = 0x8b;
	// Set when setting the read/write sector
	static constexpr u32 READ_WRITE_END = 0x8c;
} // namespace Recv3

namespace Terminator
{
	static constexpr u32 NOT_READY = 0x66;
	static constexpr u32 READY = 0x55;
} // namespace Terminator
