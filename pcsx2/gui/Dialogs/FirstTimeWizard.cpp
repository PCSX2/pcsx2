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
#include "System.h"
#include "gui/MSWstuff.h"

#include "ModalPopups.h"
#include "gui/Panels/ConfigurationPanels.h"
#include <wx/file.h>
#include <wx/filepicker.h>
#include <wx/hyperlink.h>

using namespace Panels;
using namespace pxSizerFlags;

wxIMPLEMENT_DYNAMIC_CLASS(ApplicableWizardPage, wxWizardPageSimple);

ApplicableWizardPage::ApplicableWizardPage( wxWizard* parent, wxWizardPage* prev, wxWizardPage* next, const wxBitmap& bitmap )
	: wxWizardPageSimple( parent, prev, next, bitmap )
{
}

// This is a hack feature substitute for prioritized apply events.  This callback is issued prior
// to the apply chain being run, allowing a panel to do some pre-apply prepwork.  PAnels implementing
// this function should not modify the g_conf state.
bool ApplicableWizardPage::PrepForApply()
{
	return true;
}


// ----------------------------------------------------------------------------
Panels::SettingsDirPickerPanel::SettingsDirPickerPanel( wxWindow* parent )
	: DirPickerPanel( parent, FolderId_Settings, _("Settings"), AddAppName(_("Select a folder for %s settings")) )
{
	pxSetToolTip( this, pxEt( L"This is the folder where PCSX2 saves your settings."
	) );

	SetStaticDesc( pxE( L"You may optionally specify a location for your PCSX2 settings here.  If the location contains existing PCSX2 settings, you will be given the option to import or overwrite them."
	) );
}

namespace Panels
{

	class FirstTimeIntroPanel : public wxPanelWithHelpers
	{
	public:
		FirstTimeIntroPanel( wxWindow* parent );
		virtual ~FirstTimeIntroPanel() = default;
	};
}

Panels::FirstTimeIntroPanel::FirstTimeIntroPanel( wxWindow* parent )
	: wxPanelWithHelpers( parent, wxVERTICAL )
{
	SetMinWidth( MSW_GetDPIScale() * 600 );

	FastFormatUnicode configFile, faqFile;
	configFile.Write(L"file:///%s/Configuration_Guide.pdf", WX_STR(PathDefs::GetDocs().ToString()));
	faqFile.Write(L"file:///%s/PCSX2_FAQ.pdf", WX_STR(PathDefs::GetDocs().ToString()));

	wxStaticBoxSizer& langSel	= *new wxStaticBoxSizer( wxVERTICAL, this, _("Language selector") );

	langSel += new Panels::LanguageSelectionPanel( this ) | StdCenter();
	langSel += Heading(_("Change the language only if you need to.\nThe system default should be fine for most operating systems."));
	langSel += 8;

	*this += langSel | StdExpand();
	*this += GetCharHeight() * 2;

	*this += Heading(_("Welcome to PCSX2!")).Bold() | StdExpand();
	*this += GetCharHeight();

	*this += Heading(AddAppName(
		pxE( L"This wizard will help guide you through configuring your BIOS.  It is recommended if this is your first time installing %s that you view the readme and configuration guide."
		) )
	);

	*this += GetCharHeight() * 2;

	*this	+= new wxHyperlinkCtrl( this, wxID_ANY,
		_("Configuration Guide"), configFile.c_str()
	) | pxCenter.Border( wxALL, 5 );
		
	*this	+= new wxHyperlinkCtrl( this, wxID_ANY,
		_("Readme / FAQ"), faqFile.c_str()
	) | pxCenter.Border( wxALL, 5 );

}

// --------------------------------------------------------------------------------------
//  FirstTimeWizard  (implementations)
// --------------------------------------------------------------------------------------
FirstTimeWizard::FirstTimeWizard( wxWindow* parent )
	: wxWizard( parent, wxID_ANY, AddAppName(_("%s First Time Configuration")) )
	, m_page_intro		( *new ApplicableWizardPage( this ) )
	, m_page_bios		( *new ApplicableWizardPage( this, &m_page_intro ) )

	, m_panel_Intro		( *new FirstTimeIntroPanel( &m_page_intro ))
	, m_panel_BiosSel	( *new BiosSelectorPanel( &m_page_bios ) )
{
	// Page 3 - Bios Panel

	m_page_intro.	SetSizer( new wxBoxSizer( wxVERTICAL ) );
	m_page_bios.	SetSizer( new wxBoxSizer( wxVERTICAL ) );

	m_page_intro	+= m_panel_Intro			| StdExpand();
	m_page_bios		+= m_panel_BiosSel			| StdExpand();

	// Temporary tutorial message for the BIOS, needs proof-reading!!
	m_page_bios		+= 12;
	m_page_bios		+= new pxStaticHeading( &m_page_bios,
		pxE( L"PCSX2 requires a *legal* copy of the PS2 BIOS in order to run games.\nYou cannot use a copy obtained from a friend or the Internet.\nYou must dump the BIOS from your *own* PlayStation 2 console."
		)
	) | StdExpand();

	// Assign page indexes as client data
	m_page_intro	.SetClientData( (void*)0 );
	m_page_bios		.SetClientData( (void*)1 );

	// Build the forward chain:
	//  (backward chain is built during initialization above)
	m_page_intro	.SetNext( &m_page_bios );

	GetPageAreaSizer() += m_page_intro;

	// this doesn't descent from wxDialogWithHelpers, so we need to explicitly
	// fit and center it. :(

	Bind(wxEVT_WIZARD_PAGE_CHANGED, &FirstTimeWizard::OnPageChanged, this);
	Bind(wxEVT_WIZARD_PAGE_CHANGING, &FirstTimeWizard::OnPageChanging, this);
	Bind(wxEVT_LISTBOX_DCLICK, &FirstTimeWizard::OnDoubleClicked, this);

	Bind(wxEVT_BUTTON, &FirstTimeWizard::OnRestartWizard, this, pxID_RestartWizard);
}

void FirstTimeWizard::OnRestartWizard( wxCommandEvent& evt )
{
	EndModal( pxID_RestartWizard );
	evt.Skip();
}

static void _OpenConsole()
{
	g_Conf->ProgLogBox.Visible = true;
	wxGetApp().OpenProgramLog();
}

int FirstTimeWizard::ShowModal()
{
	if( IsDebugBuild ) wxGetApp().PostIdleMethod( _OpenConsole );
	return _parent::ShowModal();
}

void FirstTimeWizard::OnDoubleClicked( wxCommandEvent& evt )
{
	wxWindow* forwardButton = FindWindow( wxID_FORWARD );
	if( forwardButton == NULL ) return;

	wxCommandEvent nextpg( wxEVT_BUTTON, wxID_FORWARD );
	nextpg.SetEventObject( forwardButton );
	forwardButton->GetEventHandler()->ProcessEvent( nextpg );
}

void FirstTimeWizard::OnPageChanging( wxWizardEvent& evt )
{
	if( evt.GetPage() == NULL ) return;		// safety valve!

	sptr page = (sptr)evt.GetPage()->GetClientData();

	if( evt.GetDirection() )
	{
		// Moving forward:
		//   Apply settings from the current page...

		if( page >= 0 )
		{
			if( ApplicableWizardPage* page = wxDynamicCast( GetCurrentPage(), ApplicableWizardPage ) )
			{
				if( !page->PrepForApply() || !page->GetApplyState().ApplyAll() )
				{
					evt.Veto();
					return;
				}
			}
		}

		if( page == 0 )
		{
			if( wxFile::Exists(GetUiSettingsFilename()) || wxFile::Exists(GetVmSettingsFilename()) )
			{
				// Asks the user if they want to import or overwrite the existing settings.

				Dialogs::ImportSettingsDialog modal( this );
				if( modal.ShowModal() != wxID_OK )
				{
					evt.Veto();
					return;
				}
			}
		}
	}
}

void FirstTimeWizard::OnPageChanged( wxWizardEvent& evt )
{
	if( ((sptr)evt.GetPage() == (sptr)&m_page_bios) )
		m_panel_BiosSel.OnShown();
}
