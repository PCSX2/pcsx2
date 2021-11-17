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

/*
  * Theoretically, this header is for anything to do with keyboard input.
  * Pragmatically, event handing's going in here too.
  */

#include "Global.h"
#include "keyboard.h"
#include "PAD.h"

/// g_key_status.press but with proper handling for analog buttons
static void PressButton(u32 pad, u32 button)
{
	// Analog controls.
	if (IsAnalogKey(button))
	{
		switch (button)
		{
			case PAD_R_LEFT:
			case PAD_R_UP:
			case PAD_L_LEFT:
			case PAD_L_UP:
				g_key_status.press(pad, button, -MAX_ANALOG_VALUE);
				break;
			case PAD_R_RIGHT:
			case PAD_R_DOWN:
			case PAD_L_RIGHT:
			case PAD_L_DOWN:
				g_key_status.press(pad, button, MAX_ANALOG_VALUE);
				break;
		}
	}
	else
	{
		g_key_status.press(pad, button);
	}
}

#if defined(__APPLE__)
// Mac keyboard input code is based on Dolphin's Source/Core/InputCommon/ControllerInterface/Quartz/QuartzKeyboardAndMouse.mm
// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+

// All keycodes are 16 bits or less, so we use top 16 bits to differentiate source
// Keyboard keys use discriminator 0
// Mouse buttons use discriminator 1

void UpdateKeyboardInput()
{
	for (u32 pad = 0; pad < GAMEPAD_NUMBER; pad++)
	{
		const auto& map = g_conf.keysym_map[pad];
		// If we loop over all keys press/release based on current state,
		// joystick axes (which have two bound keys) will always go to the later-polled key
		// Instead, release all keys first and then set the ones that are pressed
		for (const auto& key : map)
			g_key_status.release(pad, key.second);
		for (const auto& key : map)
		{
			bool state;
			if (key.first >> 16 == 0)
			{
				state = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, key.first);
			}
			else
			{
				state = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, (CGMouseButton)(key.first & 0xFFFF));
			}
			if (state)
				PressButton(pad, key.second);
		}
	}
}

bool PollForNewKeyboardKeys(u32& pkey)
{
	// All keycodes in <HIToolbox/Events.h> are 0x7e or lower. If you notice
	// keys that aren't being recognized, bump this number up!
	for (int key = 0; key < 0x80; key++)
	{
		if (CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, key))
		{
			pkey = key == kVK_Escape ? UINT32_MAX : key;
			return true;
		}
	}
	for (auto btn : {kCGMouseButtonLeft, kCGMouseButtonCenter, kCGMouseButtonRight})
	{
		if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, btn))
		{
			pkey = btn | (1 << 16);
			return true;
		}
	}
	return false;
}
#elif defined(__unix__)
static bool s_grab_input = false;
static bool s_Shift = false;
static unsigned int s_previous_mouse_x = 0;
static unsigned int s_previous_mouse_y = 0;

void AnalyzeKeyEvent(HostKeyEvent& evt)
{
	KeySym key = (KeySym)evt.key;
	int pad = 0;
	int index = -1;

	for (u32 cpad = 0; cpad < GAMEPAD_NUMBER; cpad++)
	{
		int tmp_index = get_keyboard_key(cpad, key);
		if (tmp_index != -1)
		{
			pad = cpad;
			index = tmp_index;
		}
	}

	switch (static_cast<int>(evt.type))
	{
		case KeyPress:
			// Shift F12 is not yet use by pcsx2. So keep it to grab/ungrab input
			// I found it very handy vs the automatic fullscreen detection
			// 1/ Does not need to detect full-screen
			// 2/ Can use a debugger in full-screen
			// 3/ Can grab input in window without the need of a pixelated full-screen
			if (key == XK_Shift_R || key == XK_Shift_L)
				s_Shift = true;
			if (key == XK_F12 && s_Shift)
			{
				if (!s_grab_input)
				{
					s_grab_input = true;
					XGrabPointer(GSdsp, GSwin, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, GSwin, None, CurrentTime);
					XGrabKeyboard(GSdsp, GSwin, True, GrabModeAsync, GrabModeAsync, CurrentTime);
				}
				else
				{
					s_grab_input = false;
					XUngrabPointer(GSdsp, CurrentTime);
					XUngrabKeyboard(GSdsp, CurrentTime);
				}
			}

			if (index != -1)
				PressButton(pad, index);

			//PAD_LOG("Key pressed:%d\n", index);

			event.type = HostKeyEvent::Type::KeyPressed;
			event.key = key;
			break;

		case KeyRelease:
			if (key == XK_Shift_R || key == XK_Shift_L)
				s_Shift = false;

			if (index != -1)
				g_key_status.release(pad, index);

			event.type = HostKeyEvent::Type::KeyReleased;
			event.key = key;
			break;

		case FocusIn:
			break;

		case FocusOut:
			s_Shift = false;
			break;

		case ButtonPress:
			if (index != -1)
				g_key_status.press(pad, index);
			break;

		case ButtonRelease:
			if (index != -1)
				g_key_status.release(pad, index);
			break;

		case MotionNotify:
			// FIXME: How to handle when the mouse does not move, no event generated!!!
			// 1/ small move == no move. Cons : can not do small movement
			// 2/ use a watchdog timer thread
			// 3/ ??? idea welcome ;)
			if (g_conf.pad_options[pad].mouse_l | g_conf.pad_options[pad].mouse_r)
			{
				unsigned int pad_x;
				unsigned int pad_y;
				// Note when both PADOPTION_MOUSE_R and PADOPTION_MOUSE_L are set, take only the right one
				if (g_conf.pad_options[pad].mouse_r)
				{
					pad_x = PAD_R_RIGHT;
					pad_y = PAD_R_UP;
				}
				else
				{
					pad_x = PAD_L_RIGHT;
					pad_y = PAD_L_UP;
				}

				unsigned x = evt.key & 0xFFFF;
				unsigned int value = (s_previous_mouse_x > x) ? s_previous_mouse_x - x : x - s_previous_mouse_x;
				value *= g_conf.get_sensibility();

				if (x == 0)
					g_key_status.press(pad, pad_x, -MAX_ANALOG_VALUE);
				else if (x == 0xFFFF)
					g_key_status.press(pad, pad_x, MAX_ANALOG_VALUE);
				else if (x < (s_previous_mouse_x - 2))
					g_key_status.press(pad, pad_x, -value);
				else if (x > (s_previous_mouse_x + 2))
					g_key_status.press(pad, pad_x, value);
				else
					g_key_status.release(pad, pad_x);


				unsigned y = evt.key >> 16;
				value = (s_previous_mouse_y > y) ? s_previous_mouse_y - y : y - s_previous_mouse_y;
				value *= g_conf.get_sensibility();

				if (y == 0)
					g_key_status.press(pad, pad_y, -MAX_ANALOG_VALUE);
				else if (y == 0xFFFF)
					g_key_status.press(pad, pad_y, MAX_ANALOG_VALUE);
				else if (y < (s_previous_mouse_y - 2))
					g_key_status.press(pad, pad_y, -value);
				else if (y > (s_previous_mouse_y + 2))
					g_key_status.press(pad, pad_y, value);
				else
					g_key_status.release(pad, pad_y);

				s_previous_mouse_x = x;
				s_previous_mouse_y = y;
			}

			break;
	}
}

void UpdateKeyboardInput()
{
	HostKeyEvent evt = {};
	XEvent E = {0};

	// Keyboard input send by PCSX2
	g_ev_fifo.consume_all(AnalyzeKeyEvent);

	// keyboard input
	if (!GSdsp)
		return;

	while (XPending(GSdsp) > 0)
	{
		XNextEvent(GSdsp, &E);

		// Change the format of the structure to be compatible with GSOpen2
		// mode (event come from pcsx2 not X)
		evt.type = static_cast<HostKeyEvent::Type>(E.type);
		switch (E.type)
		{
			case MotionNotify:
				evt.key = (E.xbutton.x & 0xFFFF) | (E.xbutton.y << 16);
				break;
			case ButtonRelease:
			case ButtonPress:
				evt.key = E.xbutton.button;
				break;
			default:
				evt.key = (int)XLookupKeysym(&E.xkey, 0);
		}

		AnalyzeKeyEvent(evt);
	}
}

bool PollForNewKeyboardKeys(u32& pkey)
{
	GdkEvent* ev = gdk_event_get();

	if (ev != NULL)
	{
		if (ev->type == GDK_KEY_PRESS)
		{
			pkey = ev->key.keyval != GDK_KEY_Escape ? ev->key.keyval : UINT32_MAX;
			return true;
		}
		else if (ev->type == GDK_BUTTON_PRESS)
		{
			pkey = ev->button.button;
			return true;
		}
	}

	return false;
}
#endif
