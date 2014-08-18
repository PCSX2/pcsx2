/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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
#include "Pcsx2Defs.h"

static const __aligned16 float g_fones[8]  = {1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
static const __aligned16 u32 g_mask[4]     = {0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff};
static const __aligned16 u32 g_minvals[4]  = {0xff7fffff, 0xff7fffff, 0xff7fffff, 0xff7fffff};
static const __aligned16 u32 g_maxvals[4]  = {0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff};
static const __aligned16 u32 g_clip[8]     = {0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff,
                                              0x80000000, 0x80000000, 0x80000000, 0x80000000};

static const __aligned(64) u32 g_ones[4]   = {0x00000001, 0x00000001, 0x00000001, 0x00000001};

static const __aligned16 u32 g_minvals_XYZW[16][4] =
{
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff }, //0000
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xff7fffff }, //0001
	{ 0xffffffff, 0xffffffff, 0xff7fffff, 0xffffffff }, //0010
	{ 0xffffffff, 0xffffffff, 0xff7fffff, 0xff7fffff }, //0011
	{ 0xffffffff, 0xff7fffff, 0xffffffff, 0xffffffff }, //0100
	{ 0xffffffff, 0xff7fffff, 0xffffffff, 0xff7fffff }, //0101
	{ 0xffffffff, 0xff7fffff, 0xff7fffff, 0xffffffff }, //0110
	{ 0xffffffff, 0xff7fffff, 0xff7fffff, 0xff7fffff }, //0111
	{ 0xff7fffff, 0xffffffff, 0xffffffff, 0xffffffff }, //1000
	{ 0xff7fffff, 0xffffffff, 0xffffffff, 0xff7fffff }, //1001
	{ 0xff7fffff, 0xffffffff, 0xff7fffff, 0xffffffff }, //1010
	{ 0xff7fffff, 0xffffffff, 0xff7fffff, 0xff7fffff }, //1011
	{ 0xff7fffff, 0xff7fffff, 0xffffffff, 0xffffffff }, //1100
	{ 0xff7fffff, 0xff7fffff, 0xffffffff, 0xff7fffff }, //1101
	{ 0xff7fffff, 0xff7fffff, 0xff7fffff, 0xffffffff }, //1110
	{ 0xff7fffff, 0xff7fffff, 0xff7fffff, 0xff7fffff }, //1111
};

static const __aligned16 u32 g_maxvals_XYZW[16][4] =
{
	{ 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff }, //0000
	{ 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7f7fffff }, //0001
	{ 0x7fffffff, 0x7fffffff, 0x7f7fffff, 0x7fffffff }, //0010
	{ 0x7fffffff, 0x7fffffff, 0x7f7fffff, 0x7f7fffff }, //0011
	{ 0x7fffffff, 0x7f7fffff, 0x7fffffff, 0x7fffffff }, //0100
	{ 0x7fffffff, 0x7f7fffff, 0x7fffffff, 0x7f7fffff }, //0101
	{ 0x7fffffff, 0x7f7fffff, 0x7f7fffff, 0x7fffffff }, //0110
	{ 0x7fffffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff }, //0111
	{ 0x7f7fffff, 0x7fffffff, 0x7fffffff, 0x7fffffff }, //1000
	{ 0x7f7fffff, 0x7fffffff, 0x7fffffff, 0x7f7fffff }, //1001
	{ 0x7f7fffff, 0x7fffffff, 0x7f7fffff, 0x7fffffff }, //1010
	{ 0x7f7fffff, 0x7fffffff, 0x7f7fffff, 0x7f7fffff }, //1011
	{ 0x7f7fffff, 0x7f7fffff, 0x7fffffff, 0x7fffffff }, //1100
	{ 0x7f7fffff, 0x7f7fffff, 0x7fffffff, 0x7f7fffff }, //1101
	{ 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7fffffff }, //1110
	{ 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff }, //1111
};
