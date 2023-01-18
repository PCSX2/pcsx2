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

struct StereoOut32
{
	static const StereoOut32 Empty;

	s32 Left;
	s32 Right;

	StereoOut32()
		: Left(0)
		, Right(0)
	{
	}

	StereoOut32(s32 left, s32 right)
		: Left(left)
		, Right(right)
	{
	}

	StereoOut32 operator*(const int& factor) const
	{
		return StereoOut32(
			Left * factor,
			Right * factor);
	}

	StereoOut32& operator*=(const int& factor)
	{
		Left *= factor;
		Right *= factor;
		return *this;
	}

	StereoOut32 operator+(const StereoOut32& right) const
	{
		return StereoOut32(
			Left + right.Left,
			Right + right.Right);
	}

	StereoOut32 operator/(int src) const
	{
		return StereoOut32(Left / src, Right / src);
	}
};

extern void Mix();
extern s32 clamp_mix(s32 x, u8 bitshift = 0);

extern StereoOut32 clamp_mix(const StereoOut32& sample, u8 bitshift = 0);
