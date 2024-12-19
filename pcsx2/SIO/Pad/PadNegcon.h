// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "SIO/Pad/PadBase.h"

class PadNegcon final : public PadBase
{
public:
	enum Inputs
	{
		PAD_UP, // Directional pad up
		PAD_RIGHT, // Directional pad right
		PAD_DOWN, // Directional pad down
		PAD_LEFT, // Directional pad left
		PAD_B, // B button
		PAD_A, // A button
		PAD_I, // I button
		PAD_II, // II button

		// This workaround is necessary because InputRecorder doesn't support custom Pads beside DS2:
		// https://github.com/PCSX2/pcsx2/blob/ded55635c105c547756f5369b998297f602ada2f/pcsx2/Recording/PadData.cpp#L42-L61
		// We need to consider and avoid the DS2's indexes that aren't saved in InputRecorder
		// and we also have to use DS2's analog indexes for our analog axes.
		PAD_PADDING1,

		PAD_START, // Start button
		PAD_L, // L button
		PAD_R, // R button
		PAD_TWIST_LEFT, // Twist (Left)
		PAD_TWIST_RIGHT, // Twist (Right)
		LENGTH,
	};

	static constexpr u8 VIBRATION_MOTORS = 2;

private:
	struct Analogs
	{
		u8 i = 0x00;
		u8 ii = 0x00;
		u8 l = 0x00;
		u8 twist = 0x80;
	};

	u32 buttons = 0xffffffffu;
	Analogs analogs;

	bool analogLight = false;
	bool analogLocked = false;
	bool commandStage = false;
	std::array<u8, VIBRATION_MOTORS> vibrationMotors = {};
	std::array<float, 2> vibrationScale = {1.0f, 1.0f};
	float twistDeadzone = 0.0f;
	float twistScale = 1.0f;
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
		4, // PAD_B
		5, // PAD_A
		6, // PAD_I
		7, // PAD_II
		0,
		11, // PAD_START
		2, // PAD_L
		3, // PAD_R
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
	PadNegcon(u8 unifiedSlot, size_t ejectTicks);
	~PadNegcon() override;

	static inline bool IsAnalogKey(int index)
	{
		return index == Inputs::PAD_I
			|| index == Inputs::PAD_II
			|| index == Inputs::PAD_L
			|| index == Inputs::PAD_TWIST_LEFT
			|| index == Inputs::PAD_TWIST_RIGHT;
	}

	static inline bool IsTwistKey(int index)
	{
		return index == Inputs::PAD_TWIST_LEFT
			|| index == Inputs::PAD_TWIST_RIGHT;
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
