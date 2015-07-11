/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/commandlinkbuttong.cpp
// Purpose:     wxGenericCommandLinkButton
// Author:      Rickard Westerlund
// Created:     2010-06-23
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

#if wxUSE_COMMANDLINKBUTTON

#include "wx/commandlinkbutton.h"
#include "wx/artprov.h"

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxCommandLinkButton, wxButton, "wx/commandlinkbutton.h")

wxDEFINE_FLAGS( wxCommandLinkButtonStyle )
wxBEGIN_FLAGS( wxCommandLinkButtonStyle )
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

wxFLAGS_MEMBER(wxBU_AUTODRAW)
wxFLAGS_MEMBER(wxBU_LEFT)
wxFLAGS_MEMBER(wxBU_RIGHT)
wxFLAGS_MEMBER(wxBU_TOP)
wxFLAGS_MEMBER(wxBU_BOTTOM)
wxEND_FLAGS( wxCommandLinkButtonStyle )

wxBEGIN_PROPERTIES_TABLE(wxCommandLinkButton)
wxPROPERTY( MainLabel, wxString, SetMainLabel, GetMainLabel, wxString(), \
           0 /*flags*/, wxT("The main label"), wxT("group") )

wxPROPERTY( Note, wxString, SetNote, GetNote, wxString(), \
           0 /*flags*/, wxT("The link URL"), wxT("group") )
wxPROPERTY_FLAGS( WindowStyle, wxCommandLinkButtonStyle, long, SetWindowStyleFlag, \
                 GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/,     \
                 wxT("The link style"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxCommandLinkButton)

wxCONSTRUCTOR_7( wxCommandLinkButton, wxWindow*, Parent, wxWindowID, Id, wxString, \
                MainLabel, wxString, Note, wxPoint, Position, wxSize, Size, long, WindowStyle )

// ----------------------------------------------------------------------------
// Generic command link button
// ----------------------------------------------------------------------------

bool wxGenericCommandLinkButton::Create(wxWindow *parent,
                                        wxWindowID id,
                                        const wxString& mainLabel,
                                        const wxString& note,
                                        const wxPoint& pos,
                                        const wxSize& size,
                                        long style,
                                        const wxValidator& validator,
                                        const wxString& name)
{
    if ( !wxButton::Create(parent,
                           id,
                           mainLabel + '\n' + note,
                           pos,
                           size,
                           style,
                           validator,
                           name) )
        return false;

    if ( !HasNativeBitmap() )
        SetDefaultBitmap();

    return true;

}

void wxGenericCommandLinkButton::SetDefaultBitmap()
{
    SetBitmap(wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_BUTTON));
}

#endif // wxUSE_COMMANDLINKBUTTON
