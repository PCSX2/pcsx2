// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "deviceproxy.h"
#include "usb-eyetoy/usb-eyetoy-webcam.h"
#include "usb-hid/usb-hid.h"
#include "usb-mic/usb-headset.h"
#include "usb-mic/usb-mic-singstar.h"
#include "usb-msd/usb-msd.h"
#include "usb-pad/usb-pad.h"
#include "usb-pad/usb-turntable.h"
#include "usb-printer/usb-printer.h"
#include "usb-lightgun/guncon2.h"

RegisterDevice* RegisterDevice::registerDevice = nullptr;

DeviceProxy::~DeviceProxy() = default;

std::span<const char*> DeviceProxy::SubTypes() const
{
	return {};
}

std::span<const InputBindingInfo> DeviceProxy::Bindings(u32 subtype) const
{
	return {};
}

std::span<const SettingInfo> DeviceProxy::Settings(u32 subtype) const
{
	return {};
}

float DeviceProxy::GetBindingValue(const USBDevice* dev, u32 bind) const
{
	return 0.0f;
}

void DeviceProxy::SetBindingValue(USBDevice* dev, u32 bind, float value) const
{
}

bool DeviceProxy::Freeze(USBDevice* dev, StateWrapper& sw) const
{
	return false;
}

void DeviceProxy::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
{
}

void DeviceProxy::InputDeviceConnected(USBDevice* dev, const std::string_view& identifier) const
{
}

void DeviceProxy::InputDeviceDisconnected(USBDevice* dev, const std::string_view& identifier) const
{
}

void RegisterDevice::Register()
{
	auto& inst = RegisterDevice::instance();
	if (inst.Map().size()) // FIXME Don't clear proxies, singstar keeps a copy to uninit audio
		return;
	inst.Add(DEVTYPE_PAD, new usb_pad::PadDevice());
	inst.Add(DEVTYPE_MSD, new usb_msd::MsdDevice());
	inst.Add(DEVTYPE_SINGSTAR, new usb_mic::SingstarDevice());
	inst.Add(DEVTYPE_LOGITECH_MIC, new usb_mic::LogitechMicDevice());
	inst.Add(DEVTYPE_LOGITECH_HEADSET, new usb_mic::HeadsetDevice());
	inst.Add(DEVTYPE_HIDKBD, new usb_hid::HIDKbdDevice());
	inst.Add(DEVTYPE_HIDMOUSE, new usb_hid::HIDMouseDevice());
	inst.Add(DEVTYPE_RBKIT, new usb_pad::RBDrumKitDevice());
	inst.Add(DEVTYPE_DJ, new usb_pad::DJTurntableDevice());
	inst.Add(DEVTYPE_BUZZ, new usb_pad::BuzzDevice());
	inst.Add(DEVTYPE_EYETOY, new usb_eyetoy::EyeToyWebCamDevice());
	inst.Add(DEVTYPE_BEATMANIA_DADADA, new usb_hid::BeatManiaDevice());
	inst.Add(DEVTYPE_SEGA_SEAMIC, new usb_pad::SeamicDevice());
	inst.Add(DEVTYPE_PRINTER, new usb_printer::PrinterDevice());
	inst.Add(DEVTYPE_KEYBOARDMANIA, new usb_pad::KeyboardmaniaDevice());
	inst.Add(DEVTYPE_GUNCON2, new usb_lightgun::GunCon2Device());
}

void RegisterDevice::Unregister()
{
	registerDeviceMap.clear();
	delete registerDevice;
	registerDevice = nullptr;
}
