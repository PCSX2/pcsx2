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

#include <array>

static inline bool IsButtonBitSet(u32 value, size_t bit)
{
	return !(value & (1 << bit));
}

class PadDualshock2 final : public PadBase
{
public:
	enum Inputs
	{
		PAD_UP, // Directional pad up
		PAD_RIGHT, // Directional pad right
		PAD_DOWN, // Directional pad down
		PAD_LEFT, // Directional pad left
		PAD_TRIANGLE, // Triangle button 
		PAD_CIRCLE, // Circle button 
		PAD_CROSS, // Cross button 
		PAD_SQUARE, // Square button 
		PAD_SELECT, // Select button
		PAD_START, // Start button
		PAD_L1, // L1 button
		PAD_L2, // L2 button
		PAD_R1, // R1 button
		PAD_R2, // R2 button
		PAD_L3, // Left joystick button (L3)
		PAD_R3, // Right joystick button (R3)
		PAD_ANALOG, // Analog mode toggle
		PAD_PRESSURE, // Pressure modifier
		PAD_L_UP, // Left joystick (Up) 
		PAD_L_RIGHT, // Left joystick (Right) 
		PAD_L_DOWN, // Left joystick (Down) 
		PAD_L_LEFT, // Left joystick (Left) 
		PAD_R_UP, // Right joystick (Up) 
		PAD_R_RIGHT, // Right joystick (Right) 
		PAD_R_DOWN, // Right joystick (Down) 
		PAD_R_LEFT, // Right joystick (Left) 
		LENGTH,
	};

	static constexpr u32 PRESSURE_BUTTONS = 12;
	static constexpr u8 VIBRATION_MOTORS = 2;

private:
	struct Analogs
	{
		u8 lx = 0x7f;
		u8 ly = 0x7f;
		u8 rx = 0x7f;
		u8 ry = 0x7f;
		u8 lxInvert = 0x7f;
		u8 lyInvert = 0x7f;
		u8 rxInvert = 0x7f;
		u8 ryInvert = 0x7f;
	};

	u32 buttons;
	Analogs analogs;
	bool analogLight = false;
	bool analogLocked = false;
	// Analog button can be held without changing its state.
	// We track here if it is currently held down, to avoid flipping in
	// and out of analog mode every frame.
	bool analogPressed = false;
	bool commandStage = false;
	u32 responseBytes;
	std::array<u8, PRESSURE_BUTTONS> pressures;
	std::array<u8, VIBRATION_MOTORS> vibrationMotors;
	float axisScale;
	float axisDeadzone;
	std::array<float, 2> vibrationScale;
	// When the pressure modifier binding is activated, this is multiplied against
	// all values in pressures, to artificially reduce pressures and give players
	// a way to simulate pressure sensitive controls.
	float pressureModifier;
	float buttonDeadzone;
	// Used to store the last vibration mapping request the PS2 made for the small motor.
	u8 smallMotorLastConfig = 0xff;
	// Used to store the last vibration mapping request the PS2 made for the large motor.
	u8 largeMotorLastConfig = 0xff;

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
	~PadDualshock2() override;

	static inline bool IsAnalogKey(int index)
	{
		return ((index >= Inputs::PAD_L_UP) && (index <= Inputs::PAD_R_LEFT));
	}

	static inline bool IsTriggerKey(int index)
	{
		return (index == Inputs::PAD_L2 || index == Inputs::PAD_R2);
	}

	void Init() override;
	Pad::ControllerType GetType() const override;
	const Pad::ControllerInfo& GetInfo() const override;
	void Set(u32 index, float value) override;
	void SetRawAnalogs(const std::tuple<u8, u8> left, const std::tuple<u8, u8> right) override;
	void SetAxisScale(float deadzone, float scale) override;
	float GetVibrationScale(u32 motor) const override;
	void SetVibrationScale(u32 motor, float scale) override;
	float GetPressureModifier() const override;
	void SetPressureModifier(float mod) override;
	void SetButtonDeadzone(float deadzone) override;
	void SetAnalogInvertL(bool x, bool y) override;
	void SetAnalogInvertR(bool x, bool y) override;
	u8 GetRawInput(u32 index) const override;
	std::tuple<u8, u8> GetRawLeftAnalog() const override;
	std::tuple<u8, u8> GetRawRightAnalog() const override;
	u32 GetButtons() const override;
	u8 GetPressure(u32 index) const override;

	bool Freeze(StateWrapper& sw) override;

	u8 SendCommandByte(u8 commandByte) override;

	static const Pad::ControllerInfo ControllerInfo;
};
