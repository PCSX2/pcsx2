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
#elif defined(__unix__)
#include <gtk/gtk.h>
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

#elif defined(__unix__)

void ConfigureLogging()
{
    GtkDialogFlags flags = static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT);
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Config", nullptr, flags,
                                                    "Cancel", GTK_RESPONSE_REJECT,
                                                    "Ok", GTK_RESPONSE_ACCEPT,
                                                    nullptr);

    GtkWidget *console_checkbox = gtk_check_button_new_with_label("Log to console");
    GtkWidget *file_checkbox = gtk_check_button_new_with_label("Log to file");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(console_checkbox), g_plugin_log.WriteToConsole);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(file_checkbox), g_plugin_log.WriteToFile);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content_area), console_checkbox);
    gtk_container_add(GTK_CONTAINER(content_area), file_checkbox);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        g_plugin_log.WriteToConsole = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(console_checkbox)) == TRUE;
        g_plugin_log.WriteToFile = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(file_checkbox)) == TRUE;
    }

    gtk_widget_destroy(dialog);
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
