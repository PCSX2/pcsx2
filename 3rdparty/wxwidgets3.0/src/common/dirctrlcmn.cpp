/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/dirctrlcmn.cpp
// Purpose:     wxGenericDirCtrl common code
// Author:      Harm van der Heijden, Robert Roebling, Julian Smart
// Modified by:
// Created:     12/12/98
// Copyright:   (c) Harm van der Heijden, Robert Roebling and Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_DIRDLG || wxUSE_FILEDLG

#include "wx/generic/dirctrlg.h"

extern WXDLLEXPORT_DATA(const char) wxDirDialogNameStr[] = "wxDirCtrl";
extern WXDLLEXPORT_DATA(const char) wxDirDialogDefaultFolderStr[] = "/";
extern WXDLLEXPORT_DATA(const char) wxDirSelectorPromptStr[] = "Select a directory";

//-----------------------------------------------------------------------------
// XTI
//-----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxGenericDirCtrlStyle )
wxBEGIN_FLAGS( wxGenericDirCtrlStyle )
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

    wxFLAGS_MEMBER(wxDIRCTRL_DIR_ONLY)
    wxFLAGS_MEMBER(wxDIRCTRL_3D_INTERNAL)
    wxFLAGS_MEMBER(wxDIRCTRL_SELECT_FIRST)
    wxFLAGS_MEMBER(wxDIRCTRL_SHOW_FILTERS)
wxEND_FLAGS( wxGenericDirCtrlStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxGenericDirCtrl, wxControl, "wx/dirctrl.h")

wxBEGIN_PROPERTIES_TABLE(wxGenericDirCtrl)
    wxHIDE_PROPERTY( Children )

    wxPROPERTY( DefaultPath, wxString, SetDefaultPath, GetDefaultPath, \
                wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, wxT("Helpstring"), wxT("group"))
    wxPROPERTY( Filter, wxString, SetFilter, GetFilter, wxEMPTY_PARAMETER_VALUE, \
                0 /*flags*/, wxT("Helpstring"), wxT("group") )
    wxPROPERTY( DefaultFilter, int, SetFilterIndex, GetFilterIndex, \
                wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, wxT("Helpstring"), wxT("group") )

    wxPROPERTY_FLAGS( WindowStyle, wxGenericDirCtrlStyle, long, SetWindowStyleFlag, \
                      GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0, wxT("Helpstring"), \
                      wxT("group") )
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxGenericDirCtrl)

wxCONSTRUCTOR_8( wxGenericDirCtrl, wxWindow*, Parent, wxWindowID, Id, \
                 wxString, DefaultPath, wxPoint, Position, wxSize, Size, \
                 long, WindowStyle, wxString, Filter, int, DefaultFilter )

#endif // wxUSE_DIRDLG || wxUSE_FILEDLG
