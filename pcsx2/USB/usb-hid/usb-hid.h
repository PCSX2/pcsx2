#pragma once
#include "../configuration.h"
#include "../qemu-usb/hid.h"
#include <list>
#include <string>

namespace usb_hid {

enum HIDType
{
	HIDTYPE_KBD,
	HIDTYPE_MOUSE,
};

class UsbHID
{
public:
	UsbHID(int port, const char* dev_type) : mPort(port), mDevType(dev_type) {}
	virtual ~UsbHID() {}
	virtual int Open() = 0;
	virtual int Close() = 0;
//	virtual int TokenIn(uint8_t *buf, int len) = 0;
	virtual int TokenOut(const uint8_t *data, int len) = 0;
	virtual int Reset() = 0;

	virtual int Port() { return mPort; }
	virtual void Port(int port) { mPort = port; }
	virtual void SetHIDState(HIDState *hs) { mHIDState = hs; }
	virtual void SetHIDType(HIDType t) { mHIDType = t; }

protected:
	int mPort;
	HIDState *mHIDState;
	HIDType mHIDType;
	const char *mDevType;
};

class HIDKbdDevice
{
public:
    virtual ~HIDKbdDevice() {}
    static USBDevice* CreateDevice(int port);
    static const TCHAR* Name()
    {
        return TEXT("HID Keyboard");
    }
    static const char* TypeName()
    {
        return "hidkbd";
    }
    static std::list<std::string> ListAPIs();
    static const TCHAR* LongAPIName(const std::string& name);
    static int Configure(int port, const std::string& api, void *data);
    static int Freeze(int mode, USBDevice *dev, void *data);
};

class HIDMouseDevice
{
public:
    virtual ~HIDMouseDevice() {}
    static USBDevice* CreateDevice(int port);
    static const TCHAR* Name()
    {
        return TEXT("HID Mouse");
    }
    static const char* TypeName()
    {
        return "hidmouse";
    }
    static std::list<std::string> ListAPIs();
    static const TCHAR* LongAPIName(const std::string& name);
    static int Configure(int port, const std::string& api, void *data);
    static int Freeze(int mode, USBDevice *dev, void *data);
};

}
