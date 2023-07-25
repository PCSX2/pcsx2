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

#pragma once

#include "SIO/Pad/PadBase.h"
#include "SIO/Pad/PadDualshock2Types.h"

#include <array>

static inline bool IsButtonBitSet(u32 value, size_t bit)
{
	return !(value & (1 << bit));
}

static inline bool IsAnalogKey(int index)
{
	return ((index >= Dualshock2::Inputs::PAD_L_UP) && (index <= Dualshock2::Inputs::PAD_R_LEFT));
}

static inline bool IsTriggerKey(int index)
{
	return (index == Dualshock2::Inputs::PAD_L2 || index == Dualshock2::Inputs::PAD_R2);
}

class PadDualshock2 : public PadBase
{
private:
	u32 buttons;
	Dualshock2::Analogs analogs;
	bool analogLight = false;
	bool analogLocked = false;
	// Analog button can be held without changing its state.
	// We track here if it is currently held down, to avoid flipping in
	// and out of analog mode every frame.
	bool analogPressed = false;
	bool commandStage = false;
	u32 responseBytes;
	std::array<u8, Dualshock2::PRESSURE_BUTTONS> pressures;
	std::array<u8, Dualshock2::VIBRATION_MOTORS> vibrationMotors;
	float axisScale;
	float axisDeadzone;
	float triggerScale;
	float triggerDeadzone;
	std::array<float, 2> vibrationScale;
	// When the pressure modifier binding is activated, this is multiplied against
	// all values in pressures, to artificially reduce pressures and give players
	// a way to simulate pressure sensitive controls.
	float pressureModifier;
	float buttonDeadzone;

	u8 Mystery(u8 commandByte);
	u8 ButtonQuery(u8 commandByte);
	u8 Poll(u8 commandByte);
	u8 Config(u8 commandByte);
	u8 ModeSwitch(u8 commandByte);
	u8 StatusInfo(u8 commandByte);
	u8 Constant1(u8 commandByte);
	u8 Constant2(u8 commandByte);
	u8 Constant3(u8 commandByte);
	u8 VibrationMap(u8 commandByte);
	u8 ResponseBytes(u8 commandByte);

public:
	PadDualshock2(u8 unifiedSlot);
	virtual ~PadDualshock2();

	void Init() override;
	Pad::ControllerType GetType() override;
	void Set(u32 index, float value) override;
	void SetRawAnalogs(const std::tuple<u8, u8> left, const std::tuple<u8, u8> right) override;
	void SetAxisScale(float deadzone, float scale) override;
	void SetTriggerScale(float deadzone, float scale) override;
	float GetVibrationScale(u32 motor) override;
	void SetVibrationScale(u32 motor, float scale) override;
	float GetPressureModifier() override;
	void SetPressureModifier(float mod) override;
	void SetButtonDeadzone(float deadzone) override;
	void SetAnalogInvertL(bool x, bool y) override;
	void SetAnalogInvertR(bool x, bool y) override;
	u8 GetRawInput(u32 index) override;
	std::tuple<u8, u8> GetRawLeftAnalog() override;
	std::tuple<u8, u8> GetRawRightAnalog() override;
	u32 GetButtons() override;
	u8 GetPressure(u32 index) override;

	bool Freeze(StateWrapper& sw) override;

	u8 SendCommandByte(u8 commandByte) override;
};
