/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_auinotbk.cpp
// Purpose:     XML resource handler for wxAuiNotebook
// Author:      Steve Lamerton
// Created:     2009-06-12
// Copyright:   (c) 2009 Steve Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_AUI

#include "wx/xrc/xh_auinotbk.h"
#include "wx/aui/auibook.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxAuiNotebookXmlHandler, wxXmlResourceHandler);

wxAuiNotebookXmlHandler::wxAuiNotebookXmlHandler()
    : wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxAUI_NB_DEFAULT_STYLE);
    XRC_ADD_STYLE(wxAUI_NB_TAB_SPLIT);
    XRC_ADD_STYLE(wxAUI_NB_TAB_MOVE);
    XRC_ADD_STYLE(wxAUI_NB_TAB_EXTERNAL_MOVE);
    XRC_ADD_STYLE(wxAUI_NB_TAB_FIXED_WIDTH);
    XRC_ADD_STYLE(wxAUI_NB_SCROLL_BUTTONS);
    XRC_ADD_STYLE(wxAUI_NB_WINDOWLIST_BUTTON);
    XRC_ADD_STYLE(wxAUI_NB_CLOSE_BUTTON);
    XRC_ADD_STYLE(wxAUI_NB_CLOSE_ON_ACTIVE_TAB);
    XRC_ADD_STYLE(wxAUI_NB_CLOSE_ON_ALL_TABS);
    XRC_ADD_STYLE(wxAUI_NB_TOP);
    XRC_ADD_STYLE(wxAUI_NB_BOTTOM);

    AddWindowStyles();
}

wxObject *wxAuiNotebookXmlHandler::DoCreateResource()
{
    if (m_class == wxT("notebookpage"))
    {
        wxXmlNode *anb = GetParamNode(wxT("object"));

        if (!anb)
            anb = GetParamNode(wxT("object_ref"));

        if (anb)
        {
            bool old_ins = m_isInside;
            m_isInside = false;
            wxObject *item = CreateResFromNode(anb, m_notebook, NULL);
            m_isInside = old_ins;
            wxWindow *wnd = wxDynamicCast(item, wxWindow);

            if (wnd)
            {
                if ( HasParam(wxT("bitmap")) )
                {
                    m_notebook->AddPage(wnd,
                                        GetText(wxT("label")),
                                        GetBool(wxT("selected")),
                                        GetBitmap(wxT("bitmap"), wxART_OTHER));
                }
                else
                {
                    m_notebook->AddPage(wnd,
                                        GetText(wxT("label")),
                                        GetBool(wxT("selected")));
                }
            }
            else
            {
                ReportError(anb, "notebookpage child must be a window");
            }
            return wnd;
        }
        else
        {
            ReportError("notebookpage must have a window child");
            return NULL;
        }
    }
    else
    {
        XRC_MAKE_INSTANCE(anb, wxAuiNotebook)

        anb->Create(m_parentAsWindow,
                    GetID(),
                    GetPosition(),
                    GetSize(),
                    GetStyle(wxT("style")));

        SetupWindow(anb);

        wxAuiNotebook *old_par = m_notebook;
        m_notebook = anb;
        bool old_ins = m_isInside;
        m_isInside = true;
        CreateChildren(m_notebook, true/*only this handler*/);
        m_isInside = old_ins;
        m_notebook = old_par;

        return anb;
    }
}

bool wxAuiNotebookXmlHandler::CanHandle(wxXmlNode *node)
{
    return ((!m_isInside && IsOfClass(node, wxT("wxAuiNotebook"))) ||
            (m_isInside && IsOfClass(node, wxT("notebookpage"))));
}

#endif // wxUSE_XRC && wxUSE_ANIMATIONCTRL
