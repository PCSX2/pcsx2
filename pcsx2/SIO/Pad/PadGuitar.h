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

class PadGuitar final : public PadBase
{
public:
	enum Inputs
	{
		STRUM_UP, // Strum bar
		STRUM_DOWN, // Strum bar down
		SELECT, // Select button
		START, // Start button
		GREEN, // Green fret
		RED, // Red fret
		YELLOW, // Yellow fret
		BLUE, // Blue fret
		ORANGE, // Orange fret
		WHAMMY, // Whammy bar axis
		TILT, // Tilt sensor
		LENGTH,
	};

private:
	u32 buttons = 0xffffffffu;
	u8 whammy = Pad::ANALOG_NEUTRAL_POSITION;
	// Technically guitars do not have an analog light, but they still use the same ModeSwitch command
	// as a DS2, and are told to "turn on their light".
	bool analogLight = false;
	// Guitars are also instructed to "lock" their "analog light", despite not having one.
	bool analogLocked = false;
	bool commandStage = false;
	float whammyAxisScale = 1.0f; // Guitars only have 1 axis on the whammy bar.
	float whammyDeadzone = 0.0f;
	float buttonDeadzone = 0.0f; // Button deadzone is still a good idea, in case a host analog stick is bound to a guitar button

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
	PadGuitar(u8 unifiedSlot);
	~PadGuitar() override;

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
