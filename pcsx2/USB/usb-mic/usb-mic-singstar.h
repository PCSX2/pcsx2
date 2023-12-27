// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "USB/deviceproxy.h"
#include "USB/usb-mic/audiodev.h"

namespace usb_mic
{
	class SingstarDevice : public DeviceProxy
	{
	public:
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype, bool dual_mic, const char* devtype) const;
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		const char* Name() const override;
		const char* TypeName() const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
		void UpdateSettings(USBDevice* dev, SettingsInterface& si) const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
	};

	class LogitechMicDevice final : public SingstarDevice
	{
	public:
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		const char* TypeName() const override;
		const char* Name() const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
	};
} // namespace usb_mic
