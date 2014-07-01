/////////////////////////////////////////////////////////////////////////////
// Name:        wx/html/webkit.h
// Purpose:     wxWebKitCtrl - embeddable web kit control
// Author:      Jethro Grassie / Kevin Ollivier
// Modified by:
// Created:     2004-4-16
// Copyright:   (c) Jethro Grassie / Kevin Ollivier
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WEBKIT_H
#define _WX_WEBKIT_H

#if wxUSE_WEBKIT

#if !defined(__WXMAC__) && !defined(__WXCOCOA__)
#error "wxWebKitCtrl not implemented for this platform"
#endif

#include "wx/control.h"
DECLARE_WXCOCOA_OBJC_CLASS(WebView); 

// ----------------------------------------------------------------------------
// Web Kit Control
// ----------------------------------------------------------------------------

extern WXDLLIMPEXP_DATA_CORE(const char) wxWebKitCtrlNameStr[];

class WXDLLIMPEXP_CORE wxWebKitCtrl : public wxControl
{
public:
    DECLARE_DYNAMIC_CLASS(wxWebKitCtrl)

    wxWebKitCtrl() {}
    wxWebKitCtrl(wxWindow *parent,
                    wxWindowID winID,
                    const wxString& strURL,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize, long style = 0,
                    const wxValidator& validator = wxDefaultValidator,
                    const wxString& name = wxWebKitCtrlNameStr)
    {
        Create(parent, winID, strURL, pos, size, style, validator, name);
    }
    bool Create(wxWindow *parent,
                wxWindowID winID,
                const wxString& strURL,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize, long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxWebKitCtrlNameStr);
    virtual ~wxWebKitCtrl();

    void LoadURL(const wxString &url);

    bool CanGoBack();
    bool CanGoForward();
    bool GoBack();
    bool GoForward();
    void Reload();
    void Stop();
    bool CanGetPageSource();
    wxString GetPageSource();
    void SetPageSource(const wxString& source, const wxString& baseUrl = wxEmptyString);
    wxString GetPageURL(){ return m_currentURL; }
    void SetPageTitle(const wxString& title) { m_pageTitle = title; }
    wxString GetPageTitle(){ return m_pageTitle; }

    // since these worked in 2.6, add wrappers
    void SetTitle(const wxString& title) { SetPageTitle(title); }
    wxString GetTitle() { return GetPageTitle(); }

    wxString GetSelection();

    bool CanIncreaseTextSize();
    void IncreaseTextSize();
    bool CanDecreaseTextSize();
    void DecreaseTextSize();

    void Print(bool showPrompt = false);

    void MakeEditable(bool enable = true);
    bool IsEditable();

    wxString RunScript(const wxString& javascript);

    void SetScrollPos(int pos);
    int GetScrollPos();

    // don't hide base class virtuals
    virtual void SetScrollPos( int orient, int pos, bool refresh = true )
        { return wxControl::SetScrollPos(orient, pos, refresh); }
    virtual int GetScrollPos( int orient ) const
        { return wxControl::GetScrollPos(orient); }

    //we need to resize the webview when the control size changes
    void OnSize(wxSizeEvent &event);
    void OnMove(wxMoveEvent &event);
    void OnMouseEvents(wxMouseEvent &event);
protected:
    DECLARE_EVENT_TABLE()
    void MacVisibilityChanged();

private:
    wxWindow *m_parent;
    wxWindowID m_windowID;
    wxString m_currentURL;
    wxString m_pageTitle;

    WX_WebView m_webView;

    // we may use this later to setup our own mouse events,
    // so leave it in for now.
    void* m_webKitCtrlEventHandler;
};

// ----------------------------------------------------------------------------
// Web Kit Events
// ----------------------------------------------------------------------------

enum {
    wxWEBKIT_STATE_START = 1,
    wxWEBKIT_STATE_NEGOTIATING = 2,
    wxWEBKIT_STATE_REDIRECTING = 4,
    wxWEBKIT_STATE_TRANSFERRING = 8,
    wxWEBKIT_STATE_STOP = 16,
        wxWEBKIT_STATE_FAILED = 32
};

enum {
    wxWEBKIT_NAV_LINK_CLICKED = 1,
    wxWEBKIT_NAV_BACK_NEXT = 2,
    wxWEBKIT_NAV_FORM_SUBMITTED = 4,
    wxWEBKIT_NAV_RELOAD = 8,
    wxWEBKIT_NAV_FORM_RESUBMITTED = 16,
    wxWEBKIT_NAV_OTHER = 32

};



class WXDLLIMPEXP_CORE wxWebKitBeforeLoadEvent : public wxCommandEvent
{
    DECLARE_DYNAMIC_CLASS( wxWebKitBeforeLoadEvent )

public:
    bool IsCancelled() { return m_cancelled; }
    void Cancel(bool cancel = true) { m_cancelled = cancel; }
    wxString GetURL() { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }
    void SetNavigationType(int navType) { m_navType = navType; }
    int GetNavigationType() { return m_navType; }

    wxWebKitBeforeLoadEvent( wxWindow* win = NULL );
    wxEvent *Clone(void) const { return new wxWebKitBeforeLoadEvent(*this); }

protected:
    bool m_cancelled;
    wxString m_url;
    int m_navType;
};

class WXDLLIMPEXP_CORE wxWebKitStateChangedEvent : public wxCommandEvent
{
    DECLARE_DYNAMIC_CLASS( wxWebKitStateChangedEvent )

public:
    int GetState() { return m_state; }
    void SetState(const int state) { m_state = state; }
    wxString GetURL() { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }

    wxWebKitStateChangedEvent( wxWindow* win = NULL );
    wxEvent *Clone(void) const { return new wxWebKitStateChangedEvent(*this); }

protected:
    int m_state;
    wxString m_url;
};


class WXDLLIMPEXP_CORE wxWebKitNewWindowEvent : public wxCommandEvent
{
    DECLARE_DYNAMIC_CLASS( wxWebKitNewWindowEvent )
public:
    wxString GetURL() const { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }
    wxString GetTargetName() const { return m_targetName; }
    void SetTargetName(const wxString& name) { m_targetName = name; }

    wxWebKitNewWindowEvent( wxWindow* win = (wxWindow*)(NULL));
    wxEvent *Clone(void) const { return new wxWebKitNewWindowEvent(*this); }

private:
    wxString m_url;
    wxString m_targetName;
};

typedef void (wxEvtHandler::*wxWebKitStateChangedEventFunction)(wxWebKitStateChangedEvent&);
typedef void (wxEvtHandler::*wxWebKitBeforeLoadEventFunction)(wxWebKitBeforeLoadEvent&);
typedef void (wxEvtHandler::*wxWebKitNewWindowEventFunction)(wxWebKitNewWindowEvent&);

#define wxWebKitStateChangedEventHandler( func ) \
    wxEVENT_HANDLER_CAST( wxWebKitStateChangedEventFunction, func )

#define wxWebKitBeforeLoadEventHandler( func ) \
    wxEVENT_HANDLER_CAST( wxWebKitBeforeLoadEventFunction, func )

#define wxWebKitNewWindowEventHandler( func ) \
    wxEVENT_HANDLER_CAST( wxWebKitNewWindowEventFunction, func )

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_WEBKIT_STATE_CHANGED, wxWebKitStateChangedEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_WEBKIT_BEFORE_LOAD, wxWebKitBeforeLoadEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_WEBKIT_NEW_WINDOW, wxWebKitNewWindowEvent );

#define EVT_WEBKIT_STATE_CHANGED(func) \
            wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBKIT_STATE_CHANGED, \
                            wxID_ANY, \
                            wxID_ANY, \
                            wxWebKitStateChangedEventHandler( func ), \
                            NULL ),

#define EVT_WEBKIT_BEFORE_LOAD(func) \
            wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBKIT_BEFORE_LOAD, \
                            wxID_ANY, \
                            wxID_ANY, \
                            wxWebKitBeforeLoadEventHandler( func ), \
                            NULL ),

#define EVT_WEBKIT_NEW_WINDOW(func)                              \
            wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBKIT_NEW_WINDOW, \
                            wxID_ANY, \
                            wxID_ANY, \
                            wxWebKitNewWindowEventHandler( func ), \
                            NULL ),
#endif // wxUSE_WEBKIT

#endif
    // _WX_WEBKIT_H_
