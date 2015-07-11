/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/dcbase.cpp
// Purpose:     generic methods of the wxDC Class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     05/25/99
// Copyright:   (c) wxWidgets team
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

#include "wx/dc.h"
#include "wx/dcclient.h"
#include "wx/dcmemory.h"
#include "wx/dcscreen.h"
#include "wx/dcprint.h"
#include "wx/prntbase.h"
#include "wx/scopeguard.h"

#ifndef WX_PRECOMP
    #include "wx/math.h"
    #include "wx/module.h"
    #include "wx/window.h"
#endif

#include "wx/private/textmeasure.h"

#ifdef __WXMSW__
    #include "wx/msw/dcclient.h"
    #include "wx/msw/dcmemory.h"
    #include "wx/msw/dcscreen.h"
#endif

#ifdef __WXGTK3__
    #include "wx/gtk/dc.h"
#elif defined __WXGTK20__
    #include "wx/gtk/dcclient.h"
    #include "wx/gtk/dcmemory.h"
    #include "wx/gtk/dcscreen.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/dcclient.h"
    #include "wx/gtk1/dcmemory.h"
    #include "wx/gtk1/dcscreen.h"
#endif

#ifdef __WXMAC__
    #include "wx/osx/dcclient.h"
    #include "wx/osx/dcmemory.h"
    #include "wx/osx/dcscreen.h"
#endif

#ifdef __WXPM__
    #include "wx/os2/dcclient.h"
    #include "wx/os2/dcmemory.h"
    #include "wx/os2/dcscreen.h"
#endif

#ifdef __WXCOCOA__
    #include "wx/cocoa/dcclient.h"
    #include "wx/cocoa/dcmemory.h"
    #include "wx/cocoa/dcscreen.h"
#endif

#ifdef __WXMOTIF__
    #include "wx/motif/dcclient.h"
    #include "wx/motif/dcmemory.h"
    #include "wx/motif/dcscreen.h"
#endif

#ifdef __WXX11__
    #include "wx/x11/dcclient.h"
    #include "wx/x11/dcmemory.h"
    #include "wx/x11/dcscreen.h"
#endif

#ifdef __WXDFB__
    #include "wx/dfb/dcclient.h"
    #include "wx/dfb/dcmemory.h"
    #include "wx/dfb/dcscreen.h"
#endif

//----------------------------------------------------------------------------
// wxDCFactory
//----------------------------------------------------------------------------

wxDCFactory *wxDCFactory::m_factory = NULL;

void wxDCFactory::Set(wxDCFactory *factory)
{
    delete m_factory;

    m_factory = factory;
}

wxDCFactory *wxDCFactory::Get()
{
    if ( !m_factory )
        m_factory = new wxNativeDCFactory;

    return m_factory;
}

class wxDCFactoryCleanupModule : public wxModule
{
public:
    virtual bool OnInit() { return true; }
    virtual void OnExit() { wxDCFactory::Set(NULL); }

private:
    DECLARE_DYNAMIC_CLASS(wxDCFactoryCleanupModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxDCFactoryCleanupModule, wxModule)

//-----------------------------------------------------------------------------
// wxNativeDCFactory
//-----------------------------------------------------------------------------

wxDCImpl* wxNativeDCFactory::CreateWindowDC( wxWindowDC *owner, wxWindow *window )
{
    wxDCImpl * const impl = new wxWindowDCImpl( owner, window );
    impl->InheritAttributes(window);
    return impl;
}

wxDCImpl* wxNativeDCFactory::CreateClientDC( wxClientDC *owner, wxWindow *window )
{
    wxDCImpl * const impl = new wxClientDCImpl( owner, window );
    impl->InheritAttributes(window);
    return impl;
}

wxDCImpl* wxNativeDCFactory::CreatePaintDC( wxPaintDC *owner, wxWindow *window )
{
    wxDCImpl * const impl = new wxPaintDCImpl( owner, window );
    impl->InheritAttributes(window);
    return impl;
}

wxDCImpl* wxNativeDCFactory::CreateMemoryDC( wxMemoryDC *owner )
{
    return new wxMemoryDCImpl( owner );
}

wxDCImpl* wxNativeDCFactory::CreateMemoryDC(wxMemoryDC *owner, wxBitmap& bitmap)
{
    // the bitmap may be modified when it's selected into a memory DC so make
    // sure changing this bitmap doesn't affect any other shallow copies of it
    // (see wxMemoryDC::SelectObject())
    //
    // notice that we don't provide any ctor equivalent to SelectObjectAsSource
    // method because this should be rarely needed and easy to work around by
    // using the default ctor and calling SelectObjectAsSource itself
    if ( bitmap.IsOk() )
        bitmap.UnShare();

    return new wxMemoryDCImpl(owner, bitmap);
}

wxDCImpl* wxNativeDCFactory::CreateMemoryDC( wxMemoryDC *owner, wxDC *dc )
{
    return new wxMemoryDCImpl( owner, dc );
}

wxDCImpl* wxNativeDCFactory::CreateScreenDC( wxScreenDC *owner )
{
    return new wxScreenDCImpl( owner );
}

#if wxUSE_PRINTING_ARCHITECTURE
wxDCImpl *wxNativeDCFactory::CreatePrinterDC( wxPrinterDC *owner, const wxPrintData &data )
{
    wxPrintFactory *factory = wxPrintFactory::GetFactory();
    return factory->CreatePrinterDCImpl( owner, data );
}
#endif

//-----------------------------------------------------------------------------
// wxWindowDC
//-----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxWindowDC, wxDC)

wxWindowDC::wxWindowDC(wxWindow *win)
          : wxDC(wxDCFactory::Get()->CreateWindowDC(this, win))
{
}

//-----------------------------------------------------------------------------
// wxClientDC
//-----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxClientDC, wxWindowDC)

wxClientDC::wxClientDC(wxWindow *win)
          : wxWindowDC(wxDCFactory::Get()->CreateClientDC(this, win))
{
}

//-----------------------------------------------------------------------------
// wxMemoryDC
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxMemoryDC, wxDC)

wxMemoryDC::wxMemoryDC()
          : wxDC(wxDCFactory::Get()->CreateMemoryDC(this))
{
}

wxMemoryDC::wxMemoryDC(wxBitmap& bitmap)
          : wxDC(wxDCFactory::Get()->CreateMemoryDC(this, bitmap))
{
}

wxMemoryDC::wxMemoryDC(wxDC *dc)
          : wxDC(wxDCFactory::Get()->CreateMemoryDC(this, dc))
{
}

void wxMemoryDC::SelectObject(wxBitmap& bmp)
{
    if ( bmp.IsSameAs(GetSelectedBitmap()) )
    {
        // Nothing to do, this bitmap is already selected.
        return;
    }

    // make sure that the given wxBitmap is not sharing its data with other
    // wxBitmap instances as its contents will be modified by any drawing
    // operation done on this DC
    if (bmp.IsOk())
        bmp.UnShare();

    GetImpl()->DoSelect(bmp);
}

void wxMemoryDC::SelectObjectAsSource(const wxBitmap& bmp)
{
    GetImpl()->DoSelect(bmp);
}

const wxBitmap& wxMemoryDC::GetSelectedBitmap() const
{
    return GetImpl()->GetSelectedBitmap();
}

wxBitmap& wxMemoryDC::GetSelectedBitmap()
{
    return GetImpl()->GetSelectedBitmap();
}


//-----------------------------------------------------------------------------
// wxPaintDC
//-----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxPaintDC, wxClientDC)

wxPaintDC::wxPaintDC(wxWindow *win)
         : wxClientDC(wxDCFactory::Get()->CreatePaintDC(this, win))
{
}

//-----------------------------------------------------------------------------
// wxScreenDC
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxScreenDC, wxWindowDC)

wxScreenDC::wxScreenDC()
          : wxDC(wxDCFactory::Get()->CreateScreenDC(this))
{
}

//-----------------------------------------------------------------------------
// wxPrinterDC
//-----------------------------------------------------------------------------

#if wxUSE_PRINTING_ARCHITECTURE

IMPLEMENT_DYNAMIC_CLASS(wxPrinterDC, wxDC)

wxPrinterDC::wxPrinterDC()
           : wxDC(wxDCFactory::Get()->CreatePrinterDC(this, wxPrintData()))
{
}

wxPrinterDC::wxPrinterDC(const wxPrintData& data)
           : wxDC(wxDCFactory::Get()->CreatePrinterDC(this, data))
{
}

wxRect wxPrinterDC::GetPaperRect() const
{
    return GetImpl()->GetPaperRect();
}

int wxPrinterDC::GetResolution() const
{
    return GetImpl()->GetResolution();
}

#endif // wxUSE_PRINTING_ARCHITECTURE

//-----------------------------------------------------------------------------
// wxDCImpl
//-----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxDCImpl, wxObject)

wxDCImpl::wxDCImpl( wxDC *owner )
        : m_window(NULL)
        , m_colour(wxColourDisplay())
        , m_ok(true)
        , m_clipping(false)
        , m_isInteractive(0)
        , m_isBBoxValid(false)
        , m_logicalOriginX(0), m_logicalOriginY(0)
        , m_deviceOriginX(0), m_deviceOriginY(0)
        , m_deviceLocalOriginX(0), m_deviceLocalOriginY(0)
        , m_logicalScaleX(1.0), m_logicalScaleY(1.0)
        , m_userScaleX(1.0), m_userScaleY(1.0)
        , m_scaleX(1.0), m_scaleY(1.0)
        , m_signX(1), m_signY(1)
        , m_contentScaleFactor(1)
        , m_minX(0), m_minY(0), m_maxX(0), m_maxY(0)
        , m_clipX1(0), m_clipY1(0), m_clipX2(0), m_clipY2(0)
        , m_logicalFunction(wxCOPY)
        , m_backgroundMode(wxBRUSHSTYLE_TRANSPARENT)
        , m_mappingMode(wxMM_TEXT)
        , m_pen()
        , m_brush()
        , m_backgroundBrush()
        , m_textForegroundColour(*wxBLACK)
        , m_textBackgroundColour(*wxWHITE)
        , m_font()
#if wxUSE_PALETTE
        , m_palette()
        , m_hasCustomPalette(false)
#endif // wxUSE_PALETTE
{
    m_owner = owner;

    m_mm_to_pix_x = (double)wxGetDisplaySize().GetWidth() /
                    (double)wxGetDisplaySizeMM().GetWidth();
    m_mm_to_pix_y = (double)wxGetDisplaySize().GetHeight() /
                    (double)wxGetDisplaySizeMM().GetHeight();

    ResetBoundingBox();
    ResetClipping();
}

wxDCImpl::~wxDCImpl()
{
}

// ----------------------------------------------------------------------------
// clipping
// ----------------------------------------------------------------------------

void wxDCImpl::DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord w, wxCoord h)
{
    if ( m_clipping )
    {
        m_clipX1 = wxMax( m_clipX1, x );
        m_clipY1 = wxMax( m_clipY1, y );
        m_clipX2 = wxMin( m_clipX2, (x + w) );
        m_clipY2 = wxMin( m_clipY2, (y + h) );
    }
    else
    {
        m_clipping = true;

        m_clipX1 = x;
        m_clipY1 = y;
        m_clipX2 = x + w;
        m_clipY2 = y + h;
    }
}

// ----------------------------------------------------------------------------
// coordinate conversions and transforms
// ----------------------------------------------------------------------------

wxCoord wxDCImpl::DeviceToLogicalX(wxCoord x) const
{
    return wxRound( (double)((x - m_deviceOriginX - m_deviceLocalOriginX) * m_signX) / m_scaleX ) + m_logicalOriginX ;
}

wxCoord wxDCImpl::DeviceToLogicalY(wxCoord y) const
{
    return wxRound( (double)((y - m_deviceOriginY - m_deviceLocalOriginY) * m_signY) / m_scaleY ) + m_logicalOriginY ;
}

wxCoord wxDCImpl::DeviceToLogicalXRel(wxCoord x) const
{
    return wxRound((double)(x) / m_scaleX);
}

wxCoord wxDCImpl::DeviceToLogicalYRel(wxCoord y) const
{
    return wxRound((double)(y) / m_scaleY);
}

wxCoord wxDCImpl::LogicalToDeviceX(wxCoord x) const
{
    return wxRound( (double)((x - m_logicalOriginX) * m_signX) * m_scaleX) + m_deviceOriginX + m_deviceLocalOriginX;
}

wxCoord wxDCImpl::LogicalToDeviceY(wxCoord y) const
{
    return wxRound( (double)((y - m_logicalOriginY) * m_signY) * m_scaleY) + m_deviceOriginY + m_deviceLocalOriginY;
}

wxCoord wxDCImpl::LogicalToDeviceXRel(wxCoord x) const
{
    return wxRound((double)(x) * m_scaleX);
}

wxCoord wxDCImpl::LogicalToDeviceYRel(wxCoord y) const
{
    return wxRound((double)(y) * m_scaleY);
}

void wxDCImpl::ComputeScaleAndOrigin()
{
    m_scaleX = m_logicalScaleX * m_userScaleX;
    m_scaleY = m_logicalScaleY * m_userScaleY;
}

void wxDCImpl::SetMapMode( wxMappingMode mode )
{
    switch (mode)
    {
        case wxMM_TWIPS:
          SetLogicalScale( twips2mm*m_mm_to_pix_x, twips2mm*m_mm_to_pix_y );
          break;
        case wxMM_POINTS:
          SetLogicalScale( pt2mm*m_mm_to_pix_x, pt2mm*m_mm_to_pix_y );
          break;
        case wxMM_METRIC:
          SetLogicalScale( m_mm_to_pix_x, m_mm_to_pix_y );
          break;
        case wxMM_LOMETRIC:
          SetLogicalScale( m_mm_to_pix_x/10.0, m_mm_to_pix_y/10.0 );
          break;
        default:
        case wxMM_TEXT:
          SetLogicalScale( 1.0, 1.0 );
          break;
    }
    m_mappingMode = mode;
}

void wxDCImpl::SetUserScale( double x, double y )
{
    // allow negative ? -> no
    m_userScaleX = x;
    m_userScaleY = y;
    ComputeScaleAndOrigin();
}

void wxDCImpl::SetLogicalScale( double x, double y )
{
    // allow negative ?
    m_logicalScaleX = x;
    m_logicalScaleY = y;
    ComputeScaleAndOrigin();
}

void wxDCImpl::SetLogicalOrigin( wxCoord x, wxCoord y )
{
    m_logicalOriginX = x * m_signX;
    m_logicalOriginY = y * m_signY;
    ComputeScaleAndOrigin();
}

void wxDCImpl::SetDeviceOrigin( wxCoord x, wxCoord y )
{
    m_deviceOriginX = x;
    m_deviceOriginY = y;
    ComputeScaleAndOrigin();
}

void wxDCImpl::SetDeviceLocalOrigin( wxCoord x, wxCoord y )
{
    m_deviceLocalOriginX = x;
    m_deviceLocalOriginY = y;
    ComputeScaleAndOrigin();
}

void wxDCImpl::SetAxisOrientation( bool xLeftRight, bool yBottomUp )
{
    // only wxPostScripDC has m_signX = -1, we override SetAxisOrientation there
    // wxWidgets 2.9: no longer override it
    m_signX = (xLeftRight ?  1 : -1);
    m_signY = (yBottomUp  ? -1 :  1);
    ComputeScaleAndOrigin();
}

bool wxDCImpl::DoGetPartialTextExtents(const wxString& text, wxArrayInt& widths) const
{
    wxTextMeasure tm(GetOwner(), &m_font);
    return tm.GetPartialTextExtents(text, widths, m_scaleX);
}

void wxDCImpl::GetMultiLineTextExtent(const wxString& text,
                                      wxCoord *x,
                                      wxCoord *y,
                                      wxCoord *h,
                                      const wxFont *font) const
{
    wxTextMeasure tm(GetOwner(), font && font->IsOk() ? font : &m_font);
    tm.GetMultiLineTextExtent(text, x, y, h);
}

void wxDCImpl::DoDrawCheckMark(wxCoord x1, wxCoord y1,
                               wxCoord width, wxCoord height)
{
    wxCHECK_RET( IsOk(), wxT("invalid window dc") );

    wxCoord x2 = x1 + width,
            y2 = y1 + height;

    // the pen width is calibrated to give 3 for width == height == 10
    wxDCPenChanger pen( *m_owner, wxPen(GetTextForeground(), (width + height + 1)/7));

    // we're drawing a scaled version of wx/generic/tick.xpm here
    wxCoord x3 = x1 + (4*width) / 10,   // x of the tick bottom
            y3 = y1 + height / 2;       // y of the left tick branch
    DoDrawLine(x1, y3, x3, y2);
    DoDrawLine(x3, y2, x2, y1);

    CalcBoundingBox(x1, y1);
    CalcBoundingBox(x2, y2);
}

bool
wxDCImpl::DoStretchBlit(wxCoord xdest, wxCoord ydest,
                        wxCoord dstWidth, wxCoord dstHeight,
                        wxDC *source,
                        wxCoord xsrc, wxCoord ysrc,
                        wxCoord srcWidth, wxCoord srcHeight,
                        wxRasterOperationMode rop,
                        bool useMask,
                        wxCoord xsrcMask,
                        wxCoord ysrcMask)
{
    wxCHECK_MSG( srcWidth && srcHeight && dstWidth && dstHeight, false,
                 wxT("invalid blit size") );

    // emulate the stretching by modifying the DC scale
    double xscale = (double)srcWidth/dstWidth,
           yscale = (double)srcHeight/dstHeight;

    double xscaleOld, yscaleOld;
    GetUserScale(&xscaleOld, &yscaleOld);
    SetUserScale(xscaleOld/xscale, yscaleOld/yscale);

    bool rc = DoBlit(wxCoord(xdest*xscale), wxCoord(ydest*yscale),
                     wxCoord(dstWidth*xscale), wxCoord(dstHeight*yscale),
                     source,
                     xsrc, ysrc, rop, useMask, xsrcMask, ysrcMask);

    SetUserScale(xscaleOld, yscaleOld);

    return rc;
}

void wxDCImpl::DrawLines(const wxPointList *list, wxCoord xoffset, wxCoord yoffset)
{
    int n = list->GetCount();
    wxPoint *points = new wxPoint[n];

    int i = 0;
    for ( wxPointList::compatibility_iterator node = list->GetFirst(); node; node = node->GetNext(), i++ )
    {
        wxPoint *point = node->GetData();
        points[i].x = point->x;
        points[i].y = point->y;
    }

    DoDrawLines(n, points, xoffset, yoffset);

    delete [] points;
}

void wxDCImpl::DrawPolygon(const wxPointList *list,
                           wxCoord xoffset, wxCoord yoffset,
                           wxPolygonFillMode fillStyle)
{
    int n = list->GetCount();
    wxPoint *points = new wxPoint[n];

    int i = 0;
    for ( wxPointList::compatibility_iterator node = list->GetFirst(); node; node = node->GetNext(), i++ )
    {
        wxPoint *point = node->GetData();
        points[i].x = point->x;
        points[i].y = point->y;
    }

    DoDrawPolygon(n, points, xoffset, yoffset, fillStyle);

    delete [] points;
}

void
wxDCImpl::DoDrawPolyPolygon(int n,
                            const int count[],
                            const wxPoint points[],
                            wxCoord xoffset, wxCoord yoffset,
                            wxPolygonFillMode fillStyle)
{
    if ( n == 1 )
    {
        DoDrawPolygon(count[0], points, xoffset, yoffset, fillStyle);
        return;
    }

    int      i, j, lastOfs;
    wxPoint* pts;

    for (i = j = lastOfs = 0; i < n; i++)
    {
        lastOfs = j;
        j      += count[i];
    }
    pts = new wxPoint[j+n-1];
    for (i = 0; i < j; i++)
        pts[i] = points[i];
    for (i = 2; i <= n; i++)
    {
        lastOfs -= count[n-i];
        pts[j++] = pts[lastOfs];
    }

    {
        wxDCPenChanger setTransp(*m_owner, *wxTRANSPARENT_PEN);
        DoDrawPolygon(j, pts, xoffset, yoffset, fillStyle);
    }

    for (i = j = 0; i < n; i++)
    {
        DoDrawLines(count[i], pts+j, xoffset, yoffset);
        j += count[i];
    }
    delete[] pts;
}

#if wxUSE_SPLINES

void wxDCImpl::DrawSpline(wxCoord x1, wxCoord y1,
                          wxCoord x2, wxCoord y2,
                          wxCoord x3, wxCoord y3)
{
    wxPoint points[] = { wxPoint(x1, y1), wxPoint(x2, y2), wxPoint(x3, y3) };
    DrawSpline(WXSIZEOF(points), points);
}

void wxDCImpl::DrawSpline(int n, const wxPoint points[])
{
    wxPointList list;
    for ( int i = 0; i < n; i++ )
        list.Append(const_cast<wxPoint*>(&points[i]));

    DrawSpline(&list);
}

// ----------------------------------- spline code ----------------------------------------

void wx_quadratic_spline(double a1, double b1, double a2, double b2,
                         double a3, double b3, double a4, double b4);
void wx_clear_stack();
int wx_spline_pop(double *x1, double *y1, double *x2, double *y2, double *x3,
        double *y3, double *x4, double *y4);
void wx_spline_push(double x1, double y1, double x2, double y2, double x3, double y3,
          double x4, double y4);
static bool wx_spline_add_point(double x, double y);
static void wx_spline_draw_point_array(wxDC *dc);

static wxPointList wx_spline_point_list;

#define                half(z1, z2)        ((z1+z2)/2.0)
#define                THRESHOLD        5

/* iterative version */

void wx_quadratic_spline(double a1, double b1, double a2, double b2, double a3, double b3, double a4,
                 double b4)
{
    double xmid, ymid;
    double x1, y1, x2, y2, x3, y3, x4, y4;

    wx_clear_stack();
    wx_spline_push(a1, b1, a2, b2, a3, b3, a4, b4);

    while (wx_spline_pop(&x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4)) {
        xmid = (double)half(x2, x3);
        ymid = (double)half(y2, y3);
        if (fabs(x1 - xmid) < THRESHOLD && fabs(y1 - ymid) < THRESHOLD &&
            fabs(xmid - x4) < THRESHOLD && fabs(ymid - y4) < THRESHOLD) {
            wx_spline_add_point( x1, y1 );
            wx_spline_add_point( xmid, ymid );
        } else {
            wx_spline_push(xmid, ymid, (double)half(xmid, x3), (double)half(ymid, y3),
                 (double)half(x3, x4), (double)half(y3, y4), x4, y4);
            wx_spline_push(x1, y1, (double)half(x1, x2), (double)half(y1, y2),
                 (double)half(x2, xmid), (double)half(y2, ymid), xmid, ymid);
        }
    }
}

/* utilities used by spline drawing routines */

typedef struct wx_spline_stack_struct {
    double           x1, y1, x2, y2, x3, y3, x4, y4;
} Stack;

#define         SPLINE_STACK_DEPTH             20
static Stack    wx_spline_stack[SPLINE_STACK_DEPTH];
static Stack   *wx_stack_top;
static int      wx_stack_count;

void wx_clear_stack()
{
    wx_stack_top = wx_spline_stack;
    wx_stack_count = 0;
}

void wx_spline_push(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4)
{
    wx_stack_top->x1 = x1;
    wx_stack_top->y1 = y1;
    wx_stack_top->x2 = x2;
    wx_stack_top->y2 = y2;
    wx_stack_top->x3 = x3;
    wx_stack_top->y3 = y3;
    wx_stack_top->x4 = x4;
    wx_stack_top->y4 = y4;
    wx_stack_top++;
    wx_stack_count++;
}

int wx_spline_pop(double *x1, double *y1, double *x2, double *y2,
                  double *x3, double *y3, double *x4, double *y4)
{
    if (wx_stack_count == 0)
        return (0);
    wx_stack_top--;
    wx_stack_count--;
    *x1 = wx_stack_top->x1;
    *y1 = wx_stack_top->y1;
    *x2 = wx_stack_top->x2;
    *y2 = wx_stack_top->y2;
    *x3 = wx_stack_top->x3;
    *y3 = wx_stack_top->y3;
    *x4 = wx_stack_top->x4;
    *y4 = wx_stack_top->y4;
    return (1);
}

static bool wx_spline_add_point(double x, double y)
{
    wxPoint *point = new wxPoint( wxRound(x), wxRound(y) );
    wx_spline_point_list.Append(point );
    return true;
}

static void wx_spline_draw_point_array(wxDC *dc)
{
    dc->DrawLines(&wx_spline_point_list, 0, 0 );
    wxPointList::compatibility_iterator node = wx_spline_point_list.GetFirst();
    while (node)
    {
        wxPoint *point = node->GetData();
        delete point;
        wx_spline_point_list.Erase(node);
        node = wx_spline_point_list.GetFirst();
    }
}

void wxDCImpl::DoDrawSpline( const wxPointList *points )
{
    wxCHECK_RET( IsOk(), wxT("invalid window dc") );

    const wxPoint *p;
    double           cx1, cy1, cx2, cy2, cx3, cy3, cx4, cy4;
    double           x1, y1, x2, y2;

    wxPointList::compatibility_iterator node = points->GetFirst();
    if (!node)
        // empty list
        return;

    p = node->GetData();

    x1 = p->x;
    y1 = p->y;

    node = node->GetNext();
    p = node->GetData();

    x2 = p->x;
    y2 = p->y;
    cx1 = (double)((x1 + x2) / 2);
    cy1 = (double)((y1 + y2) / 2);
    cx2 = (double)((cx1 + x2) / 2);
    cy2 = (double)((cy1 + y2) / 2);

    wx_spline_add_point(x1, y1);

    while ((node = node->GetNext())
#if !wxUSE_STD_CONTAINERS
           != NULL
#endif // !wxUSE_STD_CONTAINERS
          )
    {
        p = node->GetData();
        x1 = x2;
        y1 = y2;
        x2 = p->x;
        y2 = p->y;
        cx4 = (double)(x1 + x2) / 2;
        cy4 = (double)(y1 + y2) / 2;
        cx3 = (double)(x1 + cx4) / 2;
        cy3 = (double)(y1 + cy4) / 2;

        wx_quadratic_spline(cx1, cy1, cx2, cy2, cx3, cy3, cx4, cy4);

        cx1 = cx4;
        cy1 = cy4;
        cx2 = (double)(cx1 + x2) / 2;
        cy2 = (double)(cy1 + y2) / 2;
    }

    wx_spline_add_point( cx1, cy1 );
    wx_spline_add_point( x2, y2 );

    wx_spline_draw_point_array( m_owner );
}

#endif // wxUSE_SPLINES



void wxDCImpl::DoGradientFillLinear(const wxRect& rect,
                                    const wxColour& initialColour,
                                    const wxColour& destColour,
                                    wxDirection nDirection)
{
    // save old pen
    wxPen oldPen = m_pen;
    wxBrush oldBrush = m_brush;

    wxUint8 nR1 = initialColour.Red();
    wxUint8 nG1 = initialColour.Green();
    wxUint8 nB1 = initialColour.Blue();
    wxUint8 nR2 = destColour.Red();
    wxUint8 nG2 = destColour.Green();
    wxUint8 nB2 = destColour.Blue();
    wxUint8 nR, nG, nB;

    if ( nDirection == wxEAST || nDirection == wxWEST )
    {
        wxInt32 x = rect.GetWidth();
        wxInt32 w = x;              // width of area to shade
        wxInt32 xDelta = w/256;     // height of one shade bend
        if (xDelta < 1)
            xDelta = 1;

        while (x >= xDelta)
        {
            x -= xDelta;
            if (nR1 > nR2)
                nR = nR1 - (nR1-nR2)*(w-x)/w;
            else
                nR = nR1 + (nR2-nR1)*(w-x)/w;

            if (nG1 > nG2)
                nG = nG1 - (nG1-nG2)*(w-x)/w;
            else
                nG = nG1 + (nG2-nG1)*(w-x)/w;

            if (nB1 > nB2)
                nB = nB1 - (nB1-nB2)*(w-x)/w;
            else
                nB = nB1 + (nB2-nB1)*(w-x)/w;

            wxColour colour(nR,nG,nB);
            SetPen(wxPen(colour, 1, wxPENSTYLE_SOLID));
            SetBrush(wxBrush(colour));
            if(nDirection == wxEAST)
                DoDrawRectangle(rect.GetRight()-x-xDelta+1, rect.GetTop(),
                        xDelta, rect.GetHeight());
            else //nDirection == wxWEST
                DoDrawRectangle(rect.GetLeft()+x, rect.GetTop(),
                        xDelta, rect.GetHeight());
        }
    }
    else  // nDirection == wxNORTH || nDirection == wxSOUTH
    {
        wxInt32 y = rect.GetHeight();
        wxInt32 w = y;              // height of area to shade
        wxInt32 yDelta = w/255;     // height of one shade bend
        if (yDelta < 1)
            yDelta = 1;

        while (y > 0)
        {
            y -= yDelta;
            if (nR1 > nR2)
                nR = nR1 - (nR1-nR2)*(w-y)/w;
            else
                nR = nR1 + (nR2-nR1)*(w-y)/w;

            if (nG1 > nG2)
                nG = nG1 - (nG1-nG2)*(w-y)/w;
            else
                nG = nG1 + (nG2-nG1)*(w-y)/w;

            if (nB1 > nB2)
                nB = nB1 - (nB1-nB2)*(w-y)/w;
            else
                nB = nB1 + (nB2-nB1)*(w-y)/w;

            wxColour colour(nR,nG,nB);
            SetPen(wxPen(colour, 1, wxPENSTYLE_SOLID));
            SetBrush(wxBrush(colour));
            if(nDirection == wxNORTH)
                DoDrawRectangle(rect.GetLeft(), rect.GetTop()+y,
                        rect.GetWidth(), yDelta);
            else //nDirection == wxSOUTH
                DoDrawRectangle(rect.GetLeft(), rect.GetBottom()-y-yDelta+1,
                        rect.GetWidth(), yDelta);
        }
    }

    SetPen(oldPen);
    SetBrush(oldBrush);
}

void wxDCImpl::DoGradientFillConcentric(const wxRect& rect,
                                      const wxColour& initialColour,
                                      const wxColour& destColour,
                                      const wxPoint& circleCenter)
{
    // save the old pen and ensure it is restored on exit
    const wxPen penOrig = m_pen;
    wxON_BLOCK_EXIT_SET(m_pen, penOrig);

    wxUint8 nR1 = destColour.Red();
    wxUint8 nG1 = destColour.Green();
    wxUint8 nB1 = destColour.Blue();
    wxUint8 nR2 = initialColour.Red();
    wxUint8 nG2 = initialColour.Green();
    wxUint8 nB2 = initialColour.Blue();
    wxUint8 nR, nG, nB;


    //Radius
    double cx = rect.GetWidth() / 2;
    double cy = rect.GetHeight() / 2;
    double dRadius;
    if (cx < cy)
        dRadius = cx;
    else
        dRadius = cy;

    //Offset of circle
    double ptX, ptY;
    ptX = circleCenter.x;
    ptY = circleCenter.y;
    double nCircleOffX = ptX - cx;
    double nCircleOffY = ptY - cy;

    double dGradient;
    double dx, dy;

    for ( wxInt32 x = 0; x < rect.GetWidth(); x++ )
    {
        for ( wxInt32 y = 0; y < rect.GetHeight(); y++ )
        {
            //get color difference
            dx = x;
            dy = y;

            dGradient = ((dRadius - sqrt(  (dx - cx - nCircleOffX) * (dx - cx - nCircleOffX)
                                          +(dy - cy - nCircleOffY) * (dy - cy - nCircleOffY)
                                         )
                         ) * 100
                        ) / dRadius;

            //normalize Gradient
            if (dGradient < 0)
                dGradient = 0.0;

            //get dest colors
            nR = (wxUint8)(nR1 + ((nR2 - nR1) * dGradient / 100));
            nG = (wxUint8)(nG1 + ((nG2 - nG1) * dGradient / 100));
            nB = (wxUint8)(nB1 + ((nB2 - nB1) * dGradient / 100));

            //set the pixel
            SetPen(wxColour(nR,nG,nB));
            DoDrawPoint(x + rect.GetLeft(), y + rect.GetTop());
        }
    }
}

void wxDCImpl::InheritAttributes(wxWindow *win)
{
    wxCHECK_RET( win, "window can't be NULL" );

    SetFont(win->GetFont());
    SetTextForeground(win->GetForegroundColour());
    SetTextBackground(win->GetBackgroundColour());
    SetBackground(win->GetBackgroundColour());
    SetLayoutDirection(win->GetLayoutDirection());
}

void wxDCImpl::DoGetFontMetrics(int *height,
                                int *ascent,
                                int *descent,
                                int *internalLeading,
                                int *externalLeading,
                                int *averageWidth) const
{
    // Average width is typically the same as width of 'x'.
    wxCoord h, d;
    DoGetTextExtent("x", averageWidth, &h, &d, externalLeading);

    if ( height )
        *height = h;
    if ( ascent )
        *ascent = h - d;
    if ( descent )
        *descent = d;
    if ( internalLeading )
        *internalLeading = 0;
}

//-----------------------------------------------------------------------------
// wxDC
//-----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxDC, wxObject)

void wxDC::CopyAttributes(const wxDC& dc)
{
    SetFont(dc.GetFont());
    SetTextForeground(dc.GetTextForeground());
    SetTextBackground(dc.GetTextBackground());
    SetBackground(dc.GetBackground());
    SetLayoutDirection(dc.GetLayoutDirection());
}

void wxDC::DrawLabel(const wxString& text,
                         const wxBitmap& bitmap,
                         const wxRect& rect,
                         int alignment,
                         int indexAccel,
                         wxRect *rectBounding)
{
    // find the text position
    wxCoord widthText, heightText, heightLine;
    GetMultiLineTextExtent(text, &widthText, &heightText, &heightLine);

    wxCoord width, height;
    if ( bitmap.IsOk() )
    {
        width = widthText + bitmap.GetWidth();
        height = bitmap.GetHeight();
    }
    else // no bitmap
    {
        width = widthText;
        height = heightText;
    }

    wxCoord x, y;
    if ( alignment & wxALIGN_RIGHT )
    {
        x = rect.GetRight() - width;
    }
    else if ( alignment & wxALIGN_CENTRE_HORIZONTAL )
    {
        x = (rect.GetLeft() + rect.GetRight() + 1 - width) / 2;
    }
    else // alignment & wxALIGN_LEFT
    {
        x = rect.GetLeft();
    }

    if ( alignment & wxALIGN_BOTTOM )
    {
        y = rect.GetBottom() - height;
    }
    else if ( alignment & wxALIGN_CENTRE_VERTICAL )
    {
        y = (rect.GetTop() + rect.GetBottom() + 1 - height) / 2;
    }
    else // alignment & wxALIGN_TOP
    {
        y = rect.GetTop();
    }

    // draw the bitmap first
    wxCoord x0 = x,
            y0 = y,
            width0 = width;
    if ( bitmap.IsOk() )
    {
        DrawBitmap(bitmap, x, y, true /* use mask */);

        wxCoord offset = bitmap.GetWidth() + 4;
        x += offset;
        width -= offset;

        y += (height - heightText) / 2;
    }

    // we will draw the underscore under the accel char later
    wxCoord startUnderscore = 0,
            endUnderscore = 0,
            yUnderscore = 0;

    // split the string into lines and draw each of them separately
    //
    // NB: while wxDC::DrawText() on some platforms supports drawing multi-line
    //     strings natively, this is not the case for all of them, notably not
    //     wxMSW which uses this function for multi-line texts, so we may only
    //     call DrawText() for single-line strings from here to avoid infinite
    //     recursion.
    wxString curLine;
    for ( wxString::const_iterator pc = text.begin(); ; ++pc )
    {
        if ( pc == text.end() || *pc == '\n' )
        {
            int xRealStart = x; // init it here to avoid compielr warnings

            if ( !curLine.empty() )
            {
                // NB: can't test for !(alignment & wxALIGN_LEFT) because
                //     wxALIGN_LEFT is 0
                if ( alignment & (wxALIGN_RIGHT | wxALIGN_CENTRE_HORIZONTAL) )
                {
                    wxCoord widthLine;
                    GetTextExtent(curLine, &widthLine, NULL);

                    if ( alignment & wxALIGN_RIGHT )
                    {
                        xRealStart += width - widthLine;
                    }
                    else // if ( alignment & wxALIGN_CENTRE_HORIZONTAL )
                    {
                        xRealStart += (width - widthLine) / 2;
                    }
                }
                //else: left aligned, nothing to do

                DrawText(curLine, xRealStart, y);
            }

            y += heightLine;

            // do we have underscore in this line? we can check yUnderscore
            // because it is set below to just y + heightLine if we do
            if ( y == yUnderscore )
            {
                // adjust the horz positions to account for the shift
                startUnderscore += xRealStart;
                endUnderscore += xRealStart;
            }

            if ( pc == text.end() )
                break;

            curLine.clear();
        }
        else // not end of line
        {
            if ( pc - text.begin() == indexAccel )
            {
                // remember to draw underscore here
                GetTextExtent(curLine, &startUnderscore, NULL);
                curLine += *pc;
                GetTextExtent(curLine, &endUnderscore, NULL);

                yUnderscore = y + heightLine;
            }
            else
            {
                curLine += *pc;
            }
        }
    }

    // draw the underscore if found
    if ( startUnderscore != endUnderscore )
    {
        // it should be of the same colour as text
        SetPen(wxPen(GetTextForeground(), 0, wxPENSTYLE_SOLID));

        // This adjustment is relatively arbitrary: we need to draw the
        // underline slightly higher to avoid overflowing the character cell
        // but whether we should do it 1, 2 or 3 pixels higher is not clear.
        //
        // The currently used value seems to be compatible with native MSW
        // behaviour, i.e. it results in the same appearance of the owner-drawn
        // and normal labels.
        yUnderscore -= 2;

        DrawLine(startUnderscore, yUnderscore, endUnderscore, yUnderscore);
    }

    // return bounding rect if requested
    if ( rectBounding )
    {
        *rectBounding = wxRect(x, y - heightText, widthText, heightText);
    }

    CalcBoundingBox(x0, y0);
    CalcBoundingBox(x0 + width0, y0 + height);
}

#if WXWIN_COMPATIBILITY_2_8
    // for compatibility with the old code when wxCoord was long everywhere
void wxDC::GetTextExtent(const wxString& string,
                       long *x, long *y,
                       long *descent,
                       long *externalLeading,
                       const wxFont *theFont) const
    {
        wxCoord x2, y2, descent2, externalLeading2;
        m_pimpl->DoGetTextExtent(string, &x2, &y2,
                        &descent2, &externalLeading2,
                        theFont);
        if ( x )
            *x = x2;
        if ( y )
            *y = y2;
        if ( descent )
            *descent = descent2;
        if ( externalLeading )
            *externalLeading = externalLeading2;
    }

void wxDC::GetLogicalOrigin(long *x, long *y) const
    {
        wxCoord x2, y2;
        m_pimpl->DoGetLogicalOrigin(&x2, &y2);
        if ( x )
            *x = x2;
        if ( y )
            *y = y2;
    }

void wxDC::GetDeviceOrigin(long *x, long *y) const
    {
        wxCoord x2, y2;
        m_pimpl->DoGetDeviceOrigin(&x2, &y2);
        if ( x )
            *x = x2;
        if ( y )
            *y = y2;
    }

void wxDC::GetClippingBox(long *x, long *y, long *w, long *h) const
    {
        wxCoord xx,yy,ww,hh;
        m_pimpl->DoGetClippingBox(&xx, &yy, &ww, &hh);
        if (x) *x = xx;
        if (y) *y = yy;
        if (w) *w = ww;
        if (h) *h = hh;
    }

void wxDC::DrawObject(wxDrawObject* drawobject)
{
    drawobject->Draw(*this);
    CalcBoundingBox(drawobject->MinX(),drawobject->MinY());
    CalcBoundingBox(drawobject->MaxX(),drawobject->MaxY());
}

#endif  // WXWIN_COMPATIBILITY_2_8

/*
Notes for wxWidgets DrawEllipticArcRot(...)

wxDCBase::DrawEllipticArcRot(...) draws a rotated elliptic arc or an ellipse.
It uses wxDCBase::CalculateEllipticPoints(...) and wxDCBase::Rotate(...),
which are also new.

All methods are generic, so they can be implemented in wxDCBase.
DoDrawEllipticArcRot(...) is virtual, so it can be called from deeper
methods like (WinCE) wxDC::DoDrawArc(...).

CalculateEllipticPoints(...) fills a given list of wxPoints with some points
of an elliptic arc. The algorithm is pixel-based: In every row (in flat
parts) or every column (in steep parts) only one pixel is calculated.
Trigonometric calculation (sin, cos, tan, atan) is only done if the
starting angle is not equal to the ending angle. The calculation of the
pixels is done using simple arithmetic only and should perform not too
bad even on devices without floating point processor. I didn't test this yet.

Rotate(...) rotates a list of point pixel-based, you will see rounding errors.
For instance: an ellipse rotated 180 degrees is drawn
slightly different from the original.

The points are then moved to an array and used to draw a polyline and/or polygon
(with center added, the pie).
The result looks quite similar to the native ellipse, only e few pixels differ.

The performance on a desktop system (Athlon 1800, WinXP) is about 7 times
slower as DrawEllipse(...), which calls the native API.
An rotated ellipse outside the clipping region takes nearly the same time,
while an native ellipse outside takes nearly no time to draw.

If you draw an arc with this new method, you will see the starting and ending angles
are calculated properly.
If you use DrawEllipticArc(...), you will see they are only correct for circles
and not properly calculated for ellipses.

Peter Lenhard
p.lenhard@t-online.de
*/

#ifdef __WXWINCE__
void wxDCImpl::DoDrawEllipticArcRot( wxCoord x, wxCoord y,
                                     wxCoord w, wxCoord h,
                                     double sa, double ea, double angle )
{
    wxPointList list;

    CalculateEllipticPoints( &list, x, y, w, h, sa, ea );
    Rotate( &list, angle, wxPoint( x+w/2, y+h/2 ) );

    // Add center (for polygon/pie)
    list.Append( new wxPoint( x+w/2, y+h/2 ) );

    // copy list into array and delete list elements
    int n = list.GetCount();
    wxPoint *points = new wxPoint[n];
    int i = 0;
    wxPointList::compatibility_iterator node;
    for ( node = list.GetFirst(); node; node = node->GetNext(), i++ )
    {
        wxPoint *point = node->GetData();
        points[i].x = point->x;
        points[i].y = point->y;
        delete point;
    }

    // first draw the pie without pen, if necessary
    if( GetBrush() != *wxTRANSPARENT_BRUSH )
    {
        wxPen tempPen( GetPen() );
        SetPen( *wxTRANSPARENT_PEN );
        DoDrawPolygon( n, points, 0, 0 );
        SetPen( tempPen );
    }

    // then draw the arc without brush, if necessary
    if( GetPen() != *wxTRANSPARENT_PEN )
    {
        // without center
        DoDrawLines( n-1, points, 0, 0 );
    }

    delete [] points;

} // DrawEllipticArcRot

void wxDCImpl::Rotate( wxPointList* points, double angle, wxPoint center )
{
    if( angle != 0.0 )
    {
        double pi(M_PI);
        double dSinA = -sin(angle*2.0*pi/360.0);
        double dCosA = cos(angle*2.0*pi/360.0);
        wxPointList::compatibility_iterator node;
        for ( node = points->GetFirst(); node; node = node->GetNext() )
        {
            wxPoint* point = node->GetData();

            // transform coordinates, if necessary
            if( center.x ) point->x -= center.x;
            if( center.y ) point->y -= center.y;

            // calculate rotation, rounding simply by implicit cast to integer
            int xTemp = point->x * dCosA - point->y * dSinA;
            point->y = point->x * dSinA + point->y * dCosA;
            point->x = xTemp;

            // back transform coordinates, if necessary
            if( center.x ) point->x += center.x;
            if( center.y ) point->y += center.y;
        }
    }
}

void wxDCImpl::CalculateEllipticPoints( wxPointList* points,
                                        wxCoord xStart, wxCoord yStart,
                                        wxCoord w, wxCoord h,
                                        double sa, double ea )
{
    double pi = M_PI;
    double sar = 0;
    double ear = 0;
    int xsa = 0;
    int ysa = 0;
    int xea = 0;
    int yea = 0;
    int sq = 0;
    int eq = 0;
    bool bUseAngles = false;
    if( w<0 ) w = -w;
    if( h<0 ) h = -h;
    // half-axes
    wxCoord a = w/2;
    wxCoord b = h/2;
    // decrement 1 pixel if ellipse is smaller than 2*a, 2*b
    int decrX = 0;
    if( 2*a == w ) decrX = 1;
    int decrY = 0;
    if( 2*b == h ) decrY = 1;
    // center
    wxCoord xCenter = xStart + a;
    wxCoord yCenter = yStart + b;
    // calculate data for start and end, if necessary
    if( sa != ea )
    {
        bUseAngles = true;
        // normalisation of angles
        while( sa<0 ) sa += 360;
        while( ea<0 ) ea += 360;
        while( sa>=360 ) sa -= 360;
        while( ea>=360 ) ea -= 360;
        // calculate quadrant numbers
        if( sa > 270 ) sq = 3;
        else if( sa > 180 ) sq = 2;
        else if( sa > 90 ) sq = 1;
        if( ea > 270 ) eq = 3;
        else if( ea > 180 ) eq = 2;
        else if( ea > 90 ) eq = 1;
        sar = sa * pi / 180.0;
        ear = ea * pi / 180.0;
        // correct angle circle -> ellipse
        sar = atan( -a/(double)b * tan( sar ) );
        if ( sq == 1 || sq == 2 ) sar += pi;
        ear = atan( -a/(double)b * tan( ear ) );
        if ( eq == 1 || eq == 2 ) ear += pi;
        // coordinates of points
        xsa = xCenter + a * cos( sar );
        if( sq == 0 || sq == 3 ) xsa -= decrX;
        ysa = yCenter + b * sin( sar );
        if( sq == 2 || sq == 3 ) ysa -= decrY;
        xea = xCenter + a * cos( ear );
        if( eq == 0 || eq == 3 ) xea -= decrX;
        yea = yCenter + b * sin( ear );
        if( eq == 2 || eq == 3 ) yea -= decrY;
    } // if iUseAngles
    // calculate c1 = b^2, c2 = b^2/a^2 with a = w/2, b = h/2
    double c1 = b * b;
    double c2 = 2.0 / w;
    c2 *= c2;
    c2 *= c1;
    wxCoord x = 0;
    wxCoord y = b;
    long x2 = 1;
    long y2 = y*y;
    long y_old = 0;
    // Lists for quadrant 1 to 4
    wxPointList pointsarray[4];
    // Calculate points for first quadrant and set in all quadrants
    for( x = 0; x <= a; ++x )
    {
        x2 = x2+x+x-1;
        y_old = y;
        bool bNewPoint = false;
        while( y2 > c1 - c2 * x2 && y > 0 )
        {
            bNewPoint = true;
            y2 = y2-y-y+1;
            --y;
        }
        // old y now too big: set point with old y, old x
        if( bNewPoint && x>1)
        {
            int x1 = x - 1;
            // remove points on the same line
            pointsarray[0].Insert( new wxPoint( xCenter + x1 - decrX, yCenter - y_old ) );
            pointsarray[1].Append( new wxPoint( xCenter - x1, yCenter - y_old ) );
            pointsarray[2].Insert( new wxPoint( xCenter - x1, yCenter + y_old - decrY ) );
            pointsarray[3].Append( new wxPoint( xCenter + x1 - decrX, yCenter + y_old - decrY ) );
        } // set point
    } // calculate point

    // Starting and/or ending points for the quadrants, first quadrant gets both.
    pointsarray[0].Insert( new wxPoint( xCenter + a - decrX, yCenter ) );
    pointsarray[0].Append( new wxPoint( xCenter, yCenter - b ) );
    pointsarray[1].Append( new wxPoint( xCenter - a, yCenter ) );
    pointsarray[2].Append( new wxPoint( xCenter, yCenter + b - decrY ) );
    pointsarray[3].Append( new wxPoint( xCenter + a - decrX, yCenter ) );

    // copy quadrants in original list
    if( bUseAngles )
    {
        // Copy the right part of the points in the lists
        // and delete the wxPoints, because they do not leave this method.
        points->Append( new wxPoint( xsa, ysa ) );
        int q = sq;
        bool bStarted = false;
        bool bReady = false;
        bool bForceTurn = ( sq == eq && sa > ea );
        while( !bReady )
        {
            wxPointList::compatibility_iterator node;
            for( node = pointsarray[q].GetFirst(); node; node = node->GetNext() )
            {
                // once: go to starting point in start quadrant
                if( !bStarted &&
                    (
                      node->GetData()->x < xsa+1 && q <= 1
                      ||
                      node->GetData()->x > xsa-1 && q >= 2
                    )
                  )
                {
                    bStarted = true;
                }

                // copy point, if not at ending point
                if( bStarted )
                {
                    if( q != eq || bForceTurn
                        ||
                        ( (wxPoint*) node->GetData() )->x > xea+1 && q <= 1
                        ||
                        ( (wxPoint*) node->GetData() )->x < xea-1 && q >= 2
                      )
                    {
                        // copy point
                        wxPoint* pPoint = new wxPoint( *(node->GetData()) );
                        points->Append( pPoint );
                    }
                    else if( q == eq && !bForceTurn || node->GetData()->x == xea)
                    {
                        bReady = true;
                    }
                }
            } // for node
            ++q;
            if( q > 3 ) q = 0;
            bForceTurn = false;
            bStarted = true;
        } // while not bReady
        points->Append( new wxPoint( xea, yea ) );

        // delete points
        for( q = 0; q < 4; ++q )
        {
            wxPointList::compatibility_iterator node;
            for( node = pointsarray[q].GetFirst(); node; node = node->GetNext() )
            {
                wxPoint *p = node->GetData();
                delete p;
            }
        }
    }
    else
    {
        wxPointList::compatibility_iterator node;
        // copy whole ellipse, wxPoints will be deleted outside
        for( node = pointsarray[0].GetFirst(); node; node = node->GetNext() )
        {
            wxPoint *p = node->GetData();
            points->Append( p );
        }
        for( node = pointsarray[1].GetFirst(); node; node = node->GetNext() )
        {
            wxPoint *p = node->GetData();
            points->Append( p );
        }
        for( node = pointsarray[2].GetFirst(); node; node = node->GetNext() )
        {
            wxPoint *p = node->GetData();
            points->Append( p );
        }
        for( node = pointsarray[3].GetFirst(); node; node = node->GetNext() )
        {
            wxPoint *p = node->GetData();
            points->Append( p );
        }
    } // not iUseAngles
} // CalculateEllipticPoints

#endif // __WXWINCE__

float wxDCImpl::GetFontPointSizeAdjustment(float dpi)
{
    // wxMSW has long-standing bug where wxFont point size is interpreted as
    // "pixel size corresponding to given point size *on screen*". In other
    // words, on a typical 600dpi printer and a typical 96dpi screen, fonts
    // are ~6 times smaller when printing. Unfortunately, this bug is so severe
    // that *all* printing code has to account for it and consequently, other
    // ports need to emulate this bug too:
    const wxSize screenPPI = wxGetDisplayPPI();
    return float(screenPPI.y) / dpi;
}
