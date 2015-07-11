/////////////////////////////////////////////////////////////////////////////
// Name:        wx/brush.h
// Purpose:     Includes platform-specific wxBrush file
// Author:      Julian Smart
// Modified by:
// Created:
// Copyright:   Julian Smart
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BRUSH_H_BASE_
#define _WX_BRUSH_H_BASE_

#include "wx/defs.h"
#include "wx/object.h"
#include "wx/gdiobj.h"
#include "wx/gdicmn.h"      // for wxGDIObjListBase

// NOTE: these values cannot be combined together!
enum wxBrushStyle
{
    wxBRUSHSTYLE_INVALID = -1,

    wxBRUSHSTYLE_SOLID = wxSOLID,
    wxBRUSHSTYLE_TRANSPARENT = wxTRANSPARENT,
    wxBRUSHSTYLE_STIPPLE_MASK_OPAQUE = wxSTIPPLE_MASK_OPAQUE,
    wxBRUSHSTYLE_STIPPLE_MASK = wxSTIPPLE_MASK,
    wxBRUSHSTYLE_STIPPLE = wxSTIPPLE,
    wxBRUSHSTYLE_BDIAGONAL_HATCH = wxHATCHSTYLE_BDIAGONAL,
    wxBRUSHSTYLE_CROSSDIAG_HATCH = wxHATCHSTYLE_CROSSDIAG,
    wxBRUSHSTYLE_FDIAGONAL_HATCH = wxHATCHSTYLE_FDIAGONAL,
    wxBRUSHSTYLE_CROSS_HATCH = wxHATCHSTYLE_CROSS,
    wxBRUSHSTYLE_HORIZONTAL_HATCH = wxHATCHSTYLE_HORIZONTAL,
    wxBRUSHSTYLE_VERTICAL_HATCH = wxHATCHSTYLE_VERTICAL,
    wxBRUSHSTYLE_FIRST_HATCH = wxHATCHSTYLE_FIRST,
    wxBRUSHSTYLE_LAST_HATCH = wxHATCHSTYLE_LAST
};


// wxBrushBase
class WXDLLIMPEXP_CORE wxBrushBase: public wxGDIObject
{
public:
    virtual ~wxBrushBase() { }

    virtual void SetColour(const wxColour& col) = 0;
    virtual void SetColour(unsigned char r, unsigned char g, unsigned char b) = 0;
    virtual void SetStyle(wxBrushStyle style) = 0;
    virtual void SetStipple(const wxBitmap& stipple) = 0;

    virtual wxColour GetColour() const = 0;
    virtual wxBrushStyle GetStyle() const = 0;
    virtual wxBitmap *GetStipple() const = 0;

    virtual bool IsHatch() const
        { return (GetStyle()>=wxBRUSHSTYLE_FIRST_HATCH) && (GetStyle()<=wxBRUSHSTYLE_LAST_HATCH); }

    // Convenient helpers for testing whether the brush is a transparent one:
    // unlike GetStyle() == wxBRUSHSTYLE_TRANSPARENT, they work correctly even
    // if the brush is invalid (they both return false in this case).
    bool IsTransparent() const
    {
        return IsOk() && GetStyle() == wxBRUSHSTYLE_TRANSPARENT;
    }

    bool IsNonTransparent() const
    {
        return IsOk() && GetStyle() != wxBRUSHSTYLE_TRANSPARENT;
    }
};

#if defined(__WXMSW__)
    #include "wx/msw/brush.h"
#elif defined(__WXMOTIF__) || defined(__WXX11__)
    #include "wx/x11/brush.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/brush.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/brush.h"
#elif defined(__WXDFB__)
    #include "wx/dfb/brush.h"
#elif defined(__WXMAC__)
    #include "wx/osx/brush.h"
#elif defined(__WXCOCOA__)
    #include "wx/cocoa/brush.h"
#elif defined(__WXPM__)
    #include "wx/os2/brush.h"
#endif

class WXDLLIMPEXP_CORE wxBrushList: public wxGDIObjListBase
{
public:
    wxBrush *FindOrCreateBrush(const wxColour& colour,
                               wxBrushStyle style = wxBRUSHSTYLE_SOLID);

#if FUTURE_WXWIN_COMPATIBILITY_3_0
    wxBrush *FindOrCreateBrush(const wxColour& colour, int style)
        { return FindOrCreateBrush(colour, (wxBrushStyle)style); }
#endif

#if WXWIN_COMPATIBILITY_2_6
    wxDEPRECATED( void AddBrush(wxBrush*) );
    wxDEPRECATED( void RemoveBrush(wxBrush*) );
#endif
};

extern WXDLLIMPEXP_DATA_CORE(wxBrushList*)   wxTheBrushList;

// provide comparison operators to allow code such as
//
//      if ( brush.GetStyle() == wxTRANSPARENT )
//
// to compile without warnings which it would otherwise provoke from some
// compilers as it compares elements of different enums
#if FUTURE_WXWIN_COMPATIBILITY_3_0

// Unfortunately some compilers have ambiguity issues when enum comparisons are
// overloaded so we have to disable the overloads in this case, see
// wxCOMPILER_NO_OVERLOAD_ON_ENUM definition in wx/platform.h for more details.
#ifndef wxCOMPILER_NO_OVERLOAD_ON_ENUM

inline bool operator==(wxBrushStyle s, wxDeprecatedGUIConstants t)
{
    return static_cast<int>(s) == static_cast<int>(t);
}

inline bool operator!=(wxBrushStyle s, wxDeprecatedGUIConstants t)
{
    return !(s == t);
}

#endif // wxCOMPILER_NO_OVERLOAD_ON_ENUM

#endif // FUTURE_WXWIN_COMPATIBILITY_3_0

#endif // _WX_BRUSH_H_BASE_
