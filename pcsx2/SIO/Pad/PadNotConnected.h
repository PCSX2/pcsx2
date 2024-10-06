// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "SIO/Pad/PadBase.h"

class PadNotConnected final : public PadBase
{
public:
	PadNotConnected(u8 unifiedSlot, size_t ejectTicks = 0);
	~PadNotConnected() override;

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

	u8 SendCommandByte(u8 commandByte) override;

	static const Pad::ControllerInfo ControllerInfo;
};
