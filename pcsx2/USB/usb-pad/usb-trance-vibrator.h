// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "SaveState.h"
#include "USB/qemu-usb/desc.h"
#include <list>
#include <string>

namespace usb_pad
{
	struct TranceVibratorState
	{
		TranceVibratorState(u32 port_);
		~TranceVibratorState();

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;
	};

	class TranceVibratorDevice final : public DeviceProxy
	{
	public:
		const char* Name() const override;
		const char* TypeName() const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
		float GetBindingValue(const USBDevice* dev, u32 bind_index) const override;
		void SetBindingValue(USBDevice* dev, u32 bind_index, float value) const override;
		std::span<const InputBindingInfo> Bindings(u32 subtype) const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
	};

} // namespace usb_pad
