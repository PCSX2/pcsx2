/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/graphics.cpp
// Purpose:     wxGCDC class
// Author:      Stefan Csomor
// Modified by:
// Created:     2006-09-30
// Copyright:   (c) 2006 Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/dc.h"

#if wxUSE_GRAPHICS_CONTEXT

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcdlg.h"
    #include "wx/image.h"
    #include "wx/window.h"
    #include "wx/utils.h"
    #include "wx/dialog.h"
    #include "wx/app.h"
    #include "wx/bitmap.h"
    #include "wx/log.h"
    #include "wx/icon.h"
    #include "wx/module.h"
    // include all dc types that are used as a param
    #include "wx/dc.h"
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
    #include "wx/dcprint.h"
#endif

#include "wx/stack.h"

#include "wx/private/graphics.h"
#include "wx/msw/wrapgdip.h"
#include "wx/msw/dc.h"
#if wxUSE_ENH_METAFILE
    #include "wx/msw/enhmeta.h"
#endif
#include "wx/dcgraph.h"

#include "wx/msw/private.h" // needs to be before #include <commdlg.h>

#if wxUSE_COMMON_DIALOGS && !defined(__WXMICROWIN__)
#include <commdlg.h>
#endif

namespace
{

//-----------------------------------------------------------------------------
// constants
//-----------------------------------------------------------------------------

const double RAD2DEG = 180.0 / M_PI;

//-----------------------------------------------------------------------------
// Local functions
//-----------------------------------------------------------------------------

inline double dmin(double a, double b) { return a < b ? a : b; }
inline double dmax(double a, double b) { return a > b ? a : b; }

inline double DegToRad(double deg) { return (deg * M_PI) / 180.0; }
inline double RadToDeg(double deg) { return (deg * 180.0) / M_PI; }

// translate a wxColour to a Color
inline Color wxColourToColor(const wxColour& col)
{
    return Color(col.Alpha(), col.Red(), col.Green(), col.Blue());
}

// Do not use this pointer directly, it's only used by
// GetDrawTextStringFormat() and the cleanup code in wxGDIPlusRendererModule.
StringFormat* gs_drawTextStringFormat = NULL;

// Get the string format used for the text drawing and measuring functions:
// notice that it must be the same one for all of them, otherwise the drawn
// text might be of different size than what measuring it returned.
inline StringFormat* GetDrawTextStringFormat()
{
    if ( !gs_drawTextStringFormat )
    {
        gs_drawTextStringFormat = new StringFormat(StringFormat::GenericTypographic());

        // This doesn't make any difference for DrawText() actually but we want
        // this behaviour when measuring text.
        gs_drawTextStringFormat->SetFormatFlags
        (
            gs_drawTextStringFormat->GetFormatFlags()
                | StringFormatFlagsMeasureTrailingSpaces
        );
    }

    return gs_drawTextStringFormat;
}

} // anonymous namespace

//-----------------------------------------------------------------------------
// device context implementation
//
// more and more of the dc functionality should be implemented by calling
// the appropricate wxGDIPlusContext, but we will have to do that step by step
// also coordinate conversions should be moved to native matrix ops
//-----------------------------------------------------------------------------

// we always stock two context states, one at entry, to be able to preserve the
// state we were called with, the other one after changing to HI Graphics orientation
// (this one is used for getting back clippings etc)

//-----------------------------------------------------------------------------
// wxGraphicsPath implementation
//-----------------------------------------------------------------------------

class wxGDIPlusContext;

class wxGDIPlusPathData : public wxGraphicsPathData
{
public :
    wxGDIPlusPathData(wxGraphicsRenderer* renderer, GraphicsPath* path = NULL);
    ~wxGDIPlusPathData();

    virtual wxGraphicsObjectRefData *Clone() const;

    //
    // These are the path primitives from which everything else can be constructed
    //

    // begins a new subpath at (x,y)
    virtual void MoveToPoint( wxDouble x, wxDouble y );

    // adds a straight line from the current point to (x,y)
    virtual void AddLineToPoint( wxDouble x, wxDouble y );

    // adds a cubic Bezier curve from the current point, using two control points and an end point
    virtual void AddCurveToPoint( wxDouble cx1, wxDouble cy1, wxDouble cx2, wxDouble cy2, wxDouble x, wxDouble y );


    // adds an arc of a circle centering at (x,y) with radius (r) from startAngle to endAngle
    virtual void AddArc( wxDouble x, wxDouble y, wxDouble r, wxDouble startAngle, wxDouble endAngle, bool clockwise ) ;

    // gets the last point of the current path, (0,0) if not yet set
    virtual void GetCurrentPoint( wxDouble* x, wxDouble* y) const;

    // adds another path
    virtual void AddPath( const wxGraphicsPathData* path );

    // closes the current sub-path
    virtual void CloseSubpath();

    //
    // These are convenience functions which - if not available natively will be assembled
    // using the primitives from above
    //

    // appends a rectangle as a new closed subpath
    virtual void AddRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h ) ;
    /*

    // appends an ellipsis as a new closed subpath fitting the passed rectangle
    virtual void AddEllipsis( wxDouble x, wxDouble y, wxDouble w , wxDouble h ) ;

    // draws a an arc to two tangents connecting (current) to (x1,y1) and (x1,y1) to (x2,y2), also a straight line from (current) to (x1,y1)
    virtual void AddArcToPoint( wxDouble x1, wxDouble y1 , wxDouble x2, wxDouble y2, wxDouble r )  ;
*/

    // returns the native path
    virtual void * GetNativePath() const { return m_path; }

    // give the native path returned by GetNativePath() back (there might be some deallocations necessary)
    virtual void UnGetNativePath(void * WXUNUSED(path)) const {}

    // transforms each point of this path by the matrix
    virtual void Transform( const wxGraphicsMatrixData* matrix ) ;

    // gets the bounding box enclosing all points (possibly including control points)
    virtual void GetBox(wxDouble *x, wxDouble *y, wxDouble *w, wxDouble *h) const;

    virtual bool Contains( wxDouble x, wxDouble y, wxPolygonFillMode fillStyle = wxODDEVEN_RULE) const;

private :
    GraphicsPath* m_path;
};

class wxGDIPlusMatrixData : public wxGraphicsMatrixData
{
public :
    wxGDIPlusMatrixData(wxGraphicsRenderer* renderer, Matrix* matrix = NULL) ;
    virtual ~wxGDIPlusMatrixData() ;

    virtual wxGraphicsObjectRefData* Clone() const ;

    // concatenates the matrix
    virtual void Concat( const wxGraphicsMatrixData *t );

    // sets the matrix to the respective values
    virtual void Set(wxDouble a=1.0, wxDouble b=0.0, wxDouble c=0.0, wxDouble d=1.0,
        wxDouble tx=0.0, wxDouble ty=0.0);

    // gets the component valuess of the matrix
    virtual void Get(wxDouble* a=NULL, wxDouble* b=NULL,  wxDouble* c=NULL,
                     wxDouble* d=NULL, wxDouble* tx=NULL, wxDouble* ty=NULL) const;

    // makes this the inverse matrix
    virtual void Invert();

    // returns true if the elements of the transformation matrix are equal ?
    virtual bool IsEqual( const wxGraphicsMatrixData* t) const ;

    // return true if this is the identity matrix
    virtual bool IsIdentity() const;

    //
    // transformation
    //

    // add the translation to this matrix
    virtual void Translate( wxDouble dx , wxDouble dy );

    // add the scale to this matrix
    virtual void Scale( wxDouble xScale , wxDouble yScale );

    // add the rotation to this matrix (radians)
    virtual void Rotate( wxDouble angle );

    //
    // apply the transforms
    //

    // applies that matrix to the point
    virtual void TransformPoint( wxDouble *x, wxDouble *y ) const;

    // applies the matrix except for translations
    virtual void TransformDistance( wxDouble *dx, wxDouble *dy ) const;

    // returns the native representation
    virtual void * GetNativeMatrix() const;
private:
    Matrix* m_matrix ;
} ;

class wxGDIPlusPenData : public wxGraphicsObjectRefData
{
public:
    wxGDIPlusPenData( wxGraphicsRenderer* renderer, const wxPen &pen );
    ~wxGDIPlusPenData();

    void Init();

    virtual wxDouble GetWidth() { return m_width; }
    virtual Pen* GetGDIPlusPen() { return m_pen; }

protected :
    Pen* m_pen;
    Image* m_penImage;
    Brush* m_penBrush;

    wxDouble m_width;
};

class wxGDIPlusBrushData : public wxGraphicsObjectRefData
{
public:
    wxGDIPlusBrushData( wxGraphicsRenderer* renderer );
    wxGDIPlusBrushData( wxGraphicsRenderer* renderer, const wxBrush &brush );
    ~wxGDIPlusBrushData ();

    void CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                                   wxDouble x2, wxDouble y2,
                                   const wxGraphicsGradientStops& stops);
    void CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                                   wxDouble xc, wxDouble yc,
                                   wxDouble radius,
                                   const wxGraphicsGradientStops& stops);

    virtual Brush* GetGDIPlusBrush() { return m_brush; }

protected:
    virtual void Init();

private:
    // common part of Create{Linear,Radial}GradientBrush()
    template <typename T>
    void SetGradientStops(T *brush,
                          const wxGraphicsGradientStops& stops,
                          bool reversed = false);

    Brush* m_brush;
    Image* m_brushImage;
    GraphicsPath* m_brushPath;
};

class WXDLLIMPEXP_CORE wxGDIPlusBitmapData : public wxGraphicsBitmapData
{
public:
    wxGDIPlusBitmapData( wxGraphicsRenderer* renderer, Bitmap* bitmap );
    wxGDIPlusBitmapData( wxGraphicsRenderer* renderer, const wxBitmap &bmp );
    ~wxGDIPlusBitmapData ();

    virtual Bitmap* GetGDIPlusBitmap() { return m_bitmap; }
    virtual void* GetNativeBitmap() const { return m_bitmap; }

#if wxUSE_IMAGE
    wxImage ConvertToImage() const;
#endif // wxUSE_IMAGE

private :
    Bitmap* m_bitmap;
    Bitmap* m_helper;
};

class wxGDIPlusFontData : public wxGraphicsObjectRefData
{
public:
    wxGDIPlusFontData( wxGraphicsRenderer* renderer,
                       const wxFont &font,
                       const wxColour& col );
    wxGDIPlusFontData(wxGraphicsRenderer* renderer,
                      const wxString& name,
                      REAL sizeInPixels,
                      int style,
                      const wxColour& col);
    ~wxGDIPlusFontData();

    virtual Brush* GetGDIPlusBrush() { return m_textBrush; }
    virtual Font* GetGDIPlusFont() { return m_font; }

private :
    // Common part of all ctors, flags here is a combination of values of
    // FontStyle GDI+ enum.
    void Init(const wxString& name,
              REAL size,
              int style,
              const wxColour& col,
              Unit fontUnit);

    Brush* m_textBrush;
    Font* m_font;
};

class wxGDIPlusContext : public wxGraphicsContext
{
public:
    wxGDIPlusContext( wxGraphicsRenderer* renderer, const wxDC& dc );
    wxGDIPlusContext( wxGraphicsRenderer* renderer, HDC hdc, wxDouble width, wxDouble height );
    wxGDIPlusContext( wxGraphicsRenderer* renderer, HWND hwnd );
    wxGDIPlusContext( wxGraphicsRenderer* renderer, Graphics* gr);
    wxGDIPlusContext(wxGraphicsRenderer* renderer);

    virtual ~wxGDIPlusContext();

    virtual void Clip( const wxRegion &region );
    // clips drawings to the rect
    virtual void Clip( wxDouble x, wxDouble y, wxDouble w, wxDouble h );

    // resets the clipping to original extent
    virtual void ResetClip();

    virtual void * GetNativeContext();

    virtual void StrokePath( const wxGraphicsPath& p );
    virtual void FillPath( const wxGraphicsPath& p , wxPolygonFillMode fillStyle = wxODDEVEN_RULE );

    virtual void DrawRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h ); 

    // stroke lines connecting each of the points
    virtual void StrokeLines( size_t n, const wxPoint2DDouble *points);

    // We don't have any specific implementation for this one in wxMSW but
    // override it just to avoid warnings about hiding the base class virtual.
    virtual void StrokeLines( size_t n, const wxPoint2DDouble *beginPoints, const wxPoint2DDouble *endPoints)
    {
        wxGraphicsContext::StrokeLines(n, beginPoints, endPoints);
    }

    // draws a polygon
    virtual void DrawLines( size_t n, const wxPoint2DDouble *points, wxPolygonFillMode fillStyle = wxODDEVEN_RULE );

    virtual bool SetAntialiasMode(wxAntialiasMode antialias);

    virtual bool SetInterpolationQuality(wxInterpolationQuality interpolation);
    
    virtual bool SetCompositionMode(wxCompositionMode op);

    virtual void BeginLayer(wxDouble opacity);

    virtual void EndLayer();

    virtual void Translate( wxDouble dx , wxDouble dy );
    virtual void Scale( wxDouble xScale , wxDouble yScale );
    virtual void Rotate( wxDouble angle );

    // concatenates this transform with the current transform of this context
    virtual void ConcatTransform( const wxGraphicsMatrix& matrix );

    // sets the transform of this context
    virtual void SetTransform( const wxGraphicsMatrix& matrix );

    // gets the matrix of this context
    virtual wxGraphicsMatrix GetTransform() const;

    virtual void DrawBitmap( const wxGraphicsBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h );
    virtual void DrawBitmap( const wxBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h );
    virtual void DrawIcon( const wxIcon &icon, wxDouble x, wxDouble y, wxDouble w, wxDouble h );
    virtual void PushState();
    virtual void PopState();

    virtual void GetTextExtent( const wxString &str, wxDouble *width, wxDouble *height,
        wxDouble *descent, wxDouble *externalLeading ) const;
    virtual void GetPartialTextExtents(const wxString& text, wxArrayDouble& widths) const;
    virtual bool ShouldOffset() const;
    virtual void GetSize( wxDouble* width, wxDouble *height );

    Graphics* GetGraphics() const { return m_context; }

protected:

    wxDouble m_fontScaleRatio;

    // Used from ctors (including those in the derived classes) and takes
    // ownership of the graphics pointer that must be non-NULL.
    void Init(Graphics* graphics, int width, int height);

private:
    virtual void DoDrawText(const wxString& str, wxDouble x, wxDouble y);

    Graphics* m_context;
    wxStack<GraphicsState> m_stateStack;
    GraphicsState m_state1;
    GraphicsState m_state2;

    wxDECLARE_NO_COPY_CLASS(wxGDIPlusContext);
};

#if wxUSE_IMAGE

class wxGDIPlusImageContext : public wxGDIPlusContext
{
public:
    wxGDIPlusImageContext(wxGraphicsRenderer* renderer, wxImage& image) :
        wxGDIPlusContext(renderer),
        m_image(image),
        m_bitmap(renderer, image)
    {
        Init
        (
            new Graphics(m_bitmap.GetGDIPlusBitmap()),
            image.GetWidth(),
            image.GetHeight()
        );
    }

    virtual ~wxGDIPlusImageContext()
    {
        Flush();
    }

    virtual void Flush()
    {
        m_image = m_bitmap.ConvertToImage();
    }

private:
    wxImage& m_image;
    wxGDIPlusBitmapData m_bitmap;

    wxDECLARE_NO_COPY_CLASS(wxGDIPlusImageContext);
};

#endif // wxUSE_IMAGE

class wxGDIPlusMeasuringContext : public wxGDIPlusContext
{
public:
    wxGDIPlusMeasuringContext( wxGraphicsRenderer* renderer ) : wxGDIPlusContext( renderer , m_hdc = GetDC(NULL), 1000, 1000 )
    {
    }

    virtual ~wxGDIPlusMeasuringContext()
    {
        ReleaseDC( NULL, m_hdc );
    }

private:
    HDC m_hdc ;
} ;

class wxGDIPlusPrintingContext : public wxGDIPlusContext
{
public:
    wxGDIPlusPrintingContext( wxGraphicsRenderer* renderer, const wxDC& dc );
    virtual ~wxGDIPlusPrintingContext() { }
protected:
};

//-----------------------------------------------------------------------------
// wxGDIPlusRenderer declaration
//-----------------------------------------------------------------------------

class wxGDIPlusRenderer : public wxGraphicsRenderer
{
public :
    wxGDIPlusRenderer()
    {
        m_loaded = -1;
        m_gditoken = 0;
    }

    virtual ~wxGDIPlusRenderer()
    {
        if ( m_loaded == 1 )
        {
            Unload();
        }
    }

    // Context

    virtual wxGraphicsContext * CreateContext( const wxWindowDC& dc);

    virtual wxGraphicsContext * CreateContext( const wxMemoryDC& dc);

#if wxUSE_PRINTING_ARCHITECTURE
    virtual wxGraphicsContext * CreateContext( const wxPrinterDC& dc);
#endif

#if wxUSE_ENH_METAFILE
    virtual wxGraphicsContext * CreateContext( const wxEnhMetaFileDC& dc);
#endif

    virtual wxGraphicsContext * CreateContextFromNativeContext( void * context );

    virtual wxGraphicsContext * CreateContextFromNativeWindow( void * window );

    virtual wxGraphicsContext * CreateContext( wxWindow* window );

#if wxUSE_IMAGE
    virtual wxGraphicsContext * CreateContextFromImage(wxImage& image);
#endif // wxUSE_IMAGE

    virtual wxGraphicsContext * CreateMeasuringContext();

    // Path

    virtual wxGraphicsPath CreatePath();

    // Matrix

    virtual wxGraphicsMatrix CreateMatrix( wxDouble a=1.0, wxDouble b=0.0, wxDouble c=0.0, wxDouble d=1.0,
        wxDouble tx=0.0, wxDouble ty=0.0);


    virtual wxGraphicsPen CreatePen(const wxPen& pen) ;

    virtual wxGraphicsBrush CreateBrush(const wxBrush& brush ) ;

    virtual wxGraphicsBrush
    CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                              wxDouble x2, wxDouble y2,
                              const wxGraphicsGradientStops& stops);

    virtual wxGraphicsBrush
    CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                              wxDouble xc, wxDouble yc,
                              wxDouble radius,
                              const wxGraphicsGradientStops& stops);

    // create a native bitmap representation
    virtual wxGraphicsBitmap CreateBitmap( const wxBitmap &bitmap );
#if wxUSE_IMAGE
    virtual wxGraphicsBitmap CreateBitmapFromImage(const wxImage& image);
    virtual wxImage CreateImageFromBitmap(const wxGraphicsBitmap& bmp);
#endif // wxUSE_IMAGE

    virtual wxGraphicsFont CreateFont( const wxFont& font,
                                       const wxColour& col);

    virtual wxGraphicsFont CreateFont(double size,
                                      const wxString& facename,
                                      int flags = wxFONTFLAG_DEFAULT,
                                      const wxColour& col = *wxBLACK);

    // create a graphics bitmap from a native bitmap
    virtual wxGraphicsBitmap CreateBitmapFromNativeBitmap( void* bitmap );

    // create a subimage from a native image representation
    virtual wxGraphicsBitmap CreateSubBitmap( const wxGraphicsBitmap &bitmap, wxDouble x, wxDouble y, wxDouble w, wxDouble h  );

protected :
    bool EnsureIsLoaded();
    void Load();
    void Unload();
    friend class wxGDIPlusRendererModule;

private :
    int m_loaded;
    ULONG_PTR m_gditoken;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxGDIPlusRenderer)
} ;

//-----------------------------------------------------------------------------
// wxGDIPlusPen implementation
//-----------------------------------------------------------------------------

wxGDIPlusPenData::~wxGDIPlusPenData()
{
    delete m_pen;
    delete m_penImage;
    delete m_penBrush;
}

void wxGDIPlusPenData::Init()
{
    m_pen = NULL ;
    m_penImage = NULL;
    m_penBrush = NULL;
}

wxGDIPlusPenData::wxGDIPlusPenData( wxGraphicsRenderer* renderer, const wxPen &pen )
: wxGraphicsObjectRefData(renderer)
{
    Init();
    m_width = pen.GetWidth();
    if (m_width <= 0.0)
        m_width = 0.1;

    m_pen = new Pen(wxColourToColor(pen.GetColour()), m_width );

    LineCap cap;
    switch ( pen.GetCap() )
    {
    case wxCAP_ROUND :
        cap = LineCapRound;
        break;

    case wxCAP_PROJECTING :
        cap = LineCapSquare;
        break;

    case wxCAP_BUTT :
        cap = LineCapFlat; // TODO verify
        break;

    default :
        cap = LineCapFlat;
        break;
    }
    m_pen->SetLineCap(cap,cap, DashCapFlat);

    LineJoin join;
    switch ( pen.GetJoin() )
    {
    case wxJOIN_BEVEL :
        join = LineJoinBevel;
        break;

    case wxJOIN_MITER :
        join = LineJoinMiter;
        break;

    case wxJOIN_ROUND :
        join = LineJoinRound;
        break;

    default :
        join = LineJoinMiter;
        break;
    }

    m_pen->SetLineJoin(join);

    m_pen->SetDashStyle(DashStyleSolid);

    DashStyle dashStyle = DashStyleSolid;
    switch ( pen.GetStyle() )
    {
    case wxPENSTYLE_SOLID :
        break;

    case wxPENSTYLE_DOT :
        dashStyle = DashStyleDot;
        break;

    case wxPENSTYLE_LONG_DASH :
        dashStyle = DashStyleDash; // TODO verify
        break;

    case wxPENSTYLE_SHORT_DASH :
        dashStyle = DashStyleDash;
        break;

    case wxPENSTYLE_DOT_DASH :
        dashStyle = DashStyleDashDot;
        break;
    case wxPENSTYLE_USER_DASH :
        {
            dashStyle = DashStyleCustom;
            wxDash *dashes;
            int count = pen.GetDashes( &dashes );
            if ((dashes != NULL) && (count > 0))
            {
                REAL *userLengths = new REAL[count];
                for ( int i = 0; i < count; ++i )
                {
                    userLengths[i] = dashes[i];
                }
                m_pen->SetDashPattern( userLengths, count);
                delete[] userLengths;
            }
        }
        break;
    case wxPENSTYLE_STIPPLE :
        {
            wxBitmap* bmp = pen.GetStipple();
            if ( bmp && bmp->IsOk() )
            {
                m_penImage = Bitmap::FromHBITMAP((HBITMAP)bmp->GetHBITMAP(),
#if wxUSE_PALETTE
                    (HPALETTE)bmp->GetPalette()->GetHPALETTE()
#else
                    NULL
#endif
                );
                m_penBrush = new TextureBrush(m_penImage);
                m_pen->SetBrush( m_penBrush );
            }

        }
        break;
    default :
        if ( pen.GetStyle() >= wxPENSTYLE_FIRST_HATCH &&
             pen.GetStyle() <= wxPENSTYLE_LAST_HATCH )
        {
            HatchStyle style;
            switch( pen.GetStyle() )
            {
            case wxPENSTYLE_BDIAGONAL_HATCH :
                style = HatchStyleBackwardDiagonal;
                break ;
            case wxPENSTYLE_CROSSDIAG_HATCH :
                style = HatchStyleDiagonalCross;
                break ;
            case wxPENSTYLE_FDIAGONAL_HATCH :
                style = HatchStyleForwardDiagonal;
                break ;
            case wxPENSTYLE_CROSS_HATCH :
                style = HatchStyleCross;
                break ;
            case wxPENSTYLE_HORIZONTAL_HATCH :
                style = HatchStyleHorizontal;
                break ;
            case wxPENSTYLE_VERTICAL_HATCH :
                style = HatchStyleVertical;
                break ;
            default:
                style = HatchStyleHorizontal;
            }
            m_penBrush = new HatchBrush
                             (
                                style,
                                wxColourToColor(pen.GetColour()),
                                Color::Transparent
                             );
            m_pen->SetBrush( m_penBrush );
        }
        break;
    }
    if ( dashStyle != DashStyleSolid )
        m_pen->SetDashStyle(dashStyle);
}

//-----------------------------------------------------------------------------
// wxGDIPlusBrush implementation
//-----------------------------------------------------------------------------

wxGDIPlusBrushData::wxGDIPlusBrushData( wxGraphicsRenderer* renderer )
: wxGraphicsObjectRefData(renderer)
{
    Init();
}

wxGDIPlusBrushData::wxGDIPlusBrushData( wxGraphicsRenderer* renderer , const wxBrush &brush )
: wxGraphicsObjectRefData(renderer)
{
    Init();
    if ( brush.GetStyle() == wxSOLID)
    {
        m_brush = new SolidBrush(wxColourToColor( brush.GetColour()));
    }
    else if ( brush.IsHatch() )
    {
        HatchStyle style;
        switch( brush.GetStyle() )
        {
        case wxBRUSHSTYLE_BDIAGONAL_HATCH :
            style = HatchStyleBackwardDiagonal;
            break ;
        case wxBRUSHSTYLE_CROSSDIAG_HATCH :
            style = HatchStyleDiagonalCross;
            break ;
        case wxBRUSHSTYLE_FDIAGONAL_HATCH :
            style = HatchStyleForwardDiagonal;
            break ;
        case wxBRUSHSTYLE_CROSS_HATCH :
            style = HatchStyleCross;
            break ;
        case wxBRUSHSTYLE_HORIZONTAL_HATCH :
            style = HatchStyleHorizontal;
            break ;
        case wxBRUSHSTYLE_VERTICAL_HATCH :
            style = HatchStyleVertical;
            break ;
        default:
            style = HatchStyleHorizontal;
        }
        m_brush = new HatchBrush
                      (
                        style,
                        wxColourToColor(brush.GetColour()),
                        Color::Transparent
                      );
    }
    else
    {
        wxBitmap* bmp = brush.GetStipple();
        if ( bmp && bmp->IsOk() )
        {
            wxDELETE( m_brushImage );
            m_brushImage = Bitmap::FromHBITMAP((HBITMAP)bmp->GetHBITMAP(),
#if wxUSE_PALETTE
                (HPALETTE)bmp->GetPalette()->GetHPALETTE()
#else
                NULL
#endif
            );
            m_brush = new TextureBrush(m_brushImage);
        }
    }
}

wxGDIPlusBrushData::~wxGDIPlusBrushData()
{
    delete m_brush;
    delete m_brushImage;
    delete m_brushPath;
};

void wxGDIPlusBrushData::Init()
{
    m_brush = NULL;
    m_brushImage= NULL;
    m_brushPath= NULL;
}

template <typename T>
void
wxGDIPlusBrushData::SetGradientStops(T *brush,
        const wxGraphicsGradientStops& stops,
        bool reversed)
{
    const unsigned numStops = stops.GetCount();
    if ( numStops <= 2 )
    {
        // initial and final colours are set during the brush creation, nothing
        // more to do
        return;
    }

    wxVector<Color> colors(numStops);
    wxVector<REAL> positions(numStops);

    if ( reversed )
    {
        for ( unsigned i = 0; i < numStops; i++ )
        {
            wxGraphicsGradientStop stop = stops.Item(numStops - i - 1);

            colors[i] = wxColourToColor(stop.GetColour());
            positions[i] = 1.0 - stop.GetPosition();
        }
    }
    else
    {
        for ( unsigned i = 0; i < numStops; i++ )
        {
            wxGraphicsGradientStop stop = stops.Item(i);

            colors[i] = wxColourToColor(stop.GetColour());
            positions[i] = stop.GetPosition();
        }
    }

    brush->SetInterpolationColors(&colors[0], &positions[0], numStops);
}

void
wxGDIPlusBrushData::CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                                              wxDouble x2, wxDouble y2,
                                              const wxGraphicsGradientStops& stops)
{
    LinearGradientBrush * const
        brush = new LinearGradientBrush(PointF(x1, y1) , PointF(x2, y2),
                                        wxColourToColor(stops.GetStartColour()),
                                        wxColourToColor(stops.GetEndColour()));
    m_brush =  brush;

    SetGradientStops(brush, stops);
}

void
wxGDIPlusBrushData::CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                                              wxDouble xc, wxDouble yc,
                                              wxDouble radius,
                                              const wxGraphicsGradientStops& stops)
{
    m_brushPath = new GraphicsPath();
    m_brushPath->AddEllipse( (REAL)(xc-radius), (REAL)(yc-radius),
                             (REAL)(2*radius), (REAL)(2*radius));

    PathGradientBrush * const brush = new PathGradientBrush(m_brushPath);
    m_brush = brush;
    brush->SetCenterPoint(PointF(xo, yo));
    brush->SetCenterColor(wxColourToColor(stops.GetStartColour()));

    const Color col(wxColourToColor(stops.GetEndColour()));
    int count = 1;
    brush->SetSurroundColors(&col, &count);

    // Because the GDI+ API draws radial gradients from outside towards the
    // center we have to reverse the order of the gradient stops.
    SetGradientStops(brush, stops, true);
}

//-----------------------------------------------------------------------------
// wxGDIPlusFont implementation
//-----------------------------------------------------------------------------

void
wxGDIPlusFontData::Init(const wxString& name,
                        REAL size,
                        int style,
                        const wxColour& col,
                        Unit fontUnit)
{
    m_font = new Font(name.wc_str(), size, style, fontUnit);

    m_textBrush = new SolidBrush(wxColourToColor(col));
}

wxGDIPlusFontData::wxGDIPlusFontData( wxGraphicsRenderer* renderer,
                                      const wxFont &font,
                                      const wxColour& col )
    : wxGraphicsObjectRefData( renderer )
{
    int style = FontStyleRegular;
    if ( font.GetStyle() == wxFONTSTYLE_ITALIC )
        style |= FontStyleItalic;
    if ( font.GetUnderlined() )
        style |= FontStyleUnderline;
    if ( font.GetWeight() == wxFONTWEIGHT_BOLD )
        style |= FontStyleBold;

    // Create font which size is measured in logical units
    // and let the system rescale it according to the target resolution.
    Init(font.GetFaceName(), font.GetPixelSize().GetHeight(), style, col, UnitPixel);
}

wxGDIPlusFontData::wxGDIPlusFontData(wxGraphicsRenderer* renderer,
                                     const wxString& name,
                                     REAL sizeInPixels,
                                     int style,
                                     const wxColour& col) :
    wxGraphicsObjectRefData(renderer)
{
    Init(name, sizeInPixels, style, col, UnitPixel);
}

wxGDIPlusFontData::~wxGDIPlusFontData()
{
    delete m_textBrush;
    delete m_font;
}

// the built-in conversions functions create non-premultiplied bitmaps, while GDIPlus needs them in the
// premultiplied format, therefore in the failing cases we create a new bitmap using the non-premultiplied
// bytes as parameter, since there is no real copying of the data going in, only references are stored
// m_helper has to be kept alive as well

//-----------------------------------------------------------------------------
// wxGDIPlusBitmapData implementation
//-----------------------------------------------------------------------------

wxGDIPlusBitmapData::wxGDIPlusBitmapData( wxGraphicsRenderer* renderer, Bitmap* bitmap ) :
    wxGraphicsBitmapData( renderer ), m_bitmap( bitmap )
{
    m_helper = NULL;
}

wxGDIPlusBitmapData::wxGDIPlusBitmapData( wxGraphicsRenderer* renderer,
                        const wxBitmap &bmp) : wxGraphicsBitmapData( renderer )
{
    m_bitmap = NULL;
    m_helper = NULL;

    Bitmap* image = NULL;
    if ( bmp.GetMask() )
    {
        Bitmap interim((HBITMAP)bmp.GetHBITMAP(),
#if wxUSE_PALETTE
            (HPALETTE)bmp.GetPalette()->GetHPALETTE()
#else
            NULL
#endif
        );

        size_t width = interim.GetWidth();
        size_t height = interim.GetHeight();
        Rect bounds(0,0,width,height);

        image = new Bitmap(width,height,PixelFormat32bppPARGB) ;

        Bitmap interimMask((HBITMAP)bmp.GetMask()->GetMaskBitmap(),NULL);
        wxASSERT(interimMask.GetPixelFormat() == PixelFormat1bppIndexed);

        BitmapData dataMask ;
        interimMask.LockBits(&bounds,ImageLockModeRead,
            interimMask.GetPixelFormat(),&dataMask);


        BitmapData imageData ;
        image->LockBits(&bounds,ImageLockModeWrite, PixelFormat32bppPARGB, &imageData);

        BYTE maskPattern = 0 ;
        BYTE maskByte = 0;
        size_t maskIndex ;

        for ( size_t y = 0 ; y < height ; ++y)
        {
            maskIndex = 0 ;
            for( size_t x = 0 ; x < width; ++x)
            {
                if ( x % 8 == 0)
                {
                    maskPattern = 0x80;
                    maskByte = *((BYTE*)dataMask.Scan0 + dataMask.Stride*y + maskIndex);
                    maskIndex++;
                }
                else
                    maskPattern = maskPattern >> 1;

                ARGB *dest = (ARGB*)((BYTE*)imageData.Scan0 + imageData.Stride*y + x*4);
                if ( (maskByte & maskPattern) == 0 )
                    *dest = 0x00000000;
                else
                {
                    Color c ;
                    interim.GetPixel(x,y,&c) ;
                    *dest = (c.GetValue() | Color::AlphaMask);
                }
            }
        }

        image->UnlockBits(&imageData);

        interimMask.UnlockBits(&dataMask);
        interim.UnlockBits(&dataMask);
    }
    else
    {
        image = Bitmap::FromHBITMAP((HBITMAP)bmp.GetHBITMAP(),
#if wxUSE_PALETTE
            (HPALETTE)bmp.GetPalette()->GetHPALETTE()
#else
            NULL
#endif
        );
        if ( bmp.HasAlpha() && GetPixelFormatSize(image->GetPixelFormat()) == 32 )
        {
            size_t width = image->GetWidth();
            size_t height = image->GetHeight();
            Rect bounds(0,0,width,height);
            static BitmapData data ;

            m_helper = image ;
            image = NULL ;
            m_helper->LockBits(&bounds, ImageLockModeRead,
                m_helper->GetPixelFormat(),&data);

            image = new Bitmap(data.Width, data.Height, data.Stride,
                PixelFormat32bppPARGB , (BYTE*) data.Scan0);

            m_helper->UnlockBits(&data);
        }
    }
    if ( image )
        m_bitmap = image;
}

#if wxUSE_IMAGE

wxImage wxGDIPlusBitmapData::ConvertToImage() const
{
    // We could use Bitmap::LockBits() and convert to wxImage directly but
    // passing by wxBitmap is easier. It would be nice to measure performance
    // of the two methods but for this the second one would need to be written
    // first...
    HBITMAP hbmp;
    if ( m_bitmap->GetHBITMAP(Color(0xffffffff), &hbmp) != Gdiplus::Ok )
        return wxNullImage;

    wxBitmap bmp;
    bmp.SetWidth(m_bitmap->GetWidth());
    bmp.SetHeight(m_bitmap->GetHeight());
    bmp.SetHBITMAP(hbmp);
    bmp.SetDepth(IsAlphaPixelFormat(m_bitmap->GetPixelFormat()) ? 32 : 24);
    return bmp.ConvertToImage();
}

#endif // wxUSE_IMAGE

wxGDIPlusBitmapData::~wxGDIPlusBitmapData()
{
    delete m_bitmap;
    delete m_helper;
}

//-----------------------------------------------------------------------------
// wxGDIPlusPath implementation
//-----------------------------------------------------------------------------

wxGDIPlusPathData::wxGDIPlusPathData(wxGraphicsRenderer* renderer, GraphicsPath* path ) : wxGraphicsPathData(renderer)
{
    if ( path )
        m_path = path;
    else
        m_path = new GraphicsPath();
}

wxGDIPlusPathData::~wxGDIPlusPathData()
{
    delete m_path;
}

wxGraphicsObjectRefData* wxGDIPlusPathData::Clone() const
{
    return new wxGDIPlusPathData( GetRenderer() , m_path->Clone());
}

//
// The Primitives
//

void wxGDIPlusPathData::MoveToPoint( wxDouble x , wxDouble y )
{
    m_path->StartFigure();
    m_path->AddLine((REAL) x,(REAL) y,(REAL) x,(REAL) y);
}

void wxGDIPlusPathData::AddLineToPoint( wxDouble x , wxDouble y )
{
    m_path->AddLine((REAL) x,(REAL) y,(REAL) x,(REAL) y);
}

void wxGDIPlusPathData::CloseSubpath()
{
    m_path->CloseFigure();
}

void wxGDIPlusPathData::AddCurveToPoint( wxDouble cx1, wxDouble cy1, wxDouble cx2, wxDouble cy2, wxDouble x, wxDouble y )
{
    PointF c1(cx1,cy1);
    PointF c2(cx2,cy2);
    PointF end(x,y);
    PointF start;
    m_path->GetLastPoint(&start);
    m_path->AddBezier(start,c1,c2,end);
}

// gets the last point of the current path, (0,0) if not yet set
void wxGDIPlusPathData::GetCurrentPoint( wxDouble* x, wxDouble* y) const
{
    PointF start;
    m_path->GetLastPoint(&start);
    *x = start.X ;
    *y = start.Y ;
}

void wxGDIPlusPathData::AddArc( wxDouble x, wxDouble y, wxDouble r, double startAngle, double endAngle, bool clockwise )
{
    double sweepAngle = endAngle - startAngle ;
    if( fabs(sweepAngle) >= 2*M_PI)
    {
        sweepAngle = 2 * M_PI;
    }
    else
    {
        if ( clockwise )
        {
            if( sweepAngle < 0 )
                sweepAngle += 2 * M_PI;
        }
        else
        {
            if( sweepAngle > 0 )
                sweepAngle -= 2 * M_PI;

        }
   }
   m_path->AddArc((REAL) (x-r),(REAL) (y-r),(REAL) (2*r),(REAL) (2*r),RadToDeg(startAngle),RadToDeg(sweepAngle));
}

void wxGDIPlusPathData::AddRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    m_path->AddRectangle(RectF(x,y,w,h));
}

void wxGDIPlusPathData::AddPath( const wxGraphicsPathData* path )
{
    m_path->AddPath( (GraphicsPath*) path->GetNativePath(), FALSE);
}


// transforms each point of this path by the matrix
void wxGDIPlusPathData::Transform( const wxGraphicsMatrixData* matrix )
{
    m_path->Transform( (Matrix*) matrix->GetNativeMatrix() );
}

// gets the bounding box enclosing all points (possibly including control points)
void wxGDIPlusPathData::GetBox(wxDouble *x, wxDouble *y, wxDouble *w, wxDouble *h) const
{
    RectF bounds;
    m_path->GetBounds( &bounds, NULL, NULL) ;
    *x = bounds.X;
    *y = bounds.Y;
    *w = bounds.Width;
    *h = bounds.Height;
}

bool wxGDIPlusPathData::Contains( wxDouble x, wxDouble y, wxPolygonFillMode fillStyle ) const
{
    m_path->SetFillMode( fillStyle == wxODDEVEN_RULE ? FillModeAlternate : FillModeWinding);
    return m_path->IsVisible( (FLOAT) x,(FLOAT) y) == TRUE ;
}

//-----------------------------------------------------------------------------
// wxGDIPlusMatrixData implementation
//-----------------------------------------------------------------------------

wxGDIPlusMatrixData::wxGDIPlusMatrixData(wxGraphicsRenderer* renderer, Matrix* matrix )
    : wxGraphicsMatrixData(renderer)
{
    if ( matrix )
        m_matrix = matrix ;
    else
        m_matrix = new Matrix();
}

wxGDIPlusMatrixData::~wxGDIPlusMatrixData()
{
    delete m_matrix;
}

wxGraphicsObjectRefData *wxGDIPlusMatrixData::Clone() const
{
    return new wxGDIPlusMatrixData( GetRenderer(), m_matrix->Clone());
}

// concatenates the matrix
void wxGDIPlusMatrixData::Concat( const wxGraphicsMatrixData *t )
{
    m_matrix->Multiply( (Matrix*) t->GetNativeMatrix());
}

// sets the matrix to the respective values
void wxGDIPlusMatrixData::Set(wxDouble a, wxDouble b, wxDouble c, wxDouble d,
                 wxDouble tx, wxDouble ty)
{
    m_matrix->SetElements(a,b,c,d,tx,ty);
}

// gets the component valuess of the matrix
void wxGDIPlusMatrixData::Get(wxDouble* a, wxDouble* b,  wxDouble* c,
                              wxDouble* d, wxDouble* tx, wxDouble* ty) const
{
    REAL elements[6];
    m_matrix->GetElements(elements);
    if (a)  *a = elements[0];
    if (b)  *b = elements[1];
    if (c)  *c = elements[2];
    if (d)  *d = elements[3];
    if (tx) *tx= elements[4];
    if (ty) *ty= elements[5];
}

// makes this the inverse matrix
void wxGDIPlusMatrixData::Invert()
{
    m_matrix->Invert();
}

// returns true if the elements of the transformation matrix are equal ?
bool wxGDIPlusMatrixData::IsEqual( const wxGraphicsMatrixData* t) const
{
    return m_matrix->Equals((Matrix*) t->GetNativeMatrix())== TRUE ;
}

// return true if this is the identity matrix
bool wxGDIPlusMatrixData::IsIdentity() const
{
    return m_matrix->IsIdentity() == TRUE ;
}

//
// transformation
//

// add the translation to this matrix
void wxGDIPlusMatrixData::Translate( wxDouble dx , wxDouble dy )
{
    m_matrix->Translate(dx,dy);
}

// add the scale to this matrix
void wxGDIPlusMatrixData::Scale( wxDouble xScale , wxDouble yScale )
{
    m_matrix->Scale(xScale,yScale);
}

// add the rotation to this matrix (radians)
void wxGDIPlusMatrixData::Rotate( wxDouble angle )
{
    m_matrix->Rotate( RadToDeg(angle) );
}

//
// apply the transforms
//

// applies that matrix to the point
void wxGDIPlusMatrixData::TransformPoint( wxDouble *x, wxDouble *y ) const
{
    PointF pt(*x,*y);
    m_matrix->TransformPoints(&pt);
    *x = pt.X;
    *y = pt.Y;
}

// applies the matrix except for translations
void wxGDIPlusMatrixData::TransformDistance( wxDouble *dx, wxDouble *dy ) const
{
    PointF pt(*dx,*dy);
    m_matrix->TransformVectors(&pt);
    *dx = pt.X;
    *dy = pt.Y;
}

// returns the native representation
void * wxGDIPlusMatrixData::GetNativeMatrix() const
{
    return m_matrix;
}

//-----------------------------------------------------------------------------
// wxGDIPlusContext implementation
//-----------------------------------------------------------------------------

class wxGDIPlusOffsetHelper
{
public :
    wxGDIPlusOffsetHelper( Graphics* gr , bool offset )
    {
        m_gr = gr;
        m_offset = offset;
        if ( m_offset )
            m_gr->TranslateTransform( 0.5, 0.5 );
    }
    ~wxGDIPlusOffsetHelper( )
    {
        if ( m_offset )
            m_gr->TranslateTransform( -0.5, -0.5 );
    }
public :
    Graphics* m_gr;
    bool m_offset;
} ;

wxGDIPlusContext::wxGDIPlusContext( wxGraphicsRenderer* renderer, HDC hdc, wxDouble width, wxDouble height   )
    : wxGraphicsContext(renderer)
{
    Init(new Graphics(hdc), width, height);
}

wxGDIPlusContext::wxGDIPlusContext( wxGraphicsRenderer* renderer, const wxDC& dc )
    : wxGraphicsContext(renderer)
{
    wxMSWDCImpl *msw = wxDynamicCast( dc.GetImpl() , wxMSWDCImpl );
    HDC hdc = (HDC) msw->GetHDC();
    wxSize sz = dc.GetSize();

    Init(new Graphics(hdc), sz.x, sz.y);
}

wxGDIPlusContext::wxGDIPlusContext( wxGraphicsRenderer* renderer, HWND hwnd  )
    : wxGraphicsContext(renderer)
{
    RECT rect = wxGetWindowRect(hwnd);
    Init(new Graphics(hwnd), rect.right - rect.left, rect.bottom - rect.top);
    m_enableOffset = true;
}

wxGDIPlusContext::wxGDIPlusContext( wxGraphicsRenderer* renderer, Graphics* gr  )
    : wxGraphicsContext(renderer)
{
    Init(gr, 0, 0);
}

wxGDIPlusContext::wxGDIPlusContext(wxGraphicsRenderer* renderer)
    : wxGraphicsContext(renderer)
{
    // Derived class must call Init() later but just set m_context to NULL for
    // safety to avoid crashing in our dtor if Init() ends up not being called.
    m_context = NULL;
}

void wxGDIPlusContext::Init(Graphics* graphics, int width, int height)
{
    m_context = graphics;
    m_state1 = 0;
    m_state2 = 0;
    m_width = width;
    m_height = height;
    m_fontScaleRatio = 1.0;

    m_context->SetTextRenderingHint(TextRenderingHintSystemDefault);
    m_context->SetPixelOffsetMode(PixelOffsetModeHalf);
    m_context->SetSmoothingMode(SmoothingModeHighQuality);

    SetInterpolationQuality(wxINTERPOLATION_GOOD);

    m_state1 = m_context->Save();
    m_state2 = m_context->Save();
}

wxGDIPlusContext::~wxGDIPlusContext()
{
    if ( m_context )
    {
        m_context->Restore( m_state2 );
        m_context->Restore( m_state1 );
        delete m_context;
    }
}


void wxGDIPlusContext::Clip( const wxRegion &region )
{
    Region rgn((HRGN)region.GetHRGN());
    m_context->SetClip(&rgn,CombineModeIntersect);
}

void wxGDIPlusContext::Clip( wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    m_context->SetClip(RectF(x,y,w,h),CombineModeIntersect);
}

void wxGDIPlusContext::ResetClip()
{
    m_context->ResetClip();
}

void wxGDIPlusContext::DrawRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxGDIPlusOffsetHelper helper( m_context , ShouldOffset() );
    Brush *brush = m_brush.IsNull() ? NULL : ((wxGDIPlusBrushData*)m_brush.GetRefData())->GetGDIPlusBrush();
    Pen *pen = m_pen.IsNull() ? NULL : ((wxGDIPlusPenData*)m_pen.GetGraphicsData())->GetGDIPlusPen();

    if ( brush )
    {
        // the offset is used to fill only the inside of the rectangle and not paint underneath
        // its border which may influence a transparent Pen
        REAL offset = 0;
        if ( pen )
             offset = pen->GetWidth();
        m_context->FillRectangle( brush, (REAL)x + offset/2, (REAL)y + offset/2, (REAL)w - offset, (REAL)h - offset);
    }

    if ( pen )
    {
        m_context->DrawRectangle( pen, (REAL)x, (REAL)y, (REAL)w, (REAL)h );
    }
}

void wxGDIPlusContext::StrokeLines( size_t n, const wxPoint2DDouble *points)
{
   if (m_composition == wxCOMPOSITION_DEST)
        return;

   if ( !m_pen.IsNull() )
   {
       wxGDIPlusOffsetHelper helper( m_context , ShouldOffset() );
       PointF *cpoints = new PointF[n];
       for (size_t i = 0; i < n; i++)
       {
           cpoints[i].X = static_cast<REAL>(points[i].m_x);
           cpoints[i].Y = static_cast<REAL>(points[i].m_y);

       } // for (size_t i = 0; i < n; i++)
       m_context->DrawLines( ((wxGDIPlusPenData*)m_pen.GetGraphicsData())->GetGDIPlusPen() , cpoints , n ) ;
       delete[] cpoints;
   }
}

void wxGDIPlusContext::DrawLines( size_t n, const wxPoint2DDouble *points, wxPolygonFillMode WXUNUSED(fillStyle) )
{
   if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxGDIPlusOffsetHelper helper( m_context , ShouldOffset() );
    PointF *cpoints = new PointF[n];
    for (size_t i = 0; i < n; i++)
    {
        cpoints[i].X = static_cast<REAL>(points[i].m_x);
        cpoints[i].Y = static_cast<REAL>(points[i].m_y);

    } // for (int i = 0; i < n; i++)
    if ( !m_brush.IsNull() )
        m_context->FillPolygon( ((wxGDIPlusBrushData*)m_brush.GetRefData())->GetGDIPlusBrush() , cpoints , n ) ;
    if ( !m_pen.IsNull() )
        m_context->DrawLines( ((wxGDIPlusPenData*)m_pen.GetGraphicsData())->GetGDIPlusPen() , cpoints , n ) ;
    delete[] cpoints;
}

void wxGDIPlusContext::StrokePath( const wxGraphicsPath& path )
{
   if (m_composition == wxCOMPOSITION_DEST)
        return;

    if ( !m_pen.IsNull() )
    {
        wxGDIPlusOffsetHelper helper( m_context , ShouldOffset() );
        m_context->DrawPath( ((wxGDIPlusPenData*)m_pen.GetGraphicsData())->GetGDIPlusPen() , (GraphicsPath*) path.GetNativePath() );
    }
}

void wxGDIPlusContext::FillPath( const wxGraphicsPath& path , wxPolygonFillMode fillStyle )
{
   if (m_composition == wxCOMPOSITION_DEST)
        return;

    if ( !m_brush.IsNull() )
    {
        wxGDIPlusOffsetHelper helper( m_context , ShouldOffset() );
        ((GraphicsPath*) path.GetNativePath())->SetFillMode( fillStyle == wxODDEVEN_RULE ? FillModeAlternate : FillModeWinding);
        m_context->FillPath( ((wxGDIPlusBrushData*)m_brush.GetRefData())->GetGDIPlusBrush() ,
            (GraphicsPath*) path.GetNativePath());
    }
}

bool wxGDIPlusContext::SetAntialiasMode(wxAntialiasMode antialias)
{
    if (m_antialias == antialias)
        return true;

    m_antialias = antialias;

    SmoothingMode antialiasMode;
    switch (antialias)
    {
        case wxANTIALIAS_DEFAULT:
            antialiasMode = SmoothingModeHighQuality;
            break;
        case wxANTIALIAS_NONE:
            antialiasMode = SmoothingModeNone;
            break;
        default:
            return false;
    }
    m_context->SetSmoothingMode(antialiasMode);
    return true;
}

bool wxGDIPlusContext::SetInterpolationQuality(wxInterpolationQuality interpolation)
{
    if (m_interpolation == interpolation)
        return true;

    InterpolationMode interpolationMode = InterpolationModeDefault;
    switch (interpolation)
    {
        case wxINTERPOLATION_DEFAULT:
            interpolationMode = InterpolationModeDefault;
            break;

        case wxINTERPOLATION_NONE:
            interpolationMode = InterpolationModeNearestNeighbor;
            break;

        case wxINTERPOLATION_FAST:
            interpolationMode = InterpolationModeLowQuality;
            break;

        case wxINTERPOLATION_GOOD:
            interpolationMode = InterpolationModeHighQuality;
            break;

        case wxINTERPOLATION_BEST:
            interpolationMode = InterpolationModeHighQualityBicubic;
            break;

        default:
            return false;
    }

    if ( m_context->SetInterpolationMode(interpolationMode) != Gdiplus::Ok )
        return false;

    m_interpolation = interpolation;

    return true;
}

bool wxGDIPlusContext::SetCompositionMode(wxCompositionMode op)
{
    if ( m_composition == op )
        return true;

    m_composition = op;

    if (m_composition == wxCOMPOSITION_DEST)
        return true;

    CompositingMode cop;
    switch (op)
    {
        case wxCOMPOSITION_SOURCE:
            cop = CompositingModeSourceCopy;
            break;
        case wxCOMPOSITION_OVER:
            cop = CompositingModeSourceOver;
            break;
        default:
            return false;
    }

    m_context->SetCompositingMode(cop);
    return true;
}

void wxGDIPlusContext::BeginLayer(wxDouble /* opacity */)
{
    // TODO
}

void wxGDIPlusContext::EndLayer()
{
    // TODO
}

void wxGDIPlusContext::Rotate( wxDouble angle )
{
    m_context->RotateTransform( RadToDeg(angle) );
}

void wxGDIPlusContext::Translate( wxDouble dx , wxDouble dy )
{
    m_context->TranslateTransform( dx , dy );
}

void wxGDIPlusContext::Scale( wxDouble xScale , wxDouble yScale )
{
    m_context->ScaleTransform(xScale,yScale);
}

void wxGDIPlusContext::PushState()
{
    GraphicsState state = m_context->Save();
    m_stateStack.push(state);
}

void wxGDIPlusContext::PopState()
{
    wxCHECK_RET( !m_stateStack.empty(), wxT("No state to pop") );

    GraphicsState state = m_stateStack.top();
    m_stateStack.pop();
    m_context->Restore(state);
}

void wxGDIPlusContext::DrawBitmap( const wxGraphicsBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
   if (m_composition == wxCOMPOSITION_DEST)
        return;

    Bitmap* image = static_cast<wxGDIPlusBitmapData*>(bmp.GetRefData())->GetGDIPlusBitmap();
    if ( image )
    {
        if( image->GetWidth() != (UINT) w || image->GetHeight() != (UINT) h )
        {
            Rect drawRect((REAL) x, (REAL)y, (REAL)w, (REAL)h);
            m_context->SetPixelOffsetMode( PixelOffsetModeNone );
            m_context->DrawImage(image, drawRect, 0 , 0 , image->GetWidth(), image->GetHeight(), UnitPixel ) ;
            m_context->SetPixelOffsetMode( PixelOffsetModeHalf );
        }
        else
            m_context->DrawImage(image,(REAL) x,(REAL) y,(REAL) w,(REAL) h) ;
    }
}

void wxGDIPlusContext::DrawBitmap( const wxBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    wxGraphicsBitmap bitmap = GetRenderer()->CreateBitmap(bmp);
    DrawBitmap(bitmap, x, y, w, h);
}

void wxGDIPlusContext::DrawIcon( const wxIcon &icon, wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
   if (m_composition == wxCOMPOSITION_DEST)
        return;

    // the built-in conversion fails when there is alpha in the HICON (eg XP style icons), we can only
    // find out by looking at the bitmap data whether there really was alpha in it
    HICON hIcon = (HICON)icon.GetHICON();
    ICONINFO iconInfo ;
    // IconInfo creates the bitmaps for color and mask, we must dispose of them after use
    if (!GetIconInfo(hIcon,&iconInfo))
        return;

    Bitmap interim(iconInfo.hbmColor,NULL);

    Bitmap* image = NULL ;

    // if it's not 32 bit, it doesn't have an alpha channel, note that since the conversion doesn't
    // work correctly, asking IsAlphaPixelFormat at this point fails as well
    if( GetPixelFormatSize(interim.GetPixelFormat())!= 32 )
    {
        image = Bitmap::FromHICON(hIcon);
    }
    else
    {
        size_t width = interim.GetWidth();
        size_t height = interim.GetHeight();
        Rect bounds(0,0,width,height);
        BitmapData data ;

        interim.LockBits(&bounds, ImageLockModeRead,
            interim.GetPixelFormat(),&data);

        bool hasAlpha = false;
        for ( size_t yy = 0 ; yy < height && !hasAlpha ; ++yy)
        {
            for( size_t xx = 0 ; xx < width && !hasAlpha; ++xx)
            {
                ARGB *dest = (ARGB*)((BYTE*)data.Scan0 + data.Stride*yy + xx*4);
                if ( ( *dest & Color::AlphaMask ) != 0 )
                    hasAlpha = true;
            }
        }

        if ( hasAlpha )
        {
        image = new Bitmap(data.Width, data.Height, data.Stride,
            PixelFormat32bppARGB , (BYTE*) data.Scan0);
        }
        else
        {
            image = Bitmap::FromHICON(hIcon);
        }

        interim.UnlockBits(&data);
    }

    m_context->DrawImage(image,(REAL) x,(REAL) y,(REAL) w,(REAL) h) ;

    delete image ;
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
}

void wxGDIPlusContext::DoDrawText(const wxString& str,
                                        wxDouble x, wxDouble y )
{
   if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxCHECK_RET( !m_font.IsNull(),
                 wxT("wxGDIPlusContext::DrawText - no valid font set") );

    if ( str.IsEmpty())
        return ;

    wxGDIPlusFontData * const
        fontData = (wxGDIPlusFontData *)m_font.GetRefData();
 
    m_context->DrawString
               (
                    str.wc_str(*wxConvUI),  // string to draw, always Unicode
                    -1,                     // length: string is NUL-terminated
                    fontData->GetGDIPlusFont(),
                    PointF(x, y),
                    GetDrawTextStringFormat(),
                    fontData->GetGDIPlusBrush()
               );
}

void wxGDIPlusContext::GetTextExtent( const wxString &str, wxDouble *width, wxDouble *height,
                                     wxDouble *descent, wxDouble *externalLeading ) const
{
    wxCHECK_RET( !m_font.IsNull(), wxT("wxGDIPlusContext::GetTextExtent - no valid font set") );

    wxWCharBuffer s = str.wc_str( *wxConvUI );
    FontFamily ffamily ;
    Font* f = ((wxGDIPlusFontData*)m_font.GetRefData())->GetGDIPlusFont();

    f->GetFamily(&ffamily) ;

    REAL factorY = m_fontScaleRatio;

    // Notice that we must use the real font style or the results would be
    // incorrect for italic/bold fonts.
    const INT style = f->GetStyle();
    const REAL size = f->GetSize();
    const REAL emHeight = ffamily.GetEmHeight(style);
    REAL rDescent = ffamily.GetCellDescent(style) * size / emHeight;
    REAL rAscent = ffamily.GetCellAscent(style) * size / emHeight;
    REAL rHeight = ffamily.GetLineSpacing(style) * size / emHeight;

    if ( height )
        *height = rHeight * factorY;
    if ( descent )
        *descent = rDescent * factorY;
    if ( externalLeading )
        *externalLeading = (rHeight - rAscent - rDescent) * factorY;
    // measuring empty strings is not guaranteed, so do it by hand
    if ( str.IsEmpty())
    {
        if ( width )
            *width = 0 ;
    }
    else
    {
        RectF layoutRect(0,0, 100000.0f, 100000.0f);

        RectF bounds ;
        m_context->MeasureString((const wchar_t *) s , wcslen(s) , f, layoutRect, GetDrawTextStringFormat(), &bounds ) ;
        if ( width )
            *width = bounds.Width;
        if ( height )
            *height = bounds.Height;
    }
}

void wxGDIPlusContext::GetPartialTextExtents(const wxString& text, wxArrayDouble& widths) const
{
    widths.Empty();
    widths.Add(0, text.length());

    wxCHECK_RET( !m_font.IsNull(), wxT("wxGDIPlusContext::GetPartialTextExtents - no valid font set") );

    if (text.empty())
        return;

    Font* f = ((wxGDIPlusFontData*)m_font.GetRefData())->GetGDIPlusFont();
    wxWCharBuffer ws = text.wc_str( *wxConvUI );
    size_t len = wcslen( ws ) ;
    wxASSERT_MSG(text.length() == len , wxT("GetPartialTextExtents not yet implemented for multichar situations"));

    RectF layoutRect(0,0, 100000.0f, 100000.0f);
    StringFormat strFormat( GetDrawTextStringFormat() );

    size_t startPosition = 0;
    size_t remainder = len;
    const size_t maxSpan = 32;
    CharacterRange* ranges = new CharacterRange[maxSpan] ;
    Region* regions = new Region[maxSpan];

    while( remainder > 0 )
    {
        size_t span = wxMin( maxSpan, remainder );

        for( size_t i = 0 ; i < span ; ++i)
        {
            ranges[i].First = 0 ;
            ranges[i].Length = startPosition+i+1 ;
        }
        strFormat.SetMeasurableCharacterRanges(span,ranges);
        m_context->MeasureCharacterRanges(ws, -1 , f,layoutRect, &strFormat,span,regions) ;

        RectF bbox ;
        for ( size_t i = 0 ; i < span ; ++i)
        {
            regions[i].GetBounds(&bbox,m_context);
            widths[startPosition+i] = bbox.Width;
        }
        remainder -= span;
        startPosition += span;
    }

    delete[] ranges;
    delete[] regions;
}

bool wxGDIPlusContext::ShouldOffset() const
{
    if ( !m_enableOffset )
        return false;
    
    int penwidth = 0 ;
    if ( !m_pen.IsNull() )
    {
        penwidth = (int)((wxGDIPlusPenData*)m_pen.GetRefData())->GetWidth();
        if ( penwidth == 0 )
            penwidth = 1;
    }
    return ( penwidth % 2 ) == 1;
}

void* wxGDIPlusContext::GetNativeContext()
{
    return m_context;
}

// concatenates this transform with the current transform of this context
void wxGDIPlusContext::ConcatTransform( const wxGraphicsMatrix& matrix )
{
    m_context->MultiplyTransform((Matrix*) matrix.GetNativeMatrix());
}

// sets the transform of this context
void wxGDIPlusContext::SetTransform( const wxGraphicsMatrix& matrix )
{
    m_context->SetTransform((Matrix*) matrix.GetNativeMatrix());
}

// gets the matrix of this context
wxGraphicsMatrix wxGDIPlusContext::GetTransform() const
{
    wxGraphicsMatrix matrix = CreateMatrix();
    m_context->GetTransform((Matrix*) matrix.GetNativeMatrix());
    return matrix;
}

void wxGDIPlusContext::GetSize( wxDouble* width, wxDouble *height )
{
    *width = m_width;
    *height = m_height;
}

//-----------------------------------------------------------------------------
// wxGDIPlusPrintingContext implementation
//-----------------------------------------------------------------------------

wxGDIPlusPrintingContext::wxGDIPlusPrintingContext( wxGraphicsRenderer* renderer,
                                                    const wxDC& dc )
    : wxGDIPlusContext(renderer, dc)
{
    Graphics* context = GetGraphics();

    //m_context->SetPageUnit(UnitDocument);

    // Setup page scale, based on DPI ratio.
    // Antecedent should be 100dpi when the default page unit
    // (UnitDisplay) is used. Page unit UnitDocument would require 300dpi
    // instead. Note that calling SetPageScale() does not have effect on
    // non-printing DCs (that is, any other than wxPrinterDC or
    // wxEnhMetaFileDC).
    REAL dpiRatio = 100.0 / context->GetDpiY();
    context->SetPageScale(dpiRatio);

    // We use this modifier when measuring fonts. It is needed because the
    // page scale is modified above.
    m_fontScaleRatio = context->GetDpiY() / 72.0;
}

//-----------------------------------------------------------------------------
// wxGDIPlusRenderer implementation
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxGDIPlusRenderer,wxGraphicsRenderer)

static wxGDIPlusRenderer gs_GDIPlusRenderer;

wxGraphicsRenderer* wxGraphicsRenderer::GetDefaultRenderer()
{
    return &gs_GDIPlusRenderer;
}

bool wxGDIPlusRenderer::EnsureIsLoaded()
{
    // load gdiplus.dll if not yet loaded, but don't bother doing it again
    // if we already tried and failed (we don't want to spend lot of time
    // returning NULL from wxGraphicsContext::Create(), which may be called
    // relatively frequently):
    if ( m_loaded == -1 )
    {
        Load();
    }

    return m_loaded == 1;
}

// call EnsureIsLoaded() and return returnOnFail value if it fails
#define ENSURE_LOADED_OR_RETURN(returnOnFail)  \
    if ( !EnsureIsLoaded() )                   \
        return (returnOnFail)


void wxGDIPlusRenderer::Load()
{
    GdiplusStartupInput input;
    GdiplusStartupOutput output;
    if ( GdiplusStartup(&m_gditoken,&input,&output) == Gdiplus::Ok )
    {
        wxLogTrace("gdiplus", "successfully initialized GDI+");
        m_loaded = 1;
    }
    else
    {
        wxLogTrace("gdiplus", "failed to initialize GDI+, missing gdiplus.dll?");
        m_loaded = 0;
    }
}

void wxGDIPlusRenderer::Unload()
{
    if ( m_gditoken )
    {
        GdiplusShutdown(m_gditoken);
        m_gditoken = 0;
    }
    m_loaded = -1; // next Load() will try again
}

wxGraphicsContext * wxGDIPlusRenderer::CreateContext( const wxWindowDC& dc)
{
    ENSURE_LOADED_OR_RETURN(NULL);
    wxGDIPlusContext* context = new wxGDIPlusContext(this, dc);
    context->EnableOffset(true);
    return context;
}

#if wxUSE_PRINTING_ARCHITECTURE
wxGraphicsContext * wxGDIPlusRenderer::CreateContext( const wxPrinterDC& dc)
{
    ENSURE_LOADED_OR_RETURN(NULL);
    wxGDIPlusContext* context = new wxGDIPlusPrintingContext(this, dc);
    return context;
}
#endif

#if wxUSE_ENH_METAFILE
wxGraphicsContext * wxGDIPlusRenderer::CreateContext( const wxEnhMetaFileDC& dc)
{
    ENSURE_LOADED_OR_RETURN(NULL);
    wxGDIPlusContext* context = new wxGDIPlusPrintingContext(this, dc);
    return context;
}
#endif

wxGraphicsContext * wxGDIPlusRenderer::CreateContext( const wxMemoryDC& dc)
{
    ENSURE_LOADED_OR_RETURN(NULL);
    wxGDIPlusContext* context = new wxGDIPlusContext(this, dc);
    context->EnableOffset(true);
    return context;
}

#if wxUSE_IMAGE
wxGraphicsContext * wxGDIPlusRenderer::CreateContextFromImage(wxImage& image)
{
    ENSURE_LOADED_OR_RETURN(NULL);
    wxGDIPlusContext* context = new wxGDIPlusImageContext(this, image);
    context->EnableOffset(true);
    return context;
}

#endif // wxUSE_IMAGE

wxGraphicsContext * wxGDIPlusRenderer::CreateMeasuringContext()
{
    ENSURE_LOADED_OR_RETURN(NULL);
    return new wxGDIPlusMeasuringContext(this);
}

wxGraphicsContext * wxGDIPlusRenderer::CreateContextFromNativeContext( void * context )
{
    ENSURE_LOADED_OR_RETURN(NULL);
    return new wxGDIPlusContext(this,(Graphics*) context);
}


wxGraphicsContext * wxGDIPlusRenderer::CreateContextFromNativeWindow( void * window )
{
    ENSURE_LOADED_OR_RETURN(NULL);
    return new wxGDIPlusContext(this,(HWND) window);
}

wxGraphicsContext * wxGDIPlusRenderer::CreateContext( wxWindow* window )
{
    ENSURE_LOADED_OR_RETURN(NULL);
    return new wxGDIPlusContext(this, (HWND) window->GetHWND() );
}

// Path

wxGraphicsPath wxGDIPlusRenderer::CreatePath()
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsPath);
    wxGraphicsPath m;
    m.SetRefData( new wxGDIPlusPathData(this));
    return m;
}


// Matrix

wxGraphicsMatrix wxGDIPlusRenderer::CreateMatrix( wxDouble a, wxDouble b, wxDouble c, wxDouble d,
                                                           wxDouble tx, wxDouble ty)

{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsMatrix);
    wxGraphicsMatrix m;
    wxGDIPlusMatrixData* data = new wxGDIPlusMatrixData( this );
    data->Set( a,b,c,d,tx,ty ) ;
    m.SetRefData(data);
    return m;
}

wxGraphicsPen wxGDIPlusRenderer::CreatePen(const wxPen& pen)
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsPen);
    if ( !pen.IsOk() || pen.GetStyle() == wxTRANSPARENT )
        return wxNullGraphicsPen;
    else
    {
        wxGraphicsPen p;
        p.SetRefData(new wxGDIPlusPenData( this, pen ));
        return p;
    }
}

wxGraphicsBrush wxGDIPlusRenderer::CreateBrush(const wxBrush& brush )
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsBrush);
    if ( !brush.IsOk() || brush.GetStyle() == wxTRANSPARENT )
        return wxNullGraphicsBrush;
    else
    {
        wxGraphicsBrush p;
        p.SetRefData(new wxGDIPlusBrushData( this, brush ));
        return p;
    }
}

wxGraphicsBrush
wxGDIPlusRenderer::CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                                             wxDouble x2, wxDouble y2,
                                             const wxGraphicsGradientStops& stops)
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsBrush);
    wxGraphicsBrush p;
    wxGDIPlusBrushData* d = new wxGDIPlusBrushData( this );
    d->CreateLinearGradientBrush(x1, y1, x2, y2, stops);
    p.SetRefData(d);
    return p;
 }

wxGraphicsBrush
wxGDIPlusRenderer::CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                                             wxDouble xc, wxDouble yc,
                                             wxDouble radius,
                                             const wxGraphicsGradientStops& stops)
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsBrush);
    wxGraphicsBrush p;
    wxGDIPlusBrushData* d = new wxGDIPlusBrushData( this );
    d->CreateRadialGradientBrush(xo,yo,xc,yc,radius,stops);
    p.SetRefData(d);
    return p;
}

wxGraphicsFont
wxGDIPlusRenderer::CreateFont( const wxFont &font,
                               const wxColour &col )
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsFont);
    if ( font.IsOk() )
    {
        wxGraphicsFont p;
        p.SetRefData(new wxGDIPlusFontData( this, font, col ));
        return p;
    }
    else
        return wxNullGraphicsFont;
}

wxGraphicsFont
wxGDIPlusRenderer::CreateFont(double size,
                              const wxString& facename,
                              int flags,
                              const wxColour& col)
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsFont);

    // Convert wxFont flags to GDI+ style:
    int style = FontStyleRegular;
    if ( flags & wxFONTFLAG_ITALIC )
        style |= FontStyleItalic;
    if ( flags & wxFONTFLAG_UNDERLINED )
        style |= FontStyleUnderline;
    if ( flags & wxFONTFLAG_BOLD )
        style |= FontStyleBold;
    if ( flags & wxFONTFLAG_STRIKETHROUGH )
        style |= FontStyleStrikeout;


    wxGraphicsFont f;
    f.SetRefData(new wxGDIPlusFontData(this, facename, size, style, col));
    return f;
}

wxGraphicsBitmap wxGDIPlusRenderer::CreateBitmap( const wxBitmap &bitmap )
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsBitmap);
    if ( bitmap.IsOk() )
    {
        wxGraphicsBitmap p;
        p.SetRefData(new wxGDIPlusBitmapData( this , bitmap ));
        return p;
    }
    else
        return wxNullGraphicsBitmap;
}

#if wxUSE_IMAGE

wxGraphicsBitmap wxGDIPlusRenderer::CreateBitmapFromImage(const wxImage& image)
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsBitmap);
    if ( image.IsOk() )
    {
        // Notice that we rely on conversion from wxImage to wxBitmap here but
        // we could probably do it more efficiently by converting from wxImage
        // to GDI+ Bitmap directly, i.e. copying wxImage pixels to the buffer
        // returned by Bitmap::LockBits(). However this would require writing
        // code specific for this task while like this we can reuse existing
        // code (see also wxGDIPlusBitmapData::ConvertToImage()).
        wxGraphicsBitmap gb;
        gb.SetRefData(new wxGDIPlusBitmapData(this, image));
        return gb;
    }
    else
        return wxNullGraphicsBitmap;
}


wxImage wxGDIPlusRenderer::CreateImageFromBitmap(const wxGraphicsBitmap& bmp)
{
    ENSURE_LOADED_OR_RETURN(wxNullImage);
    const wxGDIPlusBitmapData* const
        data = static_cast<wxGDIPlusBitmapData*>(bmp.GetGraphicsData());

    return data ? data->ConvertToImage() : wxNullImage;
}

#endif // wxUSE_IMAGE


wxGraphicsBitmap wxGDIPlusRenderer::CreateBitmapFromNativeBitmap( void *bitmap )
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsBitmap);
    if ( bitmap != NULL )
    {
        wxGraphicsBitmap p;
        p.SetRefData(new wxGDIPlusBitmapData( this , (Bitmap*) bitmap ));
        return p;
    }
    else
        return wxNullGraphicsBitmap;
}

wxGraphicsBitmap wxGDIPlusRenderer::CreateSubBitmap( const wxGraphicsBitmap &bitmap, wxDouble x, wxDouble y, wxDouble w, wxDouble h  )
{
    ENSURE_LOADED_OR_RETURN(wxNullGraphicsBitmap);
    Bitmap* image = static_cast<wxGDIPlusBitmapData*>(bitmap.GetRefData())->GetGDIPlusBitmap();
    if ( image )
    {
        wxGraphicsBitmap p;
        p.SetRefData(new wxGDIPlusBitmapData( this , image->Clone( (REAL) x , (REAL) y , (REAL) w , (REAL) h , PixelFormat32bppPARGB) ));
        return p;
    }
    else
        return wxNullGraphicsBitmap;
}

// Shutdown GDI+ at app exit, before possible dll unload
class wxGDIPlusRendererModule : public wxModule
{
public:
    wxGDIPlusRendererModule()
    {
        // We must be uninitialized before GDI+ DLL itself is unloaded.
        AddDependency("wxGdiPlusModule");
    }

    virtual bool OnInit() { return true; }
    virtual void OnExit()
    {
        wxDELETE(gs_drawTextStringFormat);

        gs_GDIPlusRenderer.Unload();
    }

private:
    DECLARE_DYNAMIC_CLASS(wxGDIPlusRendererModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxGDIPlusRendererModule, wxModule)

// ----------------------------------------------------------------------------
// wxMSW-specific parts of wxGCDC
// ----------------------------------------------------------------------------

WXHDC wxGCDC::AcquireHDC()
{
    wxGraphicsContext * const gc = GetGraphicsContext();
    if ( !gc )
        return NULL;

#if wxUSE_CAIRO
    // we can't get the HDC if it is not a GDI+ context
    wxGraphicsRenderer* r1 = gc->GetRenderer();
    wxGraphicsRenderer* r2 = wxGraphicsRenderer::GetCairoRenderer();
    if (r1 == r2)
        return NULL;
#endif

    Graphics * const g = static_cast<Graphics *>(gc->GetNativeContext());
    return g ? g->GetHDC() : NULL;
}

void wxGCDC::ReleaseHDC(WXHDC hdc)
{
    if ( !hdc )
        return;

    wxGraphicsContext * const gc = GetGraphicsContext();
    wxCHECK_RET( gc, "can't release HDC because there is no wxGraphicsContext" );

#if wxUSE_CAIRO
    // we can't get the HDC if it is not a GDI+ context
    wxGraphicsRenderer* r1 = gc->GetRenderer();
    wxGraphicsRenderer* r2 = wxGraphicsRenderer::GetCairoRenderer();
    if (r1 == r2)
        return;
#endif

    Graphics * const g = static_cast<Graphics *>(gc->GetNativeContext());
    wxCHECK_RET( g, "can't release HDC because there is no Graphics" );

    g->ReleaseHDC((HDC)hdc);
}

#endif  // wxUSE_GRAPHICS_CONTEXT
