/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"
#include "Frontend/InputSource.h"
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

	if (StringUtil::StartsWith(sub_binding, "+Axis") || StringUtil::StartsWith(sub_binding, "-Axis"))
	{
		const std::optional<s32> axis_number = StringUtil::FromChars<s32>(sub_binding.substr(5));
		if (!axis_number.has_value() || axis_number.value() < 0)
			return std::nullopt;

		key.source_subtype = InputSubclass::ControllerAxis;
		key.data = static_cast<u32>(axis_number.value());

		if (sub_binding[0] == '+')
			key.negative = false;
		else if (sub_binding[0] == '-')
			key.negative = true;
		else
			return std::nullopt;
	}
	else if (StringUtil::StartsWith(sub_binding, "Button"))
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
		return StringUtil::StdStringFromFormat("%s-%u/%cAxis%u", InputManager::InputSourceToString(key.source_type),
			key.source_index, key.negative ? '+' : '-', key.data);
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
