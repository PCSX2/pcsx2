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

#include "Linux/KeyboardMouse.h"

// actually it is even more but it is enough to distinguish different key
#define MAX_KEYCODE (0xFF)

LinuxKeyboard::LinuxKeyboard() :
	Device(LNX_KEYBOARD, KEYBOARD, L"displayName", L"instanceID", L"deviceID")
{
	for (int i=0; i<MAX_KEYCODE; i++) {
		AddPhysicalControl(PSHBTN, i, i);
	}
}

int LinuxKeyboard::Activate(InitInfo* args) {
	// Always active
	active = 1;
	
	AllocState();

#if 0
	for (int vkey=5; vkey<256; vkey++) {
		int value = (unsigned short)(((short)GetAsyncKeyState(vkey))>>15);
		value += value&1;
		if (vkey == VK_CONTROL || vkey == VK_MENU || vkey == VK_SHIFT) {
			value = 0;
		}
		physicalControlState[vkey] = 0;
	}
#endif
	// Every button released
	memset(physicalControlState, 0, sizeof(int)*MAX_KEYCODE);

	return 1;
}

int LinuxKeyboard::Update() {
	keyEvent event;
	int status = 0;
	while (R_GetQueuedKeyEvent(&event)) {
		switch (event.evt) {
			case KeyPress:
				physicalControlState[MAX_KEYCODE & event.key] = FULLY_DOWN;
				status = 1;
				break;
			case KeyRelease:
				physicalControlState[MAX_KEYCODE & event.key] = 0;
				status = 1;
				break;
			default:
				//fprintf(stderr, "Unsupported event %x\n", event.evt);
				//assert(0);
				break;
		}
	}

	return status; // XXX ????
}

void EnumLnx() {
	dm->AddDevice(new LinuxKeyboard());
}
