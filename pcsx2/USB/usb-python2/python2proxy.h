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

#ifndef USBPYTHON2PROXY_H
#define USBPYTHON2PROXY_H
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "usb-python2.h"
#include "USB/helpers.h"
#include "USB/deviceproxy.h"

namespace usb_python2
{

	class UsbPython2Error : public std::runtime_error
	{
	public:
		UsbPython2Error(const char* msg)
			: std::runtime_error(msg)
		{
		}
		virtual ~UsbPython2Error() throw() {}
	};

	class UsbPython2ProxyBase : public ProxyBase
	{
		UsbPython2ProxyBase(const UsbPython2ProxyBase&) = delete;

	public:
		UsbPython2ProxyBase() {}
		UsbPython2ProxyBase(const std::string& name);
		virtual Python2Input* CreateObject(int port, const char* dev_type) const = 0;
		// ProxyBase::Configure is ignored
		virtual int Configure(int port, const char* dev_type, void* data) = 0;
	};

	template <class T>
	class UsbPython2Proxy : public UsbPython2ProxyBase
	{
		UsbPython2Proxy(const UsbPython2Proxy&) = delete;

	public:
		UsbPython2Proxy() {}
		UsbPython2Proxy(const std::string& name)
			: UsbPython2ProxyBase(name)
		{
		}
		Python2Input* CreateObject(int port, const char* dev_type) const
		{
			try
			{
				return new T(port, dev_type);
			}
			catch (UsbPython2Error& err)
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

	class RegisterUsbPython2 : public RegisterProxy<UsbPython2ProxyBase>
	{
	public:
		static void Register();
	};

} // namespace usb_python2
#endif
