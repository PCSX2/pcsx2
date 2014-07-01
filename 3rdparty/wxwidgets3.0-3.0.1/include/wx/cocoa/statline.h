/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/statline.h
// Purpose:     wxStaticLine class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/18
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_STATLINE_H__
#define __WX_COCOA_STATLINE_H__

// #include "wx/cocoa/NSButton.h"

// ========================================================================
// wxStaticLine
// ========================================================================
class WXDLLIMPEXP_CORE wxStaticLine: public wxStaticLineBase// , protected wxCocoaNSButton
{
    DECLARE_DYNAMIC_CLASS(wxStaticLine)
    DECLARE_EVENT_TABLE()
//    WX_DECLARE_COCOA_OWNER(NSButton,NSControl,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxStaticLine() { }
    wxStaticLine(wxWindow *parent, wxWindowID winid = wxID_ANY,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = 0, const wxString& name = wxStaticLineNameStr)
    {
        Create(parent, winid, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid = wxID_ANY,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = 0, const wxString& name = wxStaticLineNameStr);
    virtual ~wxStaticLine();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
    // Static lines cannot be enabled/disabled
    virtual void CocoaSetEnabled(bool WXUNUSED(enable)) { }
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
};

#endif // __WX_COCOA_STATLINE_H__
