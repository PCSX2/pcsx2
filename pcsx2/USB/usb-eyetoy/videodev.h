// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "USB/qemu-usb/qusb.h"
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace usb_eyetoy
{
	enum FrameFormat
	{
		format_mpeg,
		format_jpeg,
		format_yuv400
	};

	enum DeviceType
	{
		TYPE_EYETOY,
		TYPE_OV511P,
	};

	class VideoDevice
	{
	public:
		virtual ~VideoDevice() {}
		virtual int Open(int width, int height, FrameFormat format, int mirror) = 0;
		virtual int Close() = 0;
		virtual int GetImage(uint8_t* buf, size_t len) = 0;
		virtual void SetMirroring(bool state) = 0;
		virtual int Reset() = 0;

		virtual const std::string& HostDevice() const { return mHostDevice; }
		virtual void HostDevice(std::string dev) { mHostDevice = std::move(dev); }

		static std::unique_ptr<VideoDevice> CreateInstance();
		static std::vector<std::pair<std::string, std::string>> GetDeviceList();

	protected:
		std::string mHostDevice;
	};
} // namespace usb_eyetoy
