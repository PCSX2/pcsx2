/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_mdi.cpp
// Purpose:     XRC resource for wxMDI
// Author:      David M. Falkinder & Vaclav Slavik
// Created:     14/02/2005
// Copyright:   (c) 2005 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_MDI

#include "wx/xrc/xh_mdi.h"
#include "wx/mdi.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/dialog.h" // to get wxDEFAULT_DIALOG_STYLE
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxMdiXmlHandler, wxXmlResourceHandler);

wxMdiXmlHandler::wxMdiXmlHandler() : wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxSTAY_ON_TOP);
    XRC_ADD_STYLE(wxCAPTION);
    XRC_ADD_STYLE(wxDEFAULT_DIALOG_STYLE);
    XRC_ADD_STYLE(wxDEFAULT_FRAME_STYLE);
    XRC_ADD_STYLE(wxSYSTEM_MENU);
    XRC_ADD_STYLE(wxRESIZE_BORDER);
    XRC_ADD_STYLE(wxCLOSE_BOX);
    XRC_ADD_STYLE(wxFRAME_NO_TASKBAR);
    XRC_ADD_STYLE(wxFRAME_SHAPED);
    XRC_ADD_STYLE(wxFRAME_TOOL_WINDOW);
    XRC_ADD_STYLE(wxFRAME_FLOAT_ON_PARENT);
    XRC_ADD_STYLE(wxMAXIMIZE_BOX);
    XRC_ADD_STYLE(wxMINIMIZE_BOX);
    XRC_ADD_STYLE(wxSTAY_ON_TOP);
    XRC_ADD_STYLE(wxTAB_TRAVERSAL);
    XRC_ADD_STYLE(wxWS_EX_VALIDATE_RECURSIVELY);
    XRC_ADD_STYLE(wxFRAME_EX_METAL);

    XRC_ADD_STYLE(wxHSCROLL);
    XRC_ADD_STYLE(wxVSCROLL);
    XRC_ADD_STYLE(wxMAXIMIZE);
    XRC_ADD_STYLE(wxFRAME_NO_WINDOW_MENU);

    AddWindowStyles();
}

wxWindow *wxMdiXmlHandler::CreateFrame()
{
    if (m_class == wxT("wxMDIParentFrame"))
    {
        XRC_MAKE_INSTANCE(frame, wxMDIParentFrame);

        frame->Create(m_parentAsWindow,
                      GetID(),
                      GetText(wxT("title")),
                      wxDefaultPosition, wxDefaultSize,
                      GetStyle(wxT("style"),
                               wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL),
                      GetName());
        return frame;
    }
    else // wxMDIChildFrame
    {
        wxMDIParentFrame *mdiParent = wxDynamicCast(m_parent, wxMDIParentFrame);

        if ( !mdiParent )
        {
            ReportError("parent of wxMDIChildFrame must be wxMDIParentFrame");
            return NULL;
        }

        XRC_MAKE_INSTANCE(frame, wxMDIChildFrame);

        frame->Create(mdiParent,
                      GetID(),
                      GetText(wxT("title")),
                      wxDefaultPosition, wxDefaultSize,
                      GetStyle(wxT("style"), wxDEFAULT_FRAME_STYLE),
                      GetName());

        return frame;
    }
}

wxObject *wxMdiXmlHandler::DoCreateResource()
{
    wxWindow *frame = CreateFrame();

    if (HasParam(wxT("size")))
        frame->SetClientSize(GetSize());
    if (HasParam(wxT("pos")))
        frame->Move(GetPosition());
    if (HasParam(wxT("icon")))
    {
        wxFrame* f = wxDynamicCast(frame, wxFrame);
        if (f)
            f->SetIcons(GetIconBundle(wxT("icon"), wxART_FRAME_ICON));
    }

    SetupWindow(frame);

    CreateChildren(frame);

    if (GetBool(wxT("centered"), false))
        frame->Centre();

    return frame;
}

bool wxMdiXmlHandler::CanHandle(wxXmlNode *node)
{
    return (IsOfClass(node, wxT("wxMDIParentFrame")) ||
            IsOfClass(node, wxT("wxMDIChildFrame")));
}

#endif // wxUSE_XRC && wxUSE_MDI
