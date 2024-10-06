// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "SaveState.h"
#include "USB/qemu-usb/desc.h"
#include <list>
#include <string>

namespace usb_pad
{
	enum RealPlayControlID
	{
		CID_RP_DPAD_UP,
		CID_RP_DPAD_DOWN,
		CID_RP_DPAD_LEFT,
		CID_RP_DPAD_RIGHT,
		CID_RP_RED,
		CID_RP_GREEN,
		CID_RP_YELLOW,
		CID_RP_BLUE,
		CID_RP_ACC_X,
		CID_RP_ACC_Y,
		CID_RP_ACC_Z,
		CID_RP_COUNT,
	};

	enum RealPlayType
	{
		REALPLAY_RACING,
		REALPLAY_SPHERE,
		REALPLAY_GOLF,
		REALPLAY_POOL,
	};

	struct RealPlayState
	{
		RealPlayState(u32 port_, u32 type_);
		~RealPlayState();

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;
		u32 type = 0;
		u8 state = 0;

		// Settings
		bool invert_x_axis = false;
		bool invert_y_axis = false;
		bool invert_z_axis = false;

		#pragma pack(push, 1)
		struct
		{
			u16 acc_x;
			u16 acc_y;
			u16 acc_z;

			u32 : 32;
			u32 : 32;

			u8 dpad_up : 1;
			u8 dpad_down : 1;
			u8 dpad_left : 1;
			u8 dpad_right : 1;
			u8 btn_red : 1;
			u8 btn_green: 1;
			u8 btn_yellow : 1;
			u8 btn_blue : 1;

			u32 : 32;
		} data = {};
		#pragma pack(pop)
	};

	class RealPlayDevice final : public DeviceProxy
	{
	public:
		const char* Name() const override;
		const char* TypeName() const override;
		float GetBindingValue(const USBDevice* dev, u32 bind_index) const override;
		void SetBindingValue(USBDevice* dev, u32 bind_index, float value) const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
		void UpdateSettings(USBDevice* dev, SettingsInterface& si) const override;
		std::span<const char*> SubTypes() const override;
		std::span<const InputBindingInfo> Bindings(u32 subtype) const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
	};
} // namespace usb_pad
