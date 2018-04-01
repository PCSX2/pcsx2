/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2018  PCSX2 Dev Team
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

// This header is essentially a limiter for how much of DPRC (Discord Rich Presence)
// is visible to the rest of the project. This way we can expose access to things that
// matter to us, such as updating the details and icons, while ignoring things such as
// the invite handlers (for now anyways, if NetPlay (cough SmileTheory) takes off they
// could be very useful to add here).

static char* drpcMenuStr = "In the menus";

extern char *drpcGameTitle;
extern u64 drpcStartTime; // "When the timekeeping crisis of 2038 invaded, we were ready."

extern void drpcSetGame(char* newTitle);
extern void drpcUpdate();
extern void drpcShutdown();
extern void drpcInit();
