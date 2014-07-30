/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dc.h
// Purpose:     wxDC class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     05/25/99
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DC_H_BASE_
#define _WX_DC_H_BASE_

// ----------------------------------------------------------------------------
// headers which we must include here
// ----------------------------------------------------------------------------

#include "wx/object.h"          // the base class

#include "wx/intl.h"            // for wxLayoutDirection
#include "wx/cursor.h"          // we have member variables of these classes
#include "wx/font.h"            // so we can't do without them
#include "wx/colour.h"
#include "wx/bitmap.h"          // for wxNullBitmap
#include "wx/brush.h"
#include "wx/pen.h"
#include "wx/palette.h"
#include "wx/dynarray.h"
#include "wx/math.h"
#include "wx/image.h"
#include "wx/region.h"
#include "wx/affinematrix2d.h"

#define wxUSE_NEW_DC 1

class WXDLLIMPEXP_FWD_CORE wxDC;
class WXDLLIMPEXP_FWD_CORE wxClientDC;
class WXDLLIMPEXP_FWD_CORE wxPaintDC;
class WXDLLIMPEXP_FWD_CORE wxWindowDC;
class WXDLLIMPEXP_FWD_CORE wxScreenDC;
class WXDLLIMPEXP_FWD_CORE wxMemoryDC;
class WXDLLIMPEXP_FWD_CORE wxPrinterDC;
class WXDLLIMPEXP_FWD_CORE wxPrintData;

#if wxUSE_GRAPHICS_CONTEXT
class WXDLLIMPEXP_FWD_CORE wxGraphicsContext;
#endif

//  Logical ops
enum wxRasterOperationMode
{
    wxCLEAR,       // 0
    wxXOR,         // src XOR dst
    wxINVERT,      // NOT dst
    wxOR_REVERSE,  // src OR (NOT dst)
    wxAND_REVERSE, // src AND (NOT dst)
    wxCOPY,        // src
    wxAND,         // src AND dst
    wxAND_INVERT,  // (NOT src) AND dst
    wxNO_OP,       // dst
    wxNOR,         // (NOT src) AND (NOT dst)
    wxEQUIV,       // (NOT src) XOR dst
    wxSRC_INVERT,  // (NOT src)
    wxOR_INVERT,   // (NOT src) OR dst
    wxNAND,        // (NOT src) OR (NOT dst)
    wxOR,          // src OR dst
    wxSET          // 1
#if WXWIN_COMPATIBILITY_2_8
    ,wxROP_BLACK = wxCLEAR,
    wxBLIT_BLACKNESS = wxCLEAR,
    wxROP_XORPEN = wxXOR,
    wxBLIT_SRCINVERT = wxXOR,
    wxROP_NOT = wxINVERT,
    wxBLIT_DSTINVERT = wxINVERT,
    wxROP_MERGEPENNOT = wxOR_REVERSE,
    wxBLIT_00DD0228 = wxOR_REVERSE,
    wxROP_MASKPENNOT = wxAND_REVERSE,
    wxBLIT_SRCERASE = wxAND_REVERSE,
    wxROP_COPYPEN = wxCOPY,
    wxBLIT_SRCCOPY = wxCOPY,
    wxROP_MASKPEN = wxAND,
    wxBLIT_SRCAND = wxAND,
    wxROP_MASKNOTPEN = wxAND_INVERT,
    wxBLIT_00220326 = wxAND_INVERT,
    wxROP_NOP = wxNO_OP,
    wxBLIT_00AA0029 = wxNO_OP,
    wxROP_NOTMERGEPEN = wxNOR,
    wxBLIT_NOTSRCERASE = wxNOR,
    wxROP_NOTXORPEN = wxEQUIV,
    wxBLIT_00990066 = wxEQUIV,
    wxROP_NOTCOPYPEN = wxSRC_INVERT,
    wxBLIT_NOTSCRCOPY = wxSRC_INVERT,
    wxROP_MERGENOTPEN = wxOR_INVERT,
    wxBLIT_MERGEPAINT = wxOR_INVERT,
    wxROP_NOTMASKPEN = wxNAND,
    wxBLIT_007700E6 = wxNAND,
    wxROP_MERGEPEN = wxOR,
    wxBLIT_SRCPAINT = wxOR,
    wxROP_WHITE = wxSET,
    wxBLIT_WHITENESS = wxSET
#endif //WXWIN_COMPATIBILITY_2_8
};

//  Flood styles
enum wxFloodFillStyle
{
    wxFLOOD_SURFACE = 1,
    wxFLOOD_BORDER
};

//  Mapping modes
enum wxMappingMode
{
    wxMM_TEXT = 1,
    wxMM_METRIC,
    wxMM_LOMETRIC,
    wxMM_TWIPS,
    wxMM_POINTS
};

// Description of text characteristics.
struct wxFontMetrics
{
    wxFontMetrics()
    {
        height =
        ascent =
        descent =
        internalLeading =
        externalLeading =
        averageWidth = 0;
    }

    int height,             // Total character height.
        ascent,             // Part of the height above the baseline.
        descent,            // Part of the height below the baseline.
        internalLeading,    // Intra-line spacing.
        externalLeading,    // Inter-line spacing.
        averageWidth;       // Average font width, a.k.a. "x-width".
};

#if WXWIN_COMPATIBILITY_2_8

//-----------------------------------------------------------------------------
// wxDrawObject helper class
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDrawObject
{
public:
    wxDEPRECATED_CONSTRUCTOR(wxDrawObject)()
        : m_isBBoxValid(false)
        , m_minX(0), m_minY(0), m_maxX(0), m_maxY(0)
    { }

    virtual ~wxDrawObject() { }

    virtual void Draw(wxDC&) const { }

    virtual void CalcBoundingBox(wxCoord x, wxCoord y)
    {
      if ( m_isBBoxValid )
      {
         if ( x < m_minX ) m_minX = x;
         if ( y < m_minY ) m_minY = y;
         if ( x > m_maxX ) m_maxX = x;
         if ( y > m_maxY ) m_maxY = y;
      }
      else
      {
         m_isBBoxValid = true;

         m_minX = x;
         m_minY = y;
         m_maxX = x;
         m_maxY = y;
      }
    }

    void ResetBoundingBox()
    {
        m_isBBoxValid = false;

        m_minX = m_maxX = m_minY = m_maxY = 0;
    }

    // Get the final bounding box of the PostScript or Metafile picture.

    wxCoord MinX() const { return m_minX; }
    wxCoord MaxX() const { return m_maxX; }
    wxCoord MinY() const { return m_minY; }
    wxCoord MaxY() const { return m_maxY; }

    //to define the type of object for derived objects
    virtual int GetType()=0;

protected:
    //for boundingbox calculation
    bool m_isBBoxValid:1;
    //for boundingbox calculation
    wxCoord m_minX, m_minY, m_maxX, m_maxY;
};

#endif // WXWIN_COMPATIBILITY_2_8


//-----------------------------------------------------------------------------
// wxDCFactory
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxDCImpl;

class WXDLLIMPEXP_CORE wxDCFactory
{
public:
    wxDCFactory() {}
    virtual ~wxDCFactory() {}

    virtual wxDCImpl* CreateWindowDC( wxWindowDC *owner, wxWindow *window ) = 0;
    virtual wxDCImpl* CreateClientDC( wxClientDC *owner, wxWindow *window ) = 0;
    virtual wxDCImpl* CreatePaintDC( wxPaintDC *owner, wxWindow *window ) = 0;
    virtual wxDCImpl* CreateMemoryDC( wxMemoryDC *owner ) = 0;
    virtual wxDCImpl* CreateMemoryDC( wxMemoryDC *owner, wxBitmap &bitmap ) = 0;
    virtual wxDCImpl* CreateMemoryDC( wxMemoryDC *owner, wxDC *dc ) = 0;
    virtual wxDCImpl* CreateScreenDC( wxScreenDC *owner ) = 0;
#if wxUSE_PRINTING_ARCHITECTURE
    virtual wxDCImpl* CreatePrinterDC( wxPrinterDC *owner, const wxPrintData &data  ) = 0;
#endif

    static void Set(wxDCFactory *factory);
    static wxDCFactory *Get();

private:
    static wxDCFactory *m_factory;
};

//-----------------------------------------------------------------------------
// wxNativeDCFactory
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxNativeDCFactory: public wxDCFactory
{
public:
    wxNativeDCFactory() {}

    virtual wxDCImpl* CreateWindowDC( wxWindowDC *owner, wxWindow *window );
    virtual wxDCImpl* CreateClientDC( wxClientDC *owner, wxWindow *window );
    virtual wxDCImpl* CreatePaintDC( wxPaintDC *owner, wxWindow *window );
    virtual wxDCImpl* CreateMemoryDC( wxMemoryDC *owner );
    virtual wxDCImpl* CreateMemoryDC( wxMemoryDC *owner, wxBitmap &bitmap );
    virtual wxDCImpl* CreateMemoryDC( wxMemoryDC *owner, wxDC *dc );
    virtual wxDCImpl* CreateScreenDC( wxScreenDC *owner );
#if wxUSE_PRINTING_ARCHITECTURE
    virtual wxDCImpl* CreatePrinterDC( wxPrinterDC *owner, const wxPrintData &data  );
#endif
};

//-----------------------------------------------------------------------------
// wxDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDCImpl: public wxObject
{
public:
    wxDCImpl( wxDC *owner );
    virtual ~wxDCImpl();

    wxDC *GetOwner() const { return m_owner; }

    wxWindow* GetWindow() const { return m_window; }

    virtual bool IsOk() const { return m_ok; }

    // query capabilities

    virtual bool CanDrawBitmap() const = 0;
    virtual bool CanGetTextExtent() const = 0;

    // get Cairo context
    virtual void* GetCairoContext() const
    {
        return NULL;
    }

    virtual void* GetHandle() const { return NULL; }
    
    // query dimension, colour deps, resolution

    virtual void DoGetSize(int *width, int *height) const = 0;
    void GetSize(int *width, int *height) const
    {
        DoGetSize(width, height);
        return ;
    }

    wxSize GetSize() const
    {
        int w, h;
        DoGetSize(&w, &h);
        return wxSize(w, h);
    }

    virtual void DoGetSizeMM(int* width, int* height) const = 0;

    virtual int GetDepth() const = 0;
    virtual wxSize GetPPI() const = 0;

    // Right-To-Left (RTL) modes

    virtual void SetLayoutDirection(wxLayoutDirection WXUNUSED(dir)) { }
    virtual wxLayoutDirection GetLayoutDirection() const  { return wxLayout_Default; }

    // page and document

    virtual bool StartDoc(const wxString& WXUNUSED(message)) { return true; }
    virtual void EndDoc() { }

    virtual void StartPage() { }
    virtual void EndPage() { }

    // flushing the content of this dc immediately eg onto screen
    virtual void Flush() { }

    // bounding box

    virtual void CalcBoundingBox(wxCoord x, wxCoord y)
    {
      if ( m_isBBoxValid )
      {
         if ( x < m_minX ) m_minX = x;
         if ( y < m_minY ) m_minY = y;
         if ( x > m_maxX ) m_maxX = x;
         if ( y > m_maxY ) m_maxY = y;
      }
      else
      {
         m_isBBoxValid = true;

         m_minX = x;
         m_minY = y;
         m_maxX = x;
         m_maxY = y;
      }
    }
    void ResetBoundingBox()
    {
        m_isBBoxValid = false;

        m_minX = m_maxX = m_minY = m_maxY = 0;
    }

    wxCoord MinX() const { return m_minX; }
    wxCoord MaxX() const { return m_maxX; }
    wxCoord MinY() const { return m_minY; }
    wxCoord MaxY() const { return m_maxY; }

    // setters and getters

    virtual void SetFont(const wxFont& font) = 0;
    virtual const wxFont& GetFont() const { return m_font; }

    virtual void SetPen(const wxPen& pen) = 0;
    virtual const wxPen& GetPen() const { return m_pen; }

    virtual void SetBrush(const wxBrush& brush) = 0;
    virtual const wxBrush& GetBrush() const { return m_brush; }

    virtual void SetBackground(const wxBrush& brush) = 0;
    virtual const wxBrush& GetBackground() const { return m_backgroundBrush; }

    virtual void SetBackgroundMode(int mode) = 0;
    virtual int GetBackgroundMode() const { return m_backgroundMode; }

    virtual void SetTextForeground(const wxColour& colour)
        { m_textForegroundColour = colour; }
    virtual const wxColour& GetTextForeground() const
        { return m_textForegroundColour; }

    virtual void SetTextBackground(const wxColour& colour)
        { m_textBackgroundColour = colour; }
    virtual const wxColour& GetTextBackground() const
        { return m_textBackgroundColour; }

#if wxUSE_PALETTE
    virtual void SetPalette(const wxPalette& palette) = 0;
#endif // wxUSE_PALETTE

    // inherit the DC attributes (font and colours) from the given window
    //
    // this is called automatically when a window, client or paint DC is
    // created
    virtual void InheritAttributes(wxWindow *win);


    // logical functions

    virtual void SetLogicalFunction(wxRasterOperationMode function) = 0;
    virtual wxRasterOperationMode GetLogicalFunction() const
                                      { return m_logicalFunction; }

    // text measurement

    virtual wxCoord GetCharHeight() const = 0;
    virtual wxCoord GetCharWidth() const = 0;

    // The derived classes should really override DoGetFontMetrics() to return
    // the correct values in the future but for now provide a default
    // implementation in terms of DoGetTextExtent() to avoid breaking the
    // compilation of all other ports as wxMSW is the only one to implement it.
    virtual void DoGetFontMetrics(int *height,
                                  int *ascent,
                                  int *descent,
                                  int *internalLeading,
                                  int *externalLeading,
                                  int *averageWidth) const;

    virtual void DoGetTextExtent(const wxString& string,
                                 wxCoord *x, wxCoord *y,
                                 wxCoord *descent = NULL,
                                 wxCoord *externalLeading = NULL,
                                 const wxFont *theFont = NULL) const = 0;
    virtual void GetMultiLineTextExtent(const wxString& string,
                                        wxCoord *width,
                                        wxCoord *height,
                                        wxCoord *heightLine = NULL,
                                        const wxFont *font = NULL) const;
    virtual bool DoGetPartialTextExtents(const wxString& text, wxArrayInt& widths) const;

    // clearing

    virtual void Clear() = 0;

    // clipping

    // Note that this pure virtual method has an implementation that updates
    // the values returned by DoGetClippingBox() and so can be called from the
    // derived class overridden version if it makes sense (i.e. if the clipping
    // box coordinates are not already updated in some other way).
    virtual void DoSetClippingRegion(wxCoord x, wxCoord y,
                                     wxCoord w, wxCoord h) = 0;

    // NB: this function works with device coordinates, not the logical ones!
    virtual void DoSetDeviceClippingRegion(const wxRegion& region) = 0;

    virtual void DoGetClippingBox(wxCoord *x, wxCoord *y,
                                  wxCoord *w, wxCoord *h) const
    {
        if ( x )
            *x = m_clipX1;
        if ( y )
            *y = m_clipY1;
        if ( w )
            *w = m_clipX2 - m_clipX1;
        if ( h )
            *h = m_clipY2 - m_clipY1;
    }

    virtual void DestroyClippingRegion() { ResetClipping(); }


    // coordinates conversions and transforms

    virtual wxCoord DeviceToLogicalX(wxCoord x) const;
    virtual wxCoord DeviceToLogicalY(wxCoord y) const;
    virtual wxCoord DeviceToLogicalXRel(wxCoord x) const;
    virtual wxCoord DeviceToLogicalYRel(wxCoord y) const;
    virtual wxCoord LogicalToDeviceX(wxCoord x) const;
    virtual wxCoord LogicalToDeviceY(wxCoord y) const;
    virtual wxCoord LogicalToDeviceXRel(wxCoord x) const;
    virtual wxCoord LogicalToDeviceYRel(wxCoord y) const;

    virtual void SetMapMode(wxMappingMode mode);
    virtual wxMappingMode GetMapMode() const { return m_mappingMode; }

    virtual void SetUserScale(double x, double y);
    virtual void GetUserScale(double *x, double *y) const
    {
        if ( x ) *x = m_userScaleX;
        if ( y ) *y = m_userScaleY;
    }

    virtual void SetLogicalScale(double x, double y);
    virtual void GetLogicalScale(double *x, double *y) const
    {
        if ( x ) *x = m_logicalScaleX;
        if ( y ) *y = m_logicalScaleY;
    }

    virtual void SetLogicalOrigin(wxCoord x, wxCoord y);
    virtual void DoGetLogicalOrigin(wxCoord *x, wxCoord *y) const
    {
        if ( x ) *x = m_logicalOriginX;
        if ( y ) *y = m_logicalOriginY;
    }

    virtual void SetDeviceOrigin(wxCoord x, wxCoord y);
    virtual void DoGetDeviceOrigin(wxCoord *x, wxCoord *y) const
    {
        if ( x ) *x = m_deviceOriginX;
        if ( y ) *y = m_deviceOriginY;
    }

#if wxUSE_DC_TRANSFORM_MATRIX
    // Transform matrix support is not available in most ports right now
    // (currently only wxMSW provides it) so do nothing in these methods by
    // default.
    virtual bool CanUseTransformMatrix() const
        { return false; }
    virtual bool SetTransformMatrix(const wxAffineMatrix2D& WXUNUSED(matrix))
        { return false; }
    virtual wxAffineMatrix2D GetTransformMatrix() const
        { return wxAffineMatrix2D(); }
    virtual void ResetTransformMatrix()
        { }
#endif // wxUSE_DC_TRANSFORM_MATRIX

    virtual void SetDeviceLocalOrigin( wxCoord x, wxCoord y );

    virtual void ComputeScaleAndOrigin();

    // this needs to overidden if the axis is inverted
    virtual void SetAxisOrientation(bool xLeftRight, bool yBottomUp);
    
    virtual double GetContentScaleFactor() const { return m_contentScaleFactor; }

#ifdef __WXMSW__
    // Native Windows functions using the underlying HDC don't honour GDI+
    // transformations which may be applied to it. Using this function we can
    // transform the coordinates manually before passing them to such functions
    // (as in e.g. wxRendererMSW code). It doesn't do anything if this is not a
    // wxGCDC.
    virtual wxRect MSWApplyGDIPlusTransform(const wxRect& r) const
    {
        return r;
    }
#endif // __WXMSW__


    // ---------------------------------------------------------
    // the actual drawing API

    virtual bool DoFloodFill(wxCoord x, wxCoord y, const wxColour& col,
                             wxFloodFillStyle style = wxFLOOD_SURFACE) = 0;

    virtual void DoGradientFillLinear(const wxRect& rect,
                                      const wxColour& initialColour,
                                      const wxColour& destColour,
                                      wxDirection nDirection = wxEAST);

    virtual void DoGradientFillConcentric(const wxRect& rect,
                                        const wxColour& initialColour,
                                        const wxColour& destColour,
                                        const wxPoint& circleCenter);

    virtual bool DoGetPixel(wxCoord x, wxCoord y, wxColour *col) const = 0;

    virtual void DoDrawPoint(wxCoord x, wxCoord y) = 0;
    virtual void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2) = 0;

    virtual void DoDrawArc(wxCoord x1, wxCoord y1,
                           wxCoord x2, wxCoord y2,
                           wxCoord xc, wxCoord yc) = 0;
    virtual void DoDrawCheckMark(wxCoord x, wxCoord y,
                                 wxCoord width, wxCoord height);
    virtual void DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                                   double sa, double ea) = 0;

    virtual void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height) = 0;
    virtual void DoDrawRoundedRectangle(wxCoord x, wxCoord y,
                                        wxCoord width, wxCoord height,
                                        double radius) = 0;
    virtual void DoDrawEllipse(wxCoord x, wxCoord y,
                               wxCoord width, wxCoord height) = 0;

    virtual void DoCrossHair(wxCoord x, wxCoord y) = 0;

    virtual void DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y) = 0;
    virtual void DoDrawBitmap(const wxBitmap &bmp, wxCoord x, wxCoord y,
                              bool useMask = false) = 0;

    virtual void DoDrawText(const wxString& text, wxCoord x, wxCoord y) = 0;
    virtual void DoDrawRotatedText(const wxString& text,
                                   wxCoord x, wxCoord y, double angle) = 0;

    virtual bool DoBlit(wxCoord xdest, wxCoord ydest,
                        wxCoord width, wxCoord height,
                        wxDC *source,
                        wxCoord xsrc, wxCoord ysrc,
                        wxRasterOperationMode rop = wxCOPY,
                        bool useMask = false,
                        wxCoord xsrcMask = wxDefaultCoord,
                        wxCoord ysrcMask = wxDefaultCoord) = 0;

    virtual bool DoStretchBlit(wxCoord xdest, wxCoord ydest,
                               wxCoord dstWidth, wxCoord dstHeight,
                               wxDC *source,
                               wxCoord xsrc, wxCoord ysrc,
                               wxCoord srcWidth, wxCoord srcHeight,
                               wxRasterOperationMode rop = wxCOPY,
                               bool useMask = false,
                               wxCoord xsrcMask = wxDefaultCoord,
                               wxCoord ysrcMask = wxDefaultCoord);

    virtual wxBitmap DoGetAsBitmap(const wxRect *WXUNUSED(subrect)) const
        { return wxNullBitmap; }


    virtual void DoDrawLines(int n, const wxPoint points[],
                             wxCoord xoffset, wxCoord yoffset ) = 0;
    virtual void DrawLines(const wxPointList *list,
                           wxCoord xoffset, wxCoord yoffset );

    virtual void DoDrawPolygon(int n, const wxPoint points[],
                           wxCoord xoffset, wxCoord yoffset,
                           wxPolygonFillMode fillStyle = wxODDEVEN_RULE) = 0;
    virtual void DoDrawPolyPolygon(int n, const int count[], const wxPoint points[],
                               wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle);
    void DrawPolygon(const wxPointList *list,
                     wxCoord xoffset, wxCoord yoffset,
                     wxPolygonFillMode fillStyle );


#if wxUSE_SPLINES
    void DrawSpline(wxCoord x1, wxCoord y1,
                            wxCoord x2, wxCoord y2,
                            wxCoord x3, wxCoord y3);
    void DrawSpline(int n, const wxPoint points[]);
    void DrawSpline(const wxPointList *points) { DoDrawSpline(points); }

    virtual void DoDrawSpline(const wxPointList *points);
#endif

    // ---------------------------------------------------------
    // wxMemoryDC Impl API

    virtual void DoSelect(const wxBitmap& WXUNUSED(bmp))
       { }

    virtual const wxBitmap& GetSelectedBitmap() const
        { return wxNullBitmap; }
    virtual wxBitmap& GetSelectedBitmap()
        { return wxNullBitmap; }

    // ---------------------------------------------------------
    // wxPrinterDC Impl API

    virtual wxRect GetPaperRect() const
        { int w = 0; int h = 0; DoGetSize( &w, &h ); return wxRect(0,0,w,h); }

    virtual int GetResolution() const
        { return -1; }

#if wxUSE_GRAPHICS_CONTEXT
    virtual wxGraphicsContext* GetGraphicsContext() const
        { return NULL; }
    virtual void SetGraphicsContext( wxGraphicsContext* WXUNUSED(ctx) )
        {}
#endif

private:
    wxDC       *m_owner;

protected:
    // unset clipping variables (after clipping region was destroyed)
    void ResetClipping()
    {
        m_clipping = false;

        m_clipX1 = m_clipX2 = m_clipY1 = m_clipY2 = 0;
    }

#ifdef __WXWINCE__
    //! Generic method to draw ellipses, circles and arcs with current pen and brush.
    /*! \param x Upper left corner of bounding box.
     *  \param y Upper left corner of bounding box.
     *  \param w Width of bounding box.
     *  \param h Height of bounding box.
     *  \param sa Starting angle of arc
     *            (counterclockwise, start at 3 o'clock, 360 is full circle).
     *  \param ea Ending angle of arc.
     *  \param angle Rotation angle, the Arc will be rotated after
     *               calculating begin and end.
     */
    void DrawEllipticArcRot( wxCoord x, wxCoord y,
                             wxCoord width, wxCoord height,
                             double sa = 0, double ea = 0, double angle = 0 )
    { DoDrawEllipticArcRot( x, y, width, height, sa, ea, angle ); }

    void DrawEllipticArcRot( const wxPoint& pt,
                             const wxSize& sz,
                             double sa = 0, double ea = 0, double angle = 0 )
    { DoDrawEllipticArcRot( pt.x, pt.y, sz.x, sz.y, sa, ea, angle ); }

    void DrawEllipticArcRot( const wxRect& rect,
                             double sa = 0, double ea = 0, double angle = 0 )
    { DoDrawEllipticArcRot( rect.x, rect.y, rect.width, rect.height, sa, ea, angle ); }

    virtual void DoDrawEllipticArcRot( wxCoord x, wxCoord y,
                                       wxCoord w, wxCoord h,
                                       double sa = 0, double ea = 0, double angle = 0 );

    //! Rotates points around center.
    /*! This is a quite straight method, it calculates in pixels
     *  and so it produces rounding errors.
     *  \param points The points inside will be rotated.
     *  \param angle Rotating angle (counterclockwise, start at 3 o'clock, 360 is full circle).
     *  \param center Center of rotation.
     */
    void Rotate( wxPointList* points, double angle, wxPoint center = wxPoint(0,0) );

    // used by DrawEllipticArcRot
    // Careful: wxList gets filled with points you have to delete later.
    void CalculateEllipticPoints( wxPointList* points,
                                  wxCoord xStart, wxCoord yStart,
                                  wxCoord w, wxCoord h,
                                  double sa, double ea );
#endif // __WXWINCE__

    // returns adjustment factor for converting wxFont "point size"; in wx
    // it is point size on screen and needs to be multiplied by this value
    // for rendering on higher-resolution DCs such as printer ones
    static float GetFontPointSizeAdjustment(float dpi);

    // window on which the DC draws or NULL
    wxWindow   *m_window;

    // flags
    bool m_colour:1;
    bool m_ok:1;
    bool m_clipping:1;
    bool m_isInteractive:1;
    bool m_isBBoxValid:1;

    // coordinate system variables

    wxCoord m_logicalOriginX, m_logicalOriginY;
    wxCoord m_deviceOriginX, m_deviceOriginY;           // Usually 0,0, can be change by user

    wxCoord m_deviceLocalOriginX, m_deviceLocalOriginY; // non-zero if native top-left corner
                                                        // is not at 0,0. This was the case under
                                                        // Mac's GrafPorts (coordinate system
                                                        // used toplevel window's origin) and
                                                        // e.g. for Postscript, where the native
                                                        // origin in the bottom left corner.
    double m_logicalScaleX, m_logicalScaleY;
    double m_userScaleX, m_userScaleY;
    double m_scaleX, m_scaleY;  // calculated from logical scale and user scale

    int m_signX, m_signY;  // Used by SetAxisOrientation() to invert the axes
    
    double m_contentScaleFactor; // used by high resolution displays (retina)

    // what is a mm on a screen you don't know the size of?
    double       m_mm_to_pix_x,
                 m_mm_to_pix_y;

    // bounding and clipping boxes
    wxCoord m_minX, m_minY, m_maxX, m_maxY;
    wxCoord m_clipX1, m_clipY1, m_clipX2, m_clipY2;

    wxRasterOperationMode m_logicalFunction;
    int m_backgroundMode;
    wxMappingMode m_mappingMode;

    wxPen             m_pen;
    wxBrush           m_brush;
    wxBrush           m_backgroundBrush;
    wxColour          m_textForegroundColour;
    wxColour          m_textBackgroundColour;
    wxFont            m_font;

#if wxUSE_PALETTE
    wxPalette         m_palette;
    bool              m_hasCustomPalette;
#endif // wxUSE_PALETTE

private:
    DECLARE_ABSTRACT_CLASS(wxDCImpl)
};


class WXDLLIMPEXP_CORE wxDC : public wxObject
{
public:
    // copy attributes (font, colours and writing direction) from another DC
    void CopyAttributes(const wxDC& dc);

    virtual ~wxDC() { delete m_pimpl; }

    wxDCImpl *GetImpl()
        { return m_pimpl; }
    const wxDCImpl *GetImpl() const
        { return m_pimpl; }

    wxWindow *GetWindow() const
        { return m_pimpl->GetWindow(); }

    void *GetHandle() const
        { return m_pimpl->GetHandle(); }

    bool IsOk() const
        { return m_pimpl && m_pimpl->IsOk(); }

    // query capabilities

    bool CanDrawBitmap() const
        { return m_pimpl->CanDrawBitmap(); }
    bool CanGetTextExtent() const
        { return m_pimpl->CanGetTextExtent(); }

    // query dimension, colour deps, resolution

    void GetSize(int *width, int *height) const
        { m_pimpl->DoGetSize(width, height); }
    wxSize GetSize() const
        { return m_pimpl->GetSize(); }

    void GetSizeMM(int* width, int* height) const
        { m_pimpl->DoGetSizeMM(width, height); }
    wxSize GetSizeMM() const
    {
        int w, h;
        m_pimpl->DoGetSizeMM(&w, &h);
        return wxSize(w, h);
    }

    int GetDepth() const
        { return m_pimpl->GetDepth(); }
    wxSize GetPPI() const
        { return m_pimpl->GetPPI(); }

    virtual int GetResolution() const
        { return m_pimpl->GetResolution(); }

    double GetContentScaleFactor() const
        { return m_pimpl->GetContentScaleFactor(); }

    // Right-To-Left (RTL) modes

    void SetLayoutDirection(wxLayoutDirection dir)
        { m_pimpl->SetLayoutDirection( dir ); }
    wxLayoutDirection GetLayoutDirection() const
        { return m_pimpl->GetLayoutDirection(); }

    // page and document

    bool StartDoc(const wxString& message)
        { return m_pimpl->StartDoc(message); }
    void EndDoc()
        { m_pimpl->EndDoc(); }

    void StartPage()
        { m_pimpl->StartPage(); }
    void EndPage()
        { m_pimpl->EndPage(); }

    // bounding box

    void CalcBoundingBox(wxCoord x, wxCoord y)
        { m_pimpl->CalcBoundingBox(x,y); }
    void ResetBoundingBox()
        { m_pimpl->ResetBoundingBox(); }

    wxCoord MinX() const
        { return m_pimpl->MinX(); }
    wxCoord MaxX() const
        { return m_pimpl->MaxX(); }
    wxCoord MinY() const
        { return m_pimpl->MinY(); }
    wxCoord MaxY() const
        { return m_pimpl->MaxY(); }

    // setters and getters

    void SetFont(const wxFont& font)
        { m_pimpl->SetFont( font ); }
    const wxFont&   GetFont() const
        { return m_pimpl->GetFont(); }

    void SetPen(const wxPen& pen)
        { m_pimpl->SetPen( pen ); }
    const wxPen&    GetPen() const
        { return m_pimpl->GetPen(); }

    void SetBrush(const wxBrush& brush)
        { m_pimpl->SetBrush( brush ); }
    const wxBrush&  GetBrush() const
        { return m_pimpl->GetBrush(); }

    void SetBackground(const wxBrush& brush)
        { m_pimpl->SetBackground( brush ); }
    const wxBrush&  GetBackground() const
        { return m_pimpl->GetBackground(); }

    void SetBackgroundMode(int mode)
        { m_pimpl->SetBackgroundMode( mode ); }
    int GetBackgroundMode() const
        { return m_pimpl->GetBackgroundMode(); }

    void SetTextForeground(const wxColour& colour)
        { m_pimpl->SetTextForeground(colour); }
    const wxColour& GetTextForeground() const
        { return m_pimpl->GetTextForeground(); }

    void SetTextBackground(const wxColour& colour)
        { m_pimpl->SetTextBackground(colour); }
    const wxColour& GetTextBackground() const
        { return m_pimpl->GetTextBackground(); }

#if wxUSE_PALETTE
    void SetPalette(const wxPalette& palette)
        { m_pimpl->SetPalette(palette); }
#endif // wxUSE_PALETTE

    // logical functions

    void SetLogicalFunction(wxRasterOperationMode function)
        { m_pimpl->SetLogicalFunction(function); }
    wxRasterOperationMode GetLogicalFunction() const
        { return m_pimpl->GetLogicalFunction(); }

    // text measurement

    wxCoord GetCharHeight() const
        { return m_pimpl->GetCharHeight(); }
    wxCoord GetCharWidth() const
        { return m_pimpl->GetCharWidth(); }

    wxFontMetrics GetFontMetrics() const
    {
        wxFontMetrics fm;
        m_pimpl->DoGetFontMetrics(&fm.height, &fm.ascent, &fm.descent,
                                  &fm.internalLeading, &fm.externalLeading,
                                  &fm.averageWidth);
        return fm;
    }

    void GetTextExtent(const wxString& string,
                       wxCoord *x, wxCoord *y,
                       wxCoord *descent = NULL,
                       wxCoord *externalLeading = NULL,
                       const wxFont *theFont = NULL) const
        { m_pimpl->DoGetTextExtent(string, x, y, descent, externalLeading, theFont); }

    wxSize GetTextExtent(const wxString& string) const
    {
        wxCoord w, h;
        m_pimpl->DoGetTextExtent(string, &w, &h);
        return wxSize(w, h);
    }

    void GetMultiLineTextExtent(const wxString& string,
                                        wxCoord *width,
                                        wxCoord *height,
                                        wxCoord *heightLine = NULL,
                                        const wxFont *font = NULL) const
        { m_pimpl->GetMultiLineTextExtent( string, width, height, heightLine, font ); }

    wxSize GetMultiLineTextExtent(const wxString& string) const
    {
        wxCoord w, h;
        m_pimpl->GetMultiLineTextExtent(string, &w, &h);
        return wxSize(w, h);
    }

    bool GetPartialTextExtents(const wxString& text, wxArrayInt& widths) const
        { return m_pimpl->DoGetPartialTextExtents(text, widths); }

    // clearing

    void Clear()
        { m_pimpl->Clear(); }

    // clipping

    void SetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
        { m_pimpl->DoSetClippingRegion(x, y, width, height); }
    void SetClippingRegion(const wxPoint& pt, const wxSize& sz)
        { m_pimpl->DoSetClippingRegion(pt.x, pt.y, sz.x, sz.y); }
    void SetClippingRegion(const wxRect& rect)
        { m_pimpl->DoSetClippingRegion(rect.x, rect.y, rect.width, rect.height); }

    // unlike the functions above, the coordinates of the region used in this
    // one are in device coordinates, not the logical ones
    void SetDeviceClippingRegion(const wxRegion& region)
        { m_pimpl->DoSetDeviceClippingRegion(region); }

    // this function is deprecated because its name is confusing: you may
    // expect it to work with logical coordinates but, in fact, it does exactly
    // the same thing as SetDeviceClippingRegion()
    //
    // please review the code using it and either replace it with calls to
    // SetDeviceClippingRegion() or correct it if it was [wrongly] passing
    // logical coordinates to this function
    wxDEPRECATED_INLINE(void SetClippingRegion(const wxRegion& region),
                        SetDeviceClippingRegion(region); )

    void DestroyClippingRegion()
        { m_pimpl->DestroyClippingRegion(); }

    void GetClippingBox(wxCoord *x, wxCoord *y, wxCoord *w, wxCoord *h) const
        { m_pimpl->DoGetClippingBox(x, y, w, h); }
    void GetClippingBox(wxRect& rect) const
        { m_pimpl->DoGetClippingBox(&rect.x, &rect.y, &rect.width, &rect.height); }

    // coordinates conversions and transforms

    wxCoord DeviceToLogicalX(wxCoord x) const
        { return m_pimpl->DeviceToLogicalX(x); }
    wxCoord DeviceToLogicalY(wxCoord y) const
        { return m_pimpl->DeviceToLogicalY(y); }
    wxCoord DeviceToLogicalXRel(wxCoord x) const
        { return m_pimpl->DeviceToLogicalXRel(x); }
    wxCoord DeviceToLogicalYRel(wxCoord y) const
        { return m_pimpl->DeviceToLogicalYRel(y); }
    wxCoord LogicalToDeviceX(wxCoord x) const
        { return m_pimpl->LogicalToDeviceX(x); }
    wxCoord LogicalToDeviceY(wxCoord y) const
        { return m_pimpl->LogicalToDeviceY(y); }
    wxCoord LogicalToDeviceXRel(wxCoord x) const
        { return m_pimpl->LogicalToDeviceXRel(x); }
    wxCoord LogicalToDeviceYRel(wxCoord y) const
        { return m_pimpl->LogicalToDeviceYRel(y); }

    void SetMapMode(wxMappingMode mode)
        { m_pimpl->SetMapMode(mode); }
    wxMappingMode GetMapMode() const
        { return m_pimpl->GetMapMode(); }

    void SetUserScale(double x, double y)
        { m_pimpl->SetUserScale(x,y); }
    void GetUserScale(double *x, double *y) const
        { m_pimpl->GetUserScale( x, y ); }

    void SetLogicalScale(double x, double y)
        { m_pimpl->SetLogicalScale( x, y ); }
    void GetLogicalScale(double *x, double *y) const
        { m_pimpl->GetLogicalScale( x, y ); }

    void SetLogicalOrigin(wxCoord x, wxCoord y)
        { m_pimpl->SetLogicalOrigin(x,y); }
    void GetLogicalOrigin(wxCoord *x, wxCoord *y) const
        { m_pimpl->DoGetLogicalOrigin(x, y); }
    wxPoint GetLogicalOrigin() const
        { wxCoord x, y; m_pimpl->DoGetLogicalOrigin(&x, &y); return wxPoint(x, y); }

    void SetDeviceOrigin(wxCoord x, wxCoord y)
        { m_pimpl->SetDeviceOrigin( x, y); }
    void GetDeviceOrigin(wxCoord *x, wxCoord *y) const
        { m_pimpl->DoGetDeviceOrigin(x, y); }
    wxPoint GetDeviceOrigin() const
        { wxCoord x, y; m_pimpl->DoGetDeviceOrigin(&x, &y); return wxPoint(x, y); }

    void SetAxisOrientation(bool xLeftRight, bool yBottomUp)
        { m_pimpl->SetAxisOrientation(xLeftRight, yBottomUp); }

#if wxUSE_DC_TRANSFORM_MATRIX
    bool CanUseTransformMatrix() const
        { return m_pimpl->CanUseTransformMatrix(); }

    bool SetTransformMatrix(const wxAffineMatrix2D &matrix)
        { return m_pimpl->SetTransformMatrix(matrix); }

    wxAffineMatrix2D GetTransformMatrix() const
        { return m_pimpl->GetTransformMatrix(); }

    void ResetTransformMatrix()
        { m_pimpl->ResetTransformMatrix(); }
#endif // wxUSE_DC_TRANSFORM_MATRIX

    // mostly internal
    void SetDeviceLocalOrigin( wxCoord x, wxCoord y )
        { m_pimpl->SetDeviceLocalOrigin( x, y ); }


    // -----------------------------------------------
    // the actual drawing API

    bool FloodFill(wxCoord x, wxCoord y, const wxColour& col,
                   wxFloodFillStyle style = wxFLOOD_SURFACE)
        { return m_pimpl->DoFloodFill(x, y, col, style); }
    bool FloodFill(const wxPoint& pt, const wxColour& col,
                   wxFloodFillStyle style = wxFLOOD_SURFACE)
        { return m_pimpl->DoFloodFill(pt.x, pt.y, col, style); }

    // fill the area specified by rect with a radial gradient, starting from
    // initialColour in the centre of the cercle and fading to destColour.
    void GradientFillConcentric(const wxRect& rect,
                                const wxColour& initialColour,
                                const wxColour& destColour)
        { m_pimpl->DoGradientFillConcentric( rect, initialColour, destColour,
                                             wxPoint(rect.GetWidth() / 2,
                                                     rect.GetHeight() / 2)); }

    void GradientFillConcentric(const wxRect& rect,
                                const wxColour& initialColour,
                                const wxColour& destColour,
                                const wxPoint& circleCenter)
        { m_pimpl->DoGradientFillConcentric(rect, initialColour, destColour, circleCenter); }

    // fill the area specified by rect with a linear gradient
    void GradientFillLinear(const wxRect& rect,
                            const wxColour& initialColour,
                            const wxColour& destColour,
                            wxDirection nDirection = wxEAST)
        { m_pimpl->DoGradientFillLinear(rect, initialColour, destColour, nDirection); }

    bool GetPixel(wxCoord x, wxCoord y, wxColour *col) const
        { return m_pimpl->DoGetPixel(x, y, col); }
    bool GetPixel(const wxPoint& pt, wxColour *col) const
        { return m_pimpl->DoGetPixel(pt.x, pt.y, col); }

    void DrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2)
        { m_pimpl->DoDrawLine(x1, y1, x2, y2); }
    void DrawLine(const wxPoint& pt1, const wxPoint& pt2)
        { m_pimpl->DoDrawLine(pt1.x, pt1.y, pt2.x, pt2.y); }

    void CrossHair(wxCoord x, wxCoord y)
        { m_pimpl->DoCrossHair(x, y); }
    void CrossHair(const wxPoint& pt)
        { m_pimpl->DoCrossHair(pt.x, pt.y); }

    void DrawArc(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2,
                 wxCoord xc, wxCoord yc)
        { m_pimpl->DoDrawArc(x1, y1, x2, y2, xc, yc); }
    void DrawArc(const wxPoint& pt1, const wxPoint& pt2, const wxPoint& centre)
        { m_pimpl->DoDrawArc(pt1.x, pt1.y, pt2.x, pt2.y, centre.x, centre.y); }

    void DrawCheckMark(wxCoord x, wxCoord y,
                       wxCoord width, wxCoord height)
        { m_pimpl->DoDrawCheckMark(x, y, width, height); }
    void DrawCheckMark(const wxRect& rect)
        { m_pimpl->DoDrawCheckMark(rect.x, rect.y, rect.width, rect.height); }

    void DrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                         double sa, double ea)
        { m_pimpl->DoDrawEllipticArc(x, y, w, h, sa, ea); }
    void DrawEllipticArc(const wxPoint& pt, const wxSize& sz,
                         double sa, double ea)
        { m_pimpl->DoDrawEllipticArc(pt.x, pt.y, sz.x, sz.y, sa, ea); }

    void DrawPoint(wxCoord x, wxCoord y)
        { m_pimpl->DoDrawPoint(x, y); }
    void DrawPoint(const wxPoint& pt)
        { m_pimpl->DoDrawPoint(pt.x, pt.y); }

    void DrawLines(int n, const wxPoint points[],
                   wxCoord xoffset = 0, wxCoord yoffset = 0)
        { m_pimpl->DoDrawLines(n, points, xoffset, yoffset); }
    void DrawLines(const wxPointList *list,
                   wxCoord xoffset = 0, wxCoord yoffset = 0)
        { m_pimpl->DrawLines( list, xoffset, yoffset ); }
#if WXWIN_COMPATIBILITY_2_8
    wxDEPRECATED( void DrawLines(const wxList *list,
                                 wxCoord xoffset = 0, wxCoord yoffset = 0) );
#endif  // WXWIN_COMPATIBILITY_2_8

    void DrawPolygon(int n, const wxPoint points[],
                     wxCoord xoffset = 0, wxCoord yoffset = 0,
                     wxPolygonFillMode fillStyle = wxODDEVEN_RULE)
        { m_pimpl->DoDrawPolygon(n, points, xoffset, yoffset, fillStyle); }
    void DrawPolygon(const wxPointList *list,
                     wxCoord xoffset = 0, wxCoord yoffset = 0,
                     wxPolygonFillMode fillStyle = wxODDEVEN_RULE)
        { m_pimpl->DrawPolygon( list, xoffset, yoffset, fillStyle ); }
    void DrawPolyPolygon(int n, const int count[], const wxPoint points[],
                         wxCoord xoffset = 0, wxCoord yoffset = 0,
                         wxPolygonFillMode fillStyle = wxODDEVEN_RULE)
        { m_pimpl->DoDrawPolyPolygon(n, count, points, xoffset, yoffset, fillStyle); }
#if WXWIN_COMPATIBILITY_2_8
    wxDEPRECATED( void DrawPolygon(const wxList *list,
                     wxCoord xoffset = 0, wxCoord yoffset = 0,
                     wxPolygonFillMode fillStyle = wxODDEVEN_RULE) );
#endif  // WXWIN_COMPATIBILITY_2_8

    void DrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
        { m_pimpl->DoDrawRectangle(x, y, width, height); }
    void DrawRectangle(const wxPoint& pt, const wxSize& sz)
        { m_pimpl->DoDrawRectangle(pt.x, pt.y, sz.x, sz.y); }
    void DrawRectangle(const wxRect& rect)
        { m_pimpl->DoDrawRectangle(rect.x, rect.y, rect.width, rect.height); }

    void DrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height,
                              double radius)
        { m_pimpl->DoDrawRoundedRectangle(x, y, width, height, radius); }
    void DrawRoundedRectangle(const wxPoint& pt, const wxSize& sz,
                             double radius)
        { m_pimpl->DoDrawRoundedRectangle(pt.x, pt.y, sz.x, sz.y, radius); }
    void DrawRoundedRectangle(const wxRect& r, double radius)
        { m_pimpl->DoDrawRoundedRectangle(r.x, r.y, r.width, r.height, radius); }

    void DrawCircle(wxCoord x, wxCoord y, wxCoord radius)
        { m_pimpl->DoDrawEllipse(x - radius, y - radius, 2*radius, 2*radius); }
    void DrawCircle(const wxPoint& pt, wxCoord radius)
        { m_pimpl->DoDrawEllipse(pt.x - radius, pt.y - radius, 2*radius, 2*radius); }

    void DrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
        { m_pimpl->DoDrawEllipse(x, y, width, height); }
    void DrawEllipse(const wxPoint& pt, const wxSize& sz)
        { m_pimpl->DoDrawEllipse(pt.x, pt.y, sz.x, sz.y); }
    void DrawEllipse(const wxRect& rect)
        { m_pimpl->DoDrawEllipse(rect.x, rect.y, rect.width, rect.height); }

    void DrawIcon(const wxIcon& icon, wxCoord x, wxCoord y)
        { m_pimpl->DoDrawIcon(icon, x, y); }
    void DrawIcon(const wxIcon& icon, const wxPoint& pt)
        { m_pimpl->DoDrawIcon(icon, pt.x, pt.y); }

    void DrawBitmap(const wxBitmap &bmp, wxCoord x, wxCoord y,
                    bool useMask = false)
        { m_pimpl->DoDrawBitmap(bmp, x, y, useMask); }
    void DrawBitmap(const wxBitmap &bmp, const wxPoint& pt,
                    bool useMask = false)
        { m_pimpl->DoDrawBitmap(bmp, pt.x, pt.y, useMask); }

    void DrawText(const wxString& text, wxCoord x, wxCoord y)
        { m_pimpl->DoDrawText(text, x, y); }
    void DrawText(const wxString& text, const wxPoint& pt)
        { m_pimpl->DoDrawText(text, pt.x, pt.y); }

    void DrawRotatedText(const wxString& text, wxCoord x, wxCoord y, double angle)
        { m_pimpl->DoDrawRotatedText(text, x, y, angle); }
    void DrawRotatedText(const wxString& text, const wxPoint& pt, double angle)
        { m_pimpl->DoDrawRotatedText(text, pt.x, pt.y, angle); }

    // this version puts both optional bitmap and the text into the given
    // rectangle and aligns is as specified by alignment parameter; it also
    // will emphasize the character with the given index if it is != -1 and
    // return the bounding rectangle if required
    void DrawLabel(const wxString& text,
                           const wxBitmap& image,
                           const wxRect& rect,
                           int alignment = wxALIGN_LEFT | wxALIGN_TOP,
                           int indexAccel = -1,
                           wxRect *rectBounding = NULL);

    void DrawLabel(const wxString& text, const wxRect& rect,
                   int alignment = wxALIGN_LEFT | wxALIGN_TOP,
                   int indexAccel = -1)
        { DrawLabel(text, wxNullBitmap, rect, alignment, indexAccel); }

    bool Blit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
              wxDC *source, wxCoord xsrc, wxCoord ysrc,
              wxRasterOperationMode rop = wxCOPY, bool useMask = false,
              wxCoord xsrcMask = wxDefaultCoord, wxCoord ysrcMask = wxDefaultCoord)
    {
        return m_pimpl->DoBlit(xdest, ydest, width, height,
                      source, xsrc, ysrc, rop, useMask, xsrcMask, ysrcMask);
    }
    bool Blit(const wxPoint& destPt, const wxSize& sz,
              wxDC *source, const wxPoint& srcPt,
              wxRasterOperationMode rop = wxCOPY, bool useMask = false,
              const wxPoint& srcPtMask = wxDefaultPosition)
    {
        return m_pimpl->DoBlit(destPt.x, destPt.y, sz.x, sz.y,
                      source, srcPt.x, srcPt.y, rop, useMask, srcPtMask.x, srcPtMask.y);
    }

    bool StretchBlit(wxCoord dstX, wxCoord dstY,
                     wxCoord dstWidth, wxCoord dstHeight,
                     wxDC *source,
                     wxCoord srcX, wxCoord srcY,
                     wxCoord srcWidth, wxCoord srcHeight,
                     wxRasterOperationMode rop = wxCOPY, bool useMask = false,
                     wxCoord srcMaskX = wxDefaultCoord, wxCoord srcMaskY = wxDefaultCoord)
    {
        return m_pimpl->DoStretchBlit(dstX, dstY, dstWidth, dstHeight,
                      source, srcX, srcY, srcWidth, srcHeight, rop, useMask, srcMaskX, srcMaskY);
    }
    bool StretchBlit(const wxPoint& dstPt, const wxSize& dstSize,
                     wxDC *source, const wxPoint& srcPt, const wxSize& srcSize,
                     wxRasterOperationMode rop = wxCOPY, bool useMask = false,
                     const wxPoint& srcMaskPt = wxDefaultPosition)
    {
        return m_pimpl->DoStretchBlit(dstPt.x, dstPt.y, dstSize.x, dstSize.y,
                      source, srcPt.x, srcPt.y, srcSize.x, srcSize.y, rop, useMask, srcMaskPt.x, srcMaskPt.y);
    }

    wxBitmap GetAsBitmap(const wxRect *subrect = (const wxRect *) NULL) const
    {
        return m_pimpl->DoGetAsBitmap(subrect);
    }

#if wxUSE_SPLINES
    void DrawSpline(wxCoord x1, wxCoord y1,
                    wxCoord x2, wxCoord y2,
                    wxCoord x3, wxCoord y3)
        { m_pimpl->DrawSpline(x1,y1,x2,y2,x3,y3); }
    void DrawSpline(int n, const wxPoint points[])
        { m_pimpl->DrawSpline(n,points); }
    void DrawSpline(const wxPointList *points)
        { m_pimpl->DrawSpline(points); }
#endif // wxUSE_SPLINES


#if WXWIN_COMPATIBILITY_2_8
    // for compatibility with the old code when wxCoord was long everywhere
    wxDEPRECATED( void GetTextExtent(const wxString& string,
                       long *x, long *y,
                       long *descent = NULL,
                       long *externalLeading = NULL,
                       const wxFont *theFont = NULL) const );
    wxDEPRECATED( void GetLogicalOrigin(long *x, long *y) const );
    wxDEPRECATED( void GetDeviceOrigin(long *x, long *y) const );
    wxDEPRECATED( void GetClippingBox(long *x, long *y, long *w, long *h) const );

    wxDEPRECATED( void DrawObject(wxDrawObject* drawobject) );
#endif  // WXWIN_COMPATIBILITY_2_8

#ifdef __WXMSW__
    // GetHDC() is the simplest way to retrieve an HDC From a wxDC but only
    // works if this wxDC is GDI-based and fails for GDI+ contexts (and
    // anything else without HDC, e.g. wxPostScriptDC)
    WXHDC GetHDC() const;

    // don't use these methods manually, use GetTempHDC() instead
    virtual WXHDC AcquireHDC() { return GetHDC(); }
    virtual void ReleaseHDC(WXHDC WXUNUSED(hdc)) { }

    // helper class holding the result of GetTempHDC() with std::auto_ptr<>-like
    // semantics, i.e. it is moved when copied
    class TempHDC
    {
    public:
        TempHDC(wxDC& dc)
            : m_dc(dc),
              m_hdc(dc.AcquireHDC())
        {
        }

        TempHDC(const TempHDC& thdc)
            : m_dc(thdc.m_dc),
              m_hdc(thdc.m_hdc)
        {
            const_cast<TempHDC&>(thdc).m_hdc = 0;
        }

        ~TempHDC()
        {
            if ( m_hdc )
                m_dc.ReleaseHDC(m_hdc);
        }

        WXHDC GetHDC() const { return m_hdc; }

    private:
        wxDC& m_dc;
        WXHDC m_hdc;

        wxDECLARE_NO_ASSIGN_CLASS(TempHDC);
    };

    // GetTempHDC() also works for wxGCDC (but still not for wxPostScriptDC &c)
    TempHDC GetTempHDC() { return TempHDC(*this); }
#endif // __WXMSW__

#if wxUSE_GRAPHICS_CONTEXT
    virtual wxGraphicsContext* GetGraphicsContext() const
    {
        return m_pimpl->GetGraphicsContext();
    }
    virtual void SetGraphicsContext( wxGraphicsContext* ctx )
    {
        m_pimpl->SetGraphicsContext(ctx);
    }
#endif

protected:
    // ctor takes ownership of the pointer
    wxDC(wxDCImpl *pimpl) : m_pimpl(pimpl) { }

    wxDCImpl * const m_pimpl;

private:
    DECLARE_ABSTRACT_CLASS(wxDC)
    wxDECLARE_NO_COPY_CLASS(wxDC);
};

// ----------------------------------------------------------------------------
// helper class: you can use it to temporarily change the DC text colour and
// restore it automatically when the object goes out of scope
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDCTextColourChanger
{
public:
    wxDCTextColourChanger(wxDC& dc) : m_dc(dc), m_colFgOld() { }

    wxDCTextColourChanger(wxDC& dc, const wxColour& col) : m_dc(dc)
    {
        Set(col);
    }

    ~wxDCTextColourChanger()
    {
        if ( m_colFgOld.IsOk() )
            m_dc.SetTextForeground(m_colFgOld);
    }

    void Set(const wxColour& col)
    {
        if ( !m_colFgOld.IsOk() )
            m_colFgOld = m_dc.GetTextForeground();
        m_dc.SetTextForeground(col);
    }

private:
    wxDC& m_dc;

    wxColour m_colFgOld;

    wxDECLARE_NO_COPY_CLASS(wxDCTextColourChanger);
};

// ----------------------------------------------------------------------------
// helper class: you can use it to temporarily change the DC pen and
// restore it automatically when the object goes out of scope
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDCPenChanger
{
public:
    wxDCPenChanger(wxDC& dc, const wxPen& pen) : m_dc(dc), m_penOld(dc.GetPen())
    {
        m_dc.SetPen(pen);
    }

    ~wxDCPenChanger()
    {
        if ( m_penOld.IsOk() )
            m_dc.SetPen(m_penOld);
    }

private:
    wxDC& m_dc;

    wxPen m_penOld;

    wxDECLARE_NO_COPY_CLASS(wxDCPenChanger);
};

// ----------------------------------------------------------------------------
// helper class: you can use it to temporarily change the DC brush and
// restore it automatically when the object goes out of scope
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDCBrushChanger
{
public:
    wxDCBrushChanger(wxDC& dc, const wxBrush& brush) : m_dc(dc), m_brushOld(dc.GetBrush())
    {
        m_dc.SetBrush(brush);
    }

    ~wxDCBrushChanger()
    {
        if ( m_brushOld.IsOk() )
            m_dc.SetBrush(m_brushOld);
    }

private:
    wxDC& m_dc;

    wxBrush m_brushOld;

    wxDECLARE_NO_COPY_CLASS(wxDCBrushChanger);
};

// ----------------------------------------------------------------------------
// another small helper class: sets the clipping region in its ctor and
// destroys it in the dtor
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDCClipper
{
public:
    wxDCClipper(wxDC& dc, const wxRegion& r) : m_dc(dc)
        { dc.SetClippingRegion(r.GetBox()); }
    wxDCClipper(wxDC& dc, const wxRect& r) : m_dc(dc)
        { dc.SetClippingRegion(r.x, r.y, r.width, r.height); }
    wxDCClipper(wxDC& dc, wxCoord x, wxCoord y, wxCoord w, wxCoord h) : m_dc(dc)
        { dc.SetClippingRegion(x, y, w, h); }

    ~wxDCClipper() { m_dc.DestroyClippingRegion(); }

private:
    wxDC& m_dc;

    wxDECLARE_NO_COPY_CLASS(wxDCClipper);
};

// ----------------------------------------------------------------------------
// helper class: you can use it to temporarily change the DC font and
// restore it automatically when the object goes out of scope
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDCFontChanger
{
public:
    wxDCFontChanger(wxDC& dc)
        : m_dc(dc), m_fontOld()
    {
    }

    wxDCFontChanger(wxDC& dc, const wxFont& font)
        : m_dc(dc), m_fontOld(dc.GetFont())
    {
        m_dc.SetFont(font);
    }

    void Set(const wxFont& font)
    {
        if ( !m_fontOld.IsOk() )
            m_fontOld = m_dc.GetFont();
        m_dc.SetFont(font);
    }

    ~wxDCFontChanger()
    {
        if ( m_fontOld.IsOk() )
            m_dc.SetFont(m_fontOld);
    }

private:
    wxDC& m_dc;

    wxFont m_fontOld;

    wxDECLARE_NO_COPY_CLASS(wxDCFontChanger);
};


#endif // _WX_DC_H_BASE_
