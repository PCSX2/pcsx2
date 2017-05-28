/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/msw/webviewhistoryitem.h
// Purpose:     wxWebViewHistoryItem header for MSW
// Author:      Steven Lamerton
// Copyright:   (c) 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_WEBVIEWHISTORYITEM_H_
#define _WX_MSW_WEBVIEWHISTORYITEM_H_

#include "wx/setup.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_IE && defined(__WXMSW__)

class WXDLLIMPEXP_WEBVIEW wxWebViewHistoryItem
{
public:
    wxWebViewHistoryItem(const wxString& url, const wxString& title) :
                     m_url(url), m_title(title) {}
    wxString GetUrl() { return m_url; }
    wxString GetTitle() { return m_title; }

private:
    wxString m_url, m_title;
};

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_IE && defined(__WXMSW__)

#endif // _WX_MSW_WEBVIEWHISTORYITEM_H_
