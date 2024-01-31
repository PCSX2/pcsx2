// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

	static constexpr u8 VIBRATION_MOTORS = 2;

private:
	struct Analogs
	{
		u8 lx = Pad::ANALOG_NEUTRAL_POSITION;
		u8 ly = Pad::ANALOG_NEUTRAL_POSITION;
		u8 rx = Pad::ANALOG_NEUTRAL_POSITION;
		u8 ry = Pad::ANALOG_NEUTRAL_POSITION;
		bool lxInvert = false;
		bool lyInvert = false;
		bool rxInvert = false;
		bool ryInvert = false;
	};

	u32 buttons = 0xffffffffu;
	Analogs analogs;
	bool analogLight = false;
	bool analogLocked = false;
	// Analog button can be held without changing its state.
	// We track here if it is currently held down, to avoid flipping in
	// and out of analog mode every frame.
	bool analogPressed = false;
	bool commandStage = false;
	u32 responseBytes = 0;
	std::array<u8, VIBRATION_MOTORS> vibrationMotors = {};
	float axisScale = 1.0f;
	float axisDeadzone = 0.0f;
	std::array<float, 2> vibrationScale = {1.0f, 1.0f};
	// When the pressure modifier binding is activated, this is multiplied against
	// all values in pressures, to artificially reduce pressures and give players
	// a way to simulate pressure sensitive controls.
	float pressureModifier = 0.5f;
	float buttonDeadzone = 0.0f;
	// Used to store the last vibration mapping request the PS2 made for the small motor.
	u8 smallMotorLastConfig = 0xff;
	// Used to store the last vibration mapping request the PS2 made for the large motor.
	u8 largeMotorLastConfig = 0xff;

	void ConfigLog();

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
	PadDualshock2(u8 unifiedSlot, size_t ejectTicks);
	~PadDualshock2() override;

	static inline bool IsAnalogKey(int index)
	{
		return ((index >= Inputs::PAD_L_UP) && (index <= Inputs::PAD_R_LEFT));
	}

	static inline bool IsTriggerKey(int index)
	{
		return (index == Inputs::PAD_L2 || index == Inputs::PAD_R2);
	}

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
	float GetEffectiveInput(u32 index) const override;
	u8 GetRawInput(u32 index) const override;
	std::tuple<u8, u8> GetRawLeftAnalog() const override;
	std::tuple<u8, u8> GetRawRightAnalog() const override;
	u32 GetButtons() const override;
	u8 GetPressure(u32 index) const override;

	bool Freeze(StateWrapper& sw) override;

	u8 SendCommandByte(u8 commandByte) override;

	static const Pad::ControllerInfo ControllerInfo;
};
