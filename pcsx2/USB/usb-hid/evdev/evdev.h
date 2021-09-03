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
#include "USB/usb-hid/usb-hid.h"
#include "USB/linux/util.h"
#include "common/Console.h"
#include <linux/input.h>
#include <unistd.h>
#include <dirent.h>
#include <thread>
#include <atomic>

namespace usb_hid
{
	namespace evdev
	{

		static const char* APINAME = "evdev";

		class EvDev : public UsbHID
		{
		public:
			EvDev(int port, const char* dev_type)
				: UsbHID(port, dev_type)
				, mHandle(-1)
				, mReaderThreadIsRunning(false)
			{
			}

			~EvDev() { Close(); }
			int Open();
			int Close();
			int TokenIn(uint8_t* buf, int len);
			int TokenOut(const uint8_t* data, int len);
			int Reset() { return 0; }

			static const TCHAR* Name()
			{
				return TEXT("Evdev");
			}

			static int Configure(int port, const char* dev_type, HIDType hid_type, void* data);

		protected:
			void ReaderThread();

			int mHandle;

			std::thread mReaderThread;
			std::atomic<bool> mReaderThreadIsRunning;
		};

		template <size_t _Size>
		bool GetEvdevName(const std::string& path, char (&name)[_Size])
		{
			int fd = 0;
			if ((fd = open(path.c_str(), O_RDONLY)) < 0)
			{
				Console.Warning("Cannot open %s\n", path.c_str());
			}
			else
			{
				if (ioctl(fd, EVIOCGNAME(_Size), name) < -1)
				{
					Console.Warning("Cannot get controller's name\n");
					close(fd);
					return false;
				}
				close(fd);
				return true;
			}
			return false;
		}

	} // namespace evdev
} // namespace usb_hid
