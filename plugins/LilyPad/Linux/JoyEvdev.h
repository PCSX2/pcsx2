/*  LilyPad - Pad plugin for PS2 Emulator
 *  Copyright (C) 2002-2015  PCSX2 Dev Team/ChickenLiver
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU Lesser General Public License as published by the Free
 *  Software Found- ation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with PCSX2.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Global.h"
#include "InputManager.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>

struct abs_info {
	uint16_t code;
	int32_t min;
	int32_t max;

	int32_t factor;
	int32_t translation;

	abs_info(int32_t _code, int32_t _min, int32_t _max, ControlType type) : code(_code), min(_min), max(_max) {
		translation = 0;
		// Note: ABSAXIS ranges from -64K to 64K
		// Note: PSHBTN ranges from 0 to 64K
		if ((min == 0) && (max == 255)) {
			if (type == ABSAXIS) {
				translation = 128;
				factor = FULLY_DOWN/128;
			} else {
				factor = FULLY_DOWN/256;
			}
		} else if ((min == -1) && (max == 1)) {
			factor = FULLY_DOWN;
		} else if ((min == 0) && (std::abs(max - 127) < 2)) {
			translation = 64;
			factor = -FULLY_DOWN/64;
		} else if ((max == 255) && (std::abs(min - 127) < 2)) {
			translation = 64+128;
			factor = FULLY_DOWN/64;
		} else {
			fprintf(stderr, "Scale not supported\n");
			factor = 0;
		}
	}

	int scale(int32_t value) {
		return (value - translation) * factor;
	}
};

class JoyEvdev : public Device {
	int m_fd;
	std::vector<abs_info> m_abs;
	std::vector<uint16_t> m_btn;
	std::vector<uint16_t> m_rel;

	public:
		JoyEvdev(int fd, bool ds3, const wchar_t *id);
		~JoyEvdev();
		int Activate(InitInfo* args);
		int Update();
};

void EnumJoystickEvdev();
