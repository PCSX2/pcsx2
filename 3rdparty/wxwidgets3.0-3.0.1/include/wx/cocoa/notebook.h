/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/notebook.h
// Purpose:     wxNotebook class
// Author:      David Elliott
// Modified by:
// Created:     2004/04/08
// Copyright:   (c) 2004 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_NOTEBOOK_H__
#define _WX_COCOA_NOTEBOOK_H__

#include "wx/cocoa/NSTabView.h"

// ========================================================================
// wxNotebook
// ========================================================================
class WXDLLIMPEXP_CORE wxNotebook: public wxNotebookBase, protected wxCocoaNSTabView
{
    DECLARE_DYNAMIC_CLASS(wxNotebook)
    DECLARE_EVENT_TABLE()
    WX_DECLARE_COCOA_OWNER(NSTabView,NSView,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxNotebook() { }
    wxNotebook(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = 0,
            const wxString& name = wxNotebookNameStr)
    {
        Create(parent, winid, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = 0,
            const wxString& name = wxNotebookNameStr);
    virtual ~wxNotebook();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
    // Notebooks cannot be enabled/disabled
    virtual void CocoaSetEnabled(bool WXUNUSED(enable)) { }
    virtual void CocoaDelegate_tabView_didSelectTabViewItem(WX_NSTabViewItem tabviewItem);
    virtual bool CocoaDelegate_tabView_shouldSelectTabViewItem(WX_NSTabViewItem tabviewItem);
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    // set the currently selected page, return the index of the previously
    // selected one (or wxNOT_FOUND on error)
    int SetSelection(size_t nPage);
    // get the currently selected page
    int GetSelection() const;

    // changes selected page without sending events
    int ChangeSelection(size_t nPage);

    // set/get the title of a page
    bool SetPageText(size_t nPage, const wxString& strText);
    wxString GetPageText(size_t nPage) const;

    // sets/returns item's image index in the current image list
    int  GetPageImage(size_t nPage) const;
    bool SetPageImage(size_t nPage, int nImage);

    // set the size (the same for all pages)
    void SetPageSize(const wxSize& size);

    // SetPadding and SetTabSize aren't possible to implement
    void SetPadding(const wxSize& padding);
    void SetTabSize(const wxSize& sz);

    //-----------------------
    // adding/removing pages

    // remove one page from the notebook, without deleting
    virtual wxNotebookPage *DoRemovePage(size_t nPage);

    // remove one page from the notebook
    bool DeletePage(size_t nPage);
    // remove all pages
    bool DeleteAllPages();

    // adds a new page to the notebook (it will be deleted ny the notebook,
    // don't delete it yourself). If bSelect, this page becomes active.
    // the same as AddPage(), but adds it at the specified position
    bool InsertPage( size_t position,
                     wxNotebookPage *win,
                     const wxString& strText,
                     bool bSelect = false,
                     int imageId = NO_IMAGE );

protected:
};

#endif //ndef _WX_COCOA_NOTEBOOK_H__
