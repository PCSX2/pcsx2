/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/statboxcmn.cpp
// Purpose:     wxStaticBox common code
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

#if wxUSE_STATBOX

#include "wx/statbox.h"

extern WXDLLEXPORT_DATA(const char) wxStaticBoxNameStr[] = "groupBox";

wxStaticBoxBase::wxStaticBoxBase()
{
#ifndef __WXGTK__
    m_container.DisableSelfFocus();
#endif
}

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxStaticBoxStyle )
wxBEGIN_FLAGS( wxStaticBoxStyle )
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
wxEND_FLAGS( wxStaticBoxStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxStaticBox, wxControl, "wx/statbox.h")

wxBEGIN_PROPERTIES_TABLE(wxStaticBox)
    wxPROPERTY( Label, wxString, SetLabel, GetLabel, wxString(), 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))
    wxPROPERTY_FLAGS( WindowStyle, wxStaticBoxStyle, long, SetWindowStyleFlag, \
                      GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                      wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxStaticBox)

wxCONSTRUCTOR_6( wxStaticBox, wxWindow*, Parent, wxWindowID, Id, \
                 wxString, Label, wxPoint, Position, wxSize, Size, \
                 long, WindowStyle )

#endif // wxUSE_STATBOX
