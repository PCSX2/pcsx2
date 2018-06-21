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

    GtkContainer *content_area = GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
    gtk_container_add(content_area, label);
    gtk_container_add(content_area, combobox);

    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        if (const char *selected_drive = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combobox))) {
            g_settings.Set("drive", selected_drive);
            WriteSettings();
        }
    }
    gtk_widget_destroy(dialog);
}
