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

#include <stdio.h>
#include <arpa/inet.h>

#include <gtk/gtk.h>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <vector>
#include "fmt/format.h"

#include <string>
#include "ghc/filesystem.h"

#include "DEV9/Config.h"
#include "DEV9/DEV9.h"
#include "pcap.h"
#include "DEV9/pcap_io.h"
#include "DEV9/net.h"
#include "DEV9/PacketReader/IP/IP_Address.h"
#include "Config.h"
#include "gui/AppCoreThread.h"

#include "DEV9/ATA/HddCreate.h"

using PacketReader::IP::IP_Address;

static GtkBuilder* builder = nullptr;
std::vector<AdapterEntry> adapters;

void IPControl_SetValue(GtkEntry* entryCtl, IP_Address value)
{
	char addrBuff[INET_ADDRSTRLEN] = {0};
	inet_ntop(AF_INET, &value, addrBuff, INET_ADDRSTRLEN);
	gtk_entry_set_text(entryCtl, addrBuff);
}
IP_Address IPControl_GetValue(GtkEntry* entryCtl)
{
	IP_Address ret;
	if (inet_pton(AF_INET, gtk_entry_get_text(entryCtl), &ret) == 1)
		return ret;
	Console.Error("Invalid IP address entered");
	return {0};
}

void IPControl_Enable(GtkEntry* ipEntry, bool enabled, IP_Address value)
{
	if (enabled)
	{
		gtk_widget_set_sensitive((GtkWidget*)ipEntry, true);
		IPControl_SetValue(ipEntry, value);
	}
	else
	{
		gtk_widget_set_sensitive((GtkWidget*)ipEntry, false);
		IPControl_SetValue(ipEntry, {0});
	}
}

void OnAutoMaskChanged(GtkToggleButton* togglebutton, gpointer usr_data)
{
	IPControl_Enable((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_SUBNET"), !gtk_toggle_button_get_active(togglebutton), config.Mask);
}

void OnAutoGatewayChanged(GtkToggleButton* togglebutton, gpointer usr_data)
{
	IPControl_Enable((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_GATEWAY"), !gtk_toggle_button_get_active(togglebutton), config.Gateway);
}

void OnAutoDNS1Changed(GtkToggleButton* togglebutton, gpointer usr_data)
{
	IPControl_Enable((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_DNS1"), !gtk_toggle_button_get_active(togglebutton), config.DNS1);
}

void OnAutoDNS2Changed(GtkToggleButton* togglebutton, gpointer usr_data)
{
	IPControl_Enable((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_DNS2"), !gtk_toggle_button_get_active(togglebutton), config.DNS2);
}

void OnInterceptChanged(GtkToggleButton* togglebutton, gpointer usr_data)
{
	if (gtk_toggle_button_get_active(togglebutton))
	{
		gtk_widget_set_sensitive((GtkWidget*)gtk_builder_get_object(builder, "IDC_IPADDRESS_IP"), true);
		IPControl_SetValue((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_IP"), config.PS2IP);

		gtk_widget_set_sensitive((GtkWidget*)gtk_builder_get_object(builder, "IDC_CHECK_SUBNET"), true);
		gtk_widget_set_sensitive((GtkWidget*)gtk_builder_get_object(builder, "IDC_CHECK_GATEWAY"), true);
		gtk_widget_set_sensitive((GtkWidget*)gtk_builder_get_object(builder, "IDC_CHECK_DNS1"), true);
		gtk_widget_set_sensitive((GtkWidget*)gtk_builder_get_object(builder, "IDC_CHECK_DNS2"), true);

		gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_SUBNET"), config.AutoMask);
		gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_GATEWAY"), config.AutoGateway);
		gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DNS1"), config.AutoDNS1);
		gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DNS2"), config.AutoDNS2);

		OnAutoMaskChanged((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_SUBNET"), nullptr);
		OnAutoGatewayChanged((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_GATEWAY"), nullptr);
		OnAutoDNS1Changed((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DNS1"), nullptr);
		OnAutoDNS2Changed((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DNS2"), nullptr);
	}
	else
	{
		gtk_widget_set_sensitive((GtkWidget*)gtk_builder_get_object(builder, "IDC_IPADDRESS_IP"), false);
		IPControl_SetValue((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_IP"), {0});

		gtk_widget_set_sensitive((GtkWidget*)gtk_builder_get_object(builder, "IDC_CHECK_SUBNET"), false);
		gtk_widget_set_sensitive((GtkWidget*)gtk_builder_get_object(builder, "IDC_CHECK_GATEWAY"), false);
		gtk_widget_set_sensitive((GtkWidget*)gtk_builder_get_object(builder, "IDC_CHECK_DNS1"), false);
		gtk_widget_set_sensitive((GtkWidget*)gtk_builder_get_object(builder, "IDC_CHECK_DNS2"), false);

		gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_SUBNET"), true);
		gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_GATEWAY"), true);
		gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DNS1"), true);
		gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DNS2"), true);

		IPControl_Enable((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_SUBNET"), false, config.Mask);
		IPControl_Enable((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_GATEWAY"), false, config.Gateway);
		IPControl_Enable((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_DNS1"), false, config.DNS1);
		IPControl_Enable((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_DNS2"), false, config.DNS2);
	}
}

void OnInitDialog()
{
	gint idx = 0;
	static int initialized = 0;

	LoadConf();

	if (initialized)
		return;

	gtk_combo_box_text_append_text((GtkComboBoxText*)gtk_builder_get_object(builder, "IDC_BAYTYPE"), "Expansion");
	gtk_combo_box_text_append_text((GtkComboBoxText*)gtk_builder_get_object(builder, "IDC_BAYTYPE"), "PC Card");

	adapters = PCAPAdapter::GetAdapters();

	for (size_t i = 0; i < adapters.size(); i++)
	{
		std::string dev = fmt::format("{}: {}", (char*)NetApiToString(adapters[i].type), adapters[i].name.c_str());

		gtk_combo_box_text_append_text((GtkComboBoxText*)gtk_builder_get_object(builder, "IDC_ETHDEV"), dev.c_str());
		if (config.EthApi == adapters[i].type && strcmp(adapters[i].guid.c_str(), config.Eth) == 0)
			gtk_combo_box_set_active((GtkComboBox*)gtk_builder_get_object(builder, "IDC_ETHDEV"), idx);

		idx++;
	}

	gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DHCP"), config.InterceptDHCP);
	OnInterceptChanged((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DHCP"), nullptr);

	gtk_entry_set_text((GtkEntry*)gtk_builder_get_object(builder, "IDC_HDDFILE"), config.Hdd);

	//HDDSpin
	gtk_spin_button_set_range((GtkSpinButton*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SPIN"), HDD_MIN_GB, HDD_MAX_GB);
	gtk_spin_button_set_increments((GtkSpinButton*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SPIN"), 1, 10);
	gtk_spin_button_set_value((GtkSpinButton*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SPIN"), config.HddSize / 1024);

	//HDDSlider
	gtk_range_set_range((GtkRange*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SLIDER"), HDD_MIN_GB, HDD_MAX_GB);
	gtk_range_set_increments((GtkRange*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SLIDER"), 1, 10);

	gtk_scale_add_mark((GtkScale*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SLIDER"), HDD_MIN_GB, GTK_POS_BOTTOM, (std::to_string(HDD_MIN_GB) + " GiB").c_str());
	gtk_scale_add_mark((GtkScale*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SLIDER"), HDD_MAX_GB, GTK_POS_BOTTOM, (std::to_string(HDD_MAX_GB) + " GiB").c_str());

	for (int i = 15; i < HDD_MAX_GB; i += 5)
	{
		gtk_scale_add_mark((GtkScale*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SLIDER"), i, GTK_POS_BOTTOM, nullptr);
	}

	gtk_range_set_value((GtkRange*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SLIDER"), config.HddSize / 1024);

	//Checkboxes
	gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_ETHENABLED"),
		config.ethEnable);
	gtk_toggle_button_set_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_HDDENABLED"),
		config.hddEnable);

	initialized = 1;
}

void OnBrowse(GtkButton* button, gpointer usr_data)
{
	ghc::filesystem::path inis(EmuFolders::Settings.ToString().ToStdString());

	static const wxChar* hddFilterType = L"HDD|*.raw;*.RAW";

	wxFileDialog ctrl(nullptr, _("HDD Image File"), EmuFolders::Settings.ToString(), HDD_DEF,
		(wxString)hddFilterType + L"|" + _("All Files (*.*)") + L"|*.*", wxFD_SAVE);

	if (ctrl.ShowModal() != wxID_CANCEL)
	{
		ghc::filesystem::path hddFile(ctrl.GetPath().ToStdString());

		if (ghc::filesystem::exists(hddFile))
		{
			//Get file size
			int filesizeGb = ghc::filesystem::file_size(hddFile) / (1024 * 1024 * 1024);

			gtk_range_set_value((GtkRange*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SLIDER"), filesizeGb);
			gtk_spin_button_set_value((GtkSpinButton*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SPIN"), filesizeGb);
		}

		if (hddFile.parent_path() == inis)
			hddFile = hddFile.filename();

		gtk_entry_set_text((GtkEntry*)gtk_builder_get_object(builder, "IDC_HDDFILE"), hddFile.c_str());
	}
}

void OnSpin(GtkSpinButton* spin, gpointer usr_data)
{
	gtk_range_set_value((GtkRange*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SLIDER"),
		gtk_spin_button_get_value(spin));
}

void OnSlide(GtkRange* range, gpointer usr_data)
{
	gtk_spin_button_set_value((GtkSpinButton*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SPIN"),
		gtk_range_get_value(range));
}

void OnOk()
{
	int ethIndex = gtk_combo_box_get_active((GtkComboBox*)gtk_builder_get_object(builder, "IDC_ETHDEV"));
	if (ethIndex != -1)
	{
		strcpy(config.Eth, adapters[ethIndex].guid.c_str());
		config.EthApi = adapters[ethIndex].type;
	}

	config.InterceptDHCP = gtk_toggle_button_get_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DHCP"));
	if (config.InterceptDHCP)
	{
		config.PS2IP = IPControl_GetValue((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_IP"));

		config.AutoMask = gtk_toggle_button_get_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_SUBNET"));
		if (!config.AutoMask)
			config.Mask = IPControl_GetValue((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_SUBNET"));

		config.AutoGateway = gtk_toggle_button_get_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_GATEWAY"));
		if (!config.AutoGateway)
			config.Gateway = IPControl_GetValue((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_GATEWAY"));

		config.AutoDNS1 = gtk_toggle_button_get_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DNS1"));
		if (!config.AutoDNS1)
			config.DNS1 = IPControl_GetValue((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_DNS1"));

		config.AutoDNS2 = gtk_toggle_button_get_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_CHECK_DNS2"));
		if (!config.AutoDNS2)
			config.DNS2 = IPControl_GetValue((GtkEntry*)gtk_builder_get_object(builder, "IDC_IPADDRESS_DNS2"));
	}

	strcpy(config.Hdd, gtk_entry_get_text((GtkEntry*)gtk_builder_get_object(builder, "IDC_HDDFILE")));
	config.HddSize = gtk_spin_button_get_value((GtkSpinButton*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SPIN")) * 1024;

	config.ethEnable = gtk_toggle_button_get_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_ETHENABLED"));
	config.hddEnable = gtk_toggle_button_get_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_HDDENABLED"));

	ghc::filesystem::path hddPath(config.Hdd);

	if (hddPath.is_relative())
	{
		ghc::filesystem::path path(EmuFolders::Settings.ToString().wx_str());
		hddPath = path / hddPath;
	}

	if (config.hddEnable && !ghc::filesystem::exists(hddPath))
	{
		HddCreate hddCreator;
		hddCreator.filePath = hddPath;
		hddCreator.neededSize = config.HddSize;
		hddCreator.Start();
	}

	SaveConf();
}

void DEV9configure()
{
	ScopedCoreThreadPause paused_core;
	ConfigDEV9 oldConfig = config;

	gtk_init(NULL, NULL);
	GError* error = NULL;
	if (builder == nullptr)
	{
		builder = gtk_builder_new();
		gtk_builder_add_callback_symbols(builder,
			"OnInterceptChanged", G_CALLBACK(&OnInterceptChanged),
			"OnAutoMaskChanged", G_CALLBACK(&OnAutoMaskChanged),
			"OnAutoGatewayChanged", G_CALLBACK(&OnAutoGatewayChanged),
			"OnAutoDNS1Changed", G_CALLBACK(&OnAutoDNS1Changed),
			"OnAutoDNS2Changed", G_CALLBACK(&OnAutoDNS2Changed),
			"OnBrowse", G_CALLBACK(&OnBrowse),
			"OnSpin", G_CALLBACK(&OnSpin),
			"OnSlide", G_CALLBACK(&OnSlide), nullptr);
		if (!gtk_builder_add_from_resource(builder, "/net/pcsx2/dev9/DEV9/Linux/dev9.ui", &error))
		{
			g_warning("Could not build config ui: %s", error->message);
			g_error_free(error);
			g_object_unref(G_OBJECT(builder));
			builder = nullptr;
			return;
		}
		gtk_builder_connect_signals(builder, nullptr);
	}
	GtkDialog* dlg = GTK_DIALOG(gtk_builder_get_object(builder, "IDD_CONFDLG"));
	OnInitDialog();
	gint result = gtk_dialog_run(dlg);
	switch (result)
	{
		case -5: //IDOK
			OnOk();
			break;
		case -6: //IDCANCEL
			break;
	}
	gtk_widget_hide(GTK_WIDGET(dlg));

	ApplyConfigIfRunning(oldConfig);

	paused_core.AllowResume();
}
