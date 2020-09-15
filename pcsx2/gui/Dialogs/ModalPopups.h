/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#pragma once

#include "App.h"
#include "ConfigurationDialog.h"
#include "Panels/ConfigurationPanels.h"

#include <wx/wizard.h>

class FirstTimeWizard : public wxWizard
{
	typedef wxWizard _parent;

protected:
	wxWizardPageSimple&		m_page_intro;
	wxWizardPageSimple&		m_page_plugins;
	wxWizardPageSimple&		m_page_bios;

	wxPanelWithHelpers&				m_panel_Intro;
	Panels::PluginSelectorPanel&	m_panel_PluginSel;
	Panels::BiosSelectorPanel&		m_panel_BiosSel;

public:
	FirstTimeWizard( wxWindow* parent );
	virtual ~FirstTimeWizard() = default;

	wxWizardPage *GetFirstPage() const { return &m_page_intro; }

	void ForceEnumPlugins()
	{
		m_panel_PluginSel.OnShown();
	}
	
	int ShowModal();

protected:
	virtual void OnPageChanging( wxWizardEvent& evt );
	virtual void OnPageChanged( wxWizardEvent& evt );
	virtual void OnDoubleClicked( wxCommandEvent& evt );

	void OnRestartWizard( wxCommandEvent& evt );
};


namespace Dialogs
{
	class AboutBoxDialog: public wxDialogWithHelpers
	{
	public:
		AboutBoxDialog( wxWindow* parent=NULL );
		virtual ~AboutBoxDialog() = default;

		static wxString GetNameStatic() { return L"AboutBox"; }
		wxString GetDialogName() const { return GetNameStatic(); }
	};


	class PickUserModeDialog : public BaseApplicableDialog
	{
	protected:
		Panels::DocsFolderPickerPanel* m_panel_usersel;
		Panels::LanguageSelectionPanel* m_panel_langsel;

	public:
		PickUserModeDialog( wxWindow* parent );
		virtual ~PickUserModeDialog() = default;

	protected:
		void OnOk_Click( wxCommandEvent& evt );
	};


	class ImportSettingsDialog : public wxDialogWithHelpers
	{
	public:
		ImportSettingsDialog( wxWindow* parent );
		virtual ~ImportSettingsDialog() = default;

	protected:
		void OnImport_Click( wxCommandEvent& evt );
		void OnOverwrite_Click( wxCommandEvent& evt );
	};

	class AssertionDialog : public wxDialogWithHelpers
	{
	public:
		AssertionDialog( const wxString& text, const wxString& stacktrace );
		virtual ~AssertionDialog() = default;
	};
}

wxWindowID pxIssueConfirmation( wxDialogWithHelpers& confirmDlg, const MsgButtons& buttons );
wxWindowID pxIssueConfirmation( wxDialogWithHelpers& confirmDlg, const MsgButtons& buttons, const wxString& disablerKey );
