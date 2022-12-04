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

#include "PrecompiledHeader.h"

#include "common/StringUtil.h"

#include "videodev.h"
#include "usb-eyetoy-webcam.h"
#include "jo_mpeg.h"
#include "USB/USB.h"

namespace usb_eyetoy
{
	namespace noop_api
	{
		class NoopVideoDevice final : public VideoDevice
		{
		public:
			int Open(int width, int height, FrameFormat format, int mirror) override
			{
				return -1;
			}

			int Close() override
			{
				return 0;
			}

			int GetImage(uint8_t* buf, size_t len) override
			{
				return 0;
			}

			void SetMirroring(bool state) override
			{
			}

			int Reset() override
			{
				return 0;
			}
		};
	} // namespace noop_api

	std::unique_ptr<VideoDevice> VideoDevice::CreateInstance()
	{
		return std::make_unique<noop_api::NoopVideoDevice>();
	}

	std::vector<std::pair<std::string, std::string>> VideoDevice::GetDeviceList()
	{
		return {};
	}
} // namespace usb_eyetoy
