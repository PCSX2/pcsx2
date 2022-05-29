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

#include "common/RedtapeWindows.h"

#include <commdlg.h>

#include "usb-msd.h"
#include "USB/Win32/Config_usb.h"
#include "USB/Win32/resource_usb.h"

namespace usb_msd
{

	static OPENFILENAMEW ofn;
	BOOL CALLBACK MsdDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		int port;
		static wchar_t buff[4096] = {0};

		switch (uMsg)
		{
			case WM_INITDIALOG:
			{
				memset(buff, 0, sizeof(buff));
				port = (int)lParam;
				SetWindowLongPtr(hW, GWLP_USERDATA, (LONG)lParam);

				std::wstring var;
				if (LoadSetting(MsdDevice::TypeName(), port, APINAME, N_CONFIG_PATH, var))
					wcsncpy_s(buff, sizeof(buff), var.c_str(), countof(buff));
				SetWindowTextW(GetDlgItem(hW, IDC_EDIT1_USB), buff);
				return TRUE;
			}
			case WM_CREATE:
				SetWindowLongPtr(hW, GWLP_USERDATA, (LONG)lParam);
				break;
			case WM_COMMAND:

				if (HIWORD(wParam) == BN_CLICKED)
				{
					switch (LOWORD(wParam))
					{
						case IDC_BUTTON1_USB:
							ZeroMemory(&ofn, sizeof(ofn));
							ofn.lStructSize = sizeof(ofn);
							ofn.hwndOwner = hW;
							ofn.lpstrTitle = L"USB image file";
							ofn.lpstrFile = buff;
							ofn.nMaxFile = countof(buff);
							ofn.lpstrFilter = L"All\0*.*\0";
							ofn.nFilterIndex = 1;
							ofn.lpstrFileTitle = NULL;
							ofn.nMaxFileTitle = 0;
							ofn.lpstrInitialDir = NULL;
							ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

							if (GetOpenFileName(&ofn))
							{
								SetWindowText(GetDlgItem(hW, IDC_EDIT1_USB), ofn.lpstrFile);
							}
							break;
						case IDOK:
						{
							INT_PTR res = RESULT_OK;
							GetWindowTextW(GetDlgItem(hW, IDC_EDIT1_USB), buff, countof(buff));
							port = (int)GetWindowLongPtr(hW, GWLP_USERDATA);
							if (!SaveSetting<std::wstring>(MsdDevice::TypeName(), port, APINAME, N_CONFIG_PATH, buff))
								res = RESULT_FAILED;
							//strcpy_s(conf.usb_img, ofn.lpstrFile);
							EndDialog(hW, res);
							return TRUE;
						}
						case IDCANCEL:
							EndDialog(hW, FALSE);
							return TRUE;
					}
				}
		}
		return FALSE;
	}

	int MsdDevice::Configure(int port, const std::string& api, void* data)
	{
		Win32Handles handles = *(Win32Handles*)data;
		return DialogBoxParam(handles.hInst,
							  MAKEINTRESOURCE(IDD_DLGMSD_USB),
							  handles.hWnd,
							  (DLGPROC)MsdDlgProc, port);
	}

} // namespace usb_msd
