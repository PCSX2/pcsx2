// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "SaveState.h"
#include "USB/qemu-usb/desc.h"
#include <list>
#include <string>

namespace usb_pad
{
	enum BuzzControlID
	{
		CID_BUZZ_PLAYER1_RED,
		CID_BUZZ_PLAYER1_BLUE,
		CID_BUZZ_PLAYER1_ORANGE,
		CID_BUZZ_PLAYER1_GREEN,
		CID_BUZZ_PLAYER1_YELLOW,
		CID_BUZZ_PLAYER2_RED,
		CID_BUZZ_PLAYER2_BLUE,
		CID_BUZZ_PLAYER2_ORANGE,
		CID_BUZZ_PLAYER2_GREEN,
		CID_BUZZ_PLAYER2_YELLOW,
		CID_BUZZ_PLAYER3_RED,
		CID_BUZZ_PLAYER3_BLUE,
		CID_BUZZ_PLAYER3_ORANGE,
		CID_BUZZ_PLAYER3_GREEN,
		CID_BUZZ_PLAYER3_YELLOW,
		CID_BUZZ_PLAYER4_RED,
		CID_BUZZ_PLAYER4_BLUE,
		CID_BUZZ_PLAYER4_ORANGE,
		CID_BUZZ_PLAYER4_GREEN,
		CID_BUZZ_PLAYER4_YELLOW,
		CID_BUZZ_COUNT,
	};

	struct BuzzState
	{
		BuzzState(u32 port_);
		~BuzzState();

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;

		#pragma pack(push, 1)
		struct
		{
			u8 head1;
			u8 head2;

			u8 player1_red : 1;
			u8 player1_yellow : 1;
			u8 player1_green : 1;
			u8 player1_orange : 1;
			u8 player1_blue : 1;
			u8 player2_red : 1;
			u8 player2_yellow : 1;
			u8 player2_green : 1;
			
			u8 player2_orange : 1;
			u8 player2_blue : 1;
			u8 player3_red : 1;
			u8 player3_yellow : 1;
			u8 player3_green : 1;
			u8 player3_orange : 1;
			u8 player3_blue : 1;
			u8 player4_red : 1;

			u8 player4_yellow : 1;
			u8 player4_green : 1;
			u8 player4_orange : 1;
			u8 player4_blue : 1;

			u8 tail : 4;
		} data = {};
		#pragma pack(pop)
	};

	class BuzzDevice final : public DeviceProxy
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
