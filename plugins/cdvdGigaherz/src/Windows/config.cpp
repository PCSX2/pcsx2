//Copyright (C) 2016 PCSX2 Dev Team
//Copyright (c) David Quintana <DavidQuintana@canal21.com>
//
//This library is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 3.0 of the License, or (at your option) any later version.
//
//This library is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//Lesser General Public License for more details.
//
//You should have received a copy of the GNU Lesser General Public
//License along with this library; if not, write to the Free Software
//Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
//

#include "../CDVD.h"
#include <Windows.h>
#include <commctrl.h>
#include "resource.h"

static HINSTANCE s_hinstance;

BOOL WINAPI DllMain(HINSTANCE hinstance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
        s_hinstance = hinstance;

    return TRUE;
}

static INT_PTR CALLBACK ConfigProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG: {
            std::vector<std::wstring> &drives =
                *reinterpret_cast<std::vector<std::wstring> *>(lParam);
            HWND combobox = GetDlgItem(hWnd, IDC_DRIVE);
            std::wstring drive;
            g_settings.Get("drive", drive);
            for (size_t n = 0; n < drives.size(); ++n) {
                SendMessage(combobox, CB_ADDSTRING, 0,
                            reinterpret_cast<LPARAM>(drives[n].c_str()));
                if (drive == drives[n])
                    SendMessage(combobox, CB_SETCURSEL, n, 0);
            }
        } break;
        case WM_COMMAND:
            // Parse the menu selections:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    HWND combobox = GetDlgItem(hWnd, IDC_DRIVE);
                    LRESULT index = SendMessage(combobox, CB_GETCURSEL, 0, 0);
                    if (index != CB_ERR) {
                        LRESULT length = SendMessage(combobox, CB_GETLBTEXTLEN,
                                                     index, 0);
                        std::vector<wchar_t> drive(length + 1);
                        SendMessage(combobox, CB_GETLBTEXT, index,
                                    reinterpret_cast<LPARAM>(drive.data()));
                        g_settings.Set("drive", std::wstring(drive.data()));
                        WriteSettings();
                    }
                    EndDialog(hWnd, 0);
                } break;
                case IDCANCEL:
                    EndDialog(hWnd, 0);
                    break;

                default:
                    return FALSE;
            }
            break;
        default:
            return FALSE;
    }
    return TRUE;
}

static std::vector<std::wstring> GetOpticalDriveList()
{
    DWORD size = GetLogicalDriveStrings(0, nullptr);
    std::vector<wchar_t> drive_strings(size);
    if (GetLogicalDriveStrings(size, drive_strings.data()) != size - 1)
        return {};

    std::vector<std::wstring> drives;
    for (auto p = drive_strings.data(); *p; ++p) {
        if (GetDriveType(p) == DRIVE_CDROM)
            drives.push_back(p);
        while (*p)
            ++p;
    }
    return drives;
}

std::wstring GetValidDrive()
{
    std::wstring drive;
    g_settings.Get("drive", drive);
    if (drive.empty() || GetDriveType(drive.c_str()) != DRIVE_CDROM) {
        auto drives = GetOpticalDriveList();
        if (drives.empty())
            return {};
        drive = drives.front();
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, drive.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::vector<char> converted_string(size);
    WideCharToMultiByte(CP_UTF8, 0, drive.c_str(), -1, converted_string.data(), converted_string.size(), nullptr, nullptr);
    printf(" * CDVD: Opening drive '%s'...\n", converted_string.data());

    // The drive string has the form "X:\", but to open the drive, the string
    // has to be in the form "\\.\X:"
    drive.pop_back();
    drive.insert(0, L"\\\\.\\");
    return drive;
}

void configure()
{
    ReadSettings();
    auto drives = GetOpticalDriveList();
    DialogBoxParam(s_hinstance, MAKEINTRESOURCE(IDD_CONFIG), GetActiveWindow(),
                   ConfigProc, reinterpret_cast<LPARAM>(&drives));
}
