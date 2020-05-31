////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/bitmap.cpp
// Purpose:     wxBitmap
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

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

#include "wx/bitmap.h"

#ifndef WX_PRECOMP
    #include <stdio.h>

    #include "wx/list.h"
    #include "wx/utils.h"
    #include "wx/app.h"
    #include "wx/palette.h"
    #include "wx/dcmemory.h"
    #include "wx/icon.h"
    #include "wx/log.h"
    #include "wx/image.h"
#endif

#include "wx/scopedptr.h"
#include "wx/msw/private.h"
#include "wx/msw/dc.h"

#if wxUSE_WXDIB
    #include "wx/msw/dib.h"
#endif

#ifdef wxHAS_RAW_BITMAP
    #include "wx/rawbmp.h"
#endif

// missing from mingw32 header
#ifndef CLR_INVALID
    #define CLR_INVALID ((COLORREF)-1)
#endif // no CLR_INVALID

// ----------------------------------------------------------------------------
// wxBitmapRefData
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxBitmapRefData : public wxGDIImageRefData
{
public:
    wxBitmapRefData() { Init(); }
    wxBitmapRefData(const wxBitmapRefData& data);
    virtual ~wxBitmapRefData() { Free(); }

    virtual void Free();

    // Creates a new bitmap (DDB or DIB) from the contents of the given DIB.
    void CopyFromDIB(const wxDIB& dib);

#if wxUSE_WXDIB
    // Takes ownership of the given DIB.
    bool AssignDIB(wxDIB& dib);
#endif // wxUSE_WXDIB


    // set the mask object to use as the mask, we take ownership of it
    void SetMask(wxMask *mask)
    {
        delete m_bitmapMask;
        m_bitmapMask = mask;
    }

    // set the HBITMAP to use as the mask
    void SetMask(HBITMAP hbmpMask)
    {
        SetMask(new wxMask((WXHBITMAP)hbmpMask));
    }

    // return the mask
    wxMask *GetMask() const { return m_bitmapMask; }

public:
#if wxUSE_PALETTE
    wxPalette     m_bitmapPalette;
#endif // wxUSE_PALETTE

    // MSW-specific
    // ------------

#if wxDEBUG_LEVEL
    // this field is solely for error checking: we detect selecting a bitmap
    // into more than one DC at once or deleting a bitmap still selected into a
    // DC (both are serious programming errors under Windows)
    wxDC         *m_selectedInto;
#endif // wxDEBUG_LEVEL

#if wxUSE_WXDIB
    // when GetRawData() is called for a DDB we need to convert it to a DIB
    // first to be able to provide direct access to it and we cache that DIB
    // here and convert it back to DDB when UngetRawData() is called
    wxDIB *m_dib;
#endif

    // true if we have alpha transparency info and can be drawn using
    // AlphaBlend()
    bool m_hasAlpha;

    // true if our HBITMAP is a DIB section, false if it is a DDB
    bool m_isDIB;

private:
    void Init();

    // Initialize using the given DIB but use (and take ownership of) the
    // bitmap handle if it is valid, assuming it's a DDB. If it's not valid,
    // use the DIB handle itself taking ownership of it (i.e. wxDIB will become
    // invalid when this function returns even though we take it as const
    // reference because this is how it's passed to us).
    void InitFromDIB(const wxDIB& dib, HBITMAP hbitmap = NULL);


    // optional mask for transparent drawing
    wxMask       *m_bitmapMask;


    // not implemented
    wxBitmapRefData& operator=(const wxBitmapRefData&);
};

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxBitmap, wxGDIObject)
IMPLEMENT_DYNAMIC_CLASS(wxMask, wxObject)

IMPLEMENT_DYNAMIC_CLASS(wxBitmapHandler, wxObject)

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

// decide whether we should create a DIB or a DDB for the given parameters
//
// NB: we always use DIBs under Windows CE as this is much simpler (even if
//     also less efficient...) and we obviously can't use them if there is no
//     DIB support compiled in at all
#ifdef __WXWINCE__
    static inline bool wxShouldCreateDIB(int, int, int, WXHDC) { return true; }

    #define ALWAYS_USE_DIB
#elif !wxUSE_WXDIB
    // no sense in defining wxShouldCreateDIB() as we can't compile code
    // executed if it is true, so we have to use #if's anyhow
    #define NEVER_USE_DIB
#else // wxUSE_WXDIB && !__WXWINCE__
    static inline bool wxShouldCreateDIB(int w, int h, int d, WXHDC hdc)
    {
        // here is the logic:
        //
        //  (a) if hdc is specified, the caller explicitly wants DDB
        //  (b) otherwise, create a DIB if depth >= 24 (we don't support 16bpp
        //      or less DIBs anyhow)
        //  (c) finally, create DIBs under Win9x even if the depth hasn't been
        //      explicitly specified but the current display depth is 24 or
        //      more and the image is "big", i.e. > 16Mb which is the
        //      theoretical limit for DDBs under Win9x
        //
        // consequences (all of which seem to make sense):
        //
        //  (i)     by default, DDBs are created (depth == -1 usually)
        //  (ii)    DIBs can be created by explicitly specifying the depth
        //  (iii)   using a DC always forces creating a DDB
        return !hdc &&
                (d >= 24 ||
                    (d == -1 &&
                        wxDIB::GetLineSize(w, wxDisplayDepth())*h > 16*1024*1024));
    }

    #define SOMETIMES_USE_DIB
#endif // different DIB usage scenarious

// ----------------------------------------------------------------------------
// wxBitmapRefData
// ----------------------------------------------------------------------------

void wxBitmapRefData::Init()
{
#if wxDEBUG_LEVEL
    m_selectedInto = NULL;
#endif
    m_bitmapMask = NULL;

    m_hBitmap = (WXHBITMAP) NULL;
#if wxUSE_WXDIB
    m_dib = NULL;
#endif

    m_isDIB =
    m_hasAlpha = false;
}

wxBitmapRefData::wxBitmapRefData(const wxBitmapRefData& data)
               : wxGDIImageRefData(data)
{
    Init();

    // (deep) copy the mask if present
    if (data.m_bitmapMask)
        m_bitmapMask = new wxMask(*data.m_bitmapMask);

    wxASSERT_MSG( !data.m_dib,
                    wxT("can't copy bitmap locked for raw access!") );

    m_hasAlpha = data.m_hasAlpha;

#if wxUSE_WXDIB
    // copy the other bitmap
    if ( data.m_hBitmap )
    {
        wxDIB dib((HBITMAP)(data.m_hBitmap));
        CopyFromDIB(dib);
    }
#endif // wxUSE_WXDIB
}

void wxBitmapRefData::Free()
{
    wxASSERT_MSG( !m_selectedInto,
                  wxT("deleting bitmap still selected into wxMemoryDC") );

#if wxUSE_WXDIB
    wxASSERT_MSG( !m_dib, wxT("forgot to call wxBitmap::UngetRawData()!") );
#endif

    if ( m_hBitmap)
    {
        if ( !::DeleteObject((HBITMAP)m_hBitmap) )
        {
            wxLogLastError(wxT("DeleteObject(hbitmap)"));
        }
    }

    wxDELETE(m_bitmapMask);
}

void wxBitmapRefData::InitFromDIB(const wxDIB& dib, HBITMAP hbitmap)
{
    m_width = dib.GetWidth();
    m_height = dib.GetHeight();
    m_depth = dib.GetDepth();

#if wxUSE_PALETTE
    wxPalette *palette = dib.CreatePalette();
    if ( palette )
        m_bitmapPalette = *palette;
    delete palette;
#endif // wxUSE_PALETTE

    if ( hbitmap )
    {
        // We assume it's a DDB, otherwise there would be no point in passing
        // it to us in addition to the DIB.
        m_isDIB = false;
    }
    else // No valid DDB provided
    {
        // Just use DIB itself.
        m_isDIB = true;

        // Notice that we must not try to use the DIB after calling Detach() on
        // it, e.g. this must be done after calling CreatePalette() above.
        hbitmap = const_cast<wxDIB &>(dib).Detach();
    }

    // In any case, take ownership of this bitmap.
    m_hBitmap = (WXHBITMAP)hbitmap;
}

void wxBitmapRefData::CopyFromDIB(const wxDIB& dib)
{
    wxCHECK_RET( !IsOk(), "bitmap already initialized" );
    wxCHECK_RET( dib.IsOk(), wxT("invalid DIB in CopyFromDIB") );

    HBITMAP hbitmap;
#ifdef SOMETIMES_USE_DIB
    hbitmap = dib.CreateDDB();
#else // ALWAYS_USE_DIB
    hbitmap = NULL;
#endif // SOMETIMES_USE_DIB/ALWAYS_USE_DIB

    InitFromDIB(dib, hbitmap);
}

#if wxUSE_WXDIB

bool wxBitmapRefData::AssignDIB(wxDIB& dib)
{
    if ( !dib.IsOk() )
        return false;

    Free();
    InitFromDIB(dib);

    return true;
}

#endif // wxUSE_WXDIB

// ----------------------------------------------------------------------------
// wxBitmap creation
// ----------------------------------------------------------------------------

wxGDIImageRefData *wxBitmap::CreateData() const
{
    return new wxBitmapRefData;
}

wxGDIRefData *wxBitmap::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxBitmapRefData(*static_cast<const wxBitmapRefData *>(data));
}

// Premultiply the values of all RGBA pixels in the given range.
static void PremultiplyPixels(unsigned char* begin, unsigned char* end)
{
    for ( unsigned char* pixels = begin; pixels < end; pixels += 4 )
    {
        const unsigned char a = pixels[3];

        pixels[0] = ((pixels[0]*a) + 127)/255;
        pixels[1] = ((pixels[1]*a) + 127)/255;
        pixels[2] = ((pixels[2]*a) + 127)/255;
    }
}

// Helper which examines the alpha channel for any non-0 values and also
// possibly returns the DIB with premultiplied values if it does have alpha
// (i.e. this DIB is only filled if the function returns true).
//
// The function semantics is complicated but necessary to avoid converting to
// DIB twice, which is expensive for large bitmaps, yet avoid code duplication
// between CopyFromIconOrCursor() and MSWUpdateAlpha().
static bool CheckAlpha(HBITMAP hbmp, HBITMAP* hdib = NULL)
{
    BITMAP bm;
    if ( !::GetObject(hbmp, sizeof(bm), &bm) || (bm.bmBitsPixel != 32) )
        return false;

    wxDIB dib(hbmp);
    if ( !dib.IsOk() )
        return false;

    unsigned char* pixels = dib.GetData();
    unsigned char* const end = pixels + 4*dib.GetWidth()*dib.GetHeight();
    for ( ; pixels < end; pixels += 4 )
    {
        if ( pixels[3] != 0 )
        {
            if ( hdib )
            {
                // If we do have alpha, ensure we use premultiplied data for
                // our pixels as this is what the bitmaps created in other ways
                // do and this is necessary for e.g. AlphaBlend() to work with
                // this bitmap.
                PremultiplyPixels(dib.GetData(), end);

                *hdib = dib.Detach();
            }

            return true;
        }
    }

    return false;
}

bool wxBitmap::CopyFromIconOrCursor(const wxGDIImage& icon,
                                    wxBitmapTransparency transp)
{
#if !defined(__WXMICROWIN__) && !defined(__WXWINCE__)
    // it may be either HICON or HCURSOR
    HICON hicon = (HICON)icon.GetHandle();

    ICONINFO iconInfo;
    if ( !::GetIconInfo(hicon, &iconInfo) )
    {
        wxLogLastError(wxT("GetIconInfo"));

        return false;
    }

    wxBitmapRefData *refData = new wxBitmapRefData;
    m_refData = refData;

    int w = icon.GetWidth(),
        h = icon.GetHeight();

    if ( iconInfo.hbmColor )
    {
        refData->m_width = w;
        refData->m_height = h;
        refData->m_depth = wxDisplayDepth();
        refData->m_hBitmap = (WXHBITMAP)iconInfo.hbmColor;
    }
    else // we only have monochrome icon/cursor
    {
        // Then we need to create our own empty bitmap, which will be modified
        // by the mask below.
        wxDIB dib(w, h, wxDisplayDepth());
        if ( dib.IsOk() )
        {
            memset(dib.GetData(), 0, wxDIB::GetLineSize(w, dib.GetDepth())*h);
            refData->AssignDIB(dib);
        }
    }

    switch ( transp )
    {
        default:
            wxFAIL_MSG( wxT("unknown wxBitmapTransparency value") );

        case wxBitmapTransparency_None:
            // nothing to do, refData->m_hasAlpha is false by default
            break;

        case wxBitmapTransparency_Auto:
#if wxUSE_WXDIB
            // If the icon is 32 bits per pixel then it may have alpha channel
            // data, although there are some icons that are 32 bpp but have no
            // alpha, so check for this.
            {
                HBITMAP hdib = 0;
                if ( CheckAlpha(iconInfo.hbmColor, &hdib) )
                {
                    refData->m_hasAlpha = true;
                    ::DeleteObject(refData->m_hBitmap);
                    refData->m_hBitmap = hdib;
                }
            }
            break;
#endif // wxUSE_WXDIB

        case wxBitmapTransparency_Always:
            refData->m_hasAlpha = true;
            break;
    }

    if ( !refData->m_hasAlpha )
    {
        // the mask returned by GetIconInfo() is inverted compared to the usual
        // wxWin convention
        refData->SetMask(wxInvertMask(iconInfo.hbmMask, w, h));
    }

    // delete the old one now as we don't need it any more
    ::DeleteObject(iconInfo.hbmMask);

    return true;
#else // __WXMICROWIN__ || __WXWINCE__
    wxUnusedVar(icon);
    wxUnusedVar(transp);

    return false;
#endif // !__WXWINCE__/__WXWINCE__
}

bool wxBitmap::CopyFromCursor(const wxCursor& cursor, wxBitmapTransparency transp)
{
    UnRef();

    if ( !cursor.IsOk() )
        return false;

    return CopyFromIconOrCursor(cursor, transp);
}

bool wxBitmap::CopyFromIcon(const wxIcon& icon, wxBitmapTransparency transp)
{
    UnRef();

    if ( !icon.IsOk() )
        return false;

    return CopyFromIconOrCursor(icon, transp);
}

#if wxUSE_WXDIB

bool wxBitmap::CopyFromDIB(const wxDIB& dib)
{
    wxScopedPtr<wxBitmapRefData> newData(new wxBitmapRefData);
    newData->CopyFromDIB(dib);
    if ( !newData->IsOk() )
        return false;

    UnRef();
    m_refData = newData.release();
    return true;
}

bool wxBitmap::IsDIB() const
{
    return GetBitmapData() && GetBitmapData()->m_isDIB;
}

bool wxBitmap::ConvertToDIB()
{
    if ( IsDIB() )
        return true;

    wxDIB dib(*this);
    if ( !dib.IsOk() )
        return false;

    // It is important to reuse the current GetBitmapData() instead of creating
    // a new one, as our object identity shouldn't change just because our
    // internal representation did, but IsSameAs() compares data pointers.
    return GetBitmapData()->AssignDIB(dib);
}

#endif // wxUSE_WXDIB

wxBitmap::~wxBitmap()
{
}

wxBitmap::wxBitmap(const char bits[], int width, int height, int depth)
{
#ifndef __WXMICROWIN__
    wxBitmapRefData *refData = new wxBitmapRefData;
    m_refData = refData;

    refData->m_width = width;
    refData->m_height = height;
    refData->m_depth = depth;

    char *data;
    if ( depth == 1 )
    {
        // we assume that it is in XBM format which is not quite the same as
        // the format CreateBitmap() wants because the order of bytes in the
        // line is reversed!
        const size_t bytesPerLine = (width + 7) / 8;
        const size_t padding = bytesPerLine % 2;
        const size_t len = height * ( padding + bytesPerLine );
        data = (char *)malloc(len);
        const char *src = bits;
        char *dst = data;

        for ( int rows = 0; rows < height; rows++ )
        {
            for ( size_t cols = 0; cols < bytesPerLine; cols++ )
            {
                unsigned char val = *src++;
                unsigned char reversed = 0;

                for ( int bit = 0; bit < 8; bit++)
                {
                    reversed <<= 1;
                    reversed |= (unsigned char)(val & 0x01);
                    val >>= 1;
                }
                *dst++ = ~reversed;
            }

            if ( padding )
                *dst++ = 0;
        }
    }
    else
    {
        // bits should already be in Windows standard format
        data = const_cast<char *>(bits);
    }

    HBITMAP hbmp = ::CreateBitmap(width, height, 1, depth, data);
    if ( !hbmp )
    {
        wxLogLastError(wxT("CreateBitmap"));
    }

    if ( data != bits )
    {
        free(data);
    }

    SetHBITMAP((WXHBITMAP)hbmp);
#endif
}

wxBitmap::wxBitmap(int w, int h, const wxDC& dc)
{
    (void)Create(w, h, dc);
}

wxBitmap::wxBitmap(const void* data, wxBitmapType type, int width, int height, int depth)
{
    (void)Create(data, type, width, height, depth);
}

wxBitmap::wxBitmap(const wxString& filename, wxBitmapType type)
{
    LoadFile(filename, type);
}

bool wxBitmap::Create(int width, int height, int depth)
{
    return DoCreate(width, height, depth, 0);
}

bool wxBitmap::Create(int width, int height, const wxDC& dc)
{
    wxCHECK_MSG( dc.IsOk(), false, wxT("invalid HDC in wxBitmap::Create()") );

    const wxMSWDCImpl *impl = wxDynamicCast( dc.GetImpl(), wxMSWDCImpl );

    if (impl)
        return DoCreate(width, height, -1, impl->GetHDC());
    else
        return false;
}

bool wxBitmap::DoCreate(int w, int h, int d, WXHDC hdc)
{
    UnRef();

    m_refData = new wxBitmapRefData;

    GetBitmapData()->m_width = w;
    GetBitmapData()->m_height = h;

    HBITMAP hbmp    wxDUMMY_INITIALIZE(0);

#ifndef NEVER_USE_DIB
    if ( wxShouldCreateDIB(w, h, d, hdc) )
    {
        if ( d == -1 )
        {
            // create DIBs without alpha channel by default
            d = 24;
        }

        wxDIB dib(w, h, d);
        if ( !dib.IsOk() )
           return false;

        // don't delete the DIB section in dib object dtor
        hbmp = dib.Detach();

        GetBitmapData()->m_isDIB = true;
        GetBitmapData()->m_depth = d;
    }
    else // create a DDB
#endif // NEVER_USE_DIB
    {
#ifndef ALWAYS_USE_DIB
#ifndef __WXMICROWIN__
        if ( d > 0 )
        {
            hbmp = ::CreateBitmap(w, h, 1, d, NULL);
            if ( !hbmp )
            {
                wxLogLastError(wxT("CreateBitmap"));
            }

            GetBitmapData()->m_depth = d;
        }
        else // d == 0, create bitmap compatible with the screen
#endif // !__WXMICROWIN__
        {
            ScreenHDC dc;
            hbmp = ::CreateCompatibleBitmap(dc, w, h);
            if ( !hbmp )
            {
                wxLogLastError(wxT("CreateCompatibleBitmap"));
            }

            GetBitmapData()->m_depth = wxDisplayDepth();
        }
#endif // !ALWAYS_USE_DIB
    }

    SetHBITMAP((WXHBITMAP)hbmp);

    return IsOk();
}

#if wxUSE_IMAGE

// ----------------------------------------------------------------------------
// wxImage to/from conversions for Microwin
// ----------------------------------------------------------------------------

// Microwin versions are so different from normal ones that it really doesn't
// make sense to use #ifdefs inside the function bodies
#ifdef __WXMICROWIN__

bool wxBitmap::CreateFromImage(const wxImage& image, int depth, const wxDC& dc)
{
    // Set this to 1 to experiment with mask code,
    // which currently doesn't work
    #define USE_MASKS 0

    m_refData = new wxBitmapRefData();

    // Initial attempt at a simple-minded implementation.
    // The bitmap will always be created at the screen depth,
    // so the 'depth' argument is ignored.

    HDC hScreenDC = ::GetDC(NULL);
    int screenDepth = ::GetDeviceCaps(hScreenDC, BITSPIXEL);

    HBITMAP hBitmap = ::CreateCompatibleBitmap(hScreenDC, image.GetWidth(), image.GetHeight());
    HBITMAP hMaskBitmap = NULL;
    HBITMAP hOldMaskBitmap = NULL;
    HDC hMaskDC = NULL;
    unsigned char maskR = 0;
    unsigned char maskG = 0;
    unsigned char maskB = 0;

    //    printf("Created bitmap %d\n", (int) hBitmap);
    if (hBitmap == NULL)
    {
        ::ReleaseDC(NULL, hScreenDC);
        return false;
    }
    HDC hMemDC = ::CreateCompatibleDC(hScreenDC);

    HBITMAP hOldBitmap = ::SelectObject(hMemDC, hBitmap);
    ::ReleaseDC(NULL, hScreenDC);

    // created an mono-bitmap for the possible mask
    bool hasMask = image.HasMask();

    if ( hasMask )
    {
#if USE_MASKS
        // FIXME: we should be able to pass bpp = 1, but
        // GdBlit can't handle a different depth
#if 0
        hMaskBitmap = ::CreateBitmap( (WORD)image.GetWidth(), (WORD)image.GetHeight(), 1, 1, NULL );
#else
        hMaskBitmap = ::CreateCompatibleBitmap( hMemDC, (WORD)image.GetWidth(), (WORD)image.GetHeight());
#endif
        maskR = image.GetMaskRed();
        maskG = image.GetMaskGreen();
        maskB = image.GetMaskBlue();

        if (!hMaskBitmap)
        {
            hasMask = false;
        }
        else
        {
            hScreenDC = ::GetDC(NULL);
            hMaskDC = ::CreateCompatibleDC(hScreenDC);
           ::ReleaseDC(NULL, hScreenDC);

            hOldMaskBitmap = ::SelectObject( hMaskDC, hMaskBitmap);
        }
#else
        hasMask = false;
#endif
    }

    int i, j;
    for (i = 0; i < image.GetWidth(); i++)
    {
        for (j = 0; j < image.GetHeight(); j++)
        {
            unsigned char red = image.GetRed(i, j);
            unsigned char green = image.GetGreen(i, j);
            unsigned char blue = image.GetBlue(i, j);

            ::SetPixel(hMemDC, i, j, PALETTERGB(red, green, blue));

            if (hasMask)
            {
                // scan the bitmap for the transparent colour and set the corresponding
                // pixels in the mask to BLACK and the rest to WHITE
                if (maskR == red && maskG == green && maskB == blue)
                    ::SetPixel(hMaskDC, i, j, PALETTERGB(0, 0, 0));
                else
                    ::SetPixel(hMaskDC, i, j, PALETTERGB(255, 255, 255));
            }
        }
    }

    ::SelectObject(hMemDC, hOldBitmap);
    ::DeleteDC(hMemDC);
    if (hasMask)
    {
        ::SelectObject(hMaskDC, hOldMaskBitmap);
        ::DeleteDC(hMaskDC);

        ((wxBitmapRefData*)m_refData)->SetMask(hMaskBitmap);
    }

    SetWidth(image.GetWidth());
    SetHeight(image.GetHeight());
    SetDepth(screenDepth);
    SetHBITMAP( (WXHBITMAP) hBitmap );

#if wxUSE_PALETTE
    // Copy the palette from the source image
    SetPalette(image.GetPalette());
#endif // wxUSE_PALETTE

    return true;
}

wxImage wxBitmap::ConvertToImage() const
{
    // Initial attempt at a simple-minded implementation.
    // The bitmap will always be created at the screen depth,
    // so the 'depth' argument is ignored.
    // TODO: transparency (create a mask image)

    if (!IsOk())
    {
        wxFAIL_MSG( wxT("bitmap is invalid") );
        return wxNullImage;
    }

    wxImage image;

    wxCHECK_MSG( IsOk(), wxNullImage, wxT("invalid bitmap") );

    // create an wxImage object
    int width = GetWidth();
    int height = GetHeight();
    image.Create( width, height );
    unsigned char *data = image.GetData();
    if( !data )
    {
        wxFAIL_MSG( wxT("could not allocate data for image") );
        return wxNullImage;
    }

    HDC hScreenDC = ::GetDC(NULL);

    HDC hMemDC = ::CreateCompatibleDC(hScreenDC);
    ::ReleaseDC(NULL, hScreenDC);

    HBITMAP hBitmap = (HBITMAP) GetHBITMAP();

    HBITMAP hOldBitmap = ::SelectObject(hMemDC, hBitmap);

    int i, j;
    for (i = 0; i < GetWidth(); i++)
    {
        for (j = 0; j < GetHeight(); j++)
        {
            COLORREF color = ::GetPixel(hMemDC, i, j);
            unsigned char red = GetRValue(color);
            unsigned char green = GetGValue(color);
            unsigned char blue = GetBValue(color);

            image.SetRGB(i, j, red, green, blue);
        }
    }

    ::SelectObject(hMemDC, hOldBitmap);
    ::DeleteDC(hMemDC);

#if wxUSE_PALETTE
    // Copy the palette from the source image
    if (GetPalette())
        image.SetPalette(* GetPalette());
#endif // wxUSE_PALETTE

    return image;
}

#endif // __WXMICROWIN__

// ----------------------------------------------------------------------------
// wxImage to/from conversions
// ----------------------------------------------------------------------------

bool wxBitmap::CreateFromImage(const wxImage& image, int depth)
{
    return CreateFromImage(image, depth, 0);
}

bool wxBitmap::CreateFromImage(const wxImage& image, const wxDC& dc)
{
    wxCHECK_MSG( dc.IsOk(), false,
                    wxT("invalid HDC in wxBitmap::CreateFromImage()") );

    const wxMSWDCImpl *impl = wxDynamicCast( dc.GetImpl(), wxMSWDCImpl );

    if (impl)
        return CreateFromImage(image, -1, impl->GetHDC());
    else
        return false;
}

#if wxUSE_WXDIB

bool wxBitmap::CreateFromImage(const wxImage& image, int depth, WXHDC hdc)
{
    wxCHECK_MSG( image.IsOk(), false, wxT("invalid image") );

    UnRef();

    // first convert the image to DIB
    const int h = image.GetHeight();
    const int w = image.GetWidth();

    wxDIB dib(image);
    if ( !dib.IsOk() )
        return false;

    const bool hasAlpha = image.HasAlpha();

    if (depth == -1)
      depth = dib.GetDepth();

    // store the bitmap parameters
    wxBitmapRefData * const refData = new wxBitmapRefData;
    refData->m_width = w;
    refData->m_height = h;
    refData->m_hasAlpha = hasAlpha;
    refData->m_depth = depth;

    m_refData = refData;


    // next either store DIB as is or create a DDB from it
    HBITMAP hbitmap     wxDUMMY_INITIALIZE(0);

    // are we going to use DIB?
    //
    // NB: DDBs don't support alpha so if we have alpha channel we must use DIB
    if ( hasAlpha || wxShouldCreateDIB(w, h, depth, hdc) )
    {
        // don't delete the DIB section in dib object dtor
        hbitmap = dib.Detach();

        refData->m_isDIB = true;
    }
#ifndef ALWAYS_USE_DIB
    else // we need to convert DIB to DDB
    {
        hbitmap = dib.CreateDDB((HDC)hdc);
    }
#endif // !ALWAYS_USE_DIB

    // validate this object
    SetHBITMAP((WXHBITMAP)hbitmap);

    // finally also set the mask if we have one
    if ( image.HasMask() )
    {
        const size_t len  = 2*((w+15)/16);
        BYTE *src  = image.GetData();
        BYTE *data = new BYTE[h*len];
        memset(data, 0, h*len);
        BYTE r = image.GetMaskRed(),
             g = image.GetMaskGreen(),
             b = image.GetMaskBlue();
        BYTE *dst = data;
        for ( int y = 0; y < h; y++, dst += len )
        {
            BYTE *dstLine = dst;
            BYTE mask = 0x80;
            for ( int x = 0; x < w; x++, src += 3 )
            {
                if (src[0] != r || src[1] != g || src[2] != b)
                    *dstLine |= mask;

                if ( (mask >>= 1) == 0 )
                {
                    dstLine++;
                    mask = 0x80;
                }
            }
        }

        hbitmap = ::CreateBitmap(w, h, 1, 1, data);
        if ( !hbitmap )
        {
            wxLogLastError(wxT("CreateBitmap(mask)"));
        }
        else
        {
            SetMask(new wxMask((WXHBITMAP)hbitmap));
        }

        delete[] data;
    }

    return true;
}

wxImage wxBitmap::ConvertToImage() const
{
    // convert DDB to DIB
    wxDIB dib(*this);

    if ( !dib.IsOk() )
    {
        return wxNullImage;
    }

    // and then DIB to our wxImage
    wxImage image = dib.ConvertToImage();
    if ( !image.IsOk() )
    {
        return wxNullImage;
    }

    // now do the same for the mask, if we have any
    HBITMAP hbmpMask = GetMask() ? (HBITMAP) GetMask()->GetMaskBitmap() : NULL;
    if ( hbmpMask )
    {
        wxDIB dibMask(hbmpMask);
        if ( dibMask.IsOk() )
        {
            // TODO: use wxRawBitmap to iterate over DIB

            // we hard code the mask colour for now but we could also make an
            // effort (and waste time) to choose a colour not present in the
            // image already to avoid having to fudge the pixels below --
            // whether it's worth to do it is unclear however
            static const int MASK_RED = 1;
            static const int MASK_GREEN = 2;
            static const int MASK_BLUE = 3;
            static const int MASK_BLUE_REPLACEMENT = 2;

            const int h = dibMask.GetHeight();
            const int w = dibMask.GetWidth();
            const int bpp = dibMask.GetDepth();
            const int maskBytesPerPixel = bpp >> 3;
            const int maskBytesPerLine = wxDIB::GetLineSize(w, bpp);
            unsigned char *data = image.GetData();

            // remember that DIBs are stored in bottom to top order
            unsigned char *
                maskLineStart = dibMask.GetData() + ((h - 1) * maskBytesPerLine);

            for ( int y = 0; y < h; y++, maskLineStart -= maskBytesPerLine )
            {
                // traverse one mask DIB line
                unsigned char *mask = maskLineStart;
                for ( int x = 0; x < w; x++, mask += maskBytesPerPixel )
                {
                    // should this pixel be transparent?
                    if ( *mask )
                    {
                        // no, check that it isn't transparent by accident
                        if ( (data[0] == MASK_RED) &&
                                (data[1] == MASK_GREEN) &&
                                    (data[2] == MASK_BLUE) )
                        {
                            // we have to fudge the colour a bit to prevent
                            // this pixel from appearing transparent
                            data[2] = MASK_BLUE_REPLACEMENT;
                        }

                        data += 3;
                    }
                    else // yes, transparent pixel
                    {
                        *data++ = MASK_RED;
                        *data++ = MASK_GREEN;
                        *data++ = MASK_BLUE;
                    }
                }
            }

            image.SetMaskColour(MASK_RED, MASK_GREEN, MASK_BLUE);
        }
    }

    return image;
}

#else // !wxUSE_WXDIB

bool
wxBitmap::CreateFromImage(const wxImage& WXUNUSED(image),
                          int WXUNUSED(depth),
                          WXHDC WXUNUSED(hdc))
{
    return false;
}

wxImage wxBitmap::ConvertToImage() const
{
    return wxImage();
}

#endif // wxUSE_WXDIB/!wxUSE_WXDIB

#endif // wxUSE_IMAGE

// ----------------------------------------------------------------------------
// loading and saving bitmaps
// ----------------------------------------------------------------------------

bool wxBitmap::LoadFile(const wxString& filename, wxBitmapType type)
{
    UnRef();

    wxBitmapHandler *handler = wxDynamicCast(FindHandler(type), wxBitmapHandler);

    if ( handler )
    {
        m_refData = new wxBitmapRefData;

        return handler->LoadFile(this, filename, type, -1, -1);
    }
#if wxUSE_IMAGE && wxUSE_WXDIB
    else // no bitmap handler found
    {
        wxImage image;
        if ( image.LoadFile( filename, type ) && image.IsOk() )
        {
            *this = wxBitmap(image);

            return true;
        }
    }
#endif // wxUSE_IMAGE

    return false;
}

bool wxBitmap::Create(const void* data, wxBitmapType type, int width, int height, int depth)
{
    UnRef();

    wxBitmapHandler *handler = wxDynamicCast(FindHandler(type), wxBitmapHandler);

    if ( !handler )
    {
        wxLogDebug(wxT("Failed to create bitmap: no bitmap handler for type %ld defined."), type);

        return false;
    }

    m_refData = new wxBitmapRefData;

    return handler->Create(this, data, type, width, height, depth);
}

bool wxBitmap::SaveFile(const wxString& filename,
                        wxBitmapType type,
                        const wxPalette *palette) const
{
    wxBitmapHandler *handler = wxDynamicCast(FindHandler(type), wxBitmapHandler);

    if ( handler )
    {
        return handler->SaveFile(this, filename, type, palette);
    }
#if wxUSE_IMAGE && wxUSE_WXDIB
    else // no bitmap handler found
    {
        // FIXME what about palette? shouldn't we use it?
        wxImage image = ConvertToImage();
        if ( image.IsOk() )
        {
            return image.SaveFile(filename, type);
        }
    }
#endif // wxUSE_IMAGE

    return false;
}

// ----------------------------------------------------------------------------
// sub bitmap extraction
// ----------------------------------------------------------------------------
wxBitmap wxBitmap::GetSubBitmap( const wxRect& rect ) const
{
        MemoryHDC dcSrc;
        SelectInHDC selectSrc(dcSrc, GetHbitmap());
        return GetSubBitmapOfHDC( rect, (WXHDC)dcSrc );
}

wxBitmap wxBitmap::GetSubBitmapOfHDC( const wxRect& rect, WXHDC hdc ) const
{
    wxCHECK_MSG( IsOk() &&
                 (rect.x >= 0) && (rect.y >= 0) &&
                 (rect.x+rect.width <= GetWidth()) &&
                 (rect.y+rect.height <= GetHeight()),
                 wxNullBitmap, wxT("Invalid bitmap or bitmap region") );

    wxBitmap ret( rect.width, rect.height, GetDepth() );
    wxASSERT_MSG( ret.IsOk(), wxT("GetSubBitmap error") );

#ifndef __WXMICROWIN__
    // handle alpha channel, if any
    if (HasAlpha())
        ret.UseAlpha();

    // copy bitmap data
    MemoryHDC dcSrc,
              dcDst;

    {
        SelectInHDC selectDst(dcDst, GetHbitmapOf(ret));

        if ( !selectDst )
        {
            wxLogLastError(wxT("SelectObject(destBitmap)"));
        }

        if ( !::BitBlt(dcDst, 0, 0, rect.width, rect.height,
                       (HDC)hdc, rect.x, rect.y, SRCCOPY) )
        {
            wxLogLastError(wxT("BitBlt"));
        }
    }

    // copy mask if there is one
    if ( GetMask() )
    {
        HBITMAP hbmpMask = ::CreateBitmap(rect.width, rect.height, 1, 1, 0);

        SelectInHDC selectSrc(dcSrc, (HBITMAP) GetMask()->GetMaskBitmap()),
                    selectDst(dcDst, hbmpMask);

        if ( !::BitBlt(dcDst, 0, 0, rect.width, rect.height,
                       dcSrc, rect.x, rect.y, SRCCOPY) )
        {
            wxLogLastError(wxT("BitBlt"));
        }

        wxMask *mask = new wxMask((WXHBITMAP) hbmpMask);
        ret.SetMask(mask);
    }
#endif // !__WXMICROWIN__

    return ret;
}

// ----------------------------------------------------------------------------
// wxBitmap accessors
// ----------------------------------------------------------------------------

#if wxUSE_PALETTE
wxPalette* wxBitmap::GetPalette() const
{
    return GetBitmapData() ? &GetBitmapData()->m_bitmapPalette
                           : NULL;
}
#endif

wxMask *wxBitmap::GetMask() const
{
    return GetBitmapData() ? GetBitmapData()->GetMask() : NULL;
}

wxDC *wxBitmap::GetSelectedInto() const
{
#if wxDEBUG_LEVEL
    return GetBitmapData() ? GetBitmapData()->m_selectedInto : NULL;
#else
    return NULL;
#endif
}

void wxBitmap::UseAlpha()
{
    if ( GetBitmapData() )
        GetBitmapData()->m_hasAlpha = true;
}

void wxBitmap::ResetAlpha()
{
    if ( GetBitmapData() )
        GetBitmapData()->m_hasAlpha = false;
}

bool wxBitmap::HasAlpha() const
{
    return GetBitmapData() && GetBitmapData()->m_hasAlpha;
}

void wxBitmap::MSWUpdateAlpha()
{
    if ( CheckAlpha(GetHbitmap()) )
        GetBitmapData()->m_hasAlpha = true;
}

// ----------------------------------------------------------------------------
// wxBitmap setters
// ----------------------------------------------------------------------------

void wxBitmap::SetSelectedInto(wxDC *dc)
{
#if wxDEBUG_LEVEL
    if ( GetBitmapData() )
        GetBitmapData()->m_selectedInto = dc;
#else
    wxUnusedVar(dc);
#endif
}

#if wxUSE_PALETTE

void wxBitmap::SetPalette(const wxPalette& palette)
{
    AllocExclusive();

    GetBitmapData()->m_bitmapPalette = palette;
}

#endif // wxUSE_PALETTE

void wxBitmap::SetMask(wxMask *mask)
{
    AllocExclusive();

    GetBitmapData()->SetMask(mask);
}

// ----------------------------------------------------------------------------
// raw bitmap access support
// ----------------------------------------------------------------------------

#ifdef wxHAS_RAW_BITMAP

void *wxBitmap::GetRawData(wxPixelDataBase& data, int bpp)
{
#if wxUSE_WXDIB
    if ( !IsOk() )
    {
        // no bitmap, no data (raw or otherwise)
        return NULL;
    }

    // if we're already a DIB we can access our data directly, but if not we
    // need to convert this DDB to a DIB section and use it for raw access and
    // then convert it back
    HBITMAP hDIB;
    if ( !GetBitmapData()->m_isDIB )
    {
        wxCHECK_MSG( !GetBitmapData()->m_dib, NULL,
                        wxT("GetRawData() may be called only once") );

        wxDIB *dib = new wxDIB(*this);
        if ( !dib->IsOk() )
        {
            delete dib;

            return NULL;
        }

        // we'll free it in UngetRawData()
        GetBitmapData()->m_dib = dib;

        hDIB = dib->GetHandle();
    }
    else // we're a DIB
    {
        hDIB = GetHbitmap();
    }

    DIBSECTION ds;
    if ( ::GetObject(hDIB, sizeof(ds), &ds) != sizeof(DIBSECTION) )
    {
        wxFAIL_MSG( wxT("failed to get DIBSECTION from a DIB?") );

        return NULL;
    }

    // check that the bitmap is in correct format
    if ( ds.dsBm.bmBitsPixel != bpp )
    {
        wxFAIL_MSG( wxT("incorrect bitmap type in wxBitmap::GetRawData()") );

        return NULL;
    }

    // ok, store the relevant info in wxPixelDataBase
    const LONG h = ds.dsBm.bmHeight;

    data.m_width = ds.dsBm.bmWidth;
    data.m_height = h;

    // remember that DIBs are stored in top to bottom order!
    // (We can't just use ds.dsBm.bmWidthBytes here, because it isn't always a
    // multiple of 2, as required by the documentation.  So we use the official
    // formula, which we already use elsewhere.)
    const LONG bytesPerRow =
        wxDIB::GetLineSize(ds.dsBm.bmWidth, ds.dsBm.bmBitsPixel);
    data.m_stride = -bytesPerRow;

    char *bits = (char *)ds.dsBm.bmBits;
    if ( h > 1 )
    {
        bits += (h - 1)*bytesPerRow;
    }

    return bits;
#else
    return NULL;
#endif
}

void wxBitmap::UngetRawData(wxPixelDataBase& dataBase)
{
#if wxUSE_WXDIB
    if ( !IsOk() )
        return;

    if ( !&dataBase )
    {
        // invalid data, don't crash -- but don't assert neither as we're
        // called automatically from wxPixelDataBase dtor and so there is no
        // way to prevent this from happening
        return;
    }

    // if we're a DDB we need to convert DIB back to DDB now to make the
    // changes made via raw bitmap access effective
    if ( !GetBitmapData()->m_isDIB )
    {
        wxDIB *dib = GetBitmapData()->m_dib;
        GetBitmapData()->m_dib = NULL;

        // TODO: convert

        delete dib;
    }
#endif // wxUSE_WXDIB
}
#endif // wxHAS_RAW_BITMAP

// ----------------------------------------------------------------------------
// wxMask
// ----------------------------------------------------------------------------

wxMask::wxMask()
{
    m_maskBitmap = 0;
}

// Copy constructor
wxMask::wxMask(const wxMask &mask)
      : wxObject()
{
    BITMAP bmp;

    HDC srcDC = CreateCompatibleDC(0);
    HDC destDC = CreateCompatibleDC(0);

    // GetBitmapDimensionEx won't work if SetBitmapDimensionEx wasn't used
    // so we'll use GetObject() API here:
    if (::GetObject((HGDIOBJ)mask.m_maskBitmap, sizeof(bmp), &bmp) == 0)
    {
        wxFAIL_MSG(wxT("Cannot retrieve the dimensions of the wxMask to copy"));
        return;
    }

    // create our HBITMAP
    int w = bmp.bmWidth, h = bmp.bmHeight;
    m_maskBitmap = (WXHBITMAP)CreateCompatibleBitmap(srcDC, w, h);

    // copy the mask's HBITMAP into our HBITMAP
    SelectObject(srcDC, (HBITMAP) mask.m_maskBitmap);
    SelectObject(destDC, (HBITMAP) m_maskBitmap);

    BitBlt(destDC, 0, 0, w, h, srcDC, 0, 0, SRCCOPY);

    SelectObject(srcDC, 0);
    DeleteDC(srcDC);
    SelectObject(destDC, 0);
    DeleteDC(destDC);
}

// Construct a mask from a bitmap and a colour indicating
// the transparent area
wxMask::wxMask(const wxBitmap& bitmap, const wxColour& colour)
{
    m_maskBitmap = 0;
    Create(bitmap, colour);
}

// Construct a mask from a bitmap and a palette index indicating
// the transparent area
wxMask::wxMask(const wxBitmap& bitmap, int paletteIndex)
{
    m_maskBitmap = 0;
    Create(bitmap, paletteIndex);
}

// Construct a mask from a mono bitmap (copies the bitmap).
wxMask::wxMask(const wxBitmap& bitmap)
{
    m_maskBitmap = 0;
    Create(bitmap);
}

wxMask::~wxMask()
{
    if ( m_maskBitmap )
        ::DeleteObject((HBITMAP) m_maskBitmap);
}

// Create a mask from a mono bitmap (copies the bitmap).
bool wxMask::Create(const wxBitmap& bitmap)
{
#ifndef __WXMICROWIN__
    wxCHECK_MSG( bitmap.IsOk() && bitmap.GetDepth() == 1, false,
                 wxT("can't create mask from invalid or not monochrome bitmap") );

    if ( m_maskBitmap )
    {
        ::DeleteObject((HBITMAP) m_maskBitmap);
        m_maskBitmap = 0;
    }

    m_maskBitmap = (WXHBITMAP) CreateBitmap(
                                            bitmap.GetWidth(),
                                            bitmap.GetHeight(),
                                            1, 1, 0
                                           );
    HDC srcDC = CreateCompatibleDC(0);
    SelectObject(srcDC, (HBITMAP) bitmap.GetHBITMAP());
    HDC destDC = CreateCompatibleDC(0);
    SelectObject(destDC, (HBITMAP) m_maskBitmap);
    BitBlt(destDC, 0, 0, bitmap.GetWidth(), bitmap.GetHeight(), srcDC, 0, 0, SRCCOPY);
    SelectObject(srcDC, 0);
    DeleteDC(srcDC);
    SelectObject(destDC, 0);
    DeleteDC(destDC);
    return true;
#else
    wxUnusedVar(bitmap);
    return false;
#endif
}

// Create a mask from a bitmap and a palette index indicating
// the transparent area
bool wxMask::Create(const wxBitmap& bitmap, int paletteIndex)
{
    if ( m_maskBitmap )
    {
        ::DeleteObject((HBITMAP) m_maskBitmap);
        m_maskBitmap = 0;
    }

#if wxUSE_PALETTE
    if (bitmap.IsOk() && bitmap.GetPalette()->IsOk())
    {
        unsigned char red, green, blue;
        if (bitmap.GetPalette()->GetRGB(paletteIndex, &red, &green, &blue))
        {
            wxColour transparentColour(red, green, blue);
            return Create(bitmap, transparentColour);
        }
    }
#endif // wxUSE_PALETTE

    return false;
}

// Create a mask from a bitmap and a colour indicating
// the transparent area
bool wxMask::Create(const wxBitmap& bitmap, const wxColour& colour)
{
#ifndef __WXMICROWIN__
    wxCHECK_MSG( bitmap.IsOk(), false, wxT("invalid bitmap in wxMask::Create") );

    if ( m_maskBitmap )
    {
        ::DeleteObject((HBITMAP) m_maskBitmap);
        m_maskBitmap = 0;
    }

    int width = bitmap.GetWidth(),
        height = bitmap.GetHeight();

    // scan the bitmap for the transparent colour and set the corresponding
    // pixels in the mask to BLACK and the rest to WHITE
    COLORREF maskColour = wxColourToPalRGB(colour);
    m_maskBitmap = (WXHBITMAP)::CreateBitmap(width, height, 1, 1, 0);

    HDC srcDC = ::CreateCompatibleDC(NULL);
    HDC destDC = ::CreateCompatibleDC(NULL);
    if ( !srcDC || !destDC )
    {
        wxLogLastError(wxT("CreateCompatibleDC"));
    }

    bool ok = true;

    // SelectObject() will fail
    wxASSERT_MSG( !bitmap.GetSelectedInto(),
                  wxT("bitmap can't be selected in another DC") );

    HGDIOBJ hbmpSrcOld = ::SelectObject(srcDC, GetHbitmapOf(bitmap));
    if ( !hbmpSrcOld )
    {
        wxLogLastError(wxT("SelectObject"));

        ok = false;
    }

    HGDIOBJ hbmpDstOld = ::SelectObject(destDC, (HBITMAP)m_maskBitmap);
    if ( !hbmpDstOld )
    {
        wxLogLastError(wxT("SelectObject"));

        ok = false;
    }

    if ( ok )
    {
        // this will create a monochrome bitmap with 0 points for the pixels
        // which have the same value as the background colour and 1 for the
        // others
        ::SetBkColor(srcDC, maskColour);
        ::BitBlt(destDC, 0, 0, width, height, srcDC, 0, 0, NOTSRCCOPY);
    }

    ::SelectObject(srcDC, hbmpSrcOld);
    ::DeleteDC(srcDC);
    ::SelectObject(destDC, hbmpDstOld);
    ::DeleteDC(destDC);

    return ok;
#else // __WXMICROWIN__
    wxUnusedVar(bitmap);
    wxUnusedVar(colour);
    return false;
#endif // __WXMICROWIN__/!__WXMICROWIN__
}

wxBitmap wxMask::GetBitmap() const
{
    wxBitmap bmp;
    bmp.SetHBITMAP(m_maskBitmap);
    return bmp;
}

// ----------------------------------------------------------------------------
// wxBitmapHandler
// ----------------------------------------------------------------------------

bool wxBitmapHandler::Create(wxGDIImage *image,
                             const void* data,
                             wxBitmapType type,
                             int width, int height, int depth)
{
    wxBitmap *bitmap = wxDynamicCast(image, wxBitmap);

    return bitmap && Create(bitmap, data, type, width, height, depth);
}

bool wxBitmapHandler::Load(wxGDIImage *image,
                           const wxString& name,
                           wxBitmapType type,
                           int width, int height)
{
    wxBitmap *bitmap = wxDynamicCast(image, wxBitmap);

    return bitmap && LoadFile(bitmap, name, type, width, height);
}

bool wxBitmapHandler::Save(const wxGDIImage *image,
                           const wxString& name,
                           wxBitmapType type) const
{
    wxBitmap *bitmap = wxDynamicCast(image, wxBitmap);

    return bitmap && SaveFile(bitmap, name, type);
}

bool wxBitmapHandler::Create(wxBitmap *WXUNUSED(bitmap),
                             const void* WXUNUSED(data),
                             wxBitmapType WXUNUSED(type),
                             int WXUNUSED(width),
                             int WXUNUSED(height),
                             int WXUNUSED(depth))
{
    return false;
}

bool wxBitmapHandler::LoadFile(wxBitmap *WXUNUSED(bitmap),
                               const wxString& WXUNUSED(name),
                               wxBitmapType WXUNUSED(type),
                               int WXUNUSED(desiredWidth),
                               int WXUNUSED(desiredHeight))
{
    return false;
}

bool wxBitmapHandler::SaveFile(const wxBitmap *WXUNUSED(bitmap),
                               const wxString& WXUNUSED(name),
                               wxBitmapType WXUNUSED(type),
                               const wxPalette *WXUNUSED(palette)) const
{
    return false;
}

// ----------------------------------------------------------------------------
// global helper functions implemented here
// ----------------------------------------------------------------------------

// helper of wxBitmapToHICON/HCURSOR
static
HICON wxBitmapToIconOrCursor(const wxBitmap& bmp,
                             bool iconWanted,
                             int hotSpotX,
                             int hotSpotY)
{
    if ( !bmp.IsOk() )
    {
        // we can't create an icon/cursor form nothing
        return 0;
    }

    if ( bmp.HasAlpha() )
    {
        HBITMAP hbmp;

#if wxUSE_WXDIB && wxUSE_IMAGE
        // CreateIconIndirect() requires non-pre-multiplied pixel data on input
        // as it does pre-multiplication internally itself so we need to create
        // a special DIB in such format to pass to it. This is inefficient but
        // better than creating an icon with wrong colours.
        AutoHBITMAP hbmpRelease;
        hbmp = wxDIB(bmp.ConvertToImage(),
                     wxDIB::PixelFormat_NotPreMultiplied).Detach();
        hbmpRelease.Init(hbmp);
#else // !(wxUSE_WXDIB && wxUSE_IMAGE)
        hbmp = GetHbitmapOf(bmp);
#endif // wxUSE_WXDIB && wxUSE_IMAGE

        // Create an empty mask bitmap.
        // it doesn't seem to work if we mess with the mask at all.
        AutoHBITMAP
            hMonoBitmap(CreateBitmap(bmp.GetWidth(),bmp.GetHeight(),1,1,NULL));

        ICONINFO iconInfo;
        wxZeroMemory(iconInfo);
        iconInfo.fIcon = iconWanted;  // do we want an icon or a cursor?
        if ( !iconWanted )
        {
            iconInfo.xHotspot = hotSpotX;
            iconInfo.yHotspot = hotSpotY;
        }

        iconInfo.hbmMask = hMonoBitmap;
        iconInfo.hbmColor = hbmp;

        return ::CreateIconIndirect(&iconInfo);
    }

    wxMask* mask = bmp.GetMask();

    if ( !mask )
    {
        // we must have a mask for an icon, so even if it's probably incorrect,
        // do create it (grey is the "standard" transparent colour)
        mask = new wxMask(bmp, *wxLIGHT_GREY);
    }

    ICONINFO iconInfo;
    wxZeroMemory(iconInfo);
    iconInfo.fIcon = iconWanted;  // do we want an icon or a cursor?
    if ( !iconWanted )
    {
        iconInfo.xHotspot = hotSpotX;
        iconInfo.yHotspot = hotSpotY;
    }

    AutoHBITMAP hbmpMask(wxInvertMask((HBITMAP)mask->GetMaskBitmap()));
    iconInfo.hbmMask = hbmpMask;
    iconInfo.hbmColor = GetHbitmapOf(bmp);

    // black out the transparent area to preserve background colour, because
    // Windows blits the original bitmap using SRCINVERT (XOR) after applying
    // the mask to the dest rect.
    {
        MemoryHDC dcSrc, dcDst;
        SelectInHDC selectMask(dcSrc, (HBITMAP)mask->GetMaskBitmap()),
                    selectBitmap(dcDst, iconInfo.hbmColor);

        if ( !::BitBlt(dcDst, 0, 0, bmp.GetWidth(), bmp.GetHeight(),
                       dcSrc, 0, 0, SRCAND) )
        {
            wxLogLastError(wxT("BitBlt"));
        }
    }

    HICON hicon = ::CreateIconIndirect(&iconInfo);

    if ( !bmp.GetMask() && !bmp.HasAlpha() )
    {
        // we created the mask, now delete it
        delete mask;
    }

    return hicon;
}

HICON wxBitmapToHICON(const wxBitmap& bmp)
{
    return wxBitmapToIconOrCursor(bmp, true, 0, 0);
}

HCURSOR wxBitmapToHCURSOR(const wxBitmap& bmp, int hotSpotX, int hotSpotY)
{
    return (HCURSOR)wxBitmapToIconOrCursor(bmp, false, hotSpotX, hotSpotY);
}

HBITMAP wxInvertMask(HBITMAP hbmpMask, int w, int h)
{
#ifndef __WXMICROWIN__
    wxCHECK_MSG( hbmpMask, 0, wxT("invalid bitmap in wxInvertMask") );

    // get width/height from the bitmap if not given
    if ( !w || !h )
    {
        BITMAP bm;
        ::GetObject(hbmpMask, sizeof(BITMAP), (LPVOID)&bm);
        w = bm.bmWidth;
        h = bm.bmHeight;
    }

    HDC hdcSrc = ::CreateCompatibleDC(NULL);
    HDC hdcDst = ::CreateCompatibleDC(NULL);
    if ( !hdcSrc || !hdcDst )
    {
        wxLogLastError(wxT("CreateCompatibleDC"));
    }

    HBITMAP hbmpInvMask = ::CreateBitmap(w, h, 1, 1, 0);
    if ( !hbmpInvMask )
    {
        wxLogLastError(wxT("CreateBitmap"));
    }

    HGDIOBJ srcTmp = ::SelectObject(hdcSrc, hbmpMask);
    HGDIOBJ dstTmp = ::SelectObject(hdcDst, hbmpInvMask);
    if ( !::BitBlt(hdcDst, 0, 0, w, h,
                   hdcSrc, 0, 0,
                   NOTSRCCOPY) )
    {
        wxLogLastError(wxT("BitBlt"));
    }

    // Deselect objects
    SelectObject(hdcSrc,srcTmp);
    SelectObject(hdcDst,dstTmp);

    ::DeleteDC(hdcSrc);
    ::DeleteDC(hdcDst);

    return hbmpInvMask;
#else
    return 0;
#endif
}
