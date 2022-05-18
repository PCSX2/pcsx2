/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "ConfigurationPanels.h"

#include "common/StringUtil.h"
#include "ps2/BiosTools.h"

#include <wx/dir.h>
#include <wx/filepicker.h>
#include <wx/listbox.h>
#include <memory>

using namespace pxSizerFlags;

wxDECLARE_EVENT(pxEvt_BiosEnumerationFinished, wxCommandEvent);
wxDEFINE_EVENT(pxEvt_BiosEnumerationFinished, wxCommandEvent);

// =====================================================================================================
//  BaseSelectorPanel
// =====================================================================================================
// This base class provides event hookups and virtual functions for enumerating items in a folder.
// The most important feature of this base panel is that enumeration is done when the panel is first
// *shown*, not when it is created.  This functionality allows the panel to work either as a stand alone
// dialog, a child of a complex tabbed dialog, and as a member of a wxWizard!
//
// In addition, this panel automatically intercepts and responds to DIRPICKER messages, so that your
// panel may provide a dir picker to the user.
//
// [TODO] : wxWidgets 2.9.1 provides a class for watching directory contents changes.  When PCSX2 is
// upgraded to wx2.9/3.0, it should incorporate such functionality into this base class.  (for now
// we just provide the user with a "refresh" button).
//
Panels::BaseSelectorPanel::BaseSelectorPanel(wxWindow* parent)
	: BaseApplicableConfigPanel(parent, wxVERTICAL)
{
	Bind(wxEVT_DIRPICKER_CHANGED, &BaseSelectorPanel::OnFolderChanged, this);
	Bind(wxEVT_SHOW, &BaseSelectorPanel::OnShow, this);
}

void Panels::BaseSelectorPanel::OnShow(wxShowEvent& evt)
{
	evt.Skip();
	if (evt.IsShown())
		OnShown();
}

void Panels::BaseSelectorPanel::OnShown()
{
	if (!ValidateEnumerationStatus())
		DoRefresh();
}

bool Panels::BaseSelectorPanel::Show(bool visible)
{
	if (visible)
		OnShown();

	return BaseApplicableConfigPanel::Show(visible);
}

void Panels::BaseSelectorPanel::RefreshSelections()
{
	ValidateEnumerationStatus();
	DoRefresh();
}

void Panels::BaseSelectorPanel::OnRefreshSelections(wxCommandEvent& evt)
{
	evt.Skip();
	RefreshSelections();
}

void Panels::BaseSelectorPanel::OnFolderChanged(wxFileDirPickerEvent& evt)
{
	evt.Skip();
	OnShown();
}

// =====================================================================================================
//  BiosSelectorPanel
// =====================================================================================================
Panels::BiosSelectorPanel::BiosSelectorPanel(wxWindow* parent)
	: BaseSelectorPanel(parent)
{
	SetMinWidth(480);

	m_ComboBox = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_SINGLE | wxLB_SORT | wxLB_NEEDED_SB);
	m_FolderPicker = new DirPickerPanel(this, FolderId_Bios,
		_("BIOS Search Path:"), // static box label
		_("Select folder with PS2 BIOS roms") // dir picker popup label
	);

	m_ComboBox->SetFont(pxGetFixedFont(m_ComboBox->GetFont().GetPointSize() + 1));
	m_ComboBox->SetMinSize(wxSize(wxDefaultCoord, std::max(m_ComboBox->GetMinSize().GetHeight(), 96)));

	//if (InstallationMode != InstallMode_Portable)
	m_FolderPicker->SetStaticDesc(_("Click the Browse button to select a different folder where PCSX2 will look for PS2 BIOS roms."));

	wxButton* refreshButton = new wxButton(this, wxID_ANY, _("Refresh list"));

	*this += Label(_("Select a BIOS rom:"));
	*this += m_ComboBox | StdExpand();
	*this += refreshButton | pxBorder(wxLEFT, StdPadding);
	*this += 8;
	*this += m_FolderPicker | StdExpand();

	Bind(pxEvt_BiosEnumerationFinished, &BiosSelectorPanel::OnEnumComplete, this);

	Bind(wxEVT_BUTTON, &BiosSelectorPanel::OnRefreshSelections, this, refreshButton->GetId());
}

void Panels::BiosSelectorPanel::Apply()
{
	// User never visited this tab, so there's nothing to apply.
	if (!m_BiosList)
		return;

	int sel = m_ComboBox->GetSelection();
	if (sel == wxNOT_FOUND)
	{
		throw Exception::CannotApplySettings(this)
			.SetDiagMsg("User did not specify a valid BIOS selection.")
			.SetUserMsg("Please select a valid BIOS.  If you are unable to make a valid selection then press Cancel to close the Configuration panel.");
	}

	g_Conf->EmuOptions.BaseFilenames.Bios = StringUtil::wxStringToUTF8String(wxFileName((*m_BiosList)[(sptr)m_ComboBox->GetClientData(sel)]).GetFullName());
}

void Panels::BiosSelectorPanel::AppStatusEvent_OnSettingsApplied()
{
}

bool Panels::BiosSelectorPanel::ValidateEnumerationStatus()
{
	bool validated = true;

	// Impl Note: unique_ptr used so that resources get cleaned up if an exception
	// occurs during file enumeration.
	std::unique_ptr<wxArrayString> bioslist(new wxArrayString());

	if (m_FolderPicker->GetPath().Exists())
		wxDir::GetAllFiles(m_FolderPicker->GetPath().ToString(), bioslist.get(), L"*.*", wxDIR_FILES);

	if (!m_BiosList || (*bioslist != *m_BiosList))
		validated = false;

	m_BiosList.swap(bioslist);

	int sel = m_ComboBox->GetSelection();
	if ((sel == wxNOT_FOUND) && !(m_ComboBox->IsEmpty()))
		m_ComboBox->SetSelection(0);

	return validated;
}

void Panels::BiosSelectorPanel::EnumThread::ExecuteTaskInThread()
{
	u32 region, version;
	std::string description, zone;
	for (size_t i = 0; i < m_parent.m_BiosList->GetCount(); ++i)
	{
		if (!IsBIOS((*m_parent.m_BiosList)[i].ToUTF8().data(), version, description, region, zone))
			continue;
		Result.emplace_back(StringUtil::UTF8StringToWxString(description), i);
	}

	wxCommandEvent done(pxEvt_BiosEnumerationFinished);
	done.SetClientData(this);
	m_parent.GetEventHandler()->AddPendingEvent(done);
}

void Panels::BiosSelectorPanel::DoRefresh()
{
	m_ComboBox->Clear();
	if (!m_BiosList->size())
		return;

	m_ComboBox->Append(wxString("Enumerating BIOSes..."));
	m_ComboBox->Update();

	m_EnumeratorThread.reset(new EnumThread(*this));

	m_EnumeratorThread->Start();
}

void Panels::BiosSelectorPanel::OnEnumComplete(wxCommandEvent& evt)
{
	auto enumThread = static_cast<EnumThread*>(evt.GetClientData());
	// Sanity check, in case m_BiosList was updated by ValidateEnumerationStatus() while the EnumThread was running
	if (m_EnumeratorThread.get() != enumThread || m_BiosList->size() < enumThread->Result.size())
		return;

	const wxString currentBios(StringUtil::UTF8StringToWxString(g_Conf->EmuOptions.FullpathToBios()));
	m_ComboBox->Clear(); // Clear the "Enumerating BIOSes..."

	for (const std::pair<wxString, u32>& result : enumThread->Result)
	{
		const int sel = m_ComboBox->Append(result.first, reinterpret_cast<void*>(static_cast<uintptr_t>(result.second)));
		if (currentBios == (*m_BiosList)[result.second])
			m_ComboBox->SetSelection(sel);
	}
	// Select a bios if one isn't selected. 
	// This makes it so users don't _have_ to click on their bios,
	// possibly reducing confusion.
	if(m_ComboBox->GetSelection() == -1 && m_ComboBox->GetCount() > 0)
	{
		m_ComboBox->SetSelection(0);
	}
};

Panels::BiosSelectorPanel::EnumThread::EnumThread(BiosSelectorPanel& parent)
	: m_parent(parent){};
