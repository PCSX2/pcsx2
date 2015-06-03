/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/hyperlinkg.cpp
// Purpose:     Hyperlink control
// Author:      David Norris <danorris@gmail.com>, Otto Wyss
// Modified by: Ryan Norton, Francesco Montorsi
// Created:     04/02/2005
// Copyright:   (c) 2005 David Norris
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

//---------------------------------------------------------------------------
// Pre-compiled header stuff
//---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_HYPERLINKCTRL

//---------------------------------------------------------------------------
// Includes
//---------------------------------------------------------------------------

#include "wx/hyperlink.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h" // for wxLaunchDefaultBrowser
    #include "wx/dcclient.h"
    #include "wx/menu.h"
    #include "wx/log.h"
    #include "wx/dataobj.h"
#endif

#include "wx/clipbrd.h"
#include "wx/renderer.h"

// ============================================================================
// implementation
// ============================================================================

// reserved for internal use only
#define wxHYPERLINK_POPUP_COPY_ID           16384

// ----------------------------------------------------------------------------
// wxGenericHyperlinkCtrl
// ----------------------------------------------------------------------------

bool wxGenericHyperlinkCtrl::Create(wxWindow *parent, wxWindowID id,
    const wxString& label, const wxString& url, const wxPoint& pos,
    const wxSize& size, long style, const wxString& name)
{
    // do validation checks:
    CheckParams(label, url, style);

    if ((style & wxHL_ALIGN_LEFT) == 0)
        style |= wxFULL_REPAINT_ON_RESIZE;

    if (!wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name))
        return false;

    // set to non empty strings both the url and the label
    SetURL(url.empty() ? label : url);
    SetLabel(label.empty() ? url : label);

    Init();
    SetForegroundColour(m_normalColour);

    // by default the font of an hyperlink control is underlined
    wxFont f = GetFont();
    f.SetUnderlined(true);
    SetFont(f);

    SetInitialSize(size);


    // connect our event handlers:
    // NOTE: since this class is the base class of the GTK+'s native implementation
    //       of wxHyperlinkCtrl, we cannot use the static macros in wxBEGIN/wxEND_EVENT_TABLE
    //       blocks, otherwise the GTK+'s native impl of wxHyperlinkCtrl would not
    //       behave correctly (as we intercept events doing things which interfere
    //       with GTK+'s native handling):

    Connect( wxEVT_PAINT, wxPaintEventHandler(wxGenericHyperlinkCtrl::OnPaint) );
    Connect( wxEVT_SET_FOCUS, wxFocusEventHandler(wxGenericHyperlinkCtrl::OnFocus) );
    Connect( wxEVT_KILL_FOCUS, wxFocusEventHandler(wxGenericHyperlinkCtrl::OnFocus) );
    Connect( wxEVT_CHAR, wxKeyEventHandler(wxGenericHyperlinkCtrl::OnChar) );
    Connect( wxEVT_LEAVE_WINDOW, wxMouseEventHandler(wxGenericHyperlinkCtrl::OnLeaveWindow) );

    Connect( wxEVT_LEFT_DOWN, wxMouseEventHandler(wxGenericHyperlinkCtrl::OnLeftDown) );
    Connect( wxEVT_LEFT_UP, wxMouseEventHandler(wxGenericHyperlinkCtrl::OnLeftUp) );
    Connect( wxEVT_MOTION, wxMouseEventHandler(wxGenericHyperlinkCtrl::OnMotion) );

    ConnectMenuHandlers();

    return true;
}

void wxGenericHyperlinkCtrl::Init()
{
    m_rollover = false;
    m_clicking = false;
    m_visited = false;

    // colours
    m_normalColour = *wxBLUE;
    m_hoverColour = *wxRED;
    m_visitedColour = wxColour("#551a8b");
}

void wxGenericHyperlinkCtrl::ConnectMenuHandlers()
{
    // Connect the event handlers for the context menu.
    Connect( wxEVT_RIGHT_UP, wxMouseEventHandler(wxGenericHyperlinkCtrl::OnRightUp) );
    Connect( wxHYPERLINK_POPUP_COPY_ID, wxEVT_MENU,
             wxCommandEventHandler(wxGenericHyperlinkCtrl::OnPopUpCopy) );
}

wxSize wxGenericHyperlinkCtrl::DoGetBestClientSize() const
{
    wxClientDC dc((wxWindow *)this);
    return dc.GetTextExtent(GetLabel());
}


void wxGenericHyperlinkCtrl::SetNormalColour(const wxColour &colour)
{
    m_normalColour = colour;
    if (!m_visited)
    {
        SetForegroundColour(m_normalColour);
        Refresh();
    }
}

void wxGenericHyperlinkCtrl::SetVisitedColour(const wxColour &colour)
{
    m_visitedColour = colour;
    if (m_visited)
    {
        SetForegroundColour(m_visitedColour);
        Refresh();
    }
}

void wxGenericHyperlinkCtrl::DoContextMenu(const wxPoint &pos)
{
    wxMenu *menuPopUp = new wxMenu(wxEmptyString, wxMENU_TEAROFF);
    menuPopUp->Append(wxHYPERLINK_POPUP_COPY_ID, _("&Copy URL"));
    PopupMenu( menuPopUp, pos );
    delete menuPopUp;
}

wxRect wxGenericHyperlinkCtrl::GetLabelRect() const
{
    // our best size is always the size of the label without borders
    wxSize c(GetClientSize()), b(GetBestSize());
    wxPoint offset;

    // the label is always centered vertically
    offset.y = (c.GetHeight()-b.GetHeight())/2;

    if (HasFlag(wxHL_ALIGN_CENTRE))
        offset.x = (c.GetWidth()-b.GetWidth())/2;
    else if (HasFlag(wxHL_ALIGN_RIGHT))
        offset.x = c.GetWidth()-b.GetWidth();
    else if (HasFlag(wxHL_ALIGN_LEFT))
        offset.x = 0;
    return wxRect(offset, b);
}



// ----------------------------------------------------------------------------
// wxGenericHyperlinkCtrl - event handlers
// ----------------------------------------------------------------------------

void wxGenericHyperlinkCtrl::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    dc.SetFont(GetFont());
    dc.SetTextForeground(GetForegroundColour());
    dc.SetTextBackground(GetBackgroundColour());

    dc.DrawText(GetLabel(), GetLabelRect().GetTopLeft());
    if (HasFocus())
    {
        wxRendererNative::Get().DrawFocusRect(this, dc, GetClientRect(), wxCONTROL_SELECTED);
    }
}

void wxGenericHyperlinkCtrl::OnFocus(wxFocusEvent& event)
{
    Refresh();
    event.Skip();
}

void wxGenericHyperlinkCtrl::OnChar(wxKeyEvent& event)
{
    switch (event.m_keyCode)
    {
    default:
        event.Skip();
        break;
    case WXK_SPACE:
    case WXK_NUMPAD_SPACE:
        SetForegroundColour(m_visitedColour);
        m_visited = true;
        SendEvent();
        break;
    }
}

void wxGenericHyperlinkCtrl::OnLeftDown(wxMouseEvent& event)
{
    // the left click must start from the hyperlink rect
    m_clicking = GetLabelRect().Contains(event.GetPosition());
}

void wxGenericHyperlinkCtrl::OnLeftUp(wxMouseEvent& event)
{
    // the click must be started and ended in the hyperlink rect
    if (!m_clicking || !GetLabelRect().Contains(event.GetPosition()))
        return;

    SetForegroundColour(m_visitedColour);
    m_visited = true;
    m_clicking = false;

    // send the event
    SendEvent();
}

void wxGenericHyperlinkCtrl::OnRightUp(wxMouseEvent& event)
{
    if( GetWindowStyle() & wxHL_CONTEXTMENU )
        if ( GetLabelRect().Contains(event.GetPosition()) )
            DoContextMenu(wxPoint(event.m_x, event.m_y));
}

void wxGenericHyperlinkCtrl::OnMotion(wxMouseEvent& event)
{
    wxRect textrc = GetLabelRect();

    if (textrc.Contains(event.GetPosition()))
    {
        SetCursor(wxCursor(wxCURSOR_HAND));
        SetForegroundColour(m_hoverColour);
        m_rollover = true;
        Refresh();
    }
    else if (m_rollover)
    {
        SetCursor(*wxSTANDARD_CURSOR);
        SetForegroundColour(!m_visited ? m_normalColour : m_visitedColour);
        m_rollover = false;
        Refresh();
    }
}

void wxGenericHyperlinkCtrl::OnLeaveWindow(wxMouseEvent& WXUNUSED(event) )
{
    // NB: when the label rect and the client size rect have the same
    //     height this function is indispensable to remove the "rollover"
    //     effect as the OnMotion() event handler could not be called
    //     in that case moving the mouse out of the label vertically...

    if (m_rollover)
    {
        SetCursor(*wxSTANDARD_CURSOR);
        SetForegroundColour(!m_visited ? m_normalColour : m_visitedColour);
        m_rollover = false;
        Refresh();
    }
}

void wxGenericHyperlinkCtrl::OnPopUpCopy( wxCommandEvent& WXUNUSED(event) )
{
#if wxUSE_CLIPBOARD
    if (!wxTheClipboard->Open())
        return;

    wxTextDataObject *data = new wxTextDataObject( m_url );
    wxTheClipboard->SetData( data );
    wxTheClipboard->Close();
#endif // wxUSE_CLIPBOARD
}

#endif // wxUSE_HYPERLINKCTRL
