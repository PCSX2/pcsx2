/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/choiccmn.cpp
// Purpose:     common (to all ports) wxChoice functions
// Author:      Vadim Zeitlin
// Modified by:
// Created:     26.07.99
// Copyright:   (c) wxWidgets team
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

#if wxUSE_CHOICE

#include "wx/choice.h"

#include "wx/private/textmeasure.h"

#ifndef WX_PRECOMP
#endif

const char wxChoiceNameStr[] = "choice";


wxDEFINE_FLAGS( wxChoiceStyle )
wxBEGIN_FLAGS( wxChoiceStyle )
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

wxEND_FLAGS( wxChoiceStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxChoice, wxControl, "wx/choice.h");

wxBEGIN_PROPERTIES_TABLE(wxChoice)
wxEVENT_PROPERTY( Select, wxEVT_CHOICE, wxCommandEvent )

wxPROPERTY( Font, wxFont, SetFont, GetFont , wxEMPTY_PARAMETER_VALUE, \
           0 /*flags*/, wxT("Helpstring"), wxT("group"))
wxPROPERTY_COLLECTION( Choices, wxArrayString, wxString, AppendString, \
                      GetStrings, 0 /*flags*/, wxT("Helpstring"), wxT("group"))
wxPROPERTY( Selection,int, SetSelection, GetSelection, wxEMPTY_PARAMETER_VALUE, \
           0 /*flags*/, wxT("Helpstring"), wxT("group"))

/*
 TODO PROPERTIES
 selection (long)
 content (list)
 item
 */

wxPROPERTY_FLAGS( WindowStyle, wxChoiceStyle, long, SetWindowStyleFlag, \
                 GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                 wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxChoice)

wxCONSTRUCTOR_4( wxChoice, wxWindow*, Parent, wxWindowID, Id, \
                wxPoint, Position, wxSize, Size )

// ============================================================================
// implementation
// ============================================================================

wxChoiceBase::~wxChoiceBase()
{
    // this destructor is required for Darwin
}

wxSize wxChoiceBase::DoGetBestSize() const
{
    // a reasonable width for an empty choice list
    wxSize best(80, -1);

    const unsigned int nItems = GetCount();
    if ( nItems > 0 )
    {
        wxTextMeasure txm(this);
        best.x = txm.GetLargestStringExtent(GetStrings()).x;
    }

    return best;
}

// ----------------------------------------------------------------------------
// misc
// ----------------------------------------------------------------------------

void wxChoiceBase::Command(wxCommandEvent& event)
{
    SetSelection(event.GetInt());
    (void)GetEventHandler()->ProcessEvent(event);
}

#endif // wxUSE_CHOICE
