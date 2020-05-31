/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/radiobut.cpp
// Purpose:     wxRadioButton
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

#if wxUSE_RADIOBTN

#include "wx/radiobut.h"

#ifndef WX_PRECOMP
    #include "wx/settings.h"
    #include "wx/dcscreen.h"
    #include "wx/toplevel.h"
#endif

#include "wx/msw/private.h"

// ============================================================================
// wxRadioButton implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxRadioButton creation
// ----------------------------------------------------------------------------

void wxRadioButton::Init()
{
    m_isChecked = false;
}

bool wxRadioButton::Create(wxWindow *parent,
                           wxWindowID id,
                           const wxString& label,
                           const wxPoint& pos,
                           const wxSize& size,
                           long style,
                           const wxValidator& validator,
                           const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    WXDWORD exstyle = 0;
    WXDWORD msStyle = MSWGetStyle(style, &exstyle);

    if ( !MSWCreateControl(wxT("BUTTON"), msStyle, pos, size, label, exstyle) )
        return false;

    // for compatibility with wxGTK, the first radio button in a group is
    // always checked (this makes sense anyhow as you need to ensure that at
    // least one button in the group is checked and this is the simplest way to
    // do it)
    if ( HasFlag(wxRB_GROUP) )
        SetValue(true);

    return true;
}

// ----------------------------------------------------------------------------
// wxRadioButton functions
// ----------------------------------------------------------------------------

void wxRadioButton::SetValue(bool value)
{
    ::SendMessage(GetHwnd(), BM_SETCHECK,
                  value ? BST_CHECKED : BST_UNCHECKED, 0);

    m_isChecked = value;

    if ( !value )
        return;

    // if we set the value of one radio button we also must clear all the other
    // buttons in the same group: Windows doesn't do it automatically
    //
    // moreover, if another radiobutton in the group currently has the focus,
    // we have to set it to this radiobutton, else the old radiobutton will be
    // reselected automatically, if a parent window loses the focus and regains
    // it.
    wxWindow * const focus = FindFocus();
    wxTopLevelWindow * const
        tlw = wxDynamicCast(wxGetTopLevelParent(this), wxTopLevelWindow);
    wxCHECK_RET( tlw, wxT("radio button outside of TLW?") );
    wxWindow * const focusInTLW = tlw->GetLastFocus();

    const wxWindowList& siblings = GetParent()->GetChildren();
    wxWindowList::compatibility_iterator nodeThis = siblings.Find(this);
    wxCHECK_RET( nodeThis, wxT("radio button not a child of its parent?") );

    // this will be set to true in the code below if the focus is in our TLW
    // and belongs to one of the other buttons in the same group
    bool shouldSetFocus = false;

    // this will be set to true if the focus is outside of our TLW currently
    // but the remembered focus of this TLW is one of the other buttons in the
    // same group
    bool shouldSetTLWFocus = false;

    // if it's not the first item of the group ...
    if ( !HasFlag(wxRB_GROUP) )
    {
        // ... turn off all radio buttons before it
        for ( wxWindowList::compatibility_iterator nodeBefore = nodeThis->GetPrevious();
              nodeBefore;
              nodeBefore = nodeBefore->GetPrevious() )
        {
            wxRadioButton *btn = wxDynamicCast(nodeBefore->GetData(),
                                               wxRadioButton);
            if ( !btn )
            {
                // don't stop on non radio buttons, we could have intermixed
                // buttons and e.g. static labels
                continue;
            }

            if ( btn->HasFlag(wxRB_SINGLE) )
                {
                    // A wxRB_SINGLE button isn't part of this group
                    break;
                }

            if ( btn == focus )
                shouldSetFocus = true;
            else if ( btn == focusInTLW )
                shouldSetTLWFocus = true;

            btn->SetValue(false);

            if ( btn->HasFlag(wxRB_GROUP) )
            {
                // even if there are other radio buttons before this one,
                // they're not in the same group with us
                break;
            }
        }
    }

    // ... and also turn off all buttons after this one
    for ( wxWindowList::compatibility_iterator nodeAfter = nodeThis->GetNext();
          nodeAfter;
          nodeAfter = nodeAfter->GetNext() )
    {
        wxRadioButton *btn = wxDynamicCast(nodeAfter->GetData(),
                                           wxRadioButton);

        if ( !btn )
            continue;

        if ( btn->HasFlag(wxRB_GROUP | wxRB_SINGLE) )
        {
            // no more buttons or the first button of the next group
            break;
        }

        if ( btn == focus )
            shouldSetFocus = true;
        else if ( btn == focusInTLW )
            shouldSetTLWFocus = true;

        btn->SetValue(false);
    }

    if ( shouldSetFocus )
        SetFocus();
    else if ( shouldSetTLWFocus )
        tlw->SetLastFocus(this);
}

bool wxRadioButton::GetValue() const
{
    wxASSERT_MSG( m_isChecked ==
                    (::SendMessage(GetHwnd(), BM_GETCHECK, 0, 0L) != 0),
                  wxT("wxRadioButton::m_isChecked is out of sync?") );

    return m_isChecked;
}

// ----------------------------------------------------------------------------
// wxRadioButton event processing
// ----------------------------------------------------------------------------

void wxRadioButton::Command (wxCommandEvent& event)
{
    SetValue(event.GetInt() != 0);
    ProcessCommand(event);
}

bool wxRadioButton::MSWCommand(WXUINT param, WXWORD WXUNUSED(id))
{
    if ( param != BN_CLICKED )
        return false;

    if ( !m_isChecked )
    {
        // we need to manually update the button state as we use BS_RADIOBUTTON
        // and not BS_AUTORADIOBUTTON
        SetValue(true);

        wxCommandEvent event(wxEVT_RADIOBUTTON, GetId());
        event.SetEventObject( this );
        event.SetInt(true); // always checked

        ProcessCommand(event);
    }

    return true;
}

// ----------------------------------------------------------------------------
// wxRadioButton geometry
// ----------------------------------------------------------------------------

wxSize wxRadioButton::DoGetBestSize() const
{
    static int s_radioSize = 0;

    if ( !s_radioSize )
    {
        wxScreenDC dc;
        dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));

        s_radioSize = dc.GetCharHeight();

        // radio button bitmap size under CE is bigger than the font height,
        // adding just one pixel seems to work fine for the default font but it
        // would be nice to find some better way to find the correct height
#ifdef __WXWINCE__
        s_radioSize++;
#endif // __WXWINCE__
    }

    wxString str = GetLabel();

    int wRadio, hRadio;
    if ( !str.empty() )
    {
        GetTextExtent(GetLabelText(str), &wRadio, &hRadio);
        wRadio += s_radioSize + GetCharWidth();

        if ( hRadio < s_radioSize )
            hRadio = s_radioSize;
    }
    else
    {
        wRadio = s_radioSize;
        hRadio = s_radioSize;
    }

    wxSize best(wRadio, hRadio);
    CacheBestSize(best);
    return best;
}

WXDWORD wxRadioButton::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    WXDWORD msStyle = wxControl::MSWGetStyle(style, exstyle);

    if ( HasFlag(wxRB_GROUP) )
        msStyle |= WS_GROUP;

    // we use BS_RADIOBUTTON and not BS_AUTORADIOBUTTON because the use of the
    // latter can easily result in the application entering an infinite loop
    // inside IsDialogMessage()
    //
    // we used to use BS_RADIOBUTTON only for wxRB_SINGLE buttons but there
    // doesn't seem to be any harm to always use it and it prevents some hangs,
    // see #9786
    msStyle |= BS_RADIOBUTTON;

    if ( style & wxCLIP_SIBLINGS )
        msStyle |= WS_CLIPSIBLINGS;
    if ( style & wxALIGN_RIGHT )
        msStyle |= BS_LEFTTEXT | BS_RIGHT;


    return msStyle;
}

#endif // wxUSE_RADIOBTN
