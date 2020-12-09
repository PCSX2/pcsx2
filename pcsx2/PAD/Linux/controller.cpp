/*  OnePAD - author: arcum42(@gmail.com)
 *  Copyright (C) 2009
 *
 *  Based on ZeroPAD, author zerofrog@gmail.com
 *  Copyright (C) 2006-2007
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "onepad.h"
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
