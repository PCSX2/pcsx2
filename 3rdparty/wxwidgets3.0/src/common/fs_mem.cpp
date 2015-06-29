/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/fs_mem.cpp
// Purpose:     in-memory file system
// Author:      Vaclav Slavik
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_FILESYSTEM && wxUSE_STREAMS

#include "wx/fs_mem.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/wxcrtvararg.h"
    #if wxUSE_GUI
        #include "wx/image.h"
    #endif // wxUSE_GUI
#endif

#include "wx/mstream.h"

// represents a file entry in wxMemoryFS
class wxMemoryFSFile
{
public:
    wxMemoryFSFile(const void *data, size_t len, const wxString& mime)
    {
        m_Data = new char[len];
        memcpy(m_Data, data, len);
        m_Len = len;
        m_MimeType = mime;
        InitTime();
    }

    wxMemoryFSFile(const wxMemoryOutputStream& stream, const wxString& mime)
    {
        m_Len = stream.GetSize();
        m_Data = new char[m_Len];
        stream.CopyTo(m_Data, m_Len);
        m_MimeType = mime;
        InitTime();
    }

    virtual ~wxMemoryFSFile()
    {
        delete[] m_Data;
    }

    char *m_Data;
    size_t m_Len;
    wxString m_MimeType;
#if wxUSE_DATETIME
    wxDateTime m_Time;
#endif // wxUSE_DATETIME

private:
    void InitTime()
    {
#if wxUSE_DATETIME
        m_Time = wxDateTime::Now();
#endif // wxUSE_DATETIME
    }

    wxDECLARE_NO_COPY_CLASS(wxMemoryFSFile);
};

#if wxUSE_BASE


//--------------------------------------------------------------------------------
// wxMemoryFSHandler
//--------------------------------------------------------------------------------


wxMemoryFSHash wxMemoryFSHandlerBase::m_Hash;


wxMemoryFSHandlerBase::wxMemoryFSHandlerBase() : wxFileSystemHandler()
{
}

wxMemoryFSHandlerBase::~wxMemoryFSHandlerBase()
{
    // as only one copy of FS handler is supposed to exist, we may silently
    // delete static data here. (There is no way how to remove FS handler from
    // wxFileSystem other than releasing _all_ handlers.)
    WX_CLEAR_HASH_MAP(wxMemoryFSHash, m_Hash);
}

bool wxMemoryFSHandlerBase::CanOpen(const wxString& location)
{
    return GetProtocol(location) == "memory";
}

wxFSFile * wxMemoryFSHandlerBase::OpenFile(wxFileSystem& WXUNUSED(fs),
                                           const wxString& location)
{
    wxMemoryFSHash::const_iterator i = m_Hash.find(GetRightLocation(location));
    if ( i == m_Hash.end() )
        return NULL;

    const wxMemoryFSFile * const obj = i->second;

    return new wxFSFile
               (
                    new wxMemoryInputStream(obj->m_Data, obj->m_Len),
                    location,
                    obj->m_MimeType,
                    GetAnchor(location)
#if wxUSE_DATETIME
                    , obj->m_Time
#endif // wxUSE_DATETIME
               );
}

wxString wxMemoryFSHandlerBase::FindFirst(const wxString& url, int flags)
{
    if ( (flags & wxDIR) && !(flags & wxFILE) )
    {
        // we only store files, not directories, so we don't risk finding
        // anything
        return wxString();
    }

    const wxString spec = GetRightLocation(url);
    if ( spec.find_first_of("?*") == wxString::npos )
    {
        // simple case: there are no wildcard characters so we can return
        // either 0 or 1 results and we can find the potential match quickly
        return m_Hash.count(spec) ? url : wxString();
    }
    //else: deal with wildcards in FindNext()

    m_findArgument = spec;
    m_findIter = m_Hash.begin();

    return FindNext();
}

wxString wxMemoryFSHandlerBase::FindNext()
{
    // m_findArgument is used to indicate that search is in progress, we reset
    // it to empty string after iterating over all elements
    while ( !m_findArgument.empty() )
    {
        // test for the match before (possibly) clearing m_findArgument below
        const bool found = m_findIter->first.Matches(m_findArgument);

        // advance m_findIter first as we need to do it anyhow, whether it
        // matches or not
        const wxMemoryFSHash::const_iterator current = m_findIter;

        if ( ++m_findIter == m_Hash.end() )
            m_findArgument.clear();

        if ( found )
            return "memory:" + current->first;
    }

    return wxString();
}

bool wxMemoryFSHandlerBase::CheckDoesntExist(const wxString& filename)
{
    if ( m_Hash.count(filename) )
    {
        wxLogError(_("Memory VFS already contains file '%s'!"), filename);
        return false;
    }

    return true;
}


/*static*/
void wxMemoryFSHandlerBase::AddFileWithMimeType(const wxString& filename,
                                                const wxString& textdata,
                                                const wxString& mimetype)
{
    const wxCharBuffer buf(textdata.To8BitData());

    AddFileWithMimeType(filename, buf.data(), buf.length(), mimetype);
}


/*static*/
void wxMemoryFSHandlerBase::AddFileWithMimeType(const wxString& filename,
                                                const void *binarydata, size_t size,
                                                const wxString& mimetype)
{
    if ( !CheckDoesntExist(filename) )
        return;

    m_Hash[filename] = new wxMemoryFSFile(binarydata, size, mimetype);
}

/*static*/
void wxMemoryFSHandlerBase::AddFile(const wxString& filename,
                                    const wxString& textdata)
{
    AddFileWithMimeType(filename, textdata, wxEmptyString);
}


/*static*/
void wxMemoryFSHandlerBase::AddFile(const wxString& filename,
                                    const void *binarydata, size_t size)
{
    AddFileWithMimeType(filename, binarydata, size, wxEmptyString);
}



/*static*/ void wxMemoryFSHandlerBase::RemoveFile(const wxString& filename)
{
    wxMemoryFSHash::iterator i = m_Hash.find(filename);
    if ( i == m_Hash.end() )
    {
        wxLogError(_("Trying to remove file '%s' from memory VFS, "
                     "but it is not loaded!"),
                   filename);
        return;
    }

    delete i->second;
    m_Hash.erase(i);
}

#endif // wxUSE_BASE

#if wxUSE_GUI

#if wxUSE_IMAGE
/*static*/ void
wxMemoryFSHandler::AddFile(const wxString& filename,
                           const wxImage& image,
                           wxBitmapType type)
{
    if ( !CheckDoesntExist(filename) )
        return;

    wxMemoryOutputStream mems;
    if ( image.IsOk() && image.SaveFile(mems, type) )
    {
        m_Hash[filename] = new wxMemoryFSFile
                               (
                                    mems,
                                    wxImage::FindHandler(type)->GetMimeType()
                               );
    }
    else
    {
        wxLogError(_("Failed to store image '%s' to memory VFS!"), filename);
    }
}

/*static*/ void
wxMemoryFSHandler::AddFile(const wxString& filename,
                           const wxBitmap& bitmap,
                           wxBitmapType type)
{
    wxImage img = bitmap.ConvertToImage();
    AddFile(filename, img, type);
}

#endif // wxUSE_IMAGE

#endif // wxUSE_GUI


#endif // wxUSE_FILESYSTEM && wxUSE_FS_ZIP
