/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/webview_ie.cpp
// Purpose:     wxMSW wxWebViewIE class implementation for web view component
// Author:      Marianne Gagnon
// Copyright:   (c) 2010 Marianne Gagnon, 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#include "wx/msw/webview_ie.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_IE

#include <olectl.h>
#include <oleidl.h>
#include <exdispid.h>
#include <exdisp.h>
#include <mshtml.h>
#include "wx/msw/registry.h"
#include "wx/msw/missing.h"
#include "wx/filesys.h"
#include "wx/dynlib.h"
#include "wx/scopeguard.h"

#include <initguid.h>
#include <wininet.h>

/* These GUID definitions are our own implementation to support interfaces
 * normally in urlmon.h. See include/wx/msw/webview_ie.h
 */

namespace {

DEFINE_GUID(wxIID_IInternetProtocolRoot,0x79eac9e3,0xbaf9,0x11ce,0x8c,0x82,0,0xaa,0,0x4b,0xa9,0xb);
DEFINE_GUID(wxIID_IInternetProtocol,0x79eac9e4,0xbaf9,0x11ce,0x8c,0x82,0,0xaa,0,0x4b,0xa9,0xb);
DEFINE_GUID(wxIID_IDocHostUIHandler, 0xbd3f23c0, 0xd43e, 0x11cf, 0x89, 0x3b, 0x00, 0xaa, 0x00, 0xbd, 0xce, 0x1a);
DEFINE_GUID(wxIID_IHTMLElement2,0x3050f434,0x98b5,0x11cf,0xbb,0x82,0,0xaa,0,0xbd,0xce,0x0b);
DEFINE_GUID(wxIID_IMarkupServices,0x3050f4a0,0x98b5,0x11cf,0xbb,0x82,0,0xaa,0,0xbd,0xce,0x0b);
DEFINE_GUID(wxIID_IMarkupContainer,0x3050f5f9,0x98b5,0x11cf,0xbb,0x82,0,0xaa,0,0xbd,0xce,0x0b);

enum //Internal find flags
{
    wxWEBVIEW_FIND_ADD_POINTERS =      0x0001,
    wxWEBVIEW_FIND_REMOVE_HIGHLIGHT =  0x0002
};

}

//Convenience function for error conversion
#define WX_ERROR_CASE(error, wxerror) \
        case error: \
            event.SetString(#error); \
            event.SetInt(wxerror); \
            break;

wxIMPLEMENT_DYNAMIC_CLASS(wxWebViewIE, wxWebView);

BEGIN_EVENT_TABLE(wxWebViewIE, wxControl)
    EVT_ACTIVEX(wxID_ANY, wxWebViewIE::onActiveXEvent)
    EVT_ERASE_BACKGROUND(wxWebViewIE::onEraseBg)
END_EVENT_TABLE()

bool wxWebViewIE::Create(wxWindow* parent,
           wxWindowID id,
           const wxString& url,
           const wxPoint& pos,
           const wxSize& size,
           long style,
           const wxString& name)
{
    if (!wxControl::Create(parent, id, pos, size, style,
                           wxDefaultValidator, name))
    {
        return false;
    }

    m_webBrowser = NULL;
    m_isBusy = false;
    m_historyLoadingFromList = false;
    m_historyEnabled = true;
    m_historyPosition = -1;
    m_zoomType = wxWEBVIEW_ZOOM_TYPE_TEXT;
    FindClear();

    if (::CoCreateInstance(CLSID_WebBrowser, NULL,
                           CLSCTX_INPROC_SERVER, // CLSCTX_INPROC,
                           IID_IWebBrowser2 , (void**)&m_webBrowser) != 0)
    {
        wxLogError("Failed to initialize IE, CoCreateInstance returned an error");
        return false;
    }

    m_ie.SetDispatchPtr(m_webBrowser); // wxAutomationObject will release itself

    m_webBrowser->put_RegisterAsBrowser(VARIANT_TRUE);
    m_webBrowser->put_RegisterAsDropTarget(VARIANT_TRUE);

    m_uiHandler = new DocHostUIHandler(this);

    m_container = new wxIEContainer(this, IID_IWebBrowser2, m_webBrowser, m_uiHandler);

    EnableControlFeature(21 /* FEATURE_DISABLE_NAVIGATION_SOUNDS */);

    LoadURL(url);
    return true;
}

wxWebViewIE::~wxWebViewIE()
{
    wxDynamicLibrary urlMon(wxT("urlmon.dll"));
    if(urlMon.HasSymbol(wxT("CoInternetGetSession")))
    {
        typedef HRESULT (WINAPI *CoInternetGetSession_t)(DWORD,
                                                         wxIInternetSession**,
                                                         DWORD);
        wxDYNLIB_FUNCTION(CoInternetGetSession_t, CoInternetGetSession, urlMon);

        wxIInternetSession* session;
        HRESULT res = (*pfnCoInternetGetSession)(0, &session, 0);
        if(FAILED(res))
        {
            wxFAIL_MSG("Could not retrive internet session");
        }

        for(unsigned int i = 0; i < m_factories.size(); i++)
        {
            session->UnregisterNameSpace(m_factories[i], 
                                        (m_factories[i]->GetName()).wc_str());
            m_factories[i]->Release();
        }
    }
    FindClear();
}

void wxWebViewIE::LoadURL(const wxString& url)
{
    m_ie.CallMethod("Navigate", wxConvertStringToOle(url));
}

namespace
{

// Simple RAII wrapper for accessing SAFEARRAY<VARIANT>. This class is not specific
// to wxWebView at all and could be extended to work for any type and extracted
// into a header later if it is ever needed elsewhere, but for now keep it here
// as it is only used in MakeOneElementVariantSafeArray() which, itself, is
// only used by DoSetPage() below.
class wxSafeArrayVariantAccessor
{
public:
    explicit wxSafeArrayVariantAccessor(SAFEARRAY* sa)
        : m_sa(sa),
          m_data(DoAccessData(sa))
    {
    }

    VARIANT* GetData() const
    {
        return m_data;
    }

    ~wxSafeArrayVariantAccessor()
    {
        if ( m_data )
            SafeArrayUnaccessData(m_sa);
    }

private:
    static VARIANT* DoAccessData(SAFEARRAY* sa)
    {
        VARIANT *param;
        HRESULT hr = SafeArrayAccessData(sa, (LPVOID*)&param);
        if ( FAILED(hr) )
        {
            wxLogLastError(wxT("SafeArrayAccessData"));
            return NULL;
        }

        return param;
    }

    SAFEARRAY* const m_sa;
    VARIANT* const m_data;

    wxDECLARE_NO_COPY_CLASS(wxSafeArrayVariantAccessor);
};

// Helper function: wrap the given string in a SAFEARRAY<VARIANT> of size 1.
SAFEARRAY* MakeOneElementVariantSafeArray(const wxString& str)
{
    SAFEARRAY* const sa = SafeArrayCreateVector(VT_VARIANT, 0, 1);
    if ( !sa )
    {
        wxLogLastError(wxT("SafeArrayCreateVector"));
        return NULL;
    }

    wxSafeArrayVariantAccessor access(sa);

    VARIANT* const param = access.GetData();
    if ( !param )
    {
        SafeArrayDestroy(sa);
        return NULL;
    }

    param->vt = VT_BSTR;
    param->bstrVal = SysAllocString(str.wc_str());

    return sa;
}

} // anonymous namespace

void wxWebViewIE::DoSetPage(const wxString& html, const wxString& baseUrl)
{
    {
        SAFEARRAY* const psaStrings = MakeOneElementVariantSafeArray(wxString());
        if ( !psaStrings )
            return;

        wxON_BLOCK_EXIT1(SafeArrayDestroy, psaStrings);

        wxCOMPtr<IHTMLDocument2> document(GetDocument());

        if(!document)
            return;

        document->write(psaStrings);
        document->close();
    }

    {
        SAFEARRAY* const psaStrings = MakeOneElementVariantSafeArray(html);

        if ( !psaStrings )
            return;

        wxON_BLOCK_EXIT1(SafeArrayDestroy, psaStrings);

        wxCOMPtr<IHTMLDocument2> document(GetDocument());

        if(!document)
            return;

        document->write(psaStrings);

        //We send the events when we are done to mimic webkit
        //Navigated event
        wxWebViewEvent event(wxEVT_WEBVIEW_NAVIGATED,
                             GetId(), baseUrl, "");
        event.SetEventObject(this);
        HandleWindowEvent(event);

        //Document complete event
        event.SetEventType(wxEVT_WEBVIEW_LOADED);
        event.SetEventObject(this);
        HandleWindowEvent(event);
    }
}

wxString wxWebViewIE::GetPageSource() const
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        wxCOMPtr<IHTMLElement> bodyTag;
        wxCOMPtr<IHTMLElement> htmlTag;
        wxString source;
        HRESULT hr = document->get_body(&bodyTag);
        if(SUCCEEDED(hr))
        {
            hr = bodyTag->get_parentElement(&htmlTag);
            if(SUCCEEDED(hr))
            {
                BSTR bstr;
                if ( htmlTag->get_outerHTML(&bstr) == S_OK )
                    source = wxString(bstr);
            }
        }
        return source;
    }
    else
    {
        return "";
    }
}

wxWebViewZoom wxWebViewIE::GetZoom() const
{
    switch( m_zoomType )
    {
        case wxWEBVIEW_ZOOM_TYPE_LAYOUT:
            return GetIEOpticalZoom();
        case wxWEBVIEW_ZOOM_TYPE_TEXT:
            return GetIETextZoom();
        default:
            wxFAIL;
    }

    //Dummy return to stop compiler warnings
    return wxWEBVIEW_ZOOM_MEDIUM;

}

void wxWebViewIE::SetZoom(wxWebViewZoom zoom)
{
    switch( m_zoomType )
    {
        case wxWEBVIEW_ZOOM_TYPE_LAYOUT:
            SetIEOpticalZoom(zoom);
            break;
        case wxWEBVIEW_ZOOM_TYPE_TEXT:
            SetIETextZoom(zoom);
            break;
        default:
            wxFAIL;
    }
}

void wxWebViewIE::SetIETextZoom(wxWebViewZoom level)
{
    //We do not use OLECMDID_OPTICAL_GETZOOMRANGE as the docs say the range
    //is 0 to 4 so the check is unnecessary, these match exactly with the
    //enum values
    VARIANT zoomVariant;
    VariantInit (&zoomVariant);
    V_VT(&zoomVariant) = VT_I4;
    V_I4(&zoomVariant) = level;

#if wxDEBUG_LEVEL
    HRESULT result =
#endif
            m_webBrowser->ExecWB(OLECMDID_ZOOM,
                                 OLECMDEXECOPT_DONTPROMPTUSER,
                                 &zoomVariant, NULL);
    wxASSERT(result == S_OK);
}

wxWebViewZoom wxWebViewIE::GetIETextZoom() const
{
    VARIANT zoomVariant;
    VariantInit (&zoomVariant);
    V_VT(&zoomVariant) = VT_I4;

#if wxDEBUG_LEVEL
    HRESULT result =
#endif
            m_webBrowser->ExecWB(OLECMDID_ZOOM,
                                 OLECMDEXECOPT_DONTPROMPTUSER,
                                 NULL, &zoomVariant);
    wxASSERT(result == S_OK);

    //We can safely cast here as we know that the range matches our enum
    return static_cast<wxWebViewZoom>(V_I4(&zoomVariant));
}

void wxWebViewIE::SetIEOpticalZoom(wxWebViewZoom level)
{
    //We do not use OLECMDID_OPTICAL_GETZOOMRANGE as the docs say the range
    //is 10 to 1000 so the check is unnecessary
    VARIANT zoomVariant;
    VariantInit (&zoomVariant);
    V_VT(&zoomVariant) = VT_I4;

    //We make a somewhat arbitray map here, taken from values used by webkit
    switch(level)
    {
        case wxWEBVIEW_ZOOM_TINY:
            V_I4(&zoomVariant) = 60;
            break;
        case wxWEBVIEW_ZOOM_SMALL:
            V_I4(&zoomVariant) = 80;
            break;
        case wxWEBVIEW_ZOOM_MEDIUM:
            V_I4(&zoomVariant) = 100;
            break;
        case wxWEBVIEW_ZOOM_LARGE:
            V_I4(&zoomVariant) = 130;
            break;
        case wxWEBVIEW_ZOOM_LARGEST:
            V_I4(&zoomVariant) = 160;
            break;
        default:
            wxFAIL;
    }

#if wxDEBUG_LEVEL
    HRESULT result =
#endif
            m_webBrowser->ExecWB((OLECMDID)63 /*OLECMDID_OPTICAL_ZOOM*/,
                                 OLECMDEXECOPT_DODEFAULT,
                                 &zoomVariant,
                                 NULL);
    wxASSERT(result == S_OK);
}

wxWebViewZoom wxWebViewIE::GetIEOpticalZoom() const
{
    VARIANT zoomVariant;
    VariantInit (&zoomVariant);
    V_VT(&zoomVariant) = VT_I4;

#if wxDEBUG_LEVEL
    HRESULT result =
#endif
            m_webBrowser->ExecWB((OLECMDID)63 /*OLECMDID_OPTICAL_ZOOM*/,
                                 OLECMDEXECOPT_DODEFAULT, NULL,
                                 &zoomVariant);
    wxASSERT(result == S_OK);

    const int zoom = V_I4(&zoomVariant);

    //We make a somewhat arbitray map here, taken from values used by webkit
    if (zoom <= 65)
    {
        return wxWEBVIEW_ZOOM_TINY;
    }
    else if (zoom > 65 && zoom <= 90)
    {
        return wxWEBVIEW_ZOOM_SMALL;
    }
    else if (zoom > 90 && zoom <= 115)
    {
        return wxWEBVIEW_ZOOM_MEDIUM;
    }
    else if (zoom > 115 && zoom <= 145)
    {
        return wxWEBVIEW_ZOOM_LARGE;
    }
    else /*if (zoom > 145) */ //Using else removes a compiler warning
    {
        return wxWEBVIEW_ZOOM_LARGEST;
    }
}

void wxWebViewIE::SetZoomType(wxWebViewZoomType type)
{
    m_zoomType = type;
}

wxWebViewZoomType wxWebViewIE::GetZoomType() const
{
    return m_zoomType;
}

bool wxWebViewIE::CanSetZoomType(wxWebViewZoomType type) const
{
    //IE 6 and below only support text zoom, so check the registry to see what
    //version we actually have
    wxRegKey key(wxRegKey::HKLM, "Software\\Microsoft\\Internet Explorer");
    wxString value;
    key.QueryValue("Version", value);

    long version = wxAtoi(value.Left(1));
    if(version <= 6 && type == wxWEBVIEW_ZOOM_TYPE_LAYOUT)
        return false;
    else
        return true;
}

void wxWebViewIE::Print()
{
    m_webBrowser->ExecWB(OLECMDID_PRINTPREVIEW,
                         OLECMDEXECOPT_DODEFAULT, NULL, NULL);
}

bool wxWebViewIE::CanGoBack() const
{
    if(m_historyEnabled)
        return m_historyPosition > 0;
    else
        return false;
}

bool wxWebViewIE::CanGoForward() const
{
    if(m_historyEnabled)
        return m_historyPosition != static_cast<int>(m_historyList.size()) - 1;
    else
        return false;
}

void wxWebViewIE::LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item)
{
    int pos = -1;
    for(unsigned int i = 0; i < m_historyList.size(); i++)
    {
        //We compare the actual pointers to find the correct item
        if(m_historyList[i].get() == item.get())
            pos = i;
    }
    wxASSERT_MSG(pos != static_cast<int>(m_historyList.size()),
                 "invalid history item");
    m_historyLoadingFromList = true;
    LoadURL(item->GetUrl());
    m_historyPosition = pos;
}

wxVector<wxSharedPtr<wxWebViewHistoryItem> > wxWebViewIE::GetBackwardHistory()
{
    wxVector<wxSharedPtr<wxWebViewHistoryItem> > backhist;
    //As we don't have std::copy or an iterator constructor in the wxwidgets
    //native vector we construct it by hand
    for(int i = 0; i < m_historyPosition; i++)
    {
        backhist.push_back(m_historyList[i]);
    }
    return backhist;
}

wxVector<wxSharedPtr<wxWebViewHistoryItem> > wxWebViewIE::GetForwardHistory()
{
    wxVector<wxSharedPtr<wxWebViewHistoryItem> > forwardhist;
    //As we don't have std::copy or an iterator constructor in the wxwidgets
    //native vector we construct it by hand
    for(int i = m_historyPosition + 1; i < static_cast<int>(m_historyList.size()); i++)
    {
        forwardhist.push_back(m_historyList[i]);
    }
    return forwardhist;
}

void wxWebViewIE::GoBack()
{
    LoadHistoryItem(m_historyList[m_historyPosition - 1]);
}

void wxWebViewIE::GoForward()
{
    LoadHistoryItem(m_historyList[m_historyPosition + 1]);
}

void wxWebViewIE::Stop()
{
    m_ie.CallMethod("Stop");
}

void wxWebViewIE::ClearHistory()
{
    m_historyList.clear();
    m_historyPosition = -1;
}

void wxWebViewIE::EnableHistory(bool enable)
{
    m_historyEnabled = enable;
    m_historyList.clear();
    m_historyPosition = -1;
}

void wxWebViewIE::Reload(wxWebViewReloadFlags flags)
{
    VARIANTARG level;
    VariantInit(&level);
    V_VT(&level) = VT_I2;

    switch(flags)
    {
        case wxWEBVIEW_RELOAD_DEFAULT:
            V_I2(&level) = REFRESH_NORMAL;
            break;
        case wxWEBVIEW_RELOAD_NO_CACHE:
            V_I2(&level) = REFRESH_COMPLETELY;
            break;
        default:
            wxFAIL_MSG("Unexpected reload type");
    }

    m_webBrowser->Refresh2(&level);
}

bool wxWebViewIE::IsOfflineMode()
{
    wxVariant out = m_ie.GetProperty("Offline");

    wxASSERT(out.GetType() == "bool");

    return out.GetBool();
}

void wxWebViewIE::SetOfflineMode(bool offline)
{
    // FIXME: the wxWidgets docs do not really document what the return
    //        parameter of PutProperty is
#if wxDEBUG_LEVEL
    const bool success =
#endif
            m_ie.PutProperty("Offline", (offline ?
                                         VARIANT_TRUE :
                                         VARIANT_FALSE));
    wxASSERT(success);
}

bool wxWebViewIE::IsBusy() const
{
    if (m_isBusy) return true;

    wxVariant out = m_ie.GetProperty("Busy");

    wxASSERT(out.GetType() == "bool");

    return out.GetBool();
}

wxString wxWebViewIE::GetCurrentURL() const
{
    wxVariant out = m_ie.GetProperty("LocationURL");

    wxASSERT(out.GetType() == "string");
    return out.GetString();
}

wxString wxWebViewIE::GetCurrentTitle() const
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    wxString s;
    if(document)
    {
        BSTR title;
        if ( document->get_nameProp(&title) == S_OK )
            s = title;
    }

    return s;
}

bool wxWebViewIE::CanCut() const
{
    return CanExecCommand("Cut");
}

bool wxWebViewIE::CanCopy() const
{
    return CanExecCommand("Copy");
}

bool wxWebViewIE::CanPaste() const
{
    return CanExecCommand("Paste");
}

void wxWebViewIE::Cut()
{
    ExecCommand("Cut");
}

void wxWebViewIE::Copy()
{
    ExecCommand("Copy");
}

void wxWebViewIE::Paste()
{
    ExecCommand("Paste");
}

bool wxWebViewIE::CanUndo() const
{
    return CanExecCommand("Undo");
}

bool wxWebViewIE::CanRedo() const
{
    return CanExecCommand("Redo");
}

void wxWebViewIE::Undo()
{
    ExecCommand("Undo");
}

void wxWebViewIE::Redo()
{
    ExecCommand("Redo");
}

long wxWebViewIE::Find(const wxString& text, int flags)
{
    //If the text is empty then we clear.
    if(text.IsEmpty())
    {
        ClearSelection();
        if(m_findFlags & wxWEBVIEW_FIND_HIGHLIGHT_RESULT)
        {
            FindInternal(m_findText, (m_findFlags &~ wxWEBVIEW_FIND_HIGHLIGHT_RESULT), wxWEBVIEW_FIND_REMOVE_HIGHLIGHT);
        }
        FindClear();
        return wxNOT_FOUND;
    }
    //Have we done this search before?
    if(m_findText == text)
    {
        //Just do a highlight?
        if((flags & wxWEBVIEW_FIND_HIGHLIGHT_RESULT) != (m_findFlags & wxWEBVIEW_FIND_HIGHLIGHT_RESULT))
        {
            m_findFlags = flags;
            if(!m_findPointers.empty())
            {
                FindInternal(m_findText, m_findFlags, ((flags & wxWEBVIEW_FIND_HIGHLIGHT_RESULT) == 0 ? wxWEBVIEW_FIND_REMOVE_HIGHLIGHT : 0));
            }
            return m_findPosition;
        }
        else if(((m_findFlags & wxWEBVIEW_FIND_ENTIRE_WORD) == (flags & wxWEBVIEW_FIND_ENTIRE_WORD)) && ((m_findFlags & wxWEBVIEW_FIND_MATCH_CASE) == (flags&wxWEBVIEW_FIND_MATCH_CASE)))
        {
            m_findFlags = flags;
            return FindNext(((flags & wxWEBVIEW_FIND_BACKWARDS) ? -1 : 1));
        }
    }
    //Remove old highlight if any.
    if(m_findFlags & wxWEBVIEW_FIND_HIGHLIGHT_RESULT)
    {
        FindInternal(m_findText, (m_findFlags &~ wxWEBVIEW_FIND_HIGHLIGHT_RESULT), wxWEBVIEW_FIND_REMOVE_HIGHLIGHT);
    }
    //Reset find variables.
    FindClear();
    ClearSelection();
    m_findText = text;
    m_findFlags = flags;
    //find the text and return wxNOT_FOUND if there are no matches.
    FindInternal(text, flags, wxWEBVIEW_FIND_ADD_POINTERS);
    if(m_findPointers.empty())
        return wxNOT_FOUND;

    // Or their number if there are.
    return m_findPointers.size();
}

void wxWebViewIE::SetEditable(bool enable)
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        if( enable )
            document->put_designMode(SysAllocString(L"On"));
        else
            document->put_designMode(SysAllocString(L"Off"));

    }
}

bool wxWebViewIE::IsEditable() const
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        BSTR mode;
        if ( document->get_designMode(&mode) == S_OK )
        {
            if ( wxString(mode) == "On" )
                return true;
        }
    }
    return false;
}

void wxWebViewIE::SelectAll()
{
    ExecCommand("SelectAll");
}

bool wxWebViewIE::HasSelection() const
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        wxCOMPtr<IHTMLSelectionObject> selection;
        wxString sel;
        HRESULT hr = document->get_selection(&selection);
        if(SUCCEEDED(hr))
        {
            BSTR type;
            if ( selection->get_type(&type) == S_OK )
                sel = wxString(type);
        }
        return sel != "None";
    }
    else
    {
        return false;
    }
}

void wxWebViewIE::DeleteSelection()
{
    ExecCommand("Delete");
}

wxString wxWebViewIE::GetSelectedText() const
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        wxCOMPtr<IHTMLSelectionObject> selection;
        wxString selected;
        HRESULT hr = document->get_selection(&selection);
        if(SUCCEEDED(hr))
        {
            wxCOMPtr<IDispatch> disrange;
            hr = selection->createRange(&disrange);
            if(SUCCEEDED(hr))
            {
                wxCOMPtr<IHTMLTxtRange> range;
                hr = disrange->QueryInterface(IID_IHTMLTxtRange, (void**)&range);
                if(SUCCEEDED(hr))
                {
                    BSTR text;
                    if ( range->get_text(&text) == S_OK )
                        selected = wxString(text);
                }
            }
        }
        return selected;
    }
    else
    {
        return "";
    }
}

wxString wxWebViewIE::GetSelectedSource() const
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        wxCOMPtr<IHTMLSelectionObject> selection;
        wxString selected;
        HRESULT hr = document->get_selection(&selection);
        if(SUCCEEDED(hr))
        {
            wxCOMPtr<IDispatch> disrange;
            hr = selection->createRange(&disrange);
            if(SUCCEEDED(hr))
            {
                wxCOMPtr<IHTMLTxtRange> range;
                hr = disrange->QueryInterface(IID_IHTMLTxtRange, (void**)&range);
                if(SUCCEEDED(hr))
                {
                    BSTR text;
                    if ( range->get_htmlText(&text) == S_OK )
                        selected = wxString(text);
                }
            }
        }
        return selected;
    }
    else
    {
        return "";
    }
}

void wxWebViewIE::ClearSelection()
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        wxCOMPtr<IHTMLSelectionObject> selection;
        wxString selected;
        HRESULT hr = document->get_selection(&selection);
        if(SUCCEEDED(hr))
        {
            selection->empty();
        }
    }
}

wxString wxWebViewIE::GetPageText() const
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        wxString text;
        wxCOMPtr<IHTMLElement> body;
        HRESULT hr = document->get_body(&body);
        if(SUCCEEDED(hr))
        {
            BSTR out;
            if ( body->get_innerText(&out) == S_OK )
                text = wxString(out);
        }
        return text;
    }
    else
    {
        return "";
    }
}

void wxWebViewIE::RunScript(const wxString& javascript)
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        wxCOMPtr<IHTMLWindow2> window;
        wxString language = "javascript";
        HRESULT hr = document->get_parentWindow(&window);
        if(SUCCEEDED(hr))
        {
            VARIANT level;
            VariantInit(&level);
            V_VT(&level) = VT_EMPTY;
            window->execScript(SysAllocString(javascript.wc_str()),
                               SysAllocString(language.wc_str()),
                               &level);
        }
    }
}

void wxWebViewIE::RegisterHandler(wxSharedPtr<wxWebViewHandler> handler)
{
    wxDynamicLibrary urlMon(wxT("urlmon.dll"));
    if(urlMon.HasSymbol(wxT("CoInternetGetSession")))
    {
        typedef HRESULT (WINAPI *CoInternetGetSession_t)(DWORD, wxIInternetSession**, DWORD);
        wxDYNLIB_FUNCTION(CoInternetGetSession_t, CoInternetGetSession, urlMon);

        ClassFactory* cf = new ClassFactory(handler);
        wxIInternetSession* session;
        HRESULT res = (*pfnCoInternetGetSession)(0, &session, 0);
        if(FAILED(res))
        {
            wxFAIL_MSG("Could not retrive internet session");
        }

        HRESULT hr = session->RegisterNameSpace(cf, CLSID_FileProtocol,
                                                handler->GetName().wc_str(),
                                                0, NULL, 0);
        if(FAILED(hr))
        {
            wxFAIL_MSG("Could not register protocol");
        }
        m_factories.push_back(cf);
    }
    else
    {
        wxFAIL_MSG("urlmon does not contain CoInternetGetSession");
    }
}

bool wxWebViewIE::CanExecCommand(wxString command) const
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        VARIANT_BOOL enabled;

        document->queryCommandEnabled(SysAllocString(command.wc_str()), &enabled);

        return (enabled == VARIANT_TRUE);
    }
    else
    {
        return false;
    }

}

void wxWebViewIE::ExecCommand(wxString command)
{
    wxCOMPtr<IHTMLDocument2> document(GetDocument());

    if(document)
    {
        document->execCommand(SysAllocString(command.wc_str()), VARIANT_FALSE, VARIANT(), NULL);
    }
}

wxCOMPtr<IHTMLDocument2> wxWebViewIE::GetDocument() const
{
    wxCOMPtr<IDispatch> dispatch;
    wxCOMPtr<IHTMLDocument2> document;
    HRESULT result = m_webBrowser->get_Document(&dispatch);
    if(dispatch && SUCCEEDED(result))
    {
        //document is set to null automatically if the interface isn't supported
        dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&document);
    }
    return document;
}

bool wxWebViewIE::IsElementVisible(wxCOMPtr<IHTMLElement> elm)
{
    wxCOMPtr<IHTMLElement> elm1 = elm;
    BSTR tmp_bstr;
    bool is_visible = true;
    //This method is not perfect but it does discover most of the hidden elements.
    //so if a better solution is found, then please do improve.
    while(elm1)
    {
        wxCOMPtr<wxIHTMLElement2> elm2;
        if(SUCCEEDED(elm1->QueryInterface(wxIID_IHTMLElement2, (void**) &elm2)))
        {
            wxCOMPtr<wxIHTMLCurrentStyle> style;
            if(SUCCEEDED(elm2->get_currentStyle(&style)))
            {
                //Check if the object has the style display:none.
                if((style->get_display(&tmp_bstr) != S_OK) || 
                   (tmp_bstr != NULL && (wxCRT_StricmpW(tmp_bstr, L"none") == 0)))
                {
                    is_visible = false;
                }
                //Check if the object has the style visibility:hidden.
                if((is_visible && (style->get_visibility(&tmp_bstr) != S_OK)) ||
                  (tmp_bstr != NULL && wxCRT_StricmpW(tmp_bstr, L"hidden") == 0))
                {
                    is_visible = false;
                }
                style->Release();
            }
            elm2->Release();
        }

        //Lets check the object's parent element.
        IHTMLElement* parent;
        if(is_visible && SUCCEEDED(elm1->get_parentElement(&parent)))
        {
            elm1 = parent;
        }
        else
        {
            elm1->Release();
            break;
        }
    }
    return is_visible;
}

void wxWebViewIE::FindInternal(const wxString& text, int flags, int internal_flag)
{
    long find_flag = 0;
    wxCOMPtr<wxIMarkupServices> pIMS;
    wxCOMPtr<IHTMLDocument2> document = GetDocument();

    //This function does the acutal work.
    if(document && SUCCEEDED(document->QueryInterface(wxIID_IMarkupServices, (void **)&pIMS)))
    {
        wxCOMPtr<wxIMarkupContainer> pIMC;
        if(SUCCEEDED(document->QueryInterface(wxIID_IMarkupContainer, (void **)&pIMC)))
        {
            wxCOMPtr<wxIMarkupPointer> ptrBegin, ptrEnd;
            BSTR attr_bstr = SysAllocString(L"style=\"background-color:#ffff00\"");
            BSTR text_bstr = SysAllocString(text.wc_str());
            pIMS->CreateMarkupPointer(&ptrBegin);
            pIMS->CreateMarkupPointer(&ptrEnd);

            ptrBegin->SetGravity(wxPOINTER_GRAVITY_Right);
            ptrBegin->MoveToContainer(pIMC, TRUE);
            //Create the find flag from the wx one.
            if(flags & wxWEBVIEW_FIND_ENTIRE_WORD)
            {
                find_flag |= wxFINDTEXT_WHOLEWORD;
            }
            if(flags & wxWEBVIEW_FIND_MATCH_CASE)
            {
                find_flag |= wxFINDTEXT_MATCHCASE;
            }

            //A little speed-up to avoid to re-alloc in the positions vector.
            if(text.Len() < 3 && m_findPointers.capacity() < 500)
            {
               m_findPointers.reserve(text.Len() == 1 ? 1000 : 500);
            }

            while(ptrBegin->FindText(text_bstr, find_flag, ptrEnd, NULL) == S_OK)
            {
                wxCOMPtr<IHTMLElement> elm;
                if(ptrBegin->CurrentScope(&elm) == S_OK)
                {
                    if(IsElementVisible(elm))
                    {
                        //Highlight the word if the flag was set.
                        if(flags & wxWEBVIEW_FIND_HIGHLIGHT_RESULT)
                        {
                            IHTMLElement* pFontEl;
                            pIMS->CreateElement(wxTAGID_FONT, attr_bstr, &pFontEl);
                            pIMS->InsertElement(pFontEl, ptrBegin, ptrEnd);
                        }
                        if(internal_flag & wxWEBVIEW_FIND_REMOVE_HIGHLIGHT)
                        {
                            IHTMLElement* pFontEl;
                            ptrBegin->CurrentScope(&pFontEl);
                            pIMS->RemoveElement(pFontEl);
                            pFontEl->Release();
                        }
                        if(internal_flag & wxWEBVIEW_FIND_ADD_POINTERS)
                        {
                            wxIMarkupPointer *cptrBegin, *cptrEnd;
                            pIMS->CreateMarkupPointer(&cptrBegin);
                            pIMS->CreateMarkupPointer(&cptrEnd);
                            cptrBegin->MoveToPointer(ptrBegin);
                            cptrEnd->MoveToPointer(ptrEnd);
                            m_findPointers.push_back(wxFindPointers(cptrBegin,cptrEnd));
                        }
                    }
                }
                ptrBegin->MoveToPointer(ptrEnd);
            }
            //Clean up.
            SysFreeString(text_bstr);
            SysFreeString(attr_bstr);
        }
    }
}

long wxWebViewIE::FindNext(int direction)
{
    //Don't bother if we have no pointers set.
    if(m_findPointers.empty())
    {
        return wxNOT_FOUND;
    }
    //Manage the find position and do some checks.
    if(direction > 0)
    {
        m_findPosition++;
    }
    else
    {
        m_findPosition--;
    }

    if(m_findPosition >= (signed)m_findPointers.size())
    {
        if(m_findFlags & wxWEBVIEW_FIND_WRAP)
        {
            m_findPosition = 0;
        }
        else
        {
            m_findPosition--;
            return wxNOT_FOUND;
        }
    }
    else if(m_findPosition < 0)
    {
        if(m_findFlags & wxWEBVIEW_FIND_WRAP)
        {
            m_findPosition = m_findPointers.size()-1;
        }
        else
        {
            m_findPosition++;
            return wxNOT_FOUND;
        }
    }

    wxCOMPtr<IHTMLDocument2> document = GetDocument();
    wxCOMPtr<IHTMLElement> body_element;

    long ret = -1;
    //Now try to create a range from the body.
    if(document && SUCCEEDED(document->get_body(&body_element)))
    {
        wxCOMPtr<IHTMLBodyElement> body;
        if(SUCCEEDED(body_element->QueryInterface(IID_IHTMLBodyElement,(void**)&body)))
        {
            wxCOMPtr<wxIHTMLTxtRange> range;
            if(SUCCEEDED(body->createTextRange((IHTMLTxtRange**)(&range))))
            {
                wxCOMPtr<wxIMarkupServices> pIMS;
                //So far so good, now we try to position our find pointers.
                if(SUCCEEDED(document->QueryInterface(wxIID_IMarkupServices,(void **)&pIMS)))
                {
                    wxIMarkupPointer *begin = m_findPointers[m_findPosition].begin, *end = m_findPointers[m_findPosition].end;
                    if(pIMS->MoveRangeToPointers(begin,end,range) == S_OK && range->select() == S_OK)
                    {
                        ret = m_findPosition;
                    }
                }
            }
        }
    }
    return ret;
}

void wxWebViewIE::FindClear()
{
    //Reset find variables.
    m_findText.Empty();
    m_findFlags = wxWEBVIEW_FIND_DEFAULT;
    m_findPosition = -1;

    //The m_findPointers contains pointers for the found text.
    //Since it uses ref counting we call release on the pointers first
    //before we remove them from the vector. In other words do not just
    //remove elements from m_findPointers without calling release first
    //or you will get a memory leak.
    size_t count = m_findPointers.size();
    for(size_t i = 0; i < count; i++)
    {
        m_findPointers[i].begin->Release();
        m_findPointers[i].end->Release();
    }
    m_findPointers.clear();
}

bool wxWebViewIE::EnableControlFeature(long flag, bool enable)
{
#if wxUSE_DYNLIB_CLASS

    wxDynamicLibrary urlMon(wxT("urlmon.dll"));
    if( urlMon.IsLoaded() &&
        urlMon.HasSymbol("CoInternetSetFeatureEnabled") &&
        urlMon.HasSymbol("CoInternetIsFeatureEnabled"))
    {
        typedef HRESULT (WINAPI *CoInternetSetFeatureEnabled_t)(DWORD, DWORD, BOOL);
        typedef HRESULT (WINAPI *CoInternetIsFeatureEnabled_t)(DWORD, DWORD);

        wxDYNLIB_FUNCTION(CoInternetSetFeatureEnabled_t, CoInternetSetFeatureEnabled, urlMon);
        wxDYNLIB_FUNCTION(CoInternetIsFeatureEnabled_t, CoInternetIsFeatureEnabled, urlMon);

        HRESULT hr = (*pfnCoInternetIsFeatureEnabled)(flag,
                                                      0x2 /* SET_FEATURE_ON_PROCESS */);
        if((hr == S_OK && enable) || (hr == S_FALSE && !enable))
            return true;

        hr = (*pfnCoInternetSetFeatureEnabled)(flag,
                                               0x2/* SET_FEATURE_ON_PROCESS */,
                                               (enable ? TRUE : FALSE));
        if ( FAILED(hr) )
        {
            wxLogApiError(wxT("CoInternetSetFeatureEnabled"), hr);
            return false;
        }
        return true;
    }
    return false;
#else
    wxUnusedVar(flag);
    wxUnusedVar(enable);
    return false;
#endif // wxUSE_DYNLIB_CLASS/!wxUSE_DYNLIB_CLASS
}

void wxWebViewIE::onActiveXEvent(wxActiveXEvent& evt)
{
    if (m_webBrowser == NULL) return;

    switch (evt.GetDispatchId())
    {
        case DISPID_BEFORENAVIGATE2:
        {
            m_isBusy = true;

            wxString url = evt[1].GetString();
            wxString target = evt[3].GetString();

            wxWebViewEvent event(wxEVT_WEBVIEW_NAVIGATING,
                                 GetId(), url, target);

            //skip empty javascript events.
            if(url == "javascript:\"\"" && target.IsEmpty())
            {
                event.Veto();
            }
            else
            {
                event.SetEventObject(this);
                HandleWindowEvent(event);
            }

            if (!event.IsAllowed())
            {
                wxActiveXEventNativeMSW* nativeParams =
                    evt.GetNativeParameters();
                *V_BOOLREF(&nativeParams->pDispParams->rgvarg[0]) = VARIANT_TRUE;
            }

            // at this point, either the navigation event has been cancelled
            // and we're not busy, either it was accepted and IWebBrowser2's
            // Busy property will be true; so we don't need our override
            // flag anymore.
            m_isBusy = false;

            break;
        }

        case DISPID_NAVIGATECOMPLETE2:
        {
            wxString url = evt[1].GetString();
            // TODO: set target parameter if possible
            wxString target = wxEmptyString;
            wxWebViewEvent event(wxEVT_WEBVIEW_NAVIGATED,
                                 GetId(), url, target);
            event.SetEventObject(this);
            HandleWindowEvent(event);
            break;
        }

        case DISPID_PROGRESSCHANGE:
        {
            // download progress
            break;
        }

        case DISPID_DOCUMENTCOMPLETE:
        {
            //Only send a complete even if we are actually finished, this brings
            //the event in to line with webkit
            READYSTATE rs;
            m_webBrowser->get_ReadyState( &rs );
            if(rs != READYSTATE_COMPLETE)
                break;

            wxString url = evt[1].GetString();

            //As we are complete we also add to the history list, but not if the
            //page is not the main page, ie it is a subframe
            //We also have to check if we are loading a file:// url, if so we
            //need to change the comparison as ie passes back a different style
            //of url
            if(m_historyEnabled && !m_historyLoadingFromList &&
              (url == GetCurrentURL() ||
              (GetCurrentURL().substr(0, 4) == "file" &&
               wxFileSystem::URLToFileName(GetCurrentURL()).GetFullPath() == url)))
            {
                //If we are not at the end of the list, then erase everything
                //between us and the end before adding the new page
                if(m_historyPosition != static_cast<int>(m_historyList.size()) - 1)
                {
                    m_historyList.erase(m_historyList.begin() + m_historyPosition + 1,
                                        m_historyList.end());
                }
                wxSharedPtr<wxWebViewHistoryItem> item(new wxWebViewHistoryItem(url, GetCurrentTitle()));
                m_historyList.push_back(item);
                m_historyPosition++;
            }
            //Reset as we are done now
            m_historyLoadingFromList = false;
            //Reset the find values.
            FindClear();
            // TODO: set target parameter if possible
            wxString target = wxEmptyString;
            wxWebViewEvent event(wxEVT_WEBVIEW_LOADED, GetId(),
                                 url, target);
            event.SetEventObject(this);
            HandleWindowEvent(event);
            break;
        }

        case DISPID_STATUSTEXTCHANGE:
        {
            break;
        }

        case DISPID_TITLECHANGE:
        {
            wxString title = evt[0].GetString();

            wxWebViewEvent event(wxEVT_WEBVIEW_TITLE_CHANGED,
                                 GetId(), GetCurrentURL(), "");
            event.SetString(title);
            event.SetEventObject(this);
            HandleWindowEvent(event);
            break;
        }

        case DISPID_NAVIGATEERROR:
        {
            wxWebViewEvent event(wxEVT_WEBVIEW_ERROR, GetId(),
                                 evt[1].GetString(), evt[2].GetString());
            event.SetEventObject(this);

            switch (evt[3].GetLong())
            {
                // 400 Error codes
                WX_ERROR_CASE(HTTP_STATUS_BAD_REQUEST, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(HTTP_STATUS_DENIED, wxWEBVIEW_NAV_ERR_AUTH)
                WX_ERROR_CASE(HTTP_STATUS_PAYMENT_REQ, wxWEBVIEW_NAV_ERR_OTHER)
                WX_ERROR_CASE(HTTP_STATUS_FORBIDDEN, wxWEBVIEW_NAV_ERR_AUTH)
                WX_ERROR_CASE(HTTP_STATUS_NOT_FOUND, wxWEBVIEW_NAV_ERR_NOT_FOUND)
                WX_ERROR_CASE(HTTP_STATUS_BAD_METHOD, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(HTTP_STATUS_NONE_ACCEPTABLE, wxWEBVIEW_NAV_ERR_OTHER)
                WX_ERROR_CASE(HTTP_STATUS_PROXY_AUTH_REQ, wxWEBVIEW_NAV_ERR_AUTH)
                WX_ERROR_CASE(HTTP_STATUS_REQUEST_TIMEOUT, wxWEBVIEW_NAV_ERR_CONNECTION)
                WX_ERROR_CASE(HTTP_STATUS_CONFLICT, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(HTTP_STATUS_GONE, wxWEBVIEW_NAV_ERR_NOT_FOUND)
                WX_ERROR_CASE(HTTP_STATUS_LENGTH_REQUIRED, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(HTTP_STATUS_PRECOND_FAILED, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(HTTP_STATUS_REQUEST_TOO_LARGE, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(HTTP_STATUS_URI_TOO_LONG, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(HTTP_STATUS_UNSUPPORTED_MEDIA, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(HTTP_STATUS_RETRY_WITH, wxWEBVIEW_NAV_ERR_OTHER)

                // 500 - Error codes
                WX_ERROR_CASE(HTTP_STATUS_SERVER_ERROR, wxWEBVIEW_NAV_ERR_CONNECTION)
                WX_ERROR_CASE(HTTP_STATUS_NOT_SUPPORTED, wxWEBVIEW_NAV_ERR_CONNECTION)
                WX_ERROR_CASE(HTTP_STATUS_BAD_GATEWAY, wxWEBVIEW_NAV_ERR_CONNECTION)
                WX_ERROR_CASE(HTTP_STATUS_SERVICE_UNAVAIL, wxWEBVIEW_NAV_ERR_CONNECTION)
                WX_ERROR_CASE(HTTP_STATUS_GATEWAY_TIMEOUT, wxWEBVIEW_NAV_ERR_CONNECTION)
                WX_ERROR_CASE(HTTP_STATUS_VERSION_NOT_SUP, wxWEBVIEW_NAV_ERR_REQUEST)

                // URL Moniker error codes
                WX_ERROR_CASE(INET_E_INVALID_URL, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(INET_E_NO_SESSION, wxWEBVIEW_NAV_ERR_CONNECTION)
                WX_ERROR_CASE(INET_E_CANNOT_CONNECT, wxWEBVIEW_NAV_ERR_CONNECTION)
                WX_ERROR_CASE(INET_E_RESOURCE_NOT_FOUND, wxWEBVIEW_NAV_ERR_NOT_FOUND)
                WX_ERROR_CASE(INET_E_OBJECT_NOT_FOUND, wxWEBVIEW_NAV_ERR_NOT_FOUND)
                WX_ERROR_CASE(INET_E_DATA_NOT_AVAILABLE, wxWEBVIEW_NAV_ERR_NOT_FOUND)
                WX_ERROR_CASE(INET_E_DOWNLOAD_FAILURE, wxWEBVIEW_NAV_ERR_CONNECTION)
                WX_ERROR_CASE(INET_E_AUTHENTICATION_REQUIRED, wxWEBVIEW_NAV_ERR_AUTH)
                WX_ERROR_CASE(INET_E_NO_VALID_MEDIA, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(INET_E_CONNECTION_TIMEOUT, wxWEBVIEW_NAV_ERR_CONNECTION)
                WX_ERROR_CASE(INET_E_INVALID_REQUEST, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(INET_E_UNKNOWN_PROTOCOL, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(INET_E_SECURITY_PROBLEM, wxWEBVIEW_NAV_ERR_SECURITY)
                WX_ERROR_CASE(INET_E_CANNOT_LOAD_DATA, wxWEBVIEW_NAV_ERR_OTHER)
                WX_ERROR_CASE(INET_E_REDIRECT_FAILED, wxWEBVIEW_NAV_ERR_OTHER)
                WX_ERROR_CASE(INET_E_REDIRECT_TO_DIR, wxWEBVIEW_NAV_ERR_REQUEST)
                WX_ERROR_CASE(INET_E_CANNOT_LOCK_REQUEST, wxWEBVIEW_NAV_ERR_OTHER)
                WX_ERROR_CASE(INET_E_USE_EXTEND_BINDING, wxWEBVIEW_NAV_ERR_OTHER)
                WX_ERROR_CASE(INET_E_TERMINATED_BIND, wxWEBVIEW_NAV_ERR_OTHER)
                WX_ERROR_CASE(INET_E_INVALID_CERTIFICATE, wxWEBVIEW_NAV_ERR_CERTIFICATE)
                WX_ERROR_CASE(INET_E_CODE_DOWNLOAD_DECLINED, wxWEBVIEW_NAV_ERR_USER_CANCELLED)
                WX_ERROR_CASE(INET_E_RESULT_DISPATCHED, wxWEBVIEW_NAV_ERR_OTHER)
                WX_ERROR_CASE(INET_E_CANNOT_REPLACE_SFP_FILE, wxWEBVIEW_NAV_ERR_SECURITY)
                WX_ERROR_CASE(INET_E_CODE_INSTALL_BLOCKED_BY_HASH_POLICY, wxWEBVIEW_NAV_ERR_SECURITY)
                WX_ERROR_CASE(INET_E_CODE_INSTALL_SUPPRESSED, wxWEBVIEW_NAV_ERR_SECURITY)
            }
            HandleWindowEvent(event);
            break;
        }
        case DISPID_NEWWINDOW3:
        {
            wxString url = evt[4].GetString();

            wxWebViewEvent event(wxEVT_WEBVIEW_NEWWINDOW,
                                 GetId(), url, wxEmptyString);
            event.SetEventObject(this);
            HandleWindowEvent(event);

            //We always cancel this event otherwise an Internet Exporer window
            //is opened for the url
            wxActiveXEventNativeMSW* nativeParams = evt.GetNativeParameters();
            *V_BOOLREF(&nativeParams->pDispParams->rgvarg[3]) = VARIANT_TRUE;
            break;
        }
    }

    evt.Skip();
}

VirtualProtocol::VirtualProtocol(wxSharedPtr<wxWebViewHandler> handler)
{
    m_file = NULL;
    m_handler = handler;
}

BEGIN_IID_TABLE(VirtualProtocol)
    ADD_IID(Unknown)
    ADD_RAW_IID(wxIID_IInternetProtocolRoot)
    ADD_RAW_IID(wxIID_IInternetProtocol)
END_IID_TABLE;

IMPLEMENT_IUNKNOWN_METHODS(VirtualProtocol)

HRESULT STDMETHODCALLTYPE VirtualProtocol::Start(LPCWSTR szUrl, wxIInternetProtocolSink *pOIProtSink,
                               wxIInternetBindInfo *pOIBindInfo, DWORD grfPI,
                               HANDLE_PTR dwReserved)
{
    wxUnusedVar(szUrl);
    wxUnusedVar(pOIBindInfo);
    wxUnusedVar(grfPI);
    wxUnusedVar(dwReserved);
    m_protocolSink = pOIProtSink;

    //We get the file itself from the protocol handler
    m_file = m_handler->GetFile(szUrl);


    if(!m_file)
        return INET_E_RESOURCE_NOT_FOUND;

    //We return the stream length for current and total size as we can always
    //read the whole file from the stream
    wxFileOffset length = m_file->GetStream()->GetLength();
    m_protocolSink->ReportData(wxBSCF_FIRSTDATANOTIFICATION |
                               wxBSCF_DATAFULLYAVAILABLE |
                               wxBSCF_LASTDATANOTIFICATION,
                               length, length);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE VirtualProtocol::Read(void *pv, ULONG cb, ULONG *pcbRead)
{
    //If the file is null we return false to indicte it is finished
    if(!m_file)
        return S_FALSE;

    wxStreamError err = m_file->GetStream()->Read(pv, cb).GetLastError();
    *pcbRead = m_file->GetStream()->LastRead();

    if(err == wxSTREAM_NO_ERROR)
    {
        if(*pcbRead < cb)
        {
            wxDELETE(m_file);
            m_protocolSink->ReportResult(S_OK, 0, NULL);
        }
        //As we are not eof there is more data
        return S_OK;
    }
    else if(err == wxSTREAM_EOF)
    {
        wxDELETE(m_file);
        m_protocolSink->ReportResult(S_OK, 0, NULL);
        //We are eof and so finished
        return S_OK;
    }
    else if(err ==  wxSTREAM_READ_ERROR)
    {
        wxDELETE(m_file);
        return INET_E_DOWNLOAD_FAILURE;
    }
    else
    {
        //Dummy return to suppress a compiler warning
        wxFAIL;
        return INET_E_DOWNLOAD_FAILURE;
    }
}

BEGIN_IID_TABLE(ClassFactory)
    ADD_IID(Unknown)
    ADD_IID(ClassFactory)
END_IID_TABLE;

IMPLEMENT_IUNKNOWN_METHODS(ClassFactory)

HRESULT STDMETHODCALLTYPE ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                                     void ** ppvObject)
{
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;
    VirtualProtocol* vp = new VirtualProtocol(m_handler);
    vp->AddRef();
    HRESULT hr = vp->QueryInterface(riid, ppvObject);
    vp->Release();
    return hr;

}

STDMETHODIMP ClassFactory::LockServer(BOOL fLock)
{
    wxUnusedVar(fLock);
    return S_OK;
}

wxIEContainer::wxIEContainer(wxWindow *parent, REFIID iid, IUnknown *pUnk,
                             DocHostUIHandler* uiHandler) :
    wxActiveXContainer(parent,iid,pUnk)
{
    m_uiHandler = uiHandler;
}

wxIEContainer::~wxIEContainer()
{
}

bool wxIEContainer::QueryClientSiteInterface(REFIID iid, void **_interface,
                                             const char *&desc)
{
    if (m_uiHandler && IsEqualIID(iid, wxIID_IDocHostUIHandler))
    {
        *_interface = (IUnknown *) (wxIDocHostUIHandler *) m_uiHandler;
        desc = "IDocHostUIHandler";
        return true;
    }
    return false;
}

HRESULT wxSTDCALL DocHostUIHandler::ShowContextMenu(DWORD dwID, POINT *ppt,
                                          IUnknown *pcmdtReserved,
                                          IDispatch *pdispReserved)
{
    wxUnusedVar(dwID);
    wxUnusedVar(ppt);
    wxUnusedVar(pcmdtReserved);
    wxUnusedVar(pdispReserved);
    if(m_browser->IsContextMenuEnabled()) 
        return E_NOTIMPL; 
    else 
        return S_OK; 
}

HRESULT wxSTDCALL DocHostUIHandler::GetHostInfo(DOCHOSTUIINFO *pInfo)
{
    //don't show 3d border and enable themes.
    pInfo->dwFlags = pInfo->dwFlags | DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_THEME;
    return S_OK;
}

HRESULT wxSTDCALL DocHostUIHandler::ShowUI(DWORD dwID,
                                 IOleInPlaceActiveObject *pActiveObject,
                                 IOleCommandTarget *pCommandTarget,
                                 IOleInPlaceFrame *pFrame,
                                 IOleInPlaceUIWindow *pDoc)
{
    wxUnusedVar(dwID);
    wxUnusedVar(pActiveObject);
    wxUnusedVar(pCommandTarget);
    wxUnusedVar(pFrame);
    wxUnusedVar(pDoc);
    return S_FALSE;
}

HRESULT wxSTDCALL DocHostUIHandler::HideUI(void)
{
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::UpdateUI(void)
{
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::EnableModeless(BOOL fEnable)
{
    wxUnusedVar(fEnable);
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::OnDocWindowActivate(BOOL fActivate)
{
    wxUnusedVar(fActivate);
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::OnFrameWindowActivate(BOOL fActivate)
{
    wxUnusedVar(fActivate);
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::ResizeBorder(LPCRECT prcBorder,
                                       IOleInPlaceUIWindow *pUIWindow,
                                       BOOL fFrameWindow)
{
    wxUnusedVar(prcBorder);
    wxUnusedVar(pUIWindow);
    wxUnusedVar(fFrameWindow);
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::TranslateAccelerator(LPMSG lpMsg,
                                               const GUID *pguidCmdGroup,
                                               DWORD nCmdID)
{
    if(lpMsg && lpMsg->message == WM_KEYDOWN)
    {
        // check control is down but that it isn't right-alt which is mapped to
        // alt + ctrl
        if(GetKeyState(VK_CONTROL) & 0x8000 && 
         !(GetKeyState(VK_MENU) & 0x8000))
        {
            //skip the accelerators used by the control
            switch(lpMsg->wParam)
            {
                case 'F':
                case 'L':
                case 'N':
                case 'O':
                case 'P':
                    return S_OK;
            }
        }
        //skip F5
        if(lpMsg->wParam == VK_F5)
        {
            return S_OK;
        }
    }

    wxUnusedVar(pguidCmdGroup);
    wxUnusedVar(nCmdID);
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::GetOptionKeyPath(LPOLESTR *pchKey,DWORD dw)
{
    wxUnusedVar(pchKey);
    wxUnusedVar(dw);
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::GetDropTarget(IDropTarget *pDropTarget,
                                        IDropTarget **ppDropTarget)
{
    wxUnusedVar(pDropTarget);
    wxUnusedVar(ppDropTarget);
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::GetExternal(IDispatch **ppDispatch)
{
    wxUnusedVar(ppDispatch);
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::TranslateUrl(DWORD dwTranslate,
                                       OLECHAR *pchURLIn,
                                       OLECHAR **ppchURLOut)
{
    wxUnusedVar(dwTranslate);
    wxUnusedVar(pchURLIn);
    wxUnusedVar(ppchURLOut);
    return E_NOTIMPL;
}

HRESULT wxSTDCALL DocHostUIHandler::FilterDataObject(IDataObject *pDO, IDataObject **ppDORet)
{
    wxUnusedVar(pDO);
    wxUnusedVar(ppDORet);
    return E_NOTIMPL;
}

BEGIN_IID_TABLE(DocHostUIHandler)
    ADD_IID(Unknown)
    ADD_RAW_IID(wxIID_IDocHostUIHandler)
END_IID_TABLE;

IMPLEMENT_IUNKNOWN_METHODS(DocHostUIHandler)

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_IE
