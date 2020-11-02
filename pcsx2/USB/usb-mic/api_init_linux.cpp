#include "audiodeviceproxy.h"
#include "audiodev-noop.h"
#include "audiodev-pulse.h"

void usb_mic::RegisterAudioDevice::Register()
{
	auto& inst = RegisterAudioDevice::instance();
	inst.Add(audiodev_noop::APINAME, new AudioDeviceProxy<audiodev_noop::NoopAudioDevice>());
	inst.Add(audiodev_pulse::APINAME, new AudioDeviceProxy<audiodev_pulse::PulseAudioDevice>());
}