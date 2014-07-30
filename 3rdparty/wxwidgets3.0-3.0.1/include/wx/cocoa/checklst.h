/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/checklst.h
// Purpose:     wxCheckListBox class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/16
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_CHECKLST_H__
#define __WX_COCOA_CHECKLST_H__

//#include "wx/cocoa/NSTableView.h"

// ========================================================================
// wxCheckListBox
// ========================================================================
class WXDLLIMPEXP_CORE wxCheckListBox: public wxCheckListBoxBase //, protected wxCocoaNSTableView
{
    DECLARE_DYNAMIC_CLASS(wxCheckListBox)
    DECLARE_EVENT_TABLE()
    WX_DECLARE_COCOA_OWNER(NSTableView,NSControl,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxCheckListBox() { }
    wxCheckListBox(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr)
    {
        Create(parent, winid,  pos, size, n, choices, style, validator, name);
    }
    wxCheckListBox(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr)
    {
        Create(parent, winid,  pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr);
    bool Create(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr);
    virtual ~wxCheckListBox();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    // check list box specific methods
    virtual bool IsChecked(unsigned int item) const;
    virtual void Check(unsigned int item, bool check = true);
};

#endif // __WX_COCOA_CHECKLST_H__
