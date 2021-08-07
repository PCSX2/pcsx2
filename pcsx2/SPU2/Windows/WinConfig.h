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

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <initguid.h>
#include <tchar.h>
#include <VersionHelpers.h>

#include "resource.h"

extern HINSTANCE hInstance;

#define SET_CHECK(idc, value) SendMessage(GetDlgItem(hWnd, idc), BM_SETCHECK, ((value) == 0) ? BST_UNCHECKED : BST_CHECKED, 0)
#define HANDLE_CHECK(idc, hvar)                                                                   \
	case idc:                                                                                     \
		(hvar) = !(hvar);                                                                         \
		SendMessage(GetDlgItem(hWnd, idc), BM_SETCHECK, (hvar) ? BST_CHECKED : BST_UNCHECKED, 0); \
		break
#define HANDLE_CHECKNB(idc, hvar) \
	case idc:                     \
		(hvar) = !(hvar);         \
		SendMessage(GetDlgItem(hWnd, idc), BM_SETCHECK, (hvar) ? BST_CHECKED : BST_UNCHECKED, 0)
#define ENABLE_CONTROL(idc, value) EnableWindow(GetDlgItem(hWnd, idc), value)

#define INIT_SLIDER(idc, minrange, maxrange, tickfreq, pagesize, linesize) \
	SendMessage(GetDlgItem(hWnd, idc), TBM_SETRANGEMIN, FALSE, minrange);  \
	SendMessage(GetDlgItem(hWnd, idc), TBM_SETRANGEMAX, FALSE, maxrange);  \
	SendMessage(GetDlgItem(hWnd, idc), TBM_SETTICFREQ, tickfreq, 0);       \
	SendMessage(GetDlgItem(hWnd, idc), TBM_SETPAGESIZE, 0, pagesize);      \
	SendMessage(GetDlgItem(hWnd, idc), TBM_SETLINESIZE, 0, linesize)

#define HANDLE_SCROLL_MESSAGE(idc, idcDisplay) \
	if ((HWND)lParam == GetDlgItem(hWnd, idc)) \
	return DoHandleScrollMessage(GetDlgItem(hWnd, idcDisplay), wParam, lParam)


// *** BEGIN DRIVER-SPECIFIC CONFIGURATION ***
// -------------------------------------------

struct CONFIG_XAUDIO2
{
	wxString Device;
	s8 NumBuffers;

	CONFIG_XAUDIO2()
		: Device()
		, NumBuffers(2)
	{
	}
};

extern CONFIG_XAUDIO2 Config_XAudio2;
