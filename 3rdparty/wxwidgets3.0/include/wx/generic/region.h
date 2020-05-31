/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/region.h
// Purpose:     generic wxRegion class
// Author:      David Elliott
// Modified by:
// Created:     2004/04/12
// Copyright:   (c) 2004 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_REGION_H__
#define _WX_GENERIC_REGION_H__

class WXDLLIMPEXP_CORE wxRegionGeneric : public wxRegionBase
{
public:
    wxRegionGeneric(wxCoord x, wxCoord y, wxCoord w, wxCoord h);
    wxRegionGeneric(const wxPoint& topLeft, const wxPoint& bottomRight);
    wxRegionGeneric(const wxRect& rect);
    wxRegionGeneric(size_t n, const wxPoint *points, wxPolygonFillMode fillStyle = wxODDEVEN_RULE);
    wxRegionGeneric(const wxBitmap& bmp);
    wxRegionGeneric(const wxBitmap& bmp, const wxColour& transp, int tolerance = 0);
    wxRegionGeneric();
    virtual ~wxRegionGeneric();

    // wxRegionBase pure virtuals
    virtual void Clear();
    virtual bool IsEmpty() const;

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

    friend class WXDLLIMPEXP_FWD_CORE wxRegionIteratorGeneric;
};

class WXDLLIMPEXP_CORE wxRegionIteratorGeneric : public wxObject
{
public:
    wxRegionIteratorGeneric();
    wxRegionIteratorGeneric(const wxRegionGeneric& region);
    wxRegionIteratorGeneric(const wxRegionIteratorGeneric& iterator);
    virtual ~wxRegionIteratorGeneric();

    wxRegionIteratorGeneric& operator=(const wxRegionIteratorGeneric& iterator);

    void Reset() { m_current = 0; }
    void Reset(const wxRegionGeneric& region);

    operator bool () const { return HaveRects(); }
    bool HaveRects() const;

    wxRegionIteratorGeneric& operator++();
    wxRegionIteratorGeneric operator++(int);

    long GetX() const;
    long GetY() const;
    long GetW() const;
    long GetWidth() const { return GetW(); }
    long GetH() const;
    long GetHeight() const { return GetH(); }
    wxRect GetRect() const;
private:
    long     m_current;
    wxRegionGeneric m_region;
};

#endif // _WX_GENERIC_REGION_H__
