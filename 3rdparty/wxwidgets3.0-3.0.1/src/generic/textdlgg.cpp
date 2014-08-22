/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/textdlgg.cpp
// Purpose:     wxTextEntryDialog
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TEXTDLG

#include "wx/generic/textdlgg.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/dialog.h"
    #include "wx/button.h"
    #include "wx/stattext.h"
    #include "wx/textctrl.h"
    #include "wx/intl.h"
    #include "wx/sizer.h"
#endif

#if wxUSE_STATLINE
    #include "wx/statline.h"
#endif

const char wxGetTextFromUserPromptStr[] = "Input Text";
const char wxGetPasswordFromUserPromptStr[] = "Enter Password";

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

static const int wxID_TEXT = 3000;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxTextEntryDialog
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxTextEntryDialog, wxDialog)
    EVT_BUTTON(wxID_OK, wxTextEntryDialog::OnOK)
END_EVENT_TABLE()

IMPLEMENT_CLASS(wxTextEntryDialog, wxDialog)

bool wxTextEntryDialog::Create(wxWindow *parent,
                                     const wxString& message,
                                     const wxString& caption,
                                     const wxString& value,
                                     long style,
                                     const wxPoint& pos)
{
    if ( !wxDialog::Create(GetParentForModalDialog(parent, style),
                           wxID_ANY, caption,
                           pos, wxDefaultSize,
                           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) )
    {
        return false;
    }

    m_dialogStyle = style;
    m_value = value;

    wxBeginBusyCursor();

    wxBoxSizer *topsizer = new wxBoxSizer( wxVERTICAL );

    wxSizerFlags flagsBorder2;
    flagsBorder2.DoubleBorder();

#if wxUSE_STATTEXT
    // 1) text message
    topsizer->Add(CreateTextSizer(message), flagsBorder2);
#endif

    // 2) text ctrl
    m_textctrl = new wxTextCtrl(this, wxID_TEXT, value,
                                wxDefaultPosition, wxSize(300, wxDefaultCoord),
                                style & ~wxTextEntryDialogStyle);

    topsizer->Add(m_textctrl,
                  wxSizerFlags(style & wxTE_MULTILINE ? 1 : 0).
                    Expand().
                    TripleBorder(wxLEFT | wxRIGHT));

    // 3) buttons if any
    wxSizer *buttonSizer = CreateSeparatedButtonSizer(style & (wxOK | wxCANCEL));
    if ( buttonSizer )
    {
        topsizer->Add(buttonSizer, wxSizerFlags(flagsBorder2).Expand());
    }

    SetAutoLayout( true );
    SetSizer( topsizer );

    topsizer->SetSizeHints( this );
    topsizer->Fit( this );

    if ( style & wxCENTRE )
        Centre( wxBOTH );

    m_textctrl->SelectAll();
    m_textctrl->SetFocus();

    wxEndBusyCursor();

    return true;
}

bool wxTextEntryDialog::TransferDataToWindow()
{
    m_textctrl->SetValue(m_value);

    return wxDialog::TransferDataToWindow();
}

bool wxTextEntryDialog::TransferDataFromWindow()
{
    m_value = m_textctrl->GetValue();

    return wxDialog::TransferDataFromWindow();
}

void wxTextEntryDialog::OnOK(wxCommandEvent& WXUNUSED(event) )
{
    if ( Validate() && TransferDataFromWindow() )
    {
        EndModal( wxID_OK );
    }
}

void wxTextEntryDialog::SetMaxLength(unsigned long len)
{
    m_textctrl->SetMaxLength(len);
}

void wxTextEntryDialog::SetValue(const wxString& val)
{
    m_value = val;

    m_textctrl->SetValue(val);
}

#if wxUSE_VALIDATORS

#if WXWIN_COMPATIBILITY_2_8
void wxTextEntryDialog::SetTextValidator( long style )
{
    SetTextValidator((wxTextValidatorStyle)style);
}
#endif

void wxTextEntryDialog::SetTextValidator( wxTextValidatorStyle style )
{
    SetTextValidator(wxTextValidator(style));
}

void wxTextEntryDialog::SetTextValidator( const wxTextValidator& validator )
{
    m_textctrl->SetValidator( validator );
}

#endif // wxUSE_VALIDATORS

// ----------------------------------------------------------------------------
// wxPasswordEntryDialog
// ----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxPasswordEntryDialog, wxTextEntryDialog)

wxPasswordEntryDialog::wxPasswordEntryDialog(wxWindow *parent,
                                     const wxString& message,
                                     const wxString& caption,
                                     const wxString& value,
                                     long style,
                                     const wxPoint& pos)
                 : wxTextEntryDialog(parent, message, caption, value,
                                     style | wxTE_PASSWORD, pos)
{
    // Only change from wxTextEntryDialog is the password style
}

#endif // wxUSE_TEXTDLG
