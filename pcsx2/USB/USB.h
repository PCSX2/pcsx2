// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Config.h"

class SettingsInterface;
class StateWrapper;

namespace USB
{
	enum : u32
	{
		NUM_PORTS = 2,
	};

	s32 DeviceTypeNameToIndex(const std::string_view device);
	const char* DeviceTypeIndexToName(s32 device);

	std::vector<std::pair<const char*, const char*>> GetDeviceTypes();
	const char* GetDeviceName(const std::string_view device);
	const char* GetDeviceIconName(u32 port);
	const char* GetDeviceSubtypeName(const std::string_view device, u32 subtype);
	std::span<const char*> GetDeviceSubtypes(const std::string_view device);
	std::span<const InputBindingInfo> GetDeviceBindings(const std::string_view device, u32 subtype);
	std::span<const SettingInfo> GetDeviceSettings(const std::string_view device, u32 subtype);

	std::span<const InputBindingInfo> GetDeviceBindings(u32 port);
	float GetDeviceBindValue(u32 port, u32 bind_index);
	void SetDeviceBindValue(u32 port, u32 bind_index, float value);

	/// Called when a new input device is connected.
	void InputDeviceConnected(const std::string_view identifier);

	/// Called when an input device is disconnected.
	void InputDeviceDisconnected(const std::string_view identifier);

	std::string GetConfigSection(int port);
	std::string GetConfigDevice(const SettingsInterface& si, u32 port);
	void SetConfigDevice(SettingsInterface& si, u32 port, const char* devname);
	u32 GetConfigSubType(const SettingsInterface& si, u32 port, const std::string_view devname);
	void SetConfigSubType(SettingsInterface& si, u32 port, const std::string_view devname, u32 subtype);

	/// Returns the configuration key for the specified bind and device type.
	std::string GetConfigSubKey(const std::string_view device, const std::string_view bind_name);

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

	/// Returns true if a device-specific configuration key exists.
	bool ConfigKeyExists(SettingsInterface& si, u32 port, const char* devname, const char* key);

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
