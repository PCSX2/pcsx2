/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/slidercmn.cpp
// Purpose:     wxSlider common code
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart 1998
//                  Vadim Zeitlin 2004
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

#if wxUSE_SLIDER

#include "wx/slider.h"

extern WXDLLEXPORT_DATA(const char) wxSliderNameStr[] = "slider";

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxSliderStyle )
wxBEGIN_FLAGS( wxSliderStyle )
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

    wxFLAGS_MEMBER(wxSL_HORIZONTAL)
    wxFLAGS_MEMBER(wxSL_VERTICAL)
    wxFLAGS_MEMBER(wxSL_AUTOTICKS)
    wxFLAGS_MEMBER(wxSL_LABELS)
    wxFLAGS_MEMBER(wxSL_LEFT)
    wxFLAGS_MEMBER(wxSL_TOP)
    wxFLAGS_MEMBER(wxSL_RIGHT)
    wxFLAGS_MEMBER(wxSL_BOTTOM)
    wxFLAGS_MEMBER(wxSL_BOTH)
    wxFLAGS_MEMBER(wxSL_SELRANGE)
    wxFLAGS_MEMBER(wxSL_INVERSE)
wxEND_FLAGS( wxSliderStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxSlider, wxControl, "wx/slider.h")

wxBEGIN_PROPERTIES_TABLE(wxSlider)
    wxEVENT_RANGE_PROPERTY( Scroll, wxEVT_SCROLL_TOP, wxEVT_SCROLL_CHANGED, wxScrollEvent )
    wxEVENT_PROPERTY( Updated, wxEVT_SLIDER, wxCommandEvent )

    wxPROPERTY( Value, int, SetValue, GetValue, 0, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))
    wxPROPERTY( Minimum, int, SetMin, GetMin, 0, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))
    wxPROPERTY( Maximum, int, SetMax, GetMax, 0, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))
    wxPROPERTY( PageSize, int, SetPageSize, GetLineSize, 1, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))
    wxPROPERTY( LineSize, int, SetLineSize, GetLineSize, 1, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))
    wxPROPERTY( ThumbLength, int, SetThumbLength, GetThumbLength, 1, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))

    wxPROPERTY_FLAGS( WindowStyle, wxSliderStyle, long, SetWindowStyleFlag, \
                      GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                      wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxSlider)

wxCONSTRUCTOR_8( wxSlider, wxWindow*, Parent, wxWindowID, Id, int, Value, \
                 int, Minimum, int, Maximum, wxPoint, Position, wxSize, Size, \
                 long, WindowStyle )

#endif // wxUSE_SLIDER
