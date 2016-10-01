/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/graphicc.cpp
// Purpose:     cairo device context class
// Author:      Stefan Csomor
// Modified by:
// Created:     2006-10-03
// Copyright:   (c) 2006 Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_GRAPHICS_CONTEXT

#include "wx/graphics.h"

#if wxUSE_CAIRO

// keep cairo.h from defining dllimport as we're defining the symbols inside
// the wx dll in order to load them dynamically.
#define cairo_public 

#include <cairo.h>

bool wxCairoInit();

#ifndef WX_PRECOMP
    #include "wx/bitmap.h"
    #include "wx/icon.h"
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
    #include "wx/dcprint.h"
    #include "wx/window.h"
#endif

#include "wx/private/graphics.h"
#include "wx/rawbmp.h"
#include "wx/vector.h"

using namespace std;

//-----------------------------------------------------------------------------
// device context implementation
//
// more and more of the dc functionality should be implemented by calling
// the appropricate wxCairoContext, but we will have to do that step by step
// also coordinate conversions should be moved to native matrix ops
//-----------------------------------------------------------------------------

// we always stock two context states, one at entry, to be able to preserve the
// state we were called with, the other one after changing to HI Graphics orientation
// (this one is used for getting back clippings etc)

//-----------------------------------------------------------------------------
// wxGraphicsPath implementation
//-----------------------------------------------------------------------------

// TODO remove this dependency (gdiplus needs the macros)

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#include <cairo.h>
#ifdef __WXMSW__
#include <cairo-win32.h>
// Notice that the order is important: cairo-win32.h includes windows.h which
// pollutes the global name space with macros so include our own header which
// #undefines them after it.
#include "wx/msw/private.h"
#endif

#ifdef __WXGTK__
#include <gtk/gtk.h>
#include "wx/fontutil.h"
#ifndef __WXGTK3__
#include "wx/gtk/dc.h"
#endif
#endif

#ifdef __WXMAC__
#include "wx/osx/private.h"
#include <cairo-quartz.h>
#include <cairo-atsui.h>
#endif

class WXDLLIMPEXP_CORE wxCairoPathData : public wxGraphicsPathData
{
public :
    wxCairoPathData(wxGraphicsRenderer* renderer, cairo_t* path = NULL);
    ~wxCairoPathData();

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

    /*

    // appends a rectangle as a new closed subpath
    virtual void AddRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h ) ;
    // appends an ellipsis as a new closed subpath fitting the passed rectangle
    virtual void AddEllipsis( wxDouble x, wxDouble y, wxDouble w , wxDouble h ) ;

    // draws a an arc to two tangents connecting (current) to (x1,y1) and (x1,y1) to (x2,y2), also a straight line from (current) to (x1,y1)
    virtual void AddArcToPoint( wxDouble x1, wxDouble y1 , wxDouble x2, wxDouble y2, wxDouble r )  ;
    */

    // returns the native path
    virtual void * GetNativePath() const ;

    // give the native path returned by GetNativePath() back (there might be some deallocations necessary)
    virtual void UnGetNativePath(void *p) const;

    // transforms each point of this path by the matrix
    virtual void Transform( const wxGraphicsMatrixData* matrix ) ;

    // gets the bounding box enclosing all points (possibly including control points)
    virtual void GetBox(wxDouble *x, wxDouble *y, wxDouble *w, wxDouble *h) const;

    virtual bool Contains( wxDouble x, wxDouble y, wxPolygonFillMode fillStyle = wxWINDING_RULE) const;

private :
    cairo_t* m_pathContext;
};

class WXDLLIMPEXP_CORE wxCairoMatrixData : public wxGraphicsMatrixData
{
public :
    wxCairoMatrixData(wxGraphicsRenderer* renderer, const cairo_matrix_t* matrix = NULL ) ;
    virtual ~wxCairoMatrixData() ;

    virtual wxGraphicsObjectRefData *Clone() const ;

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
    cairo_matrix_t m_matrix ;
} ;

// Common base class for pens and brushes.
class wxCairoPenBrushBaseData : public wxGraphicsObjectRefData
{
public:
    wxCairoPenBrushBaseData(wxGraphicsRenderer* renderer,
                            const wxColour& col,
                            bool isTransparent);
    virtual ~wxCairoPenBrushBaseData();

    virtual void Apply( wxGraphicsContext* context );

protected:
    // Call this to use the given bitmap as stipple. Bitmap must be non-null
    // and valid.
    void InitStipple(wxBitmap* bmp);

    // Call this to use the given hatch style. Hatch style must be valid.
    void InitHatch(wxHatchStyle hatchStyle);


    double m_red;
    double m_green;
    double m_blue;
    double m_alpha;

    cairo_pattern_t* m_pattern;
    class wxCairoBitmapData* m_bmpdata;

private:
    // Called once to allocate m_pattern if needed.
    void InitHatchPattern(cairo_t* ctext);

    wxHatchStyle m_hatchStyle;

    wxDECLARE_NO_COPY_CLASS(wxCairoPenBrushBaseData);
};

class WXDLLIMPEXP_CORE wxCairoPenData : public wxCairoPenBrushBaseData
{
public:
    wxCairoPenData( wxGraphicsRenderer* renderer, const wxPen &pen );
    ~wxCairoPenData();

    void Init();

    virtual void Apply( wxGraphicsContext* context );
    virtual wxDouble GetWidth() { return m_width; }

private :
    double m_width;

    cairo_line_cap_t m_cap;
    cairo_line_join_t m_join;

    int m_count;
    const double *m_lengths;
    double *m_userLengths;

    wxDECLARE_NO_COPY_CLASS(wxCairoPenData);
};

class WXDLLIMPEXP_CORE wxCairoBrushData : public wxCairoPenBrushBaseData
{
public:
    wxCairoBrushData( wxGraphicsRenderer* renderer );
    wxCairoBrushData( wxGraphicsRenderer* renderer, const wxBrush &brush );

    void CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                                   wxDouble x2, wxDouble y2,
                                   const wxGraphicsGradientStops& stops);
    void CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                                   wxDouble xc, wxDouble yc, wxDouble radius,
                                   const wxGraphicsGradientStops& stops);

protected:
    virtual void Init();

    // common part of Create{Linear,Radial}GradientBrush()
    void AddGradientStops(const wxGraphicsGradientStops& stops);
};

class wxCairoFontData : public wxGraphicsObjectRefData
{
public:
    wxCairoFontData( wxGraphicsRenderer* renderer, const wxFont &font, const wxColour& col );
    wxCairoFontData(wxGraphicsRenderer* renderer,
                    double sizeInPixels,
                    const wxString& facename,
                    int flags,
                    const wxColour& col);
    ~wxCairoFontData();

    virtual bool Apply( wxGraphicsContext* context );
#ifdef __WXGTK__
    const wxFont& GetFont() const { return m_wxfont; }
#endif
private :
    void InitColour(const wxColour& col);
    void InitFontComponents(const wxString& facename,
                            cairo_font_slant_t slant,
                            cairo_font_weight_t weight);

    double m_size;
    double m_red;
    double m_green;
    double m_blue;
    double m_alpha;
#ifdef __WXMAC__
    cairo_font_face_t *m_font;
#elif defined(__WXGTK__)
    wxFont m_wxfont;
#endif

    // These members are used when the font is created from its face name and
    // flags (and not from wxFont) and also even when creating it from wxFont
    // on the platforms not covered above.
    //
    // Notice that we can't use cairo_font_face_t instead of storing those,
    // even though it would be simpler and need less #ifdefs, because
    // cairo_toy_font_face_create() that we'd need to create it is only
    // available in Cairo 1.8 and we require just 1.2 currently. If we do drop
    // support for < 1.8 versions in the future it would be definitely better
    // to use cairo_toy_font_face_create() instead.
    wxCharBuffer m_fontName;
    cairo_font_slant_t m_slant;
    cairo_font_weight_t m_weight;
};

class wxCairoBitmapData : public wxGraphicsBitmapData
{
public:
    wxCairoBitmapData( wxGraphicsRenderer* renderer, const wxBitmap& bmp );
#if wxUSE_IMAGE
    wxCairoBitmapData(wxGraphicsRenderer* renderer, const wxImage& image);
#endif // wxUSE_IMAGE
    wxCairoBitmapData( wxGraphicsRenderer* renderer, cairo_surface_t* bitmap );
    ~wxCairoBitmapData();

    virtual cairo_surface_t* GetCairoSurface() { return m_surface; }
    virtual cairo_pattern_t* GetCairoPattern() { return m_pattern; }
    virtual void* GetNativeBitmap() const { return m_surface; }
    virtual wxSize GetSize() { return wxSize(m_width, m_height); }

#if wxUSE_IMAGE
    wxImage ConvertToImage() const;
#endif // wxUSE_IMAGE

private :
    // Allocate m_buffer for the bitmap of the given size in the given format.
    //
    // Returns the stride used for the buffer.
    int InitBuffer(int width, int height, cairo_format_t format);

    // Really create the surface using the buffer (which was supposed to be
    // filled since InitBuffer() call).
    void InitSurface(cairo_format_t format, int stride);


    cairo_surface_t* m_surface;
    cairo_pattern_t* m_pattern;
    int m_width;
    int m_height;
    unsigned char* m_buffer;
};

class WXDLLIMPEXP_CORE wxCairoContext : public wxGraphicsContext
{
public:
    wxCairoContext( wxGraphicsRenderer* renderer, const wxWindowDC& dc );
    wxCairoContext( wxGraphicsRenderer* renderer, const wxMemoryDC& dc );
#if wxUSE_PRINTING_ARCHITECTURE
    wxCairoContext( wxGraphicsRenderer* renderer, const wxPrinterDC& dc );
#endif
#ifdef __WXGTK__
    wxCairoContext( wxGraphicsRenderer* renderer, GdkWindow *window );
#endif
#ifdef __WXMSW__
    wxCairoContext( wxGraphicsRenderer* renderer, HDC context );
#endif
    wxCairoContext( wxGraphicsRenderer* renderer, cairo_t *context );
    wxCairoContext( wxGraphicsRenderer* renderer, wxWindow *window);

    // If this ctor is used, Init() must be called by the derived class later.
    wxCairoContext( wxGraphicsRenderer* renderer );

    virtual ~wxCairoContext();

    virtual bool ShouldOffset() const
    {
        if ( !m_enableOffset )
            return false;
        
        int penwidth = 0 ;
        if ( !m_pen.IsNull() )
        {
            penwidth = (int)((wxCairoPenData*)m_pen.GetRefData())->GetWidth();
            if ( penwidth == 0 )
                penwidth = 1;
        }
        return ( penwidth % 2 ) == 1;
    }

    virtual void Clip( const wxRegion &region );
#ifdef __WXMSW__
    cairo_surface_t* m_mswSurface;
    WindowHDC m_mswWindowHDC;
#endif

    // clips drawings to the rect
    virtual void Clip( wxDouble x, wxDouble y, wxDouble w, wxDouble h );

    // resets the clipping to original extent
    virtual void ResetClip();

    virtual void * GetNativeContext();

    virtual bool SetAntialiasMode(wxAntialiasMode antialias);

    virtual bool SetInterpolationQuality(wxInterpolationQuality interpolation);

    virtual bool SetCompositionMode(wxCompositionMode op);

    virtual void BeginLayer(wxDouble opacity);

    virtual void EndLayer();

    virtual void StrokePath( const wxGraphicsPath& p );
    virtual void FillPath( const wxGraphicsPath& p , wxPolygonFillMode fillStyle = wxWINDING_RULE );

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

protected:
    virtual void DoDrawText( const wxString &str, wxDouble x, wxDouble y );

    void Init(cairo_t *context);

private:
    cairo_t* m_context;

    wxVector<float> m_layerOpacities;

    wxDECLARE_NO_COPY_CLASS(wxCairoContext);
};

#if wxUSE_IMAGE
// ----------------------------------------------------------------------------
// wxCairoImageContext: context associated with a wxImage.
// ----------------------------------------------------------------------------

class wxCairoImageContext : public wxCairoContext
{
public:
    wxCairoImageContext(wxGraphicsRenderer* renderer, wxImage& image) :
        wxCairoContext(renderer),
        m_image(image),
        m_data(renderer, image)
    {
        Init(cairo_create(m_data.GetCairoSurface()));
    }

    virtual ~wxCairoImageContext()
    {
        Flush();
    }

    virtual void Flush()
    {
        m_image = m_data.ConvertToImage();
    }

private:
    wxImage& m_image;
    wxCairoBitmapData m_data;

    wxDECLARE_NO_COPY_CLASS(wxCairoImageContext);
};
#endif // wxUSE_IMAGE

// ----------------------------------------------------------------------------
// wxCairoPenBrushBaseData implementation
//-----------------------------------------------------------------------------

wxCairoPenBrushBaseData::wxCairoPenBrushBaseData(wxGraphicsRenderer* renderer,
                                                 const wxColour& col,
                                                 bool isTransparent)
    : wxGraphicsObjectRefData(renderer)
{
    m_hatchStyle = wxHATCHSTYLE_INVALID;
    m_pattern = NULL;
    m_bmpdata = NULL;

    if ( isTransparent )
    {
        m_red =
        m_green =
        m_blue =
        m_alpha = 0;
    }
    else // non-transparent
    {
        m_red = col.Red()/255.0;
        m_green = col.Green()/255.0;
        m_blue = col.Blue()/255.0;
        m_alpha = col.Alpha()/255.0;
    }
}

wxCairoPenBrushBaseData::~wxCairoPenBrushBaseData()
{
    if (m_bmpdata)
    {
        // Deleting the bitmap data also deletes the pattern referenced by
        // m_pattern, so set it to NULL to avoid deleting it twice.
        delete m_bmpdata;
        m_pattern = NULL;
    }
    if (m_pattern)
        cairo_pattern_destroy(m_pattern);
}

void wxCairoPenBrushBaseData::InitHatchPattern(cairo_t* ctext)
{
    cairo_surface_t* const
        surface = cairo_surface_create_similar(
                    cairo_get_target(ctext), CAIRO_CONTENT_COLOR_ALPHA, 10, 10
                );

    cairo_t* const cr = cairo_create(surface);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
    cairo_set_line_width(cr, 1);
    cairo_set_line_join(cr,CAIRO_LINE_JOIN_MITER);

    switch ( m_hatchStyle )
    {
        case wxHATCHSTYLE_CROSS:
            cairo_move_to(cr, 5, 0);
            cairo_line_to(cr, 5, 10);
            cairo_move_to(cr, 0, 5);
            cairo_line_to(cr, 10, 5);
            break;

        case wxHATCHSTYLE_BDIAGONAL:
            cairo_move_to(cr, 0, 10);
            cairo_line_to(cr, 10, 0);
            break;

        case wxHATCHSTYLE_FDIAGONAL:
            cairo_move_to(cr, 0, 0);
            cairo_line_to(cr, 10, 10);
            break;

        case wxHATCHSTYLE_CROSSDIAG:
            cairo_move_to(cr, 0, 0);
            cairo_line_to(cr, 10, 10);
            cairo_move_to(cr, 10, 0);
            cairo_line_to(cr, 0, 10);
            break;

        case wxHATCHSTYLE_HORIZONTAL:
            cairo_move_to(cr, 0, 5);
            cairo_line_to(cr, 10, 5);
            break;

        case wxHATCHSTYLE_VERTICAL:
            cairo_move_to(cr, 5, 0);
            cairo_line_to(cr, 5, 10);
            break;

        default:
            wxFAIL_MSG(wxS("Invalid hatch pattern style."));
    }

    cairo_set_source_rgba(cr, m_red, m_green, m_blue, m_alpha);
    cairo_stroke(cr);

    cairo_destroy(cr);

    m_pattern = cairo_pattern_create_for_surface(surface);
    cairo_surface_destroy(surface);
    cairo_pattern_set_extend(m_pattern, CAIRO_EXTEND_REPEAT);
}

void wxCairoPenBrushBaseData::InitStipple(wxBitmap* bmp)
{
    wxCHECK_RET( bmp && bmp->IsOk(), wxS("Invalid stippled bitmap") );

    m_bmpdata = new wxCairoBitmapData(GetRenderer(), *bmp);
    m_pattern = m_bmpdata->GetCairoPattern();
    cairo_pattern_set_extend(m_pattern, CAIRO_EXTEND_REPEAT);
}

void wxCairoPenBrushBaseData::InitHatch(wxHatchStyle hatchStyle)
{
    // We can't create m_pattern right now as we don't have the Cairo context
    // needed for it, so just remember that we need to do it.
    m_hatchStyle = hatchStyle;
}

void wxCairoPenBrushBaseData::Apply( wxGraphicsContext* context )
{
    cairo_t* const ctext = (cairo_t*) context->GetNativeContext();

    if ( m_hatchStyle != wxHATCHSTYLE_INVALID && !m_pattern )
        InitHatchPattern(ctext);

    if ( m_pattern )
        cairo_set_source(ctext, m_pattern);
    else
        cairo_set_source_rgba(ctext, m_red, m_green, m_blue, m_alpha);
}

//-----------------------------------------------------------------------------
// wxCairoPenData implementation
//-----------------------------------------------------------------------------

wxCairoPenData::~wxCairoPenData()
{
    delete[] m_userLengths;
}

void wxCairoPenData::Init()
{
    m_lengths = NULL;
    m_userLengths = NULL;
    m_width = 0;
    m_count = 0;
}

wxCairoPenData::wxCairoPenData( wxGraphicsRenderer* renderer, const wxPen &pen )
    : wxCairoPenBrushBaseData(renderer, pen.GetColour(), pen.IsTransparent())
{
    Init();
    m_width = pen.GetWidth();
    if (m_width <= 0.0)
        m_width = 0.1;

    switch ( pen.GetCap() )
    {
    case wxCAP_ROUND :
        m_cap = CAIRO_LINE_CAP_ROUND;
        break;

    case wxCAP_PROJECTING :
        m_cap = CAIRO_LINE_CAP_SQUARE;
        break;

    case wxCAP_BUTT :
        m_cap = CAIRO_LINE_CAP_BUTT;
        break;

    default :
        m_cap = CAIRO_LINE_CAP_BUTT;
        break;
    }

    switch ( pen.GetJoin() )
    {
    case wxJOIN_BEVEL :
        m_join = CAIRO_LINE_JOIN_BEVEL;
        break;

    case wxJOIN_MITER :
        m_join = CAIRO_LINE_JOIN_MITER;
        break;

    case wxJOIN_ROUND :
        m_join = CAIRO_LINE_JOIN_ROUND;
        break;

    default :
        m_join = CAIRO_LINE_JOIN_MITER;
        break;
    }

    const double dashUnit = m_width < 1.0 ? 1.0 : m_width;
    const double dotted[] =
    {
        dashUnit , dashUnit + 2.0
    };
    static const double short_dashed[] =
    {
        9.0 , 6.0
    };
    static const double dashed[] =
    {
        19.0 , 9.0
    };
    static const double dotted_dashed[] =
    {
        9.0 , 6.0 , 3.0 , 3.0
    };

    switch ( pen.GetStyle() )
    {
    case wxPENSTYLE_SOLID :
        break;

    case wxPENSTYLE_DOT :
        m_count = WXSIZEOF(dotted);
        m_userLengths = new double[ m_count ] ;
        memcpy( m_userLengths, dotted, sizeof(dotted) );
        m_lengths = m_userLengths;
        break;

    case wxPENSTYLE_LONG_DASH :
        m_lengths = dashed ;
        m_count = WXSIZEOF(dashed);
        break;

    case wxPENSTYLE_SHORT_DASH :
        m_lengths = short_dashed ;
        m_count = WXSIZEOF(short_dashed);
        break;

    case wxPENSTYLE_DOT_DASH :
        m_lengths = dotted_dashed ;
        m_count = WXSIZEOF(dotted_dashed);
        break;

    case wxPENSTYLE_USER_DASH :
        {
            wxDash *wxdashes ;
            m_count = pen.GetDashes( &wxdashes ) ;
            if ((wxdashes != NULL) && (m_count > 0))
            {
                m_userLengths = new double[m_count] ;
                for ( int i = 0 ; i < m_count ; ++i )
                {
                    m_userLengths[i] = wxdashes[i] * dashUnit ;

                    if ( i % 2 == 1 && m_userLengths[i] < dashUnit + 2.0 )
                        m_userLengths[i] = dashUnit + 2.0 ;
                    else if ( i % 2 == 0 && m_userLengths[i] < dashUnit )
                        m_userLengths[i] = dashUnit ;
                }
            }
            m_lengths = m_userLengths ;
        }
        break;

    case wxPENSTYLE_STIPPLE :
    case wxPENSTYLE_STIPPLE_MASK :
    case wxPENSTYLE_STIPPLE_MASK_OPAQUE :
        InitStipple(pen.GetStipple());
        break;

    default :
        if ( pen.GetStyle() >= wxPENSTYLE_FIRST_HATCH
            && pen.GetStyle() <= wxPENSTYLE_LAST_HATCH )
        {
            InitHatch(static_cast<wxHatchStyle>(pen.GetStyle()));
        }
        break;
    }
}

void wxCairoPenData::Apply( wxGraphicsContext* context )
{
    wxCairoPenBrushBaseData::Apply(context);

    cairo_t * ctext = (cairo_t*) context->GetNativeContext();
    cairo_set_line_width(ctext,m_width);
    cairo_set_line_cap(ctext,m_cap);
    cairo_set_line_join(ctext,m_join);
    cairo_set_dash(ctext,(double*)m_lengths,m_count,0.0);
}

//-----------------------------------------------------------------------------
// wxCairoBrushData implementation
//-----------------------------------------------------------------------------

wxCairoBrushData::wxCairoBrushData( wxGraphicsRenderer* renderer )
    : wxCairoPenBrushBaseData(renderer, wxColour(), true /* transparent */)
{
    Init();
}

wxCairoBrushData::wxCairoBrushData( wxGraphicsRenderer* renderer,
                                    const wxBrush &brush )
    : wxCairoPenBrushBaseData(renderer, brush.GetColour(), brush.IsTransparent())
{
    Init();

    switch ( brush.GetStyle() )
    {
        case wxBRUSHSTYLE_STIPPLE:
        case wxBRUSHSTYLE_STIPPLE_MASK:
        case wxBRUSHSTYLE_STIPPLE_MASK_OPAQUE:
            InitStipple(brush.GetStipple());
            break;

        default:
            if ( brush.IsHatch() )
                InitHatch(static_cast<wxHatchStyle>(brush.GetStyle()));
            break;
    }
}

void wxCairoBrushData::AddGradientStops(const wxGraphicsGradientStops& stops)
{
    // loop over all the stops, they include the beginning and ending ones
    const unsigned numStops = stops.GetCount();
    for ( unsigned n = 0; n < numStops; n++ )
    {
        const wxGraphicsGradientStop stop = stops.Item(n);

        const wxColour col = stop.GetColour();

        cairo_pattern_add_color_stop_rgba
        (
            m_pattern,
            stop.GetPosition(),
            col.Red()/255.0,
            col.Green()/255.0,
            col.Blue()/255.0,
            col.Alpha()/255.0
        );
    }

    wxASSERT_MSG(cairo_pattern_status(m_pattern) == CAIRO_STATUS_SUCCESS,
                 wxT("Couldn't create cairo pattern"));
}

void
wxCairoBrushData::CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                                            wxDouble x2, wxDouble y2,
                                            const wxGraphicsGradientStops& stops)
{
    m_pattern = cairo_pattern_create_linear(x1,y1,x2,y2);

    AddGradientStops(stops);
}

void
wxCairoBrushData::CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                                            wxDouble xc, wxDouble yc,
                                            wxDouble radius,
                                            const wxGraphicsGradientStops& stops)
{
    m_pattern = cairo_pattern_create_radial(xo,yo,0.0,xc,yc,radius);

    AddGradientStops(stops);
}

void wxCairoBrushData::Init()
{
    m_pattern = NULL;
    m_bmpdata = NULL;
}

//-----------------------------------------------------------------------------
// wxCairoFontData implementation
//-----------------------------------------------------------------------------

void wxCairoFontData::InitColour(const wxColour& col)
{
    m_red = col.Red()/255.0;
    m_green = col.Green()/255.0;
    m_blue = col.Blue()/255.0;
    m_alpha = col.Alpha()/255.0;
}

void
wxCairoFontData::InitFontComponents(const wxString& facename,
                                    cairo_font_slant_t slant,
                                    cairo_font_weight_t weight)
{
    m_fontName = facename.mb_str(wxConvUTF8);
    m_slant = slant;
    m_weight = weight;
}

wxCairoFontData::wxCairoFontData( wxGraphicsRenderer* renderer, const wxFont &font,
                         const wxColour& col )
    : wxGraphicsObjectRefData(renderer)
#ifdef __WXGTK__
    , m_wxfont(font)
#endif
{
    InitColour(col);

    m_size = font.GetPointSize();

#ifdef __WXMAC__
    m_font = cairo_quartz_font_face_create_for_cgfont( font.OSXGetCGFont() );
#elif defined(__WXGTK__)
#else
    InitFontComponents
    (
        font.GetFaceName(),
        font.GetStyle() == wxFONTSTYLE_ITALIC ? CAIRO_FONT_SLANT_ITALIC
                                              : CAIRO_FONT_SLANT_NORMAL,
        font.GetWeight() == wxFONTWEIGHT_BOLD ? CAIRO_FONT_WEIGHT_BOLD
                                              : CAIRO_FONT_WEIGHT_NORMAL
    );
#endif
}

wxCairoFontData::wxCairoFontData(wxGraphicsRenderer* renderer,
                                 double sizeInPixels,
                                 const wxString& facename,
                                 int flags,
                                 const wxColour& col) :
    wxGraphicsObjectRefData(renderer)
{
    InitColour(col);

    // Resolution for Cairo image surfaces is 72 DPI meaning that the sizes in
    // points and pixels are identical, so we can just pass the size in pixels
    // directly to cairo_set_font_size().
    m_size = sizeInPixels;

#if defined(__WXMAC__)
    m_font = NULL;
#endif

    // There is no need to set m_underlined under wxGTK in this case, it can
    // only be used if m_font != NULL.

    InitFontComponents
    (
        facename,
        flags & wxFONTFLAG_ITALIC ? CAIRO_FONT_SLANT_ITALIC
                                  : CAIRO_FONT_SLANT_NORMAL,
        flags & wxFONTFLAG_BOLD ? CAIRO_FONT_WEIGHT_BOLD
                                : CAIRO_FONT_WEIGHT_NORMAL
    );
}

wxCairoFontData::~wxCairoFontData()
{
#ifdef __WXMAC__
    if ( m_font )
        cairo_font_face_destroy( m_font );
#endif
}

bool wxCairoFontData::Apply( wxGraphicsContext* context )
{
    cairo_t * ctext = (cairo_t*) context->GetNativeContext();
    cairo_set_source_rgba(ctext,m_red,m_green, m_blue,m_alpha);
#ifdef __WXGTK__
    if (m_wxfont.IsOk())
    {
        // Nothing to do, the caller uses Pango layout functions to do
        // everything.
        return true;
    }
#elif defined(__WXMAC__)
    if ( m_font )
    {
        cairo_set_font_face(ctext, m_font);
        cairo_set_font_size(ctext, m_size );
        return true;
    }
#endif

    // If we get here, we must be on a platform without native font support or
    // we're using toy Cairo API even under wxGTK/wxMac.
    cairo_select_font_face(ctext, m_fontName, m_slant, m_weight );
    cairo_set_font_size(ctext, m_size );

    // Indicate that we don't use native fonts for the platforms which care
    // about this (currently only wxGTK).
    return false;
}

//-----------------------------------------------------------------------------
// wxCairoPathData implementation
//-----------------------------------------------------------------------------

wxCairoPathData::wxCairoPathData( wxGraphicsRenderer* renderer, cairo_t* pathcontext)
    : wxGraphicsPathData(renderer)
{
    if (pathcontext)
    {
        m_pathContext = pathcontext;
    }
    else
    {
        cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);
        m_pathContext = cairo_create(surface);
        cairo_surface_destroy (surface);
    }
}

wxCairoPathData::~wxCairoPathData()
{
    cairo_destroy(m_pathContext);
}

wxGraphicsObjectRefData *wxCairoPathData::Clone() const
{
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);
    cairo_t* pathcontext = cairo_create(surface);
    cairo_surface_destroy (surface);

    cairo_path_t* path = cairo_copy_path(m_pathContext);
    cairo_append_path(pathcontext, path);
    cairo_path_destroy(path);
    return new wxCairoPathData( GetRenderer() ,pathcontext);
}


void* wxCairoPathData::GetNativePath() const
{
    return cairo_copy_path(m_pathContext) ;
}

void wxCairoPathData::UnGetNativePath(void *p) const
{
    cairo_path_destroy((cairo_path_t*)p);
}

//
// The Primitives
//

void wxCairoPathData::MoveToPoint( wxDouble x , wxDouble y )
{
    cairo_move_to(m_pathContext,x,y);
}

void wxCairoPathData::AddLineToPoint( wxDouble x , wxDouble y )
{
    cairo_line_to(m_pathContext,x,y);
}

void wxCairoPathData::AddPath( const wxGraphicsPathData* path )
{
    cairo_path_t* p = (cairo_path_t*)path->GetNativePath();
    cairo_append_path(m_pathContext, p);
    UnGetNativePath(p);
}

void wxCairoPathData::CloseSubpath()
{
    cairo_close_path(m_pathContext);
}

void wxCairoPathData::AddCurveToPoint( wxDouble cx1, wxDouble cy1, wxDouble cx2, wxDouble cy2, wxDouble x, wxDouble y )
{
    cairo_curve_to(m_pathContext,cx1,cy1,cx2,cy2,x,y);
}

// gets the last point of the current path, (0,0) if not yet set
void wxCairoPathData::GetCurrentPoint( wxDouble* x, wxDouble* y) const
{
    double dx,dy;
    cairo_get_current_point(m_pathContext,&dx,&dy);
    if (x)
        *x = dx;
    if (y)
        *y = dy;
}

void wxCairoPathData::AddArc( wxDouble x, wxDouble y, wxDouble r, double startAngle, double endAngle, bool clockwise )
{
    // as clockwise means positive in our system (y pointing downwards)
    // TODO make this interpretation dependent of the
    // real device trans
    if ( clockwise||(endAngle-startAngle)>=2*M_PI)
        cairo_arc(m_pathContext,x,y,r,startAngle,endAngle);
    else
        cairo_arc_negative(m_pathContext,x,y,r,startAngle,endAngle);
}

// transforms each point of this path by the matrix
void wxCairoPathData::Transform( const wxGraphicsMatrixData* matrix )
{
    // as we don't have a true path object, we have to apply the inverse
    // matrix to the context
    cairo_matrix_t m = *((cairo_matrix_t*) matrix->GetNativeMatrix());
    cairo_matrix_invert( &m );
    cairo_transform(m_pathContext,&m);
}

// gets the bounding box enclosing all points (possibly including control points)
void wxCairoPathData::GetBox(wxDouble *x, wxDouble *y, wxDouble *w, wxDouble *h) const
{
    double x1,y1,x2,y2;

    cairo_stroke_extents( m_pathContext, &x1, &y1, &x2, &y2 );
    if ( x2 < x1 )
    {
        *x = x2;
        *w = x1-x2;
    }
    else
    {
        *x = x1;
        *w = x2-x1;
    }

    if( y2 < y1 )
    {
        *y = y2;
        *h = y1-y2;
    }
    else
    {
        *y = y1;
        *h = y2-y1;
    }
}

bool wxCairoPathData::Contains( wxDouble x, wxDouble y, wxPolygonFillMode fillStyle ) const
{
    cairo_set_fill_rule(m_pathContext,fillStyle==wxODDEVEN_RULE ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    return cairo_in_fill( m_pathContext, x, y) != 0;
}

//-----------------------------------------------------------------------------
// wxCairoMatrixData implementation
//-----------------------------------------------------------------------------

wxCairoMatrixData::wxCairoMatrixData(wxGraphicsRenderer* renderer, const cairo_matrix_t* matrix )
    : wxGraphicsMatrixData(renderer)
{
    if ( matrix )
        m_matrix = *matrix;
}

wxCairoMatrixData::~wxCairoMatrixData()
{
    // nothing to do
}

wxGraphicsObjectRefData *wxCairoMatrixData::Clone() const
{
    return new wxCairoMatrixData(GetRenderer(),&m_matrix);
}

// concatenates the matrix
void wxCairoMatrixData::Concat( const wxGraphicsMatrixData *t )
{
    cairo_matrix_multiply( &m_matrix, &m_matrix, (cairo_matrix_t*) t->GetNativeMatrix());
}

// sets the matrix to the respective values
void wxCairoMatrixData::Set(wxDouble a, wxDouble b, wxDouble c, wxDouble d,
                        wxDouble tx, wxDouble ty)
{
    cairo_matrix_init( &m_matrix, a, b, c, d, tx, ty);
}

// gets the component valuess of the matrix
void wxCairoMatrixData::Get(wxDouble* a, wxDouble* b,  wxDouble* c,
                            wxDouble* d, wxDouble* tx, wxDouble* ty) const
{
    if (a)  *a = m_matrix.xx;
    if (b)  *b = m_matrix.yx;
    if (c)  *c = m_matrix.xy;
    if (d)  *d = m_matrix.yy;
    if (tx) *tx= m_matrix.x0;
    if (ty) *ty= m_matrix.y0;
}

// makes this the inverse matrix
void wxCairoMatrixData::Invert()
{
    cairo_matrix_invert( &m_matrix );
}

// returns true if the elements of the transformation matrix are equal ?
bool wxCairoMatrixData::IsEqual( const wxGraphicsMatrixData* t) const
{
    const cairo_matrix_t* tm = (cairo_matrix_t*) t->GetNativeMatrix();
    return (
        m_matrix.xx == tm->xx &&
        m_matrix.yx == tm->yx &&
        m_matrix.xy == tm->xy &&
        m_matrix.yy == tm->yy &&
        m_matrix.x0 == tm->x0 &&
        m_matrix.y0 == tm->y0 ) ;
}

// return true if this is the identity matrix
bool wxCairoMatrixData::IsIdentity() const
{
    return ( m_matrix.xx == 1 && m_matrix.yy == 1 &&
        m_matrix.yx == 0 && m_matrix.xy == 0 && m_matrix.x0 == 0 && m_matrix.y0 == 0);
}

//
// transformation
//

// add the translation to this matrix
void wxCairoMatrixData::Translate( wxDouble dx , wxDouble dy )
{
    cairo_matrix_translate( &m_matrix, dx, dy) ;
}

// add the scale to this matrix
void wxCairoMatrixData::Scale( wxDouble xScale , wxDouble yScale )
{
    cairo_matrix_scale( &m_matrix, xScale, yScale) ;
}

// add the rotation to this matrix (radians)
void wxCairoMatrixData::Rotate( wxDouble angle )
{
    cairo_matrix_rotate( &m_matrix, angle) ;
}

//
// apply the transforms
//

// applies that matrix to the point
void wxCairoMatrixData::TransformPoint( wxDouble *x, wxDouble *y ) const
{
    double lx = *x, ly = *y ;
    cairo_matrix_transform_point( &m_matrix, &lx, &ly);
    *x = lx;
    *y = ly;
}

// applies the matrix except for translations
void wxCairoMatrixData::TransformDistance( wxDouble *dx, wxDouble *dy ) const
{
    double lx = *dx, ly = *dy ;
    cairo_matrix_transform_distance( &m_matrix, &lx, &ly);
    *dx = lx;
    *dy = ly;
}

// returns the native representation
void * wxCairoMatrixData::GetNativeMatrix() const
{
    return (void*) &m_matrix;
}

// ----------------------------------------------------------------------------
// wxCairoBitmap implementation
// ----------------------------------------------------------------------------

int wxCairoBitmapData::InitBuffer(int width, int height, cairo_format_t format)
{
    wxUnusedVar(format); // Only really unused with Cairo < 1.6.

    // Determine the stride: use cairo_format_stride_for_width() if available
    // but fall back to 4*width for the earlier versions as this is what that
    // function always returns, even in latest Cairo, anyhow.
    int stride;
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 6, 0)
    if ( cairo_version() >= CAIRO_VERSION_ENCODE(1, 6, 0) )
    {
        stride = cairo_format_stride_for_width(format, width);

        // All our code would totally break if stride were not a multiple of 4
        // so ensure this is the case.
        if ( stride % 4 )
        {
            wxFAIL_MSG("Unexpected Cairo image surface stride.");

            stride += 4 - stride % 4;
        }
    }
    else
#endif
        stride = 4*width;

    m_width = width;
    m_height = height;
    m_buffer = new unsigned char[height*stride];

    return stride;
}

void wxCairoBitmapData::InitSurface(cairo_format_t format, int stride)
{
    m_surface = cairo_image_surface_create_for_data(
                            m_buffer, format, m_width, m_height, stride);
    m_pattern = cairo_pattern_create_for_surface(m_surface);
}

wxCairoBitmapData::wxCairoBitmapData( wxGraphicsRenderer* renderer, cairo_surface_t* bitmap ) :
    wxGraphicsBitmapData( renderer )
{
    m_surface = bitmap;
    m_pattern = cairo_pattern_create_for_surface(m_surface);

    m_width = cairo_image_surface_get_width(m_surface);
    m_height = cairo_image_surface_get_height(m_surface);
    m_buffer = NULL;
}

wxCairoBitmapData::wxCairoBitmapData( wxGraphicsRenderer* renderer, const wxBitmap& bmp ) : wxGraphicsBitmapData( renderer )
{
    wxCHECK_RET( bmp.IsOk(), wxT("Invalid bitmap in wxCairoContext::DrawBitmap"));

#ifdef wxHAS_RAW_BITMAP
    // Create a surface object and copy the bitmap pixel data to it.  if the
    // image has alpha (or a mask represented as alpha) then we'll use a
    // different format and iterator than if it doesn't...
    cairo_format_t bufferFormat = bmp.GetDepth() == 32
#if defined(__WXGTK__) && !defined(__WXGTK3__)
                                            || bmp.GetMask()
#endif
                                        ? CAIRO_FORMAT_ARGB32
                                        : CAIRO_FORMAT_RGB24;

    int stride = InitBuffer(bmp.GetWidth(), bmp.GetHeight(), bufferFormat);

    wxBitmap bmpSource = bmp;  // we need a non-const instance
    wxUint32* data = (wxUint32*)m_buffer;

    if ( bufferFormat == CAIRO_FORMAT_ARGB32 )
    {
        // use the bitmap's alpha
        wxAlphaPixelData
            pixData(bmpSource, wxPoint(0, 0), wxSize(m_width, m_height));
        wxCHECK_RET( pixData, wxT("Failed to gain raw access to bitmap data."));

        wxAlphaPixelData::Iterator p(pixData);
        for (int y=0; y<m_height; y++)
        {
            wxAlphaPixelData::Iterator rowStart = p;
            wxUint32* const rowStartDst = data;
            for (int x=0; x<m_width; x++)
            {
                // Each pixel in CAIRO_FORMAT_ARGB32 is a 32-bit quantity,
                // with alpha in the upper 8 bits, then red, then green, then
                // blue. The 32-bit quantities are stored native-endian.
                // Pre-multiplied alpha is used.
                unsigned char alpha = p.Alpha();
                if (alpha == 0)
                    *data = 0;
                else
                    *data = ( alpha                      << 24
                              | (p.Red() * alpha/255)    << 16
                              | (p.Green() * alpha/255)  <<  8
                              | (p.Blue() * alpha/255) );
                ++data;
                ++p;
            }

            data = rowStartDst + stride / 4;
            p = rowStart;
            p.OffsetY(pixData, 1);
        }
    }
    else  // no alpha
    {
        wxNativePixelData
            pixData(bmpSource, wxPoint(0, 0), wxSize(m_width, m_height));
        wxCHECK_RET( pixData, wxT("Failed to gain raw access to bitmap data."));

        wxNativePixelData::Iterator p(pixData);
        for (int y=0; y<m_height; y++)
        {
            wxNativePixelData::Iterator rowStart = p;
            wxUint32* const rowStartDst = data;
            for (int x=0; x<m_width; x++)
            {
                // Each pixel in CAIRO_FORMAT_RGB24 is a 32-bit quantity, with
                // the upper 8 bits unused. Red, Green, and Blue are stored in
                // the remaining 24 bits in that order.  The 32-bit quantities
                // are stored native-endian.
                *data = ( p.Red() << 16 | p.Green() << 8 | p.Blue() );
                ++data;
                ++p;
            }

            data = rowStartDst + stride / 4;
            p = rowStart;
            p.OffsetY(pixData, 1);
        }
    }
#if defined(__WXMSW__) || defined(__WXGTK3__)
    // if there is a mask, set the alpha bytes in the target buffer to 
    // fully transparent or fully opaque
    if (bmpSource.GetMask())
    {
        wxBitmap bmpMask = bmpSource.GetMask()->GetBitmap();
        bufferFormat = CAIRO_FORMAT_ARGB32;
        data = (wxUint32*)m_buffer;
        wxNativePixelData
            pixData(bmpMask, wxPoint(0, 0), wxSize(m_width, m_height));
        wxCHECK_RET( pixData, wxT("Failed to gain raw access to mask data."));

        wxNativePixelData::Iterator p(pixData);
        for (int y=0; y<m_height; y++)
        {
            wxNativePixelData::Iterator rowStart = p;
            wxUint32* const rowStartDst = data;
            for (int x=0; x<m_width; x++)
            {
                if (p.Red()+p.Green()+p.Blue() == 0)
                    *data = 0;
                else
                    *data = (wxALPHA_OPAQUE << 24) | (*data & 0x00FFFFFF);
                ++data;
                ++p;
            }

            data = rowStartDst + stride / 4;
            p = rowStart;
            p.OffsetY(pixData, 1);
        }
    }
#endif

    InitSurface(bufferFormat, stride);
#endif // wxHAS_RAW_BITMAP
}

#if wxUSE_IMAGE

// Helper functions for dealing with alpha pre-multiplication.
namespace
{

inline unsigned char Premultiply(unsigned char alpha, unsigned char data)
{
    return alpha ? (data * alpha)/0xff : data;
}

inline unsigned char Unpremultiply(unsigned char alpha, unsigned char data)
{
    return alpha ? (data * 0xff)/alpha : data;
}

} // anonymous namespace

wxCairoBitmapData::wxCairoBitmapData(wxGraphicsRenderer* renderer,
                                     const wxImage& image)
    : wxGraphicsBitmapData(renderer)
{
    const cairo_format_t bufferFormat = image.HasAlpha()
                                            ? CAIRO_FORMAT_ARGB32
                                            : CAIRO_FORMAT_RGB24;

    int stride = InitBuffer(image.GetWidth(), image.GetHeight(), bufferFormat);

    // Copy wxImage data into the buffer. Notice that we work with wxUint32
    // values and not bytes becase Cairo always works with buffers in native
    // endianness.
    wxUint32* dst = reinterpret_cast<wxUint32*>(m_buffer);
    const unsigned char* src = image.GetData();

    if ( bufferFormat == CAIRO_FORMAT_ARGB32 )
    {
        const unsigned char* alpha = image.GetAlpha();

        for ( int y = 0; y < m_height; y++ )
        {
            wxUint32* const rowStartDst = dst;

            for ( int x = 0; x < m_width; x++ )
            {
                const unsigned char a = *alpha++;

                *dst++ = a                      << 24 |
                         Premultiply(a, src[0]) << 16 |
                         Premultiply(a, src[1]) <<  8 |
                         Premultiply(a, src[2]);
                src += 3;
            }

            dst = rowStartDst + stride / 4;
        }
    }
    else // RGB
    {
        for ( int y = 0; y < m_height; y++ )
        {
            wxUint32* const rowStartDst = dst;

            for ( int x = 0; x < m_width; x++ )
            {
                *dst++ = src[0] << 16 |
                         src[1] <<  8 |
                         src[2];
                src += 3;
            }

            dst = rowStartDst + stride / 4;
        }
    }

    InitSurface(bufferFormat, stride);
}

wxImage wxCairoBitmapData::ConvertToImage() const
{
    wxImage image(m_width, m_height, false /* don't clear */);

    // Get the surface type and format.
    wxCHECK_MSG( cairo_surface_get_type(m_surface) == CAIRO_SURFACE_TYPE_IMAGE,
                 wxNullImage,
                 wxS("Can't convert non-image surface to image.") );

    switch ( cairo_image_surface_get_format(m_surface) )
    {
        case CAIRO_FORMAT_ARGB32:
            image.SetAlpha();
            break;

        case CAIRO_FORMAT_RGB24:
            // Nothing to do, we don't use alpha by default.
            break;

        case CAIRO_FORMAT_A8:
        case CAIRO_FORMAT_A1:
            wxFAIL_MSG(wxS("Unsupported Cairo image surface type."));
            return wxNullImage;

        default:
            wxFAIL_MSG(wxS("Unknown Cairo image surface type."));
            return wxNullImage;
    }

    // Prepare for copying data.
    cairo_surface_flush(m_surface);
    const wxUint32* src = (wxUint32*)cairo_image_surface_get_data(m_surface);
    wxCHECK_MSG( src, wxNullImage, wxS("Failed to get Cairo surface data.") );

    int stride = cairo_image_surface_get_stride(m_surface);
    wxCHECK_MSG( stride > 0, wxNullImage,
                 wxS("Failed to get Cairo surface stride.") );

    // As we work with wxUint32 pointers and not char ones, we need to adjust
    // the stride accordingly. This should be lossless as the stride must be a
    // multiple of pixel size.
    wxASSERT_MSG( !(stride % sizeof(wxUint32)), wxS("Unexpected stride.") );
    stride /= sizeof(wxUint32);

    unsigned char* dst = image.GetData();
    unsigned char *alpha = image.GetAlpha();
    if ( alpha )
    {
        // We need to also copy alpha and undo the pre-multiplication as Cairo
        // stores pre-multiplied values in this format while wxImage does not.
        for ( int y = 0; y < m_height; y++ )
        {
            const wxUint32* const rowStart = src;
            for ( int x = 0; x < m_width; x++ )
            {
                const wxUint32 argb = *src++;

                const unsigned char a = argb >> 24;
                *alpha++ = a;

                // Copy the RGB data undoing the pre-multiplication.
                *dst++ = Unpremultiply(a, argb >> 16);
                *dst++ = Unpremultiply(a, argb >>  8);
                *dst++ = Unpremultiply(a, argb);
            }

            src = rowStart + stride;
        }
    }
    else // RGB
    {
        // Things are pretty simple in this case, just copy RGB bytes.
        for ( int y = 0; y < m_height; y++ )
        {
            const wxUint32* const rowStart = src;
            for ( int x = 0; x < m_width; x++ )
            {
                const wxUint32 argb = *src++;

                *dst++ = (argb & 0x00ff0000) >> 16;
                *dst++ = (argb & 0x0000ff00) >>  8;
                *dst++ = (argb & 0x000000ff);
            }

            src = rowStart + stride;
        }
    }

    return image;
}

#endif // wxUSE_IMAGE

wxCairoBitmapData::~wxCairoBitmapData()
{
    if (m_pattern)
        cairo_pattern_destroy(m_pattern);

    if (m_surface)
        cairo_surface_destroy(m_surface);

    delete [] m_buffer;
}

//-----------------------------------------------------------------------------
// wxCairoContext implementation
//-----------------------------------------------------------------------------

class wxCairoOffsetHelper
{
public :
    wxCairoOffsetHelper( cairo_t* ctx , bool offset )
    {
        m_ctx = ctx;
        m_offset = offset;
        if ( m_offset )
             cairo_translate( m_ctx, 0.5, 0.5 );
    }
    ~wxCairoOffsetHelper( )
    {
        if ( m_offset )
            cairo_translate( m_ctx, -0.5, -0.5 );
    }
public :
    cairo_t* m_ctx;
    bool m_offset;
} ;

#if wxUSE_PRINTING_ARCHITECTURE
wxCairoContext::wxCairoContext( wxGraphicsRenderer* renderer, const wxPrinterDC& dc )
: wxGraphicsContext(renderer)
{
#ifdef __WXMSW__
    // wxMSW contexts always use MM_ANISOTROPIC, which messes up 
    // text rendering when printing using Cairo. Switch it to MM_TEXT
    // map mode to avoid this problem.
    HDC hdc = (HDC)dc.GetHDC();
    ::SetMapMode(hdc, MM_TEXT);
    m_mswSurface = cairo_win32_printing_surface_create(hdc);
    Init( cairo_create(m_mswSurface) );
#endif

#ifdef __WXGTK20__
    const wxDCImpl *impl = dc.GetImpl();
    cairo_t* cr = static_cast<cairo_t*>(impl->GetCairoContext());
    if (cr)
        Init(cairo_reference(cr));
#endif
    wxSize sz = dc.GetSize();
    m_width = sz.x;
    m_height = sz.y;

    wxPoint org = dc.GetDeviceOrigin();
    cairo_translate( m_context, org.x, org.y );

    double sx,sy;
    dc.GetUserScale( &sx, &sy );

// TODO: Determine if these fixes are needed on other platforms too.
// On MSW, without this the printer context will not respect wxDC SetMapMode calls.
// For example, using dc.SetMapMode(wxMM_POINTS) can let us share printer and screen
// drawing code
#ifdef __WXMSW__
    double lsx,lsy;
    dc.GetLogicalScale( &lsx, &lsy );
    sx *= lsx;
    sy *= lsy;
#endif
    cairo_scale( m_context, sx, sy );

    org = dc.GetLogicalOrigin();
    cairo_translate( m_context, -org.x, -org.y );
}
#endif

wxCairoContext::wxCairoContext( wxGraphicsRenderer* renderer, const wxWindowDC& dc )
: wxGraphicsContext(renderer)
{
    int width, height;
    dc.GetSize( &width, &height );
    m_width = width;
    m_height = height;

    m_enableOffset = true;

#ifdef __WXMSW__
    m_mswSurface = cairo_win32_surface_create((HDC)dc.GetHDC());
    Init( cairo_create(m_mswSurface) );
#endif

#ifdef __WXGTK3__
    cairo_t* cr = static_cast<cairo_t*>(dc.GetImpl()->GetCairoContext());
    if (cr)
        Init(cairo_reference(cr));
#elif defined __WXGTK20__
    wxGTKDCImpl *impldc = (wxGTKDCImpl*) dc.GetImpl();
    Init( gdk_cairo_create( impldc->GetGDKWindow() ) );

#if 0
    wxGraphicsMatrix matrix = CreateMatrix();

    wxPoint org = dc.GetDeviceOrigin();
    matrix.Translate( org.x, org.y );

    org = dc.GetLogicalOrigin();
    matrix.Translate( -org.x, -org.y );

    double sx,sy;
    dc.GetUserScale( &sx, &sy );
    matrix.Scale( sx, sy );

    ConcatTransform( matrix );
#endif
#endif

#ifdef __WXMAC__
    CGContextRef cgcontext = (CGContextRef)dc.GetWindow()->MacGetCGContextRef();
    cairo_surface_t* surface = cairo_quartz_surface_create_for_cg_context(cgcontext, width, height);
    Init( cairo_create( surface ) );
    cairo_surface_destroy( surface );
#endif
}

wxCairoContext::wxCairoContext( wxGraphicsRenderer* renderer, const wxMemoryDC& dc )
: wxGraphicsContext(renderer)
{
    int width, height;
    dc.GetSize( &width, &height );
    m_width = width;
    m_height = height;

    m_enableOffset = true;

#ifdef __WXMSW__

    HDC hdc = (HDC)dc.GetHDC();

    HBITMAP bitmap = (HBITMAP)GetCurrentObject(hdc, OBJ_BITMAP);

    BITMAP info;
    bool hasBitmap = false;

    // cairo_win32_surface_create creates a 24-bit bitmap,
    // so if we have alpha, we need to create a 32-bit surface instead.
    if (!GetObject(bitmap, sizeof(info), &info) || info.bmBitsPixel < 32)
        m_mswSurface = cairo_win32_surface_create(hdc);
    else {
        hasBitmap = true;
        m_mswSurface = cairo_image_surface_create_for_data((unsigned char*)info.bmBits,
                                               CAIRO_FORMAT_ARGB32,
                                               info.bmWidth,
                                               info.bmHeight,
                                               info.bmWidthBytes);
    }

    Init( cairo_create(m_mswSurface) );
    // If we've created a image surface, we need to flip the Y axis so that 
    // all drawing will appear right side up.
    if (hasBitmap) {
        cairo_matrix_t matrix;
        cairo_matrix_init(&matrix, 1.0, 0.0, 0.0, -1.0, 0.0, height);
        cairo_set_matrix(m_context, &matrix);
    }
#endif
    
#ifdef __WXGTK3__
    cairo_t* cr = static_cast<cairo_t*>(dc.GetImpl()->GetCairoContext());
    if (cr)
        Init(cairo_reference(cr));
#elif defined __WXGTK20__
    wxGTKDCImpl *impldc = (wxGTKDCImpl*) dc.GetImpl();
    Init( gdk_cairo_create( impldc->GetGDKWindow() ) );

#if 0
    wxGraphicsMatrix matrix = CreateMatrix();

    wxPoint org = dc.GetDeviceOrigin();
    matrix.Translate( org.x, org.y );

    org = dc.GetLogicalOrigin();
    matrix.Translate( -org.x, -org.y );

    double sx,sy;
    dc.GetUserScale( &sx, &sy );
    matrix.Scale( sx, sy );

    ConcatTransform( matrix );
#endif
#endif

#ifdef __WXMAC__
    CGContextRef cgcontext = (CGContextRef)dc.GetWindow()->MacGetCGContextRef();
    cairo_surface_t* surface = cairo_quartz_surface_create_for_cg_context(cgcontext, width, height);
    Init( cairo_create( surface ) );
    cairo_surface_destroy( surface );
#endif
}

#ifdef __WXGTK20__
wxCairoContext::wxCairoContext( wxGraphicsRenderer* renderer, GdkWindow *window )
: wxGraphicsContext(renderer)
{
    Init( gdk_cairo_create( window ) );

#ifdef __WXGTK3__
    m_width = gdk_window_get_width(window);
    m_height = gdk_window_get_height(window);
#else
    int width, height;
    gdk_drawable_get_size(window, &width, &height);
    m_width = width;
    m_height = height;
#endif
}
#endif

#ifdef __WXMSW__
wxCairoContext::wxCairoContext( wxGraphicsRenderer* renderer, HDC handle )
: wxGraphicsContext(renderer)
{
    m_mswSurface = cairo_win32_surface_create(handle);
    Init( cairo_create(m_mswSurface) );
    m_width =
    m_height = 0;
}
#endif


wxCairoContext::wxCairoContext( wxGraphicsRenderer* renderer, cairo_t *context )
: wxGraphicsContext(renderer)
{
    Init( context );
    m_width =
    m_height = 0;
}

wxCairoContext::wxCairoContext( wxGraphicsRenderer* renderer, wxWindow *window)
    : wxGraphicsContext(renderer)
#ifdef __WXMSW__
    , m_mswWindowHDC(GetHwndOf(window))
#endif
{
    m_enableOffset = true;    
#ifdef __WXGTK__
    // something along these lines (copied from dcclient)

    // Some controls don't have m_wxwindow - like wxStaticBox, but the user
    // code should still be able to create wxClientDCs for them, so we will
    // use the parent window here then.
    if (window->m_wxwindow == NULL)
    {
        window = window->GetParent();
    }

    wxASSERT_MSG( window->m_wxwindow, wxT("wxCairoContext needs a widget") );

    Init(gdk_cairo_create(window->GTKGetDrawingWindow()));

    wxSize sz = window->GetSize();
    m_width = sz.x;
    m_height = sz.y;
#endif

#ifdef __WXMSW__
    m_mswSurface = cairo_win32_surface_create((HDC)m_mswWindowHDC);
    Init(cairo_create(m_mswSurface));
#endif

}

wxCairoContext::wxCairoContext(wxGraphicsRenderer* renderer) :
    wxGraphicsContext(renderer)
{
    m_context = NULL;
}

wxCairoContext::~wxCairoContext()
{
    if ( m_context )
    {
        PopState();
        PopState();
        cairo_destroy(m_context);
    }
#ifdef __WXMSW__
    if ( m_mswSurface )
        cairo_surface_destroy(m_mswSurface);
#endif
}

void wxCairoContext::Init(cairo_t *context)
{
    m_context = context ;
    PushState();
    PushState();
}


void wxCairoContext::Clip( const wxRegion& region )
{
    // Create a path with all the rectangles in the region
    wxGraphicsPath path = GetRenderer()->CreatePath();
    wxRegionIterator ri(region);
    while (ri)
    {
        path.AddRectangle(ri.GetX(), ri.GetY(), ri.GetW(), ri.GetH());
        ++ri;
    }

    // Put it in the context
    cairo_path_t* cp = (cairo_path_t*) path.GetNativePath() ;
    cairo_append_path(m_context, cp);

    // clip to that path
    cairo_clip(m_context);
    path.UnGetNativePath(cp);
}

void wxCairoContext::Clip( wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    // Create a path with this rectangle
    wxGraphicsPath path = GetRenderer()->CreatePath();
    path.AddRectangle(x,y,w,h);

    // Put it in the context
    cairo_path_t* cp = (cairo_path_t*) path.GetNativePath() ;
    cairo_append_path(m_context, cp);

    // clip to that path
    cairo_clip(m_context);
    path.UnGetNativePath(cp);
}

void wxCairoContext::ResetClip()
{
    cairo_reset_clip(m_context);
}


void wxCairoContext::StrokePath( const wxGraphicsPath& path )
{
    if ( !m_pen.IsNull() )
    {
        wxCairoOffsetHelper helper( m_context, ShouldOffset() ) ;
        cairo_path_t* cp = (cairo_path_t*) path.GetNativePath() ;
        cairo_append_path(m_context,cp);
        ((wxCairoPenData*)m_pen.GetRefData())->Apply(this);
        cairo_stroke(m_context);
        path.UnGetNativePath(cp);
    }
}

void wxCairoContext::FillPath( const wxGraphicsPath& path , wxPolygonFillMode fillStyle )
{
    if ( !m_brush.IsNull() )
    {
        wxCairoOffsetHelper helper( m_context, ShouldOffset() ) ;
        cairo_path_t* cp = (cairo_path_t*) path.GetNativePath() ;
        cairo_append_path(m_context,cp);
        ((wxCairoBrushData*)m_brush.GetRefData())->Apply(this);
        cairo_set_fill_rule(m_context,fillStyle==wxODDEVEN_RULE ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
        cairo_fill(m_context);
        path.UnGetNativePath(cp);
    }
}

void wxCairoContext::Rotate( wxDouble angle )
{
    cairo_rotate(m_context,angle);
}

void wxCairoContext::Translate( wxDouble dx , wxDouble dy )
{
    cairo_translate(m_context,dx,dy);
}

void wxCairoContext::Scale( wxDouble xScale , wxDouble yScale )
{
    cairo_scale(m_context,xScale,yScale);
}

// concatenates this transform with the current transform of this context
void wxCairoContext::ConcatTransform( const wxGraphicsMatrix& matrix )
{
    cairo_transform(m_context,(const cairo_matrix_t *) matrix.GetNativeMatrix());
}

// sets the transform of this context
void wxCairoContext::SetTransform( const wxGraphicsMatrix& matrix )
{
    cairo_set_matrix(m_context,(const cairo_matrix_t*) matrix.GetNativeMatrix());
}

// gets the matrix of this context
wxGraphicsMatrix wxCairoContext::GetTransform() const
{
    wxGraphicsMatrix matrix = CreateMatrix();
    cairo_get_matrix(m_context,(cairo_matrix_t*) matrix.GetNativeMatrix());
    return matrix;
}



void wxCairoContext::PushState()
{
    cairo_save(m_context);
}

void wxCairoContext::PopState()
{
    cairo_restore(m_context);
}

void wxCairoContext::DrawBitmap( const wxBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    wxGraphicsBitmap bitmap = GetRenderer()->CreateBitmap(bmp);
    DrawBitmap(bitmap, x, y, w, h);

}

void wxCairoContext::DrawBitmap(const wxGraphicsBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    PushState();

    // In case we're scaling the image by using a width and height different
    // than the bitmap's size create a pattern transformation on the surface and
    // draw the transformed pattern.
    wxCairoBitmapData* data = static_cast<wxCairoBitmapData*>(bmp.GetRefData());
    cairo_pattern_t* pattern = data->GetCairoPattern();
    wxSize size = data->GetSize();

    wxDouble scaleX = w / size.GetWidth();
    wxDouble scaleY = h / size.GetHeight();

    // prepare to draw the image
    cairo_translate(m_context, x, y);
    cairo_scale(m_context, scaleX, scaleY);
    cairo_set_source(m_context, pattern);
    // use the original size here since the context is scaled already...
    cairo_rectangle(m_context, 0, 0, size.GetWidth(), size.GetHeight());
    // fill the rectangle using the pattern
    cairo_fill(m_context);

    PopState();
}

void wxCairoContext::DrawIcon( const wxIcon &icon, wxDouble x, wxDouble y, wxDouble w, wxDouble h )
{
    // An icon is a bitmap on wxGTK, so do this the easy way.  When we want to
    // start using the Cairo backend on other platforms then we may need to
    // fiddle with this...
    DrawBitmap(icon, x, y, w, h);
}


void wxCairoContext::DoDrawText(const wxString& str, wxDouble x, wxDouble y)
{
    wxCHECK_RET( !m_font.IsNull(),
                 wxT("wxCairoContext::DrawText - no valid font set") );

    if ( str.empty())
        return;

    const wxCharBuffer data = str.utf8_str();
    if ( !data )
        return;

    if ( ((wxCairoFontData*)m_font.GetRefData())->Apply(this) )
    {
#ifdef __WXGTK__
        PangoLayout *layout = pango_cairo_create_layout (m_context);
        const wxFont& font = static_cast<wxCairoFontData*>(m_font.GetRefData())->GetFont();
        pango_layout_set_font_description(layout, font.GetNativeFontInfo()->description);
        pango_layout_set_text(layout, data, data.length());
        font.GTKSetPangoAttrs(layout);

        cairo_move_to(m_context, x, y);
        pango_cairo_show_layout (m_context, layout);

        g_object_unref (layout);

        // Don't use Cairo text API, we already did everything.
        return;
#endif
    }

    // Cairo's x,y for drawing text is at the baseline, so we need to adjust
    // the position we move to by the ascent.
    cairo_font_extents_t fe;
    cairo_font_extents(m_context, &fe);
    cairo_move_to(m_context, x, y+fe.ascent);

    cairo_show_text(m_context, data);
}

void wxCairoContext::GetTextExtent( const wxString &str, wxDouble *width, wxDouble *height,
                                    wxDouble *descent, wxDouble *externalLeading ) const
{
    wxCHECK_RET( !m_font.IsNull(), wxT("wxCairoContext::GetTextExtent - no valid font set") );

    if ( width )
        *width = 0;
    if ( height )
        *height = 0;
    if ( descent )
        *descent = 0;
    if ( externalLeading )
        *externalLeading = 0;

    if ( str.empty())
        return;

    if ( ((wxCairoFontData*)m_font.GetRefData())->Apply((wxCairoContext*)this) )
    {
#ifdef __WXGTK__
        int w, h;

        PangoLayout *layout = pango_cairo_create_layout (m_context);
        const wxFont& font = static_cast<wxCairoFontData*>(m_font.GetRefData())->GetFont();
        pango_layout_set_font_description(layout, font.GetNativeFontInfo()->description);
        const wxCharBuffer data = str.utf8_str();
        if ( !data )
        {
            return;
        }
        pango_layout_set_text(layout, data, data.length());
        pango_layout_get_pixel_size (layout, &w, &h);
        if ( width )
            *width = w;
        if ( height )
            *height = h;
        if (descent)
        {
            PangoLayoutIter *iter = pango_layout_get_iter(layout);
            int baseline = pango_layout_iter_get_baseline(iter);
            pango_layout_iter_free(iter);
            *descent = h - PANGO_PIXELS(baseline);
        }
        g_object_unref (layout);
        return;
#endif
    }

    if (width)
    {
        const wxWX2MBbuf buf(str.mb_str(wxConvUTF8));
        cairo_text_extents_t te;
        cairo_text_extents(m_context, buf, &te);
        *width = te.width;
    }

    if (height || descent || externalLeading)
    {
        cairo_font_extents_t fe;
        cairo_font_extents(m_context, &fe);

        // some backends have negative descents

        if ( fe.descent < 0 )
            fe.descent = -fe.descent;

        if ( fe.height < (fe.ascent + fe.descent ) )
        {
            // some backends are broken re height ... (eg currently ATSUI)
            fe.height = fe.ascent + fe.descent;
        }

        if (height)
            *height = fe.height;
        if ( descent )
            *descent = fe.descent;
        if ( externalLeading )
            *externalLeading = wxMax(0, fe.height - (fe.ascent + fe.descent));
    }
}

void wxCairoContext::GetPartialTextExtents(const wxString& text, wxArrayDouble& widths) const
{
    widths.Empty();
    wxCHECK_RET( !m_font.IsNull(), wxT("wxCairoContext::GetPartialTextExtents - no valid font set") );
#ifdef __WXGTK__
    const wxCharBuffer data = text.utf8_str();
    int w = 0;
    if (data.length())
    {
        PangoLayout* layout = pango_cairo_create_layout(m_context);
        const wxFont& font = static_cast<wxCairoFontData*>(m_font.GetRefData())->GetFont();
        pango_layout_set_font_description(layout, font.GetNativeFontInfo()->description);
        pango_layout_set_text(layout, data, data.length());
        PangoLayoutIter* iter = pango_layout_get_iter(layout);
        PangoRectangle rect;
        do {
            pango_layout_iter_get_cluster_extents(iter, NULL, &rect);
            w += rect.width;
            widths.Add(PANGO_PIXELS(w));
        } while (pango_layout_iter_next_cluster(iter));
        pango_layout_iter_free(iter);
        g_object_unref(layout);
    }
    size_t i = widths.GetCount();
    const size_t len = text.length();
    while (i++ < len)
        widths.Add(PANGO_PIXELS(w));
#else
    // TODO
#endif
}

void * wxCairoContext::GetNativeContext()
{
    return m_context;
}

bool wxCairoContext::SetAntialiasMode(wxAntialiasMode antialias)
{
    if (m_antialias == antialias)
        return true;

    m_antialias = antialias;

    cairo_antialias_t antialiasMode;
    switch (antialias)
    {
        case wxANTIALIAS_DEFAULT:
            antialiasMode = CAIRO_ANTIALIAS_DEFAULT;
            break;
        case wxANTIALIAS_NONE:
            antialiasMode = CAIRO_ANTIALIAS_NONE;
            break;
        default:
            return false;
    }
    cairo_set_antialias(m_context, antialiasMode);
    return true;
}

bool wxCairoContext::SetInterpolationQuality(wxInterpolationQuality WXUNUSED(interpolation))
{
    // placeholder
    return false;
}

bool wxCairoContext::SetCompositionMode(wxCompositionMode op)
{
    if ( m_composition == op )
        return true;

    m_composition = op;
    cairo_operator_t cop;
    switch (op)
    {
        case wxCOMPOSITION_CLEAR:
            cop = CAIRO_OPERATOR_CLEAR;
            break;
        case wxCOMPOSITION_SOURCE:
            cop = CAIRO_OPERATOR_SOURCE;
            break;
        case wxCOMPOSITION_OVER:
            cop = CAIRO_OPERATOR_OVER;
            break;
        case wxCOMPOSITION_IN:
            cop = CAIRO_OPERATOR_IN;
            break;
        case wxCOMPOSITION_OUT:
            cop = CAIRO_OPERATOR_OUT;
            break;
        case wxCOMPOSITION_ATOP:
            cop = CAIRO_OPERATOR_ATOP;
            break;
        case wxCOMPOSITION_DEST:
            cop = CAIRO_OPERATOR_DEST;
            break;
        case wxCOMPOSITION_DEST_OVER:
            cop = CAIRO_OPERATOR_DEST_OVER;
            break;
        case wxCOMPOSITION_DEST_IN:
            cop = CAIRO_OPERATOR_DEST_IN;
            break;
        case wxCOMPOSITION_DEST_OUT:
            cop = CAIRO_OPERATOR_DEST_OUT;
            break;
        case wxCOMPOSITION_DEST_ATOP:
            cop = CAIRO_OPERATOR_DEST_ATOP;
            break;
        case wxCOMPOSITION_XOR:
            cop = CAIRO_OPERATOR_XOR;
            break;
        case wxCOMPOSITION_ADD:
            cop = CAIRO_OPERATOR_ADD;
            break;
        default:
            return false;
    }
    cairo_set_operator(m_context, cop);
    return true;
}

void wxCairoContext::BeginLayer(wxDouble opacity)
{
    m_layerOpacities.push_back(opacity);
    cairo_push_group(m_context);
}

void wxCairoContext::EndLayer()
{
    float opacity = m_layerOpacities.back();
    m_layerOpacities.pop_back();
    cairo_pop_group_to_source(m_context);
    cairo_paint_with_alpha(m_context,opacity);
}

//-----------------------------------------------------------------------------
// wxCairoRenderer declaration
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxCairoRenderer : public wxGraphicsRenderer
{
public :
    wxCairoRenderer() {}

    virtual ~wxCairoRenderer() {}

    // Context

    virtual wxGraphicsContext * CreateContext( const wxWindowDC& dc);
    virtual wxGraphicsContext * CreateContext( const wxMemoryDC& dc);
#if wxUSE_PRINTING_ARCHITECTURE
    virtual wxGraphicsContext * CreateContext( const wxPrinterDC& dc);
#endif

    virtual wxGraphicsContext * CreateContextFromNativeContext( void * context );

    virtual wxGraphicsContext * CreateContextFromNativeWindow( void * window );
#if wxUSE_IMAGE
    virtual wxGraphicsContext * CreateContextFromImage(wxImage& image);
#endif // wxUSE_IMAGE

    virtual wxGraphicsContext * CreateContext( wxWindow* window );

    virtual wxGraphicsContext * CreateMeasuringContext();
#ifdef __WXMSW__
#if wxUSE_ENH_METAFILE
    virtual wxGraphicsContext * CreateContext( const wxEnhMetaFileDC& dc);
#endif
#endif
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

    // sets the font
    virtual wxGraphicsFont CreateFont( const wxFont &font , const wxColour &col = *wxBLACK ) ;
    virtual wxGraphicsFont CreateFont(double sizeInPixels,
                                      const wxString& facename,
                                      int flags = wxFONTFLAG_DEFAULT,
                                      const wxColour& col = *wxBLACK);

    // create a native bitmap representation
    virtual wxGraphicsBitmap CreateBitmap( const wxBitmap &bitmap );
#if wxUSE_IMAGE
    virtual wxGraphicsBitmap CreateBitmapFromImage(const wxImage& image);
    virtual wxImage CreateImageFromBitmap(const wxGraphicsBitmap& bmp);
#endif // wxUSE_IMAGE

    // create a graphics bitmap from a native bitmap
    virtual wxGraphicsBitmap CreateBitmapFromNativeBitmap( void* bitmap );

    // create a subimage from a native image representation
    virtual wxGraphicsBitmap CreateSubBitmap( const wxGraphicsBitmap &bitmap, wxDouble x, wxDouble y, wxDouble w, wxDouble h  );

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxCairoRenderer)
} ;

//-----------------------------------------------------------------------------
// wxCairoRenderer implementation
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxCairoRenderer,wxGraphicsRenderer)

static wxCairoRenderer gs_cairoGraphicsRenderer;

#ifdef __WXGTK__
    #define ENSURE_LOADED_OR_RETURN(returnOnFail)
#else
    #define ENSURE_LOADED_OR_RETURN(returnOnFail)  \
        if (!wxCairoInit())                        \
            return returnOnFail
#endif

wxGraphicsContext * wxCairoRenderer::CreateContext( const wxWindowDC& dc)
{
    ENSURE_LOADED_OR_RETURN(NULL);
    return new wxCairoContext(this,dc);
}

wxGraphicsContext * wxCairoRenderer::CreateContext( const wxMemoryDC& dc)
{
    ENSURE_LOADED_OR_RETURN(NULL);
    return new wxCairoContext(this,dc);
}

#if wxUSE_PRINTING_ARCHITECTURE
wxGraphicsContext * wxCairoRenderer::CreateContext( const wxPrinterDC& dc)
{
    ENSURE_LOADED_OR_RETURN(NULL);
    return new wxCairoContext(this, dc);
}
#endif

#ifdef __WXMSW__
#if wxUSE_ENH_METAFILE
wxGraphicsContext * wxCairoRenderer::CreateContext( const wxEnhMetaFileDC& WXUNUSED(dc) )
{
    return NULL;
}
#endif
#endif

wxGraphicsContext * wxCairoRenderer::CreateContextFromNativeContext( void * context )
{
    ENSURE_LOADED_OR_RETURN(NULL);
#ifdef __WXMSW__
    return new wxCairoContext(this,(HDC)context);
#else
    return new wxCairoContext(this,(cairo_t*)context);
#endif
}


wxGraphicsContext * wxCairoRenderer::CreateContextFromNativeWindow( void * window )
{
#ifdef __WXGTK__
    return new wxCairoContext(this, static_cast<GdkWindow*>(window));
#else
    wxUnusedVar(window);
    return NULL;
#endif
}

#if wxUSE_IMAGE
wxGraphicsContext * wxCairoRenderer::CreateContextFromImage(wxImage& image)
{
    ENSURE_LOADED_OR_RETURN(NULL);
    return new wxCairoImageContext(this, image);
}
#endif // wxUSE_IMAGE

wxGraphicsContext * wxCairoRenderer::CreateMeasuringContext()
{
#ifdef __WXGTK__
    return CreateContextFromNativeWindow(gdk_get_default_root_window());
#else
    return NULL;
    // TODO
#endif
}

wxGraphicsContext * wxCairoRenderer::CreateContext( wxWindow* window )
{
    ENSURE_LOADED_OR_RETURN(NULL);
    return new wxCairoContext(this, window );
}

// Path

wxGraphicsPath wxCairoRenderer::CreatePath()
{
    wxGraphicsPath path;
    ENSURE_LOADED_OR_RETURN(path);
    path.SetRefData( new wxCairoPathData(this) );
    return path;
}


// Matrix

wxGraphicsMatrix wxCairoRenderer::CreateMatrix( wxDouble a, wxDouble b, wxDouble c, wxDouble d,
                                                wxDouble tx, wxDouble ty)

{
    wxGraphicsMatrix m;
    ENSURE_LOADED_OR_RETURN(m);
    wxCairoMatrixData* data = new wxCairoMatrixData( this );
    data->Set( a,b,c,d,tx,ty ) ;
    m.SetRefData(data);
    return m;
}

wxGraphicsPen wxCairoRenderer::CreatePen(const wxPen& pen)
{
    wxGraphicsPen p;
    ENSURE_LOADED_OR_RETURN(p);
    if (pen.IsOk() && pen.GetStyle() != wxPENSTYLE_TRANSPARENT)
    {
        p.SetRefData(new wxCairoPenData( this, pen ));
    }
    return p;
}

wxGraphicsBrush wxCairoRenderer::CreateBrush(const wxBrush& brush )
{
    wxGraphicsBrush p;
    ENSURE_LOADED_OR_RETURN(p);
    if (brush.IsOk() && brush.GetStyle() != wxBRUSHSTYLE_TRANSPARENT)
    {
        p.SetRefData(new wxCairoBrushData( this, brush ));
    }
    return p;
}

wxGraphicsBrush
wxCairoRenderer::CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                                           wxDouble x2, wxDouble y2,
                                           const wxGraphicsGradientStops& stops)
{
    wxGraphicsBrush p;
    ENSURE_LOADED_OR_RETURN(p);
    wxCairoBrushData* d = new wxCairoBrushData( this );
    d->CreateLinearGradientBrush(x1, y1, x2, y2, stops);
    p.SetRefData(d);
    return p;
}

wxGraphicsBrush
wxCairoRenderer::CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                                           wxDouble xc, wxDouble yc, wxDouble r,
                                           const wxGraphicsGradientStops& stops)
{
    wxGraphicsBrush p;
    ENSURE_LOADED_OR_RETURN(p);
    wxCairoBrushData* d = new wxCairoBrushData( this );
    d->CreateRadialGradientBrush(xo, yo, xc, yc, r, stops);
    p.SetRefData(d);
    return p;
}

wxGraphicsFont wxCairoRenderer::CreateFont( const wxFont &font , const wxColour &col )
{
    wxGraphicsFont p;
    ENSURE_LOADED_OR_RETURN(p);
    if ( font.IsOk() )
    {
        p.SetRefData(new wxCairoFontData( this , font, col ));
    }
    return p;
}

wxGraphicsFont
wxCairoRenderer::CreateFont(double sizeInPixels,
                            const wxString& facename,
                            int flags,
                            const wxColour& col)
{
    wxGraphicsFont font;
    ENSURE_LOADED_OR_RETURN(font);
    font.SetRefData(new wxCairoFontData(this, sizeInPixels, facename, flags, col));
    return font;
}

wxGraphicsBitmap wxCairoRenderer::CreateBitmap( const wxBitmap& bmp )
{
    wxGraphicsBitmap p;
    ENSURE_LOADED_OR_RETURN(p);
    if ( bmp.IsOk() )
    {
        p.SetRefData(new wxCairoBitmapData( this , bmp ));
    }
    return p;
}

#if wxUSE_IMAGE

wxGraphicsBitmap wxCairoRenderer::CreateBitmapFromImage(const wxImage& image)
{
    wxGraphicsBitmap bmp;

    ENSURE_LOADED_OR_RETURN(bmp);

    if ( image.IsOk() )
    {
        bmp.SetRefData(new wxCairoBitmapData(this, image));
    }

    return bmp;
}

wxImage wxCairoRenderer::CreateImageFromBitmap(const wxGraphicsBitmap& bmp)
{
    wxImage image;
    ENSURE_LOADED_OR_RETURN(image);

    const wxCairoBitmapData* const
        data = static_cast<wxCairoBitmapData*>(bmp.GetGraphicsData());
    if (data)
        image = data->ConvertToImage();

    return image;
}

#endif // wxUSE_IMAGE


wxGraphicsBitmap wxCairoRenderer::CreateBitmapFromNativeBitmap( void* bitmap )
{
    wxGraphicsBitmap p;
    ENSURE_LOADED_OR_RETURN(p);
    if ( bitmap != NULL )
    {
        p.SetRefData(new wxCairoBitmapData( this , (cairo_surface_t*) bitmap ));
    }
    return p;
}

wxGraphicsBitmap
wxCairoRenderer::CreateSubBitmap(const wxGraphicsBitmap& WXUNUSED(bitmap),
                                 wxDouble WXUNUSED(x),
                                 wxDouble WXUNUSED(y),
                                 wxDouble WXUNUSED(w),
                                 wxDouble WXUNUSED(h))
{
    wxGraphicsBitmap p;
    wxFAIL_MSG("wxCairoRenderer::CreateSubBitmap is not implemented.");
    return p;
}

wxGraphicsRenderer* wxGraphicsRenderer::GetCairoRenderer()
{
    return &gs_cairoGraphicsRenderer;
}

#else // !wxUSE_CAIRO

wxGraphicsRenderer* wxGraphicsRenderer::GetCairoRenderer()
{
    return NULL;
}

#endif  // wxUSE_CAIRO/!wxUSE_CAIRO

// MSW and OS X have their own native default renderers, but the other ports
// use Cairo by default
#if !(defined(__WXMSW__) || defined(__WXOSX__))
wxGraphicsRenderer* wxGraphicsRenderer::GetDefaultRenderer()
{
    return GetCairoRenderer();
}
#endif // !(__WXMSW__ || __WXOSX__)

#endif // wxUSE_GRAPHICS_CONTEXT
