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

#include "usb-python2.h"
#include "python2proxy.h"

namespace usb_python2
{
	namespace noop
	{
		static const char* APINAME = "noop";

		class NOOP : public Python2Input
		{
		public:
			NOOP(int port, const char* dev_type)
				: Python2Input(port, dev_type)
			{
			}
			~NOOP() {}
			int Open() { return 0; }
			int Close() { return 0; }
			int TokenIn(uint8_t* buf, int len) { return len; }
			int TokenOut(const uint8_t* data, int len) { return len; }
			int ReadPacket(std::vector<uint8_t>& data) { return 0; }
			int WritePacket(const std::vector<uint8_t>& data) { return 0; }
			void ReadIo(std::vector<uint8_t>& data) {}
			int Reset() { return 0; }
			bool isPassthrough() { return false; }

			void UpdateKeyStates(std::wstring keybind) {};
			bool GetKeyState(std::wstring keybind) { return false; };
			bool GetKeyStateOneShot(std::wstring keybind) { return false; };
			double GetKeyStateAnalog(std::wstring keybind) { return 0; };
			bool IsKeybindAvailable(std::wstring keybind) { return false; };
			bool IsAnalogKeybindAvailable(std::wstring keybind) { return false; };

			static const TCHAR* Name()
			{
				return TEXT("NOOP");
			}

			static int Configure(int port, const std::string& api, void* data)
			{
				return RESULT_CANCELED;
			}
		};

	} // namespace noop
} // namespace usb_hid
