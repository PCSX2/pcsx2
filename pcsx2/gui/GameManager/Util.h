/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "Common.h"

namespace GuiUtilities
{
	struct HSLColor
	{
		u8 hue;
		u8 saturation;
		u8 lightness;
	};

	// Converts an RGB Value to HSL
	//
	// This can be useful in GUI programming for determining if the user
	// is using a dark/light theme in a deterministic way.
	// wxWidgets is just starting to offer these tools, and they are not platform-comprehensive
	// nor in the version that is currently vendored (in 3.1.X)
	//
	// From - https://gist.github.com/mjackson/5311256
	HSLColor rgbToHsl(u8 red, u8 green, u8 blue)
	{
		HSLColor hsl;

		u8 max = std::max({red, green, blue});
		u8 min = std::min({red, green, blue});
		hsl.hue = hsl.saturation = hsl.lightness = (max + min) / 2;

		if (max == min)
			hsl.hue = hsl.saturation = 0;
		else
		{
			u8 diff = max - min;
			hsl.saturation = hsl.lightness > 0.5 ? diff / (2 - max - min) : diff / (max + min);

			if (max == red)
				hsl.hue = (green - blue) / diff + (green < blue ? 6 : 0);
			else if (max == green)
				hsl.hue = (blue - red) / diff + 2;
			else if (max == blue)
				hsl.hue = (red - green) / diff + 4;
			hsl.hue /= 6;
		}

		return hsl;
	}

	// Converts an RGB Value to HSL
	//
	// This can be useful in GUI programming for determining if the user
	// is using a dark/light theme in a deterministic way.
	// wxWidgets is just starting to offer these tools, and they are not platform-comprehensive
	// nor in the version that is currently vendored (in 3.1.X)
	//
	// From - https://gist.github.com/mjackson/5311256
	HSLColor rgbToHsl(wxColour clr)
	{
		return rgbToHsl(clr.Red(), clr.Green(), clr.Blue());
	}
} // namespace GuiUtilities
