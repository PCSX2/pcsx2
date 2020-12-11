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

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include "PAD.h"

#if defined(__unix__) || defined(__APPLE__)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
// x11 is dumb like that
#undef DisableScreenSaver

extern void AnalyzeKeyEvent(keyEvent& evt);
extern void UpdateKeyboardInput();
extern bool PollForNewKeyboardKeys(u32& pkey);
#ifndef __APPLE__
extern Display* GSdsp;
extern Window GSwin;
#endif

#else

extern char* KeysymToChar(int keysym);
extern WNDPROC GSwndProc;
extern HWND GShwnd;

#endif

#endif
