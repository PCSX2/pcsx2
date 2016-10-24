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

char CfgFile[MAX_PATH + 10] = "inis/cdvdGigaherz.ini";


/*| Config File Format: |¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
+--+---------------------+------------------------+
|												  |
| Option=Value									  |
|												  |
|												  |
| Boolean Values: TRUE,YES,1,T,Y mean 'true',	  |
|                 everything else means 'false'.  |
|												  |
| All Values are limited to 255 chars.			  |
|												  |
+-------------------------------------------------+
 \*_____________________________________________*/


void CfgWriteBool(char *Section, char *Name, char Value)
{
    char *Data = Value ? "TRUE" : "FALSE";

    WritePrivateProfileString(Section, Name, Data, CfgFile);
}

void CfgWriteInt(char *Section, char *Name, int Value)
{
    char Data[255];
    _itoa(Value, Data, 10);

    WritePrivateProfileString(Section, Name, Data, CfgFile);
}

void CfgWriteStr(char *Section, char *Name, char *Data)
{
    WritePrivateProfileString(Section, Name, Data, CfgFile);
}

/*****************************************************************************/

char CfgReadBool(char *Section, char *Name, char Default)
{
    char Data[255] = "";
    GetPrivateProfileString(Section, Name, "", Data, 255, CfgFile);
    Data[254] = 0;
    if (strlen(Data) == 0) {
        CfgWriteBool(Section, Name, Default);
        return Default;
    }

    if (strcmp(Data, "1") == 0)
        return -1;
    if (strcmp(Data, "Y") == 0)
        return -1;
    if (strcmp(Data, "T") == 0)
        return -1;
    if (strcmp(Data, "YES") == 0)
        return -1;
    if (strcmp(Data, "TRUE") == 0)
        return -1;
    return 0;
}

int CfgReadInt(char *Section, char *Name, int Default)
{
    char Data[255] = "";
    GetPrivateProfileString(Section, Name, "", Data, 255, CfgFile);
    Data[254] = 0;

    if (strlen(Data) == 0) {
        CfgWriteInt(Section, Name, Default);
        return Default;
    }

    return atoi(Data);
}

void CfgReadStr(char *Section, char *Name, char *Data, int DataSize, char *Default)
{
    int sl;
    GetPrivateProfileString(Section, Name, "", Data, DataSize, CfgFile);

    if (strlen(Data) == 0) {
        sl = (int)strlen(Default);
        strncpy(Data, Default, sl > 255 ? 255 : sl);
        CfgWriteStr(Section, Name, Data);
    }
}

/*****************************************************************************/

static INT_PTR CALLBACK ConfigProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG: {
            std::vector<std::string> &drives =
                *reinterpret_cast<std::vector<std::string> *>(lParam);
            HWND combobox = GetDlgItem(hWnd, IDC_DRIVE);
            std::string drive;
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
                        std::vector<char> drive(length + 1);
                        SendMessage(combobox, CB_GETLBTEXT, index,
                                    reinterpret_cast<LPARAM>(drive.data()));
                        g_settings.Set("drive", std::string(drive.data()));
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

static std::vector<std::string> GetOpticalDriveList()
{
    DWORD size = GetLogicalDriveStrings(0, nullptr);
    std::vector<char> drive_strings(size);
    if (GetLogicalDriveStrings(size, drive_strings.data()) != size - 1)
        return {};

    std::vector<std::string> drives;
    for (auto p = drive_strings.data(); *p; ++p) {
        if (GetDriveType(p) == DRIVE_CDROM)
            drives.push_back(p);
        while (*p)
            ++p;
    }
    return drives;
}

std::string GetValidDrive()
{
    std::string drive;
    g_settings.Get("drive", drive);
    if (drive.empty() || GetDriveType(drive.c_str()) != DRIVE_CDROM) {
        auto drives = GetOpticalDriveList();
        if (drives.empty())
            return {};
        drive = drives.front();
    }

    printf(" * CDVD: Opening drive '%s'...\n", drive.c_str());

    // The drive string has the form "X:\", but to open the drive, the string
    // has to be in the form "\\.\X:"
    drive.pop_back();
    drive.insert(0, "\\\\.\\");
    return drive;
}

void configure()
{
    ReadSettings();
    auto drives = GetOpticalDriveList();
    DialogBoxParam(hinst, MAKEINTRESOURCE(IDD_CONFIG), GetActiveWindow(),
                   ConfigProc, reinterpret_cast<LPARAM>(&drives));
}
