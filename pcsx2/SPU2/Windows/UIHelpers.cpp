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

#include "PrecompiledHeader.h"
#include "SPU2/Global.h"
#include "Dialogs.h"

int SendDialogMsg(HWND hwnd, int dlgId, UINT code, WPARAM wParam, LPARAM lParam)
{
	return SendMessage(GetDlgItem(hwnd, dlgId), code, wParam, lParam);
}

void AssignSliderValue(HWND idcwnd, HWND hwndDisplay, int value)
{
	value = std::min(std::max(value, 0), 512);
	SendMessage(idcwnd, TBM_SETPOS, TRUE, value);

	wchar_t tbox[32];
	swprintf_s(tbox, L"%d", value);
	SetWindowText(hwndDisplay, tbox);
}

void AssignSliderValue(HWND hWnd, int idc, int editbox, int value)
{
	AssignSliderValue(GetDlgItem(hWnd, idc), GetDlgItem(hWnd, editbox), value);
}

// Generic slider/scroller message handler.  This is succient so long as you
// don't need some kind of secondary event handling functionality, such as
// updating a custom label.
BOOL DoHandleScrollMessage(HWND hwndDisplay, WPARAM wParam, LPARAM lParam)
{
	int wmId = LOWORD(wParam);
	int wmEvent = HIWORD(wParam);

	switch (wmId)
	{
		//case TB_ENDTRACK:
		//case TB_THUMBPOSITION:
		case TB_LINEUP:
		case TB_LINEDOWN:
		case TB_PAGEUP:
		case TB_PAGEDOWN:
			wmEvent = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
		case TB_THUMBTRACK:
			AssignSliderValue((HWND)lParam, hwndDisplay, wmEvent);
			break;

		default:
			return FALSE;
	}
	return TRUE;
}

int GetSliderValue(HWND hWnd, int idc)
{
	int retval = (int)SendMessage(GetDlgItem(hWnd, idc), TBM_GETPOS, 0, 0);
	return GetClamped(retval, 0, 512);
}
