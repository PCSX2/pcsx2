///////////////////////////////////////////////////////////////////////////////
// Name:        wx/treelist.h
// Purpose:     wxTreeListCtrl class declaration.
// Author:      Vadim Zeitlin
// Created:     2011-08-17
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TREELIST_H_
#define _WX_TREELIST_H_

#include "wx/defs.h"

#if wxUSE_TREELISTCTRL

#include "wx/compositewin.h"
#include "wx/containr.h"
#include "wx/headercol.h"
#include "wx/itemid.h"
#include "wx/vector.h"
#include "wx/window.h"
#include "wx/withimages.h"

class WXDLLIMPEXP_FWD_ADV wxDataViewCtrl;
class WXDLLIMPEXP_FWD_ADV wxDataViewEvent;

extern WXDLLIMPEXP_DATA_ADV(const char) wxTreeListCtrlNameStr[];

class wxTreeListCtrl;
class wxTreeListModel;
class wxTreeListModelNode;

// ----------------------------------------------------------------------------
// Constants.
// ----------------------------------------------------------------------------

// wxTreeListCtrl styles.
//
// Notice that using wxTL_USER_3STATE implies wxTL_3STATE and wxTL_3STATE in
// turn implies wxTL_CHECKBOX.
enum
{
    wxTL_SINGLE         = 0x0000,       // This is the default anyhow.
    wxTL_MULTIPLE       = 0x0001,       // Allow multiple selection.
    wxTL_CHECKBOX       = 0x0002,       // Show checkboxes in the first column.
    wxTL_3STATE         = 0x0004,       // Allow 3rd state in checkboxes.
    wxTL_USER_3STATE    = 0x0008,       // Allow user to set 3rd state.
    wxTL_NO_HEADER      = 0x0010,       // Column titles not visible.

    wxTL_DEFAULT_STYLE  = wxTL_SINGLE,
    wxTL_STYLE_MASK     = wxTL_SINGLE |
                          wxTL_MULTIPLE |
                          wxTL_CHECKBOX |
                          wxTL_3STATE |
                          wxTL_USER_3STATE
};

// ----------------------------------------------------------------------------
// wxTreeListItem: unique identifier of an item in wxTreeListCtrl.
// ----------------------------------------------------------------------------

// Make wxTreeListItem a forward-declarable class even though it's simple
// enough to possibly be declared as a simple typedef.
class wxTreeListItem : public wxItemId<wxTreeListModelNode*>
{
public:
    wxTreeListItem(wxTreeListModelNode* item = NULL)
        : wxItemId<wxTreeListModelNode*>(item)
    {
    }
};

// Container of multiple items.
typedef wxVector<wxTreeListItem> wxTreeListItems;

// Some special "items" that can be used with InsertItem():
extern WXDLLIMPEXP_DATA_ADV(const wxTreeListItem) wxTLI_FIRST;
extern WXDLLIMPEXP_DATA_ADV(const wxTreeListItem) wxTLI_LAST;

// ----------------------------------------------------------------------------
// wxTreeListItemComparator: defines order of wxTreeListCtrl items.
// ----------------------------------------------------------------------------

class wxTreeListItemComparator
{
public:
    wxTreeListItemComparator() { }

    // The comparison function should return negative, null or positive value
    // depending on whether the first item is less than, equal to or greater
    // than the second one. The items should be compared using their values for
    // the given column.
    virtual int
    Compare(wxTreeListCtrl* treelist,
            unsigned column,
            wxTreeListItem first,
            wxTreeListItem second) = 0;

    // Although this class is not used polymorphically by wxWidgets itself,
    // provide virtual dtor in case it's used like this in the user code.
    virtual ~wxTreeListItemComparator() { }

private:
    wxDECLARE_NO_COPY_CLASS(wxTreeListItemComparator);
};

// ----------------------------------------------------------------------------
// wxTreeListCtrl: a control combining wxTree- and wxListCtrl features.
// ----------------------------------------------------------------------------

// This control also provides easy to use high level interface. Although the
// implementation uses wxDataViewCtrl internally, this class is intentionally
// simpler than wxDataViewCtrl and doesn't provide all of its functionality.
//
// If you need extra features you can always use GetDataView() accessor to work
// with wxDataViewCtrl directly but doing this makes your unportable to possible
// future non-wxDataViewCtrl-based implementations of this class.

class WXDLLIMPEXP_ADV wxTreeListCtrl
    : public wxCompositeWindow< wxNavigationEnabled<wxWindow> >,
      public wxWithImages
{
public:
    // Constructors and such
    // ---------------------

    wxTreeListCtrl() { Init(); }
    wxTreeListCtrl(wxWindow* parent,
                   wxWindowID id,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxTL_DEFAULT_STYLE,
                   const wxString& name = wxTreeListCtrlNameStr)
    {
        Init();

        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow* parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTL_DEFAULT_STYLE,
                const wxString& name = wxTreeListCtrlNameStr);


    virtual ~wxTreeListCtrl();

    // Columns methods
    // ---------------

    // Add a column with the given title and attributes, returns the index of
    // the new column or -1 on failure.
    int AppendColumn(const wxString& title,
                     int width = wxCOL_WIDTH_AUTOSIZE,
                     wxAlignment align = wxALIGN_LEFT,
                     int flags = wxCOL_RESIZABLE)
    {
        return DoInsertColumn(title, -1, width, align, flags);
    }

    // Return the total number of columns.
    unsigned GetColumnCount() const;

    // Delete the column with the given index, returns false if index is
    // invalid or deleting the column failed for some other reason.
    bool DeleteColumn(unsigned col);

    // Delete all columns.
    void ClearColumns();

    // Set column width to either the given value in pixels or to the value
    // large enough to fit all of the items if width == wxCOL_WIDTH_AUTOSIZE.
    void SetColumnWidth(unsigned col, int width);

    // Get the current width of the given column in pixels.
    int GetColumnWidth(unsigned col) const;

    // Get the width appropriate for showing the given text. This is typically
    // used as second argument for AppendColumn() or with SetColumnWidth().
    int WidthFor(const wxString& text) const;


    // Item methods
    // ------------

    // Adding items. The parent and text of the first column of the new item
    // must always be specified, the rest is optional.
    //
    // Each item can have two images: one used for closed state and another for
    // opened one. Only the first one is ever used for the items that don't
    // have children. And both are not set by default.
    //
    // It is also possible to associate arbitrary client data pointer with the
    // new item. It will be deleted by the control when the item is deleted
    // (either by an explicit DeleteItem() call or because the entire control
    // is destroyed).

    wxTreeListItem AppendItem(wxTreeListItem parent,
                              const wxString& text,
                              int imageClosed = NO_IMAGE,
                              int imageOpened = NO_IMAGE,
                              wxClientData* data = NULL)
    {
        return DoInsertItem(parent, wxTLI_LAST, text,
                            imageClosed, imageOpened, data);
    }

    wxTreeListItem InsertItem(wxTreeListItem parent,
                              wxTreeListItem previous,
                              const wxString& text,
                              int imageClosed = NO_IMAGE,
                              int imageOpened = NO_IMAGE,
                              wxClientData* data = NULL)
    {
        return DoInsertItem(parent, previous, text,
                            imageClosed, imageOpened, data);
    }

    wxTreeListItem PrependItem(wxTreeListItem parent,
                               const wxString& text,
                               int imageClosed = NO_IMAGE,
                               int imageOpened = NO_IMAGE,
                               wxClientData* data = NULL)
    {
        return DoInsertItem(parent, wxTLI_FIRST, text,
                            imageClosed, imageOpened, data);
    }

    // Deleting items.
    void DeleteItem(wxTreeListItem item);
    void DeleteAllItems();


    // Tree navigation
    // ---------------

    // Return the (never shown) root item.
    wxTreeListItem GetRootItem() const;

    // The parent item may be invalid for the root-level items.
    wxTreeListItem GetItemParent(wxTreeListItem item) const;

    // Iterate over the given item children: start by calling GetFirstChild()
    // and then call GetNextSibling() for as long as it returns valid item.
    wxTreeListItem GetFirstChild(wxTreeListItem item) const;
    wxTreeListItem GetNextSibling(wxTreeListItem item) const;

    // Return the first child of the root item, which is also the first item of
    // the tree in depth-first traversal order.
    wxTreeListItem GetFirstItem() const { return GetFirstChild(GetRootItem()); }

    // Get item after the given one in the depth-first tree-traversal order.
    // Calling this function starting with the result of GetFirstItem() allows
    // iterating over all items in the tree.
    wxTreeListItem GetNextItem(wxTreeListItem item) const;


    // Items attributes
    // ----------------

    const wxString& GetItemText(wxTreeListItem item, unsigned col = 0) const;

    // The convenience overload below sets the text for the first column.
    void SetItemText(wxTreeListItem item, unsigned col, const wxString& text);
    void SetItemText(wxTreeListItem item, const wxString& text)
    {
        SetItemText(item, 0, text);
    }

    // By default the opened image is the same as the normal, closed one (if
    // it's used at all).
    void SetItemImage(wxTreeListItem item, int closed, int opened = NO_IMAGE);

    // Retrieve or set the data associated with the item.
    wxClientData* GetItemData(wxTreeListItem item) const;
    void SetItemData(wxTreeListItem item, wxClientData* data);


    // Expanding and collapsing
    // ------------------------

    void Expand(wxTreeListItem item);
    void Collapse(wxTreeListItem item);
    bool IsExpanded(wxTreeListItem item) const;


    // Selection handling
    // ------------------

    // This function can be used with single selection controls, use
    // GetSelections() with the multi-selection ones.
    wxTreeListItem GetSelection() const;

    // This one can be used with either single or multi-selection controls.
    unsigned GetSelections(wxTreeListItems& selections) const;

    // In single selection mode Select() deselects any other selected items, in
    // multi-selection case it adds to the selection.
    void Select(wxTreeListItem item);

    // Can be used in multiple selection mode only, single selected item in the
    // single selection mode can't be unselected.
    void Unselect(wxTreeListItem item);

    // Return true if the item is selected, can be used in both single and
    // multiple selection modes.
    bool IsSelected(wxTreeListItem item) const;

    // Select or unselect all items, only valid in multiple selection mode.
    void SelectAll();
    void UnselectAll();


    // Checkbox handling
    // -----------------

    // Methods in this section can only be used with the controls created with
    // wxTL_CHECKBOX style.

    // Simple set, unset or query the checked state.
    void CheckItem(wxTreeListItem item, wxCheckBoxState state = wxCHK_CHECKED);
    void UncheckItem(wxTreeListItem item) { CheckItem(item, wxCHK_UNCHECKED); }

    // The same but do it recursively for this item itself and its children.
    void CheckItemRecursively(wxTreeListItem item,
                              wxCheckBoxState state = wxCHK_CHECKED);

    // Update the parent of this item recursively: if this item and all its
    // siblings are checked, the parent will become checked as well. If this
    // item and all its siblings are unchecked, the parent will be unchecked.
    // And if the siblings of this item are not all in the same state, the
    // parent will be switched to indeterminate state. And then the same logic
    // will be applied to the parents parent and so on recursively.
    //
    // This is typically called when the state of the given item has changed
    // from EVT_TREELIST_ITEM_CHECKED() handler in the controls which have
    // wxTL_3STATE flag. Notice that without this flag this function can't work
    // as it would be unable to set the state of a parent with both checked and
    // unchecked items so it's only allowed to call it when this flag is set.
    void UpdateItemParentStateRecursively(wxTreeListItem item);

    // Return the current state.
    wxCheckBoxState GetCheckedState(wxTreeListItem item) const;

    // Return true if all item children (if any) are in the given state.
    bool AreAllChildrenInState(wxTreeListItem item,
                               wxCheckBoxState state) const;



    // Sorting.
    // --------

    // Sort by the given column, either in ascending (default) or descending
    // sort order.
    //
    // By default, simple alphabetical sorting is done by this column contents
    // but SetItemComparator() may be called to perform comparison in some
    // other way.
    void SetSortColumn(unsigned col, bool ascendingOrder = true);

    // If the control contents is sorted, return true and fill the output
    // parameters with the column which is currently used for sorting and
    // whether we sort using ascending or descending order. Otherwise, i.e. if
    // the control contents is unsorted, simply return false.
    bool GetSortColumn(unsigned* col, bool* ascendingOrder = NULL);

    // Set the object to use for comparing the items. It will be called when
    // the control is being sorted because the user clicked on a sortable
    // column.
    //
    // The provided pointer is stored by the control so the object it points to
    // must have a life-time equal or greater to that of the control itself. In
    // addition, the pointer can be NULL to stop using custom comparator and
    // revert to the default alphabetical comparison.
    void SetItemComparator(wxTreeListItemComparator* comparator);


    // View window functions.
    // ----------------------

    // This control itself is entirely covered by the "view window" which is
    // currently a wxDataViewCtrl but if you want to avoid relying on this to
    // allow your code to work with later versions which might not be
    // wxDataViewCtrl-based, use the first function only and only use the
    // second one if you really need to call wxDataViewCtrl methods on it.
    wxWindow* GetView() const;
    wxDataViewCtrl* GetDataView() const { return m_view; }

private:
    // Common part of all ctors.
    void Init();

    // Pure virtual method inherited from wxCompositeWindow.
    virtual wxWindowList GetCompositeWindowParts() const;

    // Implementation of AppendColumn().
    int DoInsertColumn(const wxString& title,
                       int pos,     // May be -1 meaning "append".
                       int width,
                       wxAlignment align,
                       int flags);

    // Common part of {Append,Insert,Prepend}Item().
    wxTreeListItem DoInsertItem(wxTreeListItem parent,
                                wxTreeListItem previous,
                                const wxString& text,
                                int imageClosed,
                                int imageOpened,
                                wxClientData* data);

    // Send wxTreeListEvent corresponding to the given wxDataViewEvent for an
    // item (as opposed for column-oriented events).
    //
    // Also updates the original event "skipped" and "vetoed" flags.
    void SendItemEvent(wxEventType evt, wxDataViewEvent& event);

    // Send wxTreeListEvent corresponding to the given column wxDataViewEvent.
    void SendColumnEvent(wxEventType evt, wxDataViewEvent& event);


    // Called by wxTreeListModel when an item is toggled by the user.
    void OnItemToggled(wxTreeListItem item, wxCheckBoxState stateOld);

    // Event handlers.
    void OnSelectionChanged(wxDataViewEvent& event);
    void OnItemExpanding(wxDataViewEvent& event);
    void OnItemExpanded(wxDataViewEvent& event);
    void OnItemActivated(wxDataViewEvent& event);
    void OnItemContextMenu(wxDataViewEvent& event);
    void OnColumnSorted(wxDataViewEvent& event);
    void OnSize(wxSizeEvent& event);

    wxDECLARE_EVENT_TABLE();


    wxDataViewCtrl* m_view;
    wxTreeListModel* m_model;

    wxTreeListItemComparator* m_comparator;


    // It calls our inherited protected wxWithImages::GetImage() method.
    friend class wxTreeListModel;

    wxDECLARE_NO_COPY_CLASS(wxTreeListCtrl);
};

// ----------------------------------------------------------------------------
// wxTreeListEvent: event generated by wxTreeListCtrl.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxTreeListEvent : public wxNotifyEvent
{
public:
    // Default ctor is provided for wxRTTI needs only but should never be used.
    wxTreeListEvent() { Init(); }

    // The item affected by the event. Valid for all events except
    // column-specific ones such as COLUMN_SORTED.
    wxTreeListItem GetItem() const { return m_item; }

    // The previous state of the item checkbox for ITEM_CHECKED events only.
    wxCheckBoxState GetOldCheckedState() const { return m_oldCheckedState; }

    // The index of the column affected by the event. Currently only used by
    // COLUMN_SORTED event.
    unsigned GetColumn() const { return m_column; }

    virtual wxEvent* Clone() const { return new wxTreeListEvent(*this); }

private:
    // Common part of all ctors.
    void Init()
    {
        m_column = static_cast<unsigned>(-1);

        m_oldCheckedState = wxCHK_UNDETERMINED;
    }

    // Ctor is private, only wxTreeListCtrl can create events of this type.
    wxTreeListEvent(wxEventType evtType,
                    wxTreeListCtrl* treelist,
                    wxTreeListItem item)
        : wxNotifyEvent(evtType, treelist->GetId()),
          m_item(item)
    {
        SetEventObject(treelist);

        Init();
    }

    // Set the checkbox state before this event for ITEM_CHECKED events.
    void SetOldCheckedState(wxCheckBoxState state)
    {
        m_oldCheckedState = state;
    }

    // Set the column affected by this event for COLUMN_SORTED events.
    void SetColumn(unsigned column)
    {
        m_column = column;
    }


    const wxTreeListItem m_item;

    wxCheckBoxState m_oldCheckedState;

    unsigned m_column;

    friend class wxTreeListCtrl;

    wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxTreeListEvent);
};

// Event types and event table macros.

typedef void (wxEvtHandler::*wxTreeListEventFunction)(wxTreeListEvent&);

#define wxTreeListEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxTreeListEventFunction, func)

#define wxEVT_TREELIST_GENERIC(name, id, fn) \
    wx__DECLARE_EVT1(wxEVT_TREELIST_##name, id, wxTreeListEventHandler(fn))

#define wxDECLARE_TREELIST_EVENT(name) \
    wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, \
                              wxEVT_TREELIST_##name, \
                              wxTreeListEvent)

wxDECLARE_TREELIST_EVENT(SELECTION_CHANGED);
#define EVT_TREELIST_SELECTION_CHANGED(id, fn) \
    wxEVT_TREELIST_GENERIC(SELECTION_CHANGED, id, fn)

wxDECLARE_TREELIST_EVENT(ITEM_EXPANDING);
#define EVT_TREELIST_ITEM_EXPANDING(id, fn) \
    wxEVT_TREELIST_GENERIC(ITEM_EXPANDING, id, fn)

wxDECLARE_TREELIST_EVENT(ITEM_EXPANDED);
#define EVT_TREELIST_ITEM_EXPANDED(id, fn) \
    wxEVT_TREELIST_GENERIC(ITEM_EXPANDED, id, fn)

wxDECLARE_TREELIST_EVENT(ITEM_CHECKED);
#define EVT_TREELIST_ITEM_CHECKED(id, fn) \
    wxEVT_TREELIST_GENERIC(ITEM_CHECKED, id, fn)

wxDECLARE_TREELIST_EVENT(ITEM_ACTIVATED);
#define EVT_TREELIST_ITEM_ACTIVATED(id, fn) \
    wxEVT_TREELIST_GENERIC(ITEM_ACTIVATED, id, fn)

wxDECLARE_TREELIST_EVENT(ITEM_CONTEXT_MENU);
#define EVT_TREELIST_ITEM_CONTEXT_MENU(id, fn) \
    wxEVT_TREELIST_GENERIC(ITEM_CONTEXT_MENU, id, fn)

wxDECLARE_TREELIST_EVENT(COLUMN_SORTED);
#define EVT_TREELIST_COLUMN_SORTED(id, fn) \
    wxEVT_TREELIST_GENERIC(COLUMN_SORTED, id, fn)

#undef wxDECLARE_TREELIST_EVENT

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_TREELIST_SELECTION_CHANGED   wxEVT_TREELIST_SELECTION_CHANGED
#define wxEVT_COMMAND_TREELIST_ITEM_EXPANDING      wxEVT_TREELIST_ITEM_EXPANDING
#define wxEVT_COMMAND_TREELIST_ITEM_EXPANDED       wxEVT_TREELIST_ITEM_EXPANDED
#define wxEVT_COMMAND_TREELIST_ITEM_CHECKED        wxEVT_TREELIST_ITEM_CHECKED
#define wxEVT_COMMAND_TREELIST_ITEM_ACTIVATED      wxEVT_TREELIST_ITEM_ACTIVATED
#define wxEVT_COMMAND_TREELIST_ITEM_CONTEXT_MENU   wxEVT_TREELIST_ITEM_CONTEXT_MENU
#define wxEVT_COMMAND_TREELIST_COLUMN_SORTED       wxEVT_TREELIST_COLUMN_SORTED

#endif // wxUSE_TREELISTCTRL

#endif // _WX_TREELIST_H_
