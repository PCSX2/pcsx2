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

#include "PAD.h"
#include "controller.h"

__forceinline void set_keyboard_key(int pad, int keysym, int index)
{
	g_conf.keysym_map[pad][keysym] = index;
}

__forceinline int get_keyboard_key(int pad, int keysym)
{
	// You must use find instead of []
	// [] will create an element if the key does not exist and return 0
	std::map<u32, u32>::iterator it = g_conf.keysym_map[pad].find(keysym);
	if (it != g_conf.keysym_map[pad].end())
		return it->second;
	else
		return -1;
}

__forceinline bool IsAnalogKey(int index)
{
	return ((index >= PAD_L_UP) && (index <= PAD_R_LEFT));
}
