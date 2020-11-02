#ifndef USBHIDPROXY_H
#define USBHIDPROXY_H
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "usb-hid.h"
#include "../helpers.h"
#include "../deviceproxy.h"

namespace usb_hid {

class UsbHIDError : public std::runtime_error
{
public:
	UsbHIDError(const char* msg) : std::runtime_error(msg) {}
	virtual ~UsbHIDError() throw () {}
};

class UsbHIDProxyBase : public ProxyBase
{
	UsbHIDProxyBase(const UsbHIDProxyBase&) = delete;

	public:
	UsbHIDProxyBase() {}
	UsbHIDProxyBase(const std::string& name);
	virtual UsbHID* CreateObject(int port, const char* dev_type) const = 0;
	// ProxyBase::Configure is ignored
	virtual int Configure(int port, const char* dev_type, HIDType hid_type, void *data) = 0;
};

template <class T>
class UsbHIDProxy : public UsbHIDProxyBase
{
	UsbHIDProxy(const UsbHIDProxy&) = delete;

	public:
	UsbHIDProxy() {}
	UsbHIDProxy(const std::string& name): UsbHIDProxyBase(name) {}
	UsbHID* CreateObject(int port, const char* dev_type) const
	{
		try
		{
			return new T(port, dev_type);
		}
		catch(UsbHIDError& err)
		{
			(void)err;
			return nullptr;
		}
	}
	virtual const TCHAR* Name() const
	{
		return T::Name();
	}
	virtual int Configure(int port, const char* dev_type, void *data)
	{
		return RESULT_CANCELED;
	}
	virtual int Configure(int port, const char* dev_type, HIDType hid_type, void *data)
	{
		return T::Configure(port, dev_type, hid_type, data);
	}
};

class RegisterUsbHID : public RegisterProxy<UsbHIDProxyBase>
{
	public:
	static void Register();
};

}
#endif