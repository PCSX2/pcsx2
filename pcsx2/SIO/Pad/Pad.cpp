// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Host.h"
#include "Input/InputManager.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Pad/PadDualshock2.h"
#include "SIO/Pad/PadGuitar.h"
#include "SIO/Pad/PadPopn.h"
#include "SIO/Pad/PadNotConnected.h"
#include "SIO/Sio.h"

#include "IconsFontAwesome5.h"

#include "VMManager.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/StringUtil.h"

#include "fmt/format.h"

#include <vector>

namespace Pad
{
	struct MacroButton
	{
		std::vector<u32> buttons; ///< Buttons to activate.
		float pressure; ///< Pressure to apply when macro is active.
		u32 toggle_frequency; ///< Interval at which the buttons will be toggled, if not 0.
		u32 toggle_counter; ///< When this counter reaches zero, buttons will be toggled.
		bool toggle_state; ///< Current state for turbo.
		bool trigger_state; ///< Whether the macro button is active.
	};

	static const char* GetControllerTypeName(Pad::ControllerType type);

	static PadBase* CreatePad(u8 unifiedSlot, Pad::ControllerType controllerType, size_t ejectTicks = 0);

	static void LoadMacroButtonConfig(
		const SettingsInterface& si, u32 pad, const ControllerInfo* ci, const std::string& section);
	static void ApplyMacroButton(u32 controller, const MacroButton& mb);

	static std::array<std::array<MacroButton, NUM_MACRO_BUTTONS_PER_CONTROLLER>, NUM_CONTROLLER_PORTS> s_macro_buttons;
	static std::array<std::unique_ptr<PadBase>, NUM_CONTROLLER_PORTS> s_controllers;

	bool mtapPort0LastState;
	bool mtapPort1LastState;
}

bool Pad::Initialize()
{
	return true;
}

void Pad::Shutdown()
{
	for (auto& port : s_controllers)
		port.reset();
}

const char* Pad::ControllerInfo::GetLocalizedName() const
{
	return Host::TranslateToCString("Pad", display_name);
}

std::optional<u32> Pad::ControllerInfo::GetBindIndex(const std::string_view& name) const
{
	for (u32 i = 0; i < static_cast<u32>(bindings.size()); i++)
	{
		if (name == bindings[i].name)
			return i;
	}

	return std::nullopt;
}

void Pad::LoadConfig(const SettingsInterface& si)
{
	s_macro_buttons = {};

	const bool mtapPort0Changed = EmuConfig.Pad.MultitapPort0_Enabled != Pad::mtapPort0LastState;
	const bool mtapPort1Changed = EmuConfig.Pad.MultitapPort1_Enabled != Pad::mtapPort1LastState;

	for (u32 i = 0; i < Pad::NUM_CONTROLLER_PORTS; i++)
	{
		const std::string section = GetConfigSection(i);
		const ControllerInfo* ci = GetControllerInfo(EmuConfig.Pad.Ports[i].Type);
		pxAssert(ci);

		PadBase* pad = Pad::GetPad(i);

		// If pad pointer is not occupied yet, type in settings no longer matches the current type, or a multitap was slotted in/out,
		// then reconstruct a new pad.
		if (!pad || pad->GetType() != ci->type || (mtapPort0Changed && (i <= 4 && i != 1)) || (mtapPort1Changed && (i >= 5 || i == 1)))
		{
			// If the slot is a multitap slot, and the multitap is not plugged in, then the pad should be forced to Not Connected.
			if (i > 1 && ((i <= 4 && !EmuConfig.Pad.MultitapPort0_Enabled) || (i > 4 && !EmuConfig.Pad.MultitapPort1_Enabled)))
			{
				pad = Pad::CreatePad(i, Pad::ControllerType::NotConnected, (VMManager::GetState() != VMState::Shutdown ? Pad::DEFAULT_EJECT_TICKS : 0));
			}
			else
			{
				// Create the new pad. If the VM is in any kind of running state at all, set eject ticks so the PS2 will think
				// there was some kind of pad ejection event and properly detect the new one, and properly initiate its config sequence.
				pad = Pad::CreatePad(i, ci->type, (VMManager::GetState() != VMState::Shutdown ? Pad::DEFAULT_EJECT_TICKS : 0));
			}
			
			pxAssert(pad);
		}

		const float axis_deadzone = si.GetFloatValue(section.c_str(), "Deadzone", Pad::DEFAULT_STICK_DEADZONE);
		const float axis_scale = si.GetFloatValue(section.c_str(), "AxisScale", Pad::DEFAULT_STICK_SCALE);
		const float button_deadzone = si.GetFloatValue(section.c_str(), "ButtonDeadzone", Pad::DEFAULT_BUTTON_DEADZONE);
		pad->SetAxisScale(axis_deadzone, axis_scale);
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
		LoadMacroButtonConfig(si, i, ci, section);
	}

	Pad::mtapPort0LastState = EmuConfig.Pad.MultitapPort0_Enabled;
	Pad::mtapPort1LastState = EmuConfig.Pad.MultitapPort1_Enabled;
}

Pad::ControllerType Pad::GetDefaultPadType(u32 pad)
{
	return (pad == 0) ? ControllerType::DualShock2 :  ControllerType::NotConnected;
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
	si.SetBoolValue("InputSources", "SDLControllerEnhancedMode", false);
	si.SetBoolValue("Pad", "MultitapPort1", false);
	si.SetBoolValue("Pad", "MultitapPort2", false);
	si.SetFloatValue("Pad", "PointerXScale", 8.0f);
	si.SetFloatValue("Pad", "PointerYScale", 8.0f);

	// PCSX2 Controller Settings - Default pad types and parameters.
	for (u32 i = 0; i < Pad::NUM_CONTROLLER_PORTS; i++)
	{
		const std::string section = GetConfigSection(i);
		const ControllerInfo* ci = GetControllerInfo(GetDefaultPadType(i));
		si.ClearSection(section.c_str());
		si.SetStringValue(section.c_str(), "Type", ci->name);

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
	&PadPopn::ControllerInfo,
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

const Pad::ControllerInfo* Pad::GetControllerInfoByName(const std::string_view& name)
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

const Pad::ControllerInfo* Pad::GetConfigControllerType(const SettingsInterface& si, const char* section, u32 port)
{
	return GetControllerInfoByName(
		si.GetStringValue(section, "Type", GetControllerInfo(GetDefaultPadType(port))->name).c_str());
}

void Pad::ClearPortBindings(SettingsInterface& si, u32 port)
{
	// Why don't these just access EmuConfig? Input Profiles.
	const std::string section = GetConfigSection(port);
	const ControllerInfo* info = GetConfigControllerType(si, section.c_str(), port);
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
		dest_si->CopyBoolValue(src_si, "InputSources", "SDLControllerEnhancedMode");
	}

	for (u32 port = 0; port < Pad::NUM_CONTROLLER_PORTS; port++)
	{
		const std::string section = GetConfigSection(port);
		if (copy_pad_config)
			dest_si->CopyStringValue(src_si, section.c_str(), "Type");

		const Pad::ControllerInfo* info = GetConfigControllerType(src_si, section.c_str(), port);
		if (!info)
			continue;

		if (copy_pad_bindings)
		{
			for (const InputBindingInfo& bi : info->bindings)
				dest_si->CopyStringListValue(src_si, section.c_str(), bi.name);

			for (u32 i = 0; i < NUM_MACRO_BUTTONS_PER_CONTROLLER; i++)
			{
				dest_si->CopyStringListValue(src_si, section.c_str(), fmt::format("Macro{}", i + 1).c_str());
				dest_si->CopyFloatValue(src_si, section.c_str(), fmt::format("Macro{}Pressure", i + 1).c_str());
				dest_si->CopyFloatValue(src_si, section.c_str(), fmt::format("Macro{}Deadzone", i + 1).c_str());
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
	const std::string section = GetConfigSection(controller);
	const ControllerInfo* info = GetConfigControllerType(si, section.c_str(), controller);
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

// Create a new pad instance, update the smart pointer for this pad slot, and return a dumb pointer to the new pad.
PadBase* Pad::CreatePad(u8 unifiedSlot, ControllerType controllerType, size_t ejectTicks)
{
	switch (controllerType)
	{
		case ControllerType::DualShock2:
			s_controllers[unifiedSlot] = std::make_unique<PadDualshock2>(unifiedSlot, ejectTicks);
			break;
		case ControllerType::Guitar:
			s_controllers[unifiedSlot] = std::make_unique<PadGuitar>(unifiedSlot, ejectTicks);
			break;
		case ControllerType::Popn:
			s_controllers[unifiedSlot] = std::make_unique<PadPopn>(unifiedSlot, ejectTicks);
			break;
		default:
			s_controllers[unifiedSlot] = std::make_unique<PadNotConnected>(unifiedSlot, ejectTicks);
			break;
	}

	return s_controllers[unifiedSlot].get();
}

bool Pad::HasConnectedPad(u8 unifiedSlot)
{
	return (
		unifiedSlot < NUM_CONTROLLER_PORTS && s_controllers[unifiedSlot]->GetType() != ControllerType::NotConnected);
}

PadBase* Pad::GetPad(u8 port, u8 slot)
{
	const u8 unifiedSlot = sioConvertPortAndSlotToPad(port, slot);
	return s_controllers[unifiedSlot].get();
}

PadBase* Pad::GetPad(const u8 unifiedSlot)
{
	return s_controllers[unifiedSlot].get();
}

void Pad::SetControllerState(u32 controller, u32 bind, float value)
{
	if (controller >= NUM_CONTROLLER_PORTS)
		return;

	s_controllers[controller]->Set(bind, value);
}

bool Pad::Freeze(StateWrapper& sw)
{
	if (sw.IsReading())
	{
		if (!sw.DoMarker("PAD"))
		{
			Console.Error("PAD state is invalid! Leaving the current state in place.");
			return false;
		}

		for (u32 unifiedSlot = 0; unifiedSlot < NUM_CONTROLLER_PORTS; unifiedSlot++)
		{
			PadBase* currentPad = GetPad(unifiedSlot);
			ControllerType statePadType;
			
			sw.Do(&statePadType);
			
			if (sw.HasError())
				return false;
			
			if (!currentPad)
			{
				pxAssertMsg(false, fmt::format("Pad::Freeze (on read) Existing Pad {0} was nullptr", unifiedSlot).c_str());
			}
			// If the currently configured pad is of a different type than the pad which was used during the savestate...
			else if (currentPad->GetType() != statePadType)
			{
				const ControllerType currentPadType = currentPad->GetType();

				const auto& [port, slot] = sioConvertPadToPortAndSlot(unifiedSlot);
				Host::AddIconOSDMessage(fmt::format("UnfreezePad{}Changed", unifiedSlot), ICON_FA_GAMEPAD,
					fmt::format(TRANSLATE_FS("Pad",
									"Controller port {0}, slot {1} has a {2} connected, but the save state has a "
									"{3}.\nEjecting {3} and replacing it with {2}."),
						port, slot,
						GetControllerTypeName(currentPad ? currentPad->GetType() : Pad::ControllerType::NotConnected),
						GetControllerTypeName(statePadType)));
				
				// Run the freeze, using a new pad instance of the old type just so we make sure all those attributes
				// from the state are read out and we aren't going to run into some sort of consistency problem.
				currentPad = CreatePad(unifiedSlot, statePadType);

				if (currentPad)
				{
					currentPad->Freeze(sw);

					// Now immediately discard whatever malformed pad state we just created, and replace it with a fresh pad loaded
					// using whatever the current user settings are. Savestates are, by definition, never going to occur in the middle
					// of a transfer between SIO2 and the peripheral, since they aren't captured until the VM is at a point where everything
					// is "stoppable". For all intents and purposes, by the time a savestate is captured, the IOP is "done" and there is no
					// "pending work" left hanging in SIO2 or the pads. So there is nothing actually lost from just throwing the pad away and making a new one here.
					currentPad = CreatePad(unifiedSlot, currentPadType, Pad::DEFAULT_EJECT_TICKS);
				}
				else
				{
					pxAssertMsg(false, fmt::format("Pad::Freeze (on read) State Pad {0} was nullptr", unifiedSlot).c_str());
				}
			}
			// ... else, just run the freeze normally.
			else if (currentPad && !currentPad->Freeze(sw))
			{
				return false;
			}
		}
	}
	else
	{
		if (!sw.DoMarker("PAD"))
			return false;

		for (u32 unifiedSlot = 0; unifiedSlot < NUM_CONTROLLER_PORTS; unifiedSlot++)
		{
			PadBase* pad = GetPad(unifiedSlot);
			ControllerType type = pad->GetType();
			sw.Do(&type);
			if (sw.HasError() || !pad->Freeze(sw))
				return false;
		}
	}

	return !sw.HasError();
}


void Pad::LoadMacroButtonConfig(const SettingsInterface& si, u32 pad, const ControllerInfo* ci, const std::string& section)
{
	for (u32 i = 0; i < NUM_MACRO_BUTTONS_PER_CONTROLLER; i++)
	{
		std::string binds_string;
		if (!si.GetStringValue(section.c_str(), StringUtil::StdStringFromFormat("Macro%uBinds", i + 1).c_str(), &binds_string))
			continue;

		const u32 frequency = si.GetUIntValue(section.c_str(), StringUtil::StdStringFromFormat("Macro%uFrequency", i + 1).c_str(), 0u);
		const float pressure = si.GetFloatValue(section.c_str(), fmt::format("Macro{}Pressure", i + 1).c_str(), 1.0f);

		// convert binds
		std::vector<u32> bind_indices;
		std::vector<std::string_view> buttons_split(StringUtil::SplitString(binds_string, '&', true));
		if (buttons_split.empty())
			continue;
		for (const std::string_view& button : buttons_split)
		{
			std::optional<u32> bind_index = ci->GetBindIndex(button);
			if (!bind_index.has_value())
			{
				Console.Error(fmt::format("Invalid bind '{}' in macro button {} for pad {}", button, i, pad));
				continue;
			}

			bind_indices.push_back(bind_index.value());
		}
		if (bind_indices.empty())
			continue;

		MacroButton& macro = s_macro_buttons[pad][i];
		macro.buttons = std::move(bind_indices);
		macro.toggle_frequency = frequency;
		macro.pressure = pressure;
	}
}

void Pad::SetMacroButtonState(u32 pad, u32 index, bool state)
{
	if (pad >= Pad::NUM_CONTROLLER_PORTS || index >= NUM_MACRO_BUTTONS_PER_CONTROLLER)
		return;

	MacroButton& mb = s_macro_buttons[pad][index];
	if (mb.buttons.empty() || mb.trigger_state == state)
		return;

	mb.toggle_counter = mb.toggle_frequency;
	mb.trigger_state = state;
	if (mb.toggle_state != state)
	{
		mb.toggle_state = state;
		ApplyMacroButton(pad, mb);
	}
}

void Pad::ApplyMacroButton(u32 controller, const Pad::MacroButton& mb)
{
	const float value = mb.toggle_state ? mb.pressure : 0.0f;
	PadBase* const pad = Pad::GetPad(controller);

	for (const u32 btn : mb.buttons)
		pad->Set(btn, value);
}

void Pad::UpdateMacroButtons()
{
	for (u32 pad = 0; pad < Pad::NUM_CONTROLLER_PORTS; pad++)
	{
		for (u32 index = 0; index < NUM_MACRO_BUTTONS_PER_CONTROLLER; index++)
		{
			Pad::MacroButton& mb = s_macro_buttons[pad][index];

			if (!mb.trigger_state || mb.toggle_frequency == 0)
			{
				continue;
			}

			mb.toggle_counter--;

			if (mb.toggle_counter > 0)
			{
				continue;
			}

			mb.toggle_counter = mb.toggle_frequency;
			mb.toggle_state = !mb.toggle_state;
			ApplyMacroButton(pad, mb);
		}
	}
}