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

#pragma once

#include "Pcsx2Defs.h"
#include "App.h"

#if defined(__unix__) || defined(__APPLE__)

#ifndef __APPLE__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

// x11 is dumb like that
#undef DisableScreenSaver
#endif

extern void AnalyzeKeyEvent(keyEvent& evt);
extern void UpdateKeyboardInput();
extern bool PollForNewKeyboardKeys(u32& pkey);
#endif

#if defined(__unix__)
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

extern Display* GSdsp;
extern Window GSwin;

#elif defined(__APPLE__)
#include <Carbon/Carbon.h>
#endif
