///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/clrpickerg.cpp
// Purpose:     wxGenericColourButton class implementation
// Author:      Francesco Montorsi (readapted code written by Vadim Zeitlin)
// Modified by:
// Created:     15/04/2006
// Copyright:   (c) Vadim Zeitlin, Francesco Montorsi
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

#if wxUSE_COLOURPICKERCTRL

#include "wx/clrpicker.h"
#include "wx/colordlg.h"
#include "wx/dcmemory.h"


// ============================================================================
// implementation
// ============================================================================

wxColourData wxGenericColourButton::ms_data;
IMPLEMENT_DYNAMIC_CLASS(wxGenericColourButton, wxBitmapButton)

// ----------------------------------------------------------------------------
// wxGenericColourButton
// ----------------------------------------------------------------------------

bool wxGenericColourButton::Create( wxWindow *parent, wxWindowID id,
                        const wxColour &col, const wxPoint &pos,
                        const wxSize &size, long style,
                        const wxValidator& validator, const wxString &name)
{
    m_bitmap = wxBitmap( 60, 13 );

    // create this button
    if (!wxBitmapButton::Create( parent, id, m_bitmap, pos,
                           size, style | wxBU_AUTODRAW, validator, name ))
    {
        wxFAIL_MSG( wxT("wxGenericColourButton creation failed") );
        return false;
    }

    // and handle user clicks on it
    Connect(GetId(), wxEVT_BUTTON,
            wxCommandEventHandler(wxGenericColourButton::OnButtonClick),
            NULL, this);

    m_colour = col;
    UpdateColour();
    InitColourData();

    return true;
}

void wxGenericColourButton::InitColourData()
{
    ms_data.SetChooseFull(true);
    unsigned char grey = 0;
    for (int i = 0; i < 16; i++, grey += 16)
    {
        // fill with grey tones the custom colors palette
        wxColour colour(grey, grey, grey);
        ms_data.SetCustomColour(i, colour);
    }
}

void wxGenericColourButton::OnButtonClick(wxCommandEvent& WXUNUSED(ev))
{
    // update the wxColouData to be shown in the dialog
    ms_data.SetColour(m_colour);

    // create the colour dialog and display it
    wxColourDialog dlg(this, &ms_data);
    if (dlg.ShowModal() == wxID_OK)
    {
        ms_data = dlg.GetColourData();
        SetColour(ms_data.GetColour());

        // fire an event
        wxColourPickerEvent event(this, GetId(), m_colour);
        GetEventHandler()->ProcessEvent(event);
    }
}

void wxGenericColourButton::UpdateColour()
{
    wxMemoryDC dc(m_bitmap);
    dc.SetPen( *wxTRANSPARENT_PEN );
    dc.SetBrush( wxBrush(m_colour) );
    dc.DrawRectangle( 0,0,m_bitmap.GetWidth(),m_bitmap.GetHeight() );

    if ( HasFlag(wxCLRP_SHOW_LABEL) )
    {
        wxColour col( ~m_colour.Red(), ~m_colour.Green(), ~m_colour.Blue() );
        dc.SetTextForeground( col );
        dc.SetFont( GetFont() );
        dc.DrawText( m_colour.GetAsString(wxC2S_HTML_SYNTAX), 0, 0 );
    }

    dc.SelectObject( wxNullBitmap );
    SetBitmapLabel( m_bitmap );
}

wxSize wxGenericColourButton::DoGetBestSize() const
{
    wxSize sz(wxBitmapButton::DoGetBestSize());
#ifdef __WXMAC__
    sz.y += 6;
#else
    sz.y += 2;
#endif
    sz.x += 30;
    if ( HasFlag(wxCLRP_SHOW_LABEL) )
        return sz;

    // if we have no label, then make this button a square
    // (like e.g. native GTK version of this control) ???
    // sz.SetWidth(sz.GetHeight());
    return sz;
}

#endif      // wxUSE_COLOURPICKERCTRL
