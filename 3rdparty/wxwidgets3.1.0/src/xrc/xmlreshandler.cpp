/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xmlreshandler.cpp
// Purpose:     XML resource handler
// Author:      Steven Lamerton
// Created:     2011/01/26
// Copyright:   (c) 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC

#include "wx/xrc/xmlreshandler.h"

wxIMPLEMENT_ABSTRACT_CLASS(wxXmlResourceHandler, wxObject);

wxXmlResourceHandlerImplBase* wxXmlResourceHandler::GetImpl() const
{
    if ( !m_impl )
    {
        wxFAIL_MSG(wxT("SetImpl() must have been called!"));
    }

    return m_impl;
}

void wxXmlResourceHandler::AddStyle(const wxString& name, int value)
{
    m_styleNames.Add(name);
    m_styleValues.Add(value);
}

void wxXmlResourceHandler::AddWindowStyles()
{
    XRC_ADD_STYLE(wxCLIP_CHILDREN);

    // the border styles all have the old and new names, recognize both for now
    XRC_ADD_STYLE(wxSIMPLE_BORDER); XRC_ADD_STYLE(wxBORDER_SIMPLE);
    XRC_ADD_STYLE(wxSUNKEN_BORDER); XRC_ADD_STYLE(wxBORDER_SUNKEN);
    XRC_ADD_STYLE(wxDOUBLE_BORDER); XRC_ADD_STYLE(wxBORDER_DOUBLE); // deprecated
    XRC_ADD_STYLE(wxBORDER_THEME);
    XRC_ADD_STYLE(wxRAISED_BORDER); XRC_ADD_STYLE(wxBORDER_RAISED);
    XRC_ADD_STYLE(wxSTATIC_BORDER); XRC_ADD_STYLE(wxBORDER_STATIC);
    XRC_ADD_STYLE(wxNO_BORDER);     XRC_ADD_STYLE(wxBORDER_NONE);
    XRC_ADD_STYLE(wxBORDER_DEFAULT);

    XRC_ADD_STYLE(wxTRANSPARENT_WINDOW);
    XRC_ADD_STYLE(wxWANTS_CHARS);
    XRC_ADD_STYLE(wxTAB_TRAVERSAL);
    XRC_ADD_STYLE(wxNO_FULL_REPAINT_ON_RESIZE);
    XRC_ADD_STYLE(wxFULL_REPAINT_ON_RESIZE);
    XRC_ADD_STYLE(wxVSCROLL);
    XRC_ADD_STYLE(wxHSCROLL);
    XRC_ADD_STYLE(wxALWAYS_SHOW_SB);
    XRC_ADD_STYLE(wxWS_EX_BLOCK_EVENTS);
    XRC_ADD_STYLE(wxWS_EX_VALIDATE_RECURSIVELY);
    XRC_ADD_STYLE(wxWS_EX_TRANSIENT);
    XRC_ADD_STYLE(wxWS_EX_CONTEXTHELP);
    XRC_ADD_STYLE(wxWS_EX_PROCESS_IDLE);
    XRC_ADD_STYLE(wxWS_EX_PROCESS_UI_UPDATES);
}

#endif // wxUSE_XRC
