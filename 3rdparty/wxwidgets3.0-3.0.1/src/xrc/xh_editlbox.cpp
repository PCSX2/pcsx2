///////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_editlbox.cpp
// Purpose:     implementation of wxEditableListBox XRC handler
// Author:      Vadim Zeitlin
// Created:     2009-06-04
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_EDITABLELISTBOX

#ifndef WX_PRECOMP
    #include "wx/intl.h"
#endif // WX_PRECOMP

#include "wx/editlbox.h"
#include "wx/xrc/xh_editlbox.h"

#include "wx/xml/xml.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

namespace
{

const char * const EDITLBOX_CLASS_NAME = "wxEditableListBox";
const char * const EDITLBOX_ITEM_NAME = "item";

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxEditableListBoxXmlHandler, wxXmlResourceHandler)

wxEditableListBoxXmlHandler::wxEditableListBoxXmlHandler()
{
    m_insideBox = false;

    XRC_ADD_STYLE(wxEL_ALLOW_NEW);
    XRC_ADD_STYLE(wxEL_ALLOW_EDIT);
    XRC_ADD_STYLE(wxEL_ALLOW_DELETE);
    XRC_ADD_STYLE(wxEL_NO_REORDER);

    AddWindowStyles();
}

wxObject *wxEditableListBoxXmlHandler::DoCreateResource()
{
    if ( m_class == EDITLBOX_CLASS_NAME )
    {
        // create the control itself
        XRC_MAKE_INSTANCE(control, wxEditableListBox)

        control->Create
                 (
                      m_parentAsWindow,
                      GetID(),
                      GetText("label"),
                      GetPosition(),
                      GetSize(),
                      GetStyle(),
                      GetName()
                 );

        SetupWindow(control);

        // if any items are given, add them to the control
        wxXmlNode * const contents = GetParamNode("content");
        if ( contents )
        {
            m_insideBox = true;
            CreateChildrenPrivately(NULL, contents);
            m_insideBox = false;

            control->SetStrings(m_items);
            m_items.clear();
        }

        return control;
    }
    else if ( m_insideBox && m_node->GetName() == EDITLBOX_ITEM_NAME )
    {
        wxString str = GetNodeContent(m_node);
        if ( m_resource->GetFlags() & wxXRC_USE_LOCALE )
            str = wxGetTranslation(str, m_resource->GetDomain());
        m_items.push_back(str);

        return NULL;
    }
    else
    {
        ReportError("Unexpected node inside wxEditableListBox");
        return NULL;
    }
}

bool wxEditableListBoxXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, EDITLBOX_CLASS_NAME) ||
                (m_insideBox && node->GetName() == EDITLBOX_ITEM_NAME);
}


#endif // wxUSE_XRC && wxUSE_EDITABLELISTBOX
