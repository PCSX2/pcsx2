/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/stattext.h
// Purpose:     wxStaticText class
// Author:      David Elliott
// Modified by:
// Created:     2003/02/15
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_STATTEXT_H__
#define __WX_COCOA_STATTEXT_H__

#include "wx/cocoa/NSTextField.h"

// ========================================================================
// wxStaticText
// ========================================================================
class WXDLLIMPEXP_CORE wxStaticText : public wxStaticTextBase, protected wxCocoaNSTextField
{
    DECLARE_DYNAMIC_CLASS(wxStaticText)
    DECLARE_EVENT_TABLE()
    WX_DECLARE_COCOA_OWNER(NSTextField,NSControl,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxStaticText() {}
    wxStaticText(wxWindow *parent, wxWindowID winid,
            const wxString& label,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize, long style = 0,
            const wxString& name = wxStaticTextNameStr)
    {
        Create(parent, winid, label, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxString& label,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize, long style = 0,
            const wxString& name = wxStaticTextNameStr);
    virtual ~wxStaticText();

// ------------------------------------------------------------------------
// Cocoa specifics
// ------------------------------------------------------------------------
protected:
    virtual void Cocoa_didChangeText(void);
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    virtual void SetLabel(const wxString& label);
    virtual wxString GetLabel() const;
};

#endif // __WX_COCOA_STATTEXT_H__
