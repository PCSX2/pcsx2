#include "padproxy.h"
#include "raw/usb-pad-raw.h"
#include "dx/usb-pad-dx.h"

void usb_pad::RegisterPad::Register()
{
	auto& inst = RegisterPad::instance();
	inst.Add(raw::APINAME, new PadProxy<raw::RawInputPad>());
	inst.Add(dx::APINAME, new PadProxy<dx::DInputPad>());
	OSDebugOut(TEXT("yep!\n"));
}