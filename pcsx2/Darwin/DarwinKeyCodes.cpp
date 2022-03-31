/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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
#include <Carbon/Carbon.h>

int TranslateOSXtoWXK(u32 keysym)
{
	switch (keysym)
	{
		case kVK_ANSI_A:                    return 'A';
		case kVK_ANSI_S:                    return 'S';
		case kVK_ANSI_D:                    return 'D';
		case kVK_ANSI_F:                    return 'F';
		case kVK_ANSI_H:                    return 'H';
		case kVK_ANSI_G:                    return 'G';
		case kVK_ANSI_Z:                    return 'Z';
		case kVK_ANSI_X:                    return 'X';
		case kVK_ANSI_C:                    return 'C';
		case kVK_ANSI_V:                    return 'V';
		case kVK_ANSI_B:                    return 'B';
		case kVK_ANSI_Q:                    return 'Q';
		case kVK_ANSI_W:                    return 'W';
		case kVK_ANSI_E:                    return 'E';
		case kVK_ANSI_R:                    return 'R';
		case kVK_ANSI_Y:                    return 'Y';
		case kVK_ANSI_T:                    return 'T';
		case kVK_ANSI_1:                    return '1';
		case kVK_ANSI_2:                    return '2';
		case kVK_ANSI_3:                    return '3';
		case kVK_ANSI_4:                    return '4';
		case kVK_ANSI_6:                    return '6';
		case kVK_ANSI_5:                    return '5';
		case kVK_ANSI_Equal:                return '=';
		case kVK_ANSI_9:                    return '9';
		case kVK_ANSI_7:                    return '7';
		case kVK_ANSI_Minus:                return '-';
		case kVK_ANSI_8:                    return '8';
		case kVK_ANSI_0:                    return '0';
		case kVK_ANSI_RightBracket:         return ']';
		case kVK_ANSI_O:                    return 'O';
		case kVK_ANSI_U:                    return 'U';
		case kVK_ANSI_LeftBracket:          return '[';
		case kVK_ANSI_I:                    return 'I';
		case kVK_ANSI_P:                    return 'P';
		case kVK_ANSI_L:                    return 'L';
		case kVK_ANSI_J:                    return 'J';
		case kVK_ANSI_Quote:                return '\'';
		case kVK_ANSI_K:                    return 'K';
		case kVK_ANSI_Semicolon:            return ';';
		case kVK_ANSI_Backslash:            return '\\';
		case kVK_ANSI_Comma:                return ',';
		case kVK_ANSI_Slash:                return '/';
		case kVK_ANSI_N:                    return 'N';
		case kVK_ANSI_M:                    return 'M';
		case kVK_ANSI_Period:               return '.';
		case kVK_ANSI_Grave:                return '`';
		case kVK_ANSI_KeypadDecimal:        return WXK_NUMPAD_DECIMAL;
		case kVK_ANSI_KeypadMultiply:       return WXK_NUMPAD_MULTIPLY;
		case kVK_ANSI_KeypadPlus:           return WXK_NUMPAD_ADD;
		case kVK_ANSI_KeypadClear:          return WXK_CLEAR;
		case kVK_ANSI_KeypadDivide:         return WXK_NUMPAD_DIVIDE;
		case kVK_ANSI_KeypadEnter:          return WXK_NUMPAD_ENTER;
		case kVK_ANSI_KeypadMinus:          return WXK_NUMPAD_SUBTRACT;
		case kVK_ANSI_KeypadEquals:         return WXK_NUMPAD_EQUAL;
		case kVK_ANSI_Keypad0:              return WXK_NUMPAD0;
		case kVK_ANSI_Keypad1:              return WXK_NUMPAD1;
		case kVK_ANSI_Keypad2:              return WXK_NUMPAD2;
		case kVK_ANSI_Keypad3:              return WXK_NUMPAD3;
		case kVK_ANSI_Keypad4:              return WXK_NUMPAD4;
		case kVK_ANSI_Keypad5:              return WXK_NUMPAD5;
		case kVK_ANSI_Keypad6:              return WXK_NUMPAD6;
		case kVK_ANSI_Keypad7:              return WXK_NUMPAD7;
		case kVK_ANSI_Keypad8:              return WXK_NUMPAD8;
		case kVK_ANSI_Keypad9:              return WXK_NUMPAD9;

		case kVK_Return:                    return WXK_RETURN;
		case kVK_Tab:                       return WXK_TAB;
		case kVK_Space:                     return WXK_SPACE;
		case kVK_Delete:                    return WXK_BACK;
		case kVK_Escape:                    return WXK_ESCAPE;
		case kVK_Command:                   return WXK_COMMAND;
		case kVK_Shift:                     return WXK_SHIFT;
		case kVK_CapsLock:                  return WXK_CAPITAL;
		case kVK_Option:                    return WXK_ALT;
		case kVK_Control:                   return WXK_RAW_CONTROL;
		case kVK_RightCommand:              return WXK_COMMAND;
		case kVK_RightShift:                return WXK_SHIFT;
		case kVK_RightOption:               return WXK_ALT;
		case kVK_RightControl:              return WXK_RAW_CONTROL;
		case kVK_Function:                  return 0;
		case kVK_F17:                       return WXK_F17;
#if wxCHECK_VERSION(3, 1, 0)
		case kVK_VolumeUp:                  return WXK_VOLUME_UP;
		case kVK_VolumeDown:                return WXK_VOLUME_DOWN;
		case kVK_Mute:                      return WXK_VOLUME_MUTE;
#endif
		case kVK_F18:                       return WXK_F18;
		case kVK_F19:                       return WXK_F19;
		case kVK_F20:                       return WXK_F20;
		case kVK_F5:                        return WXK_F5;
		case kVK_F6:                        return WXK_F6;
		case kVK_F7:                        return WXK_F7;
		case kVK_F3:                        return WXK_F3;
		case kVK_F8:                        return WXK_F8;
		case kVK_F9:                        return WXK_F9;
		case kVK_F11:                       return WXK_F11;
		case kVK_F13:                       return WXK_F13;
		case kVK_F16:                       return WXK_F16;
		case kVK_F14:                       return WXK_F14;
		case kVK_F10:                       return WXK_F10;
		case kVK_F12:                       return WXK_F12;
		case kVK_F15:                       return WXK_F15;
		case kVK_Help:                      return WXK_HELP;
		case kVK_Home:                      return WXK_HOME;
		case kVK_PageUp:                    return WXK_PAGEUP;
		case kVK_ForwardDelete:             return WXK_DELETE;
		case kVK_F4:                        return WXK_F4;
		case kVK_End:                       return WXK_END;
		case kVK_F2:                        return WXK_F2;
		case kVK_PageDown:                  return WXK_PAGEDOWN;
		case kVK_F1:                        return WXK_F1;
		case kVK_LeftArrow:                 return WXK_LEFT;
		case kVK_RightArrow:                return WXK_RIGHT;
		case kVK_DownArrow:                 return WXK_DOWN;
		case kVK_UpArrow:                   return WXK_UP;
		default:
			return 0;
	}
}
