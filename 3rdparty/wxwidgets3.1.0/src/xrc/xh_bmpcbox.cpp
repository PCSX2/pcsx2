/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_bmpcbox.cpp
// Purpose:     XRC resource for wxBitmapComboBox
// Author:      Jaakko Salli
// Created:     Sep-10-2006
// Copyright:   (c) 2006 Jaakko Salli
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_BITMAPCOMBOBOX

#include "wx/xrc/xh_bmpcbox.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
#endif

#include "wx/bmpcbox.h"

#include "wx/xml/xml.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxBitmapComboBoxXmlHandler, wxXmlResourceHandler);

wxBitmapComboBoxXmlHandler::wxBitmapComboBoxXmlHandler()
                     :wxXmlResourceHandler()
                     ,m_combobox(NULL)
                     ,m_isInside(false)
{
    XRC_ADD_STYLE(wxCB_SORT);
    XRC_ADD_STYLE(wxCB_READONLY);
    AddWindowStyles();
}

wxObject *wxBitmapComboBoxXmlHandler::DoCreateResource()
{
    if (m_class == wxT("ownerdrawnitem"))
    {
        if ( !m_combobox )
        {
            ReportError("ownerdrawnitem only allowed within a wxBitmapComboBox");
            return NULL;
        }

        m_combobox->Append(GetText(wxT("text")),
                           GetBitmap(wxT("bitmap")));

        return m_combobox;
    }
    else /*if( m_class == wxT("wxBitmapComboBox"))*/
    {
        // find the selection
        long selection = GetLong( wxT("selection"), -1 );

        XRC_MAKE_INSTANCE(control, wxBitmapComboBox)

        control->Create(m_parentAsWindow,
                        GetID(),
                        GetText(wxT("value")),
                        GetPosition(), GetSize(),
                        0,
                        NULL,
                        GetStyle(),
                        wxDefaultValidator,
                        GetName());

        m_isInside = true;
        m_combobox = control;

        wxXmlNode *children_node = GetParamNode(wxT("object"));

        wxXmlNode *n = children_node;

        while (n)
        {
            if ((n->GetType() == wxXML_ELEMENT_NODE) &&
                (n->GetName() == wxT("object")))
            {
                CreateResFromNode(n, control, NULL);
            }
            n = n->GetNext();
        }

        m_isInside = false;
        m_combobox = NULL;

        if (selection != -1)
            control->SetSelection(selection);

        SetupWindow(control);

        return control;
    }
}

bool wxBitmapComboBoxXmlHandler::CanHandle(wxXmlNode *node)
{
    return ((!m_isInside && IsOfClass(node, wxT("wxBitmapComboBox"))) ||
            (m_isInside && IsOfClass(node, wxT("ownerdrawnitem"))));
}

#endif // wxUSE_XRC && wxUSE_BITMAPCOMBOBOX
