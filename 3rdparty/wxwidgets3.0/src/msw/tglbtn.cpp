/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/tglbtn.cpp
// Purpose:     Definition of the wxToggleButton class, which implements a
//              toggle button under wxMSW.
// Author:      John Norris, minor changes by Axel Schlueter
//              and William Gallafent.
// Modified by:
// Created:     08.02.01
// Copyright:   (c) 2000 Johnny C. Norris II
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TOGGLEBTN

#include "wx/tglbtn.h"

#ifndef WX_PRECOMP
    #include "wx/button.h"
    #include "wx/brush.h"
    #include "wx/dcscreen.h"
    #include "wx/settings.h"

    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/msw/private.h"
#include "wx/msw/private/button.h"

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

wxDEFINE_EVENT( wxEVT_TOGGLEBUTTON, wxCommandEvent );

// ============================================================================
// implementation
// ============================================================================

//-----------------------------------------------------------------------------
// wxBitmapToggleButton
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxBitmapToggleButton, wxToggleButton)

bool wxBitmapToggleButton::Create( wxWindow *parent, wxWindowID id,
                const wxBitmap& label,const wxPoint& pos, const wxSize& size, long style,
                const wxValidator& validator, const wxString& name )
{
    if (!wxToggleButton::Create( parent, id, wxEmptyString, pos, size, style, validator, name ))
        return false;

    SetBitmap(label);

    if (size.x == -1 || size.y == -1)
    {
        wxSize new_size = GetBestSize();
        if (size.x != -1)
            new_size.x = size.x;
        if (size.y != -1)
            new_size.y = size.y;
        SetSize( new_size );
    }

    return true;
}


// ----------------------------------------------------------------------------
// wxToggleButton
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxToggleButton, wxControl)

void wxToggleButton::Init()
{
    m_state = false;
}

// Single check box item
bool wxToggleButton::Create(wxWindow *parent,
                            wxWindowID id,
                            const wxString& label,
                            const wxPoint& pos,
                            const wxSize& size, long style,
                            const wxValidator& validator,
                            const wxString& name)
{
    Init();

    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    // if the label contains several lines we must explicitly tell the button
    // about it or it wouldn't draw it correctly ("\n"s would just appear as
    // black boxes)
    //
    // NB: we do it here and not in MSWGetStyle() because we need the label
    //     value and the label is not set yet when MSWGetStyle() is called
    WXDWORD exstyle;
    WXDWORD msStyle = MSWGetStyle(style, &exstyle);
    msStyle |= wxMSWButton::GetMultilineStyle(label);

    return MSWCreateControl(wxT("BUTTON"), msStyle, pos, size, label, exstyle);
}

WXDWORD wxToggleButton::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    WXDWORD msStyle = wxControl::MSWGetStyle(style, exstyle);

    msStyle |= BS_AUTOCHECKBOX | BS_PUSHLIKE | WS_TABSTOP;

    if ( style & wxBU_LEFT )
      msStyle |= BS_LEFT;
    if ( style & wxBU_RIGHT )
      msStyle |= BS_RIGHT;
    if ( style & wxBU_TOP )
      msStyle |= BS_TOP;
    if ( style & wxBU_BOTTOM )
      msStyle |= BS_BOTTOM;

    return msStyle;
}

void wxToggleButton::SetValue(bool val)
{
    m_state = val;
    if ( IsOwnerDrawn() )
    {
        Refresh();
    }
    else
    {
        ::SendMessage(GetHwnd(), BM_SETCHECK, val, 0);
    }
}

bool wxToggleButton::GetValue() const
{
    if ( IsOwnerDrawn() )
    {
        return m_state;
    }
    else
    {
        return ::SendMessage(GetHwnd(), BM_GETCHECK, 0, 0) == BST_CHECKED;
    }
}

void wxToggleButton::Command(wxCommandEvent& event)
{
    SetValue(event.GetInt() != 0);
    ProcessCommand(event);
}

bool wxToggleButton::MSWCommand(WXUINT param, WXWORD WXUNUSED(id))
{
    if ( param != BN_CLICKED && param != BN_DBLCLK )
        return false;

    // first update the value so that user event handler gets the new
    // toggle button value

    // ownerdrawn buttons don't manage their state themselves unlike usual
    // auto checkboxes so do it ourselves in any case
    m_state = !m_state;

    wxCommandEvent event(wxEVT_TOGGLEBUTTON, m_windowId);
    event.SetInt(GetValue());
    event.SetEventObject(this);
    ProcessCommand(event);
    return true;
}

wxAnyButton::State wxToggleButton::GetNormalState() const
{
    if ( GetValue() )
        return State_Pressed;
    else
        return State_Normal;
}

#endif // wxUSE_TOGGLEBTN
