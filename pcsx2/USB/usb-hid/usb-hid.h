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
