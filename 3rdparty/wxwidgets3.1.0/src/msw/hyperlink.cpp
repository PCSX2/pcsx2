/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/hyperlink.cpp
// Purpose:     Hyperlink control
// Author:      Rickard Westerlund
// Created:     2010-08-03
// Copyright:   (c) 2010 wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_HYPERLINKCTRL && wxUSE_UNICODE

#include "wx/hyperlink.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/msw/private.h"
    #include "wx/msw/missing.h"
#endif

// ----------------------------------------------------------------------------
// Definitions
// ----------------------------------------------------------------------------

#ifndef LM_GETIDEALSIZE
    #define LM_GETIDEALSIZE (WM_USER + 0x301)
#endif

#ifndef LWS_RIGHT
    #define LWS_RIGHT 0x0020
#endif

#ifndef WC_LINK
    #define WC_LINK L"SysLink"
#endif

// ----------------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------------

namespace
{
    bool HasNativeHyperlinkCtrl()
    {
        // Notice that we really must test comctl32.dll version and not the OS
        // version here as even under Vista/7 we could be not using the v6 e.g.
        // if the program doesn't have the correct manifest for some reason.
        return wxApp::GetComCtl32Version() >= 600;
    }

    wxString GetLabelForSysLink(const wxString& text, const wxString& url)
    {
        // Any "&"s in the text should appear on the screen and not be (mis)
        // interpreted as mnemonics.
        return wxString::Format("<A HREF=\"%s\">%s</A>",
                                url,
                                wxControl::EscapeMnemonics(text));
    }
}

// ----------------------------------------------------------------------------
// wxHyperlinkCtrl
// ----------------------------------------------------------------------------

bool wxHyperlinkCtrl::Create(wxWindow *parent,
                             wxWindowID id,
                             const wxString& label, const wxString& url,
                             const wxPoint& pos,
                             const wxSize& size,
                             long style,
                             const wxString& name)
{
    if ( !HasNativeHyperlinkCtrl() )
    {
        return wxGenericHyperlinkCtrl::Create( parent, id, label, url, pos,
                                               size, style, name );
    }

    if ( !CreateControl(parent, id, pos, size, style,
                        wxDefaultValidator, name) )
    {
        return false;
    }

    SetURL( url );
    SetVisited( false );

    WXDWORD exstyle;
    WXDWORD msStyle = MSWGetStyle(style, &exstyle);

    if ( !MSWCreateControl(WC_LINK, msStyle, pos, size,
                           GetLabelForSysLink( label, url ), exstyle) )
    {
        return false;
    }

    // Make sure both the label and URL are non-empty strings.
    SetURL(url.empty() ? label : url);
    SetLabel(label.empty() ? url : label);

    ConnectMenuHandlers();

    return true;
}

WXDWORD wxHyperlinkCtrl::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    WXDWORD msStyle = wxControl::MSWGetStyle( style, exstyle );

    if ( style & wxHL_ALIGN_RIGHT )
        msStyle |= LWS_RIGHT;

    return msStyle;
}

void wxHyperlinkCtrl::SetURL(const wxString &url)
{
    if ( !HasNativeHyperlinkCtrl() )
    {
        wxGenericHyperlinkCtrl::SetURL( url );
        return;
    }

    if ( GetURL() != url )
        SetVisited( false );
    wxGenericHyperlinkCtrl::SetURL( url );
    wxWindow::SetLabel( GetLabelForSysLink(m_labelOrig, url) );
}

void wxHyperlinkCtrl::SetLabel(const wxString &label)
{
    if ( !HasNativeHyperlinkCtrl() )
    {
        wxGenericHyperlinkCtrl::SetLabel( label );
        return;
    }

    m_labelOrig = label;
    wxWindow::SetLabel( GetLabelForSysLink(label, GetURL()) );
    InvalidateBestSize();
}

wxSize wxHyperlinkCtrl::DoGetBestClientSize() const
{
    // LM_GETIDEALSIZE only exists under Vista so use the generic version even
    // when using the native control under XP
    if ( !HasNativeHyperlinkCtrl() || (wxGetWinVersion() < wxWinVersion_6) )
        return wxGenericHyperlinkCtrl::DoGetBestClientSize();

    SIZE idealSize;
    ::SendMessage(m_hWnd, LM_GETIDEALSIZE, 0, (LPARAM)&idealSize);

    return wxSize(idealSize.cx, idealSize.cy);
}

bool wxHyperlinkCtrl::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result)
{
    if ( HasNativeHyperlinkCtrl() )
    {
        switch ( ((LPNMHDR) lParam)->code )
        {
            case NM_CLICK:
            case NM_RETURN:
                SetVisited();
                SendEvent();
                return 0;
        }
    }

   return wxGenericHyperlinkCtrl::MSWOnNotify(idCtrl, lParam, result);
}

#endif // wxUSE_HYPERLINKCTRL && wxUSE_UNICODE
