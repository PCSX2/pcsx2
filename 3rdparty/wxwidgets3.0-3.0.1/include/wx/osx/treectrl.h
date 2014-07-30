/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/treectrl.h
// Purpose:     wxTreeCtrl class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TREECTRL_H_
#define _WX_TREECTRL_H_

#include "wx/control.h"
#include "wx/event.h"
#include "wx/imaglist.h"

#define wxTREE_MASK_HANDLE          0x0001
#define wxTREE_MASK_STATE           0x0002
#define wxTREE_MASK_TEXT            0x0004
#define wxTREE_MASK_IMAGE           0x0008
#define wxTREE_MASK_SELECTED_IMAGE  0x0010
#define wxTREE_MASK_CHILDREN        0x0020
#define wxTREE_MASK_DATA            0x0040

#define wxTREE_STATE_BOLD           0x0001
#define wxTREE_STATE_DROPHILITED    0x0002
#define wxTREE_STATE_EXPANDED       0x0004
#define wxTREE_STATE_EXPANDEDONCE   0x0008
#define wxTREE_STATE_FOCUSED        0x0010
#define wxTREE_STATE_SELECTED       0x0020
#define wxTREE_STATE_CUT            0x0040

#define wxTREE_HITTEST_ABOVE            0x0001  // Above the client area.
#define wxTREE_HITTEST_BELOW            0x0002  // Below the client area.
#define wxTREE_HITTEST_NOWHERE          0x0004  // In the client area but below the last item.
#define wxTREE_HITTEST_ONITEMBUTTON     0x0010  // On the button associated with an item.
#define wxTREE_HITTEST_ONITEMICON       0x0020  // On the bitmap associated with an item.
#define wxTREE_HITTEST_ONITEMINDENT     0x0040  // In the indentation associated with an item.
#define wxTREE_HITTEST_ONITEMLABEL      0x0080  // On the label (string) associated with an item.
#define wxTREE_HITTEST_ONITEMRIGHT      0x0100  // In the area to the right of an item.
#define wxTREE_HITTEST_ONITEMSTATEICON  0x0200  // On the state icon for a tree view item that is in a user-defined state.
#define wxTREE_HITTEST_TOLEFT           0x0400  // To the right of the client area.
#define wxTREE_HITTEST_TORIGHT          0x0800  // To the left of the client area.

#define wxTREE_HITTEST_ONITEM (wxTREE_HITTEST_ONITEMICON | wxTREE_HITTEST_ONITEMLABEL | wxTREE_HITTEST_ONITEMSTATEICON)

// Flags for GetNextItem
enum {
    wxTREE_NEXT_CARET,                 // Retrieves the currently selected item.
    wxTREE_NEXT_CHILD,                 // Retrieves the first child item. The hItem parameter must be NULL.
    wxTREE_NEXT_DROPHILITE,            // Retrieves the item that is the target of a drag-and-drop operation.
    wxTREE_NEXT_FIRSTVISIBLE,          // Retrieves the first visible item.
    wxTREE_NEXT_NEXT,                  // Retrieves the next sibling item.
    wxTREE_NEXT_NEXTVISIBLE,           // Retrieves the next visible item that follows the specified item.
    wxTREE_NEXT_PARENT,                // Retrieves the parent of the specified item.
    wxTREE_NEXT_PREVIOUS,              // Retrieves the previous sibling item.
    wxTREE_NEXT_PREVIOUSVISIBLE,       // Retrieves the first visible item that precedes the specified item.
    wxTREE_NEXT_ROOT                   // Retrieves the first child item of the root item of which the specified item is a part.
};

#if WXWIN_COMPATIBILITY_2_6
    // Flags for InsertItem
    enum {
        wxTREE_INSERT_LAST = -1,
        wxTREE_INSERT_FIRST = -2,
        wxTREE_INSERT_SORT = -3
    };
#endif

class WXDLLIMPEXP_CORE wxTreeItem: public wxObject
{
    DECLARE_DYNAMIC_CLASS(wxTreeItem)

public:

    long            m_mask;
    long            m_itemId;
    long            m_state;
    long            m_stateMask;
    wxString        m_text;
    int             m_image;
    int             m_selectedImage;
    int             m_children;
    long            m_data;

    wxTreeItem();

// Accessors
    inline long GetMask() const { return m_mask; }
    inline long GetItemId() const { return m_itemId; }
    inline long GetState() const { return m_state; }
    inline long GetStateMask() const { return m_stateMask; }
    inline wxString GetText() const { return m_text; }
    inline int GetImage() const { return m_image; }
    inline int GetSelectedImage() const { return m_selectedImage; }
    inline int GetChildren() const { return m_children; }
    inline long GetData() const { return m_data; }

    inline void SetMask(long mask) { m_mask = mask; }
    inline void SetItemId(long id) { m_itemId = m_itemId = id; }
    inline void SetState(long state) { m_state = state; }
    inline void SetStateMask(long stateMask) { m_stateMask = stateMask; }
    inline void GetText(const wxString& text) { m_text = text; }
    inline void SetImage(int image) { m_image = image; }
    inline void GetSelectedImage(int selImage) { m_selectedImage = selImage; }
    inline void SetChildren(int children) { m_children = children; }
    inline void SetData(long data) { m_data = data; }
};

class WXDLLIMPEXP_CORE wxTreeCtrl: public wxControl
{
public:
   /*
    * Public interface
    */

    // creation
    // --------
    wxTreeCtrl();

    inline wxTreeCtrl(wxWindow *parent, wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxTR_HAS_BUTTONS|wxTR_LINES_AT_ROOT,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = "wxTreeCtrl")
    {
        Create(parent, id, pos, size, style, validator, name);
    }
    virtual ~wxTreeCtrl();

    bool Create(wxWindow *parent, wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTR_HAS_BUTTONS|wxTR_LINES_AT_ROOT,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = "wxTreeCtrl");

    // accessors
    // ---------
      //
    virtual unsigned int GetCount() const;

      // indent
    int GetIndent() const;
    void SetIndent(int indent);
      // image list
    wxImageList *GetImageList(int which = wxIMAGE_LIST_NORMAL) const;

      // navigation inside the tree
    long GetNextItem(long item, int code) const;
    bool ItemHasChildren(long item) const;
    long GetChild(long item) const;
    long GetItemParent(long item) const;
    long GetFirstVisibleItem() const;
    long GetNextVisibleItem(long item) const;
    long GetSelection() const;
    long GetRootItem() const;

      // generic function for (g|s)etting item attributes
    bool GetItem(wxTreeItem& info) const;
    bool SetItem(wxTreeItem& info);
      // item state
    int  GetItemState(long item, long stateMask) const;
    bool SetItemState(long item, long state, long stateMask);
      // item image
    bool SetItemImage(long item, int image, int selImage);
      // item text
    wxString GetItemText(long item) const;
    void SetItemText(long item, const wxString& str);
      // custom data associated with the item
    long GetItemData(long item) const;
    bool SetItemData(long item, long data);
      // convenience function
    bool IsItemExpanded(long item)
    {
        return (GetItemState(item, wxTREE_STATE_EXPANDED) &
                             wxTREE_STATE_EXPANDED) != 0;
    }

      // bounding rect
    bool GetItemRect(long item, wxRect& rect, bool textOnly = false) const;
      //
    wxTextCtrl* GetEditControl() const;

    // operations
    // ----------
      // adding/deleting items
    bool DeleteItem(long item);

#if WXWIN_COMPATIBILITY_2_6
    wxDEPRECATED( long InsertItem(long parent, wxTreeItem& info,
                  long insertAfter = wxTREE_INSERT_LAST) );
        // If image > -1 and selImage == -1, the same image is used for
        // both selected and unselected items.
    wxDEPRECATED( long InsertItem(long parent, const wxString& label,
                                  int image = -1, int selImage = -1,
                                  long insertAfter = wxTREE_INSERT_LAST) );

        // use Expand, Collapse, CollapseAndReset or Toggle
    wxDEPRECATED( bool ExpandItem(long item, int action) );
    wxDEPRECATED( void SetImageList(wxImageList *imageList, int which = wxIMAGE_LIST_NORMAL) );
#endif // WXWIN_COMPATIBILITY_2_6

      // changing item state
    bool ExpandItem(long item)   { return ExpandItem(item, wxTREE_EXPAND_EXPAND);   }
    bool CollapseItem(long item) { return ExpandItem(item, wxTREE_EXPAND_COLLAPSE); }
    bool ToggleItem(long item)   { return ExpandItem(item, wxTREE_EXPAND_TOGGLE);   }

      //
    bool SelectItem(long item);
    bool ScrollTo(long item);
    bool DeleteAllItems();

    // Edit the label (tree must have the focus)
    wxTextCtrl* EditLabel(long item, wxClassInfo* textControlClass = wxCLASSINFO(wxTextCtrl));

    // End label editing, optionally cancelling the edit
    bool EndEditLabel(bool cancel);

    long HitTest(const wxPoint& point, int& flags);
    //  wxImageList *CreateDragImage(long item);
    bool SortChildren(long item);
    bool EnsureVisible(long item);

    void Command(wxCommandEvent& event) { ProcessCommand(event); }

protected:
    wxTextCtrl*  m_textCtrl;
    wxImageList* m_imageListNormal;
    wxImageList* m_imageListState;

    DECLARE_DYNAMIC_CLASS(wxTreeCtrl)
};

/*
 wxEVT_TREE_BEGIN_DRAG,
 wxEVT_TREE_BEGIN_RDRAG,
 wxEVT_TREE_BEGIN_LABEL_EDIT,
 wxEVT_TREE_END_LABEL_EDIT,
 wxEVT_TREE_DELETE_ITEM,
 wxEVT_TREE_GET_INFO,
 wxEVT_TREE_SET_INFO,
 wxEVT_TREE_ITEM_EXPANDED,
 wxEVT_TREE_ITEM_EXPANDING,
 wxEVT_TREE_ITEM_COLLAPSED,
 wxEVT_TREE_ITEM_COLLAPSING,
 wxEVT_TREE_SEL_CHANGED,
 wxEVT_TREE_SEL_CHANGING,
 wxEVT_TREE_KEY_DOWN
*/

class WXDLLIMPEXP_CORE wxTreeEvent: public wxCommandEvent
{
    DECLARE_DYNAMIC_CLASS(wxTreeEvent)

public:
    wxTreeEvent(wxEventType commandType = wxEVT_NULL, int id = 0);

    int           m_code;
    wxTreeItem    m_item;
    long          m_oldItem;
    wxPoint       m_pointDrag;

    inline long GetOldItem() const { return m_oldItem; }
    inline wxTreeItem& GetItem() const { return (wxTreeItem&) m_item; }
    inline wxPoint GetPoint() const { return m_pointDrag; }
    inline int GetCode() const { return m_code; }
};

typedef void (wxEvtHandler::*wxTreeEventFunction)(wxTreeEvent&);

#define EVT_TREE_BEGIN_DRAG(id, fn) { wxEVT_TREE_BEGIN_DRAG, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_BEGIN_RDRAG(id, fn) { wxEVT_TREE_BEGIN_RDRAG, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_BEGIN_LABEL_EDIT(id, fn) { wxEVT_TREE_BEGIN_LABEL_EDIT, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_END_LABEL_EDIT(id, fn) { wxEVT_TREE_END_LABEL_EDIT, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_DELETE_ITEM(id, fn) { wxEVT_TREE_DELETE_ITEM, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_GET_INFO(id, fn) { wxEVT_TREE_GET_INFO, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_SET_INFO(id, fn) { wxEVT_TREE_SET_INFO, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_ITEM_EXPANDED(id, fn) { wxEVT_TREE_ITEM_EXPANDED, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_ITEM_EXPANDING(id, fn) { wxEVT_TREE_ITEM_EXPANDING, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_ITEM_COLLAPSED(id, fn) { wxEVT_TREE_ITEM_COLLAPSED, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_ITEM_COLLAPSING(id, fn) { wxEVT_TREE_ITEM_COLLAPSING, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_SEL_CHANGED(id, fn) { wxEVT_TREE_SEL_CHANGED, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_SEL_CHANGING(id, fn) { wxEVT_TREE_SEL_CHANGING, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },
#define EVT_TREE_KEY_DOWN(id, fn) { wxEVT_TREE_KEY_DOWN, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxTreeEventFunction) & fn, NULL },

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_TREE_BEGIN_DRAG         wxEVT_TREE_BEGIN_DRAG
#define wxEVT_COMMAND_TREE_BEGIN_RDRAG        wxEVT_TREE_BEGIN_RDRAG
#define wxEVT_COMMAND_TREE_BEGIN_LABEL_EDIT   wxEVT_TREE_BEGIN_LABEL_EDIT
#define wxEVT_COMMAND_TREE_END_LABEL_EDIT     wxEVT_TREE_END_LABEL_EDIT
#define wxEVT_COMMAND_TREE_DELETE_ITEM        wxEVT_TREE_DELETE_ITEM
#define wxEVT_COMMAND_TREE_GET_INFO           wxEVT_TREE_GET_INFO
#define wxEVT_COMMAND_TREE_SET_INFO           wxEVT_TREE_SET_INFO
#define wxEVT_COMMAND_TREE_ITEM_EXPANDED      wxEVT_TREE_ITEM_EXPANDED
#define wxEVT_COMMAND_TREE_ITEM_EXPANDING     wxEVT_TREE_ITEM_EXPANDING
#define wxEVT_COMMAND_TREE_ITEM_COLLAPSED     wxEVT_TREE_ITEM_COLLAPSED
#define wxEVT_COMMAND_TREE_ITEM_COLLAPSING    wxEVT_TREE_ITEM_COLLAPSING
#define wxEVT_COMMAND_TREE_SEL_CHANGED        wxEVT_TREE_SEL_CHANGED
#define wxEVT_COMMAND_TREE_SEL_CHANGING       wxEVT_TREE_SEL_CHANGING
#define wxEVT_COMMAND_TREE_KEY_DOWN           wxEVT_TREE_KEY_DOWN

#endif
    // _WX_TREECTRL_H_
