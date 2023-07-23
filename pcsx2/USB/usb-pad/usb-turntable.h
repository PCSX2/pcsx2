/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#pragma once
#include "SaveState.h"
#include <list>
#include <string>

namespace usb_pad
{
	enum TurntableControlID
	{
		CID_DJ_SQUARE,
		CID_DJ_CROSS,
		CID_DJ_CIRCLE,
		CID_DJ_TRIANGLE,
		CID_DJ_SELECT = 8,
		CID_DJ_START,
		CID_DJ_RIGHT_GREEN,
		CID_DJ_RIGHT_RED,
		CID_DJ_RIGHT_BLUE,
		CID_DJ_LEFT_GREEN = 14,
		CID_DJ_LEFT_RED,
		CID_DJ_LEFT_BLUE,
		CID_DJ_CROSSFADER_LEFT,
		CID_DJ_CROSSFADER_RIGHT,
		CID_DJ_EFFECTSKNOB_LEFT,
		CID_DJ_EFFECTSKNOB_RIGHT,
		CID_DJ_LEFT_TURNTABLE_CW,
		CID_DJ_LEFT_TURNTABLE_CCW,
		CID_DJ_RIGHT_TURNTABLE_CW,
		CID_DJ_RIGHT_TURNTABLE_CCW,
		CID_DJ_DPAD_UP,
		CID_DJ_DPAD_DOWN,
		CID_DJ_DPAD_LEFT,
		CID_DJ_DPAD_RIGHT,
		CID_DJ_COUNT,
	};
	struct TurntableState
	{
		TurntableState(u32 port_);
		~TurntableState();

		void UpdateSettings(SettingsInterface& si, const char* devname);

		float GetBindValue(u32 bind) const;
		void SetBindValue(u32 bind, float value);

		void SetEuphoriaLedState(bool state);
		int TokenIn(u8* buf, int len);
		int TokenOut(const u8* buf, int len);

		void UpdateHatSwitch();

		u32 port = 0;

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		float turntable_multiplier = 1;

		struct
		{
			// intermediate state, resolved at query time
			s16 crossfader_left;
			s16 crossfader_right;
			s16 effectsknob_left;
			s16 effectsknob_right;
			s16 left_turntable_cw;
			s16 left_turntable_ccw;
			s16 right_turntable_cw;
			s16 right_turntable_ccw;
			bool hat_left : 1;
			bool hat_right : 1;
			bool hat_up : 1;
			bool hat_down : 1;

			u8 hatswitch; // direction
			u32 buttons; // active high
			bool euphoria_led_state; // 1 = on, 0 = off
		} data = {};
	};
	class DJTurntableDevice final : public DeviceProxy
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
