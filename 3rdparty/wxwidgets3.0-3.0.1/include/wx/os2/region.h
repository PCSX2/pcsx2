/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/region.h
// Purpose:     wxRegion class
// Author:      David Webster
// Modified by:
// Created:     10/15/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OS2_REGION_H_
#define _WX_OS2_REGION_H_

#include "wx/list.h"
#include "wx/os2/private.h"

class WXDLLIMPEXP_CORE wxRegion : public wxRegionWithCombine
{
public:
    wxRegion( wxCoord x
             ,wxCoord y
             ,wxCoord vWidth
             ,wxCoord vHeight
            );
    wxRegion( const wxPoint& rTopLeft
             ,const wxPoint& rBottomRight
            );
    wxRegion(const wxRect& rRect);
    wxRegion(WXHRGN hRegion, WXHDC hPS); // Hangs on to this region
    wxRegion(size_t n, const wxPoint *points, wxPolygonFillMode fillStyle = wxODDEVEN_RULE );
    wxRegion( const wxBitmap& bmp)
    {
        Union(bmp);
    }
    wxRegion( const wxBitmap& bmp,
              const wxColour& transColour, int tolerance = 0)
    {
        Union(bmp, transColour, tolerance);
    }

    wxRegion();
    virtual ~wxRegion();

    //
    // Modify region
    //

    //
    // Clear current region
    //
    virtual void Clear();

    //
    // Is region empty?
    //
    virtual bool IsEmpty() const;

    //
    // Get internal region handle
    //
    WXHRGN GetHRGN() const;

    void   SetPS(HPS hPS);

protected:
    virtual wxGDIRefData* CreateGDIRefData(void) const;
    virtual wxGDIRefData* CloneGDIRefData(const wxGDIRefData* pData) const;

    virtual bool DoIsEqual(const wxRegion& region) const;
    virtual bool DoGetBox(wxCoord& x, wxCoord& y, wxCoord& w, wxCoord& h) const;
    virtual wxRegionContain DoContainsPoint(wxCoord x, wxCoord y) const;
    virtual wxRegionContain DoContainsRect(const wxRect& rect) const;

    virtual bool DoOffset(wxCoord x, wxCoord y);
    virtual bool DoCombine(const wxRegion& region, wxRegionOp op);

    friend class WXDLLIMPEXP_FWD_CORE wxRegionIterator;
    DECLARE_DYNAMIC_CLASS(wxRegion);

}; // end of CLASS wxRegion

class WXDLLIMPEXP_CORE wxRegionIterator : public wxObject
{
public:
    wxRegionIterator();
    wxRegionIterator(const wxRegion& rRegion);
    virtual ~wxRegionIterator();

    void Reset(void) { m_lCurrent = 0; }
    void Reset(const wxRegion& rRegion);

    operator bool (void) const { return m_lCurrent < m_lNumRects; }
    bool HaveRects(void) const { return m_lCurrent < m_lNumRects; }

    void operator ++ (void);
    void operator ++ (int);

    wxCoord GetX(void) const;
    wxCoord GetY(void) const;
    wxCoord GetW(void) const;
    wxCoord GetWidth(void) const { return GetW(); }
    wxCoord GetH(void) const;
    wxCoord GetHeight(void) const { return GetH(); }
    wxRect  GetRect(void) const { return wxRect(GetX(), GetY(), GetWidth(), GetHeight()); }

private:
    long                            m_lCurrent;
    long                            m_lNumRects;
    wxRegion                        m_vRegion;
    wxRect*                         m_pRects;

    DECLARE_DYNAMIC_CLASS(wxRegionIterator)
}; // end of wxRegionIterator

#endif // _WX_OS2_REGION_H_
