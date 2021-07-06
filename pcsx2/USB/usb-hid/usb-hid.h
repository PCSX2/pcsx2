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
#include "SaveState.h"
#include "USB/configuration.h"
#include "USB/qemu-usb/hid.h"
#include <list>
#include <string>

namespace usb_hid
{

	enum HIDType
	{
		HIDTYPE_KBD,
		HIDTYPE_MOUSE,
	};

	class UsbHID
	{
	public:
		UsbHID(int port, const char* dev_type)
			: mPort(port)
			, mDevType(dev_type)
		{
		}
		virtual ~UsbHID() {}
		virtual int Open() = 0;
		virtual int Close() = 0;
		//	virtual int TokenIn(uint8_t *buf, int len) = 0;
		virtual int TokenOut(const uint8_t* data, int len) = 0;
		virtual int Reset() = 0;

		virtual int Port() { return mPort; }
		virtual void Port(int port) { mPort = port; }
		virtual void SetHIDState(HIDState* hs) { mHIDState = hs; }
		virtual void SetHIDType(HIDType t) { mHIDType = t; }

	protected:
		int mPort;
		HIDState* mHIDState;
		HIDType mHIDType;
		const char* mDevType;
	};

	class HIDKbdDevice
	{
	public:
		virtual ~HIDKbdDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("HID Keyboard");
		}
		static const char* TypeName()
		{
			return "hidkbd";
		}
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

	class HIDMouseDevice
	{
	public:
		virtual ~HIDMouseDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("HID Mouse");
		}
		static const char* TypeName()
		{
			return "hidmouse";
		}
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

	class BeatManiaDevice
	{
	public:
		virtual ~BeatManiaDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("BeatMania Da Da Da!! Keyboard");
		}
		static const char* TypeName()
		{
			return "beatmania";
		}
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

} // namespace usb_hid
