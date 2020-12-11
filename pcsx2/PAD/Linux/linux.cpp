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

#include "AppCoreThread.h"
#include "GamePad.h"
#include "PAD.h"
#include "keyboard.h"
#include "state_management.h"

#include <string.h>
#include "wx_dialog/dialog.h"

#ifndef __APPLE__
Display* GSdsp;
Window GSwin;
#endif

static void SysMessage(const char* fmt, ...)
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg) - 1] == '\n')
		msg[strlen(msg) - 1] = 0;

	wxMessageDialog dialog(nullptr, msg, "Info", wxOK);
	dialog.ShowModal();
}

s32 _PADopen(void* pDsp)
{
#ifndef __APPLE__
	GSdsp = *(Display**)pDsp;
	GSwin = (Window) * (((u32*)pDsp) + 1);
#endif

	return 0;
}

void _PADclose()
{
	s_vgamePad.clear();
}

void PollForJoystickInput(int cpad)
{
	int index = GamePad::uid_to_index(cpad);
	if (index < 0)
		return;

	auto& gamePad = s_vgamePad[index];

	gamePad->UpdateGamePadState();

	for (int i = 0; i < MAX_KEYS; i++)
	{
		s32 value = gamePad->GetInput((gamePadValues)i);
		if (value != 0)
			g_key_status.press(cpad, i, value);
		else
			g_key_status.release(cpad, i);
	}
}

void PADupdate(int pad)
{
#ifndef __APPLE__
	// Gamepad inputs don't count as an activity. Therefore screensaver will
	// be fired after a couple of minute.
	// Emulate an user activity
	static int count = 0;
	count++;
	if ((count & 0xFFF) == 0)
	{
		// 1 call every 4096 Vsync is enough
		XResetScreenSaver(GSdsp);
	}
#endif

	// Actually PADupdate is always call with pad == 0. So you need to update both
	// pads -- Gregory

	// Poll keyboard/mouse event. There is currently no way to separate pad0 from pad1 event.
	// So we will populate both pad in the same time
	for (int cpad = 0; cpad < GAMEPAD_NUMBER; cpad++)
	{
		g_key_status.keyboard_state_acces(cpad);
	}
	UpdateKeyboardInput();

	// Get joystick state + Commit
	for (int cpad = 0; cpad < GAMEPAD_NUMBER; cpad++)
	{
		g_key_status.joystick_state_acces(cpad);

		PollForJoystickInput(cpad);

		g_key_status.commit_status(cpad);
	}

	Pad::rumble_all();
}

void PADconfigure()
{
	ScopedCoreThreadPause paused_core;
	PADLoadConfig();

	DisplayDialog();
	paused_core.AllowResume();
	return;
}
