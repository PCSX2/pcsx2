/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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
#include <Shlwapi.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include "GS.h"
#include "GSDialog.h"
#include "GS/GSVector.h"

GSDialog::GSDialog(UINT id)
	: m_id(id)
	, m_hWnd(NULL)
{
}

INT_PTR GSDialog::DoModal()
{
	return DialogBoxParam(GetModuleHandle(nullptr), MAKEINTRESOURCE(m_id), GetActiveWindow(), DialogProc, (LPARAM)this);
}

INT_PTR CALLBACK GSDialog::DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	GSDialog* dlg = NULL;

	if (message == WM_INITDIALOG)
	{
		dlg = (GSDialog*)lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)dlg);
		dlg->m_hWnd = hWnd;

		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
		GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi);

		GSVector4i r;
		GetWindowRect(hWnd, reinterpret_cast<LPRECT>(&r));

		int x = (mi.rcWork.left + mi.rcWork.right - r.width()) / 2;
		int y = (mi.rcWork.top + mi.rcWork.bottom - r.height()) / 2;

		SetWindowPos(hWnd, NULL, x, y, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

		dlg->OnInit();

		return true;
	}

	dlg = (GSDialog*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	if (message == WM_NOTIFY)
	{
		if (((LPNMHDR)lParam)->code == TTN_GETDISPINFO)
		{
			LPNMTTDISPINFO pInfo = (LPNMTTDISPINFO)lParam;
			const UINT id = (UINT)GetWindowLongPtr((HWND)pInfo->hdr.idFrom, GWL_ID);

			// lpszText is used only if hinst is NULL. Seems to be NULL already,
			// but it can't hurt to explicitly set it.
			pInfo->hinst = NULL;
			pInfo->lpszText = (LPTSTR)dialog_message(id);
			SendMessage(pInfo->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 500);
			return true;
		}
	}

	return dlg != NULL ? dlg->OnMessage(message, wParam, lParam) : FALSE;
}

// Tooltips will only show if the TOOLINFO cbSize <= the struct size. If it's
// smaller some functionality might be disabled. So let's try and use the
// correct size.
UINT GSDialog::GetTooltipStructSize()
{
	DLLGETVERSIONPROC dllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(GetModuleHandle(L"ComCtl32.dll"), "DllGetVersion");
	if (dllGetVersion)
	{
		DLLVERSIONINFO2 dllversion = {0};
		dllversion.info1.cbSize = sizeof(DLLVERSIONINFO2);

		if (dllGetVersion((DLLVERSIONINFO*)&dllversion) == S_OK)
		{
			// Minor, then major version.
			DWORD version = MAKELONG(dllversion.info1.dwMinorVersion, dllversion.info1.dwMajorVersion);
			DWORD tooltip_v3 = MAKELONG(0, 6);
			if (version >= tooltip_v3)
				return TTTOOLINFOA_V3_SIZE;
		}
	}
	// Should be fine for XP and onwards, comctl versions >= 4.7 should at least
	// be this size.
	return TTTOOLINFOA_V2_SIZE;
}

bool GSDialog::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	return message == WM_COMMAND ? OnCommand((HWND)lParam, LOWORD(wParam), HIWORD(wParam)) : false;
}

bool GSDialog::OnCommand(HWND hWnd, UINT id, UINT code)
{
	if (id == IDOK || id == IDCANCEL)
	{
		EndDialog(m_hWnd, id);

		return true;
	}

	return false;
}

std::wstring GSDialog::GetText(UINT id)
{
	std::wstring s;

	wchar_t* buff = NULL;

	for (int size = 256, limit = 65536; size < limit; size <<= 1)
	{
		buff = new wchar_t[size];

		if (GetDlgItemText(m_hWnd, id, buff, size))
		{
			s = buff;
			size = limit;
		}

		delete[] buff;
	}

	return s;
}

int GSDialog::GetTextAsInt(UINT id)
{
	return _wtoi(GetText(id).c_str());
}

void GSDialog::SetText(UINT id, const wchar_t* str)
{
	SetDlgItemText(m_hWnd, id, str);
}

void GSDialog::SetTextAsInt(UINT id, int i)
{
	wchar_t buff[32] = {0};
	_itow(i, buff, 10);
	SetText(id, buff);
}

void GSDialog::ComboBoxInit(UINT id, const std::vector<GSSetting>& settings, int32_t selectionValue, int32_t maxValue)
{
	if (settings.empty())
		return;

	HWND hWnd = GetDlgItem(m_hWnd, id);

	SendMessage(hWnd, CB_RESETCONTENT, 0, 0);

	const auto is_present = [=](const GSSetting& x) { return selectionValue == x.value; };
	if (std::none_of(settings.begin(), settings.end(), is_present))
		selectionValue = settings.front().value;

	for (size_t i = 0; i < settings.size(); i++)
	{
		const GSSetting& s = settings[i];

		if (s.value <= maxValue)
		{
			std::string str(s.name);

			if (!s.note.empty())
			{
				str = str + " (" + s.note + ")";
			}

			ComboBoxAppend(id, str.c_str(), (LPARAM)s.value, s.value == selectionValue);
		}
	}

	ComboBoxFixDroppedWidth(id);
}

int GSDialog::ComboBoxAppend(UINT id, const char* str, LPARAM data, bool select)
{
	HWND hWnd = GetDlgItem(m_hWnd, id);
	int item = (int)SendMessageA(hWnd, CB_ADDSTRING, 0, (LPARAM)str);
	return BoxAppend(hWnd, item, data, select);
}

int GSDialog::ComboBoxAppend(UINT id, const wchar_t* str, LPARAM data, bool select)
{
	HWND hWnd = GetDlgItem(m_hWnd, id);
	int item = (int)SendMessageW(hWnd, CB_ADDSTRING, 0, (LPARAM)str);
	return BoxAppend(hWnd, item, data, select);
}

int GSDialog::BoxAppend(HWND& hWnd, int item, LPARAM data, bool select)
{
	SendMessage(hWnd, CB_SETITEMDATA, item, (LPARAM)data);

	if (select)
	{
		SendMessage(hWnd, CB_SETCURSEL, item, 0);
	}

	return item;
}

bool GSDialog::ComboBoxGetSelData(UINT id, INT_PTR& data)
{
	HWND hWnd = GetDlgItem(m_hWnd, id);

	const int item = (int)SendMessage(hWnd, CB_GETCURSEL, 0, 0);

	if (item >= 0)
	{
		data = SendMessage(hWnd, CB_GETITEMDATA, item, 0);

		return true;
	}

	return false;
}

void GSDialog::ComboBoxFixDroppedWidth(UINT id)
{
	HWND hWnd = GetDlgItem(m_hWnd, id);

	int count = (int)SendMessage(hWnd, CB_GETCOUNT, 0, 0);

	if (count > 0)
	{
		HDC hDC = GetDC(hWnd);

		SelectObject(hDC, (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0));

		int width = (int)SendMessage(hWnd, CB_GETDROPPEDWIDTH, 0, 0);

		for (int i = 0; i < count; i++)
		{
			int len = (int)SendMessage(hWnd, CB_GETLBTEXTLEN, i, 0);

			if (len > 0)
			{
				wchar_t* buff = new wchar_t[len + 1];

				SendMessage(hWnd, CB_GETLBTEXT, i, (LPARAM)buff);

				SIZE size;

				if (GetTextExtentPoint32(hDC, buff, wcslen(buff), &size))
				{
					size.cx += 10;

					if (size.cx > width)
						width = size.cx;
				}

				delete[] buff;
			}
		}

		ReleaseDC(hWnd, hDC);

		if (width > 0)
		{
			SendMessage(hWnd, CB_SETDROPPEDWIDTH, width, 0);
		}
	}
}

void GSDialog::OpenFileDialog(UINT id, const wchar_t* title)
{
	wchar_t filename[512];
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
	ofn.lpstrFile = filename;
	ofn.lpstrFile[0] = 0;
	ofn.nMaxFile = 512;
	ofn.lpstrTitle = title;

	// GetOpenFileName changes the current directory, so we need to save and
	// restore the current directory or everything using relative paths will
	// break.
	wchar_t current_directory[512];
	GetCurrentDirectory(512, current_directory);

	if (GetOpenFileName(&ofn))
		SendMessage(GetDlgItem(m_hWnd, id), WM_SETTEXT, 0, (LPARAM)filename);

	SetCurrentDirectory(current_directory);
}

void GSDialog::AddTooltip(UINT id)
{
	static UINT tooltipStructSize = GetTooltipStructSize();
	bool hasTooltip;

	dialog_message(id, &hasTooltip);
	if (!hasTooltip)
		return;

	HWND hWnd = GetDlgItem(m_hWnd, id);
	if (hWnd == NULL)
		return;

	// TTS_NOPREFIX allows tabs and '&' to be used.
	HWND hwndTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
		TTS_ALWAYSTIP | TTS_NOPREFIX,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		m_hWnd, NULL, GetModuleHandle(nullptr), NULL);
	if (hwndTip == NULL)
		return;

	TOOLINFO toolInfo = {0};
	toolInfo.cbSize = tooltipStructSize;
	toolInfo.hwnd = m_hWnd;
	toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	toolInfo.uId = (UINT_PTR)hWnd;
	// Can't directly add the tooltip string - it doesn't work for long messages
	toolInfo.lpszText = LPSTR_TEXTCALLBACK;
	SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	// 32.767s is the max show time.
	SendMessage(hwndTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 32767);
}

void GSDialog::InitCommonControls()
{
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_TAB_CLASSES;

	InitCommonControlsEx(&icex);
}
