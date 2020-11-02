#ifndef USBMICSINGSTAR_H
#define USBMICSINGSTAR_H
#include "../deviceproxy.h"
#include "audiodeviceproxy.h"

struct USBDevice;

namespace usb_mic {
class SingstarDevice
{
public:
	virtual ~SingstarDevice() {}
	static USBDevice* CreateDevice(int port);
	static USBDevice* CreateDevice(int port, const std::string& api);
	static const TCHAR* Name()
	{
		return TEXT("Singstar");
	}
	static const char* TypeName()
	{
		return "singstar";
	}
	static std::list<std::string> ListAPIs()
	{
		return RegisterAudioDevice::instance().Names();
	}
	static const TCHAR* LongAPIName(const std::string& name)
	{
		auto proxy = RegisterAudioDevice::instance().Proxy(name);
		if (proxy)
			return proxy->Name();
		return nullptr;
	}
	static int Configure(int port, const std::string& api, void *data);
	static int Freeze(int mode, USBDevice *dev, void *data);
};

class LogitechMicDevice : public SingstarDevice
{
public:
	virtual ~LogitechMicDevice() {}
	static USBDevice* CreateDevice(int port);
	static const char* TypeName()
	{
		return "logitech_usbmic";
	}
	static const TCHAR* Name()
	{
		return TEXT("Logitech USB Mic");
	}
};

}
#endif
