/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/region.h
// Purpose:     wxRegion class
// Author:      David Elliott
// Modified by:
// Created:     2004/04/12
// Copyright:   (c) 2004 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_REGION_H__
#define _WX_COCOA_REGION_H__

#include "wx/generic/region.h"

#if defined(__LP64__) || defined(NS_BUILD_32_LIKE_64)
typedef struct CGRect NSRect;
#else
typedef struct _NSRect NSRect;
#endif

class WXDLLIMPEXP_CORE wxRegion : public wxRegionGeneric
{
public:
    wxRegion(wxCoord x, wxCoord y, wxCoord w, wxCoord h)
    :   wxRegionGeneric(x,y,w,h)
    {}
    wxRegion(const wxPoint& topLeft, const wxPoint& bottomRight)
    :   wxRegionGeneric(topLeft, bottomRight)
    {}
    wxRegion(const wxRect& rect)
    :   wxRegionGeneric(rect)
    {}
    wxRegion() {}
    wxRegion(const wxBitmap& bmp)
    :   wxRegionGeneric()
        { Union(bmp); }
    wxRegion(const wxBitmap& bmp,
             const wxColour& transColour, int tolerance = 0)
    :   wxRegionGeneric()
        { Union(bmp, transColour, tolerance); }
    virtual ~wxRegion() {}
    wxRegion(const wxRegion& r)
    :   wxRegionGeneric(r)
    {}
    wxRegion& operator= (const wxRegion& r)
    {   return *(wxRegion*)&(this->wxRegionGeneric::operator=(r)); }

    // Cocoa-specific creation
    wxRegion(const NSRect& rect);
    wxRegion(const NSRect *rects, int count);

private:
    DECLARE_DYNAMIC_CLASS(wxRegion);
};

class WXDLLIMPEXP_CORE wxRegionIterator : public wxRegionIteratorGeneric
{
//    DECLARE_DYNAMIC_CLASS(wxRegionIteratorGeneric);
public:
    wxRegionIterator() {}
    wxRegionIterator(const wxRegion& region)
    :   wxRegionIteratorGeneric(region)
    {}
    wxRegionIterator(const wxRegionIterator& iterator)
    :   wxRegionIteratorGeneric(iterator)
    {}
    virtual ~wxRegionIterator() {}

    wxRegionIterator& operator=(const wxRegionIterator& iter)
    {   return *(wxRegionIterator*)&(this->wxRegionIteratorGeneric::operator=(iter)); }
};

#endif
    //ndef _WX_COCOA_REGION_H__
