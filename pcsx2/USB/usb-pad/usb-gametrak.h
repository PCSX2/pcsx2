// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "SaveState.h"
#include "USB/qemu-usb/desc.h"
#include <list>
#include <string>

namespace usb_pad
{
	enum GametrakControlID
	{
		CID_GT_BUTTON,
		CID_GT_LEFT_X,
		CID_GT_LEFT_Y,
		CID_GT_LEFT_Z,
		CID_GT_RIGHT_X,
		CID_GT_RIGHT_Y,
		CID_GT_RIGHT_Z,
		CID_GT_COUNT,
	};

	struct GametrakState
	{
		GametrakState(u32 port_);
		~GametrakState();

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;
		u8 state = 0;
		u32 key = 0;

		// Settings
		bool invert_x_axis = false;
		bool invert_y_axis = false;
		bool invert_z_axis = false;
		u16 limit_z_axis = 0xfff;

		std::chrono::steady_clock::time_point last_log;

		#pragma pack(push, 1)
		struct
		{
			u16 k1 : 1;
			u16 left_x : 15;
			u16 k2 : 1;
			u16 left_y : 15;
			u16 k3 : 1;
			u16 left_z : 15;
			u16 k4 : 1;
			u16 right_x : 15;
			u16 k5 : 1;
			u16 right_y : 15;
			u16 k6 : 1;
			u16 right_z : 15;

			u8 : 4;
			u8 button : 1;
			u8 : 3;

			u8 : 8;
			u8 : 8;
			u8 : 8;
		} data = {};
		#pragma pack(pop)
	};

	class GametrakDevice final : public DeviceProxy
	{
	public:
		const char* Name() const override;
		const char* TypeName() const override;
		float GetBindingValue(const USBDevice* dev, u32 bind_index) const override;
		void SetBindingValue(USBDevice* dev, u32 bind_index, float value) const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
		void UpdateSettings(USBDevice* dev, SettingsInterface& si) const override;
		std::span<const InputBindingInfo> Bindings(u32 subtype) const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
	};

} // namespace usb_pad
