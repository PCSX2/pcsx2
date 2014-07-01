///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/fontpickercmn.cpp
// Purpose:     wxFontPickerCtrl class implementation
// Author:      Francesco Montorsi
// Modified by:
// Created:     15/04/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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

#if wxUSE_FONTPICKERCTRL

#include "wx/fontpicker.h"

#ifndef WX_PRECOMP
    #include "wx/textctrl.h"
#endif

#include "wx/fontenum.h"
#include "wx/tokenzr.h"

// ============================================================================
// implementation
// ============================================================================

const char wxFontPickerCtrlNameStr[] = "fontpicker";
const char wxFontPickerWidgetNameStr[] = "fontpickerwidget";

wxDEFINE_EVENT(wxEVT_FONTPICKER_CHANGED, wxFontPickerEvent);
IMPLEMENT_DYNAMIC_CLASS(wxFontPickerCtrl, wxPickerBase)
IMPLEMENT_DYNAMIC_CLASS(wxFontPickerEvent, wxCommandEvent)

// ----------------------------------------------------------------------------
// wxFontPickerCtrl
// ----------------------------------------------------------------------------

#define M_PICKER     ((wxFontPickerWidget*)m_picker)

bool wxFontPickerCtrl::Create( wxWindow *parent, wxWindowID id,
                        const wxFont &initial,
                        const wxPoint &pos, const wxSize &size,
                        long style, const wxValidator& validator,
                        const wxString &name )
{
    if (!wxPickerBase::CreateBase(parent, id,
                                  Font2String(initial.IsOk() ? initial
                                                             : *wxNORMAL_FONT),
                                  pos, size, style, validator, name))
        return false;

    // the picker of a wxFontPickerCtrl is a wxFontPickerWidget
    m_picker = new wxFontPickerWidget(this, wxID_ANY, initial,
                                      wxDefaultPosition, wxDefaultSize,
                                      GetPickerStyle(style));
    // complete sizer creation
    wxPickerBase::PostCreation();

    m_picker->Connect(wxEVT_FONTPICKER_CHANGED,
            wxFontPickerEventHandler(wxFontPickerCtrl::OnFontChange),
            NULL, this);

    return true;
}

wxString wxFontPickerCtrl::Font2String(const wxFont &f)
{
    wxString ret = f.GetNativeFontInfoUserDesc();
#ifdef __WXMSW__
    // on wxMSW the encoding of the font is appended at the end of the string;
    // since encoding is not very user-friendly we remove it.
    wxFontEncoding enc = f.GetEncoding();
    if ( enc != wxFONTENCODING_DEFAULT && enc != wxFONTENCODING_SYSTEM )
        ret = ret.BeforeLast(wxT(' '));
#endif
    return ret;
}

wxFont wxFontPickerCtrl::String2Font(const wxString &s)
{
    wxString str(s);
    wxFont ret;
    double n;

    // put a limit on the maximum point size which the user can enter
    // NOTE: we suppose the last word of given string is the pointsize
    wxString size = str.AfterLast(wxT(' '));
    if (size.ToDouble(&n))
    {
        if (n < 1)
            str = str.Left(str.length() - size.length()) + wxT("1");
        else if (n >= m_nMaxPointSize)
            str = str.Left(str.length() - size.length()) +
                  wxString::Format(wxT("%d"), m_nMaxPointSize);
    }

    if (!ret.SetNativeFontInfoUserDesc(str))
        return wxNullFont;

    return ret;
}

void wxFontPickerCtrl::SetSelectedFont(const wxFont &f)
{
    M_PICKER->SetSelectedFont(f);
    UpdateTextCtrlFromPicker();
}

void wxFontPickerCtrl::UpdatePickerFromTextCtrl()
{
    wxASSERT(m_text);

    // NB: we don't use the wxFont::wxFont(const wxString &) constructor
    //     since that constructor expects the native font description
    //     string returned by wxFont::GetNativeFontInfoDesc() and not
    //     the user-friendly one returned by wxFont::GetNativeFontInfoUserDesc()
    wxFont f = String2Font(m_text->GetValue());
    if (!f.IsOk())
        return;     // invalid user input

    if (M_PICKER->GetSelectedFont() != f)
    {
        M_PICKER->SetSelectedFont(f);

        // fire an event
        wxFontPickerEvent event(this, GetId(), f);
        GetEventHandler()->ProcessEvent(event);
    }
}

void wxFontPickerCtrl::UpdateTextCtrlFromPicker()
{
    if (!m_text)
        return;     // no textctrl to update

    // Take care to use ChangeValue() here and not SetValue() to avoid
    // infinite recursion.
    m_text->ChangeValue(Font2String(M_PICKER->GetSelectedFont()));
}



// ----------------------------------------------------------------------------
// wxFontPickerCtrl - event handlers
// ----------------------------------------------------------------------------

void wxFontPickerCtrl::OnFontChange(wxFontPickerEvent &ev)
{
    UpdateTextCtrlFromPicker();

    // the wxFontPickerWidget sent us a colour-change notification.
    // forward this event to our parent
    wxFontPickerEvent event(this, GetId(), ev.GetFont());
    GetEventHandler()->ProcessEvent(event);
}

#endif  // wxUSE_FONTPICKERCTRL
