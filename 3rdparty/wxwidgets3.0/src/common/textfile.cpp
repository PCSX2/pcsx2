///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/textfile.cpp
// Purpose:     implementation of wxTextFile class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     03.04.98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// headers
// ============================================================================

#include  "wx/wxprec.h"

#ifdef    __BORLANDC__
    #pragma hdrstop
#endif  //__BORLANDC__

#if !wxUSE_FILE || !wxUSE_TEXTBUFFER
    #undef wxUSE_TEXTFILE
    #define wxUSE_TEXTFILE 0
#endif // wxUSE_FILE

#if wxUSE_TEXTFILE

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/file.h"
    #include "wx/log.h"
#endif

#include "wx/textfile.h"
#include "wx/filename.h"
#include "wx/buffer.h"

// ============================================================================
// wxTextFile class implementation
// ============================================================================

wxTextFile::wxTextFile(const wxString& strFileName)
          : wxTextBuffer(strFileName)
{
}


// ----------------------------------------------------------------------------
// file operations
// ----------------------------------------------------------------------------

bool wxTextFile::OnExists() const
{
    return wxFile::Exists(m_strBufferName);
}


bool wxTextFile::OnOpen(const wxString &strBufferName, wxTextBufferOpenMode OpenMode)
{
    wxFile::OpenMode FileOpenMode;

    switch ( OpenMode )
    {
        default:
            wxFAIL_MSG( wxT("unknown open mode in wxTextFile::Open") );
            // fall through

        case ReadAccess :
            FileOpenMode = wxFile::read;
            break;

        case WriteAccess :
            FileOpenMode = wxFile::write;
            break;
    }

    return m_file.Open(strBufferName.c_str(), FileOpenMode);
}


bool wxTextFile::OnClose()
{
    return m_file.Close();
}


bool wxTextFile::OnRead(const wxMBConv& conv)
{
    // file should be opened
    wxASSERT_MSG( m_file.IsOpened(), wxT("can't read closed file") );

    // read the entire file in memory: this is not the most efficient thing to
    // do it but there is no good way to avoid it in Unicode build because if
    // we read the file block by block we can't convert each block to Unicode
    // separately (the last multibyte char in the block might be only partially
    // read and so the conversion would fail) and, as the file contents is kept
    // in memory by wxTextFile anyhow, it shouldn't be a big problem to read
    // the file entirely
    size_t bufSize = 0;

    // number of bytes to (try to) read from disk at once
    static const size_t BLOCK_SIZE = 4096;

    wxCharBuffer buf;

    // first determine if the file is seekable or not and so whether we can
    // determine its length in advance
    wxFileOffset fileLength;
    {
        wxLogNull logNull;
        fileLength = m_file.Length();
    }

    // some non-seekable files under /proc under Linux pretend that they're
    // seekable but always return 0; others do return an error
    const bool seekable = fileLength != wxInvalidOffset && fileLength != 0;
    if ( seekable )
    {
        // we know the required length, so set the buffer size in advance
        bufSize = fileLength;
        if ( !buf.extend(bufSize) )
            return false;

        // if the file is seekable, also check that we're at its beginning
        wxASSERT_MSG( m_file.Tell() == 0, wxT("should be at start of file") );

        char *dst = buf.data();
        for ( size_t nRemaining = bufSize; nRemaining > 0; )
        {
            size_t nToRead = BLOCK_SIZE;

            // the file size could have changed, avoid overflowing the buffer
            // even if it did
            if ( nToRead > nRemaining )
                nToRead = nRemaining;

            ssize_t nRead = m_file.Read(dst, nToRead);

            if ( nRead == wxInvalidOffset )
            {
                // read error (error message already given in wxFile::Read)
                return false;
            }

            if ( nRead == 0 )
            {
                // this file can't be empty because we checked for this above
                // so this must be the end of file
                break;
            }

            dst += nRead;
            nRemaining -= nRead;
        }

        wxASSERT_MSG( dst - buf.data() == (wxFileOffset)bufSize,
                      wxT("logic error") );
    }
    else // file is not seekable
    {
        char block[BLOCK_SIZE];
        for ( ;; )
        {
            ssize_t nRead = m_file.Read(block, WXSIZEOF(block));

            if ( nRead == wxInvalidOffset )
            {
                // read error (error message already given in wxFile::Read)
                return false;
            }

            if ( nRead == 0 )
            {
                // if no bytes have been read, presumably this is a
                // valid-but-empty file
                if ( bufSize == 0 )
                    return true;

                // otherwise we've finished reading the file
                break;
            }

            // extend the buffer for new data
            if ( !buf.extend(bufSize + nRead) )
                return false;

            // and append it to the buffer
            memcpy(buf.data() + bufSize, block, nRead);
            bufSize += nRead;
        }
    }

    const wxString str(buf, conv, bufSize);

    // there's no risk of this happening in ANSI build
#if wxUSE_UNICODE
    if ( bufSize > 4 && str.empty() )
    {
        wxLogError(_("Failed to convert file \"%s\" to Unicode."), GetName());
        return false;
    }
#endif // wxUSE_UNICODE

    // we don't need this memory any more
    buf.reset();


    // now break the buffer in lines

    // the beginning of the current line, changes inside the loop
    wxString::const_iterator lineStart = str.begin();
    const wxString::const_iterator end = str.end();
    for ( wxString::const_iterator p = lineStart; p != end; p++ )
    {
        const wxChar ch = *p;
        if ( ch == '\r' || ch == '\n' )
        {
            // Determine the kind of line ending this is.
            wxTextFileType lineType = wxTextFileType_None;
            if ( ch == '\r' )
            {
                wxString::const_iterator next = p + 1;
                if ( next != end && *next == '\n' )
                    lineType = wxTextFileType_Dos;
                else
                    lineType = wxTextFileType_Mac;
            }
            else // ch == '\n'
            {
                lineType = wxTextFileType_Unix;
            }

            AddLine(wxString(lineStart, p), lineType);

            // DOS EOL is the only one consisting of two chars, not one.
            if ( lineType == wxTextFileType_Dos )
                p++;

            lineStart = p + 1;
        }
    }

    // anything in the last line?
    if ( lineStart != end )
    {
        // Add the last line; notice that it is certainly not terminated with a
        // newline, otherwise it would be handled above.
        wxString lastLine(lineStart, end);
        AddLine(lastLine, wxTextFileType_None);
    }

    return true;
}


bool wxTextFile::OnWrite(wxTextFileType typeNew, const wxMBConv& conv)
{
    wxFileName fn = m_strBufferName;

    // We do NOT want wxPATH_NORM_CASE here, or the case will not
    // be preserved.
    if ( !fn.IsAbsolute() )
        fn.Normalize(wxPATH_NORM_ENV_VARS | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE |
                     wxPATH_NORM_ABSOLUTE | wxPATH_NORM_LONG);

    wxTempFile fileTmp(fn.GetFullPath());

    if ( !fileTmp.IsOpened() ) {
        wxLogError(_("can't write buffer '%s' to disk."), m_strBufferName.c_str());
        return false;
    }

    size_t nCount = GetLineCount();
    for ( size_t n = 0; n < nCount; n++ ) {
        fileTmp.Write(GetLine(n) +
                      GetEOL(typeNew == wxTextFileType_None ? GetLineType(n)
                                                            : typeNew),
                      conv);
    }

    // replace the old file with this one
    return fileTmp.Commit();
}

#endif // wxUSE_TEXTFILE
