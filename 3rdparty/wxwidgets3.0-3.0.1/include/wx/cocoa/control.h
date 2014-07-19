/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/control.h
// Purpose:     wxControl class
// Author:      David Elliott
// Modified by:
// Created:     2003/02/15
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_CONTROL_H__
#define __WX_COCOA_CONTROL_H__

#include "wx/cocoa/NSControl.h"

// ========================================================================
// wxControl
// ========================================================================

class WXDLLIMPEXP_CORE wxControl : public wxControlBase, public wxCocoaNSControl
{
    DECLARE_ABSTRACT_CLASS(wxControl)
    WX_DECLARE_COCOA_OWNER(NSControl,NSView,NSView)
    DECLARE_EVENT_TABLE()
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxControl() {}
    wxControl(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize, long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxControlNameStr)
    {
        Create(parent, winid, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize, long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxControlNameStr);
    virtual ~wxControl();

// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:

    // implementation from now on
    // --------------------------

    void OnEraseBackground(wxEraseEvent& event);

    virtual void Command(wxCommandEvent& event) { ProcessCommand(event); }

    // Calls the callback and appropriate event handlers
    bool ProcessCommand(wxCommandEvent& event);

    // Enables the control
    virtual void CocoaSetEnabled(bool enable);
protected:
    virtual wxSize DoGetBestSize() const;

    // Provides a common implementation of title setting which strips mnemonics
    // and then calls setTitle: with the stripped string.  May be implemented
    // to call setTitleWithMnemonic: on OpenStep-compatible systems.  Only
    // intended for use by views or cells which implement at least setTitle:
    // and possibly setTitleWithMnemonic: such as NSBox and NSButton or NSCell
    // classes, for example as used by wxRadioBox.  Not usable with classes like
    // NSTextField which expect setStringValue:.
    static void CocoaSetLabelForObject(const wxString& labelWithWxMnemonic, struct objc_object *anObject);
};

#endif
    // __WX_COCOA_CONTROL_H__
