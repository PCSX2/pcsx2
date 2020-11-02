#include "../deviceproxy.h"
#include "audiodeviceproxy.h"

namespace usb_mic {

class HeadsetDevice
{
public:
    virtual ~HeadsetDevice() {}
    static USBDevice* CreateDevice(int port);
    static USBDevice* CreateDevice(int port, const std::string& api);
    static const char* TypeName()
    {
        return "headset";
    }
    static const TCHAR* Name()
    {
        return TEXT("Logitech USB Headset");
    }
    static std::list<std::string> ListAPIs();
    static const TCHAR* LongAPIName(const std::string& name);
    static int Configure(int port, const std::string& api, void *data);
    static int Freeze(int mode, USBDevice *dev, void *data);
};

}