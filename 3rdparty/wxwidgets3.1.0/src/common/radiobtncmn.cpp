/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/radiobtncmn.cpp
// Purpose:     wxRadioButton common code
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
#endif

extern WXDLLEXPORT_DATA(const char) wxRadioButtonNameStr[] = "radioButton";
extern WXDLLEXPORT_DATA(const char) wxBitmapRadioButtonNameStr[] = "radioButton";

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxRadioButtonStyle )
wxBEGIN_FLAGS( wxRadioButtonStyle )
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

    wxFLAGS_MEMBER(wxRB_GROUP)
wxEND_FLAGS( wxRadioButtonStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxRadioButton, wxControl, "wx/radiobut.h");

wxBEGIN_PROPERTIES_TABLE(wxRadioButton)
    wxEVENT_PROPERTY( Click, wxEVT_RADIOBUTTON, wxCommandEvent )
    wxPROPERTY( Font, wxFont, SetFont, GetFont , wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group"))
    wxPROPERTY( Label,wxString, SetLabel, GetLabel, wxString(), 0 /*flags*/, \
                wxT("Helpstring"), wxT("group") )
    wxPROPERTY( Value,bool, SetValue, GetValue, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                wxT("Helpstring"), wxT("group") )
    wxPROPERTY_FLAGS( WindowStyle, wxRadioButtonStyle, long, SetWindowStyleFlag, \
                      GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                      wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxRadioButton)

wxCONSTRUCTOR_6( wxRadioButton, wxWindow*, Parent, wxWindowID, Id, \
                 wxString, Label, wxPoint, Position, wxSize, Size, long, WindowStyle )


#endif // wxUSE_RADIOBTN
