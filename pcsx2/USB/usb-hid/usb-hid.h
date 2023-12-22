// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "SaveState.h"
#include "USB/qemu-usb/hid.h"
#include <list>
#include <string>

namespace usb_hid
{
	class HIDKbdDevice : public DeviceProxy
	{
	public:
		const char* Name() const override;
		const char* TypeName() const override;
		std::span<const InputBindingInfo> Bindings(u32 subtype) const override;
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		void SetBindingValue(USBDevice* dev, u32 bind, float value) const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
	};

	class HIDMouseDevice final : public DeviceProxy
	{
	public:
		const char* Name() const override;
		const char* TypeName() const override;
		std::span<const InputBindingInfo> Bindings(u32 subtype) const override;
		float GetBindingValue(const USBDevice* dev, u32 bind) const override;
		void SetBindingValue(USBDevice* dev, u32 bind, float value) const override;
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
	};

	class BeatManiaDevice final : public HIDKbdDevice
	{
	public:
		const char* Name() const override;
		const char* TypeName() const override;
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
	};

} // namespace usb_hid
