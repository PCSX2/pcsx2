/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/region.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_REGION_H_
#define _WX_GTK_REGION_H_

#include "wx/list.h"

// ----------------------------------------------------------------------------
// wxRegion
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRegion : public wxRegionBase
{
public:
    wxRegion() { }

    wxRegion( wxCoord x, wxCoord y, wxCoord w, wxCoord h )
    {
        InitRect(x, y, w, h);
    }

    wxRegion( const wxPoint& topLeft, const wxPoint& bottomRight )
    {
        InitRect(topLeft.x, topLeft.y,
                 bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
    }

    wxRegion( const wxRect& rect )
    {
        InitRect(rect.x, rect.y, rect.width, rect.height);
    }

    wxRegion( size_t n, const wxPoint *points, wxPolygonFillMode fillStyle = wxODDEVEN_RULE );

    wxRegion( const wxBitmap& bmp)
    {
        Union(bmp);
    }
    wxRegion( const wxBitmap& bmp,
              const wxColour& transColour, int tolerance = 0)
    {
        Union(bmp, transColour, tolerance);
    }

    virtual ~wxRegion();

    // wxRegionBase methods
    virtual void Clear();
    virtual bool IsEmpty() const;

public:
    // Init with GdkRegion, set ref count to 2 so that
    // the C++ class will not destroy the region!
    wxRegion( GdkRegion *region );

    GdkRegion *GetRegion() const;

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

    // wxRegionBase pure virtuals
    virtual bool DoIsEqual(const wxRegion& region) const;
    virtual bool DoGetBox(wxCoord& x, wxCoord& y, wxCoord& w, wxCoord& h) const;
    virtual wxRegionContain DoContainsPoint(wxCoord x, wxCoord y) const;
    virtual wxRegionContain DoContainsRect(const wxRect& rect) const;

    virtual bool DoOffset(wxCoord x, wxCoord y);
    virtual bool DoUnionWithRect(const wxRect& rect);
    virtual bool DoUnionWithRegion(const wxRegion& region);
    virtual bool DoIntersect(const wxRegion& region);
    virtual bool DoSubtract(const wxRegion& region);
    virtual bool DoXor(const wxRegion& region);

    // common part of ctors for a rectangle region
    void InitRect(wxCoord x, wxCoord y, wxCoord w, wxCoord h);

private:
    DECLARE_DYNAMIC_CLASS(wxRegion)
};

// ----------------------------------------------------------------------------
// wxRegionIterator: decomposes a region into rectangles
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRegionIterator: public wxObject
{
public:
    wxRegionIterator();
    wxRegionIterator(const wxRegion& region);

    void Reset() { m_current = 0u; }
    void Reset(const wxRegion& region);

    bool HaveRects() const;
    operator bool () const { return HaveRects(); }

    wxRegionIterator& operator ++ ();
    wxRegionIterator operator ++ (int);

    wxCoord GetX() const;
    wxCoord GetY() const;
    wxCoord GetW() const;
    wxCoord GetWidth() const { return GetW(); }
    wxCoord GetH() const;
    wxCoord GetHeight() const { return GetH(); }
    wxRect GetRect() const;

private:
    size_t   m_current;
    wxRegion m_region;

private:
    DECLARE_DYNAMIC_CLASS(wxRegionIterator)
};


#endif
        // _WX_GTK_REGION_H_
