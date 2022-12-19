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
