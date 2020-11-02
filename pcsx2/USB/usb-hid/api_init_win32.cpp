#include "hidproxy.h"
#include "raw/rawinput.h"
#include "noop.h"

void usb_hid::RegisterUsbHID::Register()
{
	auto& inst = RegisterUsbHID::instance();
	inst.Add(usb_hid::raw::APINAME, new UsbHIDProxy<usb_hid::raw::RawInput>());
	inst.Add(usb_hid::noop::APINAME, new UsbHIDProxy<usb_hid::noop::NOOP>());
}