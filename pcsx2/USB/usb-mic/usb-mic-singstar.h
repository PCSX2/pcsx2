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

#ifndef USBMICSINGSTAR_H
#define USBMICSINGSTAR_H
#include "USB/deviceproxy.h"
#include "audiodeviceproxy.h"

struct USBDevice;

namespace usb_mic
{
	class SingstarDevice
	{
	public:
		virtual ~SingstarDevice() {}
		static USBDevice* CreateDevice(int port);
		static USBDevice* CreateDevice(int port, const std::string& api);
		static const TCHAR* Name()
		{
			return TEXT("Singstar");
		}
		static const char* TypeName()
		{
			return "singstar";
		}
		static std::list<std::string> ListAPIs()
		{
			return RegisterAudioDevice::instance().Names();
		}
		static const TCHAR* LongAPIName(const std::string& name)
		{
			auto proxy = RegisterAudioDevice::instance().Proxy(name);
			if (proxy)
				return proxy->Name();
			return nullptr;
		}
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

	class LogitechMicDevice : public SingstarDevice
	{
	public:
		virtual ~LogitechMicDevice() {}
		static USBDevice* CreateDevice(int port);
		static const char* TypeName()
		{
			return "logitech_usbmic";
		}
		static const TCHAR* Name()
		{
			return TEXT("Logitech USB Mic");
		}
	};

} // namespace usb_mic
#endif
