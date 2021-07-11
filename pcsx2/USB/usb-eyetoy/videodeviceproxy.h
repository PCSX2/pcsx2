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

#ifndef VIDEODEVICEPROXY_H
#define VIDEODEVICEPROXY_H
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "videodev.h"
#include "USB/helpers.h"
#include "USB/deviceproxy.h"

namespace usb_eyetoy
{

	class VideoDeviceError : public std::runtime_error
	{
	public:
		VideoDeviceError(const char* msg)
			: std::runtime_error(msg)
		{
		}
		virtual ~VideoDeviceError() throw() {}
	};

	class VideoDeviceProxyBase : public ProxyBase
	{
		VideoDeviceProxyBase(const VideoDeviceProxyBase&) = delete;

	public:
		VideoDeviceProxyBase() {}
		VideoDeviceProxyBase(const std::string& name);
		virtual VideoDevice* CreateObject(int port) const = 0;
	};

	template <class T>
	class VideoDeviceProxy : public VideoDeviceProxyBase
	{
		VideoDeviceProxy(const VideoDeviceProxy&) = delete;

	public:
		VideoDeviceProxy() {}
		VideoDeviceProxy(const std::string& name)
			: VideoDeviceProxyBase(name)
		{
		}
		VideoDevice* CreateObject(int port) const
		{
			try
			{
				return new T(port);
			}
			catch (VideoDeviceError& err)
			{
				(void)err;
				return nullptr;
			}
		}
		virtual const TCHAR* Name() const
		{
			return T::Name();
		}
		virtual int Configure(int port, const char* dev_type, void* data)
		{
			return T::Configure(port, dev_type, data);
		}
	};

	class RegisterVideoDevice : public RegisterProxy<VideoDeviceProxyBase>
	{
	public:
		static void Register();
	};

} // namespace usb_eyetoy
#endif
