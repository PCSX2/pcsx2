#include "python2proxy.h"
#include "noop.h"
#include "inputs/passthrough/usb-python2-passthrough.h"

#ifdef PCSX2_CORE
#include "inputs/native/usb-python2-native.h"
#endif

void usb_python2::RegisterUsbPython2::Register()
{
	auto& inst = RegisterUsbPython2::instance();
#ifdef PCSX2_CORE
	inst.Add(usb_python2::native::APINAME, new UsbPython2Proxy<usb_python2::native::NativeInput>());
#else
	inst.Add(usb_python2::noop::APINAME, new UsbPython2Proxy<usb_python2::noop::NOOP>());
#endif
	inst.Add(usb_python2::passthrough::APINAME, new UsbPython2Proxy<usb_python2::passthrough::PassthroughInput>());
}