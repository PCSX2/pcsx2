///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/ole/dataobj.cpp
// Purpose:     implementation of wx[I]DataObject class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     10.05.98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
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

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/wxcrtvararg.h"
#endif

#include "wx/dataobj.h"

#if wxUSE_OLE && defined(__WIN32__) && !defined(__GNUWIN32_OLD__)

#include "wx/scopedarray.h"
#include "wx/vector.h"
#include "wx/msw/private.h"         // includes <windows.h>

#ifdef __WXWINCE__
#include <winreg.h>
#endif

// for some compilers, the entire ole2.h must be included, not only oleauto.h
#if wxUSE_NORLANDER_HEADERS || defined(__WATCOMC__) || defined(__WXWINCE__)
  #include <ole2.h>
#endif

#include <oleauto.h>
#include "wx/msw/wrapshl.h"

#include "wx/msw/ole/oleutils.h"

#include "wx/msw/dib.h"

#ifndef CFSTR_SHELLURL
#define CFSTR_SHELLURL wxT("UniformResourceLocator")
#endif

// ----------------------------------------------------------------------------
// functions
// ----------------------------------------------------------------------------

#if wxDEBUG_LEVEL
    static const wxChar *GetTymedName(DWORD tymed);
#else // !wxDEBUG_LEVEL
    #define GetTymedName(tymed) wxEmptyString
#endif // wxDEBUG_LEVEL/!wxDEBUG_LEVEL

namespace
{

wxDataFormat HtmlFormatFixup(wxDataFormat format)
{
    // Since the HTML format is dynamically registered, the wxDF_HTML
    // format does not match the native constant in the way other formats do,
    // so for the format checks below to work, we must change the native
    // id to the wxDF_HTML constant.
    //
    // But skip this for the standard constants which are never going to match
    // wxDF_HTML anyhow.
    if ( !format.IsStandard() )
    {
        wxChar szBuf[256];
        if ( ::GetClipboardFormatName(format, szBuf, WXSIZEOF(szBuf)) )
        {
            if ( wxStrcmp(szBuf, wxT("HTML Format")) == 0 )
                format = wxDF_HTML;
        }
    }

    return format;
}

// helper function for wxCopyStgMedium()
HGLOBAL wxGlobalClone(HGLOBAL hglobIn)
{
    HGLOBAL hglobOut = NULL;

    LPVOID pvIn = GlobalLock(hglobIn);
    if (pvIn)
    {
        SIZE_T cb = GlobalSize(hglobIn);
        hglobOut = GlobalAlloc(GMEM_FIXED, cb);
        if (hglobOut)
        {
            CopyMemory(hglobOut, pvIn, cb);
        }
        GlobalUnlock(hglobIn);
    }

    return hglobOut;
}

// Copies the given STGMEDIUM structure.
//
// This is an local implementation of the function with the same name in
// urlmon.lib but to use that function would require linking with urlmon.lib
// and we don't want to require it, so simple reimplement it here.
HRESULT wxCopyStgMedium(const STGMEDIUM *pmediumIn, STGMEDIUM *pmediumOut)
{
    HRESULT hres = S_OK;
    STGMEDIUM stgmOut = *pmediumIn;

    if (pmediumIn->pUnkForRelease == NULL &&
        !(pmediumIn->tymed & (TYMED_ISTREAM | TYMED_ISTORAGE)))
    {
        // Object needs to be cloned.
        if (pmediumIn->tymed == TYMED_HGLOBAL)
        {
            stgmOut.hGlobal = wxGlobalClone(pmediumIn->hGlobal);
            if (!stgmOut.hGlobal)
            {
                hres = E_OUTOFMEMORY;
            }
        }
        else
        {
            hres = DV_E_TYMED; // Don't know how to clone GDI objects.
        }
    }

    if ( SUCCEEDED(hres) )
    {
        switch ( stgmOut.tymed )
        {
            case TYMED_ISTREAM:
                stgmOut.pstm->AddRef();
                break;

            case TYMED_ISTORAGE:
                stgmOut.pstg->AddRef();
                break;
        }

        if ( stgmOut.pUnkForRelease )
            stgmOut.pUnkForRelease->AddRef();

        *pmediumOut = stgmOut;
    }

    return hres;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// wxIEnumFORMATETC interface implementation
// ----------------------------------------------------------------------------

class wxIEnumFORMATETC : public IEnumFORMATETC
{
public:
    wxIEnumFORMATETC(const wxDataFormat* formats, ULONG nCount);
    virtual ~wxIEnumFORMATETC() { delete [] m_formats; }

    // IEnumFORMATETC
    STDMETHODIMP Next(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched);
    STDMETHODIMP Skip(ULONG celt);
    STDMETHODIMP Reset();
    STDMETHODIMP Clone(IEnumFORMATETC **ppenum);

    DECLARE_IUNKNOWN_METHODS;

private:
    CLIPFORMAT *m_formats;  // formats we can provide data in
    ULONG       m_nCount,   // number of formats we support
                m_nCurrent; // current enum position

    wxDECLARE_NO_COPY_CLASS(wxIEnumFORMATETC);
};

// ----------------------------------------------------------------------------
// wxIDataObject implementation of IDataObject interface
// ----------------------------------------------------------------------------

class wxIDataObject : public IDataObject
{
public:
    wxIDataObject(wxDataObject *pDataObject);
    virtual ~wxIDataObject();

    // normally, wxDataObject controls our lifetime (i.e. we're deleted when it
    // is), but in some cases, the situation is reversed, that is we delete it
    // when this object is deleted - setting this flag enables such logic
    void SetDeleteFlag() { m_mustDelete = true; }

    // IDataObject
    STDMETHODIMP GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium);
    STDMETHODIMP GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium);
    STDMETHODIMP QueryGetData(FORMATETC *pformatetc);
    STDMETHODIMP GetCanonicalFormatEtc(FORMATETC *In, FORMATETC *pOut);
    STDMETHODIMP SetData(FORMATETC *pfetc, STGMEDIUM *pmedium, BOOL fRelease);
    STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFEtc);
    STDMETHODIMP DAdvise(FORMATETC *pfetc, DWORD ad, IAdviseSink *p, DWORD *pdw);
    STDMETHODIMP DUnadvise(DWORD dwConnection);
    STDMETHODIMP EnumDAdvise(IEnumSTATDATA **ppenumAdvise);

    DECLARE_IUNKNOWN_METHODS;

private:
    wxDataObject *m_pDataObject;      // pointer to C++ class we belong to

    bool m_mustDelete;

    wxDECLARE_NO_COPY_CLASS(wxIDataObject);

    // The following code is need to be able to store system data the operating
    // system is using for it own purposes, e.g. drag images.

    class SystemDataEntry
    {
    public:
        // Ctor takes ownership of the pointers.
        SystemDataEntry(FORMATETC *pformatetc, STGMEDIUM *pmedium)
            : pformatetc(pformatetc), pmedium(pmedium)
        {
        }

        ~SystemDataEntry()
        {
            delete pformatetc;
            delete pmedium;
        }

        FORMATETC *pformatetc;
        STGMEDIUM *pmedium;
    };
    typedef wxVector<SystemDataEntry*> SystemData;

    // get system data specified by the given format
    bool GetSystemData(wxDataFormat format, STGMEDIUM*) const;

    // determines if the data object contains system data specified by the given format.
    bool HasSystemData(wxDataFormat format) const;

    // save system data
    HRESULT SaveSystemData(FORMATETC*, STGMEDIUM*, BOOL fRelease);

    // container for system data
    SystemData m_systemData;
};

bool
wxIDataObject::GetSystemData(wxDataFormat format, STGMEDIUM *pmedium) const
{
    for ( SystemData::const_iterator it = m_systemData.begin();
          it != m_systemData.end();
          ++it )
    {
        FORMATETC* formatEtc = (*it)->pformatetc;
        if ( formatEtc->cfFormat == format )
        {
            wxCopyStgMedium((*it)->pmedium, pmedium);
            return true;
        }
    }

    return false;
}

bool
wxIDataObject::HasSystemData(wxDataFormat format) const
{
    for ( SystemData::const_iterator it = m_systemData.begin();
          it != m_systemData.end();
          ++it )
    {
        FORMATETC* formatEtc = (*it)->pformatetc;
        if ( formatEtc->cfFormat == format )
            return true;
    }

    return false;
}

// save system data
HRESULT
wxIDataObject::SaveSystemData(FORMATETC *pformatetc,
                                 STGMEDIUM *pmedium,
                                 BOOL fRelease)
{
    if ( pformatetc == NULL || pmedium == NULL )
        return E_INVALIDARG;

    // remove entry if already available
    for ( SystemData::iterator it = m_systemData.begin();
          it != m_systemData.end();
          ++it )
    {
        if ( pformatetc->tymed & (*it)->pformatetc->tymed &&
             pformatetc->dwAspect == (*it)->pformatetc->dwAspect &&
             pformatetc->cfFormat == (*it)->pformatetc->cfFormat )
        {
            delete (*it);
            m_systemData.erase(it);
            break;
        }
    }

    // create new format/medium
    FORMATETC* pnewformatEtc = new FORMATETC;
    STGMEDIUM* pnewmedium = new STGMEDIUM;

    wxZeroMemory(*pnewformatEtc);
    wxZeroMemory(*pnewmedium);

    // copy format
    *pnewformatEtc = *pformatetc;

    // copy or take ownerschip of medium
    if ( fRelease )
        *pnewmedium = *pmedium;
    else
        wxCopyStgMedium(pmedium, pnewmedium);

    // save entry
    m_systemData.push_back(new SystemDataEntry(pnewformatEtc, pnewmedium));

    return S_OK;
}

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDataFormat
// ----------------------------------------------------------------------------

bool wxDataFormat::operator==(wxDataFormatId format) const
{
    return HtmlFormatFixup(*this).m_format == (NativeFormat)format;
}

bool wxDataFormat::operator!=(wxDataFormatId format) const
{
    return !(*this == format);
}

bool wxDataFormat::operator==(const wxDataFormat& format) const
{
    return HtmlFormatFixup(*this).m_format == HtmlFormatFixup(format).m_format;
}

bool wxDataFormat::operator!=(const wxDataFormat& format) const
{
    return !(*this == format);
}

void wxDataFormat::SetId(const wxString& format)
{
    m_format = (wxDataFormat::NativeFormat)::RegisterClipboardFormat(format.t_str());
    if ( !m_format )
    {
        wxLogError(_("Couldn't register clipboard format '%s'."), format);
    }
}

wxString wxDataFormat::GetId() const
{
    static const int max = 256;

    wxString s;

    wxCHECK_MSG( !IsStandard(), s,
                 wxT("name of predefined format cannot be retrieved") );

    int len = ::GetClipboardFormatName(m_format, wxStringBuffer(s, max), max);

    if ( !len )
    {
        wxLogError(_("The clipboard format '%d' doesn't exist."), m_format);
        return wxEmptyString;
    }

    return s;
}

// ----------------------------------------------------------------------------
// wxIEnumFORMATETC
// ----------------------------------------------------------------------------

BEGIN_IID_TABLE(wxIEnumFORMATETC)
    ADD_IID(Unknown)
    ADD_IID(EnumFORMATETC)
END_IID_TABLE;

IMPLEMENT_IUNKNOWN_METHODS(wxIEnumFORMATETC)

wxIEnumFORMATETC::wxIEnumFORMATETC(const wxDataFormat *formats, ULONG nCount)
{
    m_nCurrent = 0;
    m_nCount = nCount;
    m_formats = new CLIPFORMAT[nCount];
    for ( ULONG n = 0; n < nCount; n++ ) {
        if (formats[n].GetFormatId() != wxDF_HTML)
            m_formats[n] = formats[n].GetFormatId();
        else
            m_formats[n] = ::RegisterClipboardFormat(wxT("HTML Format"));
    }
}

STDMETHODIMP wxIEnumFORMATETC::Next(ULONG      celt,
                                    FORMATETC *rgelt,
                                    ULONG     *pceltFetched)
{
    wxLogTrace(wxTRACE_OleCalls, wxT("wxIEnumFORMATETC::Next"));

    ULONG numFetched = 0;
    while (m_nCurrent < m_nCount && numFetched < celt) {
        FORMATETC format;
        format.cfFormat = m_formats[m_nCurrent++];
        format.ptd      = NULL;
        format.dwAspect = DVASPECT_CONTENT;
        format.lindex   = -1;
        format.tymed    = TYMED_HGLOBAL;

        *rgelt++ = format;
        numFetched++;
    }

    if (pceltFetched)
        *pceltFetched = numFetched;

    return numFetched == celt ? S_OK : S_FALSE;
}

STDMETHODIMP wxIEnumFORMATETC::Skip(ULONG celt)
{
    wxLogTrace(wxTRACE_OleCalls, wxT("wxIEnumFORMATETC::Skip"));

    m_nCurrent += celt;
    if ( m_nCurrent < m_nCount )
        return S_OK;

    // no, can't skip this many elements
    m_nCurrent -= celt;

    return S_FALSE;
}

STDMETHODIMP wxIEnumFORMATETC::Reset()
{
    wxLogTrace(wxTRACE_OleCalls, wxT("wxIEnumFORMATETC::Reset"));

    m_nCurrent = 0;

    return S_OK;
}

STDMETHODIMP wxIEnumFORMATETC::Clone(IEnumFORMATETC **ppenum)
{
    wxLogTrace(wxTRACE_OleCalls, wxT("wxIEnumFORMATETC::Clone"));

    // unfortunately, we can't reuse the code in ctor - types are different
    wxIEnumFORMATETC *pNew = new wxIEnumFORMATETC(NULL, 0);
    pNew->m_nCount = m_nCount;
    pNew->m_formats = new CLIPFORMAT[m_nCount];
    for ( ULONG n = 0; n < m_nCount; n++ ) {
        pNew->m_formats[n] = m_formats[n];
    }
    pNew->AddRef();
    *ppenum = pNew;

    return S_OK;
}

// ----------------------------------------------------------------------------
// wxIDataObject
// ----------------------------------------------------------------------------

BEGIN_IID_TABLE(wxIDataObject)
    ADD_IID(Unknown)
    ADD_IID(DataObject)
END_IID_TABLE;

IMPLEMENT_IUNKNOWN_METHODS(wxIDataObject)

wxIDataObject::wxIDataObject(wxDataObject *pDataObject)
{
    m_pDataObject = pDataObject;
    m_mustDelete = false;
}

wxIDataObject::~wxIDataObject()
{
    // delete system data
    for ( SystemData::iterator it = m_systemData.begin();
          it != m_systemData.end();
          ++it )
    {
        delete (*it);
    }

    if ( m_mustDelete )
    {
        delete m_pDataObject;
    }
}

// get data functions
STDMETHODIMP wxIDataObject::GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium)
{
    wxLogTrace(wxTRACE_OleCalls, wxT("wxIDataObject::GetData"));

    // is data is in our format?
    HRESULT hr = QueryGetData(pformatetcIn);
    if ( FAILED(hr) )
        return hr;

    // for the bitmaps and metafiles we use the handles instead of global memory
    // to pass the data
    wxDataFormat format = (wxDataFormat::NativeFormat)pformatetcIn->cfFormat;
    format = HtmlFormatFixup(format);

    // is this system data?
    if ( GetSystemData(format, pmedium) )
    {
        // pmedium is already filled with corresponding data, so we're ready.
        return S_OK;
    }

    switch ( format )
    {
        case wxDF_BITMAP:
            pmedium->tymed = TYMED_GDI;
            break;

        case wxDF_ENHMETAFILE:
            pmedium->tymed = TYMED_ENHMF;
            break;

#ifndef __WXWINCE__
        case wxDF_METAFILE:
            pmedium->hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE,
                                           sizeof(METAFILEPICT));
            if ( !pmedium->hGlobal ) {
                wxLogLastError(wxT("GlobalAlloc"));
                return E_OUTOFMEMORY;
            }
            pmedium->tymed = TYMED_MFPICT;
            break;
#endif
        default:
            // alloc memory
            size_t size = m_pDataObject->GetDataSize(format);
            if ( !size ) {
                // it probably means that the method is just not implemented
                wxLogDebug(wxT("Invalid data size - can't be 0"));

                return DV_E_FORMATETC;
            }

            // we may need extra space for the buffer size
            size += m_pDataObject->GetBufferOffset( format );

            HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, size);
            if ( hGlobal == NULL ) {
                wxLogLastError(wxT("GlobalAlloc"));
                return E_OUTOFMEMORY;
            }

            // copy data
            pmedium->tymed   = TYMED_HGLOBAL;
            pmedium->hGlobal = hGlobal;
    }

    pmedium->pUnkForRelease = NULL;

    // do copy the data
    hr = GetDataHere(pformatetcIn, pmedium);
    if ( FAILED(hr) ) {
        // free resources we allocated
        if ( pmedium->tymed & (TYMED_HGLOBAL | TYMED_MFPICT) ) {
            GlobalFree(pmedium->hGlobal);
        }

        return hr;
    }

    return S_OK;
}

STDMETHODIMP wxIDataObject::GetDataHere(FORMATETC *pformatetc,
                                        STGMEDIUM *pmedium)
{
    wxLogTrace(wxTRACE_OleCalls, wxT("wxIDataObject::GetDataHere"));

    // put data in caller provided medium
    switch ( pmedium->tymed )
    {
        case TYMED_GDI:
            if ( !m_pDataObject->GetDataHere(wxDF_BITMAP, &pmedium->hBitmap) )
                return E_UNEXPECTED;
            break;

        case TYMED_ENHMF:
            if ( !m_pDataObject->GetDataHere(wxDF_ENHMETAFILE,
                                             &pmedium->hEnhMetaFile) )
                return E_UNEXPECTED;
            break;

        case TYMED_MFPICT:
            // fall through - we pass METAFILEPICT through HGLOBAL

        case TYMED_HGLOBAL:
            {
                // copy data
                HGLOBAL hGlobal = pmedium->hGlobal;
                void *pBuf = GlobalLock(hGlobal);
                if ( pBuf == NULL ) {
                    wxLogLastError(wxT("GlobalLock"));
                    return E_OUTOFMEMORY;
                }

                wxDataFormat format = pformatetc->cfFormat;

                // possibly put the size in the beginning of the buffer
                pBuf = m_pDataObject->SetSizeInBuffer
                                      (
                                        pBuf,
                                        ::GlobalSize(hGlobal),
                                        format
                                      );

                if ( !m_pDataObject->GetDataHere(format, pBuf) )
                    return E_UNEXPECTED;

                GlobalUnlock(hGlobal);
            }
            break;

        default:
            return DV_E_TYMED;
    }

    return S_OK;
}


// set data functions
STDMETHODIMP wxIDataObject::SetData(FORMATETC *pformatetc,
                                    STGMEDIUM *pmedium,
                                    BOOL       fRelease)
{
    wxLogTrace(wxTRACE_OleCalls, wxT("wxIDataObject::SetData"));

    switch ( pmedium->tymed )
    {
        case TYMED_GDI:
            m_pDataObject->SetData(wxDF_BITMAP, 0, &pmedium->hBitmap);
            break;

        case TYMED_ENHMF:
            m_pDataObject->SetData(wxDF_ENHMETAFILE, 0, &pmedium->hEnhMetaFile);
            break;

        case TYMED_ISTREAM:
            // check if this format is supported
            if ( !m_pDataObject->IsSupported(pformatetc->cfFormat,
                                             wxDataObject::Set) )
            {
                // As this is not a supported format (content data), assume it
                // is system data and save it.
                return SaveSystemData(pformatetc, pmedium, fRelease);
            }
            break;

        case TYMED_MFPICT:
            // fall through - we pass METAFILEPICT through HGLOBAL
        case TYMED_HGLOBAL:
            {
                wxDataFormat format = pformatetc->cfFormat;

                format = HtmlFormatFixup(format);

                // check if this format is supported
                if ( !m_pDataObject->IsSupported(format, wxDataObject::Set) ) {
                    // As above, assume that unsupported format must be system
                    // data and just save it.
                    return SaveSystemData(pformatetc, pmedium, fRelease);
                }

                // copy data
                const void *pBuf = GlobalLock(pmedium->hGlobal);
                if ( pBuf == NULL ) {
                    wxLogLastError(wxT("GlobalLock"));

                    return E_OUTOFMEMORY;
                }

                // we've got a problem with SetData() here because the base
                // class version requires the size parameter which we don't
                // have anywhere in OLE data transfer - so we need to
                // synthetise it for known formats and we suppose that all data
                // in custom formats starts with a DWORD containing the size
                size_t size;
                switch ( format )
                {
                    case wxDF_HTML:
                    case CF_TEXT:
                    case CF_OEMTEXT:
                        size = strlen((const char *)pBuf);
                        break;
#if !(defined(__BORLANDC__) && (__BORLANDC__ < 0x500))
                    case CF_UNICODETEXT:
#if ( defined(__BORLANDC__) && (__BORLANDC__ > 0x530) )
                        size = std::wcslen((const wchar_t *)pBuf) * sizeof(wchar_t);
#else
                        size = wxWcslen((const wchar_t *)pBuf) * sizeof(wchar_t);
#endif
                        break;
#endif
                    case CF_BITMAP:
#ifndef __WXWINCE__
                    case CF_HDROP:
                        // these formats don't use size at all, anyhow (but
                        // pass data by handle, which is always a single DWORD)
                        size = 0;
                        break;
#endif

                    case CF_DIB:
                        // the handler will calculate size itself (it's too
                        // complicated to do it here)
                        size = 0;
                        break;

#ifndef __WXWINCE__
                    case CF_METAFILEPICT:
                        size = sizeof(METAFILEPICT);
                        break;
#endif
                    default:
                        pBuf = m_pDataObject->
                                    GetSizeFromBuffer(pBuf, &size, format);
                        size -= m_pDataObject->GetBufferOffset(format);
                }

                bool ok = m_pDataObject->SetData(format, size, pBuf);

                GlobalUnlock(pmedium->hGlobal);

                if ( !ok ) {
                    return E_UNEXPECTED;
                }
            }
            break;

        default:
            return DV_E_TYMED;
    }

    if ( fRelease ) {
        // we own the medium, so we must release it - but do *not* free any
        // data we pass by handle because we have copied it elsewhere
        switch ( pmedium->tymed )
        {
            case TYMED_GDI:
                pmedium->hBitmap = 0;
                break;

            case TYMED_MFPICT:
                pmedium->hMetaFilePict = 0;
                break;

            case TYMED_ENHMF:
                pmedium->hEnhMetaFile = 0;
                break;
        }

        ReleaseStgMedium(pmedium);
    }

    return S_OK;
}

// information functions
STDMETHODIMP wxIDataObject::QueryGetData(FORMATETC *pformatetc)
{
    // do we accept data in this format?
    if ( pformatetc == NULL ) {
        wxLogTrace(wxTRACE_OleCalls,
                   wxT("wxIDataObject::QueryGetData: invalid ptr."));

        return E_INVALIDARG;
    }

    // the only one allowed by current COM implementation
    if ( pformatetc->lindex != -1 ) {
        wxLogTrace(wxTRACE_OleCalls,
                   wxT("wxIDataObject::QueryGetData: bad lindex %ld"),
                   pformatetc->lindex);

        return DV_E_LINDEX;
    }

    // we don't support anything other (THUMBNAIL, ICON, DOCPRINT...)
    if ( pformatetc->dwAspect != DVASPECT_CONTENT ) {
        wxLogTrace(wxTRACE_OleCalls,
                   wxT("wxIDataObject::QueryGetData: bad dwAspect %ld"),
                   pformatetc->dwAspect);

        return DV_E_DVASPECT;
    }

    // and now check the type of data requested
    wxDataFormat format = pformatetc->cfFormat;
    format = HtmlFormatFixup(format);

    if ( m_pDataObject->IsSupportedFormat(format) ) {
        wxLogTrace(wxTRACE_OleCalls, wxT("wxIDataObject::QueryGetData: %s ok"),
                   wxGetFormatName(format));
    }
    else if ( HasSystemData(format) )
    {
        wxLogTrace(wxTRACE_OleCalls, wxT("wxIDataObject::QueryGetData: %s ok (system data)"),
                   wxGetFormatName(format));
        // this is system data, so no further checks needed.
        return S_OK;
    }
    else {
        wxLogTrace(wxTRACE_OleCalls,
                   wxT("wxIDataObject::QueryGetData: %s unsupported"),
                   wxGetFormatName(format));

        return DV_E_FORMATETC;
    }

    // we only transfer data by global memory, except for some particular cases
    DWORD tymed = pformatetc->tymed;
    if ( (format == wxDF_BITMAP && !(tymed & TYMED_GDI)) &&
         !(tymed & TYMED_HGLOBAL) ) {
        // it's not what we're waiting for
        wxLogTrace(wxTRACE_OleCalls,
                   wxT("wxIDataObject::QueryGetData: %s != %s"),
                   GetTymedName(tymed),
                   GetTymedName(format == wxDF_BITMAP ? TYMED_GDI
                                                      : TYMED_HGLOBAL));

        return DV_E_TYMED;
    }

    return S_OK;
}

STDMETHODIMP wxIDataObject::GetCanonicalFormatEtc(FORMATETC *WXUNUSED(pFormatetcIn),
                                                  FORMATETC *pFormatetcOut)
{
    wxLogTrace(wxTRACE_OleCalls, wxT("wxIDataObject::GetCanonicalFormatEtc"));

    // TODO we might want something better than this trivial implementation here
    if ( pFormatetcOut != NULL )
        pFormatetcOut->ptd = NULL;

    return DATA_S_SAMEFORMATETC;
}

STDMETHODIMP wxIDataObject::EnumFormatEtc(DWORD dwDir,
                                          IEnumFORMATETC **ppenumFormatEtc)
{
    wxLogTrace(wxTRACE_OleCalls, wxT("wxIDataObject::EnumFormatEtc"));

    wxDataObject::Direction dir = dwDir == DATADIR_GET ? wxDataObject::Get
                                                       : wxDataObject::Set;

    // format count is total of user specified and system formats.
    const size_t ourFormatCount = m_pDataObject->GetFormatCount(dir);
    const size_t sysFormatCount = m_systemData.size();

    const ULONG
        nFormatCount = wx_truncate_cast(ULONG, ourFormatCount + sysFormatCount);

    // fill format array with formats ...
    wxScopedArray<wxDataFormat> formats(new wxDataFormat[nFormatCount]);

    // ... from content data (supported formats)
    m_pDataObject->GetAllFormats(formats.get(), dir);

    // ... from system data
    for ( size_t j = 0; j < sysFormatCount; j++ )
    {
        SystemDataEntry* entry = m_systemData[j];
        wxDataFormat& format = formats[ourFormatCount + j];
        format = entry->pformatetc->cfFormat;
    }

    wxIEnumFORMATETC *pEnum = new wxIEnumFORMATETC(formats.get(), nFormatCount);
    pEnum->AddRef();
    *ppenumFormatEtc = pEnum;

    return S_OK;
}

// ----------------------------------------------------------------------------
// advise sink functions (not implemented)
// ----------------------------------------------------------------------------

STDMETHODIMP wxIDataObject::DAdvise(FORMATETC   *WXUNUSED(pformatetc),
                                    DWORD        WXUNUSED(advf),
                                    IAdviseSink *WXUNUSED(pAdvSink),
                                    DWORD       *WXUNUSED(pdwConnection))
{
  return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP wxIDataObject::DUnadvise(DWORD WXUNUSED(dwConnection))
{
  return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP wxIDataObject::EnumDAdvise(IEnumSTATDATA **WXUNUSED(ppenumAdvise))
{
  return OLE_E_ADVISENOTSUPPORTED;
}

// ----------------------------------------------------------------------------
// wxDataObject
// ----------------------------------------------------------------------------

wxDataObject::wxDataObject()
{
    m_pIDataObject = new wxIDataObject(this);
    m_pIDataObject->AddRef();
}

wxDataObject::~wxDataObject()
{
    ReleaseInterface(m_pIDataObject);
}

void wxDataObject::SetAutoDelete()
{
    ((wxIDataObject *)m_pIDataObject)->SetDeleteFlag();
    m_pIDataObject->Release();

    // so that the dtor doesn't crash
    m_pIDataObject = NULL;
}

size_t wxDataObject::GetBufferOffset(const wxDataFormat& format )
{
    // if we prepend the size of the data to the buffer itself, account for it
    return NeedsVerbatimData(format) ? 0 : sizeof(size_t);
}

const void *wxDataObject::GetSizeFromBuffer(const void *buffer,
                                            size_t *size,
                                            const wxDataFormat& WXUNUSED(format))
{
    // hack: the third parameter is declared non-const in Wine's headers so
    // cast away the const
    const size_t realsz = ::HeapSize(::GetProcessHeap(), 0,
                                     const_cast<void*>(buffer));
    if ( realsz == (size_t)-1 )
    {
        // note that HeapSize() does not set last error
        wxLogApiError(wxT("HeapSize"), 0);
        return NULL;
    }

    *size = realsz;

    return buffer;
}

void* wxDataObject::SetSizeInBuffer( void* buffer, size_t size,
                                      const wxDataFormat& format )
{
    size_t* p = (size_t *)buffer;
    if ( !NeedsVerbatimData(format) )
    {
        // prepend the size to the data and skip it
        *p++ = size;
    }

    return p;
}

#if wxDEBUG_LEVEL

const wxChar *wxDataObject::GetFormatName(wxDataFormat format)
{
    // case 'xxx' is not a valid value for switch of enum 'wxDataFormat'
    #ifdef __VISUALC__
        #pragma warning(disable:4063)
    #endif // VC++

    static wxChar s_szBuf[256];
    switch ( format ) {
        case CF_TEXT:         return wxT("CF_TEXT");
        case CF_BITMAP:       return wxT("CF_BITMAP");
        case CF_SYLK:         return wxT("CF_SYLK");
        case CF_DIF:          return wxT("CF_DIF");
        case CF_TIFF:         return wxT("CF_TIFF");
        case CF_OEMTEXT:      return wxT("CF_OEMTEXT");
        case CF_DIB:          return wxT("CF_DIB");
        case CF_PALETTE:      return wxT("CF_PALETTE");
        case CF_PENDATA:      return wxT("CF_PENDATA");
        case CF_RIFF:         return wxT("CF_RIFF");
        case CF_WAVE:         return wxT("CF_WAVE");
        case CF_UNICODETEXT:  return wxT("CF_UNICODETEXT");
#ifndef __WXWINCE__
        case CF_METAFILEPICT: return wxT("CF_METAFILEPICT");
        case CF_ENHMETAFILE:  return wxT("CF_ENHMETAFILE");
        case CF_LOCALE:       return wxT("CF_LOCALE");
        case CF_HDROP:        return wxT("CF_HDROP");
#endif

        default:
            if ( !::GetClipboardFormatName(format, s_szBuf, WXSIZEOF(s_szBuf)) )
            {
                // it must be a new predefined format we don't know the name of
                wxSprintf(s_szBuf, wxT("unknown CF (0x%04x)"), format.GetFormatId());
            }

            return s_szBuf;
    }

    #ifdef __VISUALC__
        #pragma warning(default:4063)
    #endif // VC++
}

#endif // wxDEBUG_LEVEL

// ----------------------------------------------------------------------------
// wxBitmapDataObject supports CF_DIB format
// ----------------------------------------------------------------------------

// TODO: support CF_DIB under Windows CE as well

size_t wxBitmapDataObject::GetDataSize() const
{
#if wxUSE_WXDIB && !defined(__WXWINCE__)
    return wxDIB::ConvertFromBitmap(NULL, GetHbitmapOf(GetBitmap()));
#else
    return 0;
#endif
}

bool wxBitmapDataObject::GetDataHere(void *buf) const
{
#if wxUSE_WXDIB && !defined(__WXWINCE__)
    BITMAPINFO * const pbi = (BITMAPINFO *)buf;

    return wxDIB::ConvertFromBitmap(pbi, GetHbitmapOf(GetBitmap())) != 0;
#else
    wxUnusedVar(buf);
    return false;
#endif
}

bool wxBitmapDataObject::SetData(size_t WXUNUSED(len), const void *buf)
{
#if wxUSE_WXDIB && !defined(__WXWINCE__)
    const BITMAPINFO * const pbmi = (const BITMAPINFO *)buf;

    HBITMAP hbmp = wxDIB::ConvertToBitmap(pbmi);

    wxCHECK_MSG( hbmp, FALSE, wxT("pasting/dropping invalid bitmap") );

    const BITMAPINFOHEADER * const pbmih = &pbmi->bmiHeader;
    wxBitmap bitmap(pbmih->biWidth, pbmih->biHeight, pbmih->biBitCount);
    bitmap.SetHBITMAP((WXHBITMAP)hbmp);

    // TODO: create wxPalette if the bitmap has any

    SetBitmap(bitmap);

    return true;
#else
    wxUnusedVar(buf);
    return false;
#endif
}

// ----------------------------------------------------------------------------
// wxBitmapDataObject2 supports CF_BITMAP format
// ----------------------------------------------------------------------------

// the bitmaps aren't passed by value as other types of data (i.e. by copying
// the data into a global memory chunk and passing it to the clipboard or
// another application or whatever), but by handle, so these generic functions
// don't make much sense to them.

size_t wxBitmapDataObject2::GetDataSize() const
{
    return 0;
}

bool wxBitmapDataObject2::GetDataHere(void *pBuf) const
{
    // we put a bitmap handle into pBuf
    *(WXHBITMAP *)pBuf = GetBitmap().GetHBITMAP();

    return true;
}

bool wxBitmapDataObject2::SetData(size_t WXUNUSED(len), const void *pBuf)
{
    HBITMAP hbmp = *(HBITMAP *)pBuf;

    BITMAP bmp;
    if ( !GetObject(hbmp, sizeof(BITMAP), &bmp) )
    {
        wxLogLastError(wxT("GetObject(HBITMAP)"));
    }

    wxBitmap bitmap(bmp.bmWidth, bmp.bmHeight, bmp.bmPlanes);
    bitmap.SetHBITMAP((WXHBITMAP)hbmp);

    if ( !bitmap.IsOk() ) {
        wxFAIL_MSG(wxT("pasting/dropping invalid bitmap"));

        return false;
    }

    SetBitmap(bitmap);

    return true;
}

#if 0

size_t wxBitmapDataObject::GetDataSize(const wxDataFormat& format) const
{
    if ( format.GetFormatId() == CF_DIB )
    {
        // create the DIB
        ScreenHDC hdc;

        // shouldn't be selected into a DC or GetDIBits() would fail
        wxASSERT_MSG( !m_bitmap.GetSelectedInto(),
                      wxT("can't copy bitmap selected into wxMemoryDC") );

        // first get the info
        BITMAPINFO bi;
        if ( !GetDIBits(hdc, (HBITMAP)m_bitmap.GetHBITMAP(), 0, 0,
                        NULL, &bi, DIB_RGB_COLORS) )
        {
            wxLogLastError(wxT("GetDIBits(NULL)"));

            return 0;
        }

        return sizeof(BITMAPINFO) + bi.bmiHeader.biSizeImage;
    }
    else // CF_BITMAP
    {
        // no data to copy - we don't pass HBITMAP via global memory
        return 0;
    }
}

bool wxBitmapDataObject::GetDataHere(const wxDataFormat& format,
                                     void *pBuf) const
{
    wxASSERT_MSG( m_bitmap.IsOk(), wxT("copying invalid bitmap") );

    HBITMAP hbmp = (HBITMAP)m_bitmap.GetHBITMAP();
    if ( format.GetFormatId() == CF_DIB )
    {
        // create the DIB
        ScreenHDC hdc;

        // shouldn't be selected into a DC or GetDIBits() would fail
        wxASSERT_MSG( !m_bitmap.GetSelectedInto(),
                      wxT("can't copy bitmap selected into wxMemoryDC") );

        // first get the info
        BITMAPINFO *pbi = (BITMAPINFO *)pBuf;
        if ( !GetDIBits(hdc, hbmp, 0, 0, NULL, pbi, DIB_RGB_COLORS) )
        {
            wxLogLastError(wxT("GetDIBits(NULL)"));

            return 0;
        }

        // and now copy the bits
        if ( !GetDIBits(hdc, hbmp, 0, pbi->bmiHeader.biHeight, pbi + 1,
                        pbi, DIB_RGB_COLORS) )
        {
            wxLogLastError(wxT("GetDIBits"));

            return false;
        }
    }
    else // CF_BITMAP
    {
        // we put a bitmap handle into pBuf
        *(HBITMAP *)pBuf = hbmp;
    }

    return true;
}

bool wxBitmapDataObject::SetData(const wxDataFormat& format,
                                 size_t size, const void *pBuf)
{
    HBITMAP hbmp;
    if ( format.GetFormatId() == CF_DIB )
    {
        // here we get BITMAPINFO struct followed by the actual bitmap bits and
        // BITMAPINFO starts with BITMAPINFOHEADER followed by colour info
        ScreenHDC hdc;

        BITMAPINFO *pbmi = (BITMAPINFO *)pBuf;
        BITMAPINFOHEADER *pbmih = &pbmi->bmiHeader;
        hbmp = CreateDIBitmap(hdc, pbmih, CBM_INIT,
                              pbmi + 1, pbmi, DIB_RGB_COLORS);
        if ( !hbmp )
        {
            wxLogLastError(wxT("CreateDIBitmap"));
        }

        m_bitmap.SetWidth(pbmih->biWidth);
        m_bitmap.SetHeight(pbmih->biHeight);
    }
    else // CF_BITMAP
    {
        // it's easy with bitmaps: we pass them by handle
        hbmp = *(HBITMAP *)pBuf;

        BITMAP bmp;
        if ( !GetObject(hbmp, sizeof(BITMAP), &bmp) )
        {
            wxLogLastError(wxT("GetObject(HBITMAP)"));
        }

        m_bitmap.SetWidth(bmp.bmWidth);
        m_bitmap.SetHeight(bmp.bmHeight);
        m_bitmap.SetDepth(bmp.bmPlanes);
    }

    m_bitmap.SetHBITMAP((WXHBITMAP)hbmp);

    wxASSERT_MSG( m_bitmap.IsOk(), wxT("pasting invalid bitmap") );

    return true;
}

#endif // 0

// ----------------------------------------------------------------------------
// wxFileDataObject
// ----------------------------------------------------------------------------

bool wxFileDataObject::SetData(size_t WXUNUSED(size),
                               const void *WXUNUSED_IN_WINCE(pData))
{
#ifndef __WXWINCE__
    m_filenames.Empty();

    // the documentation states that the first member of DROPFILES structure is
    // a "DWORD offset of double NUL terminated file list". What they mean by
    // this (I wonder if you see it immediately) is that the list starts at
    // ((char *)&(pDropFiles.pFiles)) + pDropFiles.pFiles. We're also advised
    // to use DragQueryFile to work with this structure, but not told where and
    // how to get HDROP.
    HDROP hdrop = (HDROP)pData;   // NB: it works, but I'm not sure about it

    // get number of files (magic value -1)
    UINT nFiles = ::DragQueryFile(hdrop, (unsigned)-1, NULL, 0u);

    wxCHECK_MSG ( nFiles != (UINT)-1, FALSE, wxT("wrong HDROP handle") );

    // for each file get the length, allocate memory and then get the name
    wxString str;
    UINT len, n;
    for ( n = 0; n < nFiles; n++ ) {
        // +1 for terminating NUL
        len = ::DragQueryFile(hdrop, n, NULL, 0) + 1;

        UINT len2 = ::DragQueryFile(hdrop, n, wxStringBuffer(str, len), len);
        m_filenames.Add(str);

        if ( len2 != len - 1 ) {
            wxLogDebug(wxT("In wxFileDropTarget::OnDrop DragQueryFile returned\
 %d characters, %d expected."), len2, len - 1);
        }
    }

    return true;
#else
    return false;
#endif
}

void wxFileDataObject::AddFile(const wxString& file)
{
    // just add file to filenames array
    // all useful data (such as DROPFILES struct) will be
    // created later as necessary
    m_filenames.Add(file);
}

size_t wxFileDataObject::GetDataSize() const
{
#ifndef __WXWINCE__
    // size returned will be the size of the DROPFILES structure, plus the list
    // of filesnames (null byte separated), plus a double null at the end

    // if no filenames in list, size is 0
    if ( m_filenames.empty() )
        return 0;

#if wxUSE_UNICODE_MSLU
    size_t sizeOfChar;
    if ( wxGetOsVersion() == wxOS_WINDOWS_9X )
    {
        // Win9x always uses ANSI file names and MSLU doesn't help with this
        sizeOfChar = 1;
    }
    else
    {
        sizeOfChar = sizeof(wxChar);
    }
#else // !wxUSE_UNICODE_MSLU
    static const size_t sizeOfChar = sizeof(wxChar);
#endif // wxUSE_UNICODE_MSLU/!wxUSE_UNICODE_MSLU

    // initial size of DROPFILES struct + null byte
    size_t sz = sizeof(DROPFILES) + sizeOfChar;

    const size_t count = m_filenames.size();
    for ( size_t i = 0; i < count; i++ )
    {
        // add filename length plus null byte
        size_t len;
#if wxUSE_UNICODE_MSLU
        if ( sizeOfChar == 1 )
            len = strlen(m_filenames[i].mb_str(*wxConvFileName));
        else
#endif // wxUSE_UNICODE_MSLU
            len = m_filenames[i].length();

        sz += (len + 1) * sizeOfChar;
    }

    return sz;
#else
    return 0;
#endif
}

bool wxFileDataObject::GetDataHere(void *WXUNUSED_IN_WINCE(pData)) const
{
#ifndef __WXWINCE__
    // pData points to an externally allocated memory block
    // created using the size returned by GetDataSize()

    // if pData is NULL, or there are no files, return
    if ( !pData || m_filenames.empty() )
        return false;

    // convert data pointer to a DROPFILES struct pointer
    LPDROPFILES pDrop = (LPDROPFILES) pData;

    // initialize DROPFILES struct
    pDrop->pFiles = sizeof(DROPFILES);
    pDrop->fNC = FALSE;                 // not non-client coords
#if wxUSE_UNICODE_MSLU
    pDrop->fWide = wxGetOsVersion() != wxOS_WINDOWS_9X ? TRUE : FALSE;
#else
    pDrop->fWide = wxUSE_UNICODE;
#endif

    const size_t sizeOfChar = pDrop->fWide ? sizeof(wchar_t) : 1;

    // set start of filenames list (null separated)
    BYTE *pbuf = (BYTE *)(pDrop + 1);

    const size_t count = m_filenames.size();
    for ( size_t i = 0; i < count; i++ )
    {
        // copy filename to pbuf and add null terminator
        size_t len;
#if wxUSE_UNICODE_MSLU
        if ( sizeOfChar == 1 )
        {
            wxCharBuffer buf(m_filenames[i].mb_str(*wxConvFileName));
            len = strlen(buf);
            memcpy(pbuf, buf, len*sizeOfChar);
        }
        else
#endif // wxUSE_UNICODE_MSLU
        {
            len = m_filenames[i].length();
            memcpy(pbuf, m_filenames[i].t_str(), len*sizeOfChar);
        }

        pbuf += len*sizeOfChar;

        memset(pbuf, 0, sizeOfChar);
        pbuf += sizeOfChar;
    }

    // add final null terminator
    memset(pbuf, 0, sizeOfChar);

    return true;
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------
// wxURLDataObject
// ----------------------------------------------------------------------------

// Work around bug in Wine headers
#if defined(__WINE__) && defined(CFSTR_SHELLURL) && wxUSE_UNICODE
#undef CFSTR_SHELLURL
#define CFSTR_SHELLURL wxT("CFSTR_SHELLURL")
#endif

class CFSTR_SHELLURLDataObject : public wxCustomDataObject
{
public:
    CFSTR_SHELLURLDataObject() : wxCustomDataObject(CFSTR_SHELLURL) {}

    virtual size_t GetBufferOffset( const wxDataFormat& WXUNUSED(format) )
    {
        return 0;
    }

    virtual const void* GetSizeFromBuffer( const void* buffer, size_t* size,
                                           const wxDataFormat& WXUNUSED(format) )
    {
        // CFSTR_SHELLURL is _always_ ANSI text
        *size = strlen( (const char*)buffer );

        return buffer;
    }

    virtual void* SetSizeInBuffer( void* buffer, size_t WXUNUSED(size),
                                   const wxDataFormat& WXUNUSED(format) )
    {
        return buffer;
    }

    wxDECLARE_NO_COPY_CLASS(CFSTR_SHELLURLDataObject);
};



wxURLDataObject::wxURLDataObject(const wxString& url)
{
    // we support CF_TEXT and CFSTR_SHELLURL formats which are basically the
    // same but it seems that some browsers only provide one of them so we have
    // to support both
    Add(new wxTextDataObject);
    Add(new CFSTR_SHELLURLDataObject());

    // we don't have any data yet
    m_dataObjectLast = NULL;

    if ( !url.empty() )
        SetURL(url);
}

bool wxURLDataObject::SetData(const wxDataFormat& format,
                              size_t len,
                              const void *buf)
{
    m_dataObjectLast = GetObject(format);

    wxCHECK_MSG( m_dataObjectLast, FALSE,
                 wxT("unsupported format in wxURLDataObject"));

    return m_dataObjectLast->SetData(len, buf);
}

wxString wxURLDataObject::GetURL() const
{
    wxString url;
    wxCHECK_MSG( m_dataObjectLast, url, wxT("no data in wxURLDataObject") );

    if ( m_dataObjectLast->GetPreferredFormat() == CFSTR_SHELLURL )
    {
        const size_t len = m_dataObjectLast->GetDataSize();
        if ( !len )
            return wxString();

        // CFSTR_SHELLURL is always ANSI so we need to convert it from it in
        // Unicode build
#if wxUSE_UNICODE
        wxCharBuffer buf(len);

        if ( m_dataObjectLast->GetDataHere(buf.data()) )
            url = buf;
#else // !wxUSE_UNICODE
        // in ANSI build no conversion is necessary
        m_dataObjectLast->GetDataHere(wxStringBuffer(url, len));
#endif // wxUSE_UNICODE/!wxUSE_UNICODE
    }
    else // must be wxTextDataObject
    {
        url = static_cast<wxTextDataObject *>(m_dataObjectLast)->GetText();
    }

    return url;
}

void wxURLDataObject::SetURL(const wxString& url)
{
    wxCharBuffer urlMB(url.mb_str());
    if ( urlMB )
    {
        const size_t len = strlen(urlMB);

#if !wxUSE_UNICODE
        // wxTextDataObject takes the number of characters in the string, not
        // the size of the buffer (which would be len+1)
        SetData(wxDF_TEXT, len, urlMB);
#endif // !wxUSE_UNICODE

        // however CFSTR_SHELLURLDataObject doesn't append NUL automatically
        // but we probably still want to have it on the clipboard (just to be
        // safe), so do append it
        SetData(wxDataFormat(CFSTR_SHELLURL), len + 1, urlMB);
    }

#if wxUSE_UNICODE
    SetData(wxDF_UNICODETEXT, url.length()*sizeof(wxChar), url.wc_str());
#endif
}

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

#if wxDEBUG_LEVEL

static const wxChar *GetTymedName(DWORD tymed)
{
    static wxChar s_szBuf[128];
    switch ( tymed ) {
        case TYMED_HGLOBAL:   return wxT("TYMED_HGLOBAL");
        case TYMED_FILE:      return wxT("TYMED_FILE");
        case TYMED_ISTREAM:   return wxT("TYMED_ISTREAM");
        case TYMED_ISTORAGE:  return wxT("TYMED_ISTORAGE");
        case TYMED_GDI:       return wxT("TYMED_GDI");
        case TYMED_MFPICT:    return wxT("TYMED_MFPICT");
        case TYMED_ENHMF:     return wxT("TYMED_ENHMF");
        default:
            wxSprintf(s_szBuf, wxT("type of media format %ld (unknown)"), tymed);
            return s_szBuf;
    }
}

#endif // Debug

#else // not using OLE at all

// ----------------------------------------------------------------------------
// wxDataObject
// ----------------------------------------------------------------------------

#if wxUSE_DATAOBJ

wxDataObject::wxDataObject()
{
}

wxDataObject::~wxDataObject()
{
}

void wxDataObject::SetAutoDelete()
{
}

const wxChar *wxDataObject::GetFormatName(wxDataFormat WXUNUSED(format))
{
    return NULL;
}

#endif // wxUSE_DATAOBJ

#endif // wxUSE_OLE/!wxUSE_OLE


