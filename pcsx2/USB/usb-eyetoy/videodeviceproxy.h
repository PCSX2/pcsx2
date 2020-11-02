#ifndef VIDEODEVICEPROXY_H
#define VIDEODEVICEPROXY_H
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "videodev.h"
#include "../helpers.h"
#include "../deviceproxy.h"

namespace usb_eyetoy {

class VideoDeviceError : public std::runtime_error
{
public:
	VideoDeviceError(const char* msg) : std::runtime_error(msg) {}
	virtual ~VideoDeviceError() throw () {}
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
	VideoDeviceProxy(const std::string& name): VideoDeviceProxyBase(name) {}
	VideoDevice* CreateObject(int port) const
	{
		try
		{
			return new T(port);
		}
		catch(VideoDeviceError& err)
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

class RegisterVideoDevice : public RegisterProxy<VideoDeviceProxyBase>
{
	public:
	static void Register();
};

} //namespace
#endif
