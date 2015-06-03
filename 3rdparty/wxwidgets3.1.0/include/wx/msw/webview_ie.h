/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/msw/webview_ie.h
// Purpose:     wxMSW IE wxWebView backend
// Author:      Marianne Gagnon
// Copyright:   (c) 2010 Marianne Gagnon, 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef wxWebViewIE_H
#define wxWebViewIE_H

#include "wx/setup.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_IE && defined(__WXMSW__)

#include "wx/control.h"
#include "wx/webview.h"
#include "wx/msw/ole/automtn.h"
#include "wx/msw/ole/activex.h"
#include "wx/msw/ole/oleutils.h"
#include "wx/msw/private/comptr.h"
#include "wx/msw/wrapwin.h"
#include "wx/msw/missing.h"
#include "wx/msw/webview_missing.h"
#include "wx/sharedptr.h"
#include "wx/vector.h"

struct IHTMLDocument2;
struct IHTMLElement;
struct IMarkupPointer;
class wxFSFile;
class ClassFactory;
class wxIEContainer;
class DocHostUIHandler;
class wxFindPointers;
class wxIInternetProtocol;

class WXDLLIMPEXP_WEBVIEW wxWebViewIE : public wxWebView
{
public:

    wxWebViewIE() {}

    wxWebViewIE(wxWindow* parent,
           wxWindowID id,
           const wxString& url = wxWebViewDefaultURLStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = 0,
           const wxString& name = wxWebViewNameStr)
   {
       Create(parent, id, url, pos, size, style, name);
   }

    ~wxWebViewIE();

    bool Create(wxWindow* parent,
           wxWindowID id,
           const wxString& url = wxWebViewDefaultURLStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = 0,
           const wxString& name = wxWebViewNameStr);

    virtual void LoadURL(const wxString& url);
    virtual void LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item);
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetBackwardHistory();
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetForwardHistory();

    virtual bool CanGoForward() const;
    virtual bool CanGoBack() const;
    virtual void GoBack();
    virtual void GoForward();
    virtual void ClearHistory();
    virtual void EnableHistory(bool enable = true);
    virtual void Stop();
    virtual void Reload(wxWebViewReloadFlags flags = wxWEBVIEW_RELOAD_DEFAULT);

    virtual wxString GetPageSource() const;
    virtual wxString GetPageText() const;

    virtual bool IsBusy() const;
    virtual wxString GetCurrentURL() const;
    virtual wxString GetCurrentTitle() const;

    virtual void SetZoomType(wxWebViewZoomType);
    virtual wxWebViewZoomType GetZoomType() const;
    virtual bool CanSetZoomType(wxWebViewZoomType) const;

    virtual void Print();

    virtual wxWebViewZoom GetZoom() const;
    virtual void SetZoom(wxWebViewZoom zoom);

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
    virtual void SelectAll();
    virtual bool HasSelection() const;
    virtual void DeleteSelection();
    virtual wxString GetSelectedText() const;
    virtual wxString GetSelectedSource() const;
    virtual void ClearSelection();

    virtual void RunScript(const wxString& javascript);

    //Virtual Filesystem Support
    virtual void RegisterHandler(wxSharedPtr<wxWebViewHandler> handler);

    virtual void* GetNativeBackend() const { return m_webBrowser; }

    // ---- IE-specific methods

    // FIXME: I seem to be able to access remote webpages even in offline mode...
    bool IsOfflineMode();
    void SetOfflineMode(bool offline);

    wxWebViewZoom GetIETextZoom() const;
    void SetIETextZoom(wxWebViewZoom level);

    wxWebViewZoom GetIEOpticalZoom() const;
    void SetIEOpticalZoom(wxWebViewZoom level);

    void onActiveXEvent(wxActiveXEvent& evt);
    void onEraseBg(wxEraseEvent&) {}

    wxDECLARE_EVENT_TABLE();

protected:
    virtual void DoSetPage(const wxString& html, const wxString& baseUrl);

private:
    wxIEContainer* m_container;
    wxAutomationObject m_ie;
    IWebBrowser2* m_webBrowser;
    DWORD m_dwCookie;
    wxCOMPtr<DocHostUIHandler> m_uiHandler;

    //We store the current zoom type;
    wxWebViewZoomType m_zoomType;

    /** The "Busy" property of IWebBrowser2 does not always return busy when
     *  we'd want it to; this variable may be set to true in cases where the
     *  Busy property is false but should be true.
     */
    bool m_isBusy;
    //We manage our own history, the history list contains the history items
    //which are added as documentcomplete events arrive, unless we are loading
    //an item from the history. The position is stored as an int, and reflects
    //where we are in the history list.
    wxVector<wxSharedPtr<wxWebViewHistoryItem> > m_historyList;
    wxVector<ClassFactory*> m_factories;
    int m_historyPosition;
    bool m_historyLoadingFromList;
    bool m_historyEnabled;

    //We store find flag, results and position.
    wxVector<wxFindPointers> m_findPointers;
    int m_findFlags;
    wxString m_findText;
    int m_findPosition;

    //Generic helper functions
    bool CanExecCommand(wxString command) const;
    void ExecCommand(wxString command);
    wxCOMPtr<IHTMLDocument2> GetDocument() const;
    bool IsElementVisible(wxCOMPtr<IHTMLElement> elm);
    //Find helper functions.
    void FindInternal(const wxString& text, int flags, int internal_flag);
    long FindNext(int direction = 1);
    void FindClear();
    //Toggles control features see INTERNETFEATURELIST for values.
    bool EnableControlFeature(long flag, bool enable = true);

    wxDECLARE_DYNAMIC_CLASS(wxWebViewIE);
};

class WXDLLIMPEXP_WEBVIEW wxWebViewFactoryIE : public wxWebViewFactory
{
public:
    virtual wxWebView* Create() { return new wxWebViewIE; }
    virtual wxWebView* Create(wxWindow* parent,
                              wxWindowID id,
                              const wxString& url = wxWebViewDefaultURLStr,
                              const wxPoint& pos = wxDefaultPosition,
                              const wxSize& size = wxDefaultSize,
                              long style = 0,
                              const wxString& name = wxWebViewNameStr)
    { return new wxWebViewIE(parent, id, url, pos, size, style, name); }
};

class VirtualProtocol : public wxIInternetProtocol
{
protected:
    wxIInternetProtocolSink* m_protocolSink;
    wxString m_html;
    VOID * fileP;

    wxFSFile* m_file;
    wxSharedPtr<wxWebViewHandler> m_handler;

public:
    VirtualProtocol(wxSharedPtr<wxWebViewHandler> handler);
    virtual ~VirtualProtocol() {}

    //IUnknown
    DECLARE_IUNKNOWN_METHODS;

    //IInternetProtocolRoot
    HRESULT STDMETHODCALLTYPE Abort(HRESULT WXUNUSED(hrReason),
                                    DWORD WXUNUSED(dwOptions))
                                   { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Continue(wxPROTOCOLDATA *WXUNUSED(pProtocolData))
                                       { return S_OK; }
    HRESULT STDMETHODCALLTYPE Resume() { return S_OK; }
    HRESULT STDMETHODCALLTYPE Start(LPCWSTR szUrl,
                                    wxIInternetProtocolSink *pOIProtSink,
                                    wxIInternetBindInfo *pOIBindInfo,
                                    DWORD grfPI,
                                    HANDLE_PTR dwReserved);
    HRESULT STDMETHODCALLTYPE Suspend() { return S_OK; }
    HRESULT STDMETHODCALLTYPE Terminate(DWORD WXUNUSED(dwOptions)) { return S_OK; }

    //IInternetProtocol
    HRESULT STDMETHODCALLTYPE LockRequest(DWORD WXUNUSED(dwOptions))
                                          { return S_OK; }
    HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead);
    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER WXUNUSED(dlibMove),
                                   DWORD WXUNUSED(dwOrigin),
                                   ULARGE_INTEGER* WXUNUSED(plibNewPosition))
                                   { return E_FAIL; }
    HRESULT STDMETHODCALLTYPE UnlockRequest() { return S_OK; }
};

class ClassFactory : public IClassFactory
{
public:
    ClassFactory(wxSharedPtr<wxWebViewHandler> handler) : m_handler(handler) 
        { AddRef(); }
    virtual ~ClassFactory() {}

    wxString GetName() { return m_handler->GetName(); }

    //IClassFactory
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter,
                                             REFIID riid, void** ppvObject);
    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock);

    //IUnknown
    DECLARE_IUNKNOWN_METHODS;

private:
    wxSharedPtr<wxWebViewHandler> m_handler;
};

class wxIEContainer : public wxActiveXContainer
{
public:
    wxIEContainer(wxWindow *parent, REFIID iid, IUnknown *pUnk, DocHostUIHandler* uiHandler = NULL);
    virtual ~wxIEContainer();
    virtual bool QueryClientSiteInterface(REFIID iid, void **_interface, const char *&desc);
private:
    DocHostUIHandler* m_uiHandler;
};

class DocHostUIHandler : public wxIDocHostUIHandler
{
public:
    DocHostUIHandler(wxWebView* browser) { m_browser = browser; }
    virtual ~DocHostUIHandler() {}

    virtual HRESULT wxSTDCALL ShowContextMenu(DWORD dwID, POINT *ppt,
                                              IUnknown *pcmdtReserved,
                                              IDispatch *pdispReserved);

    virtual HRESULT wxSTDCALL GetHostInfo(DOCHOSTUIINFO *pInfo);

    virtual HRESULT wxSTDCALL ShowUI(DWORD dwID,
                                     IOleInPlaceActiveObject *pActiveObject,
                                     IOleCommandTarget *pCommandTarget,
                                     IOleInPlaceFrame *pFrame,
                                     IOleInPlaceUIWindow *pDoc);

    virtual HRESULT wxSTDCALL HideUI(void);

    virtual HRESULT wxSTDCALL UpdateUI(void);

    virtual HRESULT wxSTDCALL EnableModeless(BOOL fEnable);

    virtual HRESULT wxSTDCALL OnDocWindowActivate(BOOL fActivate);

    virtual HRESULT wxSTDCALL OnFrameWindowActivate(BOOL fActivate);

    virtual HRESULT wxSTDCALL ResizeBorder(LPCRECT prcBorder,
                                           IOleInPlaceUIWindow *pUIWindow,
                                           BOOL fRameWindow);

    virtual HRESULT wxSTDCALL TranslateAccelerator(LPMSG lpMsg,
                                                   const GUID *pguidCmdGroup,
                                                   DWORD nCmdID);

    virtual HRESULT wxSTDCALL GetOptionKeyPath(LPOLESTR *pchKey,
                                               DWORD dw);

    virtual HRESULT wxSTDCALL GetDropTarget(IDropTarget *pDropTarget,
                                            IDropTarget **ppDropTarget);

    virtual HRESULT wxSTDCALL GetExternal(IDispatch **ppDispatch);

    virtual HRESULT wxSTDCALL TranslateUrl(DWORD dwTranslate,
                                           OLECHAR *pchURLIn,
                                           OLECHAR **ppchURLOut);

    virtual HRESULT wxSTDCALL FilterDataObject(IDataObject *pDO,
                                               IDataObject **ppDORet);
    //IUnknown
    DECLARE_IUNKNOWN_METHODS;

private:
    wxWebView* m_browser;
};

class wxFindPointers
{
public:
    wxFindPointers(wxIMarkupPointer *ptrBegin, wxIMarkupPointer *ptrEnd)
    {
        begin = ptrBegin;
        end = ptrEnd;
    }
    //The two markup pointers.
    wxIMarkupPointer *begin, *end;
};

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_IE && defined(__WXMSW__)

#endif // wxWebViewIE_H
