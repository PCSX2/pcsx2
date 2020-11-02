#ifndef PADPROXY_H
#define PADPROXY_H
#include <memory>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "usb-pad.h"
#include "../helpers.h"
#include "../deviceproxy.h"

namespace usb_pad {

class PadError : public std::runtime_error
{
public:
	PadError(const char* msg) : std::runtime_error(msg) {}
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
	PadProxy() { OSDebugOut(TEXT("%s\n"), T::Name()); }
	~PadProxy() { OSDebugOut(TEXT("%p\n"), this); }
	Pad* CreateObject(int port, const char *dev_type) const
	{
		try
		{
			return new T(port, dev_type);
		}
		catch(PadError& err)
		{
			(void)err;
			return nullptr;
		}
	}
	virtual const TCHAR* Name() const
	{
		return T::Name();
	}
	virtual int Configure(int port, const char *dev_type, void *data)
	{
		return T::Configure(port, dev_type, data);
	}
};

class RegisterPad : public RegisterProxy<PadProxyBase>
{
	public:
	static void Register();
};

} //namespace
#endif
