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

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gsl/span"

#include "Config.h"

class SettingsInterface;
class StateWrapper;

namespace USB
{
	enum : u32
	{
		NUM_PORTS = 2,
	};

	s32 DeviceTypeNameToIndex(const std::string_view& device);
	const char* DeviceTypeIndexToName(s32 device);

	std::vector<std::pair<const char*, const char*>> GetDeviceTypes();
	const char* GetDeviceName(const std::string_view& device);
	const char* GetDeviceSubtypeName(const std::string_view& device, u32 subtype);
	gsl::span<const char*> GetDeviceSubtypes(const std::string_view& device);
	gsl::span<const InputBindingInfo> GetDeviceBindings(const std::string_view& device, u32 subtype);
	gsl::span<const SettingInfo> GetDeviceSettings(const std::string_view& device, u32 subtype);

	gsl::span<const InputBindingInfo> GetDeviceBindings(u32 port);
	float GetDeviceBindValue(u32 port, u32 bind_index);
	void SetDeviceBindValue(u32 port, u32 bind_index, float value);

	/// Called when a new input device is connected.
	void InputDeviceConnected(const std::string_view& identifier);

	/// Called when an input device is disconnected.
	void InputDeviceDisconnected(const std::string_view& identifier);

	std::string GetConfigSection(int port);
	std::string GetConfigDevice(const SettingsInterface& si, u32 port);
	void SetConfigDevice(SettingsInterface& si, u32 port, const char* devname);
	u32 GetConfigSubType(const SettingsInterface& si, u32 port, const std::string_view& devname);
	void SetConfigSubType(SettingsInterface& si, u32 port, const std::string_view& devname, u32 subtype);

	/// Returns the configuration key for the specified bind and device type.
	std::string GetConfigSubKey(const std::string_view& device, const std::string_view& bind_name);

	/// Performs automatic controller mapping with the provided list of generic mappings.
	bool MapDevice(SettingsInterface& si, u32 port, const std::vector<std::pair<GenericInputBinding, std::string>>& mapping);

	/// Clears all bindings for a given port.
	void ClearPortBindings(SettingsInterface& si, u32 port);

	/// Copies configuration between two profiles.
	void CopyConfiguration(SettingsInterface* dest_si, const SettingsInterface& src_si, bool copy_devices = true,
		bool copy_bindings = true);

	/// Resets configuration for all ports.
	void SetDefaultConfiguration(SettingsInterface* si);

	/// Identifies any device/subtype changes and recreates devices.
	void CheckForConfigChanges(const Pcsx2Config& old_config);

	/// Reads a device-specific configuration boolean.
	bool GetConfigBool(SettingsInterface& si, u32 port, const char* devname, const char* key, bool default_value);

	/// Reads a device-specific configuration integer.
	s32 GetConfigInt(SettingsInterface& si, u32 port, const char* devname, const char* key, s32 default_value);

	/// Reads a device-specific configuration floating-point value.
	float GetConfigFloat(SettingsInterface& si, u32 port, const char* devname, const char* key, float default_value);

	/// Reads a device-specific configuration string.
	std::string GetConfigString(SettingsInterface& si, u32 port, const char* devname, const char* key, const char* default_value = "");

	/// Handles loading/saving save state.
	bool DoState(StateWrapper& sw);
} // namespace USB

struct WindowInfo;

// ---------------------------------------------------------------------

void USBinit();
void USBasync(u32 cycles);
void USBshutdown();
void USBclose();
bool USBopen();
void USBreset();

u8 USBread8(u32 addr);
u16 USBread16(u32 addr);
u32 USBread32(u32 addr);
void USBwrite8(u32 addr, u8 value);
void USBwrite16(u32 addr, u16 value);
void USBwrite32(u32 addr, u32 value);

void USBsetRAM(void* mem);
