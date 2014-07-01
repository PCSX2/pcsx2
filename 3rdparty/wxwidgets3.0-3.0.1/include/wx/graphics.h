/////////////////////////////////////////////////////////////////////////////
// Name:        wx/graphics.h
// Purpose:     graphics context header
// Author:      Stefan Csomor
// Modified by:
// Created:
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GRAPHICS_H_
#define _WX_GRAPHICS_H_

#include "wx/defs.h"

#if wxUSE_GRAPHICS_CONTEXT

#include "wx/geometry.h"
#include "wx/dynarray.h"
#include "wx/dc.h"
#include "wx/image.h"
#include "wx/vector.h"

enum wxAntialiasMode
{
    wxANTIALIAS_NONE, // should be 0
    wxANTIALIAS_DEFAULT
};

enum wxInterpolationQuality
{
    // default interpolation
    wxINTERPOLATION_DEFAULT,
    // no interpolation
    wxINTERPOLATION_NONE, 
    // fast interpolation, suited for interactivity
    wxINTERPOLATION_FAST,
    // better quality
    wxINTERPOLATION_GOOD,
    // best quality, not suited for interactivity
    wxINTERPOLATION_BEST
};

enum wxCompositionMode
{
    // R = Result, S = Source, D = Destination, premultiplied with alpha
    // Ra, Sa, Da their alpha components

    // classic Porter-Duff compositions
    // http://keithp.com/~keithp/porterduff/p253-porter.pdf

    wxCOMPOSITION_INVALID = -1, /* indicates invalid/unsupported mode */
    wxCOMPOSITION_CLEAR, /* R = 0 */
    wxCOMPOSITION_SOURCE, /* R = S */
    wxCOMPOSITION_OVER, /* R = S + D*(1 - Sa) */
    wxCOMPOSITION_IN, /* R = S*Da */
    wxCOMPOSITION_OUT, /* R = S*(1 - Da) */
    wxCOMPOSITION_ATOP, /* R = S*Da + D*(1 - Sa) */

    wxCOMPOSITION_DEST, /* R = D, essentially a noop */
    wxCOMPOSITION_DEST_OVER, /* R = S*(1 - Da) + D */
    wxCOMPOSITION_DEST_IN, /* R = D*Sa */
    wxCOMPOSITION_DEST_OUT, /* R = D*(1 - Sa) */
    wxCOMPOSITION_DEST_ATOP, /* R = S*(1 - Da) + D*Sa */
    wxCOMPOSITION_XOR, /* R = S*(1 - Da) + D*(1 - Sa) */

    // mathematical compositions
    wxCOMPOSITION_ADD /* R = S + D */
};

class WXDLLIMPEXP_FWD_CORE wxWindowDC;
class WXDLLIMPEXP_FWD_CORE wxMemoryDC;
#if wxUSE_PRINTING_ARCHITECTURE
class WXDLLIMPEXP_FWD_CORE wxPrinterDC;
#endif
#ifdef __WXMSW__
#if wxUSE_ENH_METAFILE
class WXDLLIMPEXP_FWD_CORE wxEnhMetaFileDC;
#endif
#endif
class WXDLLIMPEXP_FWD_CORE wxGraphicsContext;
class WXDLLIMPEXP_FWD_CORE wxGraphicsPath;
class WXDLLIMPEXP_FWD_CORE wxGraphicsMatrix;
class WXDLLIMPEXP_FWD_CORE wxGraphicsFigure;
class WXDLLIMPEXP_FWD_CORE wxGraphicsRenderer;
class WXDLLIMPEXP_FWD_CORE wxGraphicsPen;
class WXDLLIMPEXP_FWD_CORE wxGraphicsBrush;
class WXDLLIMPEXP_FWD_CORE wxGraphicsFont;
class WXDLLIMPEXP_FWD_CORE wxGraphicsBitmap;

/*
 * notes about the graphics context apis
 *
 * angles : are measured in radians, 0.0 being in direction of positiv x axis, PI/2 being
 * in direction of positive y axis.
 */

// Base class of all objects used for drawing in the new graphics API, the always point back to their
// originating rendering engine, there is no dynamic unloading of a renderer currently allowed,
// these references are not counted

//
// The data used by objects like graphics pens etc is ref counted, in order to avoid unnecessary expensive
// duplication. Any operation on a shared instance that results in a modified state, uncouples this
// instance from the other instances that were shared - using copy on write semantics
//

class WXDLLIMPEXP_FWD_CORE wxGraphicsObjectRefData;
class WXDLLIMPEXP_FWD_CORE wxGraphicsBitmapData;
class WXDLLIMPEXP_FWD_CORE wxGraphicsMatrixData;
class WXDLLIMPEXP_FWD_CORE wxGraphicsPathData;

class WXDLLIMPEXP_CORE wxGraphicsObject : public wxObject
{
public:
    wxGraphicsObject();
    wxGraphicsObject( wxGraphicsRenderer* renderer );
    virtual ~wxGraphicsObject();

    bool IsNull() const;

    // returns the renderer that was used to create this instance, or NULL if it has not been initialized yet
    wxGraphicsRenderer* GetRenderer() const;
    wxGraphicsObjectRefData* GetGraphicsData() const;
protected:
    virtual wxObjectRefData* CreateRefData() const;
    virtual wxObjectRefData* CloneRefData(const wxObjectRefData* data) const;

    DECLARE_DYNAMIC_CLASS(wxGraphicsObject)
};

class WXDLLIMPEXP_CORE wxGraphicsPen : public wxGraphicsObject
{
public:
    wxGraphicsPen() {}
    virtual ~wxGraphicsPen() {}
private:
    DECLARE_DYNAMIC_CLASS(wxGraphicsPen)
};

extern WXDLLIMPEXP_DATA_CORE(wxGraphicsPen) wxNullGraphicsPen;

class WXDLLIMPEXP_CORE wxGraphicsBrush : public wxGraphicsObject
{
public:
    wxGraphicsBrush() {}
    virtual ~wxGraphicsBrush() {}
private:
    DECLARE_DYNAMIC_CLASS(wxGraphicsBrush)
};

extern WXDLLIMPEXP_DATA_CORE(wxGraphicsBrush) wxNullGraphicsBrush;

class WXDLLIMPEXP_CORE wxGraphicsFont : public wxGraphicsObject
{
public:
    wxGraphicsFont() {}
    virtual ~wxGraphicsFont() {}
private:
    DECLARE_DYNAMIC_CLASS(wxGraphicsFont)
};

extern WXDLLIMPEXP_DATA_CORE(wxGraphicsFont) wxNullGraphicsFont;

class WXDLLIMPEXP_CORE wxGraphicsBitmap : public wxGraphicsObject
{
public:
    wxGraphicsBitmap() {}
    virtual ~wxGraphicsBitmap() {}

    // Convert bitmap to wxImage: this is more efficient than converting to
    // wxBitmap first and then to wxImage and also works without X server
    // connection under Unix that wxBitmap requires.
#if wxUSE_IMAGE
    wxImage ConvertToImage() const;
#endif // wxUSE_IMAGE
    
    void* GetNativeBitmap() const;

    const wxGraphicsBitmapData* GetBitmapData() const
    { return (const wxGraphicsBitmapData*) GetRefData(); }
    wxGraphicsBitmapData* GetBitmapData()
    { return (wxGraphicsBitmapData*) GetRefData(); }

private:
    DECLARE_DYNAMIC_CLASS(wxGraphicsBitmap)
};

extern WXDLLIMPEXP_DATA_CORE(wxGraphicsBitmap) wxNullGraphicsBitmap;

class WXDLLIMPEXP_CORE wxGraphicsMatrix : public wxGraphicsObject
{
public:
    wxGraphicsMatrix() {}

    virtual ~wxGraphicsMatrix() {}

    // concatenates the matrix
    virtual void Concat( const wxGraphicsMatrix *t );
    void Concat( const wxGraphicsMatrix &t ) { Concat( &t ); }

    // sets the matrix to the respective values
    virtual void Set(wxDouble a=1.0, wxDouble b=0.0, wxDouble c=0.0, wxDouble d=1.0,
        wxDouble tx=0.0, wxDouble ty=0.0);

    // gets the component valuess of the matrix
    virtual void Get(wxDouble* a=NULL, wxDouble* b=NULL,  wxDouble* c=NULL,
                     wxDouble* d=NULL, wxDouble* tx=NULL, wxDouble* ty=NULL) const;

    // makes this the inverse matrix
    virtual void Invert();

    // returns true if the elements of the transformation matrix are equal ?
    virtual bool IsEqual( const wxGraphicsMatrix* t) const;
    bool IsEqual( const wxGraphicsMatrix& t) const { return IsEqual( &t ); }

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

    const wxGraphicsMatrixData* GetMatrixData() const
    { return (const wxGraphicsMatrixData*) GetRefData(); }
    wxGraphicsMatrixData* GetMatrixData()
    { return (wxGraphicsMatrixData*) GetRefData(); }

private:
    DECLARE_DYNAMIC_CLASS(wxGraphicsMatrix)
};

extern WXDLLIMPEXP_DATA_CORE(wxGraphicsMatrix) wxNullGraphicsMatrix;

class WXDLLIMPEXP_CORE wxGraphicsPath : public wxGraphicsObject
{
public:
    wxGraphicsPath()  {}
    virtual ~wxGraphicsPath() {}

    //
    // These are the path primitives from which everything else can be constructed
    //

    // begins a new subpath at (x,y)
    virtual void MoveToPoint( wxDouble x, wxDouble y );
    void MoveToPoint( const wxPoint2DDouble& p);

    // adds a straight line from the current point to (x,y)
    virtual void AddLineToPoint( wxDouble x, wxDouble y );
    void AddLineToPoint( const wxPoint2DDouble& p);

    // adds a cubic Bezier curve from the current point, using two control points and an end point
    virtual void AddCurveToPoint( wxDouble cx1, wxDouble cy1, wxDouble cx2, wxDouble cy2, wxDouble x, wxDouble y );
    void AddCurveToPoint( const wxPoint2DDouble& c1, const wxPoint2DDouble& c2, const wxPoint2DDouble& e);

    // adds another path
    virtual void AddPath( const wxGraphicsPath& path );

    // closes the current sub-path
    virtual void CloseSubpath();

    // gets the last point of the current path, (0,0) if not yet set
    virtual void GetCurrentPoint( wxDouble* x, wxDouble* y) const;
    wxPoint2DDouble GetCurrentPoint() const;

    // adds an arc of a circle centering at (x,y) with radius (r) from startAngle to endAngle
    virtual void AddArc( wxDouble x, wxDouble y, wxDouble r, wxDouble startAngle, wxDouble endAngle, bool clockwise );
    void AddArc( const wxPoint2DDouble& c, wxDouble r, wxDouble startAngle, wxDouble endAngle, bool clockwise);

    //
    // These are convenience functions which - if not available natively will be assembled
    // using the primitives from above
    //

    // adds a quadratic Bezier curve from the current point, using a control point and an end point
    virtual void AddQuadCurveToPoint( wxDouble cx, wxDouble cy, wxDouble x, wxDouble y );

    // appends a rectangle as a new closed subpath
    virtual void AddRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h );

    // appends an ellipsis as a new closed subpath fitting the passed rectangle
    virtual void AddCircle( wxDouble x, wxDouble y, wxDouble r );

    // appends a an arc to two tangents connecting (current) to (x1,y1) and (x1,y1) to (x2,y2), also a straight line from (current) to (x1,y1)
    virtual void AddArcToPoint( wxDouble x1, wxDouble y1 , wxDouble x2, wxDouble y2, wxDouble r );

    // appends an ellipse
    virtual void AddEllipse( wxDouble x, wxDouble y, wxDouble w, wxDouble h);

    // appends a rounded rectangle
    virtual void AddRoundedRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h, wxDouble radius);

    // returns the native path
    virtual void * GetNativePath() const;

    // give the native path returned by GetNativePath() back (there might be some deallocations necessary)
    virtual void UnGetNativePath(void *p)const;

    // transforms each point of this path by the matrix
    virtual void Transform( const wxGraphicsMatrix& matrix );

    // gets the bounding box enclosing all points (possibly including control points)
    virtual void GetBox(wxDouble *x, wxDouble *y, wxDouble *w, wxDouble *h)const;
    wxRect2DDouble GetBox()const;

    virtual bool Contains( wxDouble x, wxDouble y, wxPolygonFillMode fillStyle = wxODDEVEN_RULE)const;
    bool Contains( const wxPoint2DDouble& c, wxPolygonFillMode fillStyle = wxODDEVEN_RULE)const;

    const wxGraphicsPathData* GetPathData() const
    { return (const wxGraphicsPathData*) GetRefData(); }
    wxGraphicsPathData* GetPathData()
    { return (wxGraphicsPathData*) GetRefData(); }

private:
    DECLARE_DYNAMIC_CLASS(wxGraphicsPath)
};

extern WXDLLIMPEXP_DATA_CORE(wxGraphicsPath) wxNullGraphicsPath;


// Describes a single gradient stop.
class wxGraphicsGradientStop
{
public:
    wxGraphicsGradientStop(wxColour col = wxTransparentColour,
                           float pos = 0.)
        : m_col(col),
          m_pos(pos)
    {
    }

    // default copy ctor, assignment operator and dtor are ok

    const wxColour& GetColour() const { return m_col; }
    void SetColour(const wxColour& col) { m_col = col; }

    float GetPosition() const { return m_pos; }
    void SetPosition(float pos)
    {
        wxASSERT_MSG( pos >= 0 && pos <= 1, "invalid gradient stop position" );

        m_pos = pos;
    }

private:
    // The colour of this gradient band.
    wxColour m_col;

    // Its starting position: 0 is the beginning and 1 is the end.
    float m_pos;
};

// A collection of gradient stops ordered by their positions (from lowest to
// highest). The first stop (index 0, position 0.0) is always the starting
// colour and the last one (index GetCount() - 1, position 1.0) is the end
// colour.
class WXDLLIMPEXP_CORE wxGraphicsGradientStops
{
public:
    wxGraphicsGradientStops(wxColour startCol = wxTransparentColour,
                            wxColour endCol = wxTransparentColour)
    {
        // we can't use Add() here as it relies on having start/end stops as
        // first/last array elements so do it manually
        m_stops.push_back(wxGraphicsGradientStop(startCol, 0.f));
        m_stops.push_back(wxGraphicsGradientStop(endCol, 1.f));
    }

    // default copy ctor, assignment operator and dtor are ok for this class


    // Add a stop in correct order.
    void Add(const wxGraphicsGradientStop& stop);
    void Add(wxColour col, float pos) { Add(wxGraphicsGradientStop(col, pos)); }

    // Get the number of stops.
    size_t GetCount() const { return m_stops.size(); }

    // Return the stop at the given index (which must be valid).
    wxGraphicsGradientStop Item(unsigned n) const { return m_stops.at(n); }

    // Get/set start and end colours.
    void SetStartColour(wxColour col)
        { m_stops[0].SetColour(col); }
    wxColour GetStartColour() const
        { return m_stops[0].GetColour(); }
    void SetEndColour(wxColour col)
        { m_stops[m_stops.size() - 1].SetColour(col); }
    wxColour GetEndColour() const
        { return m_stops[m_stops.size() - 1].GetColour(); }

private:
    // All the stops stored in ascending order of positions.
    wxVector<wxGraphicsGradientStop> m_stops;
};

class WXDLLIMPEXP_CORE wxGraphicsContext : public wxGraphicsObject
{
public:
    wxGraphicsContext(wxGraphicsRenderer* renderer);

    virtual ~wxGraphicsContext();

    static wxGraphicsContext* Create( const wxWindowDC& dc);
    static wxGraphicsContext * Create( const wxMemoryDC& dc);
#if wxUSE_PRINTING_ARCHITECTURE
    static wxGraphicsContext * Create( const wxPrinterDC& dc);
#endif
#ifdef __WXMSW__
#if wxUSE_ENH_METAFILE
    static wxGraphicsContext * Create( const wxEnhMetaFileDC& dc);
#endif
#endif

    static wxGraphicsContext* CreateFromNative( void * context );

    static wxGraphicsContext* CreateFromNativeWindow( void * window );

    static wxGraphicsContext* Create( wxWindow* window );

#if wxUSE_IMAGE
    // Create a context for drawing onto a wxImage. The image life time must be
    // greater than that of the context itself as when the context is destroyed
    // it will copy its contents to the specified image.
    static wxGraphicsContext* Create(wxImage& image);
#endif // wxUSE_IMAGE

    // create a context that can be used for measuring texts only, no drawing allowed
    static wxGraphicsContext * Create();

    // begin a new document (relevant only for printing / pdf etc) if there is a progress dialog, message will be shown
    virtual bool StartDoc( const wxString& message );

    // done with that document (relevant only for printing / pdf etc)
    virtual void EndDoc();

    // opens a new page  (relevant only for printing / pdf etc) with the given size in points
    // (if both are null the default page size will be used)
    virtual void StartPage( wxDouble width = 0, wxDouble height = 0 );

    // ends the current page  (relevant only for printing / pdf etc)
    virtual void EndPage();

    // make sure that the current content of this context is immediately visible
    virtual void Flush();

    wxGraphicsPath CreatePath() const;

    virtual wxGraphicsPen CreatePen(const wxPen& pen) const;

    virtual wxGraphicsBrush CreateBrush(const wxBrush& brush ) const;

    // sets the brush to a linear gradient, starting at (x1,y1) and ending at
    // (x2,y2) with the given boundary colours or the specified stops
    wxGraphicsBrush
    CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                              wxDouble x2, wxDouble y2,
                              const wxColour& c1, const wxColour& c2) const;
    wxGraphicsBrush
    CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                              wxDouble x2, wxDouble y2,
                              const wxGraphicsGradientStops& stops) const;

    // sets the brush to a radial gradient originating at (xo,yc) and ending
    // on a circle around (xc,yc) with the given radius; the colours may be
    // specified by just the two extremes or the full array of gradient stops
    wxGraphicsBrush
    CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                              wxDouble xc, wxDouble yc, wxDouble radius,
                              const wxColour& oColor, const wxColour& cColor) const;

    wxGraphicsBrush
    CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                              wxDouble xc, wxDouble yc, wxDouble radius,
                              const wxGraphicsGradientStops& stops) const;

    // creates a font
    virtual wxGraphicsFont CreateFont( const wxFont &font , const wxColour &col = *wxBLACK ) const;
    virtual wxGraphicsFont CreateFont(double sizeInPixels,
                                      const wxString& facename,
                                      int flags = wxFONTFLAG_DEFAULT,
                                      const wxColour& col = *wxBLACK) const;

    // create a native bitmap representation
    virtual wxGraphicsBitmap CreateBitmap( const wxBitmap &bitmap ) const;
#if wxUSE_IMAGE
    wxGraphicsBitmap CreateBitmapFromImage(const wxImage& image) const;
#endif // wxUSE_IMAGE

    // create a native bitmap representation
    virtual wxGraphicsBitmap CreateSubBitmap( const wxGraphicsBitmap &bitmap, wxDouble x, wxDouble y, wxDouble w, wxDouble h  ) const;

    // create a 'native' matrix corresponding to these values
    virtual wxGraphicsMatrix CreateMatrix( wxDouble a=1.0, wxDouble b=0.0, wxDouble c=0.0, wxDouble d=1.0,
        wxDouble tx=0.0, wxDouble ty=0.0) const;

    wxGraphicsMatrix CreateMatrix( const wxAffineMatrix2DBase& mat ) const
    {
        wxMatrix2D mat2D;
        wxPoint2DDouble tr;
        mat.Get(&mat2D, &tr);

        return CreateMatrix(mat2D.m_11, mat2D.m_12, mat2D.m_21, mat2D.m_22,
                            tr.m_x, tr.m_y);
    }

    // push the current state of the context, ie the transformation matrix on a stack
    virtual void PushState() = 0;

    // pops a stored state from the stack
    virtual void PopState() = 0;

    // clips drawings to the region intersected with the current clipping region
    virtual void Clip( const wxRegion &region ) = 0;

    // clips drawings to the rect intersected with the current clipping region
    virtual void Clip( wxDouble x, wxDouble y, wxDouble w, wxDouble h ) = 0;

    // resets the clipping to original extent
    virtual void ResetClip() = 0;

    // returns the native context
    virtual void * GetNativeContext() = 0;

    // returns the current shape antialiasing mode
    virtual wxAntialiasMode GetAntialiasMode() const { return m_antialias; }

    // sets the antialiasing mode, returns true if it supported
    virtual bool SetAntialiasMode(wxAntialiasMode antialias) = 0;

    // returns the current interpolation quality
    virtual wxInterpolationQuality GetInterpolationQuality() const { return m_interpolation; }
    
    // sets the interpolation quality, returns true if it supported
    virtual bool SetInterpolationQuality(wxInterpolationQuality interpolation) = 0;
    
    // returns the current compositing operator
    virtual wxCompositionMode GetCompositionMode() const { return m_composition; }

    // sets the compositing operator, returns true if it supported
    virtual bool SetCompositionMode(wxCompositionMode op) = 0;

    // returns the size of the graphics context in device coordinates
    void GetSize(wxDouble* width, wxDouble* height) const
    {
        if ( width )
            *width = m_width;
        if ( height )
            *height = m_height;
    }

    // returns the resolution of the graphics context in device points per inch
    virtual void GetDPI( wxDouble* dpiX, wxDouble* dpiY);

#if 0
    // sets the current alpha on this context
    virtual void SetAlpha( wxDouble alpha );

    // returns the alpha on this context
    virtual wxDouble GetAlpha() const;
#endif

    // all rendering is done into a fully transparent temporary context
    virtual void BeginLayer(wxDouble opacity) = 0;

    // composites back the drawings into the context with the opacity given at
    // the BeginLayer call
    virtual void EndLayer() = 0;

    //
    // transformation : changes the current transformation matrix CTM of the context
    //

    // translate
    virtual void Translate( wxDouble dx , wxDouble dy ) = 0;

    // scale
    virtual void Scale( wxDouble xScale , wxDouble yScale ) = 0;

    // rotate (radians)
    virtual void Rotate( wxDouble angle ) = 0;

    // concatenates this transform with the current transform of this context
    virtual void ConcatTransform( const wxGraphicsMatrix& matrix ) = 0;

    // sets the transform of this context
    virtual void SetTransform( const wxGraphicsMatrix& matrix ) = 0;

    // gets the matrix of this context
    virtual wxGraphicsMatrix GetTransform() const = 0;
    //
    // setting the paint
    //

    // sets the pen
    virtual void SetPen( const wxGraphicsPen& pen );

    void SetPen( const wxPen& pen );

    // sets the brush for filling
    virtual void SetBrush( const wxGraphicsBrush& brush );

    void SetBrush( const wxBrush& brush );

    // sets the font
    virtual void SetFont( const wxGraphicsFont& font );

    void SetFont( const wxFont& font, const wxColour& colour );


    // strokes along a path with the current pen
    virtual void StrokePath( const wxGraphicsPath& path ) = 0;

    // fills a path with the current brush
    virtual void FillPath( const wxGraphicsPath& path, wxPolygonFillMode fillStyle = wxODDEVEN_RULE ) = 0;

    // draws a path by first filling and then stroking
    virtual void DrawPath( const wxGraphicsPath& path, wxPolygonFillMode fillStyle = wxODDEVEN_RULE );

    //
    // text
    //

    void DrawText( const wxString &str, wxDouble x, wxDouble y )
        { DoDrawText(str, x, y); }

    void DrawText( const wxString &str, wxDouble x, wxDouble y, wxDouble angle )
        { DoDrawRotatedText(str, x, y, angle); }

    void DrawText( const wxString &str, wxDouble x, wxDouble y,
                   const wxGraphicsBrush& backgroundBrush )
        { DoDrawFilledText(str, x, y, backgroundBrush); }

    void DrawText( const wxString &str, wxDouble x, wxDouble y,
                   wxDouble angle, const wxGraphicsBrush& backgroundBrush )
        { DoDrawRotatedFilledText(str, x, y, angle, backgroundBrush); }


    virtual void GetTextExtent( const wxString &text, wxDouble *width, wxDouble *height,
        wxDouble *descent = NULL, wxDouble *externalLeading = NULL ) const  = 0;

    virtual void GetPartialTextExtents(const wxString& text, wxArrayDouble& widths) const = 0;

    //
    // image support
    //

    virtual void DrawBitmap( const wxGraphicsBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h ) = 0;

    virtual void DrawBitmap( const wxBitmap &bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h ) = 0;

    virtual void DrawIcon( const wxIcon &icon, wxDouble x, wxDouble y, wxDouble w, wxDouble h ) = 0;

    //
    // convenience methods
    //

    // strokes a single line
    virtual void StrokeLine( wxDouble x1, wxDouble y1, wxDouble x2, wxDouble y2);

    // stroke lines connecting each of the points
    virtual void StrokeLines( size_t n, const wxPoint2DDouble *points);

    // stroke disconnected lines from begin to end points
    virtual void StrokeLines( size_t n, const wxPoint2DDouble *beginPoints, const wxPoint2DDouble *endPoints);

    // draws a polygon
    virtual void DrawLines( size_t n, const wxPoint2DDouble *points, wxPolygonFillMode fillStyle = wxODDEVEN_RULE );

    // draws a rectangle
    virtual void DrawRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h);

    // draws an ellipse
    virtual void DrawEllipse( wxDouble x, wxDouble y, wxDouble w, wxDouble h);

    // draws a rounded rectangle
    virtual void DrawRoundedRectangle( wxDouble x, wxDouble y, wxDouble w, wxDouble h, wxDouble radius);

     // wrappers using wxPoint2DDouble TODO

    // helper to determine if a 0.5 offset should be applied for the drawing operation
    virtual bool ShouldOffset() const { return false; }
    
    // indicates whether the context should try to offset for pixel boundaries, this only makes sense on 
    // bitmap devices like screen, by default this is turned off
    virtual void EnableOffset(bool enable = true);
    
    void DisableOffset() { EnableOffset(false); }
    bool OffsetEnabled() { return m_enableOffset; }
    
protected:
    // These fields must be initialized in the derived class ctors.
    wxDouble m_width,
             m_height;

    wxGraphicsPen m_pen;
    wxGraphicsBrush m_brush;
    wxGraphicsFont m_font;
    wxAntialiasMode m_antialias;
    wxCompositionMode m_composition;
    wxInterpolationQuality m_interpolation;
    bool m_enableOffset;

protected:
    // implementations of overloaded public functions: we use different names
    // for them to avoid the virtual function hiding problems in the derived
    // classes
    virtual void DoDrawText(const wxString& str, wxDouble x, wxDouble y) = 0;
    virtual void DoDrawRotatedText(const wxString& str, wxDouble x, wxDouble y,
                                   wxDouble angle);
    virtual void DoDrawFilledText(const wxString& str, wxDouble x, wxDouble y,
                                  const wxGraphicsBrush& backgroundBrush);
    virtual void DoDrawRotatedFilledText(const wxString& str,
                                         wxDouble x, wxDouble y,
                                         wxDouble angle,
                                         const wxGraphicsBrush& backgroundBrush);

    wxDECLARE_NO_COPY_CLASS(wxGraphicsContext);
    DECLARE_ABSTRACT_CLASS(wxGraphicsContext)
};

#if 0

//
// A graphics figure allows to cache path, pen etc creations, also will be a basis for layering/grouping elements
//

class WXDLLIMPEXP_CORE wxGraphicsFigure : public wxGraphicsObject
{
public:
    wxGraphicsFigure(wxGraphicsRenderer* renderer);

    virtual ~wxGraphicsFigure();

    void SetPath( wxGraphicsMatrix* matrix );

    void SetMatrix( wxGraphicsPath* path);

    // draws this object on the context
    virtual void Draw( wxGraphicsContext* cg );

    // returns the path of this object
    wxGraphicsPath* GetPath() { return m_path; }

    // returns the transformation matrix of this object, may be null if there is no transformation necessary
    wxGraphicsMatrix* GetMatrix() { return m_matrix; }

private:
    wxGraphicsMatrix* m_matrix;
    wxGraphicsPath* m_path;

    DECLARE_DYNAMIC_CLASS(wxGraphicsFigure)
};

#endif

//
// The graphics renderer is the instance corresponding to the rendering engine used, eg there is ONE core graphics renderer
// instance on OSX. This instance is pointed back to by all objects created by it. Therefore you can create eg additional
// paths at any point from a given matrix etc.
//

class WXDLLIMPEXP_CORE wxGraphicsRenderer : public wxObject
{
public:
    wxGraphicsRenderer() {}

    virtual ~wxGraphicsRenderer() {}

    static wxGraphicsRenderer* GetDefaultRenderer();

    static wxGraphicsRenderer* GetCairoRenderer();
    // Context

    virtual wxGraphicsContext * CreateContext( const wxWindowDC& dc) = 0;
    virtual wxGraphicsContext * CreateContext( const wxMemoryDC& dc) = 0;
#if wxUSE_PRINTING_ARCHITECTURE
    virtual wxGraphicsContext * CreateContext( const wxPrinterDC& dc) = 0;
#endif
#ifdef __WXMSW__
#if wxUSE_ENH_METAFILE
    virtual wxGraphicsContext * CreateContext( const wxEnhMetaFileDC& dc) = 0;
#endif
#endif

    virtual wxGraphicsContext * CreateContextFromNativeContext( void * context ) = 0;

    virtual wxGraphicsContext * CreateContextFromNativeWindow( void * window ) = 0;

    virtual wxGraphicsContext * CreateContext( wxWindow* window ) = 0;

#if wxUSE_IMAGE
    virtual wxGraphicsContext * CreateContextFromImage(wxImage& image) = 0;
#endif // wxUSE_IMAGE

    // create a context that can be used for measuring texts only, no drawing allowed
    virtual wxGraphicsContext * CreateMeasuringContext() = 0;

    // Path

    virtual wxGraphicsPath CreatePath() = 0;

    // Matrix

    virtual wxGraphicsMatrix CreateMatrix( wxDouble a=1.0, wxDouble b=0.0, wxDouble c=0.0, wxDouble d=1.0,
        wxDouble tx=0.0, wxDouble ty=0.0) = 0;

    // Paints

    virtual wxGraphicsPen CreatePen(const wxPen& pen) = 0;

    virtual wxGraphicsBrush CreateBrush(const wxBrush& brush ) = 0;

    // Gradient brush creation functions may not honour all the stops specified
    // stops and use just its boundary colours (this is currently the case
    // under OS X)
    virtual wxGraphicsBrush
    CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                              wxDouble x2, wxDouble y2,
                              const wxGraphicsGradientStops& stops) = 0;

    virtual wxGraphicsBrush
    CreateRadialGradientBrush(wxDouble xo, wxDouble yo,
                              wxDouble xc, wxDouble yc,
                              wxDouble radius,
                              const wxGraphicsGradientStops& stops) = 0;

    // sets the font
    virtual wxGraphicsFont CreateFont( const wxFont &font , const wxColour &col = *wxBLACK ) = 0;
    virtual wxGraphicsFont CreateFont(double sizeInPixels,
                                      const wxString& facename,
                                      int flags = wxFONTFLAG_DEFAULT,
                                      const wxColour& col = *wxBLACK) = 0;

    // create a native bitmap representation
    virtual wxGraphicsBitmap CreateBitmap( const wxBitmap &bitmap ) = 0;
#if wxUSE_IMAGE
    virtual wxGraphicsBitmap CreateBitmapFromImage(const wxImage& image) = 0;
    virtual wxImage CreateImageFromBitmap(const wxGraphicsBitmap& bmp) = 0;
#endif // wxUSE_IMAGE

    // create a graphics bitmap from a native bitmap
    virtual wxGraphicsBitmap CreateBitmapFromNativeBitmap( void* bitmap ) = 0;

    // create a subimage from a native image representation
    virtual wxGraphicsBitmap CreateSubBitmap( const wxGraphicsBitmap &bitmap, wxDouble x, wxDouble y, wxDouble w, wxDouble h  ) = 0;

private:
    wxDECLARE_NO_COPY_CLASS(wxGraphicsRenderer);
    DECLARE_ABSTRACT_CLASS(wxGraphicsRenderer)
};


#if wxUSE_IMAGE
inline
wxImage wxGraphicsBitmap::ConvertToImage() const
{
    wxGraphicsRenderer* renderer = GetRenderer();
    return renderer ? renderer->CreateImageFromBitmap(*this) : wxNullImage;
}
#endif // wxUSE_IMAGE

#endif // wxUSE_GRAPHICS_CONTEXT

#endif // _WX_GRAPHICS_H_
