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

#include "Config.h"
#include "Host.h"

namespace Guitar
{
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

	// The generic input bindings on this might seem bizarre, but they are intended to match what DS2 buttons
	// would do what actions, if you played Guitar Hero on a PS2 with a DS2 instead of a controller.
	static const InputBindingInfo defaultBindings[] = {
		{"Up", TRANSLATE_NOOP("Pad", "Strum Up"), InputBindingInfo::Type::Button, Guitar::Inputs::STRUM_UP, GenericInputBinding::DPadUp},
		{"Down", TRANSLATE_NOOP("Pad", "Strum Down"), InputBindingInfo::Type::Button, Guitar::Inputs::STRUM_DOWN, GenericInputBinding::DPadDown},
		{"Select", TRANSLATE_NOOP("Pad", "Select"), InputBindingInfo::Type::Button, Guitar::Inputs::SELECT, GenericInputBinding::Select},
		{"Start", TRANSLATE_NOOP("Pad", "Start"), InputBindingInfo::Type::Button, Guitar::Inputs::START, GenericInputBinding::Start},
		{"Green", TRANSLATE_NOOP("Pad", "Green Fret"), InputBindingInfo::Type::Button, Guitar::Inputs::GREEN, GenericInputBinding::R2},
		{"Red", TRANSLATE_NOOP("Pad", "Red Fret"), InputBindingInfo::Type::Button, Guitar::Inputs::RED, GenericInputBinding::Circle},
		{"Yellow", TRANSLATE_NOOP("Pad", "Yellow Fret"), InputBindingInfo::Type::Button, Guitar::Inputs::YELLOW, GenericInputBinding::Triangle},
		{"Blue", TRANSLATE_NOOP("Pad", "Blue Fret"), InputBindingInfo::Type::Button, Guitar::Inputs::BLUE, GenericInputBinding::Cross},
		{"Orange", TRANSLATE_NOOP("Pad", "Orange Fret"), InputBindingInfo::Type::Button, Guitar::Inputs::ORANGE, GenericInputBinding::Square},
		{"Whammy", TRANSLATE_NOOP("Pad", "Whammy Bar"), InputBindingInfo::Type::HalfAxis, Guitar::Inputs::WHAMMY, GenericInputBinding::LeftStickUp},
		{"Tilt", TRANSLATE_NOOP("Pad", "Tilt Up"), InputBindingInfo::Type::Button, Guitar::Inputs::TILT, GenericInputBinding::L2},
	};

	static const SettingInfo defaultSettings[] = {
		{SettingInfo::Type::Float, "Deadzone", TRANSLATE_NOOP("Pad", "Whammy Bar Deadzone"),
			TRANSLATE_NOOP("Pad", "Sets the whammy bar deadzone. Inputs below this value will not be sent to the PS2."),
			"0.00", "0.00", "1.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
		{SettingInfo::Type::Float, "AxisScale", TRANSLATE_NOOP("Pad", "Whammy Bar Sensitivity"),
			TRANSLATE_NOOP("Pad", "Sets the whammy bar axis scaling factor."),
			"1.0", "0.01", "2.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
	};
} // namespace Guitar
