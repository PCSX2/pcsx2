/////////////////////////////////////////////////////////////////////////////
// Name:        webview.cpp
// Purpose:     Common interface and events for web view component
// Author:      Marianne Gagnon
// Copyright:   (c) 2010 Marianne Gagnon, 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_WEBVIEW

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#include "wx/webview.h"

#if defined(__WXOSX_COCOA__) || defined(__WXOSX_CARBON__)
#include "wx/osx/webview_webkit.h"
#elif defined(__WXGTK__)
#include "wx/gtk/webview_webkit.h"
#elif defined(__WXMSW__)
#include "wx/msw/webview_ie.h"
#endif

// DLL options compatibility check:
#include "wx/app.h"
WX_CHECK_BUILD_OPTIONS("wxWEBVIEW")

extern WXDLLIMPEXP_DATA_WEBVIEW(const char) wxWebViewNameStr[] = "wxWebView";
extern WXDLLIMPEXP_DATA_WEBVIEW(const char) wxWebViewDefaultURLStr[] = "about:blank";
extern WXDLLIMPEXP_DATA_WEBVIEW(const char) wxWebViewBackendIE[] = "wxWebViewIE";
extern WXDLLIMPEXP_DATA_WEBVIEW(const char) wxWebViewBackendWebKit[] = "wxWebViewWebKit";

#ifdef __WXMSW__
extern WXDLLIMPEXP_DATA_WEBVIEW(const char) wxWebViewBackendDefault[] = "wxWebViewIE";
#else
extern WXDLLIMPEXP_DATA_WEBVIEW(const char) wxWebViewBackendDefault[] = "wxWebViewWebKit";
#endif

wxIMPLEMENT_ABSTRACT_CLASS(wxWebView, wxControl);
wxIMPLEMENT_DYNAMIC_CLASS(wxWebViewEvent, wxCommandEvent);

wxDEFINE_EVENT( wxEVT_WEBVIEW_NAVIGATING, wxWebViewEvent );
wxDEFINE_EVENT( wxEVT_WEBVIEW_NAVIGATED, wxWebViewEvent );
wxDEFINE_EVENT( wxEVT_WEBVIEW_LOADED, wxWebViewEvent );
wxDEFINE_EVENT( wxEVT_WEBVIEW_ERROR, wxWebViewEvent );
wxDEFINE_EVENT( wxEVT_WEBVIEW_NEWWINDOW, wxWebViewEvent );
wxDEFINE_EVENT( wxEVT_WEBVIEW_TITLE_CHANGED, wxWebViewEvent );

wxStringWebViewFactoryMap wxWebView::m_factoryMap;

// static
wxWebView* wxWebView::New(const wxString& backend)
{
    wxStringWebViewFactoryMap::iterator iter = FindFactory(backend);

    if(iter == m_factoryMap.end())
        return NULL;
    else
        return (*iter).second->Create();
}

// static
wxWebView* wxWebView::New(wxWindow* parent, wxWindowID id, const wxString& url,
                          const wxPoint& pos, const wxSize& size, 
                          const wxString& backend, long style, 
                          const wxString& name)
{
    wxStringWebViewFactoryMap::iterator iter = FindFactory(backend);

    if(iter == m_factoryMap.end())
        return NULL;
    else
        return (*iter).second->Create(parent, id, url, pos, size, style, name);

}

// static
void wxWebView::RegisterFactory(const wxString& backend, 
                                wxSharedPtr<wxWebViewFactory> factory)
{
    m_factoryMap[backend] = factory;
}

// static 
wxStringWebViewFactoryMap::iterator wxWebView::FindFactory(const wxString &backend)
{
    // Initialise the map, it checks internally for existing factories
    InitFactoryMap();

    return m_factoryMap.find(backend);
}
 
// static
void wxWebView::InitFactoryMap()
{
#ifdef __WXMSW__
    if(m_factoryMap.find(wxWebViewBackendIE) == m_factoryMap.end())
        RegisterFactory(wxWebViewBackendIE, wxSharedPtr<wxWebViewFactory>
                                                   (new wxWebViewFactoryIE));
#else
    if(m_factoryMap.find(wxWebViewBackendWebKit) == m_factoryMap.end())
        RegisterFactory(wxWebViewBackendWebKit, wxSharedPtr<wxWebViewFactory>
                                                       (new wxWebViewFactoryWebKit));
#endif
}

#endif // wxUSE_WEBVIEW
