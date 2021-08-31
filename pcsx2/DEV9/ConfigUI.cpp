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

#include "Config.h"
#include "DEV9.h"
#include "pcap_io.h"
#include "net.h"
#include "PacketReader/IP/IP_Address.h"
#include "gui/AppCoreThread.h"
#ifdef _WIN32
	#include "Win32/tap.h"
#endif

#include "ATA/HddCreate.h"

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
	wxChoice* m_eth_adapter_api;
	wxChoice* m_eth_adapter;
	std::vector<NetApi> m_api_list;
	std::vector<std::vector<AdapterEntry>> m_adapter_list;
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

	void addAdapter(const AdapterEntry& adapter)
	{
		if (std::find(m_api_list.begin(), m_api_list.end(), adapter.type) == m_api_list.end())
			m_api_list.push_back(adapter.type);
		u32 idx = static_cast<u32>(adapter.type);
		if (m_adapter_list.size() <= idx)
			m_adapter_list.resize(idx + 1);
		m_adapter_list[idx].push_back(adapter);
	}

public:
	DEV9Dialog()
		: wxDialog(nullptr, wxID_ANY, _("Network and HDD Settings"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER)
	{
		auto* padding = new wxBoxSizer(wxVERTICAL);
		auto* top_box = new wxBoxSizer(wxVERTICAL);

		int space = wxSizerFlags().Border().GetBorderInPixels();
		int two_space = wxSizerFlags().DoubleBorder().GetBorderInPixels();

		// Ethernet section
		auto* eth_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Ethernet"));

		m_eth_enable = new wxCheckBox(this, wxID_ANY, _("Enabled"));
		eth_sizer->Add(m_eth_enable);
		eth_sizer->AddSpacer(space);

		auto* eth_adapter_box = new wxGridBagSizer(space, space);
		for (const AdapterEntry& adapter : PCAPAdapter::GetAdapters())
			addAdapter(adapter);
#ifdef _WIN32
		for (const AdapterEntry& adapter : TAPAdapter::GetAdapters())
			addAdapter(adapter);
#endif
		std::sort(m_api_list.begin(), m_api_list.end());
		for (auto& list : m_adapter_list)
			std::sort(list.begin(), list.end(), [](const AdapterEntry& a, AdapterEntry& b){ return a.name < b.name; });
		wxArrayString adapter_api_name_list;
		adapter_api_name_list.Add("");
		for (const NetApi& type : m_api_list)
			adapter_api_name_list.Add(NetApiToWxString(type));

		auto* eth_adapter_api_label = new wxStaticText(this, wxID_ANY, _("Ethernet Device Type:"));
		auto* eth_adapter_label     = new wxStaticText(this, wxID_ANY, _("Ethernet Device:"));
		auto* intercept_dhcp_label  = new wxStaticText(this, wxID_ANY, _("Intercept DHCP:"));
		auto* ps2_addr_label        = new wxStaticText(this, wxID_ANY, _("PS2 Address:"));
		m_eth_adapter_api = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, adapter_api_name_list);
		m_eth_adapter     = new wxChoice(this, wxID_ANY);
		m_intercept_dhcp  = new wxCheckBox(this, wxID_ANY, _("Enabled"));
		m_ps2_address     = new wxTextCtrl(this, wxID_ANY);
		m_ps2_address->SetMinSize(wxSize(150, -1));

		eth_adapter_box->Add(eth_adapter_api_label, wxGBPosition(0, 0), wxDefaultSpan,  wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
		eth_adapter_box->Add(eth_adapter_label,     wxGBPosition(1, 0), wxDefaultSpan,  wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
		eth_adapter_box->Add(intercept_dhcp_label,  wxGBPosition(2, 0), wxDefaultSpan,  wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
		eth_adapter_box->Add(ps2_addr_label,        wxGBPosition(3, 0), wxDefaultSpan,  wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
		eth_adapter_box->Add(m_eth_adapter_api,     wxGBPosition(0, 1), wxGBSpan(1, 2), wxEXPAND);
		eth_adapter_box->Add(m_eth_adapter,         wxGBPosition(1, 1), wxGBSpan(1, 2), wxEXPAND);
		eth_adapter_box->Add(m_intercept_dhcp,      wxGBPosition(2, 1), wxDefaultSpan,  wxEXPAND);
		eth_adapter_box->Add(m_ps2_address,         wxGBPosition(3, 1), wxDefaultSpan,  wxEXPAND);

		m_subnet_mask    .create(4, this, eth_adapter_box, _("Subnet Mask:"));
		m_gateway_address.create(5, this, eth_adapter_box, _("Gateway Address:"));
		m_dns1_address   .create(6, this, eth_adapter_box, _("DNS1 Address:"));
		m_dns2_address   .create(7, this, eth_adapter_box, _("DNS2 Address:"));

		eth_adapter_box->AddGrowableCol(2);
		eth_sizer->Add(eth_adapter_box, wxSizerFlags().Expand());

		// HDD section
		auto* hdd_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Hard Disk Drive"));

		m_hdd_enable = new wxCheckBox(this, wxID_ANY, _("Enabled"));

		auto* hdd_file_label = new wxStaticText(this, wxID_ANY, _("HDD File"));
		m_hdd_file = new wxFilePickerCtrl(this, wxID_ANY, wxEmptyString, _("HDD image file"), "HDD|*.raw", wxDefaultPosition, wxDefaultSize, wxFLP_SAVE | wxFLP_USE_TEXTCTRL);
		auto* hdd_size_label = new wxStaticText(this, wxID_ANY, _("HDD Size (GiB)"));
		auto* hdd_size_box = new wxBoxSizer(wxHORIZONTAL);
		m_hdd_size_slider = new wxSlider(this, wxID_ANY, HDD_MIN_GB, HDD_MIN_GB, HDD_MAX_GB, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_AUTOTICKS);
		m_hdd_size_slider->SetPageSize(10);
		m_hdd_size_slider->SetTickFreq(10);
		m_hdd_size_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, HDD_MIN_GB, HDD_MAX_GB, HDD_MIN_GB);
		m_hdd_size_spin->SetSizeHints(m_hdd_size_spin->GetSizeFromTextSize(30));
		hdd_size_box->Add(m_hdd_size_slider, wxSizerFlags(1).Centre());
		hdd_size_box->AddSpacer(space);
		hdd_size_box->Add(m_hdd_size_spin,   wxSizerFlags(0).Centre());

		hdd_sizer->Add(m_hdd_enable);
		hdd_sizer->AddSpacer(two_space);
		hdd_sizer->Add(hdd_file_label, wxSizerFlags().Left());
		hdd_sizer->AddSpacer(space);
		hdd_sizer->Add(m_hdd_file,     wxSizerFlags().Expand());
		hdd_sizer->AddSpacer(two_space);
		hdd_sizer->Add(hdd_size_label, wxSizerFlags().Left());
		hdd_sizer->AddSpacer(space);
		hdd_sizer->Add(hdd_size_box,   wxSizerFlags().Expand());

		top_box->Add(eth_sizer, wxSizerFlags().Expand());
		top_box->AddSpacer(space);
		top_box->Add(hdd_sizer, wxSizerFlags().Expand());
		top_box->AddSpacer(space);
		top_box->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Right());

		padding->Add(top_box, wxSizerFlags().Expand().Border());
		SetSizerAndFit(padding);
		SetMaxSize(wxSize(wxDefaultCoord, GetMinSize().y));

		Bind(wxEVT_CHECKBOX, &DEV9Dialog::OnCheck,  this);
		Bind(wxEVT_SLIDER,   &DEV9Dialog::OnSlide,  this);
		Bind(wxEVT_SPINCTRL, &DEV9Dialog::OnSpin,   this);
		Bind(wxEVT_CHOICE,   &DEV9Dialog::OnChoice, this);
		Bind(wxEVT_BUTTON,   &DEV9Dialog::OnOK,     this, wxID_OK);
	}

	void Load(const ConfigDEV9& config)
	{
		m_eth_enable->SetValue(config.ethEnable);
		m_eth_adapter_api->SetSelection(0);
		for (size_t i = 0; i < m_api_list.size(); i++)
		{
			if (config.EthApi == m_api_list[i])
				m_eth_adapter_api->SetSelection(i + 1);
		}
		UpdateAdapters();
		m_eth_adapter->SetSelection(0);
		if (static_cast<u32>(config.EthApi) < m_adapter_list.size())
		{
			const auto& list = m_adapter_list[static_cast<u32>(config.EthApi)];
			for (size_t i = 0; i < list.size(); i++)
			{
				if (list[i].guid == config.Eth)
				{
					m_eth_adapter->SetSelection(i + 1);
					break;
				}
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
		int api = m_eth_adapter_api->GetSelection();
		int eth = m_eth_adapter->GetSelection();
		if (api && eth)
		{
			const AdapterEntry& adapter = m_adapter_list[static_cast<u32>(m_api_list[api - 1])][eth - 1];
			wxStrncpy(config.Eth, adapter.guid, std::size(config.Eth) - 1);
			config.EthApi = adapter.type;
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
		wxStrncpy(config.Hdd, m_hdd_file->GetPath(), std::size(config.Hdd) - 1);
		config.HddSize = m_hdd_size_spin->GetValue() * 1024;
	}

	void UpdateEnable()
	{
		bool eth_enable = m_eth_enable->GetValue();
		bool hdd_enable = m_hdd_enable->GetValue();
		bool dhcp_enable = eth_enable && m_intercept_dhcp->GetValue();

		m_eth_adapter_api->Enable(eth_enable);
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

	void UpdateAdapters()
	{
		int api = m_eth_adapter_api->GetSelection();
		int selection = 0;
		wxArrayString options;
		options.Add("");
		if (api)
		{
			const auto& list = m_adapter_list[static_cast<u32>(m_api_list[api - 1])];
			wxString current;
			if (m_eth_adapter->GetCount())
				current = m_eth_adapter->GetString(m_eth_adapter->GetSelection());
			if (current.empty())
				current = config.Eth;
			for (size_t i = 0; i < list.size(); i++)
			{
				options.Add(list[i].name);
				if (list[i].name == current)
					selection = i + 1;
			}
		}
		m_eth_adapter->Set(options);
		m_eth_adapter->SetSelection(selection);
		// Update minimum sizes for the possibly increased dropdown size
		// Nothing else seems to update the minimum size of the window
		SetSizerAndFit(GetSizer(), false);
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

	void OnChoice(wxCommandEvent& ev)
	{
		if (ev.GetEventObject() == m_eth_adapter_api)
			UpdateAdapters();
	}

	void OnOK(wxCommandEvent& ev)
	{
		const wxChar* msg = nullptr;

		if (m_eth_enable->GetValue() && !m_eth_adapter->GetSelection())
			msg = _("Please select an ethernet adapter or disable ethernet");
		if (m_hdd_enable->GetValue() && m_hdd_file->GetPath().empty())
			msg = _("Please specify a HDD file or disable the hard drive");

		if (msg)
			wxMessageDialog(this, msg).ShowModal();
		else
			ev.Skip();
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
