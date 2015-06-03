/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/spinctrlcmn.cpp
// Purpose:     define wxSpinCtrl event types
// Author:      Peter Most
// Created:     01.11.08
// Copyright:   (c) 2008-2009 wxWidgets team
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

#include "wx/spinbutt.h"
#include "wx/spinctrl.h"

#if wxUSE_SPINCTRL

wxDEFINE_EVENT(wxEVT_SPINCTRL, wxSpinEvent);
wxDEFINE_EVENT(wxEVT_SPINCTRLDOUBLE, wxSpinDoubleEvent);

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxSpinCtrlStyle )
wxBEGIN_FLAGS( wxSpinCtrlStyle )
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
wxEND_FLAGS( wxSpinCtrlStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxSpinCtrl, wxControl, "wx/spinctrl.h");

wxBEGIN_PROPERTIES_TABLE(wxSpinCtrl)
wxEVENT_RANGE_PROPERTY( Spin, wxEVT_SCROLL_TOP, wxEVT_SCROLL_CHANGED, wxSpinEvent )

wxEVENT_PROPERTY( Updated, wxEVT_SPINCTRL, wxCommandEvent )
wxEVENT_PROPERTY( TextUpdated, wxEVT_TEXT, wxCommandEvent )
wxEVENT_PROPERTY( TextEnter, wxEVT_TEXT_ENTER, wxCommandEvent )

wxPROPERTY( ValueString, wxString, SetValue, GetValue, \
           wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, wxT("Helpstring"), wxT("group")) ;
wxPROPERTY( Value, int, SetValue, GetValue, 0, 0 /*flags*/, \
           wxT("Helpstring"), wxT("group"))
#if 0
 wxPROPERTY( Min, int, SetMin, GetMin, 0, 0 /*flags*/, wxT("Helpstring"), wxT("group") )
 wxPROPERTY( Max, int, SetMax, GetMax, 0, 0 /*flags*/, wxT("Helpstring"), wxT("group"))
#endif
wxPROPERTY_FLAGS( WindowStyle, wxSpinCtrlStyle, long, SetWindowStyleFlag, \
                 GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                 wxT("Helpstring"), wxT("group")) // style
/*
 TODO PROPERTIES
 style wxSP_ARROW_KEYS
 */
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxSpinCtrl)

wxCONSTRUCTOR_6( wxSpinCtrl, wxWindow*, Parent, wxWindowID, Id, \
                wxString, ValueString, wxPoint, Position, \
                wxSize, Size, long, WindowStyle )


wxString wxPrivate::wxSpinCtrlFormatAsHex(long val, long maxVal)
{
    // We format the value like this is for compatibility with (native
    // behaviour of) wxMSW
    wxString text;
    if ( maxVal < 0x10000 )
        text.Printf(wxS("0x%04lx"), val);
    else
        text.Printf(wxS("0x%08lx"), val);

    return text;
}

#endif // wxUSE_SPINCTRL
