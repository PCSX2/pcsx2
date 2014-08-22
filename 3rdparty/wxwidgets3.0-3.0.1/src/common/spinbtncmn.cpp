/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/spinbtncmn.cpp
// Purpose:     wxSpinButton common code
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

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif

#if wxUSE_SPINBTN

#include "wx/spinbutt.h"

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxSpinButtonStyle )
wxBEGIN_FLAGS( wxSpinButtonStyle )
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

    wxFLAGS_MEMBER(wxSP_HORIZONTAL)
    wxFLAGS_MEMBER(wxSP_VERTICAL)
    wxFLAGS_MEMBER(wxSP_ARROW_KEYS)
    wxFLAGS_MEMBER(wxSP_WRAP)
wxEND_FLAGS( wxSpinButtonStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxSpinButton, wxControl, "wx/spinbutt.h")

wxBEGIN_PROPERTIES_TABLE(wxSpinButton)
    wxEVENT_RANGE_PROPERTY( Spin, wxEVT_SCROLL_TOP, wxEVT_SCROLL_CHANGED, wxSpinEvent )

    wxPROPERTY( Value, int, SetValue, GetValue, 0, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))
    wxPROPERTY( Min, int, SetMin, GetMin, 0, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))
    wxPROPERTY( Max, int, SetMax, GetMax, 0, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))

    wxPROPERTY_FLAGS( WindowStyle, wxSpinButtonStyle, long, SetWindowStyleFlag, \
                      GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                      wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxSpinButton)

wxCONSTRUCTOR_5( wxSpinButton, wxWindow*, Parent, wxWindowID, Id, \
                 wxPoint, Position, wxSize, Size, long, WindowStyle )

IMPLEMENT_DYNAMIC_CLASS(wxSpinEvent, wxNotifyEvent)


#endif // wxUSE_SPINBTN
