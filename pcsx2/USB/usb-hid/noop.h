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

#include "usb-hid.h"
#include "hidproxy.h"

namespace usb_hid
{
	namespace noop
	{

		static const char* APINAME = "noop";

		class NOOP : public UsbHID
		{
		public:
			NOOP(int port, const char* dev_type)
				: UsbHID(port, dev_type)
			{
			}
			~NOOP() {}
			int Open() { return 0; }
			int Close() { return 0; }
			int TokenIn(uint8_t* buf, int len) { return len; }
			int TokenOut(const uint8_t* data, int len) { return len; }
			int Reset() { return 0; }

			static const TCHAR* Name()
			{
				return TEXT("NOOP");
			}

			static int Configure(int port, const char* dev_type, HIDType type, void* data)
			{
				return RESULT_CANCELED;
			}
		};

	} // namespace noop
} // namespace usb_hid
