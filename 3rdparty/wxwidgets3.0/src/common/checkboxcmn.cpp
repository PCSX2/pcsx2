/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/checkboxcmn.cpp
// Purpose:     wxCheckBox common code
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

#if wxUSE_CHECKBOX

#include "wx/checkbox.h"

extern WXDLLEXPORT_DATA(const char) wxCheckBoxNameStr[] = "check";

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxCheckBoxStyle )
wxBEGIN_FLAGS( wxCheckBoxStyle )
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
    wxFLAGS_MEMBER(wxNO_BORDER)

    // standard window styles
    wxFLAGS_MEMBER(wxTAB_TRAVERSAL)
    wxFLAGS_MEMBER(wxCLIP_CHILDREN)
    wxFLAGS_MEMBER(wxTRANSPARENT_WINDOW)
    wxFLAGS_MEMBER(wxWANTS_CHARS)
    wxFLAGS_MEMBER(wxNO_FULL_REPAINT_ON_RESIZE)
    wxFLAGS_MEMBER(wxALWAYS_SHOW_SB )
    wxFLAGS_MEMBER(wxVSCROLL)
    wxFLAGS_MEMBER(wxHSCROLL)

wxEND_FLAGS( wxCheckBoxStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxCheckBox, wxControl, "wx/checkbox.h")

wxBEGIN_PROPERTIES_TABLE(wxCheckBox)
    wxEVENT_PROPERTY( Click, wxEVT_CHECKBOX, wxCommandEvent )

    wxPROPERTY( Font, wxFont, SetFont, GetFont, wxEMPTY_PARAMETER_VALUE, \
                0 /*flags*/, wxT("Helpstring"), wxT("group"))
    wxPROPERTY( Label,wxString, SetLabel, GetLabel, wxString(), \
                0 /*flags*/, wxT("Helpstring"), wxT("group"))
    wxPROPERTY( Value,bool, SetValue, GetValue, wxEMPTY_PARAMETER_VALUE, \
                0 /*flags*/, wxT("Helpstring"), wxT("group"))

    wxPROPERTY_FLAGS( WindowStyle, wxCheckBoxStyle, long, SetWindowStyleFlag, \
                      GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                      wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxCheckBox)

wxCONSTRUCTOR_6( wxCheckBox, wxWindow*, Parent, wxWindowID, Id, \
                 wxString, Label, wxPoint, Position, wxSize, Size, long, WindowStyle )


#endif // wxUSE_CHECKBOX
