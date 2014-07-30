/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/treectrl.h
// Purpose:     wxTreeCtrl class
// Author:      Denis Pershin
// Modified by:
// Created:     08/08/98
// Copyright:   (c) Denis Pershin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TREECTRL_H_
#define _WX_TREECTRL_H_

#include "wx/textctrl.h"
#include "wx/imaglist.h"

#include <gtk/gtk.h>

// the type for "untyped" data
typedef long wxDataType;

// fwd decl
class  WXDLLIMPEXP_CORE wxImageList;
struct wxTreeViewItem;

// a callback function used for sorting tree items, it should return -1 if the
// first item precedes the second, +1 if the second precedes the first or 0 if
// they're equivalent
class WXDLLIMPEXP_FWD_CORE wxTreeItemData;
typedef int (*wxTreeItemCmpFunc)(wxTreeItemData *item1, wxTreeItemData *item2);

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// values for the `flags' parameter of wxTreeCtrl::HitTest() which determine
// where exactly the specified point is situated:
    // above the client area.
static const int wxTREE_HITTEST_ABOVE            = 0x0001;
    // below the client area.
static const int wxTREE_HITTEST_BELOW            = 0x0002;
    // in the client area but below the last item.
static const int wxTREE_HITTEST_NOWHERE          = 0x0004;
    // on the button associated with an item.
static const int wxTREE_HITTEST_ONITEMBUTTON     = 0x0010;
    // on the bitmap associated with an item.
static const int wxTREE_HITTEST_ONITEMICON       = 0x0020;
    // in the indentation associated with an item.
static const int wxTREE_HITTEST_ONITEMINDENT     = 0x0040;
    // on the label (string) associated with an item.
static const int wxTREE_HITTEST_ONITEMLABEL      = 0x0080;
    // in the area to the right of an item.
static const int wxTREE_HITTEST_ONITEMRIGHT      = 0x0100;
    // on the state icon for a tree view item that is in a user-defined state.
static const int wxTREE_HITTEST_ONITEMSTATEICON  = 0x0200;
    // to the right of the client area.
static const int wxTREE_HITTEST_TOLEFT           = 0x0400;
    // to the left of the client area.
static const int wxTREE_HITTEST_TORIGHT          = 0x0800;
    // anywhere on the item
static const int wxTREE_HITTEST_ONITEM  = wxTREE_HITTEST_ONITEMICON |
                                          wxTREE_HITTEST_ONITEMLABEL |
                                          wxTREE_HITTEST_ONITEMSTATEICON;

#if WXWIN_COMPATIBILITY_2_6
    // NB: all the following flags are for compatbility only and will be removed in
    //     next versions
    // flags for deprecated InsertItem() variant
    #define wxTREE_INSERT_FIRST 0xFFFF0001
    #define wxTREE_INSERT_LAST  0xFFFF0002
#endif

// ----------------------------------------------------------------------------
// wxTreeItemId identifies an element of the tree. In this implementation, it's
// just a trivial wrapper around GTK GtkTreeItem *. It's opaque for the
// application.
// ----------------------------------------------------------------------------
class WXDLLIMPEXP_CORE wxTreeItemId {
public:
  // ctors
  wxTreeItemId() { m_itemId = NULL; }

      // default copy ctor/assignment operator are ok for us

  // accessors
      // is this a valid tree item?
  bool IsOk() const { return m_itemId != NULL; }

  // conversion to/from either real (system-dependent) tree item id or
  // to "long" which used to be the type for tree item ids in previous
  // versions of wxWidgets

  // for wxTreeCtrl usage only
  wxTreeItemId(GtkTreeItem *itemId) { m_itemId = itemId; }
  operator GtkTreeItem *() const { return m_itemId; }
  void operator =(GtkTreeItem *item) { m_itemId = item; }

protected:
  GtkTreeItem *m_itemId;
};

// ----------------------------------------------------------------------------
// wxTreeItemData is some (arbitrary) user class associated with some item. The
// main advantage of having this class (compared to old untyped interface) is
// that wxTreeItemData's are destroyed automatically by the tree and, as this
// class has virtual dtor, it means that the memory will be automatically
// freed. OTOH, we don't just use wxObject instead of wxTreeItemData because
// the size of this class is critical: in any real application, each tree leaf
// will have wxTreeItemData associated with it and number of leaves may be
// quite big.
//
// Because the objects of this class are deleted by the tree, they should
// always be allocated on the heap!
// ----------------------------------------------------------------------------
class WXDLLIMPEXP_CORE wxTreeItemData : private wxTreeItemId {
public:
    // default ctor/copy ctor/assignment operator are ok

    // dtor is virtual and all the items are deleted by the tree control when
    // it's deleted, so you normally don't have to care about freeing memory
    // allocated in your wxTreeItemData-derived class
    virtual ~wxTreeItemData() { }

    // accessors: set/get the item associated with this node
    void SetId(const wxTreeItemId& id) { m_itemId = id; }
    const wxTreeItemId& GetId() const { return (wxTreeItemId&) m_itemId; }
};

class WXDLLIMPEXP_CORE wxTreeCtrl: public wxControl {
public:
  // creation
  // --------
  wxTreeCtrl() { Init(); }

  wxTreeCtrl(wxWindow *parent, wxWindowID id = wxID_ANY,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = "wxTreeCtrl") {
      Create(parent, id, pos, size, style, validator, name);
  }

  virtual ~wxTreeCtrl();

    bool Create(wxWindow *parent, wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = "wxTreeCtrl");

    // accessors
    // ---------

        // get the total number of items in the control
    virtual unsigned int GetCount() const;

        // indent is the number of pixels the children are indented relative to
        // the parents position. SetIndent() also redraws the control
        // immediately.
    unsigned int GetIndent() const;
    void SetIndent(unsigned int indent);

        // image list: these functions allow to associate an image list with
        // the control and retrieve it. Note that the control does _not_ delete
        // the associated image list when it's deleted in order to allow image
        // lists to be shared between different controls.
        //
        // The normal image list is for the icons which correspond to the
        // normal tree item state (whether it is selected or not).
        // Additionally, the application might choose to show a state icon
        // which corresponds to an app-defined item state (for example,
        // checked/unchecked) which are taken from the state image list.
    wxImageList *GetImageList() const;
    wxImageList *GetStateImageList() const;

    void SetImageList(wxImageList *imageList);
    void SetStateImageList(wxImageList *imageList);

    // Functions to work with tree ctrl items. Unfortunately, they can _not_ be
    // member functions of wxTreeItem because they must know the tree the item
    // belongs to for Windows implementation and storing the pointer to
    // wxTreeCtrl in each wxTreeItem is just too much waste.

    // accessors
    // ---------

        // retrieve items label
    wxString GetItemText(const wxTreeItemId& item) const;
        // get the normal item image
    int GetItemImage(const wxTreeItemId& item) const;
        // get the data associated with the item
    wxTreeItemData *GetItemData(const wxTreeItemId& item) const;

    // modifiers
    // ---------

        // set items label
    void SetItemText(const wxTreeItemId& item, const wxString& text);
        // set the normal item image
    void SetItemImage(const wxTreeItemId& item, int image);
        // associate some data with the item
    void SetItemData(const wxTreeItemId& item, wxTreeItemData *data);

    // item status inquiries
    // ---------------------

        // is the item visible (it might be outside the view or not expanded)?
    bool IsVisible(const wxTreeItemId& item) const;
        // does the item has any children?
    bool ItemHasChildren(const wxTreeItemId& item) const;
        // is the item expanded (only makes sense if HasChildren())?
    bool IsExpanded(const wxTreeItemId& item) const;
        // is this item currently selected (the same as has focus)?
    bool IsSelected(const wxTreeItemId& item) const;

    // number of children
    // ------------------

        // if 'recursively' is false, only immediate children count, otherwise
        // the returned number is the number of all items in this branch
    size_t GetChildrenCount(const wxTreeItemId& item, bool recursively = true);

    // navigation
    // ----------

    // wxTreeItemId.IsOk() will return false if there is no such item

        // get the root tree item
    wxTreeItemId GetRootItem() const;

        // get the item currently selected (may return NULL if no selection)
    wxTreeItemId GetSelection() const;

        // get the parent of this item (may return NULL if root)
    wxTreeItemId GetItemParent(const wxTreeItemId& item) const;

        // for this enumeration function you must pass in a "cookie" parameter
        // which is opaque for the application but is necessary for the library
        // to make these functions reentrant (i.e. allow more than one
        // enumeration on one and the same object simultaneously). Of course,
        // the "cookie" passed to GetFirstChild() and GetNextChild() should be
        // the same!

        // get the last child of this item - this method doesn't use cookies
    wxTreeItemId GetLastChild(const wxTreeItemId& item) const;

        // get the next sibling of this item
    wxTreeItemId GetNextSibling(const wxTreeItemId& item) const;
        // get the previous sibling
    wxTreeItemId GetPrevSibling(const wxTreeItemId& item) const;

        // get first visible item
    wxTreeItemId GetFirstVisibleItem() const;
        // get the next visible item: item must be visible itself!
        // see IsVisible() and wxTreeCtrl::GetFirstVisibleItem()
    wxTreeItemId GetNextVisible(const wxTreeItemId& item) const;
        // get the previous visible item: item must be visible itself!
    wxTreeItemId GetPrevVisible(const wxTreeItemId& item) const;

    // operations
    // ----------

        // add the root node to the tree
    wxTreeItemId AddRoot(const wxString& text,
                         int image = -1, int selectedImage = -1,
                         wxTreeItemData *data = NULL);

        // insert a new item in as the first child of the parent
    wxTreeItemId PrependItem(const wxTreeItemId& parent,
                             const wxString& text,
                             int image = -1, int selectedImage = -1,
                             wxTreeItemData *data = NULL);

        // insert a new item after a given one
    wxTreeItemId InsertItem(const wxTreeItemId& parent,
                            const wxTreeItemId& idPrevious,
                            const wxString& text,
                            int image = -1, int selectedImage = -1,
                            wxTreeItemData *data = NULL);

        // insert a new item in as the last child of the parent
    wxTreeItemId AppendItem(const wxTreeItemId& parent,
                            const wxString& text,
                            int image = -1, int selectedImage = -1,
                            wxTreeItemData *data = NULL);

        // delete this item and associated data if any
    void Delete(const wxTreeItemId& item);
        // delete all items from the tree
    void DeleteAllItems();

        // expand this item
    void Expand(const wxTreeItemId& item);
        // collapse the item without removing its children
    void Collapse(const wxTreeItemId& item);
        // collapse the item and remove all children
    void CollapseAndReset(const wxTreeItemId& item);
        // toggles the current state
    void Toggle(const wxTreeItemId& item);

        // remove the selection from currently selected item (if any)
    void Unselect();
        // select this item
    void SelectItem(const wxTreeItemId& item);
        // make sure this item is visible (expanding the parent item and/or
        // scrolling to this item if necessary)
    void EnsureVisible(const wxTreeItemId& item);
        // scroll to this item (but don't expand its parent)
    void ScrollTo(const wxTreeItemId& item);

        // start editing the item label: this (temporarily) replaces the item
        // with a one line edit control. The item will be selected if it hadn't
        // been before. textCtrlClass parameter allows you to create an edit
        // control of arbitrary user-defined class deriving from wxTextCtrl.
    wxTextCtrl* EditLabel(const wxTreeItemId& item,
                          wxClassInfo* textCtrlClass = wxCLASSINFO(wxTextCtrl));
        // returns the same pointer as StartEdit() if the item is being edited,
        // NULL otherwise (it's assumed that no more than one item may be
        // edited simultaneously)
    wxTextCtrl* GetEditControl() const;
        // end editing and accept or discard the changes to item label
    void EndEditLabel(const wxTreeItemId& item, bool discardChanges = false);

        // sort the children of this item using the specified callback function
        // (it should return -1, 0 or +1 as usual), if it's not specified
        // alphabetical comparaison is performed.
        //
        // NB: this function is not reentrant!
    void SortChildren(const wxTreeItemId& item,
                      wxTreeItemCmpFunc *cmpFunction = NULL);

    // deprecated
    // ----------

#if WXWIN_COMPATIBILITY_2_6
    // these methods are deprecated and will be removed in future versions of
    // wxWidgets, they're here for compatibility only, don't use them in new
    // code (the comments indicate why these methods are now useless and how to
    // replace them)

        // use Expand, Collapse, CollapseAndReset or Toggle
    wxDEPRECATED( void ExpandItem(const wxTreeItemId& item, int action) );

        // use SetImageList
    wxDEPRECATED( void SetImageList(wxImageList *imageList, int) )
        { SetImageList(imageList); }

        // use Set/GetItemImage directly
    wxDEPRECATED( int GetItemSelectedImage(const wxTreeItemId& item) const );
    wxDEPRECATED( void SetItemSelectedImage(const wxTreeItemId& item, int image) );

        // get the first child of this item
    wxDEPRECATED( wxTreeItemId GetFirstChild(const wxTreeItemId& item, long& cookie) const );
        // get the next child (after GetFirstChild or GetNextChild)
    wxDEPRECATED( wxTreeItemId GetNextChild(const wxTreeItemId& item, long& cookie) const );

        // use AddRoot, PrependItem or AppendItem
    wxDEPRECATED( wxTreeItemId InsertItem(const wxTreeItemId& parent,
                                          const wxString& text,
                                          int image = -1, int selImage = -1,
                                          long insertAfter = wxTREE_INSERT_LAST) );

#endif // WXWIN_COMPATIBILITY_2_6

        // use Set/GetImageList and Set/GetStateImageList
    wxImageList *GetImageList(int) const
        { return GetImageList(); }

    void SendExpanding(const wxTreeItemId& item);
    void SendExpanded(const wxTreeItemId& item);
    void SendCollapsing(const wxTreeItemId& item);
    void SendCollapsed(const wxTreeItemId& item);
    void SendSelChanging(const wxTreeItemId& item);
    void SendSelChanged(const wxTreeItemId& item);

protected:

    wxTreeItemId m_editItem;
    GtkTree *m_tree;
    GtkTreeItem *m_anchor;
    wxTextCtrl*  m_textCtrl;
    wxImageList* m_imageListNormal;
    wxImageList* m_imageListState;

    long m_curitemId;

    void SendMessage(wxEventType command, const wxTreeItemId& item);
//  GtkTreeItem *findGtkTreeItem(wxTreeCtrlId &id) const;

    // the common part of all ctors
    void Init();
    // insert a new item in as the last child of the parent
    wxTreeItemId p_InsertItem(GtkTreeItem *p,
                              const wxString& text,
                              int image, int selectedImage,
                              wxTreeItemData *data);


    DECLARE_DYNAMIC_CLASS(wxTreeCtrl)
};

#endif
    // _WX_TREECTRL_H_
