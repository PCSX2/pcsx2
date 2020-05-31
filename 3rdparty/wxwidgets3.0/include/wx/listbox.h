///////////////////////////////////////////////////////////////////////////////
// Name:        wx/listbox.h
// Purpose:     wxListBox class interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     22.10.99
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_LISTBOX_H_BASE_
#define _WX_LISTBOX_H_BASE_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"

#if wxUSE_LISTBOX

#include "wx/ctrlsub.h"         // base class

// forward declarations are enough here
class WXDLLIMPEXP_FWD_BASE wxArrayInt;
class WXDLLIMPEXP_FWD_BASE wxArrayString;

// ----------------------------------------------------------------------------
// global data
// ----------------------------------------------------------------------------

extern WXDLLIMPEXP_DATA_CORE(const char) wxListBoxNameStr[];

// ----------------------------------------------------------------------------
// wxListBox interface is defined by the class wxListBoxBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxListBoxBase : public wxControlWithItems
{
public:
    wxListBoxBase() { }
    virtual ~wxListBoxBase();

    void InsertItems(unsigned int nItems, const wxString *items, unsigned int pos)
        { Insert(nItems, items, pos); }
    void InsertItems(const wxArrayString& items, unsigned int pos)
        { Insert(items, pos); }

    // multiple selection logic
    virtual bool IsSelected(int n) const = 0;
    virtual void SetSelection(int n);
    void SetSelection(int n, bool select) { DoSetSelection(n, select); }
    void Deselect(int n) { DoSetSelection(n, false); }
    void DeselectAll(int itemToLeaveSelected = -1);

    virtual bool SetStringSelection(const wxString& s, bool select);
    virtual bool SetStringSelection(const wxString& s)
    {
        return SetStringSelection(s, true);
    }

    // works for single as well as multiple selection listboxes (unlike
    // GetSelection which only works for listboxes with single selection)
    virtual int GetSelections(wxArrayInt& aSelections) const = 0;

    // set the specified item at the first visible item or scroll to max
    // range.
    void SetFirstItem(int n) { DoSetFirstItem(n); }
    void SetFirstItem(const wxString& s);

    // ensures that the given item is visible scrolling the listbox if
    // necessary
    virtual void EnsureVisible(int n);

    // a combination of Append() and EnsureVisible(): appends the item to the
    // listbox and ensures that it is visible i.e. not scrolled out of view
    void AppendAndEnsureVisible(const wxString& s);

    // return true if the listbox allows multiple selection
    bool HasMultipleSelection() const
    {
        return (m_windowStyle & wxLB_MULTIPLE) ||
               (m_windowStyle & wxLB_EXTENDED);
    }

    // override wxItemContainer::IsSorted
    virtual bool IsSorted() const { return HasFlag( wxLB_SORT ); }

    // emulate selecting or deselecting the item event.GetInt() (depending on
    // event.GetExtraLong())
    void Command(wxCommandEvent& event);

    // return the index of the item at this position or wxNOT_FOUND
    int HitTest(const wxPoint& point) const { return DoListHitTest(point); }
    int HitTest(int x, int y) const { return DoListHitTest(wxPoint(x, y)); }


protected:
    virtual void DoSetFirstItem(int n) = 0;

    virtual void DoSetSelection(int n, bool select) = 0;

    // there is already wxWindow::DoHitTest() so call this one differently
    virtual int DoListHitTest(const wxPoint& WXUNUSED(point)) const
        { return wxNOT_FOUND; }

    // Helper for the code generating events in single selection mode: updates
    // m_oldSelections and return true if the selection really changed.
    // Otherwise just returns false.
    bool DoChangeSingleSelection(int item);

    // Helper for generating events in multiple and extended mode: compare the
    // current selections with the previously recorded ones (in
    // m_oldSelections) and send the appropriate event if they differ,
    // otherwise just return false.
    bool CalcAndSendEvent();

    // Send a listbox (de)selection or double click event.
    //
    // Returns true if the event was processed.
    bool SendEvent(wxEventType evtType, int item, bool selected);

    // Array storing the indices of all selected items that we already notified
    // the user code about for multi selection list boxes.
    //
    // For single selection list boxes, we reuse this array to store the single
    // currently selected item, this is used by DoChangeSingleSelection().
    //
    // TODO-OPT: wxSelectionStore would be more efficient for big list boxes.
    wxArrayInt m_oldSelections;

    // Update m_oldSelections with currently selected items (does nothing in
    // single selection mode on platforms other than MSW).
    void UpdateOldSelections();

private:
    wxDECLARE_NO_COPY_CLASS(wxListBoxBase);
};

// ----------------------------------------------------------------------------
// include the platform-specific class declaration
// ----------------------------------------------------------------------------

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/listbox.h"
#elif defined(__WXMSW__)
    #include "wx/msw/listbox.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/listbox.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/listbox.h"
#elif defined(__WXGTK__)
  #include "wx/gtk1/listbox.h"
#elif defined(__WXMAC__)
    #include "wx/osx/listbox.h"
#elif defined(__WXPM__)
    #include "wx/os2/listbox.h"
#elif defined(__WXCOCOA__)
    #include "wx/cocoa/listbox.h"
#endif

#endif // wxUSE_LISTBOX

#endif
    // _WX_LISTBOX_H_BASE_
