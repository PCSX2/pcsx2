/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_radbx.cpp
// Purpose:     XRC resource for wxRadioBox
// Author:      Bob Mitchell
// Created:     2000/03/21
// Copyright:   (c) 2000 Bob Mitchell and Verant Interactive
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_RADIOBOX

#include "wx/xrc/xh_radbx.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/radiobox.h"
#endif

#include "wx/xml/xml.h"

IMPLEMENT_DYNAMIC_CLASS(wxRadioBoxXmlHandler, wxXmlResourceHandler)

wxRadioBoxXmlHandler::wxRadioBoxXmlHandler()
: wxXmlResourceHandler(), m_insideBox(false)
{
    XRC_ADD_STYLE(wxRA_SPECIFY_COLS);
    XRC_ADD_STYLE(wxRA_HORIZONTAL);
    XRC_ADD_STYLE(wxRA_SPECIFY_ROWS);
    XRC_ADD_STYLE(wxRA_VERTICAL);
    AddWindowStyles();
}

wxObject *wxRadioBoxXmlHandler::DoCreateResource()
{
    if ( m_class == wxT("wxRadioBox"))
    {
        // find the selection
        long selection = GetLong( wxT("selection"), -1 );

        // need to build the list of strings from children
        m_insideBox = true;
        CreateChildrenPrivately( NULL, GetParamNode(wxT("content")));

        XRC_MAKE_INSTANCE(control, wxRadioBox)

        control->Create(m_parentAsWindow,
                        GetID(),
                        GetText(wxT("label")),
                        GetPosition(), GetSize(),
                        m_labels,
                        GetLong(wxT("dimension"), 1),
                        GetStyle(),
                        wxDefaultValidator,
                        GetName());

        if (selection != -1)
            control->SetSelection(selection);

        SetupWindow(control);

        const unsigned count = m_labels.size();
        for( unsigned i = 0; i < count; i++ )
        {
#if wxUSE_TOOLTIPS
            if ( !m_tooltips[i].empty() )
                control->SetItemToolTip(i, m_tooltips[i]);
#endif // wxUSE_TOOLTIPS
#if wxUSE_HELP
            if ( m_helptextSpecified[i] )
                control->SetItemHelpText(i, m_helptexts[i]);
#endif // wxUSE_HELP

            if ( !m_isShown[i] )
                control->Show(i, false);
            if ( !m_isEnabled[i] )
                control->Enable(i, false);
        }


        // forget information about the items of this radiobox, we should start
        // afresh for the next one
        m_labels.clear();

#if wxUSE_TOOLTIPS
        m_tooltips.clear();
#endif // wxUSE_TOOLTIPS

#if wxUSE_HELP
        m_helptexts.clear();
        m_helptextSpecified.clear();
#endif // wxUSE_HELP

        m_isShown.clear();
        m_isEnabled.clear();

        return control;
    }
    else // inside the radiobox element
    {
        // we handle handle <item>Label</item> constructs here, and the item
        // tag can have tooltip, helptext, enabled and hidden attributes

        wxString label = GetNodeContent(m_node);

        wxString tooltip;
        m_node->GetAttribute(wxT("tooltip"), &tooltip);

        wxString helptext;
        bool hasHelptext = m_node->GetAttribute(wxT("helptext"), &helptext);

        if (m_resource->GetFlags() & wxXRC_USE_LOCALE)
        {
            label = wxGetTranslation(label, m_resource->GetDomain());
            if ( !tooltip.empty() )
                tooltip = wxGetTranslation(tooltip, m_resource->GetDomain());
            if ( hasHelptext )
                helptext = wxGetTranslation(helptext, m_resource->GetDomain());
        }

        m_labels.push_back(label);
#if wxUSE_TOOLTIPS
        m_tooltips.push_back(tooltip);
#endif // wxUSE_TOOLTIPS
#if wxUSE_HELP
        m_helptexts.push_back(helptext);
        m_helptextSpecified.push_back(hasHelptext);
#endif // wxUSE_HELP
        m_isEnabled.push_back(GetBoolAttr("enabled", 1));
        m_isShown.push_back(!GetBoolAttr("hidden", 0));

        return NULL;
    }

}

bool wxRadioBoxXmlHandler::CanHandle(wxXmlNode *node)
{
    return (IsOfClass(node, wxT("wxRadioBox")) ||
           (m_insideBox && node->GetName() == wxT("item")));
}

#endif // wxUSE_XRC && wxUSE_RADIOBOX
