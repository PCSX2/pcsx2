/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/scrolbar.h
// Purpose:     wxScrollBar class
// Author:      David Elliott
// Modified by:
// Created:     2004/04/25
// Copyright:   (c) 2004 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_SCROLBAR_H__
#define _WX_COCOA_SCROLBAR_H__

#include "wx/cocoa/NSScroller.h"

// ========================================================================
// wxScrollBar
// ========================================================================
class WXDLLIMPEXP_CORE wxScrollBar: public wxScrollBarBase, protected wxCocoaNSScroller
{
    DECLARE_DYNAMIC_CLASS(wxScrollBar)
    DECLARE_EVENT_TABLE()
    WX_DECLARE_COCOA_OWNER(NSScroller,NSControl,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxScrollBar() { }
    wxScrollBar(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxSB_HORIZONTAL,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxScrollBarNameStr)
    {
        Create(parent, winid, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxSB_HORIZONTAL,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxScrollBarNameStr);
    virtual ~wxScrollBar();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
    virtual void Cocoa_wxNSScrollerAction(void);
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    // accessors
    virtual int GetThumbPosition() const;
    virtual int GetThumbSize() const { return m_thumbSize; }
    virtual int GetPageSize() const { return m_pageSize; }
    virtual int GetRange() const { return m_range; }

    // operations
    virtual void SetThumbPosition(int viewStart);
    virtual void SetScrollbar(int position, int thumbSize,
                              int range, int pageSize,
                              bool refresh = TRUE);
protected:
    int m_range;
    int m_thumbSize;
    int m_pageSize;
};

#endif
    // _WX_COCOA_SCROLBAR_H__
