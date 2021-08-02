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

#include "USB/usb-hid/usb-hid.h"
#include "evdev.h"
#include <linux/input.h>
#include "USB/gtk.h"
#include <cstdio>
#include <sstream>

namespace usb_hid
{
	namespace evdev
	{

#define EVDEV_DIR "/dev/input/by-path/"

		typedef std::vector<std::pair<std::string, std::string>> devs_t;
		struct ConfigData
		{
			int port;
			devs_t devs;
			devs_t::const_iterator iter;
		};

		static void PopulateHIDs(ConfigData& cfg, HIDType hid_type)
		{
			std::stringstream str;
			struct dirent* dp;
			const char* devstr[] = {"event-kbd", "event-mouse"};

			cfg.devs.clear();
			cfg.devs.push_back(std::make_pair("None", ""));

			DIR* dirp = opendir(EVDEV_DIR);
			if (dirp == NULL)
			{
				Console.Warning("Error opening " EVDEV_DIR ": %s\n", strerror(errno));
				return;
			}

			// Loop over dir entries using readdir
			int len = strlen(devstr[hid_type]);
			while ((dp = readdir(dirp)) != NULL)
			{
				// Only select names that end in 'event-joystick'
				int devlen = strlen(dp->d_name);
				if (devlen >= len)
				{
					const char* const start = dp->d_name + devlen - len;
					if (strncmp(start, devstr[hid_type], len) == 0)
					{

						str.clear();
						str.str("");
						str << EVDEV_DIR << dp->d_name;

						char name[1024];
						std::string dev_path = str.str();
						if (!GetEvdevName(dev_path, name))
						{
							//XXX though it also could mean that controller is unusable
							cfg.devs.push_back(std::make_pair(dp->d_name, dev_path));
						}
						else
						{
							cfg.devs.push_back(std::make_pair(std::string(name), dev_path));
						}
					}
				}
			}
			closedir(dirp);
		}

		static void combo_changed(GtkComboBox* widget, gpointer data)
		{
			gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
			ConfigData* cfg = reinterpret_cast<ConfigData*>(data);

			if (!cfg)
				return;

			//std::string& name = (cfg->devs.begin() + idx)->first;
			cfg->iter = (cfg->devs.begin() + idx);

			if (idx > 0)
			{
			}
		}

		int GtkHidConfigure(int port, const char* dev_type, HIDType hid_type, GtkWindow* parent)
		{
			GtkWidget *main_hbox, *right_vbox, *rs_cb;

			assert((int)HIDTYPE_MOUSE == 1); //make sure there is atleast two types so we won't go beyond array length

			ConfigData cfg;
			cfg.port = port;

			PopulateHIDs(cfg, hid_type);
			cfg.iter = cfg.devs.end();

			std::string path;
			LoadSetting(dev_type, port, APINAME, N_DEVICE, path);

			// ---------------------------
			GtkWidget* dlg = gtk_dialog_new_with_buttons(
				"HID Evdev Settings", parent, GTK_DIALOG_MODAL,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
			gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
			gtk_window_set_resizable(GTK_WINDOW(dlg), TRUE);
			gtk_window_set_default_size(GTK_WINDOW(dlg), 320, 240);

			// ---------------------------
			GtkWidget* dlg_area_box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));

			main_hbox = gtk_hbox_new(FALSE, 5);
			gtk_container_add(GTK_CONTAINER(dlg_area_box), main_hbox);

			//	left_vbox = gtk_vbox_new (FALSE, 5);
			//	gtk_box_pack_start (GTK_BOX (main_hbox), left_vbox, TRUE, TRUE, 5);
			right_vbox = gtk_vbox_new(FALSE, 5);
			gtk_box_pack_start(GTK_BOX(main_hbox), right_vbox, TRUE, TRUE, 5);

			// ---------------------------
			rs_cb = new_combobox("Device:", right_vbox);

			const int evdev_dir_len = strlen(EVDEV_DIR);
			int idx = 0, sel_idx = 0;
			for (auto& it : cfg.devs)
			{
				std::stringstream str;
				str << it.first;
				if (!it.second.empty())
					str << " [" << it.second.substr(evdev_dir_len) << "]";

				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(rs_cb), str.str().c_str());
				if (!path.empty() && it.second == path)
				{
					sel_idx = idx;
				}
				idx++;
			}

			//g_object_set_data (G_OBJECT (rs_cb), CFG, &cfg);
			g_signal_connect(G_OBJECT(rs_cb), "changed", G_CALLBACK(combo_changed), reinterpret_cast<gpointer>(&cfg));
			gtk_combo_box_set_active(GTK_COMBO_BOX(rs_cb), sel_idx);

			// ---------------------------
			gtk_widget_show_all(dlg);
			gint result = gtk_dialog_run(GTK_DIALOG(dlg));

			int ret = RESULT_OK;
			if (result == GTK_RESPONSE_OK)
			{
				if (cfg.iter != cfg.devs.end())
				{
					if (!SaveSetting(dev_type, port, APINAME, N_DEVICE, cfg.iter->second))
						ret = RESULT_FAILED;
				}
			}
			else
				ret = RESULT_CANCELED;

			gtk_widget_destroy(dlg);
			return ret;
		}

		int EvDev::Configure(int port, const char* dev_type, HIDType hid_type, void* data)
		{
			return GtkHidConfigure(port, dev_type, hid_type, GTK_WINDOW(data));
		}

#undef EVDEV_DIR
	} // namespace evdev
} // namespace usb_hid
