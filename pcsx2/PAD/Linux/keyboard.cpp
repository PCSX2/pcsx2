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
#include "../Gamepad.h"

extern HostKeyEvent event;
extern MtQueue<HostKeyEvent> g_ev_fifo;

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
	g_ev_fifo.consume_all(AnalyzeKeyEvent);
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
#define CHECK(button, value) \
	if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, (button))) \
	{ \
		pkey = (value); \
		return true; \
	}
	CHECK(kCGMouseButtonLeft,   0x10001)
	CHECK(kCGMouseButtonCenter, 0x10002)
	CHECK(kCGMouseButtonRight,  0x10003)
#undef CHECK
	return false;
}
#elif defined(__unix__)
static bool s_grab_input = false;
static bool s_Shift = false;

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
				evt.type = HostKeyEvent::Type::MouseMove;
				break;
			case ButtonRelease:
				evt.key = E.xbutton.button | 0x10000;
				evt.type = HostKeyEvent::Type::MouseReleased;
				break;
			case ButtonPress:
				evt.key = E.xbutton.button | 0x10000;
				evt.type = HostKeyEvent::Type::MousePressed;
				break;
			case KeyPress:
				evt.key = (int)XLookupKeysym(&E.xkey, 0);
				evt.type = HostKeyEvent::Type::KeyPressed;
				break;
			case KeyRelease:
				evt.key = (int)XLookupKeysym(&E.xkey, 0);
				evt.type = HostKeyEvent::Type::KeyReleased;
			default:
				continue;
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
			pkey = ev->button.button | 0x10000;
			return true;
		}
	}

	return false;
}
#endif

static unsigned int s_previous_mouse_x = 0;
static unsigned int s_previous_mouse_y = 0;

void AnalyzeKeyEvent(HostKeyEvent& evt)
{
	int pad = 0;
	int index = -1;

	for (u32 cpad = 0; cpad < GAMEPAD_NUMBER; cpad++)
	{
		int tmp_index = get_keyboard_key(cpad, evt.key);
		if (tmp_index != -1)
		{
			pad = cpad;
			index = tmp_index;
		}
	}

	switch (evt.type)
	{
		case HostKeyEvent::Type::KeyPressed:
			// Shift F12 is not yet use by pcsx2. So keep it to grab/ungrab input
			// I found it very handy vs the automatic fullscreen detection
			// 1/ Does not need to detect full-screen
			// 2/ Can use a debugger in full-screen
			// 3/ Can grab input in window without the need of a pixelated full-screen
#ifdef __unix__
			if (evt.key == XK_Shift_R || evt.key == XK_Shift_L)
				s_Shift = true;
			if (evt.key == XK_F12 && s_Shift && GSdsp)
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
#endif

			if (index != -1)
				PressButton(pad, index);

			//PAD_LOG("Key pressed:%d", index);

			event.type = HostKeyEvent::Type::KeyPressed;
			event.key = evt.key;
			break;

		case HostKeyEvent::Type::KeyReleased:
#ifdef __unix__
			if (evt.key == XK_Shift_R || evt.key == XK_Shift_L)
				s_Shift = false;
#endif

			if (index != -1)
				g_key_status.release(pad, index);

			event.type = HostKeyEvent::Type::KeyReleased;
			event.key = evt.key;
			break;

		case HostKeyEvent::Type::FocusGained:
			break;

		case HostKeyEvent::Type::FocustLost:
#ifdef __unix__
			s_Shift = false;
#endif
			break;

		case HostKeyEvent::Type::MousePressed:
			if (index != -1)
				g_key_status.press(pad, index);
			break;

		case HostKeyEvent::Type::MouseReleased:
			if (index != -1)
				g_key_status.release(pad, index);
			break;

		case HostKeyEvent::Type::MouseMove:
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

		case HostKeyEvent::Type::NoEvent:
		case HostKeyEvent::Type::MouseWheelDown:
		case HostKeyEvent::Type::MouseWheelUp:
			break;
	}
}
