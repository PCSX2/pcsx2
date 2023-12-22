// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include <memory>

#include "qemu-usb/USBinternal.h"

#include "Config.h"
#include "SaveState.h"

class StateWrapper;

// also map key/array index
enum DeviceType : s32
{
	DEVTYPE_NONE = -1,
	DEVTYPE_PAD = 0,
	DEVTYPE_MSD,
	DEVTYPE_SINGSTAR,
	DEVTYPE_LOGITECH_MIC,
	DEVTYPE_LOGITECH_HEADSET,
	DEVTYPE_HIDKBD,
	DEVTYPE_HIDMOUSE,
	DEVTYPE_RBKIT,
	DEVTYPE_BUZZ,
	DEVTYPE_EYETOY,
	DEVTYPE_BEATMANIA_DADADA,
	DEVTYPE_SEGA_SEAMIC,
	DEVTYPE_PRINTER,
	DEVTYPE_KEYBOARDMANIA,
	DEVTYPE_GUNCON2,
	DEVTYPE_DJ
};

class DeviceProxy
{
public:
	virtual ~DeviceProxy();

	virtual const char* Name() const = 0;
	virtual const char* TypeName() const = 0;
	virtual std::span<const char*> SubTypes() const;
	virtual std::span<const InputBindingInfo> Bindings(u32 subtype) const;
	virtual std::span<const SettingInfo> Settings(u32 subtype) const;

	virtual USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const = 0;

	virtual float GetBindingValue(const USBDevice* dev, u32 bind) const;
	virtual void SetBindingValue(USBDevice* dev, u32 bind, float value) const;

	virtual bool Freeze(USBDevice* dev, StateWrapper& sw) const;
	virtual void UpdateSettings(USBDevice* dev, SettingsInterface& si) const;

	virtual void InputDeviceConnected(USBDevice* dev, const std::string_view& identifier) const;
	virtual void InputDeviceDisconnected(USBDevice* dev, const std::string_view& identifier) const;
};

class RegisterDevice
{
	RegisterDevice(const RegisterDevice&) = delete;
	RegisterDevice() {}
	static RegisterDevice* registerDevice;

public:
	typedef std::map<DeviceType, std::unique_ptr<DeviceProxy>> RegisterDeviceMap;
	static RegisterDevice& instance()
	{
		if (!registerDevice)
			registerDevice = new RegisterDevice();
		return *registerDevice;
	}

	~RegisterDevice() {}

	static void Register();
	void Unregister();

	void Add(DeviceType key, DeviceProxy* creator)
	{
		registerDeviceMap[key] = std::unique_ptr<DeviceProxy>(creator);
	}

	DeviceProxy* Device(const std::string_view& name)
	{
		auto proxy = std::find_if(registerDeviceMap.begin(),
								  registerDeviceMap.end(),
								  [&name](const RegisterDeviceMap::value_type& val) -> bool {
									  return val.second->TypeName() == name;
								  });
		if (proxy != registerDeviceMap.end())
			return proxy->second.get();
		return nullptr;
	}

	DeviceProxy* Device(s32 index)
	{
		const auto it = registerDeviceMap.find(static_cast<DeviceType>(index));
		return (it != registerDeviceMap.end()) ? it->second.get() : nullptr;
	}

	DeviceType Index(const std::string_view& name)
	{
		auto proxy = std::find_if(registerDeviceMap.begin(),
								  registerDeviceMap.end(),
								  [&name](RegisterDeviceMap::value_type& val) -> bool {
									  return val.second->TypeName() == name;
								  });
		if (proxy != registerDeviceMap.end())
			return proxy->first;
		return DEVTYPE_NONE;
	}

	const RegisterDeviceMap& Map() const
	{
		return registerDeviceMap;
	}

private:
	RegisterDeviceMap registerDeviceMap;
};
