/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2018 PCSX2 Dev Team
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

#include "PS2Eext.h"
#if defined(_WIN32)
#include <windows.h>
#include "resource.h"
#elif defined(__unix__) || defined(__APPLE__)
#include <wx/wx.h>
#endif
#include <string>

PluginLog g_plugin_log;

#if defined(_WIN32)

static HINSTANCE s_hinstance;

BOOL APIENTRY DllMain(HINSTANCE hinstance, DWORD reason, LPVOID /* reserved */)
{
    if (reason == DLL_PROCESS_ATTACH)
        s_hinstance = hinstance;
    return TRUE;
}

static INT_PTR CALLBACK ConfigureDialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
        case WM_INITDIALOG:
            CheckDlgButton(dialog, IDC_LOG_TO_CONSOLE, g_plugin_log.WriteToConsole);
            CheckDlgButton(dialog, IDC_LOG_TO_FILE, g_plugin_log.WriteToFile);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case IDOK:
                    g_plugin_log.WriteToConsole = IsDlgButtonChecked(dialog, IDC_LOG_TO_CONSOLE) == BST_CHECKED;
                    g_plugin_log.WriteToFile = IsDlgButtonChecked(dialog, IDC_LOG_TO_FILE) == BST_CHECKED;
                    EndDialog(dialog, 0);
                    return TRUE;
                case IDCANCEL:
                    EndDialog(dialog, 0);
                    return TRUE;
                default:
                    return FALSE;
            }
        default:
            return FALSE;
    }
}

void ConfigureLogging()
{
    DialogBox(s_hinstance, MAKEINTRESOURCE(IDD_DIALOG), GetActiveWindow(), ConfigureDialogProc);
}

#elif defined(__unix__) || defined(__APPLE__)

void ConfigureLogging()
{
    auto *dialog = new wxDialog;
    dialog->Create(nullptr, wxID_ANY, "Config", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX);

    auto *main_sizer = new wxBoxSizer(wxVERTICAL);
    auto *sizer = dialog->CreateButtonSizer(wxOK | wxCANCEL);

    auto *log_check = new wxCheckBox(dialog, wxID_ANY, "Log to Console");
    auto *file_check = new wxCheckBox(dialog, wxID_ANY, "Log to File");
    log_check->SetValue(g_plugin_log.WriteToConsole);
    file_check->SetValue(g_plugin_log.WriteToFile);

    main_sizer->Add(log_check);
    main_sizer->Add(file_check);
    main_sizer->Add(sizer);

    dialog->SetSizerAndFit(main_sizer);

    if ( dialog->ShowModal() == wxID_OK )
    {
        g_plugin_log.WriteToConsole = log_check->GetValue();
        g_plugin_log.WriteToFile = file_check->GetValue();
    }
    wxDELETE(dialog);
}

#else

void ConfigureLogging()
{
}

#endif

void SaveConfig(const std::string &pathname)
{
    PluginConf ini;
    if (!ini.Open(pathname, WRITE_FILE)) {
        g_plugin_log.WriteLn("Failed to open %s", pathname.c_str());
        return;
    }

    ini.WriteInt("write_to_console", g_plugin_log.WriteToConsole);
    ini.WriteInt("write_to_file", g_plugin_log.WriteToFile);
    ini.Close();
}

void LoadConfig(const std::string &pathname)
{
    PluginConf ini;
    if (!ini.Open(pathname, READ_FILE)) {
        g_plugin_log.WriteLn("Failed to open %s", pathname.c_str());
        SaveConfig(pathname);
        return;
    }

    g_plugin_log.WriteToConsole = ini.ReadInt("write_to_console", 0) != 0;
    g_plugin_log.WriteToFile = ini.ReadInt("write_to_file", 0) != 0;
    ini.Close();
}
