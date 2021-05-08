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

#include "videodev.h"

namespace usb_eyetoy
{
	namespace linux_api
	{

		typedef struct _buffer_t
		{
			void* start;
			size_t length;
		} buffer_t;

		static const char* APINAME = "V4L2";

		class V4L2 : public VideoDevice
		{
		public:
			V4L2(int port);
			~V4L2();
			int Open(int width, int height, FrameFormat format, int mirror);
			int Close();
			int GetImage(uint8_t* buf, size_t len);
			void SetMirroring(bool state);
			int Reset() { return 0; };

			static const TCHAR* Name()
			{
				return TEXT("V4L2");
			}
			static int Configure(int port, const char* dev_type, void* data);

			int Port() { return mPort; }
			void Port(int port) { mPort = port; }

		private:
			int mPort;
		};

	} // namespace linux_api
} // namespace usb_eyetoy
