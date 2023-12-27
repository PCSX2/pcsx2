// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
