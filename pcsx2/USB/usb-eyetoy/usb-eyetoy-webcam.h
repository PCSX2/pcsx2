#ifndef USBEYETOYWEBCAM_H
#define USBEYETOYWEBCAM_H

#include "../qemu-usb/vl.h"
#include "../configuration.h"
#include "../deviceproxy.h"
#include "videodeviceproxy.h"
#include <mutex>


namespace usb_eyetoy {

class EyeToyWebCamDevice
{
public:
	virtual ~EyeToyWebCamDevice() {}
	static USBDevice* CreateDevice(int port);
	static const TCHAR* Name()
	{
		return TEXT("EyeToy");
	}
	static const char* TypeName()
	{
		return "eyetoy";
	}
	static std::list<std::string> ListAPIs()
	{
		return RegisterVideoDevice::instance().Names();
	}
	static const TCHAR* LongAPIName(const std::string& name)
	{
		auto proxy = RegisterVideoDevice::instance().Proxy(name);
		if (proxy)
			return proxy->Name();
		return nullptr;
	}
	static int Configure(int port, const std::string& api, void *data);
	static int Freeze(int mode, USBDevice *dev, void *data);
};

} //namespace

#endif
