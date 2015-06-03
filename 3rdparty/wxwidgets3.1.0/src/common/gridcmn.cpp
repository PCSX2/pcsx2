///////////////////////////////////////////////////////////////////////////
// Name:        src/common/gridcmn.cpp
// Purpose:     wxGrid common code
// Author:      Michael Bedward (based on code by Julian Smart, Robin Dunn)
// Modified by: Robin Dunn, Vadim Zeitlin, Santiago Palacios
// Created:     1/08/1999
// Copyright:   (c) Michael Bedward (mbedward@ozemail.com.au)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_GRID

#include "wx/grid.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
    #include "wx/log.h"
    #include "wx/textctrl.h"
    #include "wx/checkbox.h"
    #include "wx/combobox.h"
    #include "wx/valtext.h"
    #include "wx/intl.h"
    #include "wx/math.h"
    #include "wx/listbox.h"
#endif

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxGridStyle )
wxBEGIN_FLAGS( wxGridStyle )
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
    wxFLAGS_MEMBER(wxALWAYS_SHOW_SB)
    wxFLAGS_MEMBER(wxVSCROLL)
    wxFLAGS_MEMBER(wxHSCROLL)
wxEND_FLAGS( wxGridStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxGrid, wxScrolledWindow, "wx/grid.h");

wxBEGIN_PROPERTIES_TABLE(wxGrid)
    wxHIDE_PROPERTY( Children )
    wxPROPERTY_FLAGS( WindowStyle, wxGridStyle, long, SetWindowStyleFlag, \
                      GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                      wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxGrid)

wxCONSTRUCTOR_5( wxGrid, wxWindow*, Parent, wxWindowID, Id, wxPoint, Position, \
                 wxSize, Size, long, WindowStyle )

/*
 TODO : Expose more information of a list's layout, etc. via appropriate objects (e.g., NotebookPageInfo)
*/

#endif // wxUSE_GRID
