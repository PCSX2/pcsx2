/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"

#include "SIO/Pad/PadConfig.h"

#include "SIO/Pad/PadManager.h"
#include "SIO/Pad/PadMacros.h"
#include "SIO/Pad/PadNotConnected.h"
#include "SIO/Pad/PadDualshock2.h"
#include "SIO/Pad/PadGuitar.h"

#include "Host.h"

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/SettingsInterface.h"

#include "Input/InputManager.h"

const char* Pad::ControllerInfo::GetLocalizedName() const
{
	return Host::TranslateToCString("Pad", display_name);
}

void Pad::LoadConfig(const SettingsInterface& si)
{
	ClearMacros();

	EmuConfig.MultitapPort0_Enabled = si.GetBoolValue("Pad", "MultitapPort1", false);
	EmuConfig.MultitapPort1_Enabled = si.GetBoolValue("Pad", "MultitapPort2", false);

	// This is where we would load controller types, if onepad supported them.
	for (u32 i = 0; i < Pad::NUM_CONTROLLER_PORTS; i++)
	{
		const std::string section(GetConfigSection(i));
		const std::string type(si.GetStringValue(section.c_str(), "Type", GetDefaultPadType(i)));
		const ControllerInfo* ci = GetControllerInfo(type);
		PadBase* pad = Pad::GetPad(i);

		// If a pad is not yet constructed, at minimum place a NotConnected pad in the slot.
		// Do not abort the for loop - If there pad settings, we want those to be applied to the slot.
		if (!pad)
		{
			pad = Pad::ChangePadType(i, Pad::ControllerType::NotConnected);
		}

		if (!ci)
		{
			pad = Pad::ChangePadType(i, Pad::ControllerType::NotConnected);
			continue;
		}


		const Pad::ControllerType oldType = pad->GetType();

		if (ci->type != oldType)
		{
			pad = Pad::ChangePadType(i, ci->type);
		}

		const float axis_deadzone = si.GetFloatValue(section.c_str(), "Deadzone", Pad::DEFAULT_STICK_DEADZONE);
		const float axis_scale = si.GetFloatValue(section.c_str(), "AxisScale", Pad::DEFAULT_STICK_SCALE);
		const float trigger_deadzone = si.GetFloatValue(section.c_str(), "TriggerDeadzone", Pad::DEFAULT_TRIGGER_DEADZONE);
		const float trigger_scale = si.GetFloatValue(section.c_str(), "TriggerScale", Pad::DEFAULT_TRIGGER_SCALE);
		const float button_deadzone = si.GetFloatValue(section.c_str(), "ButtonDeadzone", Pad::DEFAULT_BUTTON_DEADZONE);
		pad->SetAxisScale(axis_deadzone, axis_scale);
		pad->SetTriggerScale(trigger_deadzone, trigger_scale);
		pad->SetButtonDeadzone(button_deadzone);

		if (ci->vibration_caps != Pad::VibrationCapabilities::NoVibration)
		{
			const float large_motor_scale = si.GetFloatValue(section.c_str(), "LargeMotorScale", Pad::DEFAULT_MOTOR_SCALE);
			const float small_motor_scale = si.GetFloatValue(section.c_str(), "SmallMotorScale", Pad::DEFAULT_MOTOR_SCALE);
			pad->SetVibrationScale(0, large_motor_scale);
			pad->SetVibrationScale(1, small_motor_scale);
		}

		const float pressure_modifier = si.GetFloatValue(section.c_str(), "PressureModifier", 1.0f);
		pad->SetPressureModifier(pressure_modifier);

		const int invert_l = si.GetIntValue(section.c_str(), "InvertL", 0);
		const int invert_r = si.GetIntValue(section.c_str(), "InvertR", 0);
		pad->SetAnalogInvertL((invert_l & 1) != 0, (invert_l & 2) != 0);
		pad->SetAnalogInvertR((invert_r & 1) != 0, (invert_r & 2) != 0);

		LoadMacroButtonConfig(si, i, type, section);
	}
}

const char* Pad::GetDefaultPadType(u32 pad)
{
	return (pad == 0) ? "DualShock2" : "None";
}

void Pad::SetDefaultControllerConfig(SettingsInterface& si)
{
	si.ClearSection("InputSources");
	si.ClearSection("Hotkeys");
	si.ClearSection("Pad");

	// PCSX2 Controller Settings - Global Settings
	for (u32 i = 0; i < static_cast<u32>(InputSourceType::Count); i++)
	{
		si.SetBoolValue("InputSources",
			InputManager::InputSourceToString(static_cast<InputSourceType>(i)),
			InputManager::GetInputSourceDefaultEnabled(static_cast<InputSourceType>(i)));
	}
#ifdef SDL_BUILD
	si.SetBoolValue("InputSources", "SDLControllerEnhancedMode", false);
#endif
	si.SetBoolValue("Pad", "MultitapPort1", false);
	si.SetBoolValue("Pad", "MultitapPort2", false);
	si.SetFloatValue("Pad", "PointerXScale", 8.0f);
	si.SetFloatValue("Pad", "PointerYScale", 8.0f);

	// PCSX2 Controller Settings - Default pad types and parameters.
	for (u32 i = 0; i < Pad::NUM_CONTROLLER_PORTS; i++)
	{
		const char* type = GetDefaultPadType(i);
		const std::string section(GetConfigSection(i));
		si.ClearSection(section.c_str());
		si.SetStringValue(section.c_str(), "Type", type);

		const ControllerInfo* ci = GetControllerInfo(type);
		if (ci)
		{
			for (const SettingInfo& csi : ci->settings)
			{
				switch (csi.type)
				{
					case SettingInfo::Type::Boolean:
						si.SetBoolValue(section.c_str(), csi.name, csi.BooleanDefaultValue());
						break;
					case SettingInfo::Type::Integer:
					case SettingInfo::Type::IntegerList:
						si.SetIntValue(section.c_str(), csi.name, csi.IntegerDefaultValue());
						break;
					case SettingInfo::Type::Float:
						si.SetFloatValue(section.c_str(), csi.name, csi.FloatDefaultValue());
						break;
					case SettingInfo::Type::String:
					case SettingInfo::Type::StringList:
					case SettingInfo::Type::Path:
						si.SetStringValue(section.c_str(), csi.name, csi.StringDefaultValue());
						break;
					default:
						break;
				}
			}
		}
	}

	// PCSX2 Controller Settings - Controller 1 / Controller 2 / ...
	// Use the automapper to set this up.
	MapController(si, 0, InputManager::GetGenericBindingMapping("Keyboard"));
}

void Pad::SetDefaultHotkeyConfig(SettingsInterface& si)
{
	// PCSX2 Controller Settings - Hotkeys

	// PCSX2 Controller Settings - Hotkeys - General
	si.SetStringValue("Hotkeys", "ToggleFullscreen", "Keyboard/Alt & Keyboard/Return");

	// PCSX2 Controller Settings - Hotkeys - Graphics
	si.SetStringValue("Hotkeys", "CycleAspectRatio", "Keyboard/F6");
	si.SetStringValue("Hotkeys", "CycleInterlaceMode", "Keyboard/F5");
	si.SetStringValue("Hotkeys", "CycleMipmapMode", "Keyboard/Insert");
	//	si.SetStringValue("Hotkeys", "DecreaseUpscaleMultiplier", "Keyboard"); TBD
	//	si.SetStringValue("Hotkeys", "IncreaseUpscaleMultiplier", "Keyboard"); TBD
	//  si.SetStringValue("Hotkeys", "ReloadTextureReplacements", "Keyboard"); TBD
	si.SetStringValue("Hotkeys", "GSDumpMultiFrame", "Keyboard/Control & Keyboard/Shift & Keyboard/F8");
	si.SetStringValue("Hotkeys", "Screenshot", "Keyboard/F8");
	si.SetStringValue("Hotkeys", "GSDumpSingleFrame", "Keyboard/Shift & Keyboard/F8");
	si.SetStringValue("Hotkeys", "ToggleSoftwareRendering", "Keyboard/F9");
	//  si.SetStringValue("Hotkeys", "ToggleTextureDumping", "Keyboard"); TBD
	//  si.SetStringValue("Hotkeys", "ToggleTextureReplacements", "Keyboard"); TBD
	si.SetStringValue("Hotkeys", "ZoomIn", "Keyboard/Control & Keyboard/Plus");
	si.SetStringValue("Hotkeys", "ZoomOut", "Keyboard/Control & Keyboard/Minus");
	// Missing hotkey for resetting zoom back to 100 with Keyboard/Control & Keyboard/Asterisk

	// PCSX2 Controller Settings - Hotkeys - Input Recording
	si.SetStringValue("Hotkeys", "InputRecToggleMode", "Keyboard/Shift & Keyboard/R");

	// PCSX2 Controller Settings - Hotkeys - Save States
	si.SetStringValue("Hotkeys", "LoadStateFromSlot", "Keyboard/F3");
	si.SetStringValue("Hotkeys", "SaveStateToSlot", "Keyboard/F1");
	si.SetStringValue("Hotkeys", "NextSaveStateSlot", "Keyboard/F2");
	si.SetStringValue("Hotkeys", "PreviousSaveStateSlot", "Keyboard/Shift & Keyboard/F2");

	// PCSX2 Controller Settings - Hotkeys - System
	//	si.SetStringValue("Hotkeys", "DecreaseSpeed", "Keyboard"); TBD
	//  si.SetStringValue("Hotkeys", "FrameAdvance", "Keyboard"); TBD
	//	si.SetStringValue("Hotkeys", "IncreaseSpeed", "Keyboard"); TBD
	//  si.SetStringValue("Hotkeys", "ResetVM", "Keyboard"); TBD
	//  si.SetStringValue("Hotkeys", "ShutdownVM", "Keyboard"); TBD
	si.SetStringValue("Hotkeys", "OpenPauseMenu", "Keyboard/Escape");
	si.SetStringValue("Hotkeys", "ToggleFrameLimit", "Keyboard/F4");
	si.SetStringValue("Hotkeys", "TogglePause", "Keyboard/Space");
	si.SetStringValue("Hotkeys", "ToggleSlowMotion", "Keyboard/Shift & Keyboard/Backtab");
	si.SetStringValue("Hotkeys", "ToggleTurbo", "Keyboard/Tab");
	si.SetStringValue("Hotkeys", "HoldTurbo", "Keyboard/Period");
}

static const Pad::ControllerInfo* s_controller_info[] = {
	&PadNotConnected::ControllerInfo,
	&PadDualshock2::ControllerInfo,
	&PadGuitar::ControllerInfo,
};

const Pad::ControllerInfo* Pad::GetControllerInfo(Pad::ControllerType type)
{
	for (const ControllerInfo* info : s_controller_info)
	{
		if (type == info->type)
			return info;
	}

	return nullptr;
}

const Pad::ControllerInfo* Pad::GetControllerInfo(const std::string_view& name)
{
	for (const ControllerInfo* info : s_controller_info)
	{
		if (name == info->name)
			return info;
	}

	return nullptr;
}

const char* Pad::GetControllerTypeName(Pad::ControllerType type)
{
	// Not localized, because it should never happen.
	const ControllerInfo* ci = GetControllerInfo(type);
	return ci ? ci->GetLocalizedName() : "UNKNOWN";
}

const std::vector<std::pair<const char*, const char*>> Pad::GetControllerTypeNames()
{
	std::vector<std::pair<const char*, const char*>> ret;
	for (const ControllerInfo* info : s_controller_info)
		ret.emplace_back(info->name, info->GetLocalizedName());

	return ret;
}

std::vector<std::string> Pad::GetControllerBinds(const std::string_view& type)
{
	std::vector<std::string> ret;

	const ControllerInfo* info = GetControllerInfo(type);
	if (info)
	{
		for (const InputBindingInfo& bi : info->bindings)
		{
			if (bi.bind_type == InputBindingInfo::Type::Unknown || bi.bind_type == InputBindingInfo::Type::Motor)
				continue;

			ret.emplace_back(bi.name);
		}
	}

	return ret;
}

void Pad::ClearPortBindings(SettingsInterface& si, u32 port)
{
	const std::string section(StringUtil::StdStringFromFormat("Pad%u", port + 1));
	const std::string type(si.GetStringValue(section.c_str(), "Type", GetDefaultPadType(port)));

	const ControllerInfo* info = GetControllerInfo(type);
	if (!info)
		return;

	for (const InputBindingInfo& bi : info->bindings)
		si.DeleteValue(section.c_str(), bi.name);
}

void Pad::CopyConfiguration(SettingsInterface* dest_si, const SettingsInterface& src_si,
	bool copy_pad_config, bool copy_pad_bindings, bool copy_hotkey_bindings)
{
	if (copy_pad_config)
	{
		dest_si->CopyBoolValue(src_si, "Pad", "MultitapPort1");
		dest_si->CopyBoolValue(src_si, "Pad", "MultitapPort2");
		dest_si->CopyBoolValue(src_si, "Pad", "MultitapPort1");
		dest_si->CopyBoolValue(src_si, "Pad", "MultitapPort2");
		dest_si->CopyFloatValue(src_si, "Pad", "PointerXScale");
		dest_si->CopyFloatValue(src_si, "Pad", "PointerYScale");
		for (u32 i = 0; i < static_cast<u32>(InputSourceType::Count); i++)
		{
			dest_si->CopyBoolValue(src_si, "InputSources",
				InputManager::InputSourceToString(static_cast<InputSourceType>(i)));
		}
#ifdef SDL_BUILD
		dest_si->CopyBoolValue(src_si, "InputSources", "SDLControllerEnhancedMode");
#endif
	}

	for (u32 port = 0; port < Pad::NUM_CONTROLLER_PORTS; port++)
	{
		const std::string section(fmt::format("Pad{}", port + 1));
		const std::string type(src_si.GetStringValue(section.c_str(), "Type", GetDefaultPadType(port)));
		if (copy_pad_config)
			dest_si->SetStringValue(section.c_str(), "Type", type.c_str());

		const ControllerInfo* info = GetControllerInfo(type);
		if (!info)
			return;

		if (copy_pad_bindings)
		{
			for (const InputBindingInfo& bi : info->bindings)
				dest_si->CopyStringListValue(src_si, section.c_str(), bi.name);

			for (u32 i = 0; i < NUM_MACRO_BUTTONS_PER_CONTROLLER; i++)
			{
				dest_si->CopyStringListValue(src_si, section.c_str(), fmt::format("Macro{}", i + 1).c_str());
				dest_si->CopyStringValue(src_si, section.c_str(), fmt::format("Macro{}Binds", i + 1).c_str());
				dest_si->CopyUIntValue(src_si, section.c_str(), fmt::format("Macro{}Frequency", i + 1).c_str());
			}
		}

		if (copy_pad_config)
		{
			dest_si->CopyFloatValue(src_si, section.c_str(), "AxisScale");

			if (info->vibration_caps != Pad::VibrationCapabilities::NoVibration)
			{
				dest_si->CopyFloatValue(src_si, section.c_str(), "LargeMotorScale");
				dest_si->CopyFloatValue(src_si, section.c_str(), "SmallMotorScale");
			}

			for (const SettingInfo& csi : info->settings)
			{
				switch (csi.type)
				{
					case SettingInfo::Type::Boolean:
						dest_si->CopyBoolValue(src_si, section.c_str(), csi.name);
						break;
					case SettingInfo::Type::Integer:
					case SettingInfo::Type::IntegerList:
						dest_si->CopyIntValue(src_si, section.c_str(), csi.name);
						break;
					case SettingInfo::Type::Float:
						dest_si->CopyFloatValue(src_si, section.c_str(), csi.name);
						break;
					case SettingInfo::Type::String:
					case SettingInfo::Type::StringList:
					case SettingInfo::Type::Path:
						dest_si->CopyStringValue(src_si, section.c_str(), csi.name);
						break;
					default:
						break;
				}
			}
		}
	}

	if (copy_hotkey_bindings)
	{
		std::vector<const HotkeyInfo*> hotkeys(InputManager::GetHotkeyList());
		for (const HotkeyInfo* hki : hotkeys)
			dest_si->CopyStringListValue(src_si, "Hotkeys", hki->name);
	}
}

static u32 TryMapGenericMapping(SettingsInterface& si, const std::string& section,
	const InputManager::GenericInputBindingMapping& mapping, GenericInputBinding generic_name,
	const char* bind_name)
{
	// find the mapping it corresponds to
	const std::string* found_mapping = nullptr;
	for (const std::pair<GenericInputBinding, std::string>& it : mapping)
	{
		if (it.first == generic_name)
		{
			found_mapping = &it.second;
			break;
		}
	}

	if (found_mapping)
	{
		Console.WriteLn("(MapController) Map %s/%s to '%s'", section.c_str(), bind_name, found_mapping->c_str());
		si.SetStringValue(section.c_str(), bind_name, found_mapping->c_str());
		return 1;
	}
	else
	{
		si.DeleteValue(section.c_str(), bind_name);
		return 0;
	}
}


bool Pad::MapController(SettingsInterface& si, u32 controller,
	const std::vector<std::pair<GenericInputBinding, std::string>>& mapping)
{
	const std::string section(StringUtil::StdStringFromFormat("Pad%u", controller + 1));
	const std::string type(si.GetStringValue(section.c_str(), "Type", GetDefaultPadType(controller)));
	const ControllerInfo* info = GetControllerInfo(type);
	if (!info)
		return false;

	u32 num_mappings = 0;
	for (const InputBindingInfo& bi : info->bindings)
	{
		if (bi.generic_mapping == GenericInputBinding::Unknown)
			continue;

		num_mappings += TryMapGenericMapping(si, section, mapping, bi.generic_mapping, bi.name);
	}
	if (info->vibration_caps == Pad::VibrationCapabilities::LargeSmallMotors)
	{
		num_mappings += TryMapGenericMapping(si, section, mapping, GenericInputBinding::SmallMotor, "SmallMotor");
		num_mappings += TryMapGenericMapping(si, section, mapping, GenericInputBinding::LargeMotor, "LargeMotor");
	}
	else if (info->vibration_caps == Pad::VibrationCapabilities::SingleMotor)
	{
		if (TryMapGenericMapping(si, section, mapping, GenericInputBinding::LargeMotor, "Motor") == 0)
			num_mappings += TryMapGenericMapping(si, section, mapping, GenericInputBinding::SmallMotor, "Motor");
		else
			num_mappings++;
	}

	return (num_mappings > 0);
}

std::vector<std::string> Pad::GetInputProfileNames()
{
	FileSystem::FindResultsArray results;
	FileSystem::FindFiles(EmuFolders::InputProfiles.c_str(), "*.ini",
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES | FILESYSTEM_FIND_RELATIVE_PATHS,
		&results);

	std::vector<std::string> ret;
	ret.reserve(results.size());
	for (FILESYSTEM_FIND_DATA& fd : results)
		ret.emplace_back(Path::GetFileTitle(fd.FileName));
	return ret;
}

std::string Pad::GetConfigSection(u32 pad_index)
{
	return fmt::format("Pad{}", pad_index + 1);
}

void Pad::LoadMacroButtonConfig(const SettingsInterface& si, u32 pad, const std::string_view& type, const std::string& section)
{
	// lazily initialized
	std::vector<std::string> binds;

	for (u32 i = 0; i < NUM_MACRO_BUTTONS_PER_CONTROLLER; i++)
	{
		std::string binds_string;
		if (!si.GetStringValue(section.c_str(), StringUtil::StdStringFromFormat("Macro%uBinds", i + 1).c_str(), &binds_string))
			continue;

		const u32 frequency = si.GetUIntValue(section.c_str(), StringUtil::StdStringFromFormat("Macro%uFrequency", i + 1).c_str(), 0u);
		if (binds.empty())
			binds = GetControllerBinds(type);

		// convert binds
		std::vector<u32> bind_indices;
		std::vector<std::string_view> buttons_split(StringUtil::SplitString(binds_string, '&', true));
		if (buttons_split.empty())
			continue;
		for (const std::string_view& button : buttons_split)
		{
			auto it = std::find(binds.begin(), binds.end(), button);
			if (it == binds.end())
			{
				Console.Error("Invalid bind '%.*s' in macro button %u for pad %u", static_cast<int>(button.size()), button.data(), pad, i);
				continue;
			}

			bind_indices.push_back(static_cast<u32>(std::distance(binds.begin(), it)));
		}
		if (bind_indices.empty())
			continue;

		MacroButton& macro = Pad::GetMacroButton(pad, i);
		macro.buttons = std::move(bind_indices);
		macro.toggle_frequency = frequency;
	}
}