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
