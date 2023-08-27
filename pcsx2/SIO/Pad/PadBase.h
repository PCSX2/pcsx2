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

#include "SIO/Pad/PadTypes.h"

#include "StateWrapper.h"

#include <array>

class PadBase
{
protected:
	std::array<u8, 32> rawInputs = {};
	u8 unifiedSlot;
	bool isInConfig = false;
	Pad::Mode currentMode = Pad::Mode::NOT_SET;
	Pad::Command currentCommand = Pad::Command::NOT_SET;
	size_t commandBytesReceived = 0;

public: // Public members
	PadBase(u8 unifiedSlot);
	virtual ~PadBase();

	void SoftReset();
	void FullReset();

	virtual Pad::ControllerType GetType() const = 0;
	virtual const Pad::ControllerInfo& GetInfo() const = 0;

	virtual void Set(u32 index, float value) = 0;
	virtual void SetRawAnalogs(const std::tuple<u8, u8> left, const std::tuple<u8, u8> right) = 0;
	virtual void SetAxisScale(float deadzone, float scale) = 0;
	virtual float GetVibrationScale(u32 motor) const = 0;
	virtual void SetVibrationScale(u32 motor, float scale) = 0;
	virtual float GetPressureModifier() const = 0;
	virtual void SetPressureModifier(float mod) = 0;
	virtual void SetButtonDeadzone(float deadzone) = 0;
	virtual void SetAnalogInvertL(bool x, bool y) = 0;
	virtual void SetAnalogInvertR(bool x, bool y) = 0;
	virtual u8 GetRawInput(u32 index) const = 0;
	virtual std::tuple<u8, u8> GetRawLeftAnalog() const = 0;
	virtual std::tuple<u8, u8> GetRawRightAnalog() const = 0;
	virtual u32 GetButtons() const = 0;
	virtual u8 GetPressure(u32 index) const = 0;

	virtual bool Freeze(StateWrapper& sw);

	virtual u8 SendCommandByte(u8 commandByte) = 0;
};
