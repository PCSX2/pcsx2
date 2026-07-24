// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "USB/qemu-usb/qusb.h"
#include "USB/qemu-usb/desc.h"
#include "USB/deviceproxy.h"

namespace usb_pad
{
	class SeamicDevice final : public DeviceProxy
	{
		enum Seamic_ControlID
		{
			STICK_UP,
			STICK_DOWN,
			STICK_LEFT,
			STICK_RIGHT,
			BTN_A,
			BTN_B,
			BTN_C,
			BTN_X,
			BTN_Y,
			BTN_Z,
			BTN_L,
			BTN_R,
			SELECT,
			START,
			DPAD_UP,
			DPAD_DOWN,
			DPAD_LEFT,
			DPAD_RIGHT,
		};

	public:
		const char* Name() const override;
		const char* TypeName() const override;
		const char* IconName() const override;
		std::span<const InputBindingInfo> Bindings(u32 subtype) const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		float GetBindingValue(const USBDevice* dev, u32 bind_index) const override;
		void SetBindingValue(USBDevice* dev, u32 bind_index, float value) const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
	};

	struct SeamicState
	{
		SeamicState(u32 port_);
		~SeamicState();
		
		void UpdateStick() noexcept;
		u8 UpdateHatSwitch() noexcept;

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;
		USBDevice* mic;
		
		u8 stick_u, stick_d, stick_l, stick_r;
		bool hat_up, hat_down, hat_left, hat_right;

		struct
		{
			u8 stick_x;
			u8 stick_y;
			u8 : 8;

			u8 dpad : 4;
			u8 btn_a : 1;
			u8 btn_b : 1;
			u8 btn_c : 1;
			u8 btn_x : 1;

			u8 btn_y : 1;
			u8 btn_z : 1;
			u8 btn_l : 1;
			u8 btn_r : 1;
			u8 select : 1;
			u8 start : 1;
			u8 : 2;
		} data;
	};
} // namespace usb_pad
