/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/osx/webkit.h
// Purpose:     wxWebViewWebKit - embeddable web kit control,
//                             OS X implementation of web view component
// Author:      Jethro Grassie / Kevin Ollivier / Marianne Gagnon
// Modified by:
// Created:     2004-4-16
// Copyright:   (c) Jethro Grassie / Kevin Ollivier / Marianne Gagnon
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WEBKIT_H
#define _WX_WEBKIT_H

#include "wx/setup.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT && (defined(__WXOSX_COCOA__) \
                                          ||  defined(__WXOSX_CARBON__))

#include "wx/control.h"
#include "wx/webview.h"

#include "wx/osx/core/objcid.h"

// ----------------------------------------------------------------------------
// Web Kit Control
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_WEBVIEW wxWebViewWebKit : public wxWebView
{
public:
    wxDECLARE_DYNAMIC_CLASS(wxWebViewWebKit);

    wxWebViewWebKit() {}
    wxWebViewWebKit(wxWindow *parent,
                    wxWindowID winID = wxID_ANY,
                    const wxString& strURL = wxWebViewDefaultURLStr,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize, long style = 0,
                    const wxString& name = wxWebViewNameStr)
    {
        Create(parent, winID, strURL, pos, size, style, name);
    }
    bool Create(wxWindow *parent,
                wxWindowID winID = wxID_ANY,
                const wxString& strURL = wxWebViewDefaultURLStr,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize, long style = 0,
                const wxString& name = wxWebViewNameStr);
    virtual ~wxWebViewWebKit();

    virtual bool CanGoBack() const;
    virtual bool CanGoForward() const;
    virtual void GoBack();
    virtual void GoForward();
    virtual void Reload(wxWebViewReloadFlags flags = wxWEBVIEW_RELOAD_DEFAULT);
    virtual void Stop();
    virtual wxString GetPageSource() const;
    virtual wxString GetPageText() const;

    virtual void Print();

    virtual void LoadURL(const wxString& url);
    virtual wxString GetCurrentURL() const;
    virtual wxString GetCurrentTitle() const;
    virtual wxWebViewZoom GetZoom() const;
    virtual void SetZoom(wxWebViewZoom zoom);

    virtual void SetZoomType(wxWebViewZoomType zoomType);
    virtual wxWebViewZoomType GetZoomType() const;
    virtual bool CanSetZoomType(wxWebViewZoomType type) const;

    virtual bool IsBusy() const { return m_busy; }

    //History functions
    virtual void ClearHistory();
    virtual void EnableHistory(bool enable = true);
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetBackwardHistory();
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetForwardHistory();
    virtual void LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item);

    //Undo / redo functionality
    virtual bool CanUndo() const;
    virtual bool CanRedo() const;
    virtual void Undo();
    virtual void Redo();

    //Find function
    virtual long Find(const wxString& text, int flags = wxWEBVIEW_FIND_DEFAULT) 
    { 
        wxUnusedVar(text);
        wxUnusedVar(flags);
        return wxNOT_FOUND; 
    }

    //Clipboard functions
    virtual bool CanCut() const { return true; }
    virtual bool CanCopy() const { return true; }
    virtual bool CanPaste() const { return true; }
    virtual void Cut();
    virtual void Copy();
    virtual void Paste();

    //Editing functions
    virtual void SetEditable(bool enable = true);
    virtual bool IsEditable() const;

    //Selection
    virtual void DeleteSelection();
    virtual bool HasSelection() const;
    virtual void SelectAll();
    virtual wxString GetSelectedText() const;
    virtual wxString GetSelectedSource() const;
    virtual void ClearSelection();

    void RunScript(const wxString& javascript);

    //Virtual Filesystem Support
    virtual void RegisterHandler(wxSharedPtr<wxWebViewHandler> handler);

    virtual void* GetNativeBackend() const { return m_webView; }

    // ---- methods not from the parent (common) interface
    bool  CanGetPageSource() const;

    void  SetScrollPos(int pos);
    int   GetScrollPos();

    bool  CanIncreaseTextSize() const;
    void  IncreaseTextSize();
    bool  CanDecreaseTextSize() const;
    void  DecreaseTextSize();

    float GetWebkitZoom() const;
    void  SetWebkitZoom(float zoom);

    // don't hide base class virtuals
    virtual void SetScrollPos( int orient, int pos, bool refresh = true )
        { return wxControl::SetScrollPos(orient, pos, refresh); }
    virtual int GetScrollPos( int orient ) const
        { return wxControl::GetScrollPos(orient); }

    //we need to resize the webview when the control size changes
    void OnSize(wxSizeEvent &event);
    void OnMove(wxMoveEvent &event);
    void OnMouseEvents(wxMouseEvent &event);

    bool m_busy;

protected:
    virtual void DoSetPage(const wxString& html, const wxString& baseUrl);

    DECLARE_EVENT_TABLE()
    void MacVisibilityChanged();

private:
    wxWindow *m_parent;
    wxWindowID m_windowID;
    wxString m_pageTitle;

    wxObjCID m_webView;

    // we may use this later to setup our own mouse events,
    // so leave it in for now.
    void* m_webKitCtrlEventHandler;
    //It should be WebView*, but WebView is an Objective-C class
    //TODO: look into using DECLARE_WXCOCOA_OBJC_CLASS rather than this.
};

class WXDLLIMPEXP_WEBVIEW wxWebViewFactoryWebKit : public wxWebViewFactory
{
public:
    virtual wxWebView* Create() { return new wxWebViewWebKit; }
    virtual wxWebView* Create(wxWindow* parent,
                              wxWindowID id,
                              const wxString& url = wxWebViewDefaultURLStr,
                              const wxPoint& pos = wxDefaultPosition,
                              const wxSize& size = wxDefaultSize,
                              long style = 0,
                              const wxString& name = wxWebViewNameStr)
    { return new wxWebViewWebKit(parent, id, url, pos, size, style, name); }
};

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT

#endif // _WX_WEBKIT_H_
