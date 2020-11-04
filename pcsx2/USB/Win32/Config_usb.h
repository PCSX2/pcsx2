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

#ifndef WIN32_H
#define WIN32_H
#include <commctrl.h>

typedef struct Win32Handles
{
	HINSTANCE hInst;
	HWND hWnd;
	Win32Handles(HINSTANCE i, HWND w)
		: hInst(i)
		, hWnd(w)
	{
	}
} Win32Handles;

#define CHECKED_SET_MAX_INT(var, hDlg, nIDDlgItem, bSigned, min, max)           \
	do                                                                          \
	{                                                                           \
		/*CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);*/ \
		var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);                   \
		if (var < min)                                                          \
			var = min;                                                          \
		else if (var > max)                                                     \
		{                                                                       \
			var = max;                                                          \
			SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);                      \
			SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);       \
		}                                                                       \
	} while (0)
#endif
