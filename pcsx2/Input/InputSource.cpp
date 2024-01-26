// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Input/InputSource.h"
#include "Host.h"

#include "common/StringUtil.h"

InputSource::InputSource() = default;

InputSource::~InputSource() = default;

void InputSource::UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity)
{
	if (large_key.bits != 0)
		UpdateMotorState(large_key, large_intensity);
	if (small_key.bits != 0)
		UpdateMotorState(small_key, small_intensity);
}

InputBindingKey InputSource::MakeGenericControllerAxisKey(InputSourceType clazz, u32 controller_index, s32 axis_index)
{
	InputBindingKey key = {};
	key.source_type = clazz;
	key.source_index = controller_index;
	key.source_subtype = InputSubclass::ControllerAxis;
	key.data = static_cast<u32>(axis_index);
	return key;
}

InputBindingKey InputSource::MakeGenericControllerButtonKey(
	InputSourceType clazz, u32 controller_index, s32 button_index)
{
	InputBindingKey key = {};
	key.source_type = clazz;
	key.source_index = controller_index;
	key.source_subtype = InputSubclass::ControllerButton;
	key.data = static_cast<u32>(button_index);
	return key;
}

InputBindingKey InputSource::MakeGenericControllerHatKey(InputSourceType clazz, u32 controller_index, s32 hat_index, u8 hat_direction, u32 num_directions)
{
	InputBindingKey key = {};
	key.source_type = clazz;
	key.source_index = controller_index;
	key.source_subtype = InputSubclass::ControllerHat;
	key.data = static_cast<u32>(hat_index) * num_directions + hat_direction;
	return key;
}

InputBindingKey InputSource::MakeGenericControllerMotorKey(InputSourceType clazz, u32 controller_index, s32 motor_index)
{
	InputBindingKey key = {};
	key.source_type = clazz;
	key.source_index = controller_index;
	key.source_subtype = InputSubclass::ControllerMotor;
	key.data = static_cast<u32>(motor_index);
	return key;
}

std::optional<InputBindingKey> InputSource::ParseGenericControllerKey(
	InputSourceType clazz, const std::string_view& source, const std::string_view& sub_binding)
{
	// try to find the number, this function doesn't care about whether it's xinput or sdl or whatever
	std::string_view::size_type pos = 0;
	while (pos < source.size())
	{
		if (source[pos] >= '0' && source[pos] <= '9')
			break;
		pos++;
	}
	if (pos == source.size())
		return std::nullopt;

	const std::optional<s32> source_index = StringUtil::FromChars<s32>(source.substr(pos));
	if (source_index.has_value() || source_index.value() < 0)
		return std::nullopt;

	InputBindingKey key = {};
	key.source_type = clazz;
	key.source_index = source_index.value();

	if (sub_binding.starts_with("+Axis") || sub_binding.starts_with("-Axis"))
	{
		const std::optional<s32> axis_number = StringUtil::FromChars<s32>(sub_binding.substr(5));
		if (!axis_number.has_value() || axis_number.value() < 0)
			return std::nullopt;

		key.source_subtype = InputSubclass::ControllerAxis;
		key.data = static_cast<u32>(axis_number.value());

		if (sub_binding[0] == '+')
			key.modifier = InputModifier::None;
		else if (sub_binding[0] == '-')
			key.modifier = InputModifier::Negate;
		else
			return std::nullopt;
	}
	else if (sub_binding.starts_with("FullAxis"))
	{
		const std::optional<s32> axis_number = StringUtil::FromChars<s32>(sub_binding.substr(8));
		if (!axis_number.has_value() || axis_number.value() < 0)
			return std::nullopt;
		key.source_subtype = InputSubclass::ControllerAxis;
		key.data = static_cast<u32>(axis_number.value());
		key.modifier = InputModifier::FullAxis;
	}
	else if (sub_binding.starts_with("Button"))
	{
		const std::optional<s32> button_number = StringUtil::FromChars<s32>(sub_binding.substr(6));
		if (!button_number.has_value() || button_number.value() < 0)
			return std::nullopt;

		key.source_subtype = InputSubclass::ControllerButton;
		key.data = static_cast<u32>(button_number.value());
	}
	else
	{
		return std::nullopt;
	}

	return key;
}

std::string InputSource::ConvertGenericControllerKeyToString(InputBindingKey key)
{
	if (key.source_subtype == InputSubclass::ControllerAxis)
	{
		const char* modifier = "";
		switch (key.modifier) {
			case InputModifier::None:     modifier = "+";    break;
			case InputModifier::Negate:   modifier = "-";    break;
			case InputModifier::FullAxis: modifier = "Full"; break;
		}
		return StringUtil::StdStringFromFormat("%s-%u/%sAxis%u", InputManager::InputSourceToString(key.source_type),
			key.source_index, modifier, key.data);
	}
	else if (key.source_subtype == InputSubclass::ControllerButton)
	{
		return StringUtil::StdStringFromFormat(
			"%s%u/Button%u", InputManager::InputSourceToString(key.source_type), key.source_index, key.data);
	}
	else
	{
		return {};
	}
}

bool InputSource::ShouldIgnoreInversion()
{
	// This is only called when binding controllers, so the lookup is fine.
	return Host::GetBaseBoolSettingValue("InputSources", "IgnoreInversion", false);
}
