/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/dcmemory.h
// Purpose:     wxMemoryDCImpl class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/16
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_DCMEMORY_H__
#define __WX_COCOA_DCMEMORY_H__

#include "wx/cocoa/dc.h"

#include "wx/dcmemory.h"

class WXDLLIMPEXP_CORE wxMemoryDCImpl: public wxCocoaDCImpl
{
    DECLARE_DYNAMIC_CLASS(wxMemoryDCImpl)

public:
    wxMemoryDCImpl(wxMemoryDC *owner)
    :   wxCocoaDCImpl(owner)
    {   Init(); }
    wxMemoryDCImpl(wxMemoryDC *owner, wxBitmap& bitmap)
    :   wxCocoaDCImpl(owner)
    {   Init();
        owner->SelectObject(bitmap);
    }
    wxMemoryDCImpl(wxMemoryDC *owner, wxDC *dc ); // Create compatible DC
    virtual ~wxMemoryDCImpl(void);

    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoSelect(const wxBitmap& bitmap);

protected:
    wxBitmap m_selectedBitmap;
    WX_NSImage m_cocoaNSImage;
// DC stack
    virtual bool CocoaLockFocus();
    virtual bool CocoaUnlockFocus();
    virtual bool CocoaGetBounds(void *rectData);
// Blitting
    virtual bool CocoaDoBlitOnFocusedDC(wxCoord xdest, wxCoord ydest,
        wxCoord width, wxCoord height, wxCoord xsrc, wxCoord ysrc,
        wxRasterOperationMode logicalFunc, bool useMask, wxCoord xsrcMask, wxCoord ysrcMask);

private:
    void Init();
};

#endif
    // __WX_COCOA_DCMEMORY_H__
