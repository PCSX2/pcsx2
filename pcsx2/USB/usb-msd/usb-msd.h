#ifndef USBMSD_H
#define USBMSD_H
#include "../deviceproxy.h"

namespace usb_msd {

static const char *APINAME = "cstdio";

class MsdDevice
{
public:
	virtual ~MsdDevice() {}
	static USBDevice* CreateDevice(int port);
	static const char* TypeName();
	static const TCHAR* Name()
	{
		return TEXT("Mass storage device");
	}
	static std::list<std::string> ListAPIs()
	{
		return std::list<std::string> { APINAME };
	}
	static const TCHAR* LongAPIName(const std::string& name)
	{
		return TEXT("cstdio");
	}
	static int Configure(int port, const std::string& api, void *data);
	static int Freeze(int mode, USBDevice *dev, void *data);
};

}
#endif