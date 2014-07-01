/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/gauge.h
// Purpose:     wxGauge class
// Author:      David Elliott
// Modified by:
// Created:     2003/07/15
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_GAUGE_H__
#define __WX_COCOA_GAUGE_H__

// #include "wx/cocoa/NSProgressIndicator.h"

DECLARE_WXCOCOA_OBJC_CLASS(NSProgressIndicator);

// ========================================================================
// wxGauge
// ========================================================================
class WXDLLIMPEXP_CORE wxGauge: public wxGaugeBase// , protected wxCocoaNSProgressIndicator
{
    DECLARE_DYNAMIC_CLASS(wxGauge)
    DECLARE_EVENT_TABLE()
//    WX_DECLARE_COCOA_OWNER(NSProgressIndicator,NSView,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxGauge() { }
    wxGauge(wxWindow *parent, wxWindowID winid, int range,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxGA_HORIZONTAL,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxGaugeNameStr)
    {
        Create(parent, winid, range, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid, int range,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxGA_HORIZONTAL,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxGaugeNameStr);
    virtual ~wxGauge();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
public:
    inline WX_NSProgressIndicator GetNSProgressIndicator() const { return (WX_NSProgressIndicator)m_cocoaNSView; }
protected:
    // NSProgressIndicator cannot be enabled/disabled
    virtual void CocoaSetEnabled(bool WXUNUSED(enable)) { }
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    // Pure Virtuals
    virtual int GetValue() const;
    virtual void SetValue(int value);

    // retrieve/change the range
    virtual void SetRange(int maxValue);
    int GetRange(void) const;
protected:
    virtual wxSize DoGetBestSize() const;
};

#endif
    // __WX_COCOA_GAUGE_H__
