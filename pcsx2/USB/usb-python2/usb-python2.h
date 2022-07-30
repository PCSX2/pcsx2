/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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
#include <thread>
#include <atomic>

#include "SaveState.h"
#include "USB/configuration.h"
#include "USB/qemu-usb/hid.h"

#include <list>
#include <string>

namespace usb_python2
{
	static const char* APINAME = "python2";

	struct P2IO_PACKET_HEADER
	{
		uint8_t magic;
		uint8_t len;
		uint8_t seqNo;
		uint8_t cmd;
	};

	enum
	{
		// cmd_... names are from internal namings from symbols
		P2IO_CMD_GET_VERSION = 0x01, // cmd_getver
		P2IO_CMD_RESEND = 0x02, // resend_cmd
		P2IO_CMD_FWRITEMODE = 0x03, // cmd_fwritemode 0x03aa
		P2IO_CMD_SET_WATCHDOG = 0x05, // cmd_watchdog
		P2IO_CMD_SET_AV_MASK = 0x22, // cmd_avmask
		P2IO_CMD_GET_AV_REPORT = 0x23, // cmd_avreport
		P2IO_CMD_LAMP_OUT = 0x24, // cmd_lampout = 0x24, cmd_out_all = 0x24ff
		P2IO_CMD_DALLAS = 0x25, // cmd_dallas
		P2IO_CMD_SEND_IR = 0x26, // cmd_irsend
		P2IO_CMD_READ_DIPSWITCH = 0x27, // cmd_dipsw
		P2IO_CMD_GET_JAMMA_POR = 0x28, // cmd_jamma_por
		P2IO_CMD_PORT_READ = 0x29, // cmd_portread
		P2IO_CMD_PORT_READ_POR = 0x2a, // cmd_portread_por
		P2IO_CMD_JAMMA_START = 0x2f, // cmd_jammastart
		P2IO_CMD_COIN_STOCK = 0x31, // cmd_coinstock
		P2IO_CMD_COIN_COUNTER = 0x32, // cmd_coincounter
		P2IO_CMD_COIN_BLOCKER = 0x33, // cmd_coinblocker
		P2IO_CMD_COIN_COUNTER_OUT = 0x34, // cmd_coincounterout
		P2IO_CMD_SCI_SETUP = 0x38, // cmd_scisetup
		P2IO_CMD_SCI_WRITE = 0x3a, // cmd_sciwrite
		P2IO_CMD_SCI_READ = 0x3b // cmd_sciread
	};

	enum
	{
		P2IO_AVREPORT_MODE_15KHZ = 0x80, // DDR says it's 15kHz, bootloader says 16kHz
		P2IO_AVREPORT_MODE_31KHZ = 0,
	};

	enum
	{
		GAMETYPE_DM = 0,
		GAMETYPE_GF,
		GAMETYPE_DDR,
		GAMETYPE_TOYSMARCH,
		GAMETYPE_THRILLDRIVE,
		GAMETYPE_DANCE864
	};

	constexpr uint8_t P2IO_HEADER_MAGIC = 0xaa;

	constexpr uint8_t P2IO_STATUS_OK = 0;

	constexpr uint32_t P2IO_JAMMA_IO_TEST = 0x10000000;
	constexpr uint32_t P2IO_JAMMA_IO_COIN1 = 0x20000000;
	constexpr uint32_t P2IO_JAMMA_IO_COIN2 = 0x80000000;
	constexpr uint32_t P2IO_JAMMA_IO_SERVICE = 0x40000000;
	constexpr uint32_t P2IO_JAMMA_IO_SERVICE2 = 0x00000080;

	constexpr uint32_t P2IO_JAMMA_GF_P1_START = 0x00000100;
	constexpr uint32_t P2IO_JAMMA_GF_P1_PICK = 0x00000200;
	constexpr uint32_t P2IO_JAMMA_GF_P1_WAILING = 0x00000400;
	constexpr uint32_t P2IO_JAMMA_GF_P1_EFFECT2 = 0x00000800;
	constexpr uint32_t P2IO_JAMMA_GF_P1_EFFECT1 = 0x00001000;
	constexpr uint32_t P2IO_JAMMA_GF_P1_EFFECT3 = P2IO_JAMMA_GF_P1_EFFECT1 | P2IO_JAMMA_GF_P1_EFFECT2;
	constexpr uint32_t P2IO_JAMMA_GF_P1_R = 0x00002000;
	constexpr uint32_t P2IO_JAMMA_GF_P1_G = 0x00004000;
	constexpr uint32_t P2IO_JAMMA_GF_P1_B = 0x00008000;

	constexpr uint32_t P2IO_JAMMA_GF_P2_START = 0x00010000;
	constexpr uint32_t P2IO_JAMMA_GF_P2_PICK = 0x00020000;
	constexpr uint32_t P2IO_JAMMA_GF_P2_WAILING = 0x00040000;
	constexpr uint32_t P2IO_JAMMA_GF_P2_EFFECT2 = 0x00080000;
	constexpr uint32_t P2IO_JAMMA_GF_P2_EFFECT1 = 0x00100000;
	constexpr uint32_t P2IO_JAMMA_GF_P2_EFFECT3 = P2IO_JAMMA_GF_P2_EFFECT1 | P2IO_JAMMA_GF_P2_EFFECT2;
	constexpr uint32_t P2IO_JAMMA_GF_P2_R = 0x00200000;
	constexpr uint32_t P2IO_JAMMA_GF_P2_G = 0x00400000;
	constexpr uint32_t P2IO_JAMMA_GF_P2_B = 0x00800000;

	constexpr uint32_t P2IO_JAMMA_DM_START = 0x00000100;
	constexpr uint32_t P2IO_JAMMA_DM_HIHAT = 0x00000200;
	constexpr uint32_t P2IO_JAMMA_DM_SNARE = 0x00000400;
	constexpr uint32_t P2IO_JAMMA_DM_HIGH_TOM = 0x00000800;
	constexpr uint32_t P2IO_JAMMA_DM_LOW_TOM = 0x00001000;
	constexpr uint32_t P2IO_JAMMA_DM_CYMBAL = 0x00002000;
	constexpr uint32_t P2IO_JAMMA_DM_BASS_DRUM = 0x00008000;
	constexpr uint32_t P2IO_JAMMA_DM_SELECT_L = 0x00080000;
	constexpr uint32_t P2IO_JAMMA_DM_SELECT_R = 0x00100000;

	constexpr uint32_t P2IO_JAMMA_DDR_P1_START = 0x00000100;
	constexpr uint32_t P2IO_JAMMA_DDR_P1_LEFT = 0x00004000;
	constexpr uint32_t P2IO_JAMMA_DDR_P1_RIGHT = 0x00008000;
	constexpr uint32_t P2IO_JAMMA_DDR_P1_FOOT_UP = 0x00000200;
	constexpr uint32_t P2IO_JAMMA_DDR_P1_FOOT_DOWN = 0x00000400;
	constexpr uint32_t P2IO_JAMMA_DDR_P1_FOOT_LEFT = 0x00000800;
	constexpr uint32_t P2IO_JAMMA_DDR_P1_FOOT_RIGHT = 0x00001000;

	constexpr uint32_t P2IO_JAMMA_DDR_P2_START = 0x00010000;
	constexpr uint32_t P2IO_JAMMA_DDR_P2_LEFT = 0x00400000;
	constexpr uint32_t P2IO_JAMMA_DDR_P2_RIGHT = 0x00800000;
	constexpr uint32_t P2IO_JAMMA_DDR_P2_FOOT_UP = 0x00020000;
	constexpr uint32_t P2IO_JAMMA_DDR_P2_FOOT_DOWN = 0x00040000;
	constexpr uint32_t P2IO_JAMMA_DDR_P2_FOOT_LEFT = 0x00080000;
	constexpr uint32_t P2IO_JAMMA_DDR_P2_FOOT_RIGHT = 0x00100000;

	constexpr uint32_t P2IO_JAMMA_THRILLDRIVE_START = 0x00000100;
	constexpr uint32_t P2IO_JAMMA_THRILLDRIVE_GEARSHIFT_DOWN = 0x00000200;
	constexpr uint32_t P2IO_JAMMA_THRILLDRIVE_GEARSHIFT_UP = 0x00000400;

	constexpr uint32_t P2IO_JAMMA_TOYSMARCH_P1_START = 0x00000100;
	constexpr uint32_t P2IO_JAMMA_TOYSMARCH_P1_LEFT = 0x00000800;
	constexpr uint32_t P2IO_JAMMA_TOYSMARCH_P1_RIGHT = 0x00001000;

	constexpr uint32_t P2IO_JAMMA_TOYSMARCH_P2_START = 0x00010000;
	constexpr uint32_t P2IO_JAMMA_TOYSMARCH_P2_LEFT = 0x00080000;
	constexpr uint32_t P2IO_JAMMA_TOYSMARCH_P2_RIGHT = 0x00100000;

	constexpr uint32_t P2IO_JAMMA_DANCE864_P1_START = 0x00000100;
	constexpr uint32_t P2IO_JAMMA_DANCE864_P1_LEFT = 0x00002000;
	constexpr uint32_t P2IO_JAMMA_DANCE864_P1_RIGHT = 0x00004000;
	constexpr uint32_t P2IO_JAMMA_DANCE864_P1_PAD_LEFT = 0x00000800;
	constexpr uint32_t P2IO_JAMMA_DANCE864_P1_PAD_CENTER = 0x00000400;
	constexpr uint32_t P2IO_JAMMA_DANCE864_P1_PAD_RIGHT = 0x00001000;

	constexpr uint32_t P2IO_JAMMA_DANCE864_P2_START = 0x00010000;
	constexpr uint32_t P2IO_JAMMA_DANCE864_P2_LEFT = 0x00200000;
	constexpr uint32_t P2IO_JAMMA_DANCE864_P2_RIGHT = 0x00400000;
	constexpr uint32_t P2IO_JAMMA_DANCE864_P2_PAD_LEFT = 0x00080000;
	constexpr uint32_t P2IO_JAMMA_DANCE864_P2_PAD_CENTER = 0x00040000;
	constexpr uint32_t P2IO_JAMMA_DANCE864_P2_PAD_RIGHT = 0x00100000;

	class Python2Input
	{
	public:
		Python2Input(int port, const char* dev_type)
			: mPort(port)
			, mDevType(dev_type)
		{
		}
		virtual ~Python2Input() {}
		virtual int Open() = 0;
		virtual int Close() = 0;
		virtual int ReadPacket(std::vector<uint8_t>& data) = 0;
		virtual int WritePacket(const std::vector<uint8_t>& data) = 0;
		virtual void ReadIo(std::vector<uint8_t>& data) = 0;
		virtual int Reset() = 0;
		virtual bool isPassthrough() = 0;

		virtual int Port() { return mPort; }
		virtual void Port(int port) { mPort = port; }

		virtual void UpdateKeyStates(std::wstring keybind) = 0;
		virtual bool GetKeyState(std::wstring keybind) = 0;
		virtual bool GetKeyStateOneShot(std::wstring keybind) = 0;
		virtual double GetKeyStateAnalog(std::wstring keybind) = 0;
		virtual bool IsKeybindAvailable(std::wstring keybind) = 0;
		virtual bool IsAnalogKeybindAvailable(std::wstring keybind) = 0;

	protected:
		int mPort;
		const char* mDevType;
	};

	class Python2Device
	{
	public:
		virtual ~Python2Device() {}

		static USBDevice* CreateDevice(int port);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);

		static const TCHAR* Name() { return TEXT("Python 2"); }
		static const char* TypeName() { return "python2io"; }
		static std::vector<std::string> SubTypes() { return {}; }
	};

} // namespace usb_python2
