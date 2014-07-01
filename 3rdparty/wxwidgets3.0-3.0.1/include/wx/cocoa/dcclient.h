/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/dcclient.h
// Purpose:     wxClientDCImpl, wxPaintDCImpl and wxWindowDCImpl classes
// Author:      David Elliott
// Modified by:
// Created:     2003/04/01
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_DCCLIENT_H__
#define __WX_COCOA_DCCLIENT_H__

#include "wx/cocoa/dc.h"

// DFE: A while ago I stumbled upon the fact that retrieving the parent
// NSView of the content view seems to return the entire window rectangle
// (including decorations).  Of course, that is not at all part of the
// Cocoa or OpenStep APIs, but it might be a neat hack.
class WXDLLIMPEXP_CORE wxWindowDCImpl: public wxCocoaDCImpl
{
    DECLARE_DYNAMIC_CLASS(wxWindowDCImpl)
public:
    wxWindowDCImpl(wxDC *owner);
    // Create a DC corresponding to a window
    wxWindowDCImpl(wxDC *owner, wxWindow *win);
    virtual ~wxWindowDCImpl(void);

protected:
    wxWindow *m_window;
    WX_NSView m_lockedNSView;
// DC stack
    virtual bool CocoaLockFocus();
    virtual bool CocoaUnlockFocus();
    bool CocoaLockFocusOnNSView(WX_NSView nsview);
    bool CocoaUnlockFocusOnNSView();
    virtual bool CocoaGetBounds(void *rectData);
};

class WXDLLIMPEXP_CORE wxClientDCImpl: public wxWindowDCImpl
{
    DECLARE_DYNAMIC_CLASS(wxClientDCImpl)
public:
    wxClientDCImpl(wxDC *owner);
    // Create a DC corresponding to a window
    wxClientDCImpl(wxDC *owner, wxWindow *win);
    virtual ~wxClientDCImpl(void);
protected:
// DC stack
    virtual bool CocoaLockFocus();
    virtual bool CocoaUnlockFocus();
};

class WXDLLIMPEXP_CORE wxPaintDCImpl: public wxWindowDCImpl
{
    DECLARE_DYNAMIC_CLASS(wxPaintDCImpl)
public:
    wxPaintDCImpl(wxDC *owner);
    // Create a DC corresponding to a window
    wxPaintDCImpl(wxDC *owner, wxWindow *win);
    virtual ~wxPaintDCImpl(void);
protected:
// DC stack
    virtual bool CocoaLockFocus();
    virtual bool CocoaUnlockFocus();
};

#endif
    // __WX_COCOA_DCCLIENT_H__
