/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_treebk.cpp
// Purpose:     XRC resource handler for wxTreebook
// Author:      Evgeniy Tarassov
// Created:     2005/09/28
// Copyright:   (c) 2005 TT-Solutions <vadim@tt-solutions.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_TREEBOOK

#include "wx/xrc/xh_treebk.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif

#include "wx/treebook.h"
#include "wx/imaglist.h"

#include "wx/xml/xml.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxTreebookXmlHandler, wxXmlResourceHandler);

wxTreebookXmlHandler::wxTreebookXmlHandler()
                    : wxXmlResourceHandler(),
                      m_tbk(NULL),
                      m_isInside(false)
{
    XRC_ADD_STYLE(wxBK_DEFAULT);
    XRC_ADD_STYLE(wxBK_TOP);
    XRC_ADD_STYLE(wxBK_BOTTOM);
    XRC_ADD_STYLE(wxBK_LEFT);
    XRC_ADD_STYLE(wxBK_RIGHT);

    AddWindowStyles();
}

bool wxTreebookXmlHandler::CanHandle(wxXmlNode *node)
{
    return ((!m_isInside && IsOfClass(node, wxT("wxTreebook"))) ||
            (m_isInside && IsOfClass(node, wxT("treebookpage"))));
}


wxObject *wxTreebookXmlHandler::DoCreateResource()
{
    if (m_class == wxT("wxTreebook"))
    {
        XRC_MAKE_INSTANCE(tbk, wxTreebook)

        tbk->Create(m_parentAsWindow,
                    GetID(),
                    GetPosition(), GetSize(),
                    GetStyle(wxT("style")),
                    GetName());

        wxImageList *imagelist = GetImageList();
        if ( imagelist )
            tbk->AssignImageList(imagelist);

        wxTreebook * old_par = m_tbk;
        m_tbk = tbk;

        bool old_ins = m_isInside;
        m_isInside = true;

        wxArrayTbkPageIndexes old_treeContext = m_treeContext;
        m_treeContext.Clear();

        CreateChildren(m_tbk, true/*only this handler*/);

        wxXmlNode *node = GetParamNode("object");
        int pageIndex = 0;
        for (unsigned int i = 0; i < m_tbk->GetPageCount(); i++)
        {
            if ( m_tbk->GetPage(i) )
            {
                wxXmlNode *child = node->GetChildren();
                while (child)
                {
                    if (child->GetName() == "expanded" && child->GetNodeContent() == "1")
                        m_tbk->ExpandNode(pageIndex, true);

                    child = child->GetNext();
                }
                pageIndex++;
            }
        }

        m_treeContext = old_treeContext;
        m_isInside = old_ins;
        m_tbk = old_par;

        return tbk;
    }

//    else ( m_class == wxT("treebookpage") )
    wxXmlNode *n = GetParamNode(wxT("object"));
    wxWindow *wnd = NULL;

    if ( !n )
        n = GetParamNode(wxT("object_ref"));

    if (n)
    {
        bool old_ins = m_isInside;
        m_isInside = false;
        wxObject *item = CreateResFromNode(n, m_tbk, NULL);
        m_isInside = old_ins;
        wnd = wxDynamicCast(item, wxWindow);

        if (wnd == NULL && item != NULL)
        {
            ReportError(n, "treebookpage child must be a window");
        }
    }

    size_t depth = GetLong( wxT("depth") );

    if( depth <= m_treeContext.GetCount() )
    {
        // first prepare the icon
        int imgIndex = wxNOT_FOUND;
        if ( HasParam(wxT("bitmap")) )
        {
            wxBitmap bmp = GetBitmap(wxT("bitmap"), wxART_OTHER);
            wxImageList *imgList = m_tbk->GetImageList();
            if ( imgList == NULL )
            {
                imgList = new wxImageList( bmp.GetWidth(), bmp.GetHeight() );
                m_tbk->AssignImageList( imgList );
            }
            imgIndex = imgList->Add(bmp);
        }
        else if ( HasParam(wxT("image")) )
        {
            if ( m_tbk->GetImageList() )
            {
                imgIndex = GetLong(wxT("image"));
            }
            else // image without image list?
            {
                ReportError(n, "image can only be used in conjunction "
                               "with imagelist");
            }
        }

        // then add the page to the corresponding parent
        if( depth < m_treeContext.GetCount() )
            m_treeContext.RemoveAt(depth, m_treeContext.GetCount() - depth );
        if( depth == 0)
        {
            m_tbk->AddPage(wnd,
                GetText(wxT("label")), GetBool(wxT("selected")), imgIndex);
        }
        else
        {
            m_tbk->InsertSubPage(m_treeContext.Item(depth - 1), wnd,
                GetText(wxT("label")), GetBool(wxT("selected")), imgIndex);
        }

        m_treeContext.Add( m_tbk->GetPageCount() - 1);

    }
    else
    {
        ReportParamError("depth", "invalid depth");
    }

    return wnd;
}

#endif // wxUSE_XRC && wxUSE_TREEBOOK
