///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/dobjcmn.cpp
// Purpose:     implementation of data object methods common to all platforms
// Author:      Vadim Zeitlin, Robert Roebling
// Modified by:
// Created:     19.10.99
// Copyright:   (c) wxWidgets Team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_DATAOBJ

#include "wx/dataobj.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif

#include "wx/textbuf.h"

// ----------------------------------------------------------------------------
// lists
// ----------------------------------------------------------------------------

#include "wx/listimpl.cpp"

WX_DEFINE_LIST(wxSimpleDataObjectList)

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

static wxDataFormat dataFormatInvalid;
WXDLLEXPORT const wxDataFormat& wxFormatInvalid = dataFormatInvalid;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDataObjectBase
// ----------------------------------------------------------------------------

wxDataObjectBase::~wxDataObjectBase()
{
}

bool wxDataObjectBase::IsSupported(const wxDataFormat& format,
                                   Direction dir) const
{
    size_t nFormatCount = GetFormatCount( dir );
    if ( nFormatCount == 1 )
    {
        return format == GetPreferredFormat( dir );
    }
    else
    {
        wxDataFormat *formats = new wxDataFormat[nFormatCount];
        GetAllFormats( formats, dir );

        size_t n;
        for ( n = 0; n < nFormatCount; n++ )
        {
            if ( formats[n] == format )
                break;
        }

        delete [] formats;

        // found?
        return n < nFormatCount;
    }
}

// ----------------------------------------------------------------------------
// wxDataObjectComposite
// ----------------------------------------------------------------------------

wxDataObjectComposite::wxDataObjectComposite()
{
    m_preferred = 0;
    m_receivedFormat = wxFormatInvalid;
}

wxDataObjectComposite::~wxDataObjectComposite()
{
    WX_CLEAR_LIST( wxSimpleDataObjectList, m_dataObjects );
}

wxDataObjectSimple *
wxDataObjectComposite::GetObject(const wxDataFormat& format, wxDataObjectBase::Direction dir) const
{
    wxSimpleDataObjectList::compatibility_iterator node = m_dataObjects.GetFirst();

    while ( node )
    {
        wxDataObjectSimple *dataObj = node->GetData();

        if (dataObj->IsSupported(format,dir))
          return dataObj;
        node = node->GetNext();
    }
    return NULL;
}

void wxDataObjectComposite::Add(wxDataObjectSimple *dataObject, bool preferred)
{
    if ( preferred )
        m_preferred = m_dataObjects.GetCount();

    m_dataObjects.Append( dataObject );
}

wxDataFormat wxDataObjectComposite::GetReceivedFormat() const
{
    return m_receivedFormat;
}

wxDataFormat
wxDataObjectComposite::GetPreferredFormat(Direction WXUNUSED(dir)) const
{
    wxSimpleDataObjectList::compatibility_iterator node = m_dataObjects.Item( m_preferred );

    wxCHECK_MSG( node, wxFormatInvalid, wxT("no preferred format") );

    wxDataObjectSimple* dataObj = node->GetData();

    return dataObj->GetFormat();
}

#if defined(__WXMSW__)

size_t wxDataObjectComposite::GetBufferOffset( const wxDataFormat& format )
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, 0,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetBufferOffset( format );
}


const void* wxDataObjectComposite::GetSizeFromBuffer( const void* buffer,
                                                      size_t* size,
                                                      const wxDataFormat& format )
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, NULL,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetSizeFromBuffer( buffer, size, format );
}


void* wxDataObjectComposite::SetSizeInBuffer( void* buffer, size_t size,
                                              const wxDataFormat& format )
{
    wxDataObjectSimple *dataObj = GetObject( format );

    wxCHECK_MSG( dataObj, NULL,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->SetSizeInBuffer( buffer, size, format );
}

#endif

size_t wxDataObjectComposite::GetFormatCount(Direction dir) const
{
    size_t n = 0;

    // NOTE: some wxDataObjectSimple objects may return a number greater than 1
    //       from GetFormatCount(): this is the case of e.g. wxTextDataObject
    //       under wxMac and wxGTK
    wxSimpleDataObjectList::compatibility_iterator node;
    for ( node = m_dataObjects.GetFirst(); node; node = node->GetNext() )
        n += node->GetData()->GetFormatCount(dir);

    return n;
}

void wxDataObjectComposite::GetAllFormats(wxDataFormat *formats,
                                          Direction dir) const
{
    size_t index(0);
    wxSimpleDataObjectList::compatibility_iterator node;

    for ( node = m_dataObjects.GetFirst(); node; node = node->GetNext() )
    {
        // NOTE: some wxDataObjectSimple objects may return more than 1 format
        //       from GetAllFormats(): this is the case of e.g. wxTextDataObject
        //       under wxMac and wxGTK
        node->GetData()->GetAllFormats(formats+index, dir);
        index += node->GetData()->GetFormatCount(dir);
    }
}

size_t wxDataObjectComposite::GetDataSize(const wxDataFormat& format) const
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, 0,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetDataSize();
}

bool wxDataObjectComposite::GetDataHere(const wxDataFormat& format,
                                        void *buf) const
{
    wxDataObjectSimple *dataObj = GetObject( format );

    wxCHECK_MSG( dataObj, false,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetDataHere( buf );
}

bool wxDataObjectComposite::SetData(const wxDataFormat& format,
                                    size_t len,
                                    const void *buf)
{
    wxDataObjectSimple *dataObj = GetObject( format );

    wxCHECK_MSG( dataObj, false,
                 wxT("unsupported format in wxDataObjectComposite"));

    m_receivedFormat = format;

    // Notice that we must pass "format" here as wxTextDataObject, that we can
    // have as one of our "simple" sub-objects actually is not that simple and
    // can support multiple formats (ASCII/UTF-8/UTF-16/...) and so needs to
    // know which one it is given.
    return dataObj->SetData( format, len, buf );
}

// ----------------------------------------------------------------------------
// wxTextDataObject
// ----------------------------------------------------------------------------

#ifdef wxNEEDS_UTF8_FOR_TEXT_DATAOBJ

// FIXME-UTF8: we should be able to merge wchar_t and UTF-8 versions once we
//             have a way to get UTF-8 string (and its length) in both builds
//             without loss of efficiency (i.e. extra buffer copy/strlen call)

#if wxUSE_UNICODE_WCHAR

static inline wxMBConv& GetConv(const wxDataFormat& format)
{
    // use UTF8 for wxDF_UNICODETEXT and UCS4 for wxDF_TEXT
    return format == wxDF_UNICODETEXT ? wxConvUTF8 : wxConvLibc;
}

size_t wxTextDataObject::GetDataSize(const wxDataFormat& format) const
{
    wxCharBuffer buffer = GetConv(format).cWX2MB( GetText().c_str() );

    return buffer ? strlen( buffer ) : 0;
}

bool wxTextDataObject::GetDataHere(const wxDataFormat& format, void *buf) const
{
    if ( !buf )
        return false;

    wxCharBuffer buffer = GetConv(format).cWX2MB( GetText().c_str() );
    if ( !buffer )
        return false;

    memcpy( (char*) buf, buffer, GetDataSize(format) );
    // strcpy( (char*) buf, buffer );

    return true;
}

bool wxTextDataObject::SetData(const wxDataFormat& format,
                               size_t len, const void *buf)
{
    if ( buf == NULL )
        return false;

    wxWCharBuffer buffer = GetConv(format).cMB2WC((const char*)buf, len, NULL);

    SetText( buffer );

    return true;
}

#else // wxUSE_UNICODE_UTF8

size_t wxTextDataObject::GetDataSize(const wxDataFormat& format) const
{
    const wxString& text = GetText();
    if ( format == wxDF_UNICODETEXT || wxLocaleIsUtf8 )
    {
        return text.utf8_length();
    }
    else // wxDF_TEXT
    {
        const wxCharBuffer buf(wxConvLocal.cWC2MB(text.wc_str()));
        return buf ? strlen(buf) : 0;
    }
}

bool wxTextDataObject::GetDataHere(const wxDataFormat& format, void *buf) const
{
    if ( !buf )
        return false;

    const wxString& text = GetText();
    if ( format == wxDF_UNICODETEXT || wxLocaleIsUtf8 )
    {
        memcpy(buf, text.utf8_str(), text.utf8_length());
    }
    else // wxDF_TEXT
    {
        const wxCharBuffer bufLocal(wxConvLocal.cWC2MB(text.wc_str()));
        if ( !bufLocal )
            return false;

        memcpy(buf, bufLocal, strlen(bufLocal));
    }

    return true;
}

bool wxTextDataObject::SetData(const wxDataFormat& format,
                               size_t len, const void *buf_)
{
    const char * const buf = static_cast<const char *>(buf_);

    if ( buf == NULL )
        return false;

    if ( format == wxDF_UNICODETEXT || wxLocaleIsUtf8 )
    {
        // normally the data is in UTF-8 so we could use FromUTF8Unchecked()
        // but it's not absolutely clear what GTK+ does if the clipboard data
        // is not in UTF-8 so do an extra check for tranquility, it shouldn't
        // matter much if we lose a bit of performance when pasting from
        // clipboard
        SetText(wxString::FromUTF8(buf, len));
    }
    else // wxDF_TEXT, convert from current (non-UTF8) locale
    {
        SetText(wxConvLocal.cMB2WC(buf, len, NULL));
    }

    return true;
}

#endif // wxUSE_UNICODE_WCHAR/wxUSE_UNICODE_UTF8

#elif defined(wxNEEDS_UTF16_FOR_TEXT_DATAOBJ)

namespace
{

inline wxMBConv& GetConv(const wxDataFormat& format)
{
    static wxMBConvUTF16 s_UTF16Converter;

    return format == wxDF_UNICODETEXT ? static_cast<wxMBConv&>(s_UTF16Converter)
                                      : static_cast<wxMBConv&>(wxConvLocal);
}

} // anonymous namespace

size_t wxTextDataObject::GetDataSize(const wxDataFormat& format) const
{
    return GetConv(format).WC2MB(NULL, GetText().wc_str(), 0);
}

bool wxTextDataObject::GetDataHere(const wxDataFormat& format, void *buf) const
{
    if ( buf == NULL )
        return false;

    wxCharBuffer buffer(GetConv(format).cWX2MB(GetText().c_str()));

    memcpy(buf, buffer.data(), buffer.length());

    return true;
}

bool wxTextDataObject::SetData(const wxDataFormat& format,
                               size_t WXUNUSED(len),
                               const void *buf)
{
    if ( buf == NULL )
        return false;

    SetText(GetConv(format).cMB2WX(static_cast<const char*>(buf)));

    return true;
}

#else // !wxNEEDS_UTF{8,16}_FOR_TEXT_DATAOBJ

// NB: This branch, using native wxChar for the clipboard, is only used under
//     Windows currently. It's just a coincidence, but Windows is also the only
//     platform where we need to convert the text to the native EOL format, so
//     wxTextBuffer::Translate() is only used here and not in the code above.

size_t wxTextDataObject::GetDataSize() const
{
    return (wxTextBuffer::Translate(GetText()).length() + 1)*sizeof(wxChar);
}

bool wxTextDataObject::GetDataHere(void *buf) const
{
    const wxString textNative = wxTextBuffer::Translate(GetText());

    // NOTE: use wxTmemcpy() instead of wxStrncpy() to allow
    //       retrieval of strings with embedded NULLs
    wxTmemcpy(static_cast<wxChar*>(buf),
              textNative.t_str(),
              textNative.length() + 1);

    return true;
}

bool wxTextDataObject::SetData(size_t len, const void *buf)
{
    const wxString
        text = wxString(static_cast<const wxChar*>(buf), len/sizeof(wxChar));
    SetText(wxTextBuffer::Translate(text, wxTextFileType_Unix));

    return true;
}

#endif // different wxTextDataObject implementations

// ----------------------------------------------------------------------------
// wxHTMLDataObject
// ----------------------------------------------------------------------------

size_t wxHTMLDataObject::GetDataSize() const
{
    // Ensure that the temporary string returned by GetHTML() is kept alive for
    // as long as we need it here.
    const wxString& htmlStr = GetHTML();
    const wxScopedCharBuffer buffer(htmlStr.utf8_str());

    size_t size = buffer.length();

#ifdef __WXMSW__
    // On Windows we need to add some stuff to the string to satisfy
    // its clipboard format requirements.
    size += 400;
#endif

    return size;
}

bool wxHTMLDataObject::GetDataHere(void *buf) const
{
    if ( !buf )
        return false;

    // Windows and Mac always use UTF-8, and docs suggest GTK does as well.
    const wxString& htmlStr = GetHTML();
    const wxScopedCharBuffer html(htmlStr.utf8_str());
    if ( !html )
        return false;

    char* const buffer = static_cast<char*>(buf);

#ifdef __WXMSW__
    // add the extra info that the MSW clipboard format requires.

        // Create a template string for the HTML header...
    strcpy(buffer,
        "Version:0.9\r\n"
        "StartHTML:00000000\r\n"
        "EndHTML:00000000\r\n"
        "StartFragment:00000000\r\n"
        "EndFragment:00000000\r\n"
        "<html><body>\r\n"
        "<!--StartFragment -->\r\n");

    // Append the HTML...
    strcat(buffer, html);
    strcat(buffer, "\r\n");
    // Finish up the HTML format...
    strcat(buffer,
        "<!--EndFragment-->\r\n"
        "</body>\r\n"
        "</html>");

    // Now go back, calculate all the lengths, and write out the
    // necessary header information. Note, wsprintf() truncates the
    // string when you overwrite it so you follow up with code to replace
    // the 0 appended at the end with a '\r'...
    char *ptr = strstr(buffer, "StartHTML");
    sprintf(ptr+10, "%08u", (unsigned)(strstr(buffer, "<html>") - buffer));
    *(ptr+10+8) = '\r';

    ptr = strstr(buffer, "EndHTML");
    sprintf(ptr+8, "%08u", (unsigned)strlen(buffer));
    *(ptr+8+8) = '\r';

    ptr = strstr(buffer, "StartFragment");
    sprintf(ptr+14, "%08u", (unsigned)(strstr(buffer, "<!--StartFrag") - buffer));
    *(ptr+14+8) = '\r';

    ptr = strstr(buffer, "EndFragment");
    sprintf(ptr+12, "%08u", (unsigned)(strstr(buffer, "<!--EndFrag") - buffer));
    *(ptr+12+8) = '\r';
#else
    strcpy(buffer, html);
#endif // __WXMSW__

    return true;
}

bool wxHTMLDataObject::SetData(size_t WXUNUSED(len), const void *buf)
{
    if ( buf == NULL )
        return false;

    // Windows and Mac always use UTF-8, and docs suggest GTK does as well.
    wxString html = wxString::FromUTF8(static_cast<const char*>(buf));

#ifdef __WXMSW__
    // To be consistent with other platforms, we only add the Fragment part
    // of the Windows HTML clipboard format to the data object.
    int fragmentStart = html.rfind("StartFragment");
    int fragmentEnd = html.rfind("EndFragment");

    if (fragmentStart != wxNOT_FOUND && fragmentEnd != wxNOT_FOUND)
    {
        int startCommentEnd = html.find("-->", fragmentStart) + 3;
        int endCommentStart = html.rfind("<!--", fragmentEnd);

        if (startCommentEnd != wxNOT_FOUND && endCommentStart != wxNOT_FOUND)
            html = html.Mid(startCommentEnd, endCommentStart - startCommentEnd);
    }
#endif // __WXMSW__

    SetHTML( html );

    return true;
}


// ----------------------------------------------------------------------------
// wxCustomDataObject
// ----------------------------------------------------------------------------

wxCustomDataObject::wxCustomDataObject(const wxDataFormat& format)
    : wxDataObjectSimple(format)
{
    m_data = NULL;
    m_size = 0;
}

wxCustomDataObject::~wxCustomDataObject()
{
    Free();
}

void wxCustomDataObject::TakeData(size_t size, void *data)
{
    Free();

    m_size = size;
    m_data = data;
}

void *wxCustomDataObject::Alloc(size_t size)
{
    return (void *)new char[size];
}

void wxCustomDataObject::Free()
{
    delete [] (char*)m_data;
    m_size = 0;
    m_data = NULL;
}

size_t wxCustomDataObject::GetDataSize() const
{
    return GetSize();
}

bool wxCustomDataObject::GetDataHere(void *buf) const
{
    if ( buf == NULL )
        return false;

    void *data = GetData();
    if ( data == NULL )
        return false;

    memcpy( buf, data, GetSize() );

    return true;
}

bool wxCustomDataObject::SetData(size_t size, const void *buf)
{
    Free();

    m_data = Alloc(size);
    if ( m_data == NULL )
        return false;

    m_size = size;
    memcpy( m_data, buf, m_size );

    return true;
}

// ============================================================================
// some common dnd related code
// ============================================================================

#if wxUSE_DRAG_AND_DROP

#include "wx/dnd.h"

// ----------------------------------------------------------------------------
// wxTextDropTarget
// ----------------------------------------------------------------------------

// NB: we can't use "new" in ctor initializer lists because this provokes an
//     internal compiler error with VC++ 5.0 (hey, even gcc compiles this!),
//     so use SetDataObject() instead

wxTextDropTarget::wxTextDropTarget()
{
    SetDataObject(new wxTextDataObject);
}

wxDragResult wxTextDropTarget::OnData(wxCoord x, wxCoord y, wxDragResult def)
{
    if ( !GetData() )
        return wxDragNone;

    wxTextDataObject *dobj = (wxTextDataObject *)m_dataObject;
    return OnDropText( x, y, dobj->GetText() ) ? def : wxDragNone;
}

// ----------------------------------------------------------------------------
// wxFileDropTarget
// ----------------------------------------------------------------------------

wxFileDropTarget::wxFileDropTarget()
{
    SetDataObject(new wxFileDataObject);
}

wxDragResult wxFileDropTarget::OnData(wxCoord x, wxCoord y, wxDragResult def)
{
    if ( !GetData() )
        return wxDragNone;

    wxFileDataObject *dobj = (wxFileDataObject *)m_dataObject;
    return OnDropFiles( x, y, dobj->GetFilenames() ) ? def : wxDragNone;
}

#endif // wxUSE_DRAG_AND_DROP

#endif // wxUSE_DATAOBJ
