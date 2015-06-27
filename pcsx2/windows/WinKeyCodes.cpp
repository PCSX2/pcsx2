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

#include "PrecompiledHeader.h"
#include <Windows.h>

// Returns a WXK_* keycode translated from a VK_* virtual key. wxCharCodeMSWToWx was
// removed from wxWidgets 3, this should work as a replacement.
int TranslateVKToWXK(u32 keysym)
{
	int key_code;

	switch (keysym)
	{
	// Shift, Control, Alt and Menu
	case VK_LSHIFT:
	case VK_RSHIFT: key_code = WXK_SHIFT; break;

	case VK_LCONTROL:
	case VK_RCONTROL: key_code = WXK_CONTROL; break;

	case VK_LMENU:
	case VK_RMENU:
	case VK_MENU: key_code = WXK_ALT; break;

	case VK_LWIN: key_code = WXK_WINDOWS_LEFT; break;
	case VK_RWIN: key_code = WXK_WINDOWS_RIGHT; break;

	case VK_APPS: key_code = WXK_WINDOWS_MENU; break;

	// Scroll, Caps and numlocks
	case VK_SCROLL: key_code = WXK_SCROLL; break;
	case VK_CAPITAL: key_code = WXK_CAPITAL; break;
	case VK_NUMLOCK: key_code = WXK_NUMLOCK; break;

	// cursor and other extended keyboard keys
	case VK_PRIOR: key_code = WXK_PAGEUP; break;
	case VK_NEXT: key_code = WXK_PAGEDOWN; break;
	case VK_HOME: key_code = WXK_HOME; break;
	case VK_END: key_code = WXK_END; break;
	case VK_LEFT: key_code = WXK_LEFT; break;
	case VK_UP: key_code = WXK_UP; break;
	case VK_RIGHT: key_code = WXK_RIGHT; break;
	case VK_DOWN: key_code = WXK_DOWN; break;
	case VK_INSERT: key_code = WXK_INSERT; break;
	case VK_DELETE: key_code = WXK_DELETE; break;

	// numpad keys
	case VK_NUMPAD0: key_code = WXK_NUMPAD0; break;
	case VK_NUMPAD1: key_code = WXK_NUMPAD1; break;
	case VK_NUMPAD2: key_code = WXK_NUMPAD2; break;
	case VK_NUMPAD3: key_code = WXK_NUMPAD3; break;
	case VK_NUMPAD4: key_code = WXK_NUMPAD4; break;
	case VK_NUMPAD5: key_code = WXK_NUMPAD5; break;
	case VK_NUMPAD6: key_code = WXK_NUMPAD6; break;
	case VK_NUMPAD7: key_code = WXK_NUMPAD7; break;
	case VK_NUMPAD8: key_code = WXK_NUMPAD8; break;
	case VK_NUMPAD9: key_code = WXK_NUMPAD9; break;
	case VK_MULTIPLY: key_code = WXK_NUMPAD_MULTIPLY; break;
	case VK_ADD: key_code = WXK_NUMPAD_ADD; break;
	case VK_SEPARATOR:key_code = WXK_NUMPAD_SEPARATOR; break;
	case VK_SUBTRACT: key_code = WXK_NUMPAD_SUBTRACT; break;
	case VK_DECIMAL: key_code = WXK_NUMPAD_DECIMAL; break;
	case VK_DIVIDE: key_code = WXK_NUMPAD_DIVIDE; break;

	// Function keys
	case VK_F1: key_code = WXK_F1; break;
	case VK_F2: key_code = WXK_F2; break;
	case VK_F3: key_code = WXK_F3; break;
	case VK_F4: key_code = WXK_F4; break;
	case VK_F5: key_code = WXK_F5; break;
	case VK_F6: key_code = WXK_F6; break;
	case VK_F7: key_code = WXK_F7; break;
	case VK_F8: key_code = WXK_F8; break;
	case VK_F9: key_code = WXK_F9; break;
	case VK_F10: key_code = WXK_F10; break;
	case VK_F11: key_code = WXK_F11; break;
	case VK_F12: key_code = WXK_F12; break;
	case VK_F13: key_code = WXK_F13; break;
	case VK_F14: key_code = WXK_F14; break;
	case VK_F15: key_code = WXK_F15; break;
	case VK_F16: key_code = WXK_F16; break;
	case VK_F17: key_code = WXK_F17; break;
	case VK_F18: key_code = WXK_F18; break;
	case VK_F19: key_code = WXK_F19; break;
	case VK_F20: key_code = WXK_F20; break;
	case VK_F21: key_code = WXK_F21; break;
	case VK_F22: key_code = WXK_F22; break;
	case VK_F23: key_code = WXK_F23; break;
	case VK_F24: key_code = WXK_F24; break;

	// various other special keys
	case VK_BACK: key_code = WXK_BACK; break;
	case VK_TAB: key_code = WXK_TAB; break;
	case VK_RETURN: key_code = WXK_RETURN; break;
	case VK_ESCAPE: key_code = WXK_ESCAPE; break;
	case VK_SNAPSHOT: key_code = WXK_SNAPSHOT; break;
	case VK_PAUSE: key_code = WXK_PAUSE; break;

	// Not sure what these keys are
	case VK_CLEAR: key_code = WXK_CLEAR; break;
	case VK_HELP: key_code = WXK_HELP; break;
	case VK_SELECT: key_code = WXK_SELECT; break;
	case VK_EXECUTE: key_code = WXK_EXECUTE; break;
	case VK_PRINT: key_code = WXK_PRINT; break;

	default: key_code = 0;
	}

	return key_code;
}
