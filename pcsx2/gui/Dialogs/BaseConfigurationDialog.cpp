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

#include "PrecompiledHeader.h"
#include "System.h"
#include "gui/App.h"
#include "gui/MSWstuff.h"

#include "ConfigurationDialog.h"
#include "ModalPopups.h"
#include "gui/Panels/ConfigurationPanels.h"

#include <wx/artprov.h>
#include <wx/filepicker.h>
#include <wx/listbook.h>
#include <wx/spinctrl.h>

wxDEFINE_EVENT(pxEvt_ApplySettings, wxCommandEvent);
wxDEFINE_EVENT(pxEvt_SetSettingsPage, wxCommandEvent);
wxDEFINE_EVENT(pxEvt_SomethingChanged, wxCommandEvent);

using namespace Panels;

// configure the orientation of the listbox based on the platform
// For now, they're all on the left.
static const int s_orient = wxLB_LEFT;


class ScopedOkButtonDisabler
{
protected:
	Dialogs::BaseConfigurationDialog* m_parent;

	wxWindow* m_apply;
	wxWindow* m_ok;
	wxWindow* m_cancel;

public:
	ScopedOkButtonDisabler( Dialogs::BaseConfigurationDialog* parent )
	{
		m_parent = parent;
		m_parent->AllowApplyActivation( false );

		m_apply		= m_parent->FindWindow( wxID_APPLY );
		m_ok		= m_parent->FindWindow( wxID_OK );
		m_cancel	= m_parent->FindWindow( wxID_CANCEL );

		if (m_apply)	m_apply	->Disable();
		if (m_ok)		m_ok	->Disable();
		if (m_cancel)	m_cancel->Disable();
	}

	// Use this to prevent the Apply button from being re-enabled.
	void DetachApply()
	{
		m_apply = NULL;
	}

	void DetachAll()
	{
		m_apply = m_ok = m_cancel = NULL;
	}

	virtual ~ScopedOkButtonDisabler()
	{
		if (m_apply)	m_apply	->Enable();
		if (m_ok)		m_ok	->Enable();
		if (m_cancel)	m_cancel->Enable();

		m_parent->AllowApplyActivation( true );
	}
};

// --------------------------------------------------------------------------------------
//  BaseApplicableDialog  (implementations)
// --------------------------------------------------------------------------------------
wxIMPLEMENT_DYNAMIC_CLASS(BaseApplicableDialog, wxDialogWithHelpers);

BaseApplicableDialog::BaseApplicableDialog( wxWindow* parent, const wxString& title, const pxDialogCreationFlags& cflags )
	: wxDialogWithHelpers( parent, title, cflags.MinWidth(425).Minimize() )
{
	Init();
}

BaseApplicableDialog::~BaseApplicableDialog()
{
	m_ApplyState.DoCleanup();
}

wxString BaseApplicableDialog::GetDialogName() const
{
	pxFailDev( "This class must implement GetDialogName!" );
	return L"Unnamed";
}


void BaseApplicableDialog::Init()
{
	Bind(pxEvt_ApplySettings, &BaseApplicableDialog::OnSettingsApplied, this);

	wxCommandEvent applyEvent( pxEvt_ApplySettings );
	applyEvent.SetId( GetId() );
	AddPendingEvent( applyEvent );
}

void BaseApplicableDialog::OnSettingsApplied( wxCommandEvent& evt )
{
	evt.Skip();
	if( evt.GetId() == GetId() )
		AppStatusEvent_OnSettingsApplied();
}


// --------------------------------------------------------------------------------------
//  BaseConfigurationDialog  Implementations
// --------------------------------------------------------------------------------------
Dialogs::BaseConfigurationDialog::BaseConfigurationDialog( wxWindow* parent, const wxString& title, int idealWidth )
	: _parent( parent, title )
{
	float scale = MSW_GetDPIScale();

	SetMinWidth( scale * idealWidth );
	m_listbook = NULL;
	m_allowApplyActivation = true;

	Bind(wxEVT_BUTTON, &BaseConfigurationDialog::OnOk_Click, this, wxID_OK);
	Bind(wxEVT_BUTTON, &BaseConfigurationDialog::OnCancel_Click, this, wxID_CANCEL);
	Bind(wxEVT_BUTTON, &BaseConfigurationDialog::OnApply_Click, this, wxID_APPLY);
	Bind(wxEVT_BUTTON, &BaseConfigurationDialog::OnScreenshot_Click, this, wxID_SAVE);

	Bind(pxEvt_SetSettingsPage, &BaseConfigurationDialog::OnSetSettingsPage, this);

	// ----------------------------------------------------------------------------
	// Bind a variety of standard "something probably changed" events.  If the user invokes
	// any of these, we'll automatically de-gray the Apply button for this dialog box. :)

	Bind(wxEVT_TEXT, &BaseConfigurationDialog::OnSomethingChanged, this);
	Bind(wxEVT_TEXT_ENTER, &BaseConfigurationDialog::OnSomethingChanged, this);

	Bind(wxEVT_RADIOBUTTON, &BaseConfigurationDialog::OnSomethingChanged, this);
	Bind(wxEVT_COMBOBOX, &BaseConfigurationDialog::OnSomethingChanged, this);
	Bind(wxEVT_CHECKBOX, &BaseConfigurationDialog::OnSomethingChanged, this);
	Bind(wxEVT_BUTTON, &BaseConfigurationDialog::OnSomethingChanged, this);
	Bind(wxEVT_CHOICE, &BaseConfigurationDialog::OnSomethingChanged, this);
	Bind(wxEVT_LISTBOX, &BaseConfigurationDialog::OnSomethingChanged, this);
	Bind(wxEVT_SPINCTRL, &BaseConfigurationDialog::OnSomethingChanged, this);
	Bind(wxEVT_SLIDER, &BaseConfigurationDialog::OnSomethingChanged, this);
	Bind(wxEVT_DIRPICKER_CHANGED, &BaseConfigurationDialog::OnSomethingChanged, this);

	Bind(pxEvt_SomethingChanged, &BaseConfigurationDialog::OnSomethingChanged, this);
}

void Dialogs::BaseConfigurationDialog::AddListbook( wxSizer* sizer )
{
	if( !sizer ) sizer = GetSizer();
	*sizer += m_listbook	| pxExpand.Border( wxLEFT | wxRIGHT, 2 );
}

void Dialogs::BaseConfigurationDialog::CreateListbook( wxImageList& bookicons )
{
	m_listbook = new wxListbook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, s_orient );
	m_listbook->SetImageList( &bookicons );
	m_ApplyState.StartBook( m_listbook );
}

void Dialogs::BaseConfigurationDialog::AddOkCancel( wxSizer* sizer )
{
	_parent::AddOkCancel( sizer, true );
	if( wxWindow* apply = FindWindow( wxID_APPLY ) ) apply->Disable();
	SomethingChanged_StateModified_IsChanged();

	wxBitmapButton& screenshotButton(*new wxBitmapButton(this, wxID_SAVE, wxGetApp().GetScreenshotBitmap()));
	screenshotButton.SetToolTip( _("Saves a snapshot of this settings panel to a PNG file.") );

	*m_extraButtonSizer += screenshotButton|pxMiddle;
}

void Dialogs::BaseConfigurationDialog::OnSetSettingsPage( wxCommandEvent& evt )
{
	if( !m_listbook ) return;

	size_t pages = m_labels.GetCount();

	for( size_t i=0; i<pages; ++i )
	{
		if( evt.GetString() == m_labels[i] )
		{
			m_listbook->SetSelection( i );
			break;
		}
	}
}

void Dialogs::BaseConfigurationDialog::SomethingChanged()
{
	if( wxWindow* apply = FindWindow( wxID_APPLY ) ) apply->Enable();
	SomethingChanged_StateModified_IsChanged();
}

void Dialogs::BaseConfigurationDialog::OnSomethingChanged( wxCommandEvent& evt )
{
	evt.Skip();
	if (!m_allowApplyActivation) return;
	if ((evt.GetId() != wxID_OK) && (evt.GetId() != wxID_CANCEL) && (evt.GetId() != wxID_APPLY))
		SomethingChanged();
}

void Dialogs::BaseConfigurationDialog::AllowApplyActivation( bool allow )
{
	m_allowApplyActivation = allow;
}

void Dialogs::BaseConfigurationDialog::OnOk_Click( wxCommandEvent& evt )
{
	ScopedOkButtonDisabler disabler(this);

	//same as for OnApply_Click
	Apply();

	if( m_ApplyState.ApplyAll() )
	{
		if( wxWindow* apply = FindWindow( wxID_APPLY ) ) apply->Disable();
		SomethingChanged_StateModified_IsChanged();
		if( m_listbook ) GetConfSettingsTabName() = m_labels[m_listbook->GetSelection()];
		AppSaveSettings();
		disabler.DetachAll();
		evt.Skip();
	}
}

void Dialogs::BaseConfigurationDialog::OnApply_Click( wxCommandEvent& evt )
{
	ScopedOkButtonDisabler disabler(this);

	//if current instance also holds settings that need to be applied, apply them.
	//Currently only used by SysConfigDialog, which applies the preset and derivatives (menu system).
	//Needs to come before actual panels Apply since they enable/disable themselves upon Preset state,
	//  so the preset needs to be applied first.
	Apply();

	if( m_ApplyState.ApplyAll() )
		disabler.DetachApply();

	if( m_listbook ) GetConfSettingsTabName() = m_labels[m_listbook->GetSelection()];
	AppSaveSettings();

	SomethingChanged_StateModified_IsChanged();
}

void Dialogs::BaseConfigurationDialog::OnCancel_Click( wxCommandEvent& evt )
{
	//same as for Ok/Apply: let SysConfigDialog clean-up the presets and derivatives (menu system) if needed.
	Cancel();

	evt.Skip();
	if( m_listbook ) GetConfSettingsTabName() = m_labels[m_listbook->GetSelection()];
}

void Dialogs::BaseConfigurationDialog::OnScreenshot_Click( wxCommandEvent& evt )
{
	wxBitmap memBmp;

	{
	wxWindowDC dc( this );
	wxSize dcsize( dc.GetSize() );
	wxMemoryDC memDC( memBmp = wxBitmap( dcsize.x, dcsize.y ) );
	memDC.Blit( wxPoint(), dcsize, &dc, wxPoint() );
	}

	wxString pagename( m_listbook ? (L"_" + m_listbook->GetPageText( m_listbook->GetSelection() )) : wxString() );
	wxString filenameDefault;
	filenameDefault.Printf( L"%s_%s%s.png", pxGetAppName().Lower().c_str(), GetDialogName().c_str(), pagename.c_str() );
	filenameDefault.Replace( L"/", L"-" );

	wxString filename( wxFileSelector( _("Save dialog screenshots to..."), g_Conf->Folders.Snapshots.ToString(),
		filenameDefault, L"png", wxEmptyString, wxFD_SAVE | wxFD_OVERWRITE_PROMPT, this ) );

	if( !filename.IsEmpty() )
	{
		ScopedBusyCursor busy( Cursor_ReallyBusy );
#ifdef __WXMSW__
		// HACK: This works around an actual wx3.0 bug at the cost of icon
		// quality. See http://trac.wxwidgets.org/ticket/14403 .
		wxImage image = memBmp.ConvertToImage();
		if (image.HasAlpha())
			image.ClearAlpha();
		image.SaveFile( filename, wxBITMAP_TYPE_PNG );
#else
		memBmp.SaveFile( filename, wxBITMAP_TYPE_PNG );
#endif
	}
}
