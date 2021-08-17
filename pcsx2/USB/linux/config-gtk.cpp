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

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include "gui/AppCoreThread.h"
#include "USB/gtk.h"

#include "USB/configuration.h"
#include "USB/deviceproxy.h"
#include "USB/usb-pad/padproxy.h"
#include "USB/usb-mic/audiodeviceproxy.h"

#include "config.h"
#include "USB/USB.h"

struct SettingsCB
{
	int player;
	std::string device;
	std::string api;
	GtkComboBox* combo;
	GtkComboBox* subtype;
};

gboolean run_msg_dialog(gpointer data)
{
	GtkWidget* dialog = (GtkWidget*)data;
	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return FALSE;
}

static void populateApiWidget(SettingsCB* settingsCB, const std::string& device)
{
	gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(settingsCB->combo)));

	auto dev = RegisterDevice::instance().Device(device);
	int port = 1 - settingsCB->player;
	GtkComboBox* widget = settingsCB->combo;
	if (dev)
	{
		std::string api;

		auto it = changedAPIs.find(std::make_pair(port, device));
		if (it == changedAPIs.end())
		{
			LoadSetting(nullptr, port, device, N_DEVICE_API, api);
			if (!dev->IsValidAPI(api))
				api.clear();
		}
		else
			api = it->second;

		settingsCB->api = api;
		int i = 0;
		for (auto& it : dev->ListAPIs())
		{
			auto name = dev->LongAPIName(it);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), name);
			if (api.size() && api == it)
				gtk_combo_box_set_active(GTK_COMBO_BOX(widget), i);
			else
				gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
			i++;
		}
	}
}

static void populateSubtypeWidget(SettingsCB* settingsCB, const std::string& device)
{
	gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(settingsCB->subtype)));

	auto dev = RegisterDevice::instance().Device(device);
	int port = 1 - settingsCB->player;
	GtkComboBox* widget = settingsCB->subtype;
	if (dev)
	{
		int sel = 0;
		if (!LoadSetting(nullptr, port, device, N_DEV_SUBTYPE, sel))
		{
			changedSubtype[std::make_pair(port, device)] = sel;
		}

		for (auto subtype : dev->SubTypes())
		{
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), subtype.c_str());
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), sel);
	}
}

static void deviceChanged(GtkComboBox* widget, gpointer data)
{
	SettingsCB* settingsCB = (SettingsCB*)data;
	gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	int player = settingsCB->player;
	std::string s;

	if (active > 0)
		s = RegisterDevice::instance().Name(active - 1);

	settingsCB->device = s;
	populateApiWidget(settingsCB, s);
	populateSubtypeWidget(settingsCB, s);

	if (player == 0)
		conf.Port[1] = s;
	else
		conf.Port[0] = s;

}

static void apiChanged(GtkComboBox* widget, gpointer data)
{
	SettingsCB* settingsCB = (SettingsCB*)data;
	int player = settingsCB->player;
	gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	int port = 1 - player;

	auto& name = settingsCB->device;
	auto dev = RegisterDevice::instance().Device(name);
	if (dev)
	{
		auto apis = dev->ListAPIs();
		auto it = apis.begin();
		std::advance(it, active);
		if (it != apis.end())
		{
			auto pair = std::make_pair(port, name);
			auto itAPI = changedAPIs.find(pair);

			if (itAPI != changedAPIs.end())
				itAPI->second = *it;
			else
				changedAPIs[pair] = *it;
			settingsCB->api = *it;
		}
	}
}

static void subtypeChanged(GtkComboBox* widget, gpointer data)
{
	SettingsCB* settingsCB = (SettingsCB*)data;
	int player = settingsCB->player;
	gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	int port = 1 - player;

	auto& name = settingsCB->device;
	auto dev = RegisterDevice::instance().Device(name);
	if (dev)
	{
		changedSubtype[std::make_pair(port, name)] = active;
	}
}

static void configureApi(GtkWidget* widget, gpointer data)
{
	SettingsCB* settingsCB = (SettingsCB*)data;
	int player = settingsCB->player;
	int port = 1 - player;

	auto& name = settingsCB->device;
	auto& api = settingsCB->api;
	auto dev = RegisterDevice::instance().Device(name);

	if (dev)
	{
		GtkWidget* dlg = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "dlg"));
		[[maybe_unused]]int res = dev->Configure(port, api, dlg);
	}
}

GtkWidget* new_combobox(const char* label, GtkWidget* vbox, bool scrollable)
{
	GtkWidget *rs_hbox, *rs_label, *rs_cb;

	rs_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), rs_hbox, FALSE, TRUE, 0);

	rs_label = gtk_label_new(label);
	gtk_box_pack_start(GTK_BOX(rs_hbox), rs_label, FALSE, TRUE, 5);
	gtk_label_set_justify(GTK_LABEL(rs_label), GTK_JUSTIFY_RIGHT);

	rs_cb = gtk_combo_box_text_new();
	if (!scrollable)
		gtk_box_pack_start(GTK_BOX(rs_hbox), rs_cb, TRUE, TRUE, 5);
	else
	{
		GtkWidget* sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(sw), rs_cb);
		gtk_box_pack_start(GTK_BOX(rs_hbox), sw, TRUE, TRUE, 5);
	}
	return rs_cb;
}

static GtkWidget* new_frame(const char* label, GtkWidget* box)
{
	GtkWidget* ro_frame = gtk_frame_new(NULL);
	gtk_box_pack_start(GTK_BOX(box), ro_frame, TRUE, FALSE, 0);

	GtkWidget* ro_label = gtk_label_new(label);
	gtk_frame_set_label_widget(GTK_FRAME(ro_frame), ro_label);
	gtk_label_set_use_markup(GTK_LABEL(ro_label), TRUE);

	GtkWidget* vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(ro_frame), vbox);
	return vbox;
}


void USBconfigure()
{
	ScopedCoreThreadPause paused_core;

    USBsetSettingsDir();
	RegisterDevice::Register();
	LoadConfig();
	SettingsCB settingsCB[2];
	settingsCB[0].player = 0;
	settingsCB[1].player = 1;

	const char* players[] = {"Player 1:", "Player 2:"};

	GtkWidget *rs_cb, *vbox;

	// Create the dialog window
	GtkWidget* dlg = gtk_dialog_new_with_buttons(
		"USB Settings", NULL, GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW(dlg), TRUE);
	GtkWidget* dlg_area_box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
	GtkWidget* main_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(dlg_area_box), main_vbox);

	/*** Device type ***/
	vbox = new_frame("Select device type:", main_vbox);

	std::string devs[2] = {conf.Port[1], conf.Port[0]};
	/*** Devices' Comboboxes ***/
	for (int ply = 0; ply < 2; ply++)
	{
		settingsCB[ply].device = devs[ply];

		rs_cb = new_combobox(players[ply], vbox);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(rs_cb), "None");
		gtk_combo_box_set_active(GTK_COMBO_BOX(rs_cb), 0);

		auto devices = RegisterDevice::instance().Names();
		int idx = 0;
		for (auto& device : devices)
		{
			auto deviceProxy = RegisterDevice::instance().Device(device);
			if (!deviceProxy)
			{
				continue;
			}
			auto name = deviceProxy->Name();
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(rs_cb), name);
			idx++;
			if (devs[ply] == device)
				gtk_combo_box_set_active(GTK_COMBO_BOX(rs_cb), idx);
		}
		g_signal_connect(G_OBJECT(rs_cb), "changed", G_CALLBACK(deviceChanged), (gpointer)&settingsCB[ply]);
	}

	/*** APIs ***/
	vbox = new_frame("Select device API:", main_vbox);

	/*** API Comboboxes ***/
	for (int ply = 0; ply < 2; ply++)
	{
		rs_cb = new_combobox(players[ply], vbox);
		settingsCB[ply].combo = GTK_COMBO_BOX(rs_cb);
		//gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);
		g_signal_connect(G_OBJECT(rs_cb), "changed", G_CALLBACK(apiChanged), (gpointer)&settingsCB[ply]);

		GtkWidget* hbox = gtk_widget_get_parent(rs_cb);
		GtkWidget* button = gtk_button_new_with_label("Configure");
		gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_icon_name("gtk-preferences", GTK_ICON_SIZE_BUTTON));
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);

		g_signal_connect(button, "clicked", G_CALLBACK(configureApi), (gpointer)&settingsCB[ply]);
		g_object_set_data(G_OBJECT(button), "dlg", dlg);

		populateApiWidget(&settingsCB[ply], devs[ply]);
	}

	/** Emulated device / Wheel type **/
	vbox = new_frame("Emulated device:", main_vbox);

	for (int ply = 0; ply < 2; ply++)
	{
		rs_cb = new_combobox(players[ply], vbox);
		settingsCB[ply].subtype = GTK_COMBO_BOX(rs_cb);
		g_signal_connect(G_OBJECT(rs_cb), "changed", G_CALLBACK(subtypeChanged), (gpointer)&settingsCB[ply]);

		populateSubtypeWidget(&settingsCB[ply], devs[ply]);
	}

	gtk_widget_show_all(dlg);

	// Modal loop
	gint result = gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);

	// Wait for all gtk events to be consumed ...
	while (gtk_events_pending())
		gtk_main_iteration_do(FALSE);

	if (result == GTK_RESPONSE_OK)
	{
		SaveConfig();
		CreateDevices();
	}
	//	ClearAPIs();
	paused_core.AllowResume();
}

void CALLBACK USBabout()
{
}
