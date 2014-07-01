/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gdicmn.h
// Purpose:     Common GDI classes, types and declarations
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GDICMNH__
#define _WX_GDICMNH__

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#include "wx/defs.h"
#include "wx/list.h"
#include "wx/string.h"
#include "wx/fontenc.h"
#include "wx/hashmap.h"
#include "wx/math.h"

// ---------------------------------------------------------------------------
// forward declarations
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxBitmap;
class WXDLLIMPEXP_FWD_CORE wxBrush;
class WXDLLIMPEXP_FWD_CORE wxColour;
class WXDLLIMPEXP_FWD_CORE wxCursor;
class WXDLLIMPEXP_FWD_CORE wxFont;
class WXDLLIMPEXP_FWD_CORE wxIcon;
class WXDLLIMPEXP_FWD_CORE wxPalette;
class WXDLLIMPEXP_FWD_CORE wxPen;
class WXDLLIMPEXP_FWD_CORE wxRegion;
class WXDLLIMPEXP_FWD_BASE wxString;
class WXDLLIMPEXP_FWD_CORE wxIconBundle;
class WXDLLIMPEXP_FWD_CORE wxPoint;

// ---------------------------------------------------------------------------
// constants
// ---------------------------------------------------------------------------

// Bitmap flags
enum wxBitmapType
{
    wxBITMAP_TYPE_INVALID,          // should be == 0 for compatibility!
    wxBITMAP_TYPE_BMP,
    wxBITMAP_TYPE_BMP_RESOURCE,
    wxBITMAP_TYPE_RESOURCE = wxBITMAP_TYPE_BMP_RESOURCE,
    wxBITMAP_TYPE_ICO,
    wxBITMAP_TYPE_ICO_RESOURCE,
    wxBITMAP_TYPE_CUR,
    wxBITMAP_TYPE_CUR_RESOURCE,
    wxBITMAP_TYPE_XBM,
    wxBITMAP_TYPE_XBM_DATA,
    wxBITMAP_TYPE_XPM,
    wxBITMAP_TYPE_XPM_DATA,
    wxBITMAP_TYPE_TIFF,
    wxBITMAP_TYPE_TIF = wxBITMAP_TYPE_TIFF,
    wxBITMAP_TYPE_TIFF_RESOURCE,
    wxBITMAP_TYPE_TIF_RESOURCE = wxBITMAP_TYPE_TIFF_RESOURCE,
    wxBITMAP_TYPE_GIF,
    wxBITMAP_TYPE_GIF_RESOURCE,
    wxBITMAP_TYPE_PNG,
    wxBITMAP_TYPE_PNG_RESOURCE,
    wxBITMAP_TYPE_JPEG,
    wxBITMAP_TYPE_JPEG_RESOURCE,
    wxBITMAP_TYPE_PNM,
    wxBITMAP_TYPE_PNM_RESOURCE,
    wxBITMAP_TYPE_PCX,
    wxBITMAP_TYPE_PCX_RESOURCE,
    wxBITMAP_TYPE_PICT,
    wxBITMAP_TYPE_PICT_RESOURCE,
    wxBITMAP_TYPE_ICON,
    wxBITMAP_TYPE_ICON_RESOURCE,
    wxBITMAP_TYPE_ANI,
    wxBITMAP_TYPE_IFF,
    wxBITMAP_TYPE_TGA,
    wxBITMAP_TYPE_MACCURSOR,
    wxBITMAP_TYPE_MACCURSOR_RESOURCE,

    wxBITMAP_TYPE_MAX,
    wxBITMAP_TYPE_ANY = 50
};

//  Polygon filling mode
enum wxPolygonFillMode
{
    wxODDEVEN_RULE = 1,
    wxWINDING_RULE
};

// Standard cursors
enum wxStockCursor
{
    wxCURSOR_NONE,          // should be 0
    wxCURSOR_ARROW,
    wxCURSOR_RIGHT_ARROW,
    wxCURSOR_BULLSEYE,
    wxCURSOR_CHAR,
    wxCURSOR_CROSS,
    wxCURSOR_HAND,
    wxCURSOR_IBEAM,
    wxCURSOR_LEFT_BUTTON,
    wxCURSOR_MAGNIFIER,
    wxCURSOR_MIDDLE_BUTTON,
    wxCURSOR_NO_ENTRY,
    wxCURSOR_PAINT_BRUSH,
    wxCURSOR_PENCIL,
    wxCURSOR_POINT_LEFT,
    wxCURSOR_POINT_RIGHT,
    wxCURSOR_QUESTION_ARROW,
    wxCURSOR_RIGHT_BUTTON,
    wxCURSOR_SIZENESW,
    wxCURSOR_SIZENS,
    wxCURSOR_SIZENWSE,
    wxCURSOR_SIZEWE,
    wxCURSOR_SIZING,
    wxCURSOR_SPRAYCAN,
    wxCURSOR_WAIT,
    wxCURSOR_WATCH,
    wxCURSOR_BLANK,
#ifdef __WXGTK__
    wxCURSOR_DEFAULT, // standard X11 cursor
#endif
#ifdef __WXMAC__
    wxCURSOR_COPY_ARROW , // MacOS Theme Plus arrow
#endif
#ifdef __X__
    // Not yet implemented for Windows
    wxCURSOR_CROSS_REVERSE,
    wxCURSOR_DOUBLE_ARROW,
    wxCURSOR_BASED_ARROW_UP,
    wxCURSOR_BASED_ARROW_DOWN,
#endif // X11
    wxCURSOR_ARROWWAIT,
#ifdef __WXMAC__
    wxCURSOR_OPEN_HAND,
    wxCURSOR_CLOSED_HAND,
#endif

    wxCURSOR_MAX
};

#ifndef __WXGTK__
    #define wxCURSOR_DEFAULT        wxCURSOR_ARROW
#endif

#ifndef __WXMAC__
    // TODO CS supply openhand and closedhand cursors
    #define wxCURSOR_OPEN_HAND      wxCURSOR_HAND
    #define wxCURSOR_CLOSED_HAND    wxCURSOR_HAND
#endif

// ---------------------------------------------------------------------------
// macros
// ---------------------------------------------------------------------------

#if defined(__WINDOWS__) || defined(__WXPM__)
    #define wxHAS_IMAGES_IN_RESOURCES
#endif

/* Useful macro for creating icons portably, for example:

    wxIcon *icon = new wxICON(sample);

  expands into:

    wxIcon *icon = new wxIcon("sample");      // On Windows
    wxIcon *icon = new wxIcon(sample_xpm);    // On wxGTK/Linux
 */

#ifdef __WINDOWS__
    // Load from a resource
    #define wxICON(X) wxIcon(wxT(#X))
#elif defined(__WXPM__)
    // Load from a resource
    #define wxICON(X) wxIcon(wxT(#X))
#elif defined(__WXDFB__)
    // Initialize from an included XPM
    #define wxICON(X) wxIcon( X##_xpm )
#elif defined(__WXGTK__)
    // Initialize from an included XPM
    #define wxICON(X) wxIcon( X##_xpm )
#elif defined(__WXMAC__)
    // Initialize from an included XPM
    #define wxICON(X) wxIcon( X##_xpm )
#elif defined(__WXMOTIF__)
    // Initialize from an included XPM
    #define wxICON(X) wxIcon( X##_xpm )
#elif defined(__WXX11__)
    // Initialize from an included XPM
    #define wxICON(X) wxIcon( X##_xpm )
#else
    // This will usually mean something on any platform
    #define wxICON(X) wxIcon(wxT(#X))
#endif // platform

/* Another macro: this one is for portable creation of bitmaps. We assume that
   under Unix bitmaps live in XPMs and under Windows they're in ressources.
 */

#if defined(__WINDOWS__) || defined(__WXPM__)
    #define wxBITMAP(name) wxBitmap(wxT(#name), wxBITMAP_TYPE_BMP_RESOURCE)
#elif defined(__WXGTK__)   || \
      defined(__WXMOTIF__) || \
      defined(__WXX11__)   || \
      defined(__WXMAC__)   || \
      defined(__WXDFB__)   || \
      defined(__WXCOCOA__)
    // Initialize from an included XPM
    #define wxBITMAP(name) wxBitmap(name##_xpm)
#else // other platforms
    #define wxBITMAP(name) wxBitmap(name##_xpm, wxBITMAP_TYPE_XPM)
#endif // platform

// Macro for creating wxBitmap from in-memory PNG data.
//
// It reads PNG data from name_png static byte arrays that can be created using
// e.g. misc/scripts/png2c.py.
//
// This macro exists mostly as a helper for wxBITMAP_PNG() below but also
// because it's slightly more convenient to use than NewFromPNGData() directly.
#define wxBITMAP_PNG_FROM_DATA(name) \
    wxBitmap::NewFromPNGData(name##_png, WXSIZEOF(name##_png))

// Similar to wxBITMAP but used for the bitmaps in PNG format.
//
// Under Windows they should be embedded into the resource file using RT_RCDATA
// resource type and under OS X the PNG file with the specified name must be
// available in the resource subdirectory of the bundle. Elsewhere, this is
// exactly the same thing as wxBITMAP_PNG_FROM_DATA() described above.
#if defined(__WINDOWS__) || defined(__WXOSX__)
    #define wxBITMAP_PNG(name) wxBitmap(wxS(#name), wxBITMAP_TYPE_PNG_RESOURCE)
#else
    #define wxBITMAP_PNG(name) wxBITMAP_PNG_FROM_DATA(name)
#endif

// ===========================================================================
// classes
// ===========================================================================

// ---------------------------------------------------------------------------
// wxSize
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSize
{
public:
    // members are public for compatibility, don't use them directly.
    int x, y;

    // constructors
    wxSize() : x(0), y(0) { }
    wxSize(int xx, int yy) : x(xx), y(yy) { }

    // no copy ctor or assignment operator - the defaults are ok

    wxSize& operator+=(const wxSize& sz) { x += sz.x; y += sz.y; return *this; }
    wxSize& operator-=(const wxSize& sz) { x -= sz.x; y -= sz.y; return *this; }
    wxSize& operator/=(int i) { x /= i; y /= i; return *this; }
    wxSize& operator*=(int i) { x *= i; y *= i; return *this; }
    wxSize& operator/=(unsigned int i) { x /= i; y /= i; return *this; }
    wxSize& operator*=(unsigned int i) { x *= i; y *= i; return *this; }
    wxSize& operator/=(long i) { x /= i; y /= i; return *this; }
    wxSize& operator*=(long i) { x *= i; y *= i; return *this; }
    wxSize& operator/=(unsigned long i) { x /= i; y /= i; return *this; }
    wxSize& operator*=(unsigned long i) { x *= i; y *= i; return *this; }
    wxSize& operator/=(double i) { x = int(x/i); y = int(y/i); return *this; }
    wxSize& operator*=(double i) { x = int(x*i); y = int(y*i); return *this; }

    void IncTo(const wxSize& sz)
        { if ( sz.x > x ) x = sz.x; if ( sz.y > y ) y = sz.y; }
    void DecTo(const wxSize& sz)
        { if ( sz.x < x ) x = sz.x; if ( sz.y < y ) y = sz.y; }
    void DecToIfSpecified(const wxSize& sz)
    {
        if ( sz.x != wxDefaultCoord && sz.x < x )
            x = sz.x;
        if ( sz.y != wxDefaultCoord && sz.y < y )
            y = sz.y;
    }

    void IncBy(int dx, int dy) { x += dx; y += dy; }
    void IncBy(const wxPoint& pt);
    void IncBy(const wxSize& sz) { IncBy(sz.x, sz.y); }
    void IncBy(int d) { IncBy(d, d); }

    void DecBy(int dx, int dy) { IncBy(-dx, -dy); }
    void DecBy(const wxPoint& pt);
    void DecBy(const wxSize& sz) { DecBy(sz.x, sz.y); }
    void DecBy(int d) { DecBy(d, d); }


    wxSize& Scale(float xscale, float yscale)
        { x = (int)(x*xscale); y = (int)(y*yscale); return *this; }

    // accessors
    void Set(int xx, int yy) { x = xx; y = yy; }
    void SetWidth(int w) { x = w; }
    void SetHeight(int h) { y = h; }

    int GetWidth() const { return x; }
    int GetHeight() const { return y; }

    bool IsFullySpecified() const { return x != wxDefaultCoord && y != wxDefaultCoord; }

    // combine this size with the other one replacing the default (i.e. equal
    // to wxDefaultCoord) components of this object with those of the other
    void SetDefaults(const wxSize& size)
    {
        if ( x == wxDefaultCoord )
            x = size.x;
        if ( y == wxDefaultCoord )
            y = size.y;
    }

    // compatibility
    int GetX() const { return x; }
    int GetY() const { return y; }
};

inline bool operator==(const wxSize& s1, const wxSize& s2)
{
    return s1.x == s2.x && s1.y == s2.y;
}

inline bool operator!=(const wxSize& s1, const wxSize& s2)
{
    return s1.x != s2.x || s1.y != s2.y;
}

inline wxSize operator+(const wxSize& s1, const wxSize& s2)
{
    return wxSize(s1.x + s2.x, s1.y + s2.y);
}

inline wxSize operator-(const wxSize& s1, const wxSize& s2)
{
    return wxSize(s1.x - s2.x, s1.y - s2.y);
}

inline wxSize operator/(const wxSize& s, int i)
{
    return wxSize(s.x / i, s.y / i);
}

inline wxSize operator*(const wxSize& s, int i)
{
    return wxSize(s.x * i, s.y * i);
}

inline wxSize operator*(int i, const wxSize& s)
{
    return wxSize(s.x * i, s.y * i);
}

inline wxSize operator/(const wxSize& s, unsigned int i)
{
    return wxSize(s.x / i, s.y / i);
}

inline wxSize operator*(const wxSize& s, unsigned int i)
{
    return wxSize(s.x * i, s.y * i);
}

inline wxSize operator*(unsigned int i, const wxSize& s)
{
    return wxSize(s.x * i, s.y * i);
}

inline wxSize operator/(const wxSize& s, long i)
{
    return wxSize(s.x / i, s.y / i);
}

inline wxSize operator*(const wxSize& s, long i)
{
    return wxSize(int(s.x * i), int(s.y * i));
}

inline wxSize operator*(long i, const wxSize& s)
{
    return wxSize(int(s.x * i), int(s.y * i));
}

inline wxSize operator/(const wxSize& s, unsigned long i)
{
    return wxSize(int(s.x / i), int(s.y / i));
}

inline wxSize operator*(const wxSize& s, unsigned long i)
{
    return wxSize(int(s.x * i), int(s.y * i));
}

inline wxSize operator*(unsigned long i, const wxSize& s)
{
    return wxSize(int(s.x * i), int(s.y * i));
}

inline wxSize operator*(const wxSize& s, double i)
{
    return wxSize(int(s.x * i), int(s.y * i));
}

inline wxSize operator*(double i, const wxSize& s)
{
    return wxSize(int(s.x * i), int(s.y * i));
}



// ---------------------------------------------------------------------------
// Point classes: with real or integer coordinates
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRealPoint
{
public:
    double x;
    double y;

    wxRealPoint() : x(0.0), y(0.0) { }
    wxRealPoint(double xx, double yy) : x(xx), y(yy) { }
    wxRealPoint(const wxPoint& pt);

    // no copy ctor or assignment operator - the defaults are ok

    //assignment operators
    wxRealPoint& operator+=(const wxRealPoint& p) { x += p.x; y += p.y; return *this; }
    wxRealPoint& operator-=(const wxRealPoint& p) { x -= p.x; y -= p.y; return *this; }

    wxRealPoint& operator+=(const wxSize& s) { x += s.GetWidth(); y += s.GetHeight(); return *this; }
    wxRealPoint& operator-=(const wxSize& s) { x -= s.GetWidth(); y -= s.GetHeight(); return *this; }
};


inline bool operator==(const wxRealPoint& p1, const wxRealPoint& p2)
{
    return wxIsSameDouble(p1.x, p2.x) && wxIsSameDouble(p1.y, p2.y);
}

inline bool operator!=(const wxRealPoint& p1, const wxRealPoint& p2)
{
    return !(p1 == p2);
}

inline wxRealPoint operator+(const wxRealPoint& p1, const wxRealPoint& p2)
{
    return wxRealPoint(p1.x + p2.x, p1.y + p2.y);
}


inline wxRealPoint operator-(const wxRealPoint& p1, const wxRealPoint& p2)
{
    return wxRealPoint(p1.x - p2.x, p1.y - p2.y);
}


inline wxRealPoint operator/(const wxRealPoint& s, int i)
{
    return wxRealPoint(s.x / i, s.y / i);
}

inline wxRealPoint operator*(const wxRealPoint& s, int i)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator*(int i, const wxRealPoint& s)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator/(const wxRealPoint& s, unsigned int i)
{
    return wxRealPoint(s.x / i, s.y / i);
}

inline wxRealPoint operator*(const wxRealPoint& s, unsigned int i)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator*(unsigned int i, const wxRealPoint& s)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator/(const wxRealPoint& s, long i)
{
    return wxRealPoint(s.x / i, s.y / i);
}

inline wxRealPoint operator*(const wxRealPoint& s, long i)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator*(long i, const wxRealPoint& s)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator/(const wxRealPoint& s, unsigned long i)
{
    return wxRealPoint(s.x / i, s.y / i);
}

inline wxRealPoint operator*(const wxRealPoint& s, unsigned long i)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator*(unsigned long i, const wxRealPoint& s)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator*(const wxRealPoint& s, double i)
{
    return wxRealPoint(int(s.x * i), int(s.y * i));
}

inline wxRealPoint operator*(double i, const wxRealPoint& s)
{
    return wxRealPoint(int(s.x * i), int(s.y * i));
}


// ----------------------------------------------------------------------------
// wxPoint: 2D point with integer coordinates
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPoint
{
public:
    int x, y;

    wxPoint() : x(0), y(0) { }
    wxPoint(int xx, int yy) : x(xx), y(yy) { }
    wxPoint(const wxRealPoint& pt) : x(int(pt.x)), y(int(pt.y)) { }

    // no copy ctor or assignment operator - the defaults are ok

    //assignment operators
    wxPoint& operator+=(const wxPoint& p) { x += p.x; y += p.y; return *this; }
    wxPoint& operator-=(const wxPoint& p) { x -= p.x; y -= p.y; return *this; }

    wxPoint& operator+=(const wxSize& s) { x += s.GetWidth(); y += s.GetHeight(); return *this; }
    wxPoint& operator-=(const wxSize& s) { x -= s.GetWidth(); y -= s.GetHeight(); return *this; }

    // check if both components are set/initialized
    bool IsFullySpecified() const { return x != wxDefaultCoord && y != wxDefaultCoord; }

    // fill in the unset components with the values from the other point
    void SetDefaults(const wxPoint& pt)
    {
        if ( x == wxDefaultCoord )
            x = pt.x;
        if ( y == wxDefaultCoord )
            y = pt.y;
    }
};


// comparison
inline bool operator==(const wxPoint& p1, const wxPoint& p2)
{
    return p1.x == p2.x && p1.y == p2.y;
}

inline bool operator!=(const wxPoint& p1, const wxPoint& p2)
{
    return !(p1 == p2);
}


// arithmetic operations (component wise)
inline wxPoint operator+(const wxPoint& p1, const wxPoint& p2)
{
    return wxPoint(p1.x + p2.x, p1.y + p2.y);
}

inline wxPoint operator-(const wxPoint& p1, const wxPoint& p2)
{
    return wxPoint(p1.x - p2.x, p1.y - p2.y);
}

inline wxPoint operator+(const wxPoint& p, const wxSize& s)
{
    return wxPoint(p.x + s.x, p.y + s.y);
}

inline wxPoint operator-(const wxPoint& p, const wxSize& s)
{
    return wxPoint(p.x - s.x, p.y - s.y);
}

inline wxPoint operator+(const wxSize& s, const wxPoint& p)
{
    return wxPoint(p.x + s.x, p.y + s.y);
}

inline wxPoint operator-(const wxSize& s, const wxPoint& p)
{
    return wxPoint(s.x - p.x, s.y - p.y);
}

inline wxPoint operator-(const wxPoint& p)
{
    return wxPoint(-p.x, -p.y);
}

inline wxPoint operator/(const wxPoint& s, int i)
{
    return wxPoint(s.x / i, s.y / i);
}

inline wxPoint operator*(const wxPoint& s, int i)
{
    return wxPoint(s.x * i, s.y * i);
}

inline wxPoint operator*(int i, const wxPoint& s)
{
    return wxPoint(s.x * i, s.y * i);
}

inline wxPoint operator/(const wxPoint& s, unsigned int i)
{
    return wxPoint(s.x / i, s.y / i);
}

inline wxPoint operator*(const wxPoint& s, unsigned int i)
{
    return wxPoint(s.x * i, s.y * i);
}

inline wxPoint operator*(unsigned int i, const wxPoint& s)
{
    return wxPoint(s.x * i, s.y * i);
}

inline wxPoint operator/(const wxPoint& s, long i)
{
    return wxPoint(s.x / i, s.y / i);
}

inline wxPoint operator*(const wxPoint& s, long i)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

inline wxPoint operator*(long i, const wxPoint& s)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

inline wxPoint operator/(const wxPoint& s, unsigned long i)
{
    return wxPoint(s.x / i, s.y / i);
}

inline wxPoint operator*(const wxPoint& s, unsigned long i)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

inline wxPoint operator*(unsigned long i, const wxPoint& s)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

inline wxPoint operator*(const wxPoint& s, double i)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

inline wxPoint operator*(double i, const wxPoint& s)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

WX_DECLARE_LIST_WITH_DECL(wxPoint, wxPointList, class WXDLLIMPEXP_CORE);

// ---------------------------------------------------------------------------
// wxRect
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRect
{
public:
    wxRect()
        : x(0), y(0), width(0), height(0)
        { }
    wxRect(int xx, int yy, int ww, int hh)
        : x(xx), y(yy), width(ww), height(hh)
        { }
    wxRect(const wxPoint& topLeft, const wxPoint& bottomRight);
    wxRect(const wxPoint& pt, const wxSize& size)
        : x(pt.x), y(pt.y), width(size.x), height(size.y)
        { }
    wxRect(const wxSize& size)
        : x(0), y(0), width(size.x), height(size.y)
        { }

    // default copy ctor and assignment operators ok

    int GetX() const { return x; }
    void SetX(int xx) { x = xx; }

    int GetY() const { return y; }
    void SetY(int yy) { y = yy; }

    int GetWidth() const { return width; }
    void SetWidth(int w) { width = w; }

    int GetHeight() const { return height; }
    void SetHeight(int h) { height = h; }

    wxPoint GetPosition() const { return wxPoint(x, y); }
    void SetPosition( const wxPoint &p ) { x = p.x; y = p.y; }

    wxSize GetSize() const { return wxSize(width, height); }
    void SetSize( const wxSize &s ) { width = s.GetWidth(); height = s.GetHeight(); }

    bool IsEmpty() const { return (width <= 0) || (height <= 0); }

    int GetLeft()   const { return x; }
    int GetTop()    const { return y; }
    int GetBottom() const { return y + height - 1; }
    int GetRight()  const { return x + width - 1; }

    void SetLeft(int left) { x = left; }
    void SetRight(int right) { width = right - x + 1; }
    void SetTop(int top) { y = top; }
    void SetBottom(int bottom) { height = bottom - y + 1; }

    wxPoint GetTopLeft() const { return GetPosition(); }
    wxPoint GetLeftTop() const { return GetTopLeft(); }
    void SetTopLeft(const wxPoint &p) { SetPosition(p); }
    void SetLeftTop(const wxPoint &p) { SetTopLeft(p); }

    wxPoint GetBottomRight() const { return wxPoint(GetRight(), GetBottom()); }
    wxPoint GetRightBottom() const { return GetBottomRight(); }
    void SetBottomRight(const wxPoint &p) { SetRight(p.x); SetBottom(p.y); }
    void SetRightBottom(const wxPoint &p) { SetBottomRight(p); }

    wxPoint GetTopRight() const { return wxPoint(GetRight(), GetTop()); }
    wxPoint GetRightTop() const { return GetTopRight(); }
    void SetTopRight(const wxPoint &p) { SetRight(p.x); SetTop(p.y); }
    void SetRightTop(const wxPoint &p) { SetTopRight(p); }

    wxPoint GetBottomLeft() const { return wxPoint(GetLeft(), GetBottom()); }
    wxPoint GetLeftBottom() const { return GetBottomLeft(); }
    void SetBottomLeft(const wxPoint &p) { SetLeft(p.x); SetBottom(p.y); }
    void SetLeftBottom(const wxPoint &p) { SetBottomLeft(p); }

    // operations with rect
    wxRect& Inflate(wxCoord dx, wxCoord dy);
    wxRect& Inflate(const wxSize& d) { return Inflate(d.x, d.y); }
    wxRect& Inflate(wxCoord d) { return Inflate(d, d); }
    wxRect Inflate(wxCoord dx, wxCoord dy) const
    {
        wxRect r = *this;
        r.Inflate(dx, dy);
        return r;
    }

    wxRect& Deflate(wxCoord dx, wxCoord dy) { return Inflate(-dx, -dy); }
    wxRect& Deflate(const wxSize& d) { return Inflate(-d.x, -d.y); }
    wxRect& Deflate(wxCoord d) { return Inflate(-d); }
    wxRect Deflate(wxCoord dx, wxCoord dy) const
    {
        wxRect r = *this;
        r.Deflate(dx, dy);
        return r;
    }

    void Offset(wxCoord dx, wxCoord dy) { x += dx; y += dy; }
    void Offset(const wxPoint& pt) { Offset(pt.x, pt.y); }

    wxRect& Intersect(const wxRect& rect);
    wxRect Intersect(const wxRect& rect) const
    {
        wxRect r = *this;
        r.Intersect(rect);
        return r;
    }

    wxRect& Union(const wxRect& rect);
    wxRect Union(const wxRect& rect) const
    {
        wxRect r = *this;
        r.Union(rect);
        return r;
    }

    // return true if the point is (not strcitly) inside the rect
    bool Contains(int x, int y) const;
    bool Contains(const wxPoint& pt) const { return Contains(pt.x, pt.y); }
    // return true if the rectangle 'rect' is (not strictly) inside this rect
    bool Contains(const wxRect& rect) const;

#if WXWIN_COMPATIBILITY_2_6
    // use Contains() instead
    wxDEPRECATED( bool Inside(int x, int y) const );
    wxDEPRECATED( bool Inside(const wxPoint& pt) const );
    wxDEPRECATED( bool Inside(const wxRect& rect) const );
#endif // WXWIN_COMPATIBILITY_2_6

    // return true if the rectangles have a non empty intersection
    bool Intersects(const wxRect& rect) const;

    // like Union() but don't ignore empty rectangles
    wxRect& operator+=(const wxRect& rect);

    // intersections of two rectrangles not testing for empty rectangles
    wxRect& operator*=(const wxRect& rect);

    // centre this rectangle in the given (usually, but not necessarily,
    // larger) one
    wxRect CentreIn(const wxRect& r, int dir = wxBOTH) const
    {
        return wxRect(dir & wxHORIZONTAL ? r.x + (r.width - width)/2 : x,
                      dir & wxVERTICAL ? r.y + (r.height - height)/2 : y,
                      width, height);
    }

    wxRect CenterIn(const wxRect& r, int dir = wxBOTH) const
    {
        return CentreIn(r, dir);
    }

public:
    int x, y, width, height;
};


// compare rectangles
inline bool operator==(const wxRect& r1, const wxRect& r2)
{
    return (r1.x == r2.x) && (r1.y == r2.y) &&
           (r1.width == r2.width) && (r1.height == r2.height);
}

inline bool operator!=(const wxRect& r1, const wxRect& r2)
{
    return !(r1 == r2);
}

// like Union() but don't treat empty rectangles specially
WXDLLIMPEXP_CORE wxRect operator+(const wxRect& r1, const wxRect& r2);

// intersections of two rectangles
WXDLLIMPEXP_CORE wxRect operator*(const wxRect& r1, const wxRect& r2);




#if WXWIN_COMPATIBILITY_2_6
inline bool wxRect::Inside(int cx, int cy) const { return Contains(cx, cy); }
inline bool wxRect::Inside(const wxPoint& pt) const { return Contains(pt); }
inline bool wxRect::Inside(const wxRect& rect) const { return Contains(rect); }
#endif // WXWIN_COMPATIBILITY_2_6


// define functions which couldn't be defined above because of declarations
// order
inline void wxSize::IncBy(const wxPoint& pt) { IncBy(pt.x, pt.y); }
inline void wxSize::DecBy(const wxPoint& pt) { DecBy(pt.x, pt.y); }

// ---------------------------------------------------------------------------
// Management of pens, brushes and fonts
// ---------------------------------------------------------------------------

typedef wxInt8 wxDash;

class WXDLLIMPEXP_CORE wxGDIObjListBase {
public:
    wxGDIObjListBase();
    ~wxGDIObjListBase();

protected:
    wxList list;
};

WX_DECLARE_STRING_HASH_MAP(wxColour*, wxStringToColourHashMap);

class WXDLLIMPEXP_CORE wxColourDatabase
{
public:
    wxColourDatabase();
    ~wxColourDatabase();

    // find colour by name or name for the given colour
    wxColour Find(const wxString& name) const;
    wxString FindName(const wxColour& colour) const;

    // add a new colour to the database
    void AddColour(const wxString& name, const wxColour& colour);

#if WXWIN_COMPATIBILITY_2_6
    // deprecated, use Find() instead
    wxDEPRECATED( wxColour *FindColour(const wxString& name) );
#endif // WXWIN_COMPATIBILITY_2_6


#ifdef __WXPM__
    // PM keeps its own type of colour table
    long*                           m_palTable;
    size_t                          m_nSize;
#endif

private:
    // load the database with the built in colour values when called for the
    // first time, do nothing after this
    void Initialize();

    wxStringToColourHashMap *m_map;
};

class WXDLLIMPEXP_CORE wxResourceCache: public wxList
{
public:
    wxResourceCache() { }
#if !wxUSE_STD_CONTAINERS
    wxResourceCache(const unsigned int keyType) : wxList(keyType) { }
#endif
    virtual ~wxResourceCache();
};

// ---------------------------------------------------------------------------
// global variables
// ---------------------------------------------------------------------------


/* Stock objects

  wxStockGDI creates the stock GDI objects on demand.  Pointers to the
  created objects are stored in the ms_stockObject array, which is indexed
  by the Item enum values.  Platorm-specific fonts can be created by
  implementing a derived class with an override for the GetFont function.
  wxStockGDI operates as a singleton, accessed through the ms_instance
  pointer.  By default this pointer is set to an instance of wxStockGDI.
  A derived class must arrange to set this pointer to an instance of itself.
*/
class WXDLLIMPEXP_CORE wxStockGDI
{
public:
    enum Item {
        BRUSH_BLACK,
        BRUSH_BLUE,
        BRUSH_CYAN,
        BRUSH_GREEN,
        BRUSH_YELLOW,
        BRUSH_GREY,
        BRUSH_LIGHTGREY,
        BRUSH_MEDIUMGREY,
        BRUSH_RED,
        BRUSH_TRANSPARENT,
        BRUSH_WHITE,
        COLOUR_BLACK,
        COLOUR_BLUE,
        COLOUR_CYAN,
        COLOUR_GREEN,
        COLOUR_YELLOW,
        COLOUR_LIGHTGREY,
        COLOUR_RED,
        COLOUR_WHITE,
        CURSOR_CROSS,
        CURSOR_HOURGLASS,
        CURSOR_STANDARD,
        FONT_ITALIC,
        FONT_NORMAL,
        FONT_SMALL,
        FONT_SWISS,
        PEN_BLACK,
        PEN_BLACKDASHED,
        PEN_BLUE,
        PEN_CYAN,
        PEN_GREEN,
        PEN_YELLOW,
        PEN_GREY,
        PEN_LIGHTGREY,
        PEN_MEDIUMGREY,
        PEN_RED,
        PEN_TRANSPARENT,
        PEN_WHITE,
        ITEMCOUNT
    };

    wxStockGDI();
    virtual ~wxStockGDI();
    static void DeleteAll();

    static wxStockGDI& instance() { return *ms_instance; }

    static const wxBrush* GetBrush(Item item);
    static const wxColour* GetColour(Item item);
    static const wxCursor* GetCursor(Item item);
    // Can be overridden by platform-specific derived classes
    virtual const wxFont* GetFont(Item item);
    static const wxPen* GetPen(Item item);

protected:
    static wxStockGDI* ms_instance;

    static wxObject* ms_stockObject[ITEMCOUNT];

    wxDECLARE_NO_COPY_CLASS(wxStockGDI);
};

#define wxITALIC_FONT  wxStockGDI::instance().GetFont(wxStockGDI::FONT_ITALIC)
#define wxNORMAL_FONT  wxStockGDI::instance().GetFont(wxStockGDI::FONT_NORMAL)
#define wxSMALL_FONT   wxStockGDI::instance().GetFont(wxStockGDI::FONT_SMALL)
#define wxSWISS_FONT   wxStockGDI::instance().GetFont(wxStockGDI::FONT_SWISS)

#define wxBLACK_DASHED_PEN  wxStockGDI::GetPen(wxStockGDI::PEN_BLACKDASHED)
#define wxBLACK_PEN         wxStockGDI::GetPen(wxStockGDI::PEN_BLACK)
#define wxBLUE_PEN          wxStockGDI::GetPen(wxStockGDI::PEN_BLUE)
#define wxCYAN_PEN          wxStockGDI::GetPen(wxStockGDI::PEN_CYAN)
#define wxGREEN_PEN         wxStockGDI::GetPen(wxStockGDI::PEN_GREEN)
#define wxYELLOW_PEN        wxStockGDI::GetPen(wxStockGDI::PEN_YELLOW)
#define wxGREY_PEN          wxStockGDI::GetPen(wxStockGDI::PEN_GREY)
#define wxLIGHT_GREY_PEN    wxStockGDI::GetPen(wxStockGDI::PEN_LIGHTGREY)
#define wxMEDIUM_GREY_PEN   wxStockGDI::GetPen(wxStockGDI::PEN_MEDIUMGREY)
#define wxRED_PEN           wxStockGDI::GetPen(wxStockGDI::PEN_RED)
#define wxTRANSPARENT_PEN   wxStockGDI::GetPen(wxStockGDI::PEN_TRANSPARENT)
#define wxWHITE_PEN         wxStockGDI::GetPen(wxStockGDI::PEN_WHITE)

#define wxBLACK_BRUSH        wxStockGDI::GetBrush(wxStockGDI::BRUSH_BLACK)
#define wxBLUE_BRUSH         wxStockGDI::GetBrush(wxStockGDI::BRUSH_BLUE)
#define wxCYAN_BRUSH         wxStockGDI::GetBrush(wxStockGDI::BRUSH_CYAN)
#define wxGREEN_BRUSH        wxStockGDI::GetBrush(wxStockGDI::BRUSH_GREEN)
#define wxYELLOW_BRUSH       wxStockGDI::GetBrush(wxStockGDI::BRUSH_YELLOW)
#define wxGREY_BRUSH         wxStockGDI::GetBrush(wxStockGDI::BRUSH_GREY)
#define wxLIGHT_GREY_BRUSH   wxStockGDI::GetBrush(wxStockGDI::BRUSH_LIGHTGREY)
#define wxMEDIUM_GREY_BRUSH  wxStockGDI::GetBrush(wxStockGDI::BRUSH_MEDIUMGREY)
#define wxRED_BRUSH          wxStockGDI::GetBrush(wxStockGDI::BRUSH_RED)
#define wxTRANSPARENT_BRUSH  wxStockGDI::GetBrush(wxStockGDI::BRUSH_TRANSPARENT)
#define wxWHITE_BRUSH        wxStockGDI::GetBrush(wxStockGDI::BRUSH_WHITE)

#define wxBLACK       wxStockGDI::GetColour(wxStockGDI::COLOUR_BLACK)
#define wxBLUE        wxStockGDI::GetColour(wxStockGDI::COLOUR_BLUE)
#define wxCYAN        wxStockGDI::GetColour(wxStockGDI::COLOUR_CYAN)
#define wxGREEN       wxStockGDI::GetColour(wxStockGDI::COLOUR_GREEN)
#define wxYELLOW      wxStockGDI::GetColour(wxStockGDI::COLOUR_YELLOW)
#define wxLIGHT_GREY  wxStockGDI::GetColour(wxStockGDI::COLOUR_LIGHTGREY)
#define wxRED         wxStockGDI::GetColour(wxStockGDI::COLOUR_RED)
#define wxWHITE       wxStockGDI::GetColour(wxStockGDI::COLOUR_WHITE)

#define wxCROSS_CURSOR      wxStockGDI::GetCursor(wxStockGDI::CURSOR_CROSS)
#define wxHOURGLASS_CURSOR  wxStockGDI::GetCursor(wxStockGDI::CURSOR_HOURGLASS)
#define wxSTANDARD_CURSOR   wxStockGDI::GetCursor(wxStockGDI::CURSOR_STANDARD)

// 'Null' objects
extern WXDLLIMPEXP_DATA_CORE(wxBitmap)     wxNullBitmap;
extern WXDLLIMPEXP_DATA_CORE(wxIcon)       wxNullIcon;
extern WXDLLIMPEXP_DATA_CORE(wxCursor)     wxNullCursor;
extern WXDLLIMPEXP_DATA_CORE(wxPen)        wxNullPen;
extern WXDLLIMPEXP_DATA_CORE(wxBrush)      wxNullBrush;
extern WXDLLIMPEXP_DATA_CORE(wxPalette)    wxNullPalette;
extern WXDLLIMPEXP_DATA_CORE(wxFont)       wxNullFont;
extern WXDLLIMPEXP_DATA_CORE(wxColour)     wxNullColour;
extern WXDLLIMPEXP_DATA_CORE(wxIconBundle) wxNullIconBundle;

extern WXDLLIMPEXP_DATA_CORE(wxColourDatabase*)  wxTheColourDatabase;

extern WXDLLIMPEXP_DATA_CORE(const char) wxPanelNameStr[];

extern WXDLLIMPEXP_DATA_CORE(const wxSize) wxDefaultSize;
extern WXDLLIMPEXP_DATA_CORE(const wxPoint) wxDefaultPosition;

// ---------------------------------------------------------------------------
// global functions
// ---------------------------------------------------------------------------

// resource management
extern void WXDLLIMPEXP_CORE wxInitializeStockLists();
extern void WXDLLIMPEXP_CORE wxDeleteStockLists();

// is the display colour (or monochrome)?
extern bool WXDLLIMPEXP_CORE wxColourDisplay();

// Returns depth of screen
extern int WXDLLIMPEXP_CORE wxDisplayDepth();
#define wxGetDisplayDepth wxDisplayDepth

// get the display size
extern void WXDLLIMPEXP_CORE wxDisplaySize(int *width, int *height);
extern wxSize WXDLLIMPEXP_CORE wxGetDisplaySize();
extern void WXDLLIMPEXP_CORE wxDisplaySizeMM(int *width, int *height);
extern wxSize WXDLLIMPEXP_CORE wxGetDisplaySizeMM();
extern wxSize WXDLLIMPEXP_CORE wxGetDisplayPPI();

// Get position and size of the display workarea
extern void WXDLLIMPEXP_CORE wxClientDisplayRect(int *x, int *y, int *width, int *height);
extern wxRect WXDLLIMPEXP_CORE wxGetClientDisplayRect();

// set global cursor
extern void WXDLLIMPEXP_CORE wxSetCursor(const wxCursor& cursor);

#endif
    // _WX_GDICMNH__
