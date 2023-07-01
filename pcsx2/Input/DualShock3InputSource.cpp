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

#include "Input/DualShock3InputSource.h"
#include "Input/InputManager.h"

#include "Input/WindowsHIDUtility.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"

DualShock3InputSource::DualShock3InputSource() noexcept = default;

DualShock3InputSource::~DualShock3InputSource() = default;

bool DualShock3InputSource::Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{	
	ReloadDevices();
	return true;
}

void DualShock3InputSource::UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{
}

bool DualShock3InputSource::ReloadDevices()
{
	bool changed = false;

	const auto foundDS3Hids = WindowsHIDUtility::FindHids(DS3_VID, DS3_PID);

	if (std::empty(foundDS3Hids))
	{
		return changed;
	}

	for (std::size_t i = 0; i < std::size(foundDS3Hids); ++i)
	{
		//extra check to see if it really is a DualShock 3 controller
		if (foundDS3Hids[i].caps.FeatureReportByteLength == 50 &&
			foundDS3Hids[i].caps.OutputReportByteLength == 49)
		{
			//check we don't already have the DS3 controller using unique DevicePath
			if (!std::empty(m_controllers))
			{
				if (std::any_of(m_controllers.begin(), m_controllers.end(),
						[&foundDS3HidPath = std::as_const(foundDS3Hids[i].device_path)](const DS3ControllerData& cd) { return cd.device_path == foundDS3HidPath; }))
				{
					continue;
				}
			}

			DS3ControllerData ds3 = {GetFreePlayerId(), foundDS3Hids[i].device_path};

			const bool ds3_activated = ds3.Activate();

			if (ds3_activated)
			{
				InputManager::OnInputDeviceConnected(StringUtil::StdStringFromFormat("DS3-%d", ds3.player_id), 
					StringUtil::StdStringFromFormat("DualShock 3 Controller %d", ds3.player_id));

				m_controllers.push_back(ds3);

				changed = true;
			}
		}
	}

	return changed;
}

void DualShock3InputSource::Shutdown()
{
	while (!std::empty(m_controllers))
	{
		m_controllers.back().Deactivate();
		InputManager::OnInputDeviceDisconnected(StringUtil::StdStringFromFormat("DS3-%d", m_controllers.back().player_id));
		m_controllers.pop_back();
	}
}

void DualShock3InputSource::PollEvents()
{
	size_t controllerindex = 0;
	while (controllerindex < m_controllers.size())
	{
		if (!m_controllers[controllerindex].active)
		{
			++controllerindex;
			continue;
		}			

		u8 Buffer[50] = {};

		//Buffer[0] = 0; //reportId

		if (!HidD_GetFeature(m_controllers[controllerindex].hFile, Buffer, sizeof(Buffer)))
		{		
			//looks like DS3 controller has been disconnected - deactivate and erase from m_controllers 			 
			m_controllers[controllerindex].Deactivate();
			InputManager::OnInputDeviceDisconnected(StringUtil::StdStringFromFormat("DS3-%d", m_controllers[controllerindex].player_id));
			m_controllers.erase(m_controllers.begin() + controllerindex);
			continue;
		}

		const u8* getState = &Buffer[1];

		//Buttons
		m_controllers[controllerindex].physicalButtonState[0] = getState[2] & 0x01; //SELECT
		m_controllers[controllerindex].physicalButtonState[1] = getState[2] & 0x02; //L3
		m_controllers[controllerindex].physicalButtonState[2] = getState[2] & 0x04; //R3
		m_controllers[controllerindex].physicalButtonState[3] = getState[2] & 0x08; //START
		m_controllers[controllerindex].physicalButtonState[4] = getState[2] & 0x10; //Dpad Up
		m_controllers[controllerindex].physicalButtonState[5] = getState[2] & 0x20; //Dpad Right
		m_controllers[controllerindex].physicalButtonState[6] = getState[2] & 0x40; //Dpad Down
		m_controllers[controllerindex].physicalButtonState[7] = getState[2] & 0x80; //Dpad Left
		m_controllers[controllerindex].physicalButtonState[8] = getState[3] & 0x01; //L2
		m_controllers[controllerindex].physicalButtonState[9] = getState[3] & 0x02; //R2
		m_controllers[controllerindex].physicalButtonState[10] = getState[3] & 0x04; //L1
		m_controllers[controllerindex].physicalButtonState[11] = getState[3] & 0x08; //R1
		m_controllers[controllerindex].physicalButtonState[12] = getState[3] & 0x10; //Triangle
		m_controllers[controllerindex].physicalButtonState[13] = getState[3] & 0x20; //Circle
		m_controllers[controllerindex].physicalButtonState[14] = getState[3] & 0x40; //Cross
		m_controllers[controllerindex].physicalButtonState[15] = getState[3] & 0x80; //Square
		m_controllers[controllerindex].physicalButtonState[16] = getState[4] & 0x01; //PS Button	

		//If we ever want to support pressure sensitive buttons
		//m_controllers[controllerindex].physicalButtonPressureState[0] = getState[14]; //Dpad Up
		//m_controllers[controllerindex].physicalButtonPressureState[1] = getState[15]; //Dpad Right
		//m_controllers[controllerindex].physicalButtonPressureState[2] = getState[16]; //Dpad Down
		//m_controllers[controllerindex].physicalButtonPressureState[3] = getState[17]; //Dpad Left
		//m_controllers[controllerindex].physicalButtonPressureState[4] = getState[18]; //L2
		//m_controllers[controllerindex].physicalButtonPressureState[5] = getState[19]; //R2
		//m_controllers[controllerindex].physicalButtonPressureState[6] = getState[20]; //L1
		//m_controllers[controllerindex].physicalButtonPressureState[7] = getState[21]; //R1
		//m_controllers[controllerindex].physicalButtonPressureState[8] = getState[22]; //Triangle
		//m_controllers[controllerindex].physicalButtonPressureState[9] = getState[23]; //Circle
		//m_controllers[controllerindex].physicalButtonPressureState[10] = getState[24]; //Cross
		//m_controllers[controllerindex].physicalButtonPressureState[11] = getState[25]; //Square

		//Axis	
		m_controllers[controllerindex].physicalAxisState[0] = getState[6]; //Left Stick X
		m_controllers[controllerindex].physicalAxisState[1] = getState[7]; //Left Stick Y
		m_controllers[controllerindex].physicalAxisState[2] = getState[8]; //Right Stick X
		m_controllers[controllerindex].physicalAxisState[3] = getState[9]; //Right Stick Y

		//Buttons
		for (size_t btnindex = 0; btnindex < std::size(m_controllers[controllerindex].physicalButtonState); ++btnindex)
		{
			if (m_controllers[controllerindex].lastPhysicalButtonState[btnindex] != m_controllers[controllerindex].physicalButtonState[btnindex])
			{
				m_controllers[controllerindex].lastPhysicalButtonState[btnindex] = m_controllers[controllerindex].physicalButtonState[btnindex];

				const float button_value = m_controllers[controllerindex].physicalButtonState[btnindex];	

				InputManager::InvokeEvents(MakeGenericControllerButtonKey(InputSourceType::DS3Input, static_cast<u32>(controllerindex), btnindex), button_value, GenericInputBinding::Unknown);
			}
		}

		//Axis
		for (size_t axisindex = 0; axisindex < std::size(m_controllers[controllerindex].physicalAxisState); ++axisindex)
		{
			if (m_controllers[controllerindex].lastPhysicalAxisState[axisindex] != m_controllers[controllerindex].physicalAxisState[axisindex]) 
			{
				m_controllers[controllerindex].lastPhysicalAxisState[axisindex] = m_controllers[controllerindex].physicalAxisState[axisindex];								

				const float axis_value = ConvertDS3Axis(m_controllers[controllerindex].physicalAxisState[axisindex]);						

				InputManager::InvokeEvents(MakeGenericControllerAxisKey(InputSourceType::DS3Input, static_cast<u32>(controllerindex), axisindex), axis_value, GenericInputBinding::Unknown);
			}

		}		

		 ++controllerindex;
	}
}

std::vector<std::pair<std::string, std::string>> DualShock3InputSource::EnumerateDevices()
{
	std::vector<std::pair<std::string, std::string>> ret;

	for (const auto& controller : m_controllers)
	{
		if (!controller.active)
			continue;

		ret.emplace_back(StringUtil::StdStringFromFormat("DS3-%d", controller.player_id),
			StringUtil::StdStringFromFormat("DualShock 3 Controller %d", controller.player_id));
	}

	return ret;
}

std::vector<InputBindingKey> DualShock3InputSource::EnumerateMotors()
{
	std::vector<InputBindingKey> ret;

	for (const auto& controller : m_controllers)
	{
		if (!controller.active)
			continue;

		ret.emplace_back(MakeGenericControllerMotorKey(InputSourceType::DS3Input, controller.player_id, 0)); //large motor
		ret.emplace_back(MakeGenericControllerMotorKey(InputSourceType::DS3Input, controller.player_id, 1)); //small motor
	}

	return ret;
}

bool DualShock3InputSource::GetGenericBindingMapping(const std::string_view& device, InputManager::GenericInputBindingMapping* mapping)
{

	if (!StringUtil::StartsWith(device, "DS3-"))
		return false;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(4));
	if (!player_id.has_value() || player_id.value() < 0)
		return false;

	const s32 pid = player_id.value();

	for (u32 i = 0; i < std::size(DualShock3GenericAxisMapping); ++i)
	{
		const GenericInputBinding negative = DualShock3GenericAxisMapping[i][0];
		const GenericInputBinding positive = DualShock3GenericAxisMapping[i][1];
		if (negative != GenericInputBinding::Unknown)
			mapping->emplace_back(negative, StringUtil::StdStringFromFormat("DS3-%d/-%s", pid, DualShock3AxisNames[i]));

		if (positive != GenericInputBinding::Unknown)
			mapping->emplace_back(positive, StringUtil::StdStringFromFormat("DS3-%d/+%s", pid, DualShock3AxisNames[i]));
	}
	for (u32 i = 0; i < std::size(DualShock3GenericButtonMapping); ++i)
	{
		const GenericInputBinding binding = DualShock3GenericButtonMapping[i];
		if (binding != GenericInputBinding::Unknown)
			mapping->emplace_back(binding, StringUtil::StdStringFromFormat("DS3-%d/%s", pid, DualShock3ButtonNames[i]));
	}

	mapping->emplace_back(GenericInputBinding::SmallMotor, StringUtil::StdStringFromFormat("DS3-%d/SmallMotor", pid));
	mapping->emplace_back(GenericInputBinding::LargeMotor, StringUtil::StdStringFromFormat("DS3-%d/LargeMotor", pid));

	return true;
}

void DualShock3InputSource::UpdateMotorState(InputBindingKey key, float intensity)
{
}

void DualShock3InputSource::UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity)
{
	if (large_key.source_index != small_key.source_index || large_key.source_subtype != InputSubclass::ControllerMotor ||
		small_key.source_subtype != InputSubclass::ControllerMotor)
	{
		return;
	}

	const auto ctrlr_iter = GetDS3ControllerDataForPlayerId(large_key.source_index);
	if (ctrlr_iter == m_controllers.end() || !ctrlr_iter->active)
	{
		return;
	}
	
	ctrlr_iter->SmallMotorOn = static_cast<u8>(small_intensity);
	ctrlr_iter->LargeMotorForce = static_cast<u8>(std::round(large_intensity * 255.0f)); //scale intensity to range [0, 255] 

	u8 outputReport[49] = {}; 

	//outputReport[0] = 0; //reportId

	//This is command for sixaxis driver to set motors
	outputReport[1] = SIXASIS_DRIVER_COMMANDS::SIXAXIS_COMMAND_SET_MOTORS;

	outputReport[5] = 0xFF; //small motor duration - 0xFF is forever
	outputReport[6] = ctrlr_iter->SmallMotorOn; 

	outputReport[7] = 0xFF; //large motor duration - 0xFF is forever
	outputReport[8] = ctrlr_iter->LargeMotorForce;

	DWORD lpNumberOfBytesWritten = 0;
	const BOOL wfRes = WriteFile(ctrlr_iter->hFile, outputReport, sizeof(outputReport), &lpNumberOfBytesWritten, NULL);
	if (!wfRes) 
	{
		const DWORD wfError = GetLastError();
		DevCon.WriteLn("DS3: WriteFile failed with error code: %lu", wfError);
	}
}

std::optional<InputBindingKey> DualShock3InputSource::ParseKeyString(const std::string_view& device, const std::string_view& binding)
{
	if (!StringUtil::StartsWith(device, "DS3-") || binding.empty())
		return std::nullopt;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(4));
	if (!player_id.has_value() || player_id.value() < 0)
		return std::nullopt;

	InputBindingKey key = {};
	key.source_type = InputSourceType::DS3Input;
	key.source_index = static_cast<u32>(player_id.value());

	if (StringUtil::EndsWith(binding, "Motor"))
	{
		key.source_subtype = InputSubclass::ControllerMotor;
		if (binding == "LargeMotor")
		{
			key.data = 0;
			return key;
		}
		else if (binding == "SmallMotor")
		{
			key.data = 1;
			return key;
		}
		else
		{
			return std::nullopt;
		}
	}	
	else if (binding[0] == '+' || binding[0] == '-') //Axes
	{		
		const std::string_view axis_name(binding.substr(1));		

		for (std::size_t i = 0; i < std::size(DualShock3AxisNames); ++i)
		{
			if (axis_name == DualShock3AxisNames[i])
			{
				// found an axis!
				key.source_subtype = InputSubclass::ControllerAxis;
				key.data = i;
				key.modifier = (binding[0] == '-') ? InputModifier::Negate : InputModifier::None;
				return key;
			}
		}
	}	
	else //Buttons
	{
		for (std::size_t i = 0; i < std::size(DualShock3ButtonNames); ++i)
		{
			if (binding == DualShock3ButtonNames[i])
			{
				key.source_subtype = InputSubclass::ControllerButton;
				key.data = i;
				return key;
			}
		}
	}

	// unknown axis/button - should not reach here
	return std::nullopt;
}

std::string DualShock3InputSource::ConvertKeyToString(InputBindingKey key)
{
	std::string ret;

	if (key.source_type == InputSourceType::DS3Input)
	{
		if (key.source_subtype == InputSubclass::ControllerAxis)
		{
			const char modifier = key.modifier == InputModifier::Negate ? '-' : '+';

			if (key.data < std::size(DualShock3AxisNames))
				ret = StringUtil::StdStringFromFormat("DS3-%u/%c%s", key.source_index, modifier, DualShock3AxisNames[key.data]);
			else
				ret = StringUtil::StdStringFromFormat("DS3-%u/%cAxis%u%s", key.source_index, modifier, key.data, key.invert ? "~" : ""); // shouldn't be called
		}
		else if (key.source_subtype == InputSubclass::ControllerButton)
		{
			if (key.data < std::size(DualShock3ButtonNames))
				ret = StringUtil::StdStringFromFormat("DS3-%u/%s", key.source_index, DualShock3ButtonNames[key.data]);
			else
				ret = StringUtil::StdStringFromFormat("DS3-%u/Button%u", key.source_index, key.data); // shouldn't be called
		}		
		else if (key.source_subtype == InputSubclass::ControllerMotor)
		{
			ret = StringUtil::StdStringFromFormat("DS3-%u/%sMotor", key.source_index, key.data ? "Large" : "Small");
		}
	}

	return ret;
}

//convert raw DS3 axis value in range [0, 255] to PCSX2 range [-1.0, 1.0]
float DualShock3InputSource::ConvertDS3Axis(u8 axis_val) const
{
	const float adjusted_axis_val = axis_val - 128.0f;
	return adjusted_axis_val / (adjusted_axis_val < 0.0f ? 128.0f : 127.0f);
}

DualShock3InputSource::DS3ControllerDataVector::iterator DualShock3InputSource::GetDS3ControllerDataForPlayerId(int id)
{
	return std::find_if(m_controllers.begin(), m_controllers.end(), [id](const DS3ControllerData& cd) { return cd.player_id == id; });
}

int DualShock3InputSource::GetFreePlayerId() const
{
	for (int player_id = 0;; ++player_id)
	{
		size_t i = 0;
		for (; i < m_controllers.size(); ++i)
		{
			if (m_controllers[i].player_id == player_id)
				break;
		}
		if (i == m_controllers.size())
			return player_id;
	}

	return 0;
}

bool DualShock3InputSource::DS3ControllerData::Activate()
{
	hFile = CreateFileW(device_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		const DWORD cfError = GetLastError();
		DevCon.WriteLn("DS3: CreateFileW failed with error code: %lu", cfError);
		return false;
	}
	active = true;
	return true; 
}

void DualShock3InputSource::DS3ControllerData::Deactivate()
{
	if (hFile != INVALID_HANDLE_VALUE)
	{
		CancelIo(hFile);
		CloseHandle(hFile);
		hFile = INVALID_HANDLE_VALUE;		
	}

	SmallMotorOn = 0;
	LargeMotorForce = 0;	

	active = false;
}
