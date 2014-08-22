/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/radiobox.h
// Purpose:     wxRadioBox class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/18
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_RADIOBOX_H__
#define __WX_COCOA_RADIOBOX_H__

// #include "wx/cocoa/NSButton.h"
DECLARE_WXCOCOA_OBJC_CLASS(NSMatrix);

// ========================================================================
// wxRadioBox
// ========================================================================
class WXDLLIMPEXP_CORE wxRadioBox: public wxControl, public wxRadioBoxBase// , protected wxCocoaNSButton
{
    DECLARE_DYNAMIC_CLASS(wxRadioBox)
    DECLARE_EVENT_TABLE()
    // NOTE: We explicitly skip NSControl because our primary cocoa view is
    // the NSBox but we want to receive action messages from the NSMatrix.
    WX_DECLARE_COCOA_OWNER(NSBox,NSView,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxRadioBox() { }
    wxRadioBox(wxWindow *parent, wxWindowID winid,
            const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = NULL,
            int majorDim = 0,
            long style = 0, const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxRadioBoxNameStr)
    {
        Create(parent, winid, title, pos, size, n, choices, majorDim, style, validator, name);
    }
    wxRadioBox(wxWindow *parent, wxWindowID winid,
            const wxString& title,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            int majorDim = 0,
            long style = 0, const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxRadioBoxNameStr)
    {
        Create(parent, winid, title, pos, size, choices, majorDim, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = NULL,
            int majorDim = 0,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxRadioBoxNameStr);
    bool Create(wxWindow *parent, wxWindowID winid,
            const wxString& title,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            int majorDim = 0,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxRadioBoxNameStr);
    virtual ~wxRadioBox();

    // Enabling
    virtual bool Enable(unsigned int n, bool enable = true);
    virtual bool IsItemEnabled(unsigned int WXUNUSED(n)) const
    {
        /* TODO */
        return true;
    }

    // Showing
    virtual bool Show(unsigned int n, bool show = true);
    virtual bool IsItemShown(unsigned int WXUNUSED(n)) const
    {
        /* TODO */
        return true;
    }

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
    // Radio boxes cannot be enabled/disabled
    virtual void CocoaSetEnabled(bool WXUNUSED(enable)) { }
    virtual void CocoaTarget_action(void);
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
// Pure virtuals
    // selection
    virtual void SetSelection(int n);
    virtual int GetSelection() const;
    // string access
    virtual unsigned int GetCount() const;
    virtual wxString GetString(unsigned int n) const;
    virtual void SetString(unsigned int n, const wxString& label);
    // change the individual radio button state
protected:
    // We don't want the typical wxCocoaNSBox behaviour because our real
    // implementation is by using an NSMatrix as the NSBox's contentView.
    WX_NSMatrix GetNSMatrix() const;
    void AssociateNSBox(WX_NSBox theBox);
    void DisassociateNSBox(WX_NSBox theBox);

    virtual wxSize DoGetBestSize() const;

    int GetRowForIndex(int n) const
    {
        if(m_windowStyle & wxRA_SPECIFY_COLS)
            return n / GetMajorDim();
        else
            return n % GetMajorDim();
    }

    int GetColumnForIndex(int n) const
    {
        if(m_windowStyle & wxRA_SPECIFY_COLS)
            return n % GetMajorDim();
        else
            return n / GetMajorDim();
    }
};

#endif // __WX_COCOA_RADIOBOX_H__
