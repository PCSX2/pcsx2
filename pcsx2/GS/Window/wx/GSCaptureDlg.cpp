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

#include "gui/App.h"
#include "GS.h"
#include "GSCaptureDlg.h"

#include <wx/valnum.h>

#ifdef _WIN32
// Ideally this belongs in WIL, but CAUUID is used by a *single* COM function in WinAPI.
// That's presumably why it's omitted and is unlikely to make it to upstream WIL.
static void __stdcall CloseCAUUID(_Inout_ CAUUID* cauuid) WI_NOEXCEPT
{
	::CoTaskMemFree(cauuid->pElems);
}

using unique_cauuid = wil::unique_struct<CAUUID, decltype(&::CloseCAUUID), ::CloseCAUUID>;
using unique_olestr = wil::unique_any<LPOLESTR, decltype(&::CoTaskMemFree), ::CoTaskMemFree>;

template <typename Func>
static void EnumSysDev(const GUID& clsid, Func&& f)
{
	if (auto devEnum = wil::CoCreateInstanceNoThrow<ICreateDevEnum>(CLSID_SystemDeviceEnum))
	{
		wil::com_ptr_nothrow<IEnumMoniker> classEnum;
		if (SUCCEEDED(devEnum->CreateClassEnumerator(clsid, classEnum.put(), 0)))
		{
			wil::com_ptr_nothrow<IMoniker> moniker;
			while (classEnum->Next(1, moniker.put(), nullptr) == S_OK)
			{
				std::forward<Func>(f)(moniker.get());
			}
		}
	}
}

bool GSCaptureDlg::InitSelectedCodec()
{
	if (m_codecInput->GetSelection() == wxNOT_FOUND || m_codecInput->GetSelection() == 0)
	{
		return false;
	}

	// TODO - im likely doing something wrong here, but before when i was using the reference passed in
	// it wasn't updating the value within the vector, despite it sharing the same address?
	// confused -- this works but is likely not ideal -- or atleast requires some method renaming to make sense.
	// i'd like to know why though.
	Codec* c = &(m_codecs.at(m_codecInput->GetSelection() - 1)); // we decrement because uncompressed is not an actual codec!

	if (!c->filter)
	{
		c->moniker->BindToObject(NULL, NULL, IID_PPV_ARGS(c->filter.put()));
		if (!c->filter)
		{
			return false;
		}
	}

	return true;
}
#endif

void GSCaptureDlg::UpdateConfigureButton()
{
#ifdef _WIN32
	Codec c;
	if (!InitSelectedCodec())
	{
		m_codecConfigBtn->Enable(false);
		return;
	}
	// TODO - remove for linux
	c = m_codecs.at(m_codecInput->GetSelection() - 1);

	bool shouldEnable = false;
	if (auto pSPP = c.filter.try_query<ISpecifyPropertyPages>())
	{
		unique_cauuid caGUID;
		shouldEnable = SUCCEEDED(pSPP->GetPages(&caGUID));
	}
	else if (auto pAMVfWCD = c.filter.try_query<IAMVfwCompressDialogs>())
	{
		shouldEnable = pAMVfWCD->ShowDialog(VfwCompressDialog_QueryConfig, nullptr) == S_OK;
	}
	m_codecConfigBtn->Enable(shouldEnable);
#else
	// no-op, no config button is displayed!
#endif
}

// TODO - currently it seems difficult to make proper translation strings -- for example
// 'GS - ' should not be translated
// but 'Capture Settings' should be, i assume this makes an unnecessary unique string which increases translation effort?
// However, the current macro doesn't really offer much flexibility
GSCaptureDlg::GSCaptureDlg(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("GS - Capture Settings"), wxDefaultPosition, wxDefaultSize, wxSTAY_ON_TOP | wxCLOSE_BOX | wxCAPTION)
{
	// Init from Config
	m_captureWidth = theApp.GetConfigI("CaptureWidth");
	m_captureHeight = theApp.GetConfigI("CaptureHeight");
	m_filename = convert_utf8_to_utf16(theApp.GetConfigS("CaptureFileName"));
	// TODO - save colorspace to config?

	// Sizers
	wxBoxSizer* m_sizerMain = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* m_sizerContainer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* m_sizerFileRow = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* m_sizerCodecRow = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* m_sizerOptionsGroup = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* m_sizerSizeControls = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* m_sizerConfirmBtns = new wxBoxSizer(wxHORIZONTAL);

	// Widgets
	wxIntegerValidator<unsigned int> widthValidator(&m_captureWidth, wxNUM_VAL_DEFAULT);
	wxIntegerValidator<unsigned int> heightValidator(&m_captureHeight, wxNUM_VAL_DEFAULT);

	m_filePathInput = new wxTextCtrl(this, wxID_ANY);
	m_filePathInput->SetHint(_("Enter or Browse for a File Path..."));
	m_browseBtn = new wxButton(this, wxID_ANY, _("Browse..."));
#ifdef _WIN32
	m_codecInput = new wxComboBox(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
	m_codecInput->Append("Uncompressed");
	m_codecInput->SetSelection(0);
	m_codecConfigBtn = new wxButton(this, wxID_ANY, _("Config..."));
#endif
	wxStaticText* m_sizeLabel = new wxStaticText(this, wxID_ANY, _("Size:"));
	m_widthInput = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, widthValidator);
	m_heightInput = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, heightValidator);
	wxStaticText* m_colorSpaceLabel = new wxStaticText(this, wxID_ANY, _("Color Space:"));
	m_colorSpaceInput = new wxComboBox(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
	for (auto& option : m_colorSpaceOptions)
	{
		m_colorSpaceInput->Append(wxString(option));
	}
	m_colorSpaceInput->SetSelection(0);
	m_cancelBtn = new wxButton(this, wxID_CANCEL, wxEmptyString);
	m_confirmBtn = new wxButton(this, wxID_OK, wxEmptyString);

	// Add Widgets to Sizers
	m_sizerMain->Add(m_sizerContainer, 1, wxALL | wxEXPAND, 5);
	m_sizerContainer->Add(m_sizerFileRow, 1, wxALL | wxEXPAND, 5);
	m_sizerFileRow->Add(m_filePathInput, 3, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	m_sizerFileRow->Add(m_browseBtn, 1, wxALIGN_CENTER_VERTICAL, 0);
#ifdef _WIN32
	m_sizerContainer->Add(m_sizerCodecRow, 1, wxALL | wxEXPAND, 5);
	m_sizerCodecRow->Add(m_codecInput, 3, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	m_sizerCodecRow->Add(m_codecConfigBtn, 1, wxALIGN_CENTER_VERTICAL, 0);
#endif
	m_sizerContainer->Add(m_sizerOptionsGroup, 1, wxALL | wxEXPAND, 5);
	m_sizerOptionsGroup->Add(m_sizerSizeControls, 3, wxALIGN_CENTER_VERTICAL, 10);
	m_sizerOptionsGroup->Add(m_sizerConfirmBtns, 1, wxALIGN_CENTER_VERTICAL, 0);
	m_sizerSizeControls->Add(m_sizeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	m_sizerSizeControls->Add(m_widthInput, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_sizerSizeControls->Add(m_heightInput, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	m_sizerSizeControls->Add(m_colorSpaceLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_sizerSizeControls->Add(m_colorSpaceInput, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_sizerConfirmBtns->Add(m_cancelBtn, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_sizerConfirmBtns->Add(m_confirmBtn, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);

	// Bindings
	m_browseBtn->Bind(wxEVT_BUTTON, &GSCaptureDlg::BrowseForFile, this);
	m_confirmBtn->Bind(wxEVT_BUTTON, &GSCaptureDlg::ConfirmButtonClicked, this);
#ifdef _WIN32
	m_codecConfigBtn->Bind(wxEVT_BUTTON, &GSCaptureDlg::ConfigureCodec, this);
	m_codecInput->Bind(wxEVT_COMBOBOX, &GSCaptureDlg::CodecSelected, this);
#endif
	m_colorSpaceInput->Bind(wxEVT_COMBOBOX, &GSCaptureDlg::ColorSpaceSelected, this);
	m_filePathInput->Bind(wxEVT_TEXT, &GSCaptureDlg::FileEntryChanged, this);
	m_widthInput->Bind(wxEVT_TEXT, &GSCaptureDlg::CaptureWidthChanged, this);
	m_heightInput->Bind(wxEVT_TEXT, &GSCaptureDlg::CaptureHeightChanged, this);

	// Init Window
	SetSizer(m_sizerMain);
	m_sizerMain->Fit(this);
	SetEscapeId(m_cancelBtn->GetId());
	m_confirmBtn->SetDefault();
	SetIcons(wxGetApp().GetIconBundle());

	// Init Codecs - TODO linux
#ifdef _WIN32
	CoInitialize(0); // this is obviously wrong here, each thread should call this on start, and where is CoUninitalize?

	const wxString selectedCodec = wxString(convert_utf8_to_utf16(theApp.GetConfigS("CaptureVideoCodecDisplayName")));

	EnumSysDev(CLSID_VideoCompressorCategory, [&](IMoniker* moniker) {
		Codec c;

		c.moniker = moniker;

		unique_olestr str;
		if (FAILED(moniker->GetDisplayName(NULL, NULL, str.put())))
			return;

		std::wstring prefix;
		if (wcsstr(str.get(), L"@device:dmo:"))
			prefix = L"(DMO) ";
		else if (wcsstr(str.get(), L"@device:sw:"))
			prefix = L"(DS) ";
		else if (wcsstr(str.get(), L"@device:cm:"))
			prefix = L"(VfW) ";


		c.DisplayName = str.get();

		wil::com_ptr_nothrow<IPropertyBag> pPB;
		if (FAILED(moniker->BindToStorage(0, 0, IID_PPV_ARGS(pPB.put()))))
			return;

		wil::unique_variant var;
		if (FAILED(pPB->Read(L"FriendlyName", &var, nullptr)))
			return;

		c.FriendlyName = prefix + var.bstrVal;

		m_codecs.push_back(c);

		m_codecInput->Append(c.FriendlyName.c_str());
		if (c.DisplayName == selectedCodec)
		{
			m_codecInput->SetSelection(m_codecs.size() - 1);
		}
	});
#endif

	UpdateConfigureButton();

	Layout();
}

void GSCaptureDlg::UpdateConfirmationButton()
{
	m_confirmBtn->Enable(m_colorSpaceSelection != wxNOT_FOUND && m_captureWidth > 0 && m_captureHeight > 0 && m_filename != "");
}

void GSCaptureDlg::FileEntryChanged(wxCommandEvent& event)
{
	m_filename = m_filePathInput->GetValue().ToStdWstring();
}

void GSCaptureDlg::BrowseForFile(wxCommandEvent& event)
{
	wxFileDialog filePicker(this, _("GS Capture Settings - Select a File Location"), L"", L"",
		L"AVI files (*.avi)|*.avi", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	if (filePicker.ShowModal() == wxID_CANCEL)
	{
		return;
	}

	m_filename = filePicker.GetPath().ToStdWstring();
	m_filePathInput->SetValue(wxString(m_filename));
}

void GSCaptureDlg::ConfigureCodec(wxCommandEvent& event)
{
#ifdef _WIN32
	if (m_codecInput->GetSelection() == wxNOT_FOUND || m_codecInput->GetSelection() == 0)
	{
		// how'd you even get here?
		return;
	}

	Codec c = m_codecs.at(m_codecInput->GetSelection() - 1);
	if (InitSelectedCodec())
	{
		if (auto pSPP = c.filter.try_query<ISpecifyPropertyPages>())
		{
			unique_cauuid caGUID;
			if (SUCCEEDED(pSPP->GetPages(&caGUID)))
			{
				auto lpUnk = pSPP.try_query<IUnknown>();
				OleCreatePropertyFrame(m_hWnd, 0, 0, c.FriendlyName.c_str(), 1, lpUnk.addressof(), caGUID.cElems, caGUID.pElems, 0, 0, NULL);
			}
		}
		else if (auto pAMVfWCD = c.filter.try_query<IAMVfwCompressDialogs>())
		{
			if (pAMVfWCD->ShowDialog(VfwCompressDialog_QueryConfig, NULL) == S_OK)
				pAMVfWCD->ShowDialog(VfwCompressDialog_Config, m_hWnd);
		}
	}
#endif
}

void GSCaptureDlg::CodecSelected(wxCommandEvent& event)
{
	UpdateConfigureButton();
}

void GSCaptureDlg::ColorSpaceSelected(wxCommandEvent& event)
{
	if (m_colorSpaceInput->GetSelection() == wxNOT_FOUND || m_colorSpaceInput->GetSelection() == 0)
	{
		return;
	}
	m_colorSpaceSelection = m_colorSpaceInput->GetSelection();
}

void GSCaptureDlg::CaptureWidthChanged(wxCommandEvent& event)
{
	try
	{
		m_captureWidth = std::stoi(m_widthInput->GetValue().ToStdString());
	}
	catch (...)
	{
		m_widthInput->Clear();
	}
}

void GSCaptureDlg::CaptureHeightChanged(wxCommandEvent& event)
{
	try
	{
		m_captureHeight = std::stoi(m_heightInput->GetValue().ToStdString());
	}
	catch (...)
	{
		m_heightInput->Clear();
	}
}

void GSCaptureDlg::ConfirmButtonClicked(wxCommandEvent& event)
{
	theApp.SetConfig("CaptureWidth", m_captureWidth);
	theApp.SetConfig("CaptureHeight", m_captureHeight);
	theApp.SetConfig("CaptureFileName", convert_utf16_to_utf8(m_filename).c_str());

#ifdef _WIN32
	if (InitSelectedCodec())
	{
		Codec c = m_codecs.at(m_codecInput->GetSelection() - 1);
		m_enc = c.filter;
		theApp.SetConfig("CaptureVideoCodecDisplayName", convert_utf16_to_utf8(c.DisplayName).c_str());
	}
	else
	{
		theApp.SetConfig("CaptureVideoCodecDisplayName", "Uncompressed");
	}
#endif

	if (IsModal())
	{
		EndModal(wxID_OK);
	}
	else
	{
		SetReturnCode(wxID_OK);
		this->Show(false);
	}
}
