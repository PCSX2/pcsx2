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

#include "HostSettings.h"

#include "PAD/Host/Global.h"
#include "PAD/Host/PAD.h"
#include "PAD/Host/KeyStatus.h"
#include "PAD/Host/StateManagement.h"

const u32 revision = 3;
const u32 build = 0; // increase that with each version
#define PAD_SAVE_STATE_VERSION ((revision << 8) | (build << 0))

KeyStatus g_key_status;

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
	// This is where we would load controller types, if onepad supported them.

	for (u32 i = 0; i < GAMEPAD_NUMBER; i++)
	{
		const std::string section(StringUtil::StdStringFromFormat("Pad%u", i + 1u));
		const float axis_scale = si.GetFloatValue(section.c_str(), "AxisScale", 1.0f);
		const float large_motor_scale = si.GetFloatValue(section.c_str(), "LargeMotorScale", 1.0f);
		const float small_motor_scale = si.GetFloatValue(section.c_str(), "SmallMotorScale", 1.0f);

		g_key_status.SetAxisScale(i, axis_scale);
		g_key_status.SetVibrationScale(i, 0, large_motor_scale);
		g_key_status.SetVibrationScale(i, 1, small_motor_scale);
	}
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
	si.SetStringValue("Pad1", "Type", "DualShock2");
	si.SetStringValue("Pad1", "Up", "Keyboard/Up");
	si.SetStringValue("Pad1", "Right", "Keyboard/Right");
	si.SetStringValue("Pad1", "Down", "Keyboard/Down");
	si.SetStringValue("Pad1", "Left", "Keyboard/Left");
	si.SetStringValue("Pad1", "LUp", "Keyboard/W");
	si.SetStringValue("Pad1", "LRight", "Keyboard/D");
	si.SetStringValue("Pad1", "LDown", "Keyboard/S");
	si.SetStringValue("Pad1", "LLeft", "Keyboard/A");
	si.SetStringValue("Pad1", "RUp", "Keyboard/T");
	si.SetStringValue("Pad1", "RRight", "Keyboard/H");
	si.SetStringValue("Pad1", "RDown", "Keyboard/G");
	si.SetStringValue("Pad1", "RLeft", "Keyboard/F");
	si.SetStringValue("Pad1", "Triangle", "Keyboard/I");
	si.SetStringValue("Pad1", "Circle", "Keyboard/L");
	si.SetStringValue("Pad1", "Cross", "Keyboard/K");
	si.SetStringValue("Pad1", "Square", "Keyboard/J");
	si.SetStringValue("Pad1", "L1", "Keyboard/Q");
	si.SetStringValue("Pad1", "L2", "Keyboard/1");
	si.SetStringValue("Pad1", "L3", "Keyboard/2");
	si.SetStringValue("Pad1", "R1", "Keyboard/E");
	si.SetStringValue("Pad1", "R2", "Keyboard/3");
	si.SetStringValue("Pad1", "R3", "Keyboard/4");
	si.SetStringValue("Pad1", "Start", "Keyboard/Return");
	si.SetStringValue("Pad1", "Select", "Keyboard/Backspace");
	
	// PCSX2 Controller Settings - Hotkeys
	
	// PCSX2 Controller Settings - Hotkeys - General
	si.SetStringValue("Hotkeys", "Screenshot", "Keyboard/F8");
	si.SetStringValue("Hotkeys", "ToggleFullscreen", "Keyboard/Alt & Keyboard/Return");
	
	// PCSX2 Controller Settings - Hotkeys - Graphics
	si.SetStringValue("Hotkeys", "CycleAspectRatio", "Keyboard/F6");
	si.SetStringValue("Hotkeys", "CycleMipmapMode", "Keyboard/Insert");
	si.SetStringValue("Hotkeys", "CycleInterlaceMode", "Keyboard/F5");
//	si.SetStringValue("Hotkeys", "DecreaseUpscaleMultiplier", "Keyboard"); TBD 
//	si.SetStringValue("Hotkeys", "IncreaseUpscaleMultiplier", "Keyboard"); TBD 
	si.SetStringValue("Hotkeys", "ToggleSoftwareRendering", "Keyboard/F9");
	si.SetStringValue("Hotkeys", "ZoomIn", "Keyboard/Control & Keyboard/Plus");
	si.SetStringValue("Hotkeys", "ZoomOut", "Keyboard/Control & Keyboard/Minus");
// Missing hotkey for resetting zoom back to 100 with Keyboard/Control & Keyboard/Asterisk

	// PCSX2 Controller Settings - Hotkeys - Save States
	si.SetStringValue("Hotkeys", "LoadStateFromSlot", "Keyboard/F3");
	si.SetStringValue("Hotkeys", "SaveStateToSlot", "Keyboard/F1");
	si.SetStringValue("Hotkeys", "NextSaveStateSlot", "Keyboard/F2");
	si.SetStringValue("Hotkeys", "PreviousSaveStateSlot", "Keyboard/Shift & Keyboard/F2");

	// PCSX2 Controller Settings - Hotkeys - System
//	si.SetStringValue("Hotkeys", "DecreaseSpeed", "Keyboard"); TBD 
//	si.SetStringValue("Hotkeys", "IncreaseSpeed", "Keyboard"); TBD 
	si.SetStringValue("Hotkeys", "ToggleFrameLimit", "Keyboard/F4");
	si.SetStringValue("Hotkeys", "TogglePause", "Keyboard/Space");	
	si.SetStringValue("Hotkeys", "ToggleSlowMotion", "Keyboard/Shift & Keyboard/Backtab");	
	si.SetStringValue("Hotkeys", "ToggleTurbo", "Keyboard/Tab");
}

void PAD::Update()
{
	Pad::rumble_all();
}

std::vector<std::string> PAD::GetControllerTypeNames()
{
	return {"DualShock2"};
}

std::vector<std::string> PAD::GetControllerBinds(const std::string_view& type)
{
	if (type == "DualShock2")
	{
		return {
			"Up",
			"Right",
			"Down",
			"Left",
			"Triangle",
			"Circle",
			"Cross",
			"Square",
			"Select",
			"Start",
			"L1",
			"L2",
			"R1",
			"R2",
			"L3",
			"R3",
			"LUp",
			"LRight",
			"LDown",
			"LLeft",
			"RUp",
			"RRight",
			"RDown",
			"RLeft"};
	}

	return {};
}

PAD::VibrationCapabilities PAD::GetControllerVibrationCapabilities(const std::string_view& type)
{
	if (type == "DualShock2")
	{
		return VibrationCapabilities::LargeSmallMotors;
	}
	else
	{
		return VibrationCapabilities::NoVibration;
	}
}

void PAD::SetControllerState(u32 controller, u32 bind, float value)
{
	if (controller >= GAMEPAD_NUMBER || bind >= MAX_KEYS)
		return;

	g_key_status.Set(controller, bind, value);
}
