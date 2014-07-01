/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/carbon/region.h
// Purpose:     wxRegion class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MAC_CARBON_REGION_H_
#define _WX_MAC_CARBON_REGION_H_

#include "wx/list.h"

class WXDLLIMPEXP_CORE wxRegion : public wxRegionWithCombine
{
public:
    wxRegion() { }
    wxRegion(long x, long y, long w, long h);
    wxRegion(const wxPoint& topLeft, const wxPoint& bottomRight);
    wxRegion(const wxRect& rect);
    wxRegion( WXHRGN hRegion );
    wxRegion(size_t n, const wxPoint *points, wxPolygonFillMode fillStyle = wxODDEVEN_RULE );
#if wxUSE_IMAGE
    wxRegion(const wxBitmap& bmp)
    {
        Union(bmp);
    }
    wxRegion(const wxBitmap& bmp,
             const wxColour& transColour, int tolerance = 0)
    {
        Union(bmp, transColour, tolerance);
    }
#endif

    virtual ~wxRegion();

    // wxRegionBase methods
    virtual void Clear();
    virtual bool IsEmpty() const;

    // Internal
    WXHRGN GetWXHRGN() const ;

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

    virtual bool DoIsEqual(const wxRegion& region) const;
    virtual bool DoGetBox(wxCoord& x, wxCoord& y, wxCoord& w, wxCoord& h) const;
    virtual wxRegionContain DoContainsPoint(wxCoord x, wxCoord y) const;
    virtual wxRegionContain DoContainsRect(const wxRect& rect) const;

    virtual bool DoOffset(wxCoord x, wxCoord y);
    virtual bool DoCombine(const wxRegion& region, wxRegionOp op);
    virtual bool DoUnionWithRect(const wxRect& rect);

private:
    DECLARE_DYNAMIC_CLASS(wxRegion)
    friend class WXDLLIMPEXP_FWD_CORE wxRegionIterator;
};

class WXDLLIMPEXP_CORE wxRegionIterator : public wxObject
{
public:
    wxRegionIterator();
    wxRegionIterator(const wxRegion& region);
    wxRegionIterator(const wxRegionIterator& iterator);
    virtual ~wxRegionIterator();

    wxRegionIterator& operator=(const wxRegionIterator& iterator);

    void Reset() { m_current = 0; }
    void Reset(const wxRegion& region);

    operator bool () const { return m_current < m_numRects; }
    bool HaveRects() const { return m_current < m_numRects; }

    wxRegionIterator& operator++();
    wxRegionIterator operator++(int);

    long GetX() const;
    long GetY() const;
    long GetW() const;
    long GetWidth() const { return GetW(); }
    long GetH() const;
    long GetHeight() const { return GetH(); }
    wxRect GetRect() const { return wxRect((int)GetX(), (int)GetY(), (int)GetWidth(), (int)GetHeight()); }

private:
    void SetRects(long numRects, wxRect *rects);

    long     m_current;
    long     m_numRects;
    wxRegion m_region;
    wxRect*  m_rects;

    DECLARE_DYNAMIC_CLASS(wxRegionIterator)
};

#endif // _WX_MAC_CARBON_REGION_H_
