#include "videodeviceproxy.h"
#include "cam-windows.h"

void usb_eyetoy::RegisterVideoDevice::Register()
{
	auto& inst = RegisterVideoDevice::instance();
	inst.Add(windows_api::APINAME, new VideoDeviceProxy<windows_api::DirectShow>());
}
