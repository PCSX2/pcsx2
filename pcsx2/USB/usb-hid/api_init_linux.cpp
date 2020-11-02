#include "hidproxy.h"
#include "evdev/evdev.h"
#include "noop.h"

void usb_hid::RegisterUsbHID::Register()
{
	auto& inst = RegisterUsbHID::instance();
	inst.Add(usb_hid::evdev::APINAME, new UsbHIDProxy<usb_hid::evdev::EvDev>());
	inst.Add(usb_hid::noop::APINAME, new UsbHIDProxy<usb_hid::noop::NOOP>());
}