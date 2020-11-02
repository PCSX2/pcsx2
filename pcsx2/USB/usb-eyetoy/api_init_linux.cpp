#include "videodeviceproxy.h"
#include "cam-linux.h"

void usb_eyetoy::RegisterVideoDevice::Register()
{
	auto& inst = RegisterVideoDevice::instance();
	inst.Add(linux_api::APINAME, new VideoDeviceProxy<linux_api::V4L2>());
}
