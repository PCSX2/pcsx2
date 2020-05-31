/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/hyperlnkcmn.cpp
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
    #include "wx/menu.h"
    #include "wx/log.h"
    #include "wx/dataobj.h"
#endif

// ============================================================================
// implementation
// ============================================================================

wxDEFINE_FLAGS( wxHyperlinkStyle )
wxBEGIN_FLAGS( wxHyperlinkStyle )
// new style border flags, we put them first to
// use them for streaming out
wxFLAGS_MEMBER(wxBORDER_SIMPLE)
wxFLAGS_MEMBER(wxBORDER_SUNKEN)
wxFLAGS_MEMBER(wxBORDER_DOUBLE)
wxFLAGS_MEMBER(wxBORDER_RAISED)
wxFLAGS_MEMBER(wxBORDER_STATIC)
wxFLAGS_MEMBER(wxBORDER_NONE)

// old style border flags
wxFLAGS_MEMBER(wxSIMPLE_BORDER)
wxFLAGS_MEMBER(wxSUNKEN_BORDER)
wxFLAGS_MEMBER(wxDOUBLE_BORDER)
wxFLAGS_MEMBER(wxRAISED_BORDER)
wxFLAGS_MEMBER(wxSTATIC_BORDER)
wxFLAGS_MEMBER(wxBORDER)

// standard window styles
wxFLAGS_MEMBER(wxTAB_TRAVERSAL)
wxFLAGS_MEMBER(wxCLIP_CHILDREN)
wxFLAGS_MEMBER(wxTRANSPARENT_WINDOW)
wxFLAGS_MEMBER(wxWANTS_CHARS)
wxFLAGS_MEMBER(wxFULL_REPAINT_ON_RESIZE)
wxFLAGS_MEMBER(wxALWAYS_SHOW_SB )
wxFLAGS_MEMBER(wxVSCROLL)
wxFLAGS_MEMBER(wxHSCROLL)

wxFLAGS_MEMBER(wxHL_CONTEXTMENU)
wxFLAGS_MEMBER(wxHL_ALIGN_LEFT)
wxFLAGS_MEMBER(wxHL_ALIGN_RIGHT)
wxFLAGS_MEMBER(wxHL_ALIGN_CENTRE)
wxEND_FLAGS( wxHyperlinkStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI( wxHyperlinkCtrl, wxControl, "wx/hyperlink.h")

IMPLEMENT_DYNAMIC_CLASS(wxHyperlinkEvent, wxCommandEvent)
wxDEFINE_EVENT( wxEVT_HYPERLINK, wxHyperlinkEvent );

wxBEGIN_PROPERTIES_TABLE(wxHyperlinkCtrl)
wxPROPERTY( Label, wxString, SetLabel, GetLabel, wxString(), \
           0 /*flags*/, wxT("The link label"), wxT("group") )

wxPROPERTY( URL, wxString, SetURL, GetURL, wxString(), \
           0 /*flags*/, wxT("The link URL"), wxT("group") )
wxPROPERTY_FLAGS( WindowStyle, wxHyperlinkStyle, long, SetWindowStyleFlag, \
                 GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/,     \
                 wxT("The link style"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxHyperlinkCtrl)

wxCONSTRUCTOR_7( wxHyperlinkCtrl, wxWindow*, Parent, wxWindowID, Id, wxString, \
                Label, wxString, URL, wxPoint, Position, wxSize, Size, long, WindowStyle )


const char wxHyperlinkCtrlNameStr[] = "hyperlink";

// ----------------------------------------------------------------------------
// wxHyperlinkCtrlBase
// ----------------------------------------------------------------------------

void
wxHyperlinkCtrlBase::CheckParams(const wxString& label,
                                 const wxString& url,
                                 long style)
{
#if wxDEBUG_LEVEL
    wxASSERT_MSG(!url.empty() || !label.empty(),
                 wxT("Both URL and label are empty ?"));

    int alignment = (int)((style & wxHL_ALIGN_LEFT) != 0) +
                    (int)((style & wxHL_ALIGN_CENTRE) != 0) +
                    (int)((style & wxHL_ALIGN_RIGHT) != 0);
    wxASSERT_MSG(alignment == 1,
        wxT("Specify exactly one align flag!"));
#else // !wxDEBUG_LEVEL
    wxUnusedVar(label);
    wxUnusedVar(url);
    wxUnusedVar(style);
#endif // wxDEBUG_LEVEL/!wxDEBUG_LEVEL
}

void wxHyperlinkCtrlBase::SendEvent()
{
    wxString url = GetURL();
    wxHyperlinkEvent linkEvent(this, GetId(), url);
    if (!GetEventHandler()->ProcessEvent(linkEvent))     // was the event skipped ?
    {
        if (!wxLaunchDefaultBrowser(url))
        {
            wxLogWarning(wxT("Could not launch the default browser with url '%s' !"), url.c_str());
        }
    }
}

#endif // wxUSE_HYPERLINKCTRL
