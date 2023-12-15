/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
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
