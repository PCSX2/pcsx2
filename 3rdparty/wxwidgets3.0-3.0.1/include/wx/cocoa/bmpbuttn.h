/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/bmpbuttn.h
// Purpose:     wxBitmapButton class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/16
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_BMPBUTTN_H__
#define __WX_COCOA_BMPBUTTN_H__

#include "wx/cocoa/NSButton.h"

// ========================================================================
// wxBitmapButton
// ========================================================================
class WXDLLIMPEXP_CORE wxBitmapButton : public wxBitmapButtonBase
{
    DECLARE_DYNAMIC_CLASS(wxBitmapButton)
    DECLARE_EVENT_TABLE()
    WX_DECLARE_COCOA_OWNER(NSButton,NSControl,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
   wxBitmapButton() { }
   wxBitmapButton(wxWindow *parent, wxWindowID winid,
             const wxBitmap& bitmap,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize, long style = 0,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxButtonNameStr)
    {
        Create(parent, winid, bitmap, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxBitmap& bitmap,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize, long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxButtonNameStr);
    virtual ~wxBitmapButton();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
    virtual void Cocoa_wxNSButtonAction(void);
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    // The wxButton::DoGetBestSize is not correct for bitmap buttons
    wxSize DoGetBestSize() const
    {   return wxButtonBase::DoGetBestSize(); }
};

#endif // __WX_COCOA_BMPBUTTN_H__
