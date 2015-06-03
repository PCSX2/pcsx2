/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/region.h
// Purpose:     wxRegion class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) 1997-2002 wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_REGION_H_
#define _WX_MSW_REGION_H_

class WXDLLIMPEXP_CORE wxRegion : public wxRegionWithCombine
{
public:
    wxRegion();
    wxRegion(wxCoord x, wxCoord y, wxCoord w, wxCoord h);
    wxRegion(const wxPoint& topLeft, const wxPoint& bottomRight);
    wxRegion(const wxRect& rect);
    wxRegion(WXHRGN hRegion); // Hangs on to this region
    wxRegion(size_t n, const wxPoint *points, wxPolygonFillMode fillStyle = wxODDEVEN_RULE );
#if wxUSE_IMAGE
    wxRegion( const wxBitmap& bmp)
    {
        Union(bmp);
    }
    wxRegion( const wxBitmap& bmp,
              const wxColour& transColour, int tolerance = 0)
    {
        Union(bmp, transColour, tolerance);
    }
#endif // wxUSE_IMAGE

    virtual ~wxRegion();

    // wxRegionBase methods
    virtual void Clear();
    virtual bool IsEmpty() const;

    // Get internal region handle
    WXHRGN GetHRGN() const;

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

    virtual bool DoIsEqual(const wxRegion& region) const;
    virtual bool DoGetBox(wxCoord& x, wxCoord& y, wxCoord& w, wxCoord& h) const;
    virtual wxRegionContain DoContainsPoint(wxCoord x, wxCoord y) const;
    virtual wxRegionContain DoContainsRect(const wxRect& rect) const;

    virtual bool DoOffset(wxCoord x, wxCoord y);
    virtual bool DoCombine(const wxRegion& region, wxRegionOp op);

    friend class WXDLLIMPEXP_FWD_CORE wxRegionIterator;

    wxDECLARE_DYNAMIC_CLASS(wxRegion);
};

class WXDLLIMPEXP_CORE wxRegionIterator : public wxObject
{
public:
    wxRegionIterator() { Init(); }
    wxRegionIterator(const wxRegion& region);
    wxRegionIterator(const wxRegionIterator& ri) : wxObject(ri) { Init(); *this = ri; }

    wxRegionIterator& operator=(const wxRegionIterator& ri);

    virtual ~wxRegionIterator();

    void Reset() { m_current = 0; }
    void Reset(const wxRegion& region);

    bool HaveRects() const { return (m_current < m_numRects); }

    operator bool () const { return HaveRects(); }

    wxRegionIterator& operator++();
    wxRegionIterator operator++(int);

    wxCoord GetX() const;
    wxCoord GetY() const;
    wxCoord GetW() const;
    wxCoord GetWidth() const { return GetW(); }
    wxCoord GetH() const;
    wxCoord GetHeight() const { return GetH(); }

    wxRect GetRect() const { return wxRect(GetX(), GetY(), GetW(), GetH()); }

private:
    // common part of all ctors
    void Init();

    long     m_current;
    long     m_numRects;
    wxRegion m_region;
    wxRect*  m_rects;

    wxDECLARE_DYNAMIC_CLASS(wxRegionIterator);
};

#endif // _WX_MSW_REGION_H_
