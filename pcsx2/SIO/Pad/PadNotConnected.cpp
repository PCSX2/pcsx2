// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "SIO/Pad/PadNotConnected.h"

#include "Host.h"

const Pad::ControllerInfo PadNotConnected::ControllerInfo = {Pad::ControllerType::NotConnected, "None",
	TRANSLATE_NOOP("Pad", "Not Connected"), nullptr, {}, {}, Pad::VibrationCapabilities::NoVibration };

PadNotConnected::PadNotConnected(u8 unifiedSlot, size_t ejectTicks)
	: PadBase(unifiedSlot, ejectTicks)
{

}

PadNotConnected::~PadNotConnected() = default;

Pad::ControllerType PadNotConnected::GetType() const
{
	return Pad::ControllerType::NotConnected;
}

const Pad::ControllerInfo& PadNotConnected::GetInfo() const
{
	return ControllerInfo;
}

void PadNotConnected::Set(u32 index, float value)
{

}

void PadNotConnected::SetRawAnalogs(const std::tuple<u8, u8> left, const std::tuple<u8, u8> right)
{

}

void PadNotConnected::SetRawPressureButton(u32 index, const std::tuple<bool, u8> value)
{

}

void PadNotConnected::SetAxisScale(float deadzone, float scale)
{

}

float PadNotConnected::GetVibrationScale(u32 motor) const
{
	return 0;
}

void PadNotConnected::SetVibrationScale(u32 motor, float scale)
{

}

float PadNotConnected::GetPressureModifier() const
{
	return 0;
}

void PadNotConnected::SetPressureModifier(float mod)
{

}

void PadNotConnected::SetButtonDeadzone(float deadzone)
{

}

void PadNotConnected::SetAnalogInvertL(bool x, bool y)
{

}

void PadNotConnected::SetAnalogInvertR(bool x, bool y)
{

}

float PadNotConnected::GetEffectiveInput(u32 index) const
{
	return 0;
}

u8 PadNotConnected::GetRawInput(u32 index) const
{
	return 0;
}

std::tuple<u8, u8> PadNotConnected::GetRawLeftAnalog() const
{
	return std::tuple<u8, u8>{0, 0};
}

std::tuple<u8, u8> PadNotConnected::GetRawRightAnalog() const
{
	return std::tuple<u8, u8>{0, 0};
}

u32 PadNotConnected::GetButtons() const
{
	return 0;
}

u8 PadNotConnected::GetPressure(u32 index) const
{
	return 0;
}

bool PadNotConnected::IsAnalogLightEnabled() const
{
	return this->analogLight;
}

bool PadNotConnected::IsAnalogLocked() const
{
	return this->analogLocked;
}

u8 PadNotConnected::SendCommandByte(u8 commandByte)
{
	return 0xff;
}
