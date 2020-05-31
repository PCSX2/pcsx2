/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/dc.cpp
// Purpose:     wxDC class for MSW port
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcdlg.h"
    #include "wx/image.h"
    #include "wx/window.h"
    #include "wx/utils.h"
    #include "wx/dialog.h"
    #include "wx/app.h"
    #include "wx/bitmap.h"
    #include "wx/dcmemory.h"
    #include "wx/log.h"
    #include "wx/math.h"
    #include "wx/icon.h"
    #include "wx/dcprint.h"
    #include "wx/module.h"
#endif

#include "wx/msw/dc.h"
#include "wx/sysopt.h"
#include "wx/dynlib.h"

#ifdef wxHAS_RAW_BITMAP
    #include "wx/rawbmp.h"
#endif

#include <string.h>

#include "wx/msw/private/dc.h"
#include "wx/private/textmeasure.h"

using namespace wxMSWImpl;

#ifndef AC_SRC_ALPHA
    #define AC_SRC_ALPHA 1
#endif

#ifndef LAYOUT_RTL
    #define LAYOUT_RTL 1
#endif

/* Quaternary raster codes */
#ifndef MAKEROP4
#define MAKEROP4(fore,back) (DWORD)((((back) << 8) & 0xFF000000) | (fore))
#endif

// apparently with MicroWindows it is possible that HDC is 0 so we have to
// check for this ourselves
#ifdef __WXMICROWIN__
    #define WXMICROWIN_CHECK_HDC if ( !GetHDC() ) return;
    #define WXMICROWIN_CHECK_HDC_RET(x) if ( !GetHDC() ) return x;
#else
    #define WXMICROWIN_CHECK_HDC
    #define WXMICROWIN_CHECK_HDC_RET(x)
#endif

IMPLEMENT_ABSTRACT_CLASS(wxMSWDCImpl, wxDCImpl)

// ---------------------------------------------------------------------------
// constants
// ---------------------------------------------------------------------------

// The device space in Win32 GDI measures 2^27*2^27 , so we use 2^27-1 as the
// maximal possible view port extent.
static const int VIEWPORT_EXTENT = 134217727;

// ROPs which don't have standard names (see "Ternary Raster Operations" in the
// MSDN docs for how this and other numbers in wxDC::Blit() are obtained)
#define DSTCOPY 0x00AA0029      // a.k.a. NOP operation

// ----------------------------------------------------------------------------
// macros for logical <-> device coords conversion
// ----------------------------------------------------------------------------

/*
   We currently let Windows do all the translations itself so these macros are
   not really needed (any more) but keep them to enhance readability of the
   code by allowing to see where are the logical and where are the device
   coordinates used.
 */

#ifdef __WXWINCE__
    #define XLOG2DEV(x) ((x-m_logicalOriginX)*m_signX)
    #define YLOG2DEV(y) ((y-m_logicalOriginY)*m_signY)
    #define XDEV2LOG(x) ((x)*m_signX+m_logicalOriginX)
    #define YDEV2LOG(y) ((y)*m_signY+m_logicalOriginY)
#else
    #define XLOG2DEV(x) (x)
    #define YLOG2DEV(y) (y)
    #define XDEV2LOG(x) (x)
    #define YDEV2LOG(y) (y)
#endif

// ---------------------------------------------------------------------------
// private functions
// ---------------------------------------------------------------------------

// convert degrees to radians
static inline double DegToRad(double deg) { return (deg * M_PI) / 180.0; }

// call AlphaBlend() to blit contents of hdcSrc to hdcDst using alpha
//
// NB: bmpSrc is the bitmap selected in hdcSrc, it is not really needed
//     to pass it to this function but as we already have it at the point
//     of call anyhow we do
//
// return true if we could draw the bitmap in one way or the other, false
// otherwise
static bool AlphaBlt(HDC hdcDst,
                     int x, int y, int dstWidth, int dstHeight,
                     int srcX, int srcY,
                     int srcWidth, int srcHeight,
                     HDC hdcSrc,
                     const wxBitmap& bmp);

#ifdef wxHAS_RAW_BITMAP

// our (limited) AlphaBlend() replacement for Windows versions not providing it
static void
wxAlphaBlend(HDC hdcDst, int xDst, int yDst,
             int dstWidth, int dstHeight,
             int srcX, int srcY,
             int srcWidth, int srcHeight,
             const wxBitmap& bmpSrc);

#endif // wxHAS_RAW_BITMAP

namespace wxMSWImpl
{

// Wrappers for the dynamically loaded {Set,Get}Layout() functions. They work
// in exactly the same way as the standard functions and return GDI_ERROR if
// they're not actually available.
DWORD GetLayout(HDC hdc);
DWORD SetLayout(HDC hdc, DWORD dwLayout);

// Create a compatible HDC and copy the layout of the source DC to it. This is
// necessary in order to draw bitmaps (which are usually blitted from a
// temporary compatible memory DC to the real target DC) using the same layout.
HDC CreateCompatibleDCWithLayout(HDC hdc);

} // namespace wxMSWImpl

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// instead of duplicating the same code which sets and then restores text
// colours in each wxDC method working with wxSTIPPLE_MASK_OPAQUE brushes,
// encapsulate this in a small helper class

// wxBrushAttrsSetter: changes the text colours in the ctor if required and
//                  restores them in the dtor
class wxBrushAttrsSetter : private wxBkModeChanger,
                           private wxTextColoursChanger
{
public:
    wxBrushAttrsSetter(wxMSWDCImpl& dc);

private:
    wxDECLARE_NO_COPY_CLASS(wxBrushAttrsSetter);
};

#ifdef __WXWINCE__

#define SET_STRETCH_BLT_MODE(hdc)

#else // !__WXWINCE__

// this class sets the stretch blit mode to COLORONCOLOR during its lifetime
//
// don't use it directly, use SET_STRETCH_BLT_MODE() macro instead as it
// expands to nothing under WinCE which doesn't have SetStretchBltMode()
class StretchBltModeChanger
{
public:
    StretchBltModeChanger(HDC hdc)
        : m_hdc(hdc)
    {
        m_modeOld = ::SetStretchBltMode(m_hdc, COLORONCOLOR);
        if ( !m_modeOld )
        {
            wxLogLastError(wxT("SetStretchBltMode"));
        }
    }

    ~StretchBltModeChanger()
    {
        if ( !::SetStretchBltMode(m_hdc, m_modeOld) )
        {
            wxLogLastError(wxT("SetStretchBltMode"));
        }
    }

private:
    const HDC m_hdc;

    int m_modeOld;

    wxDECLARE_NO_COPY_CLASS(StretchBltModeChanger);
};

#define SET_STRETCH_BLT_MODE(hdc) \
    StretchBltModeChanger wxMAKE_UNIQUE_NAME(stretchModeChanger)(hdc)

#endif // __WXWINCE__/!__WXWINCE__

#if wxUSE_DYNLIB_CLASS

// helper class to cache dynamically loaded libraries and not attempt reloading
// them if it fails
class wxOnceOnlyDLLLoader
{
public:
    // ctor argument must be a literal string as we don't make a copy of it!
    wxOnceOnlyDLLLoader(const wxChar *dllName)
        : m_dllName(dllName)
    {
    }


    // return the symbol with the given name or NULL if the DLL not loaded
    // or symbol not present
    void *GetSymbol(const wxChar *name)
    {
        // we're prepared to handle errors here
        wxLogNull noLog;

        if ( m_dllName )
        {
            m_dll.Load(m_dllName);

            // reset the name whether we succeeded or failed so that we don't
            // try again the next time
            m_dllName = NULL;
        }

        return m_dll.IsLoaded() ? m_dll.GetSymbol(name) : NULL;
    }

    void Unload()
    {
        if ( m_dll.IsLoaded() )
        {
            m_dll.Unload();
        }
    }

private:
    wxDynamicLibrary m_dll;
    const wxChar *m_dllName;
};

static wxOnceOnlyDLLLoader wxMSIMG32DLL(wxT("msimg32"));

// we must ensure that DLLs are unloaded before the static objects cleanup time
// because we may hit the notorious DllMain() dead lock in this case if wx is
// used as a DLL (attempting to unload another DLL from inside DllMain() hangs
// under Windows because it tries to reacquire the same lock)
class wxGDIDLLsCleanupModule : public wxModule
{
public:
    virtual bool OnInit() { return true; }
    virtual void OnExit() { wxMSIMG32DLL.Unload(); }

private:
    DECLARE_DYNAMIC_CLASS(wxGDIDLLsCleanupModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxGDIDLLsCleanupModule, wxModule)

namespace
{

#if wxUSE_DC_TRANSFORM_MATRIX

// Class used to dynamically load world transform related API functions.
class GdiWorldTransformFuncs
{
public:
    static bool IsOk()
    {
        if ( !ms_worldTransformSymbolsLoaded )
            LoadWorldTransformSymbols();

        return ms_pfnSetGraphicsMode &&
                ms_pfnSetWorldTransform &&
                 ms_pfnGetWorldTransform &&
                  ms_pfnModifyWorldTransform;
    }

    typedef int (WINAPI *SetGraphicsMode_t)(HDC, int);
    static SetGraphicsMode_t SetGraphicsMode()
    {
        if ( !ms_worldTransformSymbolsLoaded )
            LoadWorldTransformSymbols();

        return ms_pfnSetGraphicsMode;
    }

    typedef BOOL (WINAPI *SetWorldTransform_t)(HDC, const XFORM *);
    static SetWorldTransform_t SetWorldTransform()
    {
        if ( !ms_worldTransformSymbolsLoaded )
            LoadWorldTransformSymbols();

        return ms_pfnSetWorldTransform;
    }

    typedef BOOL (WINAPI *GetWorldTransform_t)(HDC, LPXFORM);
    static GetWorldTransform_t GetWorldTransform()
    {
        if ( !ms_worldTransformSymbolsLoaded )
            LoadWorldTransformSymbols();

        return ms_pfnGetWorldTransform;
    }

    typedef BOOL (WINAPI *ModifyWorldTransform_t)(HDC, const XFORM *, DWORD);
    static ModifyWorldTransform_t ModifyWorldTransform()
    {
        if ( !ms_worldTransformSymbolsLoaded )
            LoadWorldTransformSymbols();

        return ms_pfnModifyWorldTransform;
    }

private:
    static void LoadWorldTransformSymbols()
    {
        wxDynamicLibrary dll(wxT("gdi32.dll"));

        wxDL_INIT_FUNC(ms_pfn, SetGraphicsMode, dll);
        wxDL_INIT_FUNC(ms_pfn, SetWorldTransform, dll);
        wxDL_INIT_FUNC(ms_pfn, GetWorldTransform, dll);
        wxDL_INIT_FUNC(ms_pfn, ModifyWorldTransform, dll);

        ms_worldTransformSymbolsLoaded = true;
    }

    static SetGraphicsMode_t ms_pfnSetGraphicsMode;
    static SetWorldTransform_t ms_pfnSetWorldTransform;
    static GetWorldTransform_t ms_pfnGetWorldTransform;
    static ModifyWorldTransform_t ms_pfnModifyWorldTransform;

    static bool ms_worldTransformSymbolsLoaded;
};

GdiWorldTransformFuncs::SetGraphicsMode_t
    GdiWorldTransformFuncs::ms_pfnSetGraphicsMode = NULL;
GdiWorldTransformFuncs::SetWorldTransform_t
    GdiWorldTransformFuncs::ms_pfnSetWorldTransform = NULL;
GdiWorldTransformFuncs::GetWorldTransform_t
    GdiWorldTransformFuncs::ms_pfnGetWorldTransform = NULL;
GdiWorldTransformFuncs::ModifyWorldTransform_t
    GdiWorldTransformFuncs::ms_pfnModifyWorldTransform = NULL;

bool GdiWorldTransformFuncs::ms_worldTransformSymbolsLoaded = false;

#endif // wxUSE_DC_TRANSFORM_MATRIX

} // anonymous namespace

#endif // wxUSE_DYNLIB_CLASS

// ===========================================================================
// implementation
// ===========================================================================

// ----------------------------------------------------------------------------
// wxBrushAttrsSetter
// ----------------------------------------------------------------------------

wxBrushAttrsSetter::wxBrushAttrsSetter(wxMSWDCImpl& dc)
                  : wxBkModeChanger(GetHdcOf(dc)),
                    wxTextColoursChanger(GetHdcOf(dc))
{
    const wxBrush& brush = dc.GetBrush();
    if ( brush.IsOk() && brush.GetStyle() == wxBRUSHSTYLE_STIPPLE_MASK_OPAQUE )
    {
        // note that Windows convention is opposite to wxWidgets one, this is
        // why text colour becomes the background one and vice versa
        wxTextColoursChanger::Change(dc.GetTextBackground(),
                                     dc.GetTextForeground());

        wxBkModeChanger::Change(dc.GetBackgroundMode());
    }
}

// ----------------------------------------------------------------------------
// wxDC MSW-specific methods
// ----------------------------------------------------------------------------

WXHDC wxDC::GetHDC() const
{
    wxMSWDCImpl * const impl = wxDynamicCast(GetImpl(), wxMSWDCImpl);
    return impl ? impl->GetHDC() : 0;
}

// ---------------------------------------------------------------------------
// wxMSWDCImpl
// ---------------------------------------------------------------------------

wxMSWDCImpl::wxMSWDCImpl( wxDC *owner, WXHDC hDC ) :
    wxDCImpl( owner )
{
    Init();
    m_hDC = hDC;
}

wxMSWDCImpl::~wxMSWDCImpl()
{
    if ( m_hDC != 0 )
    {
        SelectOldObjects(m_hDC);

        // if we own the HDC, we delete it, otherwise we just release it

        if ( m_bOwnsDC )
        {
            ::DeleteDC(GetHdc());
        }
        else // we don't own our HDC
        {
            if (m_window)
            {
                ::ReleaseDC(GetHwndOf(m_window), GetHdc());
            }
            else
            {
                // Must have been a wxScreenDC
                ::ReleaseDC((HWND) NULL, GetHdc());
            }
        }
    }
}

// This will select current objects out of the DC,
// which is what you have to do before deleting the
// DC.
void wxMSWDCImpl::SelectOldObjects(WXHDC dc)
{
    if (dc)
    {
        if (m_oldBitmap)
        {
            ::SelectObject((HDC) dc, (HBITMAP) m_oldBitmap);
            if (m_selectedBitmap.IsOk())
            {
                m_selectedBitmap.SetSelectedInto(NULL);
            }
        }
        m_oldBitmap = 0;
        if (m_oldPen)
        {
            ::SelectObject((HDC) dc, (HPEN) m_oldPen);
        }
        m_oldPen = 0;
        if (m_oldBrush)
        {
            ::SelectObject((HDC) dc, (HBRUSH) m_oldBrush);
        }
        m_oldBrush = 0;
        if (m_oldFont)
        {
            ::SelectObject((HDC) dc, (HFONT) m_oldFont);
        }
        m_oldFont = 0;

#if wxUSE_PALETTE
        if (m_oldPalette)
        {
            ::SelectPalette((HDC) dc, (HPALETTE) m_oldPalette, FALSE);
        }
        m_oldPalette = 0;
#endif // wxUSE_PALETTE
    }

    m_brush = wxNullBrush;
    m_pen = wxNullPen;
#if wxUSE_PALETTE
    m_palette = wxNullPalette;
#endif // wxUSE_PALETTE
    m_font = wxNullFont;
    m_backgroundBrush = wxNullBrush;
    m_selectedBitmap = wxNullBitmap;
}

// ---------------------------------------------------------------------------
// clipping
// ---------------------------------------------------------------------------

void wxMSWDCImpl::UpdateClipBox()
{
    WXMICROWIN_CHECK_HDC

    RECT rect;
    ::GetClipBox(GetHdc(), &rect);

    m_clipX1 = (wxCoord) XDEV2LOG(rect.left);
    m_clipY1 = (wxCoord) YDEV2LOG(rect.top);
    m_clipX2 = (wxCoord) XDEV2LOG(rect.right);
    m_clipY2 = (wxCoord) YDEV2LOG(rect.bottom);
}

void
wxMSWDCImpl::DoGetClippingBox(wxCoord *x, wxCoord *y, wxCoord *w, wxCoord *h) const
{
    // check if we should try to retrieve the clipping region possibly not set
    // by our SetClippingRegion() but preset by Windows:this can only happen
    // when we're associated with an existing HDC usign SetHDC(), see there
    if ( m_clipping && !m_clipX1 && !m_clipX2 )
    {
        wxMSWDCImpl *self = wxConstCast(this, wxMSWDCImpl);
        self->UpdateClipBox();

        if ( !m_clipX1 && !m_clipX2 )
            self->m_clipping = false;
    }

    wxDCImpl::DoGetClippingBox(x, y, w, h);
}

// common part of DoSetClippingRegion() and DoSetDeviceClippingRegion()
void wxMSWDCImpl::SetClippingHrgn(WXHRGN hrgn)
{
    wxCHECK_RET( hrgn, wxT("invalid clipping region") );

    WXMICROWIN_CHECK_HDC

    // note that we combine the new clipping region with the existing one: this
    // is compatible with what the other ports do and is the documented
    // behaviour now (starting with 2.3.3)
#if defined(__WXWINCE__)
    RECT rectClip;
    if ( !::GetClipBox(GetHdc(), &rectClip) )
        return;

    // GetClipBox returns logical coordinates, so transform to device
    rectClip.left = LogicalToDeviceX(rectClip.left);
    rectClip.top = LogicalToDeviceY(rectClip.top);
    rectClip.right = LogicalToDeviceX(rectClip.right);
    rectClip.bottom = LogicalToDeviceY(rectClip.bottom);

    HRGN hrgnDest = ::CreateRectRgn(0, 0, 0, 0);
    HRGN hrgnClipOld = ::CreateRectRgn(rectClip.left, rectClip.top,
                                       rectClip.right, rectClip.bottom);

    if ( ::CombineRgn(hrgnDest, hrgnClipOld, (HRGN)hrgn, RGN_AND) != ERROR )
    {
        ::SelectClipRgn(GetHdc(), hrgnDest);
    }

    ::DeleteObject(hrgnClipOld);
    ::DeleteObject(hrgnDest);
#else // !WinCE
    if ( ::ExtSelectClipRgn(GetHdc(), (HRGN)hrgn, RGN_AND) == ERROR )
    {
        wxLogLastError(wxT("ExtSelectClipRgn"));

        return;
    }
#endif // WinCE/!WinCE

    m_clipping = true;

    UpdateClipBox();
}

void wxMSWDCImpl::DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord w, wxCoord h)
{
    // the region coords are always the device ones, so do the translation
    // manually
    //
    // FIXME: possible +/-1 error here, to check!
    HRGN hrgn = ::CreateRectRgn(LogicalToDeviceX(x),
                                LogicalToDeviceY(y),
                                LogicalToDeviceX(x + w),
                                LogicalToDeviceY(y + h));
    if ( !hrgn )
    {
        wxLogLastError(wxT("CreateRectRgn"));
    }
    else
    {
        SetClippingHrgn((WXHRGN)hrgn);

        ::DeleteObject(hrgn);
    }
}

void wxMSWDCImpl::DoSetDeviceClippingRegion(const wxRegion& region)
{
    SetClippingHrgn(region.GetHRGN());
}

void wxMSWDCImpl::DestroyClippingRegion()
{
    WXMICROWIN_CHECK_HDC

    if (m_clipping && m_hDC)
    {
#if 1
        // On a PocketPC device (not necessarily emulator), resetting
        // the clip region as per the old method causes bad display
        // problems. In fact setting a null region is probably OK
        // on desktop WIN32 also, since the WIN32 docs imply that the user
        // clipping region is independent from the paint clipping region.
        ::SelectClipRgn(GetHdc(), 0);
#else
        // TODO: this should restore the previous clipping region,
        //       so that OnPaint processing works correctly, and the update
        //       clipping region doesn't get destroyed after the first
        //       DestroyClippingRegion.
        HRGN rgn = CreateRectRgn(0, 0, 32000, 32000);
        ::SelectClipRgn(GetHdc(), rgn);
        ::DeleteObject(rgn);
#endif
    }

    wxDCImpl::DestroyClippingRegion();
}

// ---------------------------------------------------------------------------
// query capabilities
// ---------------------------------------------------------------------------

bool wxMSWDCImpl::CanDrawBitmap() const
{
    return true;
}

bool wxMSWDCImpl::CanGetTextExtent() const
{
#ifdef __WXMICROWIN__
    // TODO Extend MicroWindows' GetDeviceCaps function
    return true;
#else
    // What sort of display is it?
    int technology = ::GetDeviceCaps(GetHdc(), TECHNOLOGY);

    return (technology == DT_RASDISPLAY) || (technology == DT_RASPRINTER);
#endif
}

int wxMSWDCImpl::GetDepth() const
{
    WXMICROWIN_CHECK_HDC_RET(16)

    return (int)::GetDeviceCaps(GetHdc(), BITSPIXEL);
}

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------

void wxMSWDCImpl::Clear()
{
    WXMICROWIN_CHECK_HDC

    RECT rect;
    if (m_window)
    {
        GetClientRect((HWND) m_window->GetHWND(), &rect);
    }
    else
    {
        // No, I think we should simply ignore this if printing on e.g.
        // a printer DC.
        // wxCHECK_RET( m_selectedBitmap.IsOk(), wxT("this DC can't be cleared") );
        if (!m_selectedBitmap.IsOk())
            return;

        rect.left = rect.top = 0;
        rect.right = m_selectedBitmap.GetWidth();
        rect.bottom = m_selectedBitmap.GetHeight();
    }

    ::OffsetRect(&rect, -m_deviceOriginX, -m_deviceOriginY);

#ifndef __WXWINCE__
    (void) ::SetMapMode(GetHdc(), MM_TEXT);
#endif

    DWORD colour = ::GetBkColor(GetHdc());
    HBRUSH brush = ::CreateSolidBrush(colour);
    ::FillRect(GetHdc(), &rect, brush);
    ::DeleteObject(brush);

    RealizeScaleAndOrigin();
}

bool wxMSWDCImpl::DoFloodFill(wxCoord WXUNUSED_IN_WINCE(x),
                       wxCoord WXUNUSED_IN_WINCE(y),
                       const wxColour& WXUNUSED_IN_WINCE(col),
                       wxFloodFillStyle WXUNUSED_IN_WINCE(style))
{
#ifdef __WXWINCE__
    return false;
#else
    WXMICROWIN_CHECK_HDC_RET(false)

    bool success = (0 != ::ExtFloodFill(GetHdc(), XLOG2DEV(x), YLOG2DEV(y),
                         col.GetPixel(),
                         style == wxFLOOD_SURFACE ? FLOODFILLSURFACE
                                                  : FLOODFILLBORDER) ) ;
    if (!success)
    {
        // quoting from the MSDN docs:
        //
        //      Following are some of the reasons this function might fail:
        //
        //      * The filling could not be completed.
        //      * The specified point has the boundary color specified by the
        //        crColor parameter (if FLOODFILLBORDER was requested).
        //      * The specified point does not have the color specified by
        //        crColor (if FLOODFILLSURFACE was requested)
        //      * The point is outside the clipping region that is, it is not
        //        visible on the device.
        //
        wxLogLastError(wxT("ExtFloodFill"));
    }

    CalcBoundingBox(x, y);

    return success;
#endif
}

bool wxMSWDCImpl::DoGetPixel(wxCoord x, wxCoord y, wxColour *col) const
{
    WXMICROWIN_CHECK_HDC_RET(false)

    wxCHECK_MSG( col, false, wxT("NULL colour parameter in wxMSWDCImpl::GetPixel") );

    // get the color of the pixel
    COLORREF pixelcolor = ::GetPixel(GetHdc(), XLOG2DEV(x), YLOG2DEV(y));

    wxRGBToColour(*col, pixelcolor);

    return true;
}

void wxMSWDCImpl::DoCrossHair(wxCoord x, wxCoord y)
{
    WXMICROWIN_CHECK_HDC

    wxCoord x1 = x-VIEWPORT_EXTENT;
    wxCoord y1 = y-VIEWPORT_EXTENT;
    wxCoord x2 = x+VIEWPORT_EXTENT;
    wxCoord y2 = y+VIEWPORT_EXTENT;

    wxDrawLine(GetHdc(), XLOG2DEV(x1), YLOG2DEV(y), XLOG2DEV(x2), YLOG2DEV(y));
    wxDrawLine(GetHdc(), XLOG2DEV(x), YLOG2DEV(y1), XLOG2DEV(x), YLOG2DEV(y2));

    CalcBoundingBox(x1, y1);
    CalcBoundingBox(x2, y2);
}

void wxMSWDCImpl::DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2)
{
    WXMICROWIN_CHECK_HDC

    wxDrawLine(GetHdc(), XLOG2DEV(x1), YLOG2DEV(y1), XLOG2DEV(x2), YLOG2DEV(y2));

    CalcBoundingBox(x1, y1);
    CalcBoundingBox(x2, y2);
}

// Draws an arc of a circle, centred on (xc, yc), with starting point (x1, y1)
// and ending at (x2, y2)
void wxMSWDCImpl::DoDrawArc(wxCoord x1, wxCoord y1,
                     wxCoord x2, wxCoord y2,
                     wxCoord xc, wxCoord yc)
{
    double dx = xc - x1;
    double dy = yc - y1;
    wxCoord r = (wxCoord)sqrt(dx*dx + dy*dy);


#ifdef __WXWINCE__
    // Slower emulation since WinCE doesn't support Pie and Arc
    double sa = acos((x1-xc)/r)/M_PI*180; // between 0 and 180
    if( y1>yc )
        sa = -sa; // below center
    double ea = atan2(yc-y2, x2-xc)/M_PI*180;
    DoDrawEllipticArcRot( xc-r, yc-r, 2*r, 2*r, sa, ea );
#else

    WXMICROWIN_CHECK_HDC

    wxBrushAttrsSetter cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    // treat the special case of full circle separately
    if ( x1 == x2 && y1 == y2 )
    {
        GetOwner()->DrawEllipse(xc - r, yc - r, 2*r, 2*r);
        return;
    }

    wxCoord xx1 = XLOG2DEV(x1);
    wxCoord yy1 = YLOG2DEV(y1);
    wxCoord xx2 = XLOG2DEV(x2);
    wxCoord yy2 = YLOG2DEV(y2);
    wxCoord xxc = XLOG2DEV(xc);
    wxCoord yyc = YLOG2DEV(yc);
    dx = xxc - xx1;
    dy = yyc - yy1;
    wxCoord ray = (wxCoord)sqrt(dx*dx + dy*dy);

    wxCoord xxx1 = (wxCoord) (xxc-ray);
    wxCoord yyy1 = (wxCoord) (yyc-ray);
    wxCoord xxx2 = (wxCoord) (xxc+ray);
    wxCoord yyy2 = (wxCoord) (yyc+ray);

    if ( m_brush.IsNonTransparent() )
    {
        // Have to add 1 to bottom-right corner of rectangle
        // to make semi-circles look right (crooked line otherwise).
        // Unfortunately this is not a reliable method, depends
        // on the size of shape.
        // TODO: figure out why this happens!
        Pie(GetHdc(),xxx1,yyy1,xxx2+1,yyy2+1, xx1,yy1,xx2,yy2);
    }
    else
    {
        Arc(GetHdc(),xxx1,yyy1,xxx2,yyy2, xx1,yy1,xx2,yy2);
    }

    CalcBoundingBox(xc - r, yc - r);
    CalcBoundingBox(xc + r, yc + r);
#endif
}

void wxMSWDCImpl::DoDrawCheckMark(wxCoord x1, wxCoord y1,
                           wxCoord width, wxCoord height)
{
    // cases when we don't have DrawFrameControl()
#if defined(__SYMANTEC__) || defined(__WXMICROWIN__)
    return wxDCBase::DoDrawCheckMark(x1, y1, width, height);
#else // normal case
    wxCoord x2 = x1 + width,
            y2 = y1 + height;

    RECT rect;
    rect.left   = x1;
    rect.top    = y1;
    rect.right  = x2;
    rect.bottom = y2;

#ifdef __WXWINCE__
    DrawFrameControl(GetHdc(), &rect, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_CHECKED);
#else
    DrawFrameControl(GetHdc(), &rect, DFC_MENU, DFCS_MENUCHECK);
#endif

    CalcBoundingBox(x1, y1);
    CalcBoundingBox(x2, y2);
#endif // Microwin/Normal
}

void wxMSWDCImpl::DoDrawPoint(wxCoord x, wxCoord y)
{
    WXMICROWIN_CHECK_HDC

    COLORREF color = 0x00ffffff;
    if (m_pen.IsOk())
    {
        color = m_pen.GetColour().GetPixel();
    }

    SetPixel(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), color);

    CalcBoundingBox(x, y);
}

void wxMSWDCImpl::DoDrawPolygon(int n,
                         const wxPoint points[],
                         wxCoord xoffset,
                         wxCoord yoffset,
                         wxPolygonFillMode WXUNUSED_IN_WINCE(fillStyle))
{
    WXMICROWIN_CHECK_HDC

    wxBrushAttrsSetter cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    // Do things less efficiently if we have offsets
    if (xoffset != 0 || yoffset != 0)
    {
        POINT *cpoints = new POINT[n];
        int i;
        for (i = 0; i < n; i++)
        {
            cpoints[i].x = (int)(points[i].x + xoffset);
            cpoints[i].y = (int)(points[i].y + yoffset);

            CalcBoundingBox(cpoints[i].x, cpoints[i].y);
        }
#ifndef __WXWINCE__
        int prev = SetPolyFillMode(GetHdc(),fillStyle==wxODDEVEN_RULE?ALTERNATE:WINDING);
#endif
        (void)Polygon(GetHdc(), cpoints, n);
#ifndef __WXWINCE__
        SetPolyFillMode(GetHdc(),prev);
#endif
        delete[] cpoints;
    }
    else
    {
        int i;
        for (i = 0; i < n; i++)
            CalcBoundingBox(points[i].x, points[i].y);

#ifndef __WXWINCE__
        int prev = SetPolyFillMode(GetHdc(),fillStyle==wxODDEVEN_RULE?ALTERNATE:WINDING);
#endif
        (void)Polygon(GetHdc(), (POINT*) points, n);
#ifndef __WXWINCE__
        SetPolyFillMode(GetHdc(),prev);
#endif
    }
}

void
wxMSWDCImpl::DoDrawPolyPolygon(int n,
                        const int count[],
                        const wxPoint points[],
                        wxCoord xoffset,
                        wxCoord yoffset,
                        wxPolygonFillMode fillStyle)
{
#ifdef __WXWINCE__
    wxDCImpl::DoDrawPolyPolygon(n, count, points, xoffset, yoffset, fillStyle);
#else
    WXMICROWIN_CHECK_HDC

    wxBrushAttrsSetter cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling
    int i, cnt;
    for (i = cnt = 0; i < n; i++)
        cnt += count[i];

    // Do things less efficiently if we have offsets
    if (xoffset != 0 || yoffset != 0)
    {
        POINT *cpoints = new POINT[cnt];
        for (i = 0; i < cnt; i++)
        {
            cpoints[i].x = (int)(points[i].x + xoffset);
            cpoints[i].y = (int)(points[i].y + yoffset);

            CalcBoundingBox(cpoints[i].x, cpoints[i].y);
        }
#ifndef __WXWINCE__
        int prev = SetPolyFillMode(GetHdc(),fillStyle==wxODDEVEN_RULE?ALTERNATE:WINDING);
#endif
        (void)PolyPolygon(GetHdc(), cpoints, count, n);
#ifndef __WXWINCE__
        SetPolyFillMode(GetHdc(),prev);
#endif
        delete[] cpoints;
    }
    else
    {
        for (i = 0; i < cnt; i++)
            CalcBoundingBox(points[i].x, points[i].y);

#ifndef __WXWINCE__
        int prev = SetPolyFillMode(GetHdc(),fillStyle==wxODDEVEN_RULE?ALTERNATE:WINDING);
#endif
        (void)PolyPolygon(GetHdc(), (POINT*) points, count, n);
#ifndef __WXWINCE__
        SetPolyFillMode(GetHdc(),prev);
#endif
    }
#endif
  // __WXWINCE__
}

void wxMSWDCImpl::DoDrawLines(int n, const wxPoint points[], wxCoord xoffset, wxCoord yoffset)
{
    WXMICROWIN_CHECK_HDC

    // Do things less efficiently if we have offsets
    if (xoffset != 0 || yoffset != 0)
    {
        POINT *cpoints = new POINT[n];
        int i;
        for (i = 0; i < n; i++)
        {
            cpoints[i].x = (int)(points[i].x + xoffset);
            cpoints[i].y = (int)(points[i].y + yoffset);

            CalcBoundingBox(cpoints[i].x, cpoints[i].y);
        }
        (void)Polyline(GetHdc(), cpoints, n);
        delete[] cpoints;
    }
    else
    {
        int i;
        for (i = 0; i < n; i++)
            CalcBoundingBox(points[i].x, points[i].y);

        (void)Polyline(GetHdc(), (POINT*) points, n);
    }
}

void wxMSWDCImpl::DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
    WXMICROWIN_CHECK_HDC

    wxBrushAttrsSetter cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    wxCoord x2 = x + width;
    wxCoord y2 = y + height;

    wxCoord x1dev = XLOG2DEV(x),
            y1dev = YLOG2DEV(y),
            x2dev = XLOG2DEV(x2),
            y2dev = YLOG2DEV(y2);

    // Windows (but not Windows CE) draws the filled rectangles without outline
    // (i.e. drawn with a transparent pen) one pixel smaller in both directions
    // and we want them to have the same size regardless of which pen is used
#ifndef __WXWINCE__
    if ( m_pen.IsTransparent() )
    {
        // Right edge to be extended is "displayed right edge"
        // and hence its device coordinates depend
        // on layout direction and can be either x1 or x2.
        if ( GetLayoutDirection() == wxLayout_RightToLeft )
            x1dev--;
        else
            x2dev++;
        y2dev++;
    }
#endif // !__WXWINCE__

    (void)Rectangle(GetHdc(), x1dev, y1dev, x2dev, y2dev);

    CalcBoundingBox(x, y);
    CalcBoundingBox(x2, y2);
}

void wxMSWDCImpl::DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius)
{
    WXMICROWIN_CHECK_HDC

    wxBrushAttrsSetter cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    // Now, a negative radius value is interpreted to mean
    // 'the proportion of the smallest X or Y dimension'

    if (radius < 0.0)
    {
        double smallest = (width < height) ? width : height;
        radius = (- radius * smallest);
    }

    wxCoord x2 = (x+width);
    wxCoord y2 = (y+height);

    // Windows draws the filled rectangles without outline (i.e. drawn with a
    // transparent pen) one pixel smaller in both directions and we want them
    // to have the same size regardless of which pen is used - adjust
    if ( m_pen.IsTransparent() )
    {
        x2++;
        y2++;
    }

    (void)RoundRect(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), XLOG2DEV(x2),
        YLOG2DEV(y2), (int) (2*XLOG2DEV(radius)), (int)( 2*YLOG2DEV(radius)));

    CalcBoundingBox(x, y);
    CalcBoundingBox(x2, y2);
}

void wxMSWDCImpl::DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
    WXMICROWIN_CHECK_HDC

    wxBrushAttrsSetter cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    // +1 below makes the ellipse more similar to other platforms.
    // In particular, DoDrawEllipse(x,y,1,1) should draw one point.
    wxCoord x2 = x + width + 1;
    wxCoord y2 = y + height + 1;

    // Problem: Windows GDI Ellipse() with x2-x == y2-y == 3 and transparent
    // pen doesn't draw anything. Should we provide a workaround?

    ::Ellipse(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), XLOG2DEV(x2), YLOG2DEV(y2));

    CalcBoundingBox(x, y);
    CalcBoundingBox(x2, y2);
}

#if wxUSE_SPLINES && !defined(__WXWINCE__)
void wxMSWDCImpl::DoDrawSpline(const wxPointList *points)
{
    // quadratic b-spline to cubic bezier spline conversion
    //
    // quadratic spline with control points P0,P1,P2
    // P(s) = P0*(1-s)^2 + P1*2*(1-s)*s + P2*s^2
    //
    // bezier spline with control points B0,B1,B2,B3
    // B(s) = B0*(1-s)^3 + B1*3*(1-s)^2*s + B2*3*(1-s)*s^2 + B3*s^3
    //
    // control points of bezier spline calculated from b-spline
    // B0 = P0
    // B1 = (2*P1 + P0)/3
    // B2 = (2*P1 + P2)/3
    // B3 = P2

    WXMICROWIN_CHECK_HDC

    wxASSERT_MSG( points, wxT("NULL pointer to spline points?") );

    const size_t n_points = points->GetCount();
    wxASSERT_MSG( n_points > 2 , wxT("incomplete list of spline points?") );

    const size_t n_bezier_points = n_points * 3 + 1;
    POINT *lppt = (POINT *)malloc(n_bezier_points*sizeof(POINT));
    size_t bezier_pos = 0;
    wxCoord x1, y1, x2, y2, cx1, cy1, cx4, cy4;

    wxPointList::compatibility_iterator node = points->GetFirst();
    wxPoint *p = node->GetData();
    lppt[ bezier_pos ].x = x1 = p->x;
    lppt[ bezier_pos ].y = y1 = p->y;
    bezier_pos++;
    lppt[ bezier_pos ] = lppt[ bezier_pos-1 ];
    bezier_pos++;

    node = node->GetNext();
    p = node->GetData();

    x2 = p->x;
    y2 = p->y;
    cx1 = ( x1 + x2 ) / 2;
    cy1 = ( y1 + y2 ) / 2;
    lppt[ bezier_pos ].x = XLOG2DEV(cx1);
    lppt[ bezier_pos ].y = YLOG2DEV(cy1);
    bezier_pos++;
    lppt[ bezier_pos ] = lppt[ bezier_pos-1 ];
    bezier_pos++;

#if !wxUSE_STD_CONTAINERS
    while ((node = node->GetNext()) != NULL)
#else
    while ((node = node->GetNext()))
#endif // !wxUSE_STD_CONTAINERS
    {
        p = (wxPoint *)node->GetData();
        x1 = x2;
        y1 = y2;
        x2 = p->x;
        y2 = p->y;
        cx4 = (x1 + x2) / 2;
        cy4 = (y1 + y2) / 2;
        // B0 is B3 of previous segment
        // B1:
        lppt[ bezier_pos ].x = XLOG2DEV((x1*2+cx1)/3);
        lppt[ bezier_pos ].y = YLOG2DEV((y1*2+cy1)/3);
        bezier_pos++;
        // B2:
        lppt[ bezier_pos ].x = XLOG2DEV((x1*2+cx4)/3);
        lppt[ bezier_pos ].y = YLOG2DEV((y1*2+cy4)/3);
        bezier_pos++;
        // B3:
        lppt[ bezier_pos ].x = XLOG2DEV(cx4);
        lppt[ bezier_pos ].y = YLOG2DEV(cy4);
        bezier_pos++;
        cx1 = cx4;
        cy1 = cy4;
    }

    lppt[ bezier_pos ] = lppt[ bezier_pos-1 ];
    bezier_pos++;
    lppt[ bezier_pos ].x = XLOG2DEV(x2);
    lppt[ bezier_pos ].y = YLOG2DEV(y2);
    bezier_pos++;
    lppt[ bezier_pos ] = lppt[ bezier_pos-1 ];
    bezier_pos++;

    ::PolyBezier( GetHdc(), lppt, bezier_pos );

    free(lppt);
}
#endif // wxUSE_SPLINES

// Chris Breeze 20/5/98: first implementation of DrawEllipticArc on Windows
void wxMSWDCImpl::DoDrawEllipticArc(wxCoord x,wxCoord y,wxCoord w,wxCoord h,double sa,double ea)
{
#ifdef __WXWINCE__
    DoDrawEllipticArcRot( x, y, w, h, sa, ea );
#else

    WXMICROWIN_CHECK_HDC

    wxBrushAttrsSetter cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    wxCoord x2 = x + w;
    wxCoord y2 = y + h;

    int rx1 = XLOG2DEV(x+w/2);
    int ry1 = YLOG2DEV(y+h/2);
    int rx2 = rx1;
    int ry2 = ry1;

    sa = DegToRad(sa);
    ea = DegToRad(ea);

    rx1 += (int)(100.0 * abs(w) * cos(sa));
    ry1 -= (int)(100.0 * abs(h) * m_signY * sin(sa));
    rx2 += (int)(100.0 * abs(w) * cos(ea));
    ry2 -= (int)(100.0 * abs(h) * m_signY * sin(ea));

    // Swap start and end positions if the end angle is less than the start angle.
    if (ea < sa) {
        int temp;
        temp = rx2;
        rx2 = rx1;
        rx1 = temp;
        temp = ry2;
        ry2 = ry1;
        ry1 = temp;
    }

    // draw pie with NULL_PEN first and then outline otherwise a line is
    // drawn from the start and end points to the centre
    HPEN hpenOld = (HPEN) ::SelectObject(GetHdc(), (HPEN) ::GetStockObject(NULL_PEN));
    if (m_signY > 0)
    {
        (void)Pie(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), XLOG2DEV(x2)+1, YLOG2DEV(y2)+1,
                  rx1, ry1, rx2, ry2);
    }
    else
    {
        (void)Pie(GetHdc(), XLOG2DEV(x), YLOG2DEV(y)-1, XLOG2DEV(x2)+1, YLOG2DEV(y2),
                  rx1, ry1-1, rx2, ry2-1);
    }

    ::SelectObject(GetHdc(), hpenOld);

    (void)Arc(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), XLOG2DEV(x2), YLOG2DEV(y2),
              rx1, ry1, rx2, ry2);

    CalcBoundingBox(x, y);
    CalcBoundingBox(x2, y2);
#endif
}

void wxMSWDCImpl::DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y)
{
    WXMICROWIN_CHECK_HDC

    wxCHECK_RET( icon.IsOk(), wxT("invalid icon in DrawIcon") );

    // Check if we are printing: notice that it's not enough to just check for
    // DT_RASPRINTER as this is also the kind of print preview HDC, but its
    // type is OBJ_ENHMETADC while the type of the "real" printer DC is OBJ_DC.
    if ( ::GetDeviceCaps(GetHdc(), TECHNOLOGY) == DT_RASPRINTER &&
            ::GetObjectType(GetHdc()) == OBJ_DC )
    {
        // DrawIcon API doesn't work for printer DCs (printer DC is write-only
        // and DrawIcon requires DC supporting SRCINVERT ROP).
        // We need to convert icon to bitmap and use alternative API calls.
        wxBitmap bmp(icon);
        DoDrawBitmap(bmp, x, y, !bmp.HasAlpha() /* use mask */);
    }
    else
    {
#ifdef __WIN32__
        ::DrawIconEx(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), GetHiconOf(icon), icon.GetWidth(), icon.GetHeight(), 0, NULL, DI_NORMAL);
#else
        ::DrawIcon(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), GetHiconOf(icon));
#endif
    }

    CalcBoundingBox(x, y);
    CalcBoundingBox(x + icon.GetWidth(), y + icon.GetHeight());
}

void wxMSWDCImpl::DoDrawBitmap( const wxBitmap &bmp, wxCoord x, wxCoord y, bool useMask )
{
    WXMICROWIN_CHECK_HDC

    wxCHECK_RET( bmp.IsOk(), wxT("invalid bitmap in wxMSWDCImpl::DrawBitmap") );

    int width = bmp.GetWidth(),
        height = bmp.GetHeight();

    HBITMAP hbmpMask = 0;

#if wxUSE_PALETTE
    HPALETTE oldPal = 0;
#endif // wxUSE_PALETTE

    if ( bmp.HasAlpha() )
    {
        MemoryHDC hdcMem;
        SelectInHDC select(hdcMem, GetHbitmapOf(bmp));

        if ( AlphaBlt(GetHdc(), x, y, width, height, 0, 0, width, height, hdcMem, bmp) )
        {
            CalcBoundingBox(x, y);
            CalcBoundingBox(x + bmp.GetWidth(), y + bmp.GetHeight());
            return;
        }
    }

    SET_STRETCH_BLT_MODE(GetHdc());

    if ( useMask )
    {
        wxMask *mask = bmp.GetMask();
        if ( mask )
            hbmpMask = (HBITMAP)mask->GetMaskBitmap();

        if ( !hbmpMask )
        {
            // don't give assert here because this would break existing
            // programs - just silently ignore useMask parameter
            useMask = false;
        }
    }
    if ( useMask )
    {
#ifdef __WIN32__
        // use MaskBlt() with ROP which doesn't do anything to dst in the mask
        // points
        bool ok = false;

#if wxUSE_SYSTEM_OPTIONS
        // On some systems, MaskBlt succeeds yet is much much slower
        // than the wxWidgets fall-back implementation. So we need
        // to be able to switch this on and off at runtime.
        //
        // NB: don't query the value of the option every time but do it only
        //     once as otherwise it can have real (and bad) performance
        //     implications (see #11172)
        static bool
            s_maskBltAllowed = wxSystemOptions::GetOptionInt("no-maskblt") == 0;
        if ( s_maskBltAllowed )
#endif // wxUSE_SYSTEM_OPTIONS
        {
            HDC cdc = GetHdc();
            HDC hdcMem = wxMSWImpl::CreateCompatibleDCWithLayout(cdc);
            HGDIOBJ hOldBitmap = ::SelectObject(hdcMem, GetHbitmapOf(bmp));
#if wxUSE_PALETTE
            wxPalette *pal = bmp.GetPalette();
            if ( pal && ::GetDeviceCaps(cdc,BITSPIXEL) <= 8 )
            {
                oldPal = ::SelectPalette(hdcMem, GetHpaletteOf(*pal), FALSE);
                ::RealizePalette(hdcMem);
            }
#endif // wxUSE_PALETTE

            ok = ::MaskBlt(cdc, x, y, width, height,
                            hdcMem, 0, 0,
                            hbmpMask, 0, 0,
                            MAKEROP4(SRCCOPY, DSTCOPY)) != 0;

#if wxUSE_PALETTE
            if (oldPal)
                ::SelectPalette(hdcMem, oldPal, FALSE);
#endif // wxUSE_PALETTE

            ::SelectObject(hdcMem, hOldBitmap);
            ::DeleteDC(hdcMem);
        }

        if ( !ok )
#endif // Win32
        {
            // Rather than reproduce wxMSWDCImpl::Blit, let's do it at the wxWin API
            // level
            wxMemoryDC memDC;

            memDC.SetLayoutDirection(GetLayoutDirection());
            memDC.SelectObjectAsSource(bmp);

            GetOwner()->Blit(x, y, width, height, &memDC, 0, 0, wxCOPY, useMask);
        }
    }
    else // no mask, just use BitBlt()
    {
        HDC cdc = GetHdc();
        HDC memdc = wxMSWImpl::CreateCompatibleDCWithLayout( cdc );
        HBITMAP hbitmap = (HBITMAP) bmp.GetHBITMAP( );

        wxASSERT_MSG( hbitmap, wxT("bitmap is ok but HBITMAP is NULL?") );

        wxTextColoursChanger textCol(GetHdc(), *this);

#if wxUSE_PALETTE
        wxPalette *pal = bmp.GetPalette();
        if ( pal && ::GetDeviceCaps(cdc,BITSPIXEL) <= 8 )
        {
            oldPal = ::SelectPalette(memdc, GetHpaletteOf(*pal), FALSE);
            ::RealizePalette(memdc);
        }
#endif // wxUSE_PALETTE

        HGDIOBJ hOldBitmap = ::SelectObject( memdc, hbitmap );
        ::BitBlt( cdc, x, y, width, height, memdc, 0, 0, SRCCOPY);

#if wxUSE_PALETTE
        if (oldPal)
            ::SelectPalette(memdc, oldPal, FALSE);
#endif // wxUSE_PALETTE

        ::SelectObject( memdc, hOldBitmap );
        ::DeleteDC( memdc );
    }

    CalcBoundingBox(x, y);
    CalcBoundingBox(x + bmp.GetWidth(), y + bmp.GetHeight());
}

void wxMSWDCImpl::DoDrawText(const wxString& text, wxCoord x, wxCoord y)
{
    // For compatibility with other ports (notably wxGTK) and because it's
    // genuinely useful, we allow passing multiline strings to DrawText().
    // However there is no native MSW function to draw them directly so we
    // instead reuse the generic DrawLabel() method to render them. Of course,
    // DrawLabel() itself will call back to us but with single line strings
    // only so there won't be any infinite recursion here.
    if ( text.find('\n') != wxString::npos )
    {
        GetOwner()->DrawLabel(text, wxRect(x, y, 0, 0));
        return;
    }

    WXMICROWIN_CHECK_HDC

    DrawAnyText(text, x, y);

    // update the bounding box
    CalcBoundingBox(x, y);

    wxCoord w, h;
    GetOwner()->GetTextExtent(text, &w, &h);
    CalcBoundingBox(x + w, y + h);
}

void wxMSWDCImpl::DrawAnyText(const wxString& text, wxCoord x, wxCoord y)
{
    WXMICROWIN_CHECK_HDC

    // prepare for drawing the text
    wxTextColoursChanger textCol(GetHdc(), *this);

    wxBkModeChanger bkMode(GetHdc(), m_backgroundMode);

    if ( ::ExtTextOut(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), 0, NULL,
                   text.c_str(), text.length(), NULL) == 0 )
    {
        wxLogLastError(wxT("TextOut"));
    }
}

void wxMSWDCImpl::DoDrawRotatedText(const wxString& text,
                             wxCoord x, wxCoord y,
                             double angle)
{
    WXMICROWIN_CHECK_HDC

    // we test that we have some font because otherwise we should still use the
    // "else" part below to avoid that DrawRotatedText(angle = 180) and
    // DrawRotatedText(angle = 0) use different fonts (we can't use the default
    // font for drawing rotated fonts unfortunately)
    if ( (angle == 0.0) && m_font.IsOk() )
    {
        DoDrawText(text, x, y);
    }
#ifndef __WXMICROWIN__
    else
    {
        // NB: don't take DEFAULT_GUI_FONT (a.k.a. wxSYS_DEFAULT_GUI_FONT)
        //     because it's not TrueType and so can't have non zero
        //     orientation/escapement under Win9x
        wxFont font = m_font.IsOk() ? m_font : *wxSWISS_FONT;
        HFONT hfont = (HFONT)font.GetResourceHandle();
        LOGFONT lf;
        if ( ::GetObject(hfont, sizeof(lf), &lf) == 0 )
        {
            wxLogLastError(wxT("GetObject(hfont)"));
        }

        // GDI wants the angle in tenth of degree
        long angle10 = (long)(angle * 10);
        lf.lfEscapement = angle10;
        lf. lfOrientation = angle10;

        hfont = ::CreateFontIndirect(&lf);
        if ( !hfont )
        {
            wxLogLastError(wxT("CreateFont"));
        }
        else
        {
            HFONT hfontOld = (HFONT)::SelectObject(GetHdc(), hfont);

            DrawAnyText(text, x, y);

            (void)::SelectObject(GetHdc(), hfontOld);
            (void)::DeleteObject(hfont);
        }

        // call the bounding box by adding all four vertices of the rectangle
        // containing the text to it (simpler and probably not slower than
        // determining which of them is really topmost/leftmost/...)
        wxCoord w, h;
        GetOwner()->GetTextExtent(text, &w, &h);

        double rad = DegToRad(angle);

        // "upper left" and "upper right"
        CalcBoundingBox(x, y);
        CalcBoundingBox(x + wxCoord(w*cos(rad)), y - wxCoord(w*sin(rad)));

        // "bottom left" and "bottom right"
        x += (wxCoord)(h*sin(rad));
        y += (wxCoord)(h*cos(rad));
        CalcBoundingBox(x, y);
        CalcBoundingBox(x + wxCoord(w*cos(rad)), y - wxCoord(w*sin(rad)));
    }
#endif
}

// ---------------------------------------------------------------------------
// set GDI objects
// ---------------------------------------------------------------------------

#if wxUSE_PALETTE

void wxMSWDCImpl::DoSelectPalette(bool realize)
{
    WXMICROWIN_CHECK_HDC

    // Set the old object temporarily, in case the assignment deletes an object
    // that's not yet selected out.
    if (m_oldPalette)
    {
        ::SelectPalette(GetHdc(), (HPALETTE) m_oldPalette, FALSE);
        m_oldPalette = 0;
    }

    if ( m_palette.IsOk() )
    {
        HPALETTE oldPal = ::SelectPalette(GetHdc(),
                                          GetHpaletteOf(m_palette),
                                          false);
        if (!m_oldPalette)
            m_oldPalette = (WXHPALETTE) oldPal;

        if (realize)
            ::RealizePalette(GetHdc());
    }
}

void wxMSWDCImpl::SetPalette(const wxPalette& palette)
{
    if ( palette.IsOk() )
    {
        m_palette = palette;
        DoSelectPalette(true);
    }
}

void wxMSWDCImpl::InitializePalette()
{
    if ( wxDisplayDepth() <= 8 )
    {
        // look for any window or parent that has a custom palette. If any has
        // one then we need to use it in drawing operations
        wxWindow *win = m_window->GetAncestorWithCustomPalette();

        m_hasCustomPalette = win && win->HasCustomPalette();
        if ( m_hasCustomPalette )
        {
            m_palette = win->GetPalette();

            // turn on MSW translation for this palette
            DoSelectPalette();
        }
    }
}

#endif // wxUSE_PALETTE

// SetFont/Pen/Brush() really ask to be implemented as a single template
// function... but doing it is not worth breaking OpenWatcom build <sigh>

void wxMSWDCImpl::SetFont(const wxFont& font)
{
    WXMICROWIN_CHECK_HDC

    if ( font == m_font )
        return;

    if ( font.IsOk() )
    {
        HGDIOBJ hfont = ::SelectObject(GetHdc(), GetHfontOf(font));
        if ( hfont == HGDI_ERROR )
        {
            wxLogLastError(wxT("SelectObject(font)"));
        }
        else // selected ok
        {
            if ( !m_oldFont )
                m_oldFont = (WXHFONT)hfont;

            m_font = font;
        }
    }
    else // invalid font, reset the current font
    {
        if ( m_oldFont )
        {
            if ( ::SelectObject(GetHdc(), (HPEN) m_oldFont) == HGDI_ERROR )
            {
                wxLogLastError(wxT("SelectObject(old font)"));
            }

            m_oldFont = 0;
        }

        m_font = wxNullFont;
    }
}

void wxMSWDCImpl::SetPen(const wxPen& pen)
{
    WXMICROWIN_CHECK_HDC

    if ( pen == m_pen )
        return;

    if ( pen.IsOk() )
    {
        HGDIOBJ hpen = ::SelectObject(GetHdc(), GetHpenOf(pen));
        if ( hpen == HGDI_ERROR )
        {
            wxLogLastError(wxT("SelectObject(pen)"));
        }
        else // selected ok
        {
            if ( !m_oldPen )
                m_oldPen = (WXHPEN)hpen;

            m_pen = pen;
        }
    }
    else // invalid pen, reset the current pen
    {
        if ( m_oldPen )
        {
            if ( ::SelectObject(GetHdc(), (HPEN) m_oldPen) == HGDI_ERROR )
            {
                wxLogLastError(wxT("SelectObject(old pen)"));
            }

            m_oldPen = 0;
        }

        m_pen = wxNullPen;
    }
}

void wxMSWDCImpl::SetBrush(const wxBrush& brush)
{
    WXMICROWIN_CHECK_HDC

    if ( brush == m_brush )
        return;

    if ( brush.IsOk() )
    {
        // we must make sure the brush is aligned with the logical coordinates
        // before selecting it or using the same brush for the background of
        // different windows would result in discontinuities
        wxSize sizeBrushBitmap = wxDefaultSize;
        wxBitmap *stipple = brush.GetStipple();
        if ( stipple && stipple->IsOk() )
            sizeBrushBitmap = stipple->GetSize();
        else if ( brush.IsHatch() )
            sizeBrushBitmap = wxSize(8, 8);

        if ( sizeBrushBitmap.IsFullySpecified() )
        {
            if ( !::SetBrushOrgEx
                    (
                        GetHdc(),
                        m_deviceOriginX % sizeBrushBitmap.x,
                        m_deviceOriginY % sizeBrushBitmap.y,
                        NULL                    // [out] previous brush origin
                    ) )
            {
                wxLogLastError(wxT("SetBrushOrgEx()"));
            }
        }

        HGDIOBJ hbrush = ::SelectObject(GetHdc(), GetHbrushOf(brush));
        if ( hbrush == HGDI_ERROR )
        {
            wxLogLastError(wxT("SelectObject(brush)"));
        }
        else // selected ok
        {
            if ( !m_oldBrush )
                m_oldBrush = (WXHBRUSH)hbrush;

            m_brush = brush;
        }
    }
    else // invalid brush, reset the current brush
    {
        if ( m_oldBrush )
        {
            if ( ::SelectObject(GetHdc(), (HPEN) m_oldBrush) == HGDI_ERROR )
            {
                wxLogLastError(wxT("SelectObject(old brush)"));
            }

            m_oldBrush = 0;
        }

        m_brush = wxNullBrush;
    }
}

void wxMSWDCImpl::SetBackground(const wxBrush& brush)
{
    WXMICROWIN_CHECK_HDC

    m_backgroundBrush = brush;

    if ( m_backgroundBrush.IsOk() )
    {
        (void)SetBkColor(GetHdc(), m_backgroundBrush.GetColour().GetPixel());
    }
}

void wxMSWDCImpl::SetBackgroundMode(int mode)
{
    WXMICROWIN_CHECK_HDC

    m_backgroundMode = mode;

    // SetBackgroundColour now only refers to text background
    // and m_backgroundMode is used there
}

void wxMSWDCImpl::SetLogicalFunction(wxRasterOperationMode function)
{
    WXMICROWIN_CHECK_HDC

    m_logicalFunction = function;

    SetRop(m_hDC);
}

void wxMSWDCImpl::SetRop(WXHDC dc)
{
    if ( !dc || m_logicalFunction < 0 )
        return;

    int rop;

    switch (m_logicalFunction)
    {
        case wxCLEAR:        rop = R2_BLACK;         break;
        case wxXOR:          rop = R2_XORPEN;        break;
        case wxINVERT:       rop = R2_NOT;           break;
        case wxOR_REVERSE:   rop = R2_MERGEPENNOT;   break;
        case wxAND_REVERSE:  rop = R2_MASKPENNOT;    break;
        case wxCOPY:         rop = R2_COPYPEN;       break;
        case wxAND:          rop = R2_MASKPEN;       break;
        case wxAND_INVERT:   rop = R2_MASKNOTPEN;    break;
        case wxNO_OP:        rop = R2_NOP;           break;
        case wxNOR:          rop = R2_NOTMERGEPEN;   break;
        case wxEQUIV:        rop = R2_NOTXORPEN;     break;
        case wxSRC_INVERT:   rop = R2_NOTCOPYPEN;    break;
        case wxOR_INVERT:    rop = R2_MERGENOTPEN;   break;
        case wxNAND:         rop = R2_NOTMASKPEN;    break;
        case wxOR:           rop = R2_MERGEPEN;      break;
        case wxSET:          rop = R2_WHITE;         break;
        default:
            wxFAIL_MSG( wxS("unknown logical function") );
            return;
    }

    SetROP2(GetHdc(), rop);
}

bool wxMSWDCImpl::StartDoc(const wxString& WXUNUSED(message))
{
    // We might be previewing, so return true to let it continue.
    return true;
}

void wxMSWDCImpl::EndDoc()
{
}

void wxMSWDCImpl::StartPage()
{
}

void wxMSWDCImpl::EndPage()
{
}

// ---------------------------------------------------------------------------
// text metrics
// ---------------------------------------------------------------------------

wxCoord wxMSWDCImpl::GetCharHeight() const
{
    WXMICROWIN_CHECK_HDC_RET(0)

    TEXTMETRIC lpTextMetric;

    GetTextMetrics(GetHdc(), &lpTextMetric);

    return lpTextMetric.tmHeight;
}

wxCoord wxMSWDCImpl::GetCharWidth() const
{
    WXMICROWIN_CHECK_HDC_RET(0)

    TEXTMETRIC lpTextMetric;

    GetTextMetrics(GetHdc(), &lpTextMetric);

    return lpTextMetric.tmAveCharWidth;
}

void wxMSWDCImpl::DoGetFontMetrics(int *height,
                                   int *ascent,
                                   int *descent,
                                   int *internalLeading,
                                   int *externalLeading,
                                   int *averageWidth) const
{
    TEXTMETRIC tm;

    GetTextMetrics(GetHdc(), &tm);

    if ( height )
        *height = tm.tmHeight;
    if ( ascent )
        *ascent = tm.tmAscent;
    if ( descent )
        *descent = tm.tmDescent;
    if ( internalLeading )
        *internalLeading = tm.tmInternalLeading;
    if ( externalLeading )
        *externalLeading = tm.tmExternalLeading;
    if ( averageWidth )
        *averageWidth = tm.tmAveCharWidth;
}

void wxMSWDCImpl::DoGetTextExtent(const wxString& string, wxCoord *x, wxCoord *y,
                           wxCoord *descent, wxCoord *externalLeading,
                           const wxFont *font) const
{
#ifdef __WXMICROWIN__
    if (!GetHDC())
    {
        if (x) *x = 0;
        if (y) *y = 0;
        if (descent) *descent = 0;
        if (externalLeading) *externalLeading = 0;
        return;
    }
#endif // __WXMICROWIN__

    wxASSERT_MSG( !font || font->IsOk(), wxT("invalid font in wxMSWDCImpl::GetTextExtent") );

    wxTextMeasure txm(GetOwner(), font);
    txm.GetTextExtent(string, x, y, descent, externalLeading);
}


bool wxMSWDCImpl::DoGetPartialTextExtents(const wxString& text, wxArrayInt& widths) const
{
    wxTextMeasure txm(GetOwner(), NULL); // don't change the font
    return txm.GetPartialTextExtents(text, widths, 1.0);
}

namespace
{

void ApplyEffectiveScale(double scale, int sign, int *device, int *logical)
{
    // To reduce rounding errors as much as possible, we try to use the largest
    // possible extent (2^27-1) for the device space but we must also avoid
    // overflowing the int range i.e. ensure that logical extents are less than
    // 2^31 in magnitude. So the minimal scale we can use is 1/16 as for
    // anything smaller VIEWPORT_EXTENT/scale would overflow the int range.
    static const double MIN_LOGICAL_SCALE = 1./16;

    double physExtent = VIEWPORT_EXTENT;
    if ( scale < MIN_LOGICAL_SCALE )
    {
        physExtent *= scale/MIN_LOGICAL_SCALE;
        scale = MIN_LOGICAL_SCALE;
    }

    *device = wxRound(physExtent);
    *logical = sign*wxRound(VIEWPORT_EXTENT/scale);
}

// Binary GCD algorithm
// See: http://en.wikipedia.org/wiki/Binary_GCD_algorithm#Iterative_version_in_C
unsigned int CalcGCD(unsigned int u, unsigned int v)
{
    // GCD(0,v) == v; GCD(u,0) == u, GCD(0,0) == 0
    if (u == 0)
        return v;
    if (v == 0)
        return u;

    int shift;

    // Let shift := lg K, where K is the greatest power of 2
    // dividing both u and v.
    for (shift = 0; ((u | v) & 1) == 0; ++shift)
    {
        u >>= 1;
        v >>= 1;
    }

    while ((u & 1) == 0)
        u >>= 1;

    // From here on, u is always odd.
    do
    {
        // remove all factors of 2 in v -- they are not common
        // note: v is not zero, so while will terminate
        while ((v & 1) == 0)
            v >>= 1;

        // Now u and v are both odd. Swap if necessary so u <= v,
        // then set v = v - u (which is even)
        if (u > v)
        {
            wxSwap(u, v);
        }
        v -= u;  // Here v >= u
    } while (v != 0);

    // restore common factors of 2
    return u << shift;
}

} // anonymous namespace

void wxMSWDCImpl::RealizeScaleAndOrigin()
{
    // although it may seem wasteful to always use MM_ANISOTROPIC here instead
    // of using MM_TEXT if there is no scaling, benchmarking doesn't detect any
    // noticeable difference between these mapping modes
#ifndef __WXWINCE__
    ::SetMapMode(GetHdc(), MM_ANISOTROPIC);

    // wxWidgets API assumes that the coordinate space is "infinite" (i.e. only
    // limited by 2^32 range of the integer coordinates) but in MSW API we must
    // actually specify the extents that we use so compute them here.

    int devExtX, devExtY,   // Viewport, i.e. device space, extents.
        logExtX, logExtY;   // Window, i.e. logical coordinate space, extents.

    ApplyEffectiveScale(m_scaleX, m_signX, &devExtX, &logExtX);
    ApplyEffectiveScale(m_scaleY, m_signY, &devExtY, &logExtY);

    // Becaue only devExtX/logExtX ratio and devExtY/logExtY ratio are counted
    // we can reduce the fractions to avoid large absolute numbers
    // and possible arithmetic overflows.
    int gcd = CalcGCD(abs(devExtX), abs(logExtX));
    devExtX /= gcd;
    logExtX /= gcd;
    gcd = CalcGCD(abs(devExtY), abs(logExtY));
    devExtY /= gcd;
    logExtY /= gcd;

    ::SetViewportExtEx(GetHdc(), devExtX, devExtY, NULL);
    ::SetWindowExtEx(GetHdc(), logExtX, logExtY, NULL);

    ::SetViewportOrgEx(GetHdc(), m_deviceOriginX, m_deviceOriginY, NULL);
    ::SetWindowOrgEx(GetHdc(), m_logicalOriginX, m_logicalOriginY, NULL);
#endif
}

void wxMSWDCImpl::SetMapMode(wxMappingMode mode)
{
    WXMICROWIN_CHECK_HDC

    m_mappingMode = mode;

    if ( mode == wxMM_TEXT )
    {
        m_logicalScaleX =
        m_logicalScaleY = 1.0;
    }
    else // need to do some calculations
    {
        int pixel_width = ::GetDeviceCaps(GetHdc(), HORZRES),
            pixel_height = ::GetDeviceCaps(GetHdc(), VERTRES),
            mm_width = ::GetDeviceCaps(GetHdc(), HORZSIZE),
            mm_height = ::GetDeviceCaps(GetHdc(), VERTSIZE);

        if ( (mm_width == 0) || (mm_height == 0) )
        {
            // we can't calculate mm2pixels[XY] then!
            return;
        }

        double mm2pixelsX = (double)pixel_width / mm_width,
               mm2pixelsY = (double)pixel_height / mm_height;

        switch (mode)
        {
            case wxMM_TWIPS:
                m_logicalScaleX = twips2mm * mm2pixelsX;
                m_logicalScaleY = twips2mm * mm2pixelsY;
                break;

            case wxMM_POINTS:
                m_logicalScaleX = pt2mm * mm2pixelsX;
                m_logicalScaleY = pt2mm * mm2pixelsY;
                break;

            case wxMM_METRIC:
                m_logicalScaleX = mm2pixelsX;
                m_logicalScaleY = mm2pixelsY;
                break;

            case wxMM_LOMETRIC:
                m_logicalScaleX = mm2pixelsX / 10.0;
                m_logicalScaleY = mm2pixelsY / 10.0;
                break;

            default:
                wxFAIL_MSG( wxT("unknown mapping mode in SetMapMode") );
        }
    }

    ComputeScaleAndOrigin();

    RealizeScaleAndOrigin();
}

void wxMSWDCImpl::SetUserScale(double x, double y)
{
    WXMICROWIN_CHECK_HDC

    if ( x == m_userScaleX && y == m_userScaleY )
        return;

    wxDCImpl::SetUserScale(x,y);

    RealizeScaleAndOrigin();
}

void wxMSWDCImpl::SetAxisOrientation(bool xLeftRight,
                              bool yBottomUp)
{
    WXMICROWIN_CHECK_HDC

    int signX = xLeftRight ? 1 : -1,
        signY = yBottomUp ? -1 : 1;

    if (signX == m_signX && signY == m_signY)
        return;

    wxDCImpl::SetAxisOrientation( xLeftRight, yBottomUp );

    RealizeScaleAndOrigin();
}

void wxMSWDCImpl::SetLogicalOrigin(wxCoord x, wxCoord y)
{
    WXMICROWIN_CHECK_HDC

    if ( x == m_logicalOriginX && y == m_logicalOriginY )
        return;

    wxDCImpl::SetLogicalOrigin( x, y );

    RealizeScaleAndOrigin();
}

// For use by wxWidgets only, unless custom units are required.
void wxMSWDCImpl::SetLogicalScale(double x, double y)
{
    WXMICROWIN_CHECK_HDC

    wxDCImpl::SetLogicalScale(x,y);

    RealizeScaleAndOrigin();
}

void wxMSWDCImpl::SetDeviceOrigin(wxCoord x, wxCoord y)
{
    WXMICROWIN_CHECK_HDC

    if ( x == m_deviceOriginX && y == m_deviceOriginY )
        return;

    wxDCImpl::SetDeviceOrigin( x, y );

    ::SetViewportOrgEx(GetHdc(), (int)m_deviceOriginX, (int)m_deviceOriginY, NULL);
}

// ----------------------------------------------------------------------------
// Transform matrix
// ----------------------------------------------------------------------------

#if wxUSE_DC_TRANSFORM_MATRIX

bool wxMSWDCImpl::CanUseTransformMatrix() const
{
    return GdiWorldTransformFuncs::IsOk();
}

bool wxMSWDCImpl::SetTransformMatrix(const wxAffineMatrix2D &matrix)
{
    if ( !GdiWorldTransformFuncs::IsOk() )
        return false;

    if ( matrix.IsIdentity() )
    {
        ResetTransformMatrix();
        return true;
    }

    if ( !GdiWorldTransformFuncs::SetGraphicsMode()(GetHdc(), GM_ADVANCED) )
    {
        wxLogLastError(wxT("SetGraphicsMode"));
        return false;
    }

    wxMatrix2D mat;
    wxPoint2DDouble tr;
    matrix.Get(&mat, &tr);

    XFORM xform;
    xform.eM11 = mat.m_11;
    xform.eM12 = mat.m_12;
    xform.eM21 = mat.m_21;
    xform.eM22 = mat.m_22;
    xform.eDx = tr.m_x;
    xform.eDy = tr.m_y;

    if ( !GdiWorldTransformFuncs::SetWorldTransform()(GetHdc(), &xform) )
    {
        wxLogLastError(wxT("SetWorldTransform"));
        return false;
    }

    return true;
}

wxAffineMatrix2D wxMSWDCImpl::GetTransformMatrix() const
{
    wxAffineMatrix2D transform;

    if ( !GdiWorldTransformFuncs::IsOk() )
        return transform;

    XFORM xform;
    if ( !GdiWorldTransformFuncs::GetWorldTransform()(GetHdc(), &xform) )
    {
        wxLogLastError(wxT("GetWorldTransform"));
        return transform;
    }

    wxMatrix2D m(xform.eM11, xform.eM12, xform.eM21, xform.eM22);
    wxPoint2DDouble p(xform.eDx, xform.eDy);
    transform.Set(m, p);

    return transform;
}

void wxMSWDCImpl::ResetTransformMatrix()
{
    if ( GdiWorldTransformFuncs::IsOk() )
    {
        GdiWorldTransformFuncs::ModifyWorldTransform()(GetHdc(), NULL, MWT_IDENTITY);
        GdiWorldTransformFuncs::SetGraphicsMode()(GetHdc(), GM_COMPATIBLE);
    }
}

#endif // wxUSE_DC_TRANSFORM_MATRIX

// ---------------------------------------------------------------------------
// bit blit
// ---------------------------------------------------------------------------

bool wxMSWDCImpl::DoBlit(wxCoord dstX, wxCoord dstY,
                  wxCoord dstWidth, wxCoord dstHeight,
                  wxDC *source,
                  wxCoord srcX, wxCoord srcY,
                  wxRasterOperationMode rop, bool useMask,
                  wxCoord srcMaskX, wxCoord srcMaskY)
{
    return DoStretchBlit(dstX, dstY, dstWidth, dstHeight, source, srcX, srcY, dstWidth, dstHeight, rop, useMask, srcMaskX, srcMaskY);
}

bool wxMSWDCImpl::DoStretchBlit(wxCoord xdest, wxCoord ydest,
                         wxCoord dstWidth, wxCoord dstHeight,
                         wxDC *source,
                         wxCoord xsrc, wxCoord ysrc,
                         wxCoord srcWidth, wxCoord srcHeight,
                         wxRasterOperationMode rop, bool useMask,
                         wxCoord xsrcMask, wxCoord ysrcMask)
{
    wxCHECK_MSG( source, false, wxT("wxMSWDCImpl::Blit(): NULL wxDC pointer") );

    WXMICROWIN_CHECK_HDC_RET(false)

    wxMSWDCImpl *implSrc = wxDynamicCast( source->GetImpl(), wxMSWDCImpl );
    if ( !implSrc )
    {
        // TODO: Do we want to be able to blit from other DCs too?
        return false;
    }

    HDC hdcSrc = GetHdcOf(*implSrc);

    // if either the source or destination has alpha channel, we must use
    // AlphaBlt() as other function don't handle it correctly
    const wxBitmap& bmpSrc = implSrc->GetSelectedBitmap();
    if ( bmpSrc.IsOk() && (bmpSrc.HasAlpha() ||
            (m_selectedBitmap.IsOk() && m_selectedBitmap.HasAlpha())) )
    {
        if ( AlphaBlt(GetHdc(), xdest, ydest, dstWidth, dstHeight,
                      xsrc, ysrc, srcWidth, srcHeight, hdcSrc, bmpSrc) )
        {
            CalcBoundingBox(xdest, ydest);
            CalcBoundingBox(xdest + dstWidth, ydest + dstHeight);
            return true;
        }
    }

    wxMask *mask = NULL;
    if ( useMask )
    {
        mask = bmpSrc.GetMask();

        if ( !(bmpSrc.IsOk() && mask && mask->GetMaskBitmap()) )
        {
            // don't give assert here because this would break existing
            // programs - just silently ignore useMask parameter
            useMask = false;
        }
    }

    if (xsrcMask == -1 && ysrcMask == -1)
    {
        xsrcMask = xsrc; ysrcMask = ysrc;
    }

    wxTextColoursChanger textCol(GetHdc(), *this);

    DWORD dwRop;
    switch (rop)
    {
        case wxXOR:          dwRop = SRCINVERT;        break;
        case wxINVERT:       dwRop = DSTINVERT;        break;
        case wxOR_REVERSE:   dwRop = 0x00DD0228;       break;
        case wxAND_REVERSE:  dwRop = SRCERASE;         break;
        case wxCLEAR:        dwRop = BLACKNESS;        break;
        case wxSET:          dwRop = WHITENESS;        break;
        case wxOR_INVERT:    dwRop = MERGEPAINT;       break;
        case wxAND:          dwRop = SRCAND;           break;
        case wxOR:           dwRop = SRCPAINT;         break;
        case wxEQUIV:        dwRop = 0x00990066;       break;
        case wxNAND:         dwRop = 0x007700E6;       break;
        case wxAND_INVERT:   dwRop = 0x00220326;       break;
        case wxCOPY:         dwRop = SRCCOPY;          break;
        case wxNO_OP:        dwRop = DSTCOPY;          break;
        case wxSRC_INVERT:   dwRop = NOTSRCCOPY;       break;
        case wxNOR:          dwRop = NOTSRCCOPY;       break;
        default:
           wxFAIL_MSG( wxT("unsupported logical function") );
           return false;
    }

    // Most of the operations involve the source or the pattern, but a few of
    // them (and only those few, no other are possible) only use destination
    // HDC. For them we must not give a valid source HDC to MaskBlt() as it
    // still uses it, somehow, and the result is garbage.
    if ( dwRop == BLACKNESS || dwRop == WHITENESS ||
            dwRop == DSTINVERT || dwRop == DSTCOPY )
    {
        hdcSrc = NULL;
    }

    bool success = false;

    if (useMask)
    {
#ifdef __WIN32__
        // we want the part of the image corresponding to the mask to be
        // transparent, so use "DSTCOPY" ROP for the mask points (the usual
        // meaning of fg and bg is inverted which corresponds to wxWin notion
        // of the mask which is also contrary to the Windows one)

        // On some systems, MaskBlt succeeds yet is much much slower
        // than the wxWidgets fall-back implementation. So we need
        // to be able to switch this on and off at runtime.
#if wxUSE_SYSTEM_OPTIONS
        static bool s_maskBltAllowed = wxSystemOptions::GetOptionInt("no-maskblt") == 0;
        if ( s_maskBltAllowed )
#endif
        {
            if ( dstWidth == srcWidth && dstHeight == srcHeight )
            {
                success = ::MaskBlt
                            (
                            GetHdc(),
                            xdest, ydest, dstWidth, dstHeight,
                            hdcSrc,
                            xsrc, ysrc,
                            (HBITMAP)mask->GetMaskBitmap(),
                            xsrcMask, ysrcMask,
                            MAKEROP4(dwRop, DSTCOPY)
                            ) != 0;
            }
        }

        if ( !success )
#endif // Win32
        {
            // Blit bitmap with mask
            HDC dc_mask ;
            HDC  dc_buffer ;
            HBITMAP buffer_bmap ;

#if wxUSE_DC_CACHEING
            // create a temp buffer bitmap and DCs to access it and the mask
            wxDCCacheEntry* dcCacheEntry1 = FindDCInCache(NULL, hdcSrc);
            dc_mask = (HDC) dcCacheEntry1->m_dc;

            wxDCCacheEntry* dcCacheEntry2 = FindDCInCache(dcCacheEntry1, GetHDC());
            dc_buffer = (HDC) dcCacheEntry2->m_dc;

            wxDCCacheEntry* bitmapCacheEntry = FindBitmapInCache(GetHDC(),
                dstWidth, dstHeight);

            buffer_bmap = (HBITMAP) bitmapCacheEntry->m_bitmap;
#else // !wxUSE_DC_CACHEING
            // create a temp buffer bitmap and DCs to access it and the mask
            dc_mask = wxMSWImpl::CreateCompatibleDCWithLayout(hdcSrc);
            dc_buffer = wxMSWImpl::CreateCompatibleDCWithLayout(GetHdc());
            buffer_bmap = ::CreateCompatibleBitmap(GetHdc(), dstWidth, dstHeight);
#endif // wxUSE_DC_CACHEING/!wxUSE_DC_CACHEING
            HGDIOBJ hOldMaskBitmap = ::SelectObject(dc_mask, (HBITMAP) mask->GetMaskBitmap());
            HGDIOBJ hOldBufferBitmap = ::SelectObject(dc_buffer, buffer_bmap);

            // copy dest to buffer
            if ( !::BitBlt(dc_buffer, 0, 0, dstWidth, dstHeight,
                           GetHdc(), xdest, ydest, SRCCOPY) )
            {
                wxLogLastError(wxT("BitBlt"));
            }

            SET_STRETCH_BLT_MODE(GetHdc());

            // copy src to buffer using selected raster op
            if ( !::StretchBlt(dc_buffer, 0, 0, dstWidth, dstHeight,
                               hdcSrc, xsrc, ysrc, srcWidth, srcHeight, dwRop) )
            {
                wxLogLastError(wxT("StretchBlt"));
            }

            // set masked area in buffer to BLACK
            {
                wxTextColoursChanger textCol2(GetHdc(), *wxBLACK, *wxWHITE);
                if ( !::StretchBlt(dc_buffer, 0, 0, dstWidth, dstHeight,
                                   dc_mask, xsrcMask, ysrcMask,
                                   srcWidth, srcHeight, SRCAND) )
                {
                    wxLogLastError(wxT("StretchBlt"));
                }

                // set unmasked area in dest to BLACK
                ::SetBkColor(GetHdc(), RGB(0, 0, 0));
                ::SetTextColor(GetHdc(), RGB(255, 255, 255));
                if ( !::StretchBlt(GetHdc(), xdest, ydest, dstWidth, dstHeight,
                                   dc_mask, xsrcMask, ysrcMask,
                                   srcWidth, srcHeight, SRCAND) )
                {
                    wxLogLastError(wxT("StretchBlt"));
                }
            } // restore the original text and background colours

            // OR buffer to dest
            success = ::BitBlt(GetHdc(), xdest, ydest, dstWidth, dstHeight,
                               dc_buffer, 0, 0, SRCPAINT) != 0;
            if ( !success )
            {
                wxLogLastError(wxT("BitBlt"));
            }

            // tidy up temporary DCs and bitmap
            ::SelectObject(dc_mask, hOldMaskBitmap);
            ::SelectObject(dc_buffer, hOldBufferBitmap);

#if !wxUSE_DC_CACHEING
            {
                ::DeleteDC(dc_mask);
                ::DeleteDC(dc_buffer);
                ::DeleteObject(buffer_bmap);
            }
#endif
        }
    }
    else // no mask, just BitBlt() it
    {
        // if we already have a DIB, draw it using StretchDIBits(), otherwise
        // use StretchBlt() if available and finally fall back to BitBlt()

        // FIXME: use appropriate WinCE functions
#ifndef __WXWINCE__
        const int caps = ::GetDeviceCaps(GetHdc(), RASTERCAPS);
        if ( bmpSrc.IsOk() && (caps & RC_STRETCHDIB) )
        {
            DIBSECTION ds;
            wxZeroMemory(ds);

            if ( ::GetObject(GetHbitmapOf(bmpSrc),
                             sizeof(ds),
                             &ds) == sizeof(ds) )
            {
                SET_STRETCH_BLT_MODE(GetHdc());

                // Unlike all the other functions used here (i.e. AlphaBlt(),
                // MaskBlt(), BitBlt() and StretchBlt()), StretchDIBits() does
                // not take into account the source DC logical coordinates
                // automatically as it doesn't even work with the source HDC.
                // So do this manually to ensure that the coordinates are
                // interpreted in the same way here as in all the other cases.
                xsrc = source->LogicalToDeviceX(xsrc);
                ysrc = source->LogicalToDeviceY(ysrc);
                srcWidth = source->LogicalToDeviceXRel(srcWidth);
                srcHeight = source->LogicalToDeviceYRel(srcHeight);

                // Figure out what co-ordinate system we're supposed to specify
                // ysrc in.
                const LONG hDIB = ds.dsBmih.biHeight;
                if ( hDIB > 0 )
                {
                    // reflect ysrc
                    ysrc = hDIB - (ysrc + srcHeight);
                }

                if ( ::StretchDIBits(GetHdc(),
                                     xdest, ydest,
                                     dstWidth, dstHeight,
                                     xsrc, ysrc,
                                     srcWidth, srcHeight,
                                     ds.dsBm.bmBits,
                                     (LPBITMAPINFO)&ds.dsBmih,
                                     DIB_RGB_COLORS,
                                     dwRop
                                     ) == (int)GDI_ERROR )
                {
                    // On Win9x this API fails most (all?) of the time, so
                    // logging it becomes quite distracting.  Since it falls
                    // back to the code below this is not really serious, so
                    // don't log it.
                    //wxLogLastError(wxT("StretchDIBits"));
                }
                else
                {
                    success = true;
                }
            }
        }

        if ( !success && (caps & RC_STRETCHBLT) )
#endif
        // __WXWINCE__
        {
            SET_STRETCH_BLT_MODE(GetHdc());

            if ( !::StretchBlt
                    (
                        GetHdc(),
                        xdest, ydest, dstWidth, dstHeight,
                        hdcSrc,
                        xsrc, ysrc, srcWidth, srcHeight,
                        dwRop
                    ) )
            {
                wxLogLastError(wxT("StretchBlt"));
            }
            else
            {
                success = true;
            }
        }

        if ( !success )
        {
            if ( !::BitBlt(GetHdc(), xdest, ydest, dstWidth, dstHeight,
                           hdcSrc, xsrc, ysrc, dwRop) )
            {
                wxLogLastError(wxT("BitBlt"));
            }
            else
            {
                success = true;
            }
        }
    }

    if ( success )
    {
        CalcBoundingBox(xdest, ydest);
        CalcBoundingBox(xdest + dstWidth, ydest + dstHeight);
    }

    return success;
}

void wxMSWDCImpl::GetDeviceSize(int *width, int *height) const
{
    WXMICROWIN_CHECK_HDC

    if ( width )
        *width = ::GetDeviceCaps(GetHdc(), HORZRES);
    if ( height )
        *height = ::GetDeviceCaps(GetHdc(), VERTRES);
}

void wxMSWDCImpl::DoGetSizeMM(int *w, int *h) const
{
    WXMICROWIN_CHECK_HDC

    // if we implement it in terms of DoGetSize() instead of directly using the
    // results returned by GetDeviceCaps(HORZ/VERTSIZE) as was done before, it
    // will also work for wxWindowDC and wxClientDC even though their size is
    // not the same as the total size of the screen
    int wPixels, hPixels;
    DoGetSize(&wPixels, &hPixels);

    if ( w )
    {
        int wTotal = ::GetDeviceCaps(GetHdc(), HORZRES);

        wxCHECK_RET( wTotal, wxT("0 width device?") );

        *w = (wPixels * ::GetDeviceCaps(GetHdc(), HORZSIZE)) / wTotal;
    }

    if ( h )
    {
        int hTotal = ::GetDeviceCaps(GetHdc(), VERTRES);

        wxCHECK_RET( hTotal, wxT("0 height device?") );

        *h = (hPixels * ::GetDeviceCaps(GetHdc(), VERTSIZE)) / hTotal;
    }
}

wxSize wxMSWDCImpl::GetPPI() const
{
    WXMICROWIN_CHECK_HDC_RET(wxSize(0,0))

    int x = ::GetDeviceCaps(GetHdc(), LOGPIXELSX);
    int y = ::GetDeviceCaps(GetHdc(), LOGPIXELSY);

    return wxSize(x, y);
}

// ----------------------------------------------------------------------------
// DC caching
// ----------------------------------------------------------------------------

#if wxUSE_DC_CACHEING

/*
 * This implementation is a bit ugly and uses the old-fashioned wxList class, so I will
 * improve it in due course, either using arrays, or simply storing pointers to one
 * entry for the bitmap, and two for the DCs. -- JACS
 */

wxObjectList wxMSWDCImpl::sm_bitmapCache;
wxObjectList wxMSWDCImpl::sm_dcCache;

wxDCCacheEntry::wxDCCacheEntry(WXHBITMAP hBitmap, int w, int h, int depth)
{
    m_bitmap = hBitmap;
    m_dc = 0;
    m_width = w;
    m_height = h;
    m_depth = depth;
}

wxDCCacheEntry::wxDCCacheEntry(WXHDC hDC, int depth)
{
    m_bitmap = 0;
    m_dc = hDC;
    m_width = 0;
    m_height = 0;
    m_depth = depth;
}

wxDCCacheEntry::~wxDCCacheEntry()
{
    if (m_bitmap)
        ::DeleteObject((HBITMAP) m_bitmap);
    if (m_dc)
        ::DeleteDC((HDC) m_dc);
}

wxDCCacheEntry* wxMSWDCImpl::FindBitmapInCache(WXHDC dc, int w, int h)
{
    int depth = ::GetDeviceCaps((HDC) dc, PLANES) * ::GetDeviceCaps((HDC) dc, BITSPIXEL);
    wxList::compatibility_iterator node = sm_bitmapCache.GetFirst();
    while (node)
    {
        wxDCCacheEntry* entry = (wxDCCacheEntry*) node->GetData();

        if (entry->m_depth == depth)
        {
            if (entry->m_width < w || entry->m_height < h)
            {
                ::DeleteObject((HBITMAP) entry->m_bitmap);
                entry->m_bitmap = (WXHBITMAP) ::CreateCompatibleBitmap((HDC) dc, w, h);
                if ( !entry->m_bitmap)
                {
                    wxLogLastError(wxT("CreateCompatibleBitmap"));
                }
                entry->m_width = w; entry->m_height = h;
                return entry;
            }
            return entry;
        }

        node = node->GetNext();
    }
    WXHBITMAP hBitmap = (WXHBITMAP) ::CreateCompatibleBitmap((HDC) dc, w, h);
    if ( !hBitmap)
    {
        wxLogLastError(wxT("CreateCompatibleBitmap"));
    }
    wxDCCacheEntry* entry = new wxDCCacheEntry(hBitmap, w, h, depth);
    AddToBitmapCache(entry);
    return entry;
}

wxDCCacheEntry* wxMSWDCImpl::FindDCInCache(wxDCCacheEntry* notThis, WXHDC dc)
{
    int depth = ::GetDeviceCaps((HDC) dc, PLANES) * ::GetDeviceCaps((HDC) dc, BITSPIXEL);
    wxList::compatibility_iterator node = sm_dcCache.GetFirst();
    while (node)
    {
        wxDCCacheEntry* entry = (wxDCCacheEntry*) node->GetData();

        // Don't return the same one as we already have
        if (!notThis || (notThis != entry))
        {
            if (entry->m_depth == depth)
            {
                return entry;
            }
        }

        node = node->GetNext();
    }
    WXHDC hDC = (WXHDC) wxMSWImpl::CreateCompatibleDCWithLayout((HDC) dc);
    if ( !hDC)
    {
        wxLogLastError(wxT("CreateCompatibleDC"));
    }
    wxDCCacheEntry* entry = new wxDCCacheEntry(hDC, depth);
    AddToDCCache(entry);
    return entry;
}

void wxMSWDCImpl::AddToBitmapCache(wxDCCacheEntry* entry)
{
    sm_bitmapCache.Append(entry);
}

void wxMSWDCImpl::AddToDCCache(wxDCCacheEntry* entry)
{
    sm_dcCache.Append(entry);
}

void wxMSWDCImpl::ClearCache()
{
    WX_CLEAR_LIST(wxList, sm_dcCache);
    WX_CLEAR_LIST(wxList, sm_bitmapCache);
}

// Clean up cache at app exit
class wxDCModule : public wxModule
{
public:
    virtual bool OnInit() { return true; }
    virtual void OnExit() { wxMSWDCImpl::ClearCache(); }

private:
    DECLARE_DYNAMIC_CLASS(wxDCModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxDCModule, wxModule)

#endif // wxUSE_DC_CACHEING

// ----------------------------------------------------------------------------
// alpha channel support
// ----------------------------------------------------------------------------

static bool AlphaBlt(HDC hdcDst,
                     int x, int y, int dstWidth, int dstHeight,
                     int srcX, int srcY,
                     int srcWidth, int srcHeight,
                     HDC hdcSrc,
                     const wxBitmap& bmp)
{
    wxASSERT_MSG( bmp.IsOk() && bmp.HasAlpha(), wxT("AlphaBlt(): invalid bitmap") );
    wxASSERT_MSG( hdcDst && hdcSrc, wxT("AlphaBlt(): invalid HDC") );

    // do we have AlphaBlend() and company in the headers?
#if defined(AC_SRC_OVER) && wxUSE_DYNLIB_CLASS
    // yes, now try to see if we have it during run-time
    typedef BOOL (WINAPI *AlphaBlend_t)(HDC,int,int,int,int,
                                        HDC,int,int,int,int,
                                        BLENDFUNCTION);

    static AlphaBlend_t
        pfnAlphaBlend = (AlphaBlend_t)wxMSIMG32DLL.GetSymbol(wxT("AlphaBlend"));
    if ( pfnAlphaBlend )
    {
        BLENDFUNCTION bf;
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 0xff;
        bf.AlphaFormat = AC_SRC_ALPHA;

        if ( pfnAlphaBlend(hdcDst, x, y, dstWidth, dstHeight,
                           hdcSrc, srcX, srcY, srcWidth, srcHeight,
                           bf) )
        {
            // skip wxAlphaBlend() call below
            return true;
        }

        wxLogLastError(wxT("AlphaBlend"));
    }
#else
    wxUnusedVar(hdcSrc);
#endif // defined(AC_SRC_OVER)

    // AlphaBlend() unavailable of failed: use our own (probably much slower)
    // implementation
#ifdef wxHAS_RAW_BITMAP
    wxAlphaBlend(hdcDst, x, y, dstWidth, dstHeight, srcX, srcY, srcWidth, srcHeight, bmp);

    return true;
#else // !wxHAS_RAW_BITMAP
    // no wxAlphaBlend() neither, fall back to using simple BitBlt() (we lose
    // alpha but at least something will be shown like this)
    wxUnusedVar(bmp);
    return false;
#endif // wxHAS_RAW_BITMAP/!wxHAS_RAW_BITMAP
}


// wxAlphaBlend: our fallback if ::AlphaBlend() is unavailable
#ifdef wxHAS_RAW_BITMAP

static void
wxAlphaBlend(HDC hdcDst, int xDst, int yDst,
             int dstWidth, int dstHeight,
             int srcX, int srcY,
             int srcWidth, int srcHeight,
             const wxBitmap& bmpSrc)
{
    // get the destination DC pixels
    wxBitmap bmpDst(dstWidth, dstHeight, 32 /* force creating RGBA DIB */);
    MemoryHDC hdcMem;
    SelectInHDC select(hdcMem, GetHbitmapOf(bmpDst));

    if ( !::BitBlt(hdcMem, 0, 0, dstWidth, dstHeight, hdcDst, xDst, yDst, SRCCOPY) )
    {
        wxLogLastError(wxT("BitBlt"));
    }

    // combine them with the source bitmap using alpha
    wxAlphaPixelData dataDst(bmpDst),
                     dataSrc((wxBitmap &)bmpSrc);

    wxCHECK_RET( dataDst && dataSrc,
                    wxT("failed to get raw data in wxAlphaBlend") );

    wxAlphaPixelData::Iterator pDst(dataDst),
                               pSrc(dataSrc);


    for ( int y = 0; y < dstHeight; y++ )
    {
        wxAlphaPixelData::Iterator pDstRowStart = pDst;

        for ( int x = 0; x < dstWidth; x++ )
        {
            // source is point sampled, Alpha StretchBlit is ugly on Win95
            // (but does not impact performance)
            pSrc.MoveTo(dataSrc, srcX + (srcWidth*x/dstWidth), srcY + (srcHeight*y/dstHeight));

            // note that source bitmap uses premultiplied alpha (as required by
            // the real AlphaBlend)
            const unsigned beta = 255 - pSrc.Alpha();

            pDst.Red() = pSrc.Red() + (beta * pDst.Red() + 127) / 255;
            pDst.Blue() = pSrc.Blue() + (beta * pDst.Blue() + 127) / 255;
            pDst.Green() = pSrc.Green() + (beta * pDst.Green() + 127) / 255;

            ++pDst;
        }

        pDst = pDstRowStart;
        pDst.OffsetY(dataDst, 1);
    }

    // and finally blit them back to the destination DC
    if ( !::BitBlt(hdcDst, xDst, yDst, dstWidth, dstHeight, hdcMem, 0, 0, SRCCOPY) )
    {
        wxLogLastError(wxT("BitBlt"));
    }
}

#endif // wxHAS_RAW_BITMAP

void wxMSWDCImpl::DoGradientFillLinear (const wxRect& rect,
                                 const wxColour& initialColour,
                                 const wxColour& destColour,
                                 wxDirection nDirection)
{
    // use native function if we have compile-time support it and can load it
    // during run-time (linking to it statically would make the program
    // unusable on earlier Windows versions)
#if defined(GRADIENT_FILL_RECT_H) && wxUSE_DYNLIB_CLASS
    typedef BOOL
        (WINAPI *GradientFill_t)(HDC, PTRIVERTEX, ULONG, PVOID, ULONG, ULONG);
    static GradientFill_t pfnGradientFill =
        (GradientFill_t)wxMSIMG32DLL.GetSymbol(wxT("GradientFill"));

    if ( pfnGradientFill )
    {
        GRADIENT_RECT grect;
        grect.UpperLeft = 0;
        grect.LowerRight = 1;

        // invert colours direction if not filling from left-to-right or
        // top-to-bottom
        int firstVertex = nDirection == wxNORTH || nDirection == wxWEST ? 1 : 0;

        // one vertex for upper left and one for upper-right
        TRIVERTEX vertices[2];

        vertices[0].x = rect.GetLeft();
        vertices[0].y = rect.GetTop();
        vertices[1].x = rect.GetRight()+1;
        vertices[1].y = rect.GetBottom()+1;

        vertices[firstVertex].Red = (COLOR16)(initialColour.Red() << 8);
        vertices[firstVertex].Green = (COLOR16)(initialColour.Green() << 8);
        vertices[firstVertex].Blue = (COLOR16)(initialColour.Blue() << 8);
        vertices[firstVertex].Alpha = 0;
        vertices[1 - firstVertex].Red = (COLOR16)(destColour.Red() << 8);
        vertices[1 - firstVertex].Green = (COLOR16)(destColour.Green() << 8);
        vertices[1 - firstVertex].Blue = (COLOR16)(destColour.Blue() << 8);
        vertices[1 - firstVertex].Alpha = 0;

        if ( (*pfnGradientFill)
             (
                GetHdc(),
                vertices,
                WXSIZEOF(vertices),
                &grect,
                1,
                nDirection == wxWEST || nDirection == wxEAST
                    ? GRADIENT_FILL_RECT_H
                    : GRADIENT_FILL_RECT_V
             ) )
        {
            // skip call of the base class version below
            CalcBoundingBox(rect.GetLeft(), rect.GetBottom());
            CalcBoundingBox(rect.GetRight(), rect.GetTop());
            return;
        }

        wxLogLastError(wxT("GradientFill"));
    }
#endif // wxUSE_DYNLIB_CLASS

    wxDCImpl::DoGradientFillLinear(rect, initialColour, destColour, nDirection);
}

#if wxUSE_DYNLIB_CLASS

namespace wxMSWImpl
{

DWORD GetLayout(HDC hdc)
{
    typedef DWORD (WINAPI *GetLayout_t)(HDC);
    static GetLayout_t
        wxDL_INIT_FUNC(s_pfn, GetLayout, wxDynamicLibrary(wxT("gdi32.dll")));

    return s_pfnGetLayout ? s_pfnGetLayout(hdc) : GDI_ERROR;
}

DWORD SetLayout(HDC hdc, DWORD dwLayout)
{
    typedef DWORD (WINAPI *SetLayout_t)(HDC, DWORD);
    static SetLayout_t
        wxDL_INIT_FUNC(s_pfn, SetLayout, wxDynamicLibrary(wxT("gdi32.dll")));

    return s_pfnSetLayout ? s_pfnSetLayout(hdc, dwLayout) : GDI_ERROR;
}

HDC CreateCompatibleDCWithLayout(HDC hdc)
{
    HDC hdcNew = ::CreateCompatibleDC(hdc);
    if ( hdcNew )
    {
        DWORD dwLayout = wxMSWImpl::GetLayout(hdc);
        if ( dwLayout != GDI_ERROR )
            wxMSWImpl::SetLayout(hdcNew, dwLayout);
    }

    return hdcNew;
}

} // namespace wxMSWImpl

wxLayoutDirection wxMSWDCImpl::GetLayoutDirection() const
{
    DWORD layout = wxMSWImpl::GetLayout(GetHdc());

    if ( layout == GDI_ERROR )
        return wxLayout_Default;

    return layout & LAYOUT_RTL ? wxLayout_RightToLeft : wxLayout_LeftToRight;
}

void wxMSWDCImpl::SetLayoutDirection(wxLayoutDirection dir)
{
    if ( dir == wxLayout_Default )
    {
        dir = wxTheApp->GetLayoutDirection();
        if ( dir == wxLayout_Default )
            return;
    }

    DWORD layout = wxMSWImpl::GetLayout(GetHdc());
    if ( layout == GDI_ERROR )
        return;

    if ( dir == wxLayout_RightToLeft )
        layout |= LAYOUT_RTL;
    else
        layout &= ~LAYOUT_RTL;

    wxMSWImpl::SetLayout(GetHdc(), layout);
}

#else // !wxUSE_DYNLIB_CLASS

// Provide stubs to avoid ifdefs in the code using these functions.
namespace wxMSWImpl
{

DWORD GetLayout(HDC WXUNUSED(hdc))
{
    return GDI_ERROR;
}

DWORD SetLayout(HDC WXUNUSED(hdc), DWORD WXUNUSED(dwLayout))
{
    return GDI_ERROR;
}

HDC CreateCompatibleDCWithLayout(HDC hdc)
{
    return ::CreateCompatibleDC(hdc);
}

} // namespace wxMSWImpl

// we can't provide RTL support without dynamic loading, so stub it out
wxLayoutDirection wxMSWDCImpl::GetLayoutDirection() const
{
    return wxLayout_Default;
}

void wxMSWDCImpl::SetLayoutDirection(wxLayoutDirection WXUNUSED(dir))
{
}

#endif // wxUSE_DYNLIB_CLASS/!wxUSE_DYNLIB_CLASS
