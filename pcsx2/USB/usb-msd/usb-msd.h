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

#ifndef USBMSD_H
#define USBMSD_H
#include "USB/deviceproxy.h"

namespace usb_msd
{

	static const char* APINAME = "cstdio";

	class MsdDevice
	{
	public:
		virtual ~MsdDevice() {}
		static USBDevice* CreateDevice(int port);
		static const char* TypeName();
		static const TCHAR* Name()
		{
			return TEXT("Mass storage device");
		}
		static std::list<std::string> ListAPIs()
		{
			return std::list<std::string>{APINAME};
		}
		static const TCHAR* LongAPIName(const std::string& name)
		{
			return TEXT("cstdio");
		}
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

} // namespace usb_msd
#endif
