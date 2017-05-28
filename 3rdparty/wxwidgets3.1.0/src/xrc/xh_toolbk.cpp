/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_toolbk.cpp
// Purpose:     XRC resource for wxToolbook
// Author:      Andrea Zanellato
// Created:     2009/12/12
// Copyright:   (c) 2010 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_TOOLBOOK

#include "wx/xrc/xh_toolbk.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/sizer.h"
#endif

#include "wx/toolbook.h"
#include "wx/imaglist.h"

#include "wx/xml/xml.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxToolbookXmlHandler, wxXmlResourceHandler);

wxToolbookXmlHandler::wxToolbookXmlHandler()
                     :wxXmlResourceHandler(),
                      m_isInside(false),
                      m_toolbook(NULL)
{
    XRC_ADD_STYLE(wxBK_DEFAULT);
    XRC_ADD_STYLE(wxBK_TOP);
    XRC_ADD_STYLE(wxBK_BOTTOM);
    XRC_ADD_STYLE(wxBK_LEFT);
    XRC_ADD_STYLE(wxBK_RIGHT);

    XRC_ADD_STYLE(wxTBK_BUTTONBAR);
    XRC_ADD_STYLE(wxTBK_HORZ_LAYOUT);

    AddWindowStyles();
}

wxObject *wxToolbookXmlHandler::DoCreateResource()
{
    if (m_class == wxT("toolbookpage"))
    {
        wxXmlNode *n = GetParamNode(wxT("object"));

        if ( !n )
            n = GetParamNode(wxT("object_ref"));

        if (n)
        {
            bool old_ins = m_isInside;
            m_isInside = false;
            wxObject *item = CreateResFromNode(n, m_toolbook, NULL);
            m_isInside = old_ins;
            wxWindow *wnd = wxDynamicCast(item, wxWindow);

            if (wnd)
            {
                int imgId = -1;

                if ( HasParam(wxT("bitmap")) )
                {
                    wxBitmap bmp = GetBitmap(wxT("bitmap"), wxART_OTHER);
                    wxImageList *imgList = m_toolbook->GetImageList();
                    if ( imgList == NULL )
                    {
                        imgList = new wxImageList( bmp.GetWidth(), bmp.GetHeight() );
                        m_toolbook->AssignImageList( imgList );
                    }
                    imgId = imgList->Add(bmp);
                }
                else if ( HasParam(wxT("image")) )
                {
                    if ( m_toolbook->GetImageList() )
                    {
                        imgId = (int)GetLong(wxT("image"));
                    }
                    else // image without image list?
                    {
                        ReportError(n, "image can only be used in conjunction "
                                       "with imagelist");
                    }
                }

                m_toolbook->AddPage(wnd, GetText(wxT("label")),
                        GetBool(wxT("selected")), imgId );
            }
            else
            {
                ReportError(n, "toolbookpage child must be a window");
            }
            return wnd;
        }
        else
        {
            ReportError("toolbookpage must have a window child");
            return NULL;
        }
    }

    else
    {
        XRC_MAKE_INSTANCE(nb, wxToolbook)

        nb->Create( m_parentAsWindow,
                    GetID(),
                    GetPosition(), GetSize(),
                    GetStyle(wxT("style")),
                    GetName() );

        wxImageList *imagelist = GetImageList();
        if ( imagelist )
            nb->AssignImageList(imagelist);

        wxToolbook *old_par = m_toolbook;
        m_toolbook = nb;
        bool old_ins = m_isInside;
        m_isInside = true;
        CreateChildren(m_toolbook, true/*only this handler*/);
        m_isInside = old_ins;
        m_toolbook = old_par;

        return nb;
    }
}

bool wxToolbookXmlHandler::CanHandle(wxXmlNode *node)
{
    return ((!m_isInside && IsOfClass(node, wxT("wxToolbook"))) ||
            (m_isInside && IsOfClass(node, wxT("toolbookpage"))));
}

#endif // wxUSE_XRC && wxUSE_TOOLBOOK
