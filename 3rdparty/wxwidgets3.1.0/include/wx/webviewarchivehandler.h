/////////////////////////////////////////////////////////////////////////////
// Name:        webviewarchivehandler.h
// Purpose:     Custom webview handler to allow archive browsing
// Author:      Steven Lamerton
// Copyright:   (c) 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WEBVIEW_FILE_HANDLER_H_
#define _WX_WEBVIEW_FILE_HANDLER_H_

#include "wx/setup.h"

#if wxUSE_WEBVIEW

class wxFSFile;
class wxFileSystem;

#include "wx/webview.h"

//Loads from uris such as scheme:///C:/example/example.html or archives such as
//scheme:///C:/example/example.zip;protocol=zip/example.html

class WXDLLIMPEXP_WEBVIEW wxWebViewArchiveHandler : public wxWebViewHandler
{
public:
    wxWebViewArchiveHandler(const wxString& scheme);
    virtual ~wxWebViewArchiveHandler();
    virtual wxFSFile* GetFile(const wxString &uri) wxOVERRIDE;
private:
    wxFileSystem* m_fileSystem;
};

#endif // wxUSE_WEBVIEW

#endif // _WX_WEBVIEW_FILE_HANDLER_H_
