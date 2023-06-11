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

class PadNotConnected : public PadBase
{
public:
	PadNotConnected(u8 unifiedSlot);
	virtual ~PadNotConnected();

	void Init();
	Pad::ControllerType GetType();
	void Set(u32 index, float value);
	void SetRawAnalogs(const std::tuple<u8, u8> left, const std::tuple<u8, u8> right);
	void SetAxisScale(float deadzone, float scale);
	void SetTriggerScale(float deadzone, float scale) override;
	float GetVibrationScale(u32 motor);
	void SetVibrationScale(u32 motor, float scale);
	float GetPressureModifier();
	void SetPressureModifier(float mod);
	void SetButtonDeadzone(float deadzone);
	void SetAnalogInvertL(bool x, bool y);
	void SetAnalogInvertR(bool x, bool y);
	u8 GetRawInput(u32 index);
	std::tuple<u8, u8> GetRawLeftAnalog();
	std::tuple<u8, u8> GetRawRightAnalog();
	u32 GetButtons();
	u8 GetPressure(u32 index);

	void Freeze(StateWrapper& sw) override;

	u8 SendCommandByte(u8 commandByte) override;
};
