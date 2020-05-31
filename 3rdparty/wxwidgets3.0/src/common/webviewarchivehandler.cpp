/////////////////////////////////////////////////////////////////////////////
// Name:        webviewfilehandler.cpp
// Purpose:     Custom webview handler to allow archive browsing
// Author:      Steven Lamerton
// Copyright:   (c) 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_WEBVIEW

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#include "wx/webviewarchivehandler.h"
#include "wx/filesys.h"

//Taken from wx/filesys.cpp
static wxString EscapeFileNameCharsInURL(const char *in)
{
    wxString s;

    for ( const unsigned char *p = (const unsigned char*)in; *p; ++p )
    {
        const unsigned char c = *p;

        if ( c == '/' || c == '-' || c == '.' || c == '_' || c == '~' ||
             (c >= '0' && c <= '9') ||
             (c >= 'a' && c <= 'z') ||
             (c >= 'A' && c <= 'Z') )
        {
            s << c;
        }
        else
        {
            s << wxString::Format("%%%02x", c);
        }
    }

    return s;
}

wxWebViewArchiveHandler::wxWebViewArchiveHandler(const wxString& scheme) :
                         wxWebViewHandler(scheme)
{
    m_fileSystem = new wxFileSystem();
}

wxWebViewArchiveHandler::~wxWebViewArchiveHandler()
{
    wxDELETE(m_fileSystem);
}

wxFSFile* wxWebViewArchiveHandler::GetFile(const wxString &uri)
{
    //If there is a fragment at the end of the path then we need to strip it
    //off as not all backends do this for us
    wxString path = uri;
    size_t hashloc = uri.find('#');
    if(hashloc != wxString::npos)
    {
        path = uri.substr(0, hashloc);
    }

    //We iterate through the string to see if there is a protocol description
    size_t start = wxString::npos;
    for(size_t i = 0; i < path.length(); i++)
    {
        if(path[i] == ';' && path.substr(i, 10) == ";protocol=")
        {
            start = i;
            break;
        }
    }

    //We do not have a protocol string so we just pass the path withouth the 
    if(start == wxString::npos)
    {
        size_t doubleslash = path.find("//");
        //The path is incorrectly formed without // after the scheme
        if(doubleslash == wxString::npos)
            return NULL;

        wxString fspath = "file:" + 
                          EscapeFileNameCharsInURL(path.substr(doubleslash + 2).c_str());
        return m_fileSystem->OpenFile(fspath);
    }
    //Otherwise we need to extract the protocol
    else
    {
        size_t end = path.find('/', start);
        //For the path to be valid there must to a path after the protocol
        if(end == wxString::npos)
        {
            return NULL;
        }
        wxString mainpath = path.substr(0, start);
        wxString archivepath = path.substr(end);
        wxString protstring = path.substr(start, end - start);
        wxString protocol = protstring.substr(10);
        //We can now construct the correct path
        size_t doubleslash = path.find("//");
        //The path is incorrectly formed without // after the first protocol
        if(doubleslash == wxString::npos)
            return NULL;

        wxString fspath = "file:" + 
                          EscapeFileNameCharsInURL(mainpath.substr(doubleslash + 2).c_str())
                          + "#" + protocol +":" + archivepath;
        return m_fileSystem->OpenFile(fspath);
    }
}

#endif // wxUSE_WEBVIEW
