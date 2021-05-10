/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include <vector>
#include <ghc/filesystem.h>

#include <wx/wx.h>
#include <wx/collpane.h>
#include <wx/filepicker.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>

#include "Config.h"
#include "DEV9.h"
#include "pcap_io.h"
#include "net.h"
#include "AppCoreThread.h"
#ifdef _WIN32
#include "Win32/tap.h"
#endif

#include "ATA/HddCreate.h"

class DEV9Dialog : public wxDialog
{
	wxCheckBox* m_eth_enable;
	wxChoice* m_eth_adapter;
	std::vector<AdapterEntry> m_adapter_list;

	wxCheckBox* m_hdd_enable;
	wxFilePickerCtrl* m_hdd_file;
	wxSpinCtrl* m_hdd_size_spin;
	wxSlider* m_hdd_size_slider;

public:
	DEV9Dialog()
		: wxDialog(nullptr, wxID_ANY, _("Network and HDD Settings"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER)
	{
		auto* padding = new wxBoxSizer(wxVERTICAL);
		auto* top_box = new wxBoxSizer(wxVERTICAL);

		// Ethernet section
		auto* eth_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Ethernet"));

		m_eth_enable = new wxCheckBox(this, wxID_ANY, _("Enabled"));
		eth_sizer->Add(m_eth_enable);
		eth_sizer->AddSpacer(5);

		auto* eth_adapter_box = new wxBoxSizer(wxHORIZONTAL);
		auto* eth_adapter_label = new wxStaticText(this, wxID_ANY, _("Ethernet Device:"));
#ifdef _WIN32
		m_adapter_list = TAPAdapter::GetAdapters();
		auto pcap_adapters = PCAPAdapter::GetAdapters();
		m_adapter_list.reserve(m_adapter_list.size() + pcap_adapters.size());
		m_adapter_list.insert(m_adapter_list.end(), pcap_adapters.begin(), pcap_adapters.end());
#else
		m_adapter_list = PCAPAdapter::GetAdapters();
#endif
		wxArrayString adapter_name_list;
		adapter_name_list.Add("");
		for (const AdapterEntry& adapter : m_adapter_list)
			adapter_name_list.Add(wxString::Format(_("%s (%s)"), adapter.name, NetApiToWxString(adapter.type)));
		m_eth_adapter = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, adapter_name_list);

		eth_adapter_box->Add(eth_adapter_label);
		eth_adapter_box->Add(m_eth_adapter);
		eth_sizer->Add(eth_adapter_box, wxSizerFlags().Expand());

		// HDD section
		auto* hdd_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Hard Disk Drive"));

		m_hdd_enable = new wxCheckBox(this, wxID_ANY, _("Enabled"));
		hdd_sizer->Add(m_hdd_enable);
		hdd_sizer->AddSpacer(5);

		auto* hdd_grid = new wxFlexGridSizer(2, 0, 5);
		hdd_grid->AddGrowableCol(1);
		auto* hdd_file_label = new wxStaticText(this, wxID_ANY, _("HDD File:"));
		m_hdd_file = new wxFilePickerCtrl(this, wxID_ANY, wxEmptyString, _("HDD image file"), "HDD|*.raw", wxDefaultPosition, wxDefaultSize, wxFLP_SAVE | wxFLP_USE_TEXTCTRL);
		hdd_grid->Add(hdd_file_label, wxSizerFlags().Centre().Right());
		hdd_grid->Add(m_hdd_file,     wxSizerFlags().Centre().Expand());
		auto* hdd_size_label = new wxStaticText(this, wxID_ANY, _("HDD Size (GiB):"));
		auto* hdd_size_box = new wxBoxSizer(wxHORIZONTAL);
		m_hdd_size_slider = new wxSlider(this, wxID_ANY, HDD_MIN_GB, HDD_MIN_GB, HDD_MAX_GB, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_MIN_MAX_LABELS | wxSL_AUTOTICKS);
		m_hdd_size_slider->SetPageSize(10);
		for (int i = 15; i < HDD_MAX_GB; i += 5)
			m_hdd_size_slider->SetTick(i);
		m_hdd_size_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, HDD_MIN_GB, HDD_MAX_GB, HDD_MIN_GB);
		m_hdd_size_spin->SetSizeHints(m_hdd_size_spin->GetSizeFromTextSize(30));
		hdd_size_box->Add(m_hdd_size_slider, wxSizerFlags(1).Centre());
		hdd_size_box->Add(m_hdd_size_spin,   wxSizerFlags(0).Centre());
		hdd_grid->Add(hdd_size_label, wxSizerFlags().Centre().Right());
		hdd_grid->Add(hdd_size_box,   wxSizerFlags().Centre().Expand());

		hdd_sizer->Add(hdd_grid, wxSizerFlags().Expand());

		top_box->Add(eth_sizer, wxSizerFlags().Expand());
		top_box->Add(hdd_sizer, wxSizerFlags().Expand());
		top_box->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Right());

		padding->Add(top_box, wxSizerFlags().Centre().Expand().Border(wxALL, 5));
		SetSizerAndFit(padding);
		SetMaxSize(wxSize(wxDefaultCoord, GetMinSize().y));

		Bind(wxEVT_CHECKBOX, &DEV9Dialog::OnCheck, this);
		Bind(wxEVT_SLIDER,   &DEV9Dialog::OnSlide, this);
		Bind(wxEVT_SPINCTRL, &DEV9Dialog::OnSpin,  this);
	}

	void Load(const Config& config)
	{
		m_eth_enable->SetValue(config.ethEnable);
		m_eth_adapter->SetSelection(0);
		for (size_t i = 0; i < m_adapter_list.size(); i++)
		{
			const AdapterEntry& adapter = m_adapter_list[i];
			if (adapter.type == config.EthApi && adapter.name == config.Eth)
			{
				m_eth_adapter->SetSelection(i + 1);
				break;
			}
		}

		m_hdd_enable->SetValue(config.hddEnable);
		m_hdd_file->SetInitialDirectory(config.Hdd);
		m_hdd_file->SetPath(config.Hdd);
		m_hdd_size_spin->SetValue(config.HddSize / 1024);
		m_hdd_size_slider->SetValue(config.HddSize / 1024);

		UpdateEnable();
	}

	void Save(Config& config)
	{
		config.ethEnable = m_eth_enable->GetValue();
		int eth = m_eth_adapter->GetSelection();
		if (eth)
		{
			wxStrncpy(config.Eth, m_adapter_list[eth - 1].name, ArraySize(config.Eth) - 1);
			config.EthApi = m_adapter_list[eth - 1].type;
		}
		else
		{
			config.Eth[0] = 0;
			config.EthApi = NetApi::Unset;
		}

		config.hddEnable = m_hdd_enable->GetValue();
		wxStrncpy(config.Hdd, m_hdd_file->GetPath(), ArraySize(config.Hdd) - 1);
		config.HddSize = m_hdd_size_spin->GetValue() * 1024;
	}

	void UpdateEnable()
	{
		bool eth_enable = m_eth_enable->GetValue();
		bool hdd_enable = m_hdd_enable->GetValue();

		m_eth_adapter->Enable(eth_enable);
		m_hdd_file->Enable(hdd_enable);
		m_hdd_size_spin->Enable(hdd_enable);
		m_hdd_size_slider->Enable(hdd_enable);
	}

	void OnCheck(wxCommandEvent&)
	{
		UpdateEnable();
	}

	void OnSlide(wxCommandEvent&)
	{
		m_hdd_size_spin->SetValue(m_hdd_size_slider->GetValue());
	}

	void OnSpin(wxCommandEvent&)
	{
		m_hdd_size_slider->SetValue(m_hdd_size_spin->GetValue());
	}
};

void DEV9configure()
{
	ScopedCoreThreadPause paused_core;

	DEV9Dialog dialog;
	LoadConf();
	dialog.Load(config);
	if (dialog.ShowModal() == wxID_OK)
	{
		Config oldConfig = config;
		dialog.Save(config);

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

		ApplyConfigIfRunning(oldConfig);
	}

	paused_core.AllowResume();
}
