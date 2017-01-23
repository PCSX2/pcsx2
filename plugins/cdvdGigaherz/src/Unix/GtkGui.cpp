/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2016  PCSX2 Dev Team
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

#include "CDVD.h"
#include <gtk/gtk.h>

std::vector<std::string> GetOpticalDriveList();

static void ComboboxCallback(GtkComboBoxText *combobox, gpointer data)
{
    Settings *settings = reinterpret_cast<Settings *>(data);
    settings->Set("drive", gtk_combo_box_text_get_active_text(combobox));
}

void configure()
{
    ReadSettings();

    GtkDialogFlags flags = static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT);
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Config", nullptr,
                                                    flags,
                                                    "Cancel", GTK_RESPONSE_REJECT,
                                                    "Ok", GTK_RESPONSE_ACCEPT,
                                                    nullptr);

    GtkWidget *label = gtk_label_new("Device:");
    GtkWidget *combobox = gtk_combo_box_text_new();

    auto drives = GetOpticalDriveList();
    std::string drive;
    g_settings.Get("drive", drive);
    for (size_t n = 0; n < drives.size(); ++n) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combobox), drives[n].c_str());
        if (drive == drives[n])
            gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), n);
    }

#if GTK_MAJOR_VERSION >= 3
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
    GtkWidget *box = gtk_vbox_new(0, 10);
#endif
    gtk_box_pack_start(GTK_BOX(box), label, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(box), combobox, 0, 0, 10);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content_area), box);

    Settings settings_copy = g_settings;
    g_signal_connect(combobox, "changed", G_CALLBACK(ComboboxCallback), &settings_copy);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        g_settings = settings_copy;
        WriteSettings();
    }
    gtk_widget_destroy(dialog);
}
