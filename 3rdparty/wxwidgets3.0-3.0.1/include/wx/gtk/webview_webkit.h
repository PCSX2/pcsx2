/////////////////////////////////////////////////////////////////////////////
// Name:        include/gtk/wx/webview.h
// Purpose:     GTK webkit backend for web view component
// Author:      Robert Roebling, Marianne Gagnon
// Copyright:   (c) 2010 Marianne Gagnon, 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_WEBKITCTRL_H_
#define _WX_GTK_WEBKITCTRL_H_

#include "wx/defs.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT && defined(__WXGTK__)

#include "wx/sharedptr.h"
#include "wx/webview.h"

typedef struct _WebKitWebView WebKitWebView;

//-----------------------------------------------------------------------------
// wxWebViewWebKit
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_WEBVIEW wxWebViewWebKit : public wxWebView
{
public:
    wxWebViewWebKit();

    wxWebViewWebKit(wxWindow *parent,
           wxWindowID id = wxID_ANY,
           const wxString& url = wxWebViewDefaultURLStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxString& name = wxWebViewNameStr)
    {
        Create(parent, id, url, pos, size, style, name);
    }

    virtual bool Create(wxWindow *parent,
           wxWindowID id = wxID_ANY,
           const wxString& url = wxWebViewDefaultURLStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxString& name = wxWebViewNameStr);

    virtual ~wxWebViewWebKit();

    virtual bool Enable( bool enable = true );

    // implementation
    // --------------

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    virtual void Stop();
    virtual void LoadURL(const wxString& url);
    virtual void GoBack();
    virtual void GoForward();
    virtual void Reload(wxWebViewReloadFlags flags = wxWEBVIEW_RELOAD_DEFAULT);
    virtual bool CanGoBack() const;
    virtual bool CanGoForward() const;
    virtual void ClearHistory();
    virtual void EnableContextMenu(bool enable = true);
    virtual void EnableHistory(bool enable = true);
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetBackwardHistory();
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetForwardHistory();
    virtual void LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item);
    virtual wxString GetCurrentURL() const;
    virtual wxString GetCurrentTitle() const;
    virtual wxString GetPageSource() const;
    virtual wxString GetPageText() const;
    virtual void Print();
    virtual bool IsBusy() const;

    void SetZoomType(wxWebViewZoomType);
    wxWebViewZoomType GetZoomType() const;
    bool CanSetZoomType(wxWebViewZoomType) const;
    virtual wxWebViewZoom GetZoom() const;
    virtual void SetZoom(wxWebViewZoom);

    //Clipboard functions
    virtual bool CanCut() const;
    virtual bool CanCopy() const;
    virtual bool CanPaste() const;
    virtual void Cut();
    virtual void Copy();
    virtual void Paste();

    //Undo / redo functionality
    virtual bool CanUndo() const;
    virtual bool CanRedo() const;
    virtual void Undo();
    virtual void Redo();

    //Find function
    virtual long Find(const wxString& text, int flags = wxWEBVIEW_FIND_DEFAULT);

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

    virtual void RunScript(const wxString& javascript);

    //Virtual Filesystem Support
    virtual void RegisterHandler(wxSharedPtr<wxWebViewHandler> handler);
    virtual wxVector<wxSharedPtr<wxWebViewHandler> > GetHandlers() { return m_handlerList; }

    virtual void* GetNativeBackend() const { return m_web_view; }

    /** TODO: check if this can be made private
     * The native control has a getter to check for busy state, but except in
     * very recent versions of webkit this getter doesn't say everything we need
     * (namely it seems to stay indefinitely busy when loading is cancelled by
     * user)
     */
    bool m_busy;

    wxString m_vfsurl;

    //We use this flag to stop recursion when we load a page from the navigation
    //callback, mainly when loading a VFS page
    bool m_guard;
    //This flag is use to indicate when a navigation event is the result of a
    //create-web-view signal and so we need to send a new window event
    bool m_creating;

protected:
    virtual void DoSetPage(const wxString& html, const wxString& baseUrl);

    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

private:

    void ZoomIn();
    void ZoomOut();
    void SetWebkitZoom(float level);
    float GetWebkitZoom() const;

    //Find helper function
    void FindClear();

    // focus event handler: calls GTKUpdateBitmap()
    void GTKOnFocus(wxFocusEvent& event);

    WebKitWebView *m_web_view;
    int m_historyLimit;

    wxVector<wxSharedPtr<wxWebViewHandler> > m_handlerList;

    //variables used for Find()
    int m_findFlags;
    wxString m_findText;
    int m_findPosition;
    int m_findCount;

    wxDECLARE_DYNAMIC_CLASS(wxWebViewWebKit);
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


#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT && defined(__WXGTK__)

#endif
