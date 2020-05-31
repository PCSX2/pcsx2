///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/enhmeta.cpp
// Purpose:     implementation of wxEnhMetaFileXXX classes
// Author:      Vadim Zeitlin
// Modified by:
// Created:     13.01.00
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_ENH_METAFILE

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
#endif //WX_PRECOMP

#include "wx/dc.h"
#include "wx/msw/dc.h"

#include "wx/metafile.h"
#include "wx/clipbrd.h"

#include "wx/msw/private.h"

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxEnhMetaFile, wxObject)

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

#define GetEMF()            ((HENHMETAFILE)m_hMF)
#define GetEMFOf(mf)        ((HENHMETAFILE)((mf).m_hMF))

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// we must pass NULL if the string is empty to metafile functions
static inline const wxChar *GetMetaFileName(const wxString& fn)
    { return !fn ? NULL : wxMSW_CONV_LPCTSTR(fn); }

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxEnhMetaFile
// ----------------------------------------------------------------------------

wxGDIRefData *wxEnhMetaFile::CreateGDIRefData() const
{
    wxFAIL_MSG( wxT("must be implemented if used") );

    return NULL;
}

wxGDIRefData *
wxEnhMetaFile::CloneGDIRefData(const wxGDIRefData *WXUNUSED(data)) const
{
    wxFAIL_MSG( wxT("must be implemented if used") );

    return NULL;
}

void wxEnhMetaFile::Init()
{
    if ( m_filename.empty() )
    {
        m_hMF = 0;
    }
    else // have valid file name, load metafile from it
    {
        m_hMF = (WXHANDLE)::GetEnhMetaFile(m_filename.t_str());
        if ( !m_hMF )
        {
            wxLogSysError(_("Failed to load metafile from file \"%s\"."),
                          m_filename.c_str());
        }
    }
}

void wxEnhMetaFile::Assign(const wxEnhMetaFile& mf)
{
    if ( &mf == this )
        return;

    if ( mf.m_hMF )
    {
        m_hMF = (WXHANDLE)::CopyEnhMetaFile(GetEMFOf(mf),
                                            GetMetaFileName(m_filename));
        if ( !m_hMF )
        {
            wxLogLastError(wxT("CopyEnhMetaFile"));
        }
    }
    else
    {
        m_hMF = 0;
    }
}

void wxEnhMetaFile::Free()
{
    if ( m_hMF )
    {
        if ( !::DeleteEnhMetaFile(GetEMF()) )
        {
            wxLogLastError(wxT("DeleteEnhMetaFile"));
        }
    }
}

bool wxEnhMetaFile::Play(wxDC *dc, wxRect *rectBound)
{
    wxCHECK_MSG( IsOk(), false, wxT("can't play invalid enhanced metafile") );
    wxCHECK_MSG( dc, false, wxT("invalid wxDC in wxEnhMetaFile::Play") );

    RECT rect;
    if ( rectBound )
    {
        rect.top = rectBound->y;
        rect.left = rectBound->x;
        rect.right = rectBound->x + rectBound->width;
        rect.bottom = rectBound->y + rectBound->height;
    }
    else
    {
        wxSize size = GetSize();

        rect.top =
        rect.left = 0;
        rect.right = size.x;
        rect.bottom = size.y;
    }

    wxDCImpl *impl = dc->GetImpl();
    wxMSWDCImpl *msw_impl = wxDynamicCast( impl, wxMSWDCImpl );
    if (!msw_impl)
        return false;

    if ( !::PlayEnhMetaFile(GetHdcOf(*msw_impl), GetEMF(), &rect) )
    {
        wxLogLastError(wxT("PlayEnhMetaFile"));

        return false;
    }

    return true;
}

wxSize wxEnhMetaFile::GetSize() const
{
    wxSize size = wxDefaultSize;

    if ( IsOk() )
    {
        ENHMETAHEADER hdr;
        if ( !::GetEnhMetaFileHeader(GetEMF(), sizeof(hdr), &hdr) )
        {
            wxLogLastError(wxT("GetEnhMetaFileHeader"));
        }
        else
        {
            // the width and height are in HIMETRIC (0.01mm) units, transform
            // them to pixels
            LONG w = hdr.rclFrame.right,
                 h = hdr.rclFrame.bottom;

            HIMETRICToPixel(&w, &h);

            size.x = w;
            size.y = h;
        }
    }

    return size;
}

bool wxEnhMetaFile::SetClipboard(int WXUNUSED(width), int WXUNUSED(height))
{
#if wxUSE_DRAG_AND_DROP && wxUSE_CLIPBOARD
    wxCHECK_MSG( m_hMF, false, wxT("can't copy invalid metafile to clipboard") );

    return wxTheClipboard->AddData(new wxEnhMetaFileDataObject(*this));
#else // !wxUSE_DRAG_AND_DROP
    wxFAIL_MSG(wxT("not implemented"));
    return false;
#endif // wxUSE_DRAG_AND_DROP/!wxUSE_DRAG_AND_DROP
}

// ----------------------------------------------------------------------------
// wxEnhMetaFileDCImpl
// ----------------------------------------------------------------------------

class wxEnhMetaFileDCImpl : public wxMSWDCImpl
{
public:
    wxEnhMetaFileDCImpl( wxEnhMetaFileDC *owner,
                         const wxString& filename, int width, int height,
                         const wxString& description );
    wxEnhMetaFileDCImpl( wxEnhMetaFileDC *owner,
                         const wxDC& referenceDC,
                         const wxString& filename, int width, int height,
                         const wxString& description );
    virtual ~wxEnhMetaFileDCImpl();

    // obtain a pointer to the new metafile (caller should delete it)
    wxEnhMetaFile *Close();

protected:
    virtual void DoGetSize(int *width, int *height) const;

private:
    void Create(HDC hdcRef,
                const wxString& filename, int width, int height,
                const wxString& description);

    // size passed to ctor and returned by DoGetSize()
    int m_width,
        m_height;
};


wxEnhMetaFileDCImpl::wxEnhMetaFileDCImpl( wxEnhMetaFileDC* owner,
                                 const wxString& filename,
                                 int width, int height,
                                 const wxString& description )
                   : wxMSWDCImpl( owner )
{
    Create(ScreenHDC(), filename, width, height, description);
}

wxEnhMetaFileDCImpl::wxEnhMetaFileDCImpl( wxEnhMetaFileDC* owner,
                                 const wxDC& referenceDC,
                                 const wxString& filename,
                                 int width, int height,
                                 const wxString& description )
                   : wxMSWDCImpl( owner )
{
    Create(GetHdcOf(referenceDC), filename, width, height, description);
}

void wxEnhMetaFileDCImpl::Create(HDC hdcRef,
                                 const wxString& filename,
                                 int width, int height,
                                 const wxString& description)
{
    m_width = width;
    m_height = height;

    RECT rect;
    RECT *pRect;
    if ( width && height )
    {
        rect.top =
        rect.left = 0;
        rect.right = width;
        rect.bottom = height;

        // CreateEnhMetaFile() wants them in HIMETRIC
        PixelToHIMETRIC(&rect.right, &rect.bottom, hdcRef);

        pRect = &rect;
    }
    else
    {
        // GDI will try to find out the size for us (not recommended)
        pRect = (LPRECT)NULL;
    }

    m_hDC = (WXHDC)::CreateEnhMetaFile(hdcRef, GetMetaFileName(filename),
                                       pRect, description.t_str());
    if ( !m_hDC )
    {
        wxLogLastError(wxT("CreateEnhMetaFile"));
    }
}

void wxEnhMetaFileDCImpl::DoGetSize(int *width, int *height) const
{
    if ( width )
        *width = m_width;
    if ( height )
        *height = m_height;
}

wxEnhMetaFile *wxEnhMetaFileDCImpl::Close()
{
    wxCHECK_MSG( IsOk(), NULL, wxT("invalid wxEnhMetaFileDC") );

    HENHMETAFILE hMF = ::CloseEnhMetaFile(GetHdc());
    if ( !hMF )
    {
        wxLogLastError(wxT("CloseEnhMetaFile"));

        return NULL;
    }

    wxEnhMetaFile *mf = new wxEnhMetaFile;
    mf->SetHENHMETAFILE((WXHANDLE)hMF);
    return mf;
}

wxEnhMetaFileDCImpl::~wxEnhMetaFileDCImpl()
{
    // avoid freeing it in the base class dtor
    m_hDC = 0;
}

// ----------------------------------------------------------------------------
// wxEnhMetaFileDC
// ----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxEnhMetaFileDC, wxDC)

wxEnhMetaFileDC::wxEnhMetaFileDC(const wxString& filename,
                                 int width, int height,
                                 const wxString& description)
               : wxDC(new wxEnhMetaFileDCImpl(this,
                                              filename,
                                              width, height,
                                              description))
{
}

wxEnhMetaFileDC::wxEnhMetaFileDC(const wxDC& referenceDC,
                                 const wxString& filename,
                                 int width, int height,
                                 const wxString& description)
               : wxDC(new wxEnhMetaFileDCImpl(this,
                                              referenceDC,
                                              filename,
                                              width, height,
                                              description))
{
}

wxEnhMetaFile *wxEnhMetaFileDC::Close()
{
    wxEnhMetaFileDCImpl * const
        impl = static_cast<wxEnhMetaFileDCImpl *>(GetImpl());
    wxCHECK_MSG( impl, NULL, wxT("no wxEnhMetaFileDC implementation") );

    return impl->Close();
}

#if wxUSE_DRAG_AND_DROP

// ----------------------------------------------------------------------------
// wxEnhMetaFileDataObject
// ----------------------------------------------------------------------------

wxDataFormat
wxEnhMetaFileDataObject::GetPreferredFormat(Direction WXUNUSED(dir)) const
{
    return wxDF_ENHMETAFILE;
}

size_t wxEnhMetaFileDataObject::GetFormatCount(Direction WXUNUSED(dir)) const
{
    // wxDF_ENHMETAFILE and wxDF_METAFILE
    return 2;
}

void wxEnhMetaFileDataObject::GetAllFormats(wxDataFormat *formats,
                                            Direction WXUNUSED(dir)) const
{
    formats[0] = wxDF_ENHMETAFILE;
    formats[1] = wxDF_METAFILE;
}

size_t wxEnhMetaFileDataObject::GetDataSize(const wxDataFormat& format) const
{
    if ( format == wxDF_ENHMETAFILE )
    {
        // we pass data by handle and not HGLOBAL
        return 0u;
    }
    else
    {
        wxASSERT_MSG( format == wxDF_METAFILE, wxT("unsupported format") );

        return sizeof(METAFILEPICT);
    }
}

bool wxEnhMetaFileDataObject::GetDataHere(const wxDataFormat& format, void *buf) const
{
    wxCHECK_MSG( m_metafile.IsOk(), false, wxT("copying invalid enh metafile") );

    HENHMETAFILE hEMF = (HENHMETAFILE)m_metafile.GetHENHMETAFILE();

    if ( format == wxDF_ENHMETAFILE )
    {
        HENHMETAFILE hEMFCopy = ::CopyEnhMetaFile(hEMF, NULL);
        if ( !hEMFCopy )
        {
            wxLogLastError(wxT("CopyEnhMetaFile"));

            return false;
        }

        *(HENHMETAFILE *)buf = hEMFCopy;
    }
    else
    {
        wxASSERT_MSG( format == wxDF_METAFILE, wxT("unsupported format") );

        // convert to WMF

        ScreenHDC hdc;

        // first get the buffer size and alloc memory
        size_t size = ::GetWinMetaFileBits(hEMF, 0, NULL, MM_ANISOTROPIC, hdc);
        wxCHECK_MSG( size, false, wxT("GetWinMetaFileBits() failed") );

        BYTE *bits = (BYTE *)malloc(size);

        // then get the enh metafile bits
        if ( !::GetWinMetaFileBits(hEMF, size, bits, MM_ANISOTROPIC, hdc) )
        {
            wxLogLastError(wxT("GetWinMetaFileBits"));

            free(bits);

            return false;
        }

        // and finally convert them to the WMF
        HMETAFILE hMF = ::SetMetaFileBitsEx(size, bits);
        free(bits);
        if ( !hMF )
        {
            wxLogLastError(wxT("SetMetaFileBitsEx"));

            return false;
        }

        METAFILEPICT *mfpict = (METAFILEPICT *)buf;

        wxSize sizeMF = m_metafile.GetSize();
        mfpict->hMF  = hMF;
        mfpict->mm   = MM_ANISOTROPIC;
        mfpict->xExt = sizeMF.x;
        mfpict->yExt = sizeMF.y;

        PixelToHIMETRIC(&mfpict->xExt, &mfpict->yExt);
    }

    return true;
}

bool wxEnhMetaFileDataObject::SetData(const wxDataFormat& format,
                                      size_t WXUNUSED(len),
                                      const void *buf)
{
    HENHMETAFILE hEMF;

    if ( format == wxDF_ENHMETAFILE )
    {
        hEMF = *(HENHMETAFILE *)buf;

        wxCHECK_MSG( hEMF, false, wxT("pasting invalid enh metafile") );
    }
    else
    {
        wxASSERT_MSG( format == wxDF_METAFILE, wxT("unsupported format") );

        // convert from WMF
        const METAFILEPICT *mfpict = (const METAFILEPICT *)buf;

        // first get the buffer size
        size_t size = ::GetMetaFileBitsEx(mfpict->hMF, 0, NULL);
        wxCHECK_MSG( size, false, wxT("GetMetaFileBitsEx() failed") );

        // then get metafile bits
        BYTE *bits = (BYTE *)malloc(size);
        if ( !::GetMetaFileBitsEx(mfpict->hMF, size, bits) )
        {
            wxLogLastError(wxT("GetMetaFileBitsEx"));

            free(bits);

            return false;
        }

        ScreenHDC hdcRef;

        // and finally create an enhanced metafile from them
        hEMF = ::SetWinMetaFileBits(size, bits, hdcRef, mfpict);
        free(bits);
        if ( !hEMF )
        {
            wxLogLastError(wxT("SetWinMetaFileBits"));

            return false;
        }
    }

    m_metafile.SetHENHMETAFILE((WXHANDLE)hEMF);

    return true;
}

// ----------------------------------------------------------------------------
// wxEnhMetaFileSimpleDataObject
// ----------------------------------------------------------------------------

size_t wxEnhMetaFileSimpleDataObject::GetDataSize() const
{
    // we pass data by handle and not HGLOBAL
    return 0u;
}

bool wxEnhMetaFileSimpleDataObject::GetDataHere(void *buf) const
{
    wxCHECK_MSG( m_metafile.IsOk(), false, wxT("copying invalid enh metafile") );

    HENHMETAFILE hEMF = (HENHMETAFILE)m_metafile.GetHENHMETAFILE();

    HENHMETAFILE hEMFCopy = ::CopyEnhMetaFile(hEMF, NULL);
    if ( !hEMFCopy )
    {
        wxLogLastError(wxT("CopyEnhMetaFile"));

        return false;
    }

    *(HENHMETAFILE *)buf = hEMFCopy;
    return true;
}

bool wxEnhMetaFileSimpleDataObject::SetData(size_t WXUNUSED(len),
                                            const void *buf)
{
    HENHMETAFILE hEMF = *(HENHMETAFILE *)buf;

    wxCHECK_MSG( hEMF, false, wxT("pasting invalid enh metafile") );
    m_metafile.SetHENHMETAFILE((WXHANDLE)hEMF);

    return true;
}


#endif // wxUSE_DRAG_AND_DROP

#endif // wxUSE_ENH_METAFILE
