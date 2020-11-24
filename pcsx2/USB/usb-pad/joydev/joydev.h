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
#include "../../linux/util.h"
#include "../evdev/evdev-ff.h"
#include "../evdev/shared.h"
#include "Utilities/Console.h"

namespace usb_pad
{
	namespace joydev
	{

		void EnumerateDevices(device_list& list);

		static constexpr const char* APINAME = "joydev";

		class JoyDevPad : public Pad
		{
		public:
			JoyDevPad(int port, const char* dev_type)
				: Pad(port, dev_type)
			{
			}

			~JoyDevPad() { Close(); }
			int Open();
			int Close();
			int TokenIn(uint8_t* buf, int len);
			int TokenOut(const uint8_t* data, int len);
			int Reset() { return 0; }

			static const TCHAR* Name()
			{
				return "Joydev";
			}

			static int Configure(int port, const char* dev_type, void* data);

		protected:
			int mHandleFF = -1;
			struct wheel_data_t mWheelData
			{
			};
			std::vector<evdev::device_data> mDevices;
		};

		template <size_t _Size>
		bool GetJoystickName(const std::string& path, char (&name)[_Size])
		{
			int fd = 0;
			if ((fd = open(path.c_str(), O_RDONLY)) < 0)
			{
				Console.Warning("Cannot open %s\n", path.c_str());
			}
			else
			{
				if (ioctl(fd, JSIOCGNAME(_Size), name) < -1)
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

	} // namespace joydev
} // namespace usb_pad
