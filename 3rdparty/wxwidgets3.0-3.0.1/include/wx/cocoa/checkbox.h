/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/checkbox.h
// Purpose:     wxCheckBox class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/16
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_CHECKBOX_H__
#define __WX_COCOA_CHECKBOX_H__

#include "wx/cocoa/NSButton.h"

// ========================================================================
// wxCheckBox
// ========================================================================
class WXDLLIMPEXP_CORE wxCheckBox: public wxCheckBoxBase , protected wxCocoaNSButton
{
    DECLARE_DYNAMIC_CLASS(wxCheckBox)
    DECLARE_EVENT_TABLE()
    WX_DECLARE_COCOA_OWNER(NSButton,NSControl,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxCheckBox() { }
    wxCheckBox(wxWindow *parent, wxWindowID winid,
            const wxString& label,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxCheckBoxNameStr)
    {
        Create(parent, winid, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxString& label,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxCheckBoxNameStr);
    virtual ~wxCheckBox();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
    virtual void Cocoa_wxNSButtonAction(void);
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    virtual void SetValue(bool);
    virtual bool GetValue() const;
    virtual void SetLabel(const wxString& label);
    virtual wxString GetLabel() const;

protected:
    virtual void DoSet3StateValue(wxCheckBoxState state);
    virtual wxCheckBoxState DoGet3StateValue() const;
};

#endif // __WX_COCOA_CHECKBOX_H__
