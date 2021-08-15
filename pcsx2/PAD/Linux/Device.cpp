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

#include "Device.h"
#ifdef SDL_BUILD
#include "SDL/joystick.h"
#endif

/**
 * Following static methods are just forwarders to their backend
 * This is where link between agnostic and specific code is done
 **/

/**
 * Safely dispatch to the Rumble method above
 **/
void Device::DoRumble(unsigned type, unsigned pad)
{
	int index = uid_to_index(pad);
	if (index >= 0)
		device_manager.devices[index]->Rumble(type, pad);
}

size_t Device::index_to_uid(int index)
{
	if ((index >= 0) && (index < (int)device_manager.devices.size()))
		return device_manager.devices[index]->GetUniqueIdentifier();
	else
		return 0;
}

int Device::uid_to_index(int pad)
{
	size_t uid = g_conf.get_joy_uid(pad);

	for (int i = 0; i < (int)device_manager.devices.size(); ++i)
	{
		if (device_manager.devices[i]->GetUniqueIdentifier() == uid)
			return i;
	}

	// Current uid wasn't found maybe the pad was unplugged. Or
	// user didn't select it. Fallback to 1st pad for
	// 1st player. And 2nd pad for 2nd player.
	if ((int)device_manager.devices.size() > pad)
		return pad;

	return -1;
}
