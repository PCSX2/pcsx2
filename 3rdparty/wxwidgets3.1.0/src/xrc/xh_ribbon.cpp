/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_ribbon.cpp
// Purpose:     XML resource handler for wxRibbon related classes
// Author:      Armel Asselin
// Created:     2010-04-23
// Copyright:   (c) 2010 Armel Asselin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_RIBBON

#include "wx/xrc/xh_ribbon.h"

#include "wx/ribbon/bar.h"
#include "wx/ribbon/buttonbar.h"
#include "wx/ribbon/gallery.h"

#include "wx/scopeguard.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
#endif

// Ribbon bars can contain only pages which are usually panels but may contain
// any wxWindow.
//
// Panels are usually for wxRibbonControls but may as well contain any
// wxWindow.
//
// Galleries are wxRibbonControl and simply contain bitmaps with IDs.
//
// Button bars are wxRibbonControl and contain buttons (normal/dropdown/mixed),
// with id/bitmap/label/short help.

wxIMPLEMENT_DYNAMIC_CLASS(wxRibbonXmlHandler, wxXmlResourceHandler);

wxRibbonXmlHandler::wxRibbonXmlHandler()
    : wxXmlResourceHandler(),
      m_isInside(NULL)
{
    XRC_ADD_STYLE(wxRIBBON_BAR_SHOW_PAGE_LABELS);
    XRC_ADD_STYLE(wxRIBBON_BAR_SHOW_PAGE_ICONS);
    XRC_ADD_STYLE(wxRIBBON_BAR_FLOW_HORIZONTAL);
    XRC_ADD_STYLE(wxRIBBON_BAR_FLOW_VERTICAL);
    XRC_ADD_STYLE(wxRIBBON_BAR_SHOW_PANEL_EXT_BUTTONS);
    XRC_ADD_STYLE(wxRIBBON_BAR_SHOW_PANEL_MINIMISE_BUTTONS);
    XRC_ADD_STYLE(wxRIBBON_BAR_ALWAYS_SHOW_TABS);
    XRC_ADD_STYLE(wxRIBBON_BAR_DEFAULT_STYLE);
    XRC_ADD_STYLE(wxRIBBON_BAR_FOLDBAR_STYLE);
}

wxObject *wxRibbonXmlHandler::DoCreateResource()
{
    if (m_class == wxT("button"))
        return Handle_button();
    if (m_class == wxT("wxRibbonButtonBar"))
        return Handle_buttonbar();
    else if (m_class == wxT("item"))
        return Handle_galleryitem();
    else if (m_class == wxT("wxRibbonGallery"))
        return Handle_gallery();
    else if (m_class == wxT("wxRibbonPanel") || m_class == wxT("panel"))
        return Handle_panel();
    else if (m_class == wxT("wxRibbonPage") || m_class == wxT("page"))
        return Handle_page();
    else if (m_class == wxT("wxRibbonBar"))
        return Handle_bar();
    else
        return Handle_control ();
}

bool wxRibbonXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsRibbonControl(node) ||
           (m_isInside == &wxRibbonButtonBar::ms_classInfo &&
                IsOfClass(node, wxT("button"))) ||
           (m_isInside == &wxRibbonBar::ms_classInfo &&
                IsOfClass(node, wxT("page"))) ||
           (m_isInside == &wxRibbonPage::ms_classInfo &&
                IsOfClass(node, wxT("panel"))) ||
           (m_isInside == &wxRibbonGallery::ms_classInfo &&
                IsOfClass(node, wxT("item")));
}

bool wxRibbonXmlHandler::IsRibbonControl (wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxRibbonBar")) ||
           IsOfClass(node, wxT("wxRibbonButtonBar")) ||
           IsOfClass(node, wxT("wxRibbonPage")) ||
           IsOfClass(node, wxT("wxRibbonPanel")) ||
           IsOfClass(node, wxT("wxRibbonGallery")) ||
           IsOfClass(node, wxT("wxRibbonControl"));
}

void wxRibbonXmlHandler::Handle_RibbonArtProvider(wxRibbonControl *control)
{
    wxString provider = GetText("art-provider", false);

    if (provider == "default" || provider.IsEmpty())
        control->SetArtProvider(new wxRibbonDefaultArtProvider);
    else if (provider.CmpNoCase("aui") == 0)
        control->SetArtProvider(new wxRibbonAUIArtProvider);
    else if (provider.CmpNoCase("msw") == 0)
        control->SetArtProvider(new wxRibbonMSWArtProvider);
    else
        ReportError("invalid ribbon art provider");
}

wxObject* wxRibbonXmlHandler::Handle_buttonbar()
{
    XRC_MAKE_INSTANCE (buttonBar, wxRibbonButtonBar);

    if (!buttonBar->Create (wxDynamicCast(m_parent, wxWindow), GetID(),
            GetPosition(), GetSize(), GetStyle()))
    {
        ReportError("could not create ribbon panel");
    }
    else
    {
        const wxClassInfo* const wasInside = m_isInside;
        wxON_BLOCK_EXIT_SET(m_isInside, wasInside);
        m_isInside = &wxRibbonButtonBar::ms_classInfo;

        CreateChildren (buttonBar, true);

        buttonBar->Realize();
    }

    return buttonBar;
}

wxObject* wxRibbonXmlHandler::Handle_button()
{
    wxRibbonButtonBar *buttonBar = wxStaticCast(m_parent, wxRibbonButtonBar);

    wxRibbonButtonKind  kind = wxRIBBON_BUTTON_NORMAL;

    if (GetBool(wxT("hybrid")))
        kind = wxRIBBON_BUTTON_HYBRID;

    // FIXME: The code below uses wxXmlNode directly but this can't be done
    //        in the ribbon library code as it would force it to always link
    //        with the xml library. Disable it for now but the real solution
    //        would be to virtualize GetChildren() and GetNext() methods via
    //        wxXmlResourceHandler, just as we already do for many others.
    //
    // FIXME: If re-enabling, don't forget to update the docs and RELAG NG schema!
#if 0 // wxUSE_MENUS
    // check whether we have dropdown tag inside
    wxMenu *menu = NULL; // menu for drop down items
    wxXmlNode * const nodeDropdown = GetParamNode("dropdown");
    if ( nodeDropdown )
    {
        if (kind == wxRIBBON_BUTTON_NORMAL)
            kind = wxRIBBON_BUTTON_DROPDOWN;

        // also check for the menu specified inside dropdown (it is
        // optional and may be absent for e.g. dynamically-created
        // menus)
        wxXmlNode * const nodeMenu = nodeDropdown->GetChildren();
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

            if ( nodeMenu->GetNext() )
            {
                ReportError
                (
                    nodeMenu->GetNext(),
                    "unexpected extra contents under drop-down tool"
                );
            }
        }
    }
#endif // wxUSE_MENUS

    if (!buttonBar->AddButton(GetID(),
                              GetText("label"),
                              GetBitmap ("bitmap"),
                              GetBitmap ("small-bitmap"),
                              GetBitmap ("disabled-bitmap"),
                              GetBitmap ("small-disabled-bitmap"),
                              kind,
                              GetText("help")))
    {
        ReportError ("could not create button");
    }

    if ( GetBool(wxT("disabled")) )
            buttonBar->EnableButton(GetID(), false);

    return NULL; // nothing to return
}

wxObject* wxRibbonXmlHandler::Handle_control()
{
    wxRibbonControl *control = wxDynamicCast (m_instance, wxRibbonControl);

    if (!m_instance)
        ReportError("wxRibbonControl must be subclassed");
    else if (!control)
        ReportError("controls must derive from wxRibbonControl");

    control->Create(wxDynamicCast(m_parent, wxWindow), GetID(),
                    GetPosition(), GetSize(), GetStyle());

    return m_instance;
}

wxObject* wxRibbonXmlHandler::Handle_page()
{
    XRC_MAKE_INSTANCE (ribbonPage, wxRibbonPage);

    if (!ribbonPage->Create (wxDynamicCast(m_parent, wxRibbonBar), GetID(),
            GetText ("label"), GetBitmap ("icon"), GetStyle()))
    {
        ReportError("could not create ribbon page");
    }
    else
    {
        const wxClassInfo* const wasInside = m_isInside;
        wxON_BLOCK_EXIT_SET(m_isInside, wasInside);
        m_isInside = &wxRibbonPage::ms_classInfo;

        CreateChildren (ribbonPage);

        ribbonPage->Realize();
    }

    return ribbonPage;
}

wxObject* wxRibbonXmlHandler::Handle_gallery()
{
    XRC_MAKE_INSTANCE (ribbonGallery, wxRibbonGallery);

    if (!ribbonGallery->Create (wxDynamicCast(m_parent, wxWindow), GetID(),
            GetPosition(), GetSize(), GetStyle()))
    {
        ReportError("could not create ribbon gallery");
    }
    else
    {
        const wxClassInfo* const wasInside = m_isInside;
        wxON_BLOCK_EXIT_SET(m_isInside, wasInside);
        m_isInside = &wxRibbonGallery::ms_classInfo;

        CreateChildren (ribbonGallery);

        ribbonGallery->Realize();
    }

    return ribbonGallery;
}

wxObject* wxRibbonXmlHandler::Handle_galleryitem()
{
    wxRibbonGallery *gallery = wxStaticCast(m_parent, wxRibbonGallery);
    wxCHECK (gallery, NULL);

    gallery->Append (GetBitmap(), GetID());

    return NULL; // nothing to return
}

wxObject* wxRibbonXmlHandler::Handle_panel()
{
    XRC_MAKE_INSTANCE (ribbonPanel, wxRibbonPanel);

    if (!ribbonPanel->Create (wxDynamicCast(m_parent, wxWindow), GetID(),
            GetText ("label"), GetBitmap ("icon"), GetPosition(), GetSize(),
            GetStyle("style", wxRIBBON_PANEL_DEFAULT_STYLE)))
    {
        ReportError("could not create ribbon panel");
    }
    else
    {
        CreateChildren (ribbonPanel);

        ribbonPanel->Realize();
    }

    return ribbonPanel;
}

wxObject* wxRibbonXmlHandler::Handle_bar()
{
    XRC_MAKE_INSTANCE (ribbonBar, wxRibbonBar);

    Handle_RibbonArtProvider (ribbonBar);

    if ( !ribbonBar->Create(wxDynamicCast(m_parent, wxWindow),
                            GetID(),
                            GetPosition(),
                            GetSize(),
                            GetStyle("style", wxRIBBON_BAR_DEFAULT_STYLE)) )
    {
        ReportError ("could not create ribbonbar");
    }
    else
    {
        // Currently the art provider style must be explicitly set to the
        // ribbon style too.
        ribbonBar->GetArtProvider()
            ->SetFlags(GetStyle("style", wxRIBBON_BAR_DEFAULT_STYLE));

        const wxClassInfo* const wasInside = m_isInside;
        wxON_BLOCK_EXIT_SET(m_isInside, wasInside);
        m_isInside = &wxRibbonBar::ms_classInfo;

        CreateChildren (ribbonBar, true);

        ribbonBar->Realize();
    }

    return ribbonBar;
}

#endif // wxUSE_XRC && wxUSE_RIBBON
