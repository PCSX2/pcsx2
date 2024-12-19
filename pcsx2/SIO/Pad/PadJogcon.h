// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "SIO/Pad/PadBase.h"

class PadJogcon final : public PadBase
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

		// This workaround is necessary because InputRecorder doesn't support custom Pads beside DS2:
		// https://github.com/PCSX2/pcsx2/blob/ded55635c105c547756f5369b998297f602ada2f/pcsx2/Recording/PadData.cpp#L42-L61
		// We need to consider and avoid the DS2's indexes that aren't saved in InputRecorder
		// and we also have to use DS2's analog indexes for our analog axes.
		PADDING1, PADDING2, PADDING3, PADDING4,

		PAD_DIAL_LEFT, // Dial (Left)
		PAD_DIAL_RIGHT, // Dial (Right)
		LENGTH,
	};

	static constexpr u8 VIBRATION_MOTORS = 2;

private:
	u32 buttons = 0xffffffffu;
	s16 dial = 0x0000;
	s16 lastdial = 0x0000;

	bool analogLight = false;
	bool analogLocked = false;
	bool commandStage = false;
	std::array<u8, VIBRATION_MOTORS> vibrationMotors = {};
	std::array<float, 2> vibrationScale = {1.0f, 1.0f};
	float dialDeadzone = 0.0f;
	float dialScale = 1.0f;
	// Used to store the last vibration mapping request the PS2 made for the small motor.
	u8 smallMotorLastConfig = 0xff;
	// Used to store the last vibration mapping request the PS2 made for the large motor.
	u8 largeMotorLastConfig = 0xff;

	// Since we reordered the buttons for better UI, we need to remap them here.
	static constexpr std::array<u8, Inputs::LENGTH> bitmaskMapping = {{
		12, // PAD_UP
		13, // PAD_RIGHT
		14, // PAD_DOWN
		15, // PAD_LEFT
		4, // PAD_TRIANGLE
		5, // PAD_CIRCLE
		6, // PAD_CROSS
		7, // PAD_SQUARE
		8, // PAD_SELECT
		11, // PAD_START
		2, // PAD_L1
		0, // PAD_L2
		3, // PAD_R1
		1, // PAD_R2
	}};

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

public:
	PadJogcon(u8 unifiedSlot, size_t ejectTicks);
	~PadJogcon() override;

	static inline bool IsAnalogKey(int index)
	{
		return index == Inputs::PAD_DIAL_LEFT || index == Inputs::PAD_DIAL_RIGHT;
	}

	Pad::ControllerType GetType() const override;
	const Pad::ControllerInfo& GetInfo() const override;
	void Set(u32 index, float value) override;
	void SetRawAnalogs(const std::tuple<u8, u8> left, const std::tuple<u8, u8> right) override;
	void SetRawPressureButton(u32 index, const std::tuple<bool, u8> value) override;
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
