/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/region.h
// Purpose:     wxRegion class
// Author:      Vaclav Slavik
// Created:     2006-08-08
// Copyright:   (c) 2006 REA Elektronik GmbH
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_REGION_H_
#define _WX_DFB_REGION_H_

class WXDLLIMPEXP_CORE wxRegion : public wxRegionBase
{
public:
    wxRegion();
    wxRegion(wxCoord x, wxCoord y, wxCoord w, wxCoord h);
    wxRegion(const wxPoint& topLeft, const wxPoint& bottomRight);
    wxRegion(const wxRect& rect);
    wxRegion(const wxBitmap& bmp)
    {
        Union(bmp);
    }
    wxRegion(const wxBitmap& bmp,
             const wxColour& transColour, int tolerance = 0)
    {
        Union(bmp, transColour, tolerance);
    }

    virtual ~wxRegion();

    // wxRegionBase methods
    virtual void Clear();
    virtual bool IsEmpty() const;

    // NB: implementation detail of DirectFB, should be removed if full
    //     (i.e. not rect-only version is implemented) so that all code that
    //     assumes region==rect breaks
    wxRect AsRect() const { return GetBox(); }

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


    friend class WXDLLIMPEXP_FWD_CORE wxRegionIterator;

    DECLARE_DYNAMIC_CLASS(wxRegion);
};


class WXDLLIMPEXP_CORE wxRegionIterator : public wxObject
{
public:
    wxRegionIterator() {}
    wxRegionIterator(const wxRegion& region) { Reset(region); }

    void Reset() { m_rect = wxRect(); }
    void Reset(const wxRegion& region);

    bool HaveRects() const { return !m_rect.IsEmpty(); }
    operator bool() const { return HaveRects(); }

    wxRegionIterator& operator++();
    wxRegionIterator operator++(int);

    wxCoord GetX() const { return m_rect.GetX(); }
    wxCoord GetY() const { return m_rect.GetY(); }
    wxCoord GetW() const { return m_rect.GetWidth(); }
    wxCoord GetWidth() const { return GetW(); }
    wxCoord GetH() const { return m_rect.GetHeight(); }
    wxCoord GetHeight() const { return GetH(); }
    wxRect GetRect() const { return m_rect; }

private:
    wxRect m_rect;

    DECLARE_DYNAMIC_CLASS(wxRegionIterator);
};

#endif // _WX_DFB_REGION_H_
