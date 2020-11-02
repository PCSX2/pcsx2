#include "deviceproxy.h"
#include "usb-pad/padproxy.h"
#include "usb-mic/audiodeviceproxy.h"
#include "usb-hid/hidproxy.h"
#include "usb-eyetoy/videodeviceproxy.h"

RegisterDevice *RegisterDevice::registerDevice = nullptr;

void RegisterAPIs()
{
	usb_pad::RegisterPad::Register();
	usb_mic::RegisterAudioDevice::Register();
	usb_hid::RegisterUsbHID::Register();
	usb_eyetoy::RegisterVideoDevice::Register();
}

void UnregisterAPIs()
{
	usb_pad::RegisterPad::instance().Clear();
	usb_mic::RegisterAudioDevice::instance().Clear();
	usb_hid::RegisterUsbHID::instance().Clear();
	usb_eyetoy::RegisterVideoDevice::instance().Clear();
}