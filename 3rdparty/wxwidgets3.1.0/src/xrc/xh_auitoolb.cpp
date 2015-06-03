/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_toolb.cpp
// Purpose:     XRC resource for wxAuiToolBar
// Author:      Vaclav Slavik
// Created:     2000/08/11
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_AUI

#include "wx/bitmap.h"
#include "wx/xml/xml.h"

#ifndef WX_PRECOMP
    #include "wx/frame.h"
    #include "wx/log.h"
    #include "wx/toolbar.h"
#endif

#include "wx/xrc/xh_auitoolb.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxAuiToolBarXmlHandler, wxXmlResourceHandler);

wxAuiToolBarXmlHandler::wxAuiToolBarXmlHandler()
    : wxXmlResourceHandler()
    , m_isInside(false)
    , m_toolbar(NULL)
{
    XRC_ADD_STYLE(wxAUI_TB_TEXT);
    XRC_ADD_STYLE(wxAUI_TB_NO_TOOLTIPS);
    XRC_ADD_STYLE(wxAUI_TB_NO_AUTORESIZE);
    XRC_ADD_STYLE(wxAUI_TB_GRIPPER);
    XRC_ADD_STYLE(wxAUI_TB_OVERFLOW);
    XRC_ADD_STYLE(wxAUI_TB_VERTICAL);
    XRC_ADD_STYLE(wxAUI_TB_HORZ_LAYOUT);
    XRC_ADD_STYLE(wxAUI_TB_HORIZONTAL);
    XRC_ADD_STYLE(wxAUI_TB_PLAIN_BACKGROUND);
    XRC_ADD_STYLE(wxAUI_TB_HORZ_TEXT);

    AddWindowStyles();
}

wxObject *wxAuiToolBarXmlHandler::DoCreateResource()
{
    if (m_class == wxS("tool"))
    {
        if ( !m_toolbar )
        {
            ReportError("tool only allowed inside a wxAuiToolBar");
            return NULL;
        }

        wxItemKind kind = wxITEM_NORMAL;
        if (GetBool(wxS("radio")))
            kind = wxITEM_RADIO;

        if (GetBool(wxS("toggle")))
        {
            if ( kind != wxITEM_NORMAL )
            {
                ReportParamError
                (
                    "toggle",
                    "tool can't have both <radio> and <toggle> properties"
                );
            }

            kind = wxITEM_CHECK;
        }
#if wxUSE_MENUS
        // check whether we have dropdown tag inside
        wxMenu *menu = NULL; // menu for drop down items
        wxXmlNode * const nodeDropdown = GetParamNode("dropdown");
        if ( nodeDropdown )
        {
            // also check for the menu specified inside dropdown (it is
            // optional and may be absent for e.g. dynamically-created
            // menus)
            wxXmlNode * const nodeMenu = GetNodeChildren(nodeDropdown);
            if ( nodeMenu )
            {
                wxObject *res = CreateResFromNode(nodeMenu, NULL);
                menu = wxDynamicCast(res, wxMenu);
                if ( !menu )
                {
                    ReportError
                    (
                        nodeMenu,
                        "drop-down tool contents can only be a wxMenu"
                    );
                }

                if ( GetNodeNext(nodeMenu) )
                {
                    ReportError
                    (
                        GetNodeNext(nodeMenu),
                        "unexpected extra contents under drop-down tool"
                    );
                }
            }
        }
#endif
        wxAuiToolBarItem * const tool =
            m_toolbar->AddTool
                       (
                          GetID(),
                          GetText(wxS("label")),
                          GetBitmap(wxS("bitmap"), wxART_TOOLBAR, m_toolSize),
                          GetBitmap(wxS("bitmap2"), wxART_TOOLBAR, m_toolSize),
                          kind,
                          GetText(wxS("tooltip")),
                          GetText(wxS("longhelp")),
                          NULL
                       );

        if ( GetBool(wxS("disabled")) )
            m_toolbar->EnableTool(GetID(), false);

#if wxUSE_MENUS
        if (menu)
        {
            tool->SetHasDropDown(true);
            tool->SetUserData(m_menuHandler.RegisterMenu(m_toolbar, GetID(), menu));
        }
#endif

        return m_toolbar; // must return non-NULL
    }

    else if (m_class == wxS("separator") || m_class == wxS("space") || m_class == wxS("label"))
    {
        if ( !m_toolbar )
        {
            ReportError("separators only allowed inside wxAuiToolBar");
            return NULL;
        }

        if ( m_class == wxS("separator") )
            m_toolbar->AddSeparator();

        else if (m_class == wxS("space"))
        {
            // This may be a stretch spacer (the default) or a non-stretch one
            bool hasProportion = HasParam(wxS("proportion"));
            bool hasWidth = HasParam(wxS("width"));
            if (hasProportion && hasWidth)
            {
                ReportError("A space can't both stretch and have width");
                return NULL;
            }

            if (hasWidth)
            {
                m_toolbar->AddSpacer
                (
                    GetLong(wxS("width"))
                );
            }
            else
            {
                m_toolbar->AddStretchSpacer
                (
                    GetLong(wxS("proportion"), 1l)
                );
            }
        }

        else if (m_class == wxS("label"))
        {
            m_toolbar->AddLabel
            (
                GetID(),
                GetText(wxS("label")),
                GetLong(wxS("width"), -1l)
            );
        }

        return m_toolbar; // must return non-NULL
    }

    else /*<object class="wxAuiToolBar">*/
    {
        int style = GetStyle(wxS("style"), wxNO_BORDER | wxTB_HORIZONTAL);
#ifdef __WXMSW__
        if (!(style & wxNO_BORDER)) style |= wxNO_BORDER;
#endif

        XRC_MAKE_INSTANCE(toolbar, wxAuiToolBar)

        toolbar->Create(m_parentAsWindow,
                         GetID(),
                         GetPosition(),
                         GetSize(),
                         style);
        toolbar->SetName(GetName());
        SetupWindow(toolbar);

        m_toolSize = GetSize(wxS("bitmapsize"));
        if (!(m_toolSize == wxDefaultSize))
            toolbar->SetToolBitmapSize(m_toolSize);
        wxSize margins = GetSize(wxS("margins"));
        if (!(margins == wxDefaultSize))
            toolbar->SetMargins(margins.x, margins.y);
        long packing = GetLong(wxS("packing"), -1);
        if (packing != -1)
            toolbar->SetToolPacking(packing);
        long separation = GetLong(wxS("separation"), -1);
        if (separation != -1)
            toolbar->SetToolSeparation(separation);

        wxXmlNode *children_node = GetParamNode(wxS("object"));
        if (!children_node)
           children_node = GetParamNode(wxS("object_ref"));

        if (children_node == NULL) return toolbar;

        m_isInside = true;
        m_toolbar = toolbar;

        wxXmlNode *n = children_node;

        while (n)
        {
            if (IsObjectNode(n))
            {
                wxObject *created = CreateResFromNode(n, toolbar, NULL);
                wxControl *control = wxDynamicCast(created, wxControl);
                if (!IsOfClass(n, wxS("tool")) &&
                    !IsOfClass(n, wxS("separator")) &&
                    !IsOfClass(n, wxS("label")) &&
                    !IsOfClass(n, wxS("space")) &&
                    control != NULL)
                    toolbar->AddControl(control);
            }
            n = GetNodeNext(n);
        }

        m_isInside = false;
        m_toolbar = NULL;

        toolbar->Realize();

        return toolbar;
    }
}

bool wxAuiToolBarXmlHandler::CanHandle(wxXmlNode *node)
{
    return ((!m_isInside && IsOfClass(node, wxS("wxAuiToolBar"))) ||
            (m_isInside && IsOfClass(node, wxS("tool"))) ||
            (m_isInside && IsOfClass(node, wxS("label"))) ||
            (m_isInside && IsOfClass(node, wxS("space"))) ||
            (m_isInside && IsOfClass(node, wxS("separator"))));
}

void wxAuiToolBarXmlHandler::MenuHandler::OnDropDown(wxAuiToolBarEvent& event)
{
    if (event.IsDropDownClicked())
    {
        wxAuiToolBar *toobar = wxDynamicCast(event.GetEventObject(), wxAuiToolBar);
        if (toobar != NULL)
        {
            wxAuiToolBarItem *item = toobar->FindTool(event.GetId());
            if (item != NULL)
            {
                wxMenu * const menu = m_menus[item->GetUserData()];
                if (menu != NULL)
                {
                    wxRect rect = item->GetSizerItem()->GetRect();
                    toobar->PopupMenu(menu, rect.GetRight() - 10, rect.GetBottom());
                }
            }
        }
    }
    else
    {
        event.Skip();
    }
}

unsigned
wxAuiToolBarXmlHandler::MenuHandler::RegisterMenu(wxAuiToolBar *toolbar,
                                                  int id,
                                                  wxMenu *menu)
 {
    m_menus.push_back(menu);
    toolbar->Bind(wxEVT_COMMAND_AUITOOLBAR_TOOL_DROPDOWN,
                  &wxAuiToolBarXmlHandler::MenuHandler::OnDropDown,
                  this,
                  id);

    return m_menus.size() - 1;
}

#endif // wxUSE_XRC && wxUSE_AUI
