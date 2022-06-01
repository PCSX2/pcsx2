/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "common/StringUtil.h"
#include "common/SettingsInterface.h"

#include "Frontend/InputManager.h"
#include "HostSettings.h"

#include "PAD/Host/Global.h"
#include "PAD/Host/PAD.h"
#include "PAD/Host/KeyStatus.h"
#include "PAD/Host/StateManagement.h"

const u32 revision = 3;
const u32 build = 0; // increase that with each version
#define PAD_SAVE_STATE_VERSION ((revision << 8) | (build << 0))

KeyStatus g_key_status;

namespace PAD
{
	struct MacroButton
	{
		std::vector<u32> buttons; ///< Buttons to activate.
		u32 toggle_frequency; ///< Interval at which the buttons will be toggled, if not 0.
		u32 toggle_counter; ///< When this counter reaches zero, buttons will be toggled.
		bool toggle_state; ///< Current state for turbo.
		bool trigger_state; ///< Whether the macro button is active.
	};

	static void LoadMacroButtonConfig(const SettingsInterface& si, u32 pad, const std::string_view& type, const std::string& section);
	static void ApplyMacroButton(u32 pad, const MacroButton& mb);
	static void UpdateMacroButtons();

	static std::array<std::array<MacroButton, NUM_MACRO_BUTTONS_PER_CONTROLLER>, GAMEPAD_NUMBER> s_macro_buttons;
} // namespace PAD

s32 PADinit()
{
	Pad::reset_all();

	query.reset();

	for (int port = 0; port < 2; port++)
		slots[port] = 0;

	return 0;
}

void PADshutdown()
{
}

s32 PADopen(const WindowInfo& wi)
{
	g_key_status.Init();
	return 0;
}

void PADclose()
{
}

s32 PADsetSlot(u8 port, u8 slot)
{
	port--;
	slot--;
	if (port > 1 || slot > 3)
	{
		return 0;
	}
	// Even if no pad there, record the slot, as it is the active slot regardless.
	slots[port] = slot;

	return 1;
}

s32 PADfreeze(FreezeAction mode, freezeData* data)
{
	if (!data)
		return -1;

	if (mode == FreezeAction::Size)
	{
		data->size = sizeof(PadFullFreezeData);
	}
	else if (mode == FreezeAction::Load)
	{
		PadFullFreezeData* pdata = (PadFullFreezeData*)(data->data);

		Pad::stop_vibrate_all();

		if (data->size != sizeof(PadFullFreezeData) || pdata->version != PAD_SAVE_STATE_VERSION ||
			strncmp(pdata->format, "LinPad", sizeof(pdata->format)))
			return 0;

		query = pdata->query;
		if (pdata->query.slot < 4)
		{
			query = pdata->query;
		}

		// Tales of the Abyss - pad fix
		// - restore data for both ports
		for (int port = 0; port < 2; port++)
		{
			for (int slot = 0; slot < 4; slot++)
			{
				u8 mode = pdata->padData[port][slot].mode;

				if (mode != MODE_DIGITAL && mode != MODE_ANALOG && mode != MODE_DS2_NATIVE)
				{
					break;
				}

				memcpy(&pads[port][slot], &pdata->padData[port][slot], sizeof(PadFreezeData));
			}

			if (pdata->slot[port] < 4)
				slots[port] = pdata->slot[port];
		}
	}
	else if (mode == FreezeAction::Save)
	{
		if (data->size != sizeof(PadFullFreezeData))
			return 0;

		PadFullFreezeData* pdata = (PadFullFreezeData*)(data->data);

		// Tales of the Abyss - pad fix
		// - PCSX2 only saves port0 (save #1), then port1 (save #2)

		memset(pdata, 0, data->size);
		strncpy(pdata->format, "LinPad", sizeof(pdata->format));
		pdata->version = PAD_SAVE_STATE_VERSION;
		pdata->query = query;

		for (int port = 0; port < 2; port++)
		{
			for (int slot = 0; slot < 4; slot++)
			{
				pdata->padData[port][slot] = pads[port][slot];
			}

			pdata->slot[port] = slots[port];
		}
	}
	else
	{
		return -1;
	}

	return 0;
}

u8 PADstartPoll(int pad)
{
	return pad_start_poll(pad);
}

u8 PADpoll(u8 value)
{
	return pad_poll(value);
}

void PAD::LoadConfig(const SettingsInterface& si)
{
	PAD::s_macro_buttons = {};

	// This is where we would load controller types, if onepad supported them.
	for (u32 i = 0; i < GAMEPAD_NUMBER; i++)
	{
		const std::string section(StringUtil::StdStringFromFormat("Pad%u", i + 1u));
		const std::string type(si.GetStringValue(section.c_str(), "Type", GetDefaultPadType(i)));

		const ControllerInfo* ci = GetControllerInfo(type);
		if (!ci)
		{
			g_key_status.SetType(i, ControllerType::NotConnected);
			continue;
		}

		g_key_status.SetType(i, ci->type);

		const float axis_scale = si.GetFloatValue(section.c_str(), "AxisScale", 1.0f);
		g_key_status.SetAxisScale(i, axis_scale);

		if (ci->vibration_caps != VibrationCapabilities::NoVibration)
		{
			const float large_motor_scale = si.GetFloatValue(section.c_str(), "LargeMotorScale", 1.0f);
			const float small_motor_scale = si.GetFloatValue(section.c_str(), "SmallMotorScale", 1.0f);
			g_key_status.SetVibrationScale(i, 0, large_motor_scale);
			g_key_status.SetVibrationScale(i, 1, small_motor_scale);
		}

		LoadMacroButtonConfig(si, i, type, section);
	}
}

const char* PAD::GetDefaultPadType(u32 pad)
{
	return (pad == 0) ? "DualShock2" : "None";
}

void PAD::SetDefaultConfig(SettingsInterface& si)
{
	si.ClearSection("InputSources");

	for (u32 i = 0; i < GAMEPAD_NUMBER; i++)
		si.ClearSection(StringUtil::StdStringFromFormat("Pad%u", i + 1).c_str());

	si.ClearSection("Hotkeys");

	// PCSX2 Controller Settings - Global Settings
	si.SetBoolValue("InputSources", "SDL", true);
	si.SetBoolValue("InputSources", "SDLControllerEnhancedMode", false);
	si.SetBoolValue("InputSources", "XInput", false);

	// PCSX2 Controller Settings - Controller 1 / Controller 2 / ...
	// Use the automapper to set this up.
	si.SetStringValue("Pad1", "Type", GetDefaultPadType(0));
	MapController(si, 0, InputManager::GetGenericBindingMapping("Keyboard"));

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
	si.SetStringValue("Hotkeys", "ShutdownVM", "Keyboard/Escape");
	si.SetStringValue("Hotkeys", "ToggleFrameLimit", "Keyboard/F4");
	si.SetStringValue("Hotkeys", "TogglePause", "Keyboard/Space");
	si.SetStringValue("Hotkeys", "ToggleSlowMotion", "Keyboard/Shift & Keyboard/Backtab");
	si.SetStringValue("Hotkeys", "ToggleTurbo", "Keyboard/Tab");
}

void PAD::Update()
{
	Pad::rumble_all();
	UpdateMacroButtons();
}

static const PAD::ControllerBindingInfo s_dualshock2_binds[] = {
	{"Up", "D-Pad Up", PAD::ControllerBindingType::Button, GenericInputBinding::DPadUp},
	{"Right", "D-Pad Right", PAD::ControllerBindingType::Button, GenericInputBinding::DPadRight},
	{"Down", "D-Pad Down", PAD::ControllerBindingType::Button, GenericInputBinding::DPadDown},
	{"Left", "D-Pad Left", PAD::ControllerBindingType::Button, GenericInputBinding::DPadLeft},
	{"Triangle", "Triangle", PAD::ControllerBindingType::Button, GenericInputBinding::Triangle},
	{"Circle", "Circle", PAD::ControllerBindingType::Button, GenericInputBinding::Circle},
	{"Cross", "Cross", PAD::ControllerBindingType::Button, GenericInputBinding::Cross},
	{"Square", "Square", PAD::ControllerBindingType::Button, GenericInputBinding::Square},
	{"Select", "Select", PAD::ControllerBindingType::Button, GenericInputBinding::Select},
	{"Start", "Start", PAD::ControllerBindingType::Button, GenericInputBinding::Start},
	{"L1", "L1 (Left Bumper)", PAD::ControllerBindingType::Button, GenericInputBinding::L1},
	{"L2", "L2 (Left Trigger)", PAD::ControllerBindingType::HalfAxis, GenericInputBinding::L2},
	{"R1", "R1 (Right Bumper)", PAD::ControllerBindingType::Button, GenericInputBinding::R1},
	{"R2", "R2 (Right Trigger)", PAD::ControllerBindingType::HalfAxis, GenericInputBinding::R2},
	{"L3", "L3 (Left Stick Button)", PAD::ControllerBindingType::Button, GenericInputBinding::L3},
	{"R3", "R3 (Right Stick Button)", PAD::ControllerBindingType::Button, GenericInputBinding::R3},
	{"Analog", "Analog Toggle", PAD::ControllerBindingType::Button, GenericInputBinding::System},
	{"LUp", "Left Stick Up", PAD::ControllerBindingType::HalfAxis, GenericInputBinding::LeftStickUp},
	{"LRight", "Left Stick Right", PAD::ControllerBindingType::HalfAxis, GenericInputBinding::LeftStickRight},
	{"LDown", "Left Stick Down", PAD::ControllerBindingType::HalfAxis, GenericInputBinding::LeftStickDown},
	{"LLeft", "Left Stick Left", PAD::ControllerBindingType::HalfAxis, GenericInputBinding::LeftStickLeft},
	{"RUp", "Right Stick Up", PAD::ControllerBindingType::HalfAxis, GenericInputBinding::RightStickUp},
	{"RRight", "Right Stick Right", PAD::ControllerBindingType::HalfAxis, GenericInputBinding::RightStickRight},
	{"RDown", "Right Stick Down", PAD::ControllerBindingType::HalfAxis, GenericInputBinding::RightStickDown},
	{"RLeft", "Right Stick Left", PAD::ControllerBindingType::HalfAxis, GenericInputBinding::RightStickLeft},
	{"LargeMotor", "Large (Low Frequency) Motor", PAD::ControllerBindingType::Motor, GenericInputBinding::LargeMotor},
	{"SmallMotor", "Small (High Frequency) Motor", PAD::ControllerBindingType::Motor, GenericInputBinding::SmallMotor},
};

static const PAD::ControllerInfo s_controller_info[] = {
	{"None", "Not Connected", nullptr, 0, PAD::ControllerType::NotConnected, PAD::VibrationCapabilities::NoVibration},
	{"DualShock2", "DualShock 2", s_dualshock2_binds, std::size(s_dualshock2_binds), PAD::ControllerType::DualShock2, PAD::VibrationCapabilities::LargeSmallMotors},
};

const PAD::ControllerInfo* PAD::GetControllerInfo(ControllerType type)
{
	for (const ControllerInfo& info : s_controller_info)
	{
		if (type == info.type)
			return &info;
	}

	return nullptr;
}

const PAD::ControllerInfo* PAD::GetControllerInfo(const std::string_view& name)
{
	for (const ControllerInfo& info : s_controller_info)
	{
		if (name == info.name)
			return &info;
	}

	return nullptr;
}

std::vector<std::pair<std::string, std::string>> PAD::GetControllerTypeNames()
{
	std::vector<std::pair<std::string, std::string>> ret;
	for (const ControllerInfo& info : s_controller_info)
		ret.emplace_back(info.name, info.display_name);

	return ret;
}

std::vector<std::string> PAD::GetControllerBinds(const std::string_view& type)
{
	std::vector<std::string> ret;

	const ControllerInfo* info = GetControllerInfo(type);
	if (info)
	{
		for (u32 i = 0; i < info->num_bindings; i++)
		{
			const ControllerBindingInfo& bi = info->bindings[i];
			if (bi.type == ControllerBindingType::Unknown || bi.type == ControllerBindingType::Motor)
				continue;

			ret.emplace_back(info->bindings[i].name);
		}
	}

	return ret;
}

void PAD::ClearPortBindings(SettingsInterface& si, u32 port)
{
	const std::string section(StringUtil::StdStringFromFormat("Pad%u", port + 1));
	const std::string type(si.GetStringValue(section.c_str(), "Type", GetDefaultPadType(port)));

	const ControllerInfo* info = GetControllerInfo(type);
	if (!info)
		return;

	for (u32 i = 0; i < info->num_bindings; i++)
		si.DeleteValue(section.c_str(), info->bindings[i].name);
}

PAD::VibrationCapabilities PAD::GetControllerVibrationCapabilities(const std::string_view& type)
{
	const ControllerInfo* info = GetControllerInfo(type);
	return info ? info->vibration_caps : VibrationCapabilities::NoVibration;
}

static u32 TryMapGenericMapping(SettingsInterface& si, const std::string& section,
	const GenericInputBindingMapping& mapping, GenericInputBinding generic_name,
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


bool PAD::MapController(SettingsInterface& si, u32 controller,
	const std::vector<std::pair<GenericInputBinding, std::string>>& mapping)
{
	const std::string section(StringUtil::StdStringFromFormat("Pad%u", controller + 1));
	const std::string type(si.GetStringValue(section.c_str(), "Type", GetDefaultPadType(controller)));
	const ControllerInfo* info = GetControllerInfo(type);
	if (!info)
		return false;

	u32 num_mappings = 0;
	for (u32 i = 0; i < info->num_bindings; i++)
	{
		const ControllerBindingInfo& bi = info->bindings[i];
		if (bi.generic_mapping == GenericInputBinding::Unknown)
			continue;

		num_mappings += TryMapGenericMapping(si, section, mapping, bi.generic_mapping, bi.name);
	}
	if (info->vibration_caps == VibrationCapabilities::LargeSmallMotors)
	{
		num_mappings += TryMapGenericMapping(si, section, mapping, GenericInputBinding::SmallMotor, "SmallMotor");
		num_mappings += TryMapGenericMapping(si, section, mapping, GenericInputBinding::LargeMotor, "LargeMotor");
	}
	else if (info->vibration_caps == VibrationCapabilities::SingleMotor)
	{
		if (TryMapGenericMapping(si, section, mapping, GenericInputBinding::LargeMotor, "Motor") == 0)
			num_mappings += TryMapGenericMapping(si, section, mapping, GenericInputBinding::SmallMotor, "Motor");
		else
			num_mappings++;
	}

	return (num_mappings > 0);
}

void PAD::SetControllerState(u32 controller, u32 bind, float value)
{
	if (controller >= GAMEPAD_NUMBER || bind >= MAX_KEYS)
		return;

	g_key_status.Set(controller, bind, value);
}

void PAD::LoadMacroButtonConfig(const SettingsInterface& si, u32 pad, const std::string_view& type, const std::string& section)
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

		s_macro_buttons[pad][i].buttons = std::move(bind_indices);
		s_macro_buttons[pad][i].toggle_frequency = frequency;
	}
}

void PAD::SetMacroButtonState(u32 pad, u32 index, bool state)
{
	if (pad >= GAMEPAD_NUMBER || index >= NUM_MACRO_BUTTONS_PER_CONTROLLER)
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

void PAD::ApplyMacroButton(u32 pad, const MacroButton& mb)
{
	const float value = mb.toggle_state ? 1.0f : 0.0f;
	for (const u32 btn : mb.buttons)
		g_key_status.Set(pad, btn, value);
}

void PAD::UpdateMacroButtons()
{
	for (u32 pad = 0; pad < GAMEPAD_NUMBER; pad++)
	{
		for (u32 index = 0; index < NUM_MACRO_BUTTONS_PER_CONTROLLER; index++)
		{
			MacroButton& mb = s_macro_buttons[pad][index];
			if (!mb.trigger_state || mb.toggle_frequency == 0)
				continue;

			mb.toggle_counter--;
			if (mb.toggle_counter > 0)
				continue;

			mb.toggle_counter = mb.toggle_frequency;
			mb.toggle_state = !mb.toggle_state;
			ApplyMacroButton(pad, mb);
		}
	}
}
