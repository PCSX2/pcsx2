#include "padproxy.h"
#include "evdev/evdev.h"
#include "joydev/joydev.h"

void usb_pad::RegisterPad::Register()
{
	auto& inst = RegisterPad::instance();
	inst.Add("evdev", new PadProxy<evdev::EvDevPad>());
	inst.Add("joydev", new PadProxy<joydev::JoyDevPad>());
}