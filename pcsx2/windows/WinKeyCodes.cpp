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
#include "common/Assertions.h"
#include "common/RedtapeWindows.h"

// VK values without MS constants. see - https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.85%29.aspx
#define PX_VK_A 0x41
#define PX_VK_Z 0x5A

#define PX_VK2WX_SIZE 256
static int vk2wx[PX_VK2WX_SIZE] = { 0 };
static bool initialized = false;

static void addTx(UINT vkCode, int wxCode)
{
	pxAssert(vkCode < PX_VK2WX_SIZE);
	pxAssert(!vk2wx[vkCode]); // we shouldn't override entries here
	vk2wx[vkCode] = wxCode;
}

static void initTable()
{
	// Shift, Control, Alt and Menu
	addTx(VK_SHIFT, WXK_SHIFT);
	addTx(VK_LSHIFT, WXK_SHIFT);
	addTx(VK_RSHIFT, WXK_SHIFT);

	addTx(VK_CONTROL, WXK_CONTROL);
	addTx(VK_LCONTROL, WXK_CONTROL);
	addTx(VK_RCONTROL, WXK_CONTROL);

	addTx(VK_MENU, WXK_ALT);
	addTx(VK_LMENU, WXK_ALT);
	addTx(VK_RMENU, WXK_ALT);

	addTx(VK_LWIN, WXK_WINDOWS_LEFT);
	addTx(VK_RWIN, WXK_WINDOWS_RIGHT);

	addTx(VK_APPS, WXK_WINDOWS_MENU);

	// Scroll, Caps and numlocks
	addTx(VK_SCROLL, WXK_SCROLL);
	addTx(VK_CAPITAL, WXK_CAPITAL);
	addTx(VK_NUMLOCK, WXK_NUMLOCK);

	// cursor and other extended keyboard keys
	addTx(VK_PRIOR, WXK_PAGEUP);
	addTx(VK_NEXT, WXK_PAGEDOWN);
	addTx(VK_HOME, WXK_HOME);
	addTx(VK_END, WXK_END);
	addTx(VK_LEFT, WXK_LEFT);
	addTx(VK_UP, WXK_UP);
	addTx(VK_RIGHT, WXK_RIGHT);
	addTx(VK_DOWN, WXK_DOWN);
	addTx(VK_INSERT, WXK_INSERT);
	addTx(VK_DELETE, WXK_DELETE);

	// numpad keys
	addTx(VK_NUMPAD0, WXK_NUMPAD0);
	addTx(VK_NUMPAD1, WXK_NUMPAD1);
	addTx(VK_NUMPAD2, WXK_NUMPAD2);
	addTx(VK_NUMPAD3, WXK_NUMPAD3);
	addTx(VK_NUMPAD4, WXK_NUMPAD4);
	addTx(VK_NUMPAD5, WXK_NUMPAD5);
	addTx(VK_NUMPAD6, WXK_NUMPAD6);
	addTx(VK_NUMPAD7, WXK_NUMPAD7);
	addTx(VK_NUMPAD8, WXK_NUMPAD8);
	addTx(VK_NUMPAD9, WXK_NUMPAD9);
	addTx(VK_MULTIPLY, WXK_NUMPAD_MULTIPLY);
	addTx(VK_ADD, WXK_NUMPAD_ADD);
	addTx(VK_SEPARATOR, WXK_NUMPAD_SEPARATOR);
	addTx(VK_SUBTRACT, WXK_NUMPAD_SUBTRACT);
	addTx(VK_DECIMAL, WXK_NUMPAD_DECIMAL);
	addTx(VK_DIVIDE, WXK_NUMPAD_DIVIDE);

	// Function keys
	addTx(VK_F1, WXK_F1);
	addTx(VK_F2, WXK_F2);
	addTx(VK_F3, WXK_F3);
	addTx(VK_F4, WXK_F4);
	addTx(VK_F5, WXK_F5);
	addTx(VK_F6, WXK_F6);
	addTx(VK_F7, WXK_F7);
	addTx(VK_F8, WXK_F8);
	addTx(VK_F9, WXK_F9);
	addTx(VK_F10, WXK_F10);
	addTx(VK_F11, WXK_F11);
	addTx(VK_F12, WXK_F12);
	addTx(VK_F13, WXK_F13);
	addTx(VK_F14, WXK_F14);
	addTx(VK_F15, WXK_F15);
	addTx(VK_F16, WXK_F16);
	addTx(VK_F17, WXK_F17);
	addTx(VK_F18, WXK_F18);
	addTx(VK_F19, WXK_F19);
	addTx(VK_F20, WXK_F20);
	addTx(VK_F21, WXK_F21);
	addTx(VK_F22, WXK_F22);
	addTx(VK_F23, WXK_F23);
	addTx(VK_F24, WXK_F24);

	// various other special keys
	addTx(VK_BACK, WXK_BACK);
	addTx(VK_TAB, WXK_TAB);
	addTx(VK_RETURN, WXK_RETURN);
	addTx(VK_ESCAPE, WXK_ESCAPE);
	addTx(VK_SNAPSHOT, WXK_SNAPSHOT);
	addTx(VK_PAUSE, WXK_PAUSE);

	// Not sure what these keys are
	addTx(VK_CLEAR, WXK_CLEAR);
	addTx(VK_HELP, WXK_HELP);
	addTx(VK_SELECT, WXK_SELECT);
	addTx(VK_EXECUTE, WXK_EXECUTE);
	addTx(VK_PRINT, WXK_PRINT);

	// symbol-only keys on all keyboards - return ascii
	addTx(VK_OEM_PERIOD, '.');
	addTx(VK_OEM_PLUS, '=');
	addTx(VK_OEM_MINUS, '-');
	addTx(VK_OEM_COMMA, ',');

	// symbol-only keys on US keyboards - return ascii
	addTx(VK_OEM_1, ';');
	addTx(VK_OEM_2, '/');
	addTx(VK_OEM_3, '`');
	addTx(VK_OEM_4, '[');
	addTx(VK_OEM_5, '\\');
	addTx(VK_OEM_6, ']');
	addTx(VK_OEM_7, '\'');

	pxAssert(PX_VK_A < PX_VK_Z);
	// VK codes for letter keys - return lower case ascii
	for (UINT i = PX_VK_A; i <= PX_VK_Z; i++)
		addTx(i, i + 'a' - PX_VK_A);

	initialized = true;
}


// Returns a WXK_* keycode translated from a VK_* virtual key. wxCharCodeMSWToWx was
// removed from wxWidgets 3, this should work as a replacement.
// Where an ascii code for a printable char exists, we try to return it
int TranslateVKToWXK(u32 keysym)
{
	if (!initialized)
		initTable();

	return keysym < PX_VK2WX_SIZE ? vk2wx[keysym] : 0;
}
