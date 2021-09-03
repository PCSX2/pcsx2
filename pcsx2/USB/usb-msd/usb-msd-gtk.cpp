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

#include "usb-msd.h"
#include "USB/linux/ini.h"
#include "USB/configuration.h"
#include "USB/gtk.h"
#include "common/Console.h"

namespace usb_msd
{

	static void entryChanged(GtkWidget* widget, gpointer data)
	{
#ifndef NDEBUG
		const gchar* text = gtk_entry_get_text(GTK_ENTRY(widget));
		Console.Warning("Entry text:%s\n", text);
#endif
	}

	static void fileChooser(GtkWidget* widget, gpointer data)
	{
		GtkWidget *dialog, *entry = NULL;

		entry = (GtkWidget*)data;
		dialog = gtk_file_chooser_dialog_new("Open File",
											 NULL,
											 GTK_FILE_CHOOSER_ACTION_OPEN,
											 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
											 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
											 NULL);

		//XXX check access? Dialog seems to default to "Recently used" etc.
		//Or set to empty string anyway? Then it seems to default to some sort of "working dir"
		if (access(gtk_entry_get_text(GTK_ENTRY(entry)), F_OK) == 0)
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), gtk_entry_get_text(GTK_ENTRY(entry)));

		if (entry && gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		{
			char* filename;

			filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
			Console.Warning("%s\n", filename);
			gtk_entry_set_text(GTK_ENTRY(entry), filename);
			g_free(filename);
		}

		gtk_widget_destroy(dialog);
	}

	int MsdDevice::Configure(int port, const std::string& api, void* data)
	{
		GtkWidget *ro_frame, *ro_label, *rs_hbox, *vbox;

		GtkWidget* dlg = gtk_dialog_new_with_buttons(
			"Mass Storage Settings", GTK_WINDOW(data), GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
		gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
		gtk_window_set_resizable(GTK_WINDOW(dlg), TRUE);
		GtkWidget* dlg_area_box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));

		ro_frame = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(dlg_area_box), ro_frame, TRUE, FALSE, 5);

		ro_label = gtk_label_new("Select USB image:");
		gtk_frame_set_label_widget(GTK_FRAME(ro_frame), ro_label);
		gtk_label_set_use_markup(GTK_LABEL(ro_label), TRUE);

		vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(ro_frame), vbox);

		rs_hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), rs_hbox, FALSE, TRUE, 0);

		GtkWidget* entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_PATH); //TODO max length

		std::string var;
		if (LoadSetting(TypeName(), port, APINAME, N_CONFIG_PATH, var))
			gtk_entry_set_text(GTK_ENTRY(entry), var.c_str());

		g_signal_connect(entry, "changed", G_CALLBACK(entryChanged), NULL);

		GtkWidget* button = gtk_button_new_with_label("Browse");
		gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_icon_name("gtk-open", GTK_ICON_SIZE_BUTTON));
		g_signal_connect(button, "clicked", G_CALLBACK(fileChooser), entry);

		gtk_box_pack_start(GTK_BOX(rs_hbox), entry, TRUE, TRUE, 5);
		gtk_box_pack_start(GTK_BOX(rs_hbox), button, FALSE, FALSE, 5);

		gtk_widget_show_all(dlg);
		gint result = gtk_dialog_run(GTK_DIALOG(dlg));
		std::string path = gtk_entry_get_text(GTK_ENTRY(entry));
		gtk_widget_destroy(dlg);

		// Wait for all gtk events to be consumed ...
		while (gtk_events_pending())
			gtk_main_iteration_do(FALSE);

		if (result == GTK_RESPONSE_OK)
		{
			if (SaveSetting(TypeName(), port, APINAME, N_CONFIG_PATH, path))
				return RESULT_OK;
			else
				return RESULT_FAILED;
		}

		return RESULT_CANCELED;
	}

} // namespace usb_msd
