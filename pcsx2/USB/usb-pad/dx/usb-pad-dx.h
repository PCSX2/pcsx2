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

#include "USB/usb-pad/padproxy.h"
#include "USB/Win32/Config_usb.h"

namespace usb_pad
{
	namespace dx
	{

		static const char* APINAME = "dinput";

		class DInputPad : public Pad
		{
		public:
			DInputPad(int port, const char* dev_type)
				: Pad(port, dev_type)
				, mUseRamp(0)
			{
			}
			~DInputPad();
			int Open();
			int Close();
			int TokenIn(uint8_t* buf, int len);
			int TokenOut(const uint8_t* data, int len);
			int Reset() { return 0; }

			static const TCHAR* Name()
			{
				return TEXT("DInput");
			}

			static int Configure(int port, const char* dev_type, void* data);

		private:
			int32_t mUseRamp;
		};

	} // namespace dx
} // namespace usb_pad
