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

#ifndef PADPROXY_H
#define PADPROXY_H
#include <memory>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "usb-pad.h"
#include "USB/helpers.h"
#include "USB/deviceproxy.h"

namespace usb_pad
{

	class PadError : public std::runtime_error
	{
	public:
		PadError(const char* msg)
			: std::runtime_error(msg)
		{
		}
		virtual ~PadError() {}
	};

	class PadProxyBase : public ProxyBase
	{
		PadProxyBase(const PadProxyBase&) = delete;

	public:
		PadProxyBase() {}
		PadProxyBase(const std::string& name);
		virtual Pad* CreateObject(int port, const char* dev_type) const = 0;
	};

	template <class T>
	class PadProxy : public PadProxyBase
	{
		PadProxy(const PadProxy&) = delete;

	public:
		PadProxy() {  }
		~PadProxy() {  }
		Pad* CreateObject(int port, const char* dev_type) const
		{
			try
			{
				return new T(port, dev_type);
			}
			catch (PadError& err)
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

	class RegisterPad : public RegisterProxy<PadProxyBase>
	{
	public:
		static void Register();
	};

} // namespace usb_pad
#endif
