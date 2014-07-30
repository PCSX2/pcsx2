/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/spinbutt.h
// Purpose:     wxSpinButton class
// Author:      David Elliott
// Modified by:
// Created:     2003/07/14
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_SPINBUTT_H__
#define __WX_COCOA_SPINBUTT_H__

// #include "wx/cocoa/NSStepper.h"

// ========================================================================
// wxSpinButton
// ========================================================================
class WXDLLIMPEXP_CORE wxSpinButton: public wxSpinButtonBase// , protected wxCocoaNSStepper
{
    DECLARE_DYNAMIC_CLASS(wxSpinButton)
    DECLARE_EVENT_TABLE()
//    WX_DECLARE_COCOA_OWNER(NSStepper,NSControl,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxSpinButton() { }
    wxSpinButton(wxWindow *parent, wxWindowID winid = wxID_ANY,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxSP_VERTICAL | wxSP_ARROW_KEYS,
            const wxString& name = wxSPIN_BUTTON_NAME)
    {
        Create(parent, winid, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid = wxID_ANY,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxSP_HORIZONTAL,
            const wxString& name = wxSPIN_BUTTON_NAME);
    virtual ~wxSpinButton();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
    virtual void CocoaTarget_action();
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    // Pure Virtuals
    virtual int GetValue() const;
    virtual void SetValue(int value);

    // retrieve/change the range
    virtual void SetRange(int minValue, int maxValue);
};

#endif
    // __WX_COCOA_SPINBUTT_H__
