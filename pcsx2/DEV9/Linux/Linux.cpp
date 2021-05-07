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

#include <stdio.h>

#include <vector>
#include <ghc/filesystem.h>

#include <wx/wx.h>
#include <wx/collpane.h>
#include <wx/filepicker.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>

#include "DEV9/Config.h"
#include "DEV9/DEV9.h"
#include "pcap.h"
#include "DEV9/pcap_io.h"
#include "DEV9/net.h"
#include "DEV9/PacketReader/IP/IP_Address.h"
#include "gui/AppCoreThread.h"

#include "DEV9/ATA/HddCreate.h"

using PacketReader::IP::IP_Address;

class DEV9Dialog : public wxDialog
{
	static void IPControl_SetValue(wxTextCtrl* ctl, IP_Address value)
	{
		ctl->SetValue(wxString::Format("%u.%u.%u.%u", value.bytes[0], value.bytes[1], value.bytes[2], value.bytes[3]));
	}

	static IP_Address IPControl_GetValue(wxTextCtrl* ctl)
	{
		IP_Address ret;
		if (4 == wxSscanf(ctl->GetValue(), "%hhu.%hhu.%hhu.%hhu", &ret.bytes[0], &ret.bytes[1], &ret.bytes[2], &ret.bytes[3]))
			return ret;
		Console.Error("Invalid IP address entered");
		return {};
	}

	struct IPInputWithAutoCheck
	{
		wxTextCtrl* m_ip;
		wxCheckBox* m_auto;
		void create(int col, wxWindow* window, wxGridBagSizer* sizer, const wxString& label)
		{
			auto* label_box = new wxStaticText(window, wxID_ANY, label);
			m_ip = new wxTextCtrl(window, wxID_ANY);
			m_auto = new wxCheckBox(window, wxID_ANY, _("Auto"));
			sizer->Add(label_box, wxGBPosition(col, 0), wxDefaultSpan, wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
			sizer->Add(m_ip,      wxGBPosition(col, 1), wxDefaultSpan, wxEXPAND);
			sizer->Add(m_auto,    wxGBPosition(col, 2), wxDefaultSpan, wxEXPAND);
		}
		void setEnabled(bool enabled)
		{
			m_auto->Enable(enabled);
			m_ip->Enable(enabled && !m_auto->GetValue());
		}
		void load(const IP_Address& ip, bool is_auto)
		{
			IPControl_SetValue(m_ip, ip);
			m_auto->SetValue(is_auto);
		}
		void save(IP_Address& ip, int& is_auto)
		{
			ip = IPControl_GetValue(m_ip);
			is_auto = m_auto->GetValue();
		}
	};
	wxCheckBox* m_eth_enable;
	wxChoice* m_eth_adapter;
	std::vector<AdapterEntry> m_adapter_list;
	wxCheckBox* m_intercept_dhcp;
	wxTextCtrl* m_ps2_address;
	IPInputWithAutoCheck m_subnet_mask;
	IPInputWithAutoCheck m_gateway_address;
	IPInputWithAutoCheck m_dns1_address;
	IPInputWithAutoCheck m_dns2_address;

	wxCheckBox* m_hdd_enable;
	wxFilePickerCtrl* m_hdd_file;
	wxSpinCtrl* m_hdd_size_spin;
	wxSlider* m_hdd_size_slider;

public:
	DEV9Dialog()
		: wxDialog(nullptr, wxID_ANY, _("Network and HDD Settings"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
	{
		auto* padding = new wxBoxSizer(wxVERTICAL);
		auto* top_box = new wxBoxSizer(wxVERTICAL);

		// Ethernet section
		auto* eth_section = new wxCollapsiblePane(this, wxID_ANY, _("Ethernet"));
		auto* eth_pane = eth_section->GetPane();
		auto* eth_sizer = new wxBoxSizer(wxVERTICAL);

		m_eth_enable = new wxCheckBox(eth_pane, wxID_ANY, _("Enabled"));
		eth_sizer->Add(m_eth_enable);
		eth_sizer->AddSpacer(5);

		auto* eth_adapter_box = new wxGridBagSizer(5, 5);
		auto* eth_adapter_label = new wxStaticText(eth_pane, wxID_ANY, _("Ethernet Device:"));
		m_adapter_list = PCAPAdapter::GetAdapters();
		wxArrayString adapter_name_list;
		adapter_name_list.Add("");
		for (const AdapterEntry& adapter : m_adapter_list)
			adapter_name_list.Add(wxString::Format(_("%s (%s)"), adapter.name, NetApiToWxString(adapter.type)));
		m_eth_adapter = new wxChoice(eth_pane, wxID_ANY, wxDefaultPosition, wxDefaultSize, adapter_name_list);
		auto* intercept_dhcp_label = new wxStaticText(eth_pane, wxID_ANY, _("Intercept DHCP:"));
		m_intercept_dhcp = new wxCheckBox(eth_pane, wxID_ANY, _("Enabled"));
		auto* ps2_addr_label = new wxStaticText(eth_pane, wxID_ANY, _("PS2 Address:"));
		m_ps2_address = new wxTextCtrl(eth_pane, wxID_ANY);
		m_ps2_address->SetMinSize(wxSize(150, -1));

		eth_adapter_box->Add(eth_adapter_label,    wxGBPosition(0, 0), wxDefaultSpan,  wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
		eth_adapter_box->Add(intercept_dhcp_label, wxGBPosition(1, 0), wxDefaultSpan,  wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
		eth_adapter_box->Add(ps2_addr_label,       wxGBPosition(2, 0), wxDefaultSpan,  wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
		eth_adapter_box->Add(m_eth_adapter,        wxGBPosition(0, 1), wxGBSpan(1, 2), wxEXPAND);
		eth_adapter_box->Add(m_intercept_dhcp,     wxGBPosition(1, 1), wxDefaultSpan,  wxEXPAND);
		eth_adapter_box->Add(m_ps2_address,        wxGBPosition(2, 1), wxDefaultSpan,  wxEXPAND);

		m_subnet_mask    .create(3, eth_pane, eth_adapter_box, _("Subnet Mask:"));
		m_gateway_address.create(4, eth_pane, eth_adapter_box, _("Gateway Address:"));
		m_dns1_address   .create(5, eth_pane, eth_adapter_box, _("DNS1 Address:"));
		m_dns2_address   .create(6, eth_pane, eth_adapter_box, _("DNS2 Address:"));

		eth_adapter_box->AddGrowableCol(1);
		eth_sizer->Add(eth_adapter_box, wxSizerFlags().Expand());
		eth_pane->SetSizer(eth_sizer);

		// HDD section
		auto* hdd_section = new wxCollapsiblePane(this, wxID_ANY, _("Hard Disk Drive"));
		auto* hdd_pane = hdd_section->GetPane();
		auto* hdd_sizer = new wxBoxSizer(wxVERTICAL);

		m_hdd_enable = new wxCheckBox(hdd_pane, wxID_ANY, _("Enabled"));
		hdd_sizer->Add(m_hdd_enable);
		hdd_sizer->AddSpacer(5);

		auto* hdd_grid = new wxFlexGridSizer(2, 0, 5);
		hdd_grid->AddGrowableCol(1);
		auto* hdd_file_label = new wxStaticText(hdd_pane, wxID_ANY, _("HDD File:"));
		m_hdd_file = new wxFilePickerCtrl(hdd_pane, wxID_ANY, wxEmptyString, _("HDD image file"), "HDD|*.raw", wxDefaultPosition, wxDefaultSize, wxFLP_SAVE | wxFLP_USE_TEXTCTRL);
		hdd_grid->Add(hdd_file_label, wxSizerFlags().Centre().Right());
		hdd_grid->Add(m_hdd_file,     wxSizerFlags().Expand());
		auto* hdd_size_label = new wxStaticText(hdd_pane, wxID_ANY, _("HDD Size (GiB):"));
		m_hdd_size_spin = new wxSpinCtrl(hdd_pane, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, HDD_MIN_GB, HDD_MAX_GB, HDD_MIN_GB);
		hdd_grid->Add(hdd_size_label,  wxSizerFlags().Centre().Right());
		hdd_grid->Add(m_hdd_size_spin, wxSizerFlags().Expand());
		m_hdd_size_slider = new wxSlider(hdd_pane, wxID_ANY, HDD_MIN_GB, HDD_MIN_GB, HDD_MAX_GB, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_VALUE_LABEL | wxSL_MIN_MAX_LABELS | wxSL_AUTOTICKS);
		m_hdd_size_slider->SetPageSize(10);
		for (int i = 15; i < HDD_MAX_GB; i += 5)
			m_hdd_size_slider->SetTick(i);
		hdd_grid->AddSpacer(0);
		hdd_grid->Add(m_hdd_size_slider, wxSizerFlags().Expand());

		hdd_sizer->Add(hdd_grid, wxSizerFlags().Expand());
		hdd_pane->SetSizer(hdd_sizer);

		top_box->Add(eth_section, wxSizerFlags().Expand());
		top_box->Add(hdd_section, wxSizerFlags().Expand());
		top_box->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Right());

		padding->Add(top_box, wxSizerFlags().Expand().Border(wxALL, 5));
		SetSizerAndFit(padding);

		Bind(wxEVT_CHECKBOX, &DEV9Dialog::OnCheck, this);
		Bind(wxEVT_SLIDER,   &DEV9Dialog::OnSlide, this);
		Bind(wxEVT_SPINCTRL, &DEV9Dialog::OnSpin,  this);
	}

	void Load(const ConfigDEV9& config)
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
		m_intercept_dhcp->SetValue(config.InterceptDHCP);
		IPControl_SetValue(m_ps2_address, config.PS2IP);
		m_subnet_mask    .load(config.Mask,    config.AutoMask);
		m_gateway_address.load(config.Gateway, config.AutoGateway);
		m_dns1_address   .load(config.DNS1,    config.AutoDNS1);
		m_dns2_address   .load(config.DNS2,    config.AutoDNS2);

		m_hdd_enable->SetValue(config.hddEnable);
		m_hdd_file->SetInitialDirectory(config.Hdd);
		m_hdd_file->SetPath(config.Hdd);
		m_hdd_size_spin->SetValue(config.HddSize / 1024);
		m_hdd_size_slider->SetValue(config.HddSize / 1024);

		UpdateEnable();
	}

	void Save(ConfigDEV9& config)
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
		config.InterceptDHCP = m_intercept_dhcp->GetValue();
		config.PS2IP = IPControl_GetValue(m_ps2_address);
		m_subnet_mask    .save(config.Mask,    config.AutoMask);
		m_gateway_address.save(config.Gateway, config.AutoGateway);
		m_dns1_address   .save(config.DNS1,    config.AutoDNS1);
		m_dns2_address   .save(config.DNS2,    config.AutoDNS2);

		config.hddEnable = m_hdd_enable->GetValue();
		wxStrncpy(config.Hdd, m_hdd_file->GetPath(), ArraySize(config.Hdd) - 1);
		config.HddSize = m_hdd_size_spin->GetValue() * 1024;
	}

	void UpdateEnable()
	{
		bool eth_enable = m_eth_enable->GetValue();
		bool hdd_enable = m_hdd_enable->GetValue();
		bool dhcp_enable = eth_enable && m_intercept_dhcp->GetValue();

		m_eth_adapter->Enable(eth_enable);
		m_intercept_dhcp->Enable(eth_enable);
		m_ps2_address->Enable(dhcp_enable);
		m_subnet_mask.setEnabled(dhcp_enable);
		m_gateway_address.setEnabled(dhcp_enable);
		m_dns1_address.setEnabled(dhcp_enable);
		m_dns2_address.setEnabled(dhcp_enable);
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
		ConfigDEV9 oldConfig = config;
		dialog.Save(config);

		ghc::filesystem::path hddPath(config.Hdd);

		if (hddPath.is_relative())
		{
			//GHC uses UTF8 on all platforms
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

		ApplyConfigIfRunning(oldConfig);
	}

	paused_core.AllowResume();
}
