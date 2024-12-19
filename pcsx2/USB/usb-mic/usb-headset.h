// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "USB/deviceproxy.h"

namespace usb_mic
{
	class HeadsetDevice final : public DeviceProxy
	{
	public:
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		const char* TypeName() const override;
		const char* Name() const override;

		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
		void UpdateSettings(USBDevice* dev, SettingsInterface& si) const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
	};

} // namespace usb_mic
