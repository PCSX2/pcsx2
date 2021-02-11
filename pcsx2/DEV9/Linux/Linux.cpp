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

#include <gtk/gtk.h>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <vector>
#include "fmt/format.h"

#include <string>
#include "ghc/filesystem.h"

#include "../Config.h"
#include "../DEV9.h"
#include "pcap.h"
#include "../pcap_io.h"
#include "../net.h"
#include "AppCoreThread.h"

#include "../ATA/HddCreate.h"

static GtkBuilder* builder = nullptr;
std::vector<AdapterEntry> adapters;

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
	ghc::filesystem::path inis(GetSettingsFolder().ToString().ToStdString());

	static const wxChar* hddFilterType = L"HDD|*.raw;*.RAW";

	wxFileDialog ctrl(nullptr, _("HDD Image File"), GetSettingsFolder().ToString(), HDD_DEF,
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

	strcpy(config.Hdd, gtk_entry_get_text((GtkEntry*)gtk_builder_get_object(builder, "IDC_HDDFILE")));
	config.HddSize = gtk_spin_button_get_value((GtkSpinButton*)gtk_builder_get_object(builder, "IDC_HDDSIZE_SPIN")) * 1024;

	config.ethEnable = gtk_toggle_button_get_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_ETHENABLED"));
	config.hddEnable = gtk_toggle_button_get_active((GtkToggleButton*)gtk_builder_get_object(builder, "IDC_HDDENABLED"));

	ghc::filesystem::path hddPath(config.Hdd);

	if (hddPath.is_relative())
	{
		//GHC uses UTF8 on all platforms
		ghc::filesystem::path path(GetSettingsFolder().ToUTF8().data());
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
	Config oldConfig = config;

	gtk_init(NULL, NULL);
	GError* error = NULL;
	if (builder == nullptr)
	{
		builder = gtk_builder_new();
		gtk_builder_add_callback_symbols(builder,
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
