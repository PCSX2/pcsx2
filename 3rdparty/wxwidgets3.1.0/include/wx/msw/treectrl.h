/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/treectrl.h
// Purpose:     wxTreeCtrl class
// Author:      Julian Smart
// Modified by: Vadim Zeitlin to be less MSW-specific on 10/10/98
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_TREECTRL_H_
#define _WX_MSW_TREECTRL_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#if wxUSE_TREECTRL

#include "wx/textctrl.h"
#include "wx/dynarray.h"
#include "wx/treebase.h"
#include "wx/hashmap.h"

#ifdef __GNUWIN32__
    // Cygwin windows.h defines these identifiers
    #undef GetFirstChild
    #undef GetNextSibling
#endif // Cygwin

// fwd decl
class  WXDLLIMPEXP_FWD_CORE wxImageList;
class  WXDLLIMPEXP_FWD_CORE wxDragImage;
struct WXDLLIMPEXP_FWD_CORE wxTreeViewItem;

// hash storing attributes for our items
WX_DECLARE_EXPORTED_VOIDPTR_HASH_MAP(wxTreeItemAttr *, wxMapTreeAttr);

// ----------------------------------------------------------------------------
// wxTreeCtrl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTreeCtrl : public wxTreeCtrlBase
{
public:
    // creation
    // --------
    wxTreeCtrl() { Init(); }

    wxTreeCtrl(wxWindow *parent, wxWindowID id = wxID_ANY,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxTreeCtrlNameStr)
    {
        Create(parent, id, pos, size, style, validator, name);
    }

    virtual ~wxTreeCtrl();

    bool Create(wxWindow *parent, wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxTreeCtrlNameStr);

    // implement base class pure virtuals
    // ----------------------------------

    virtual unsigned int GetCount() const;

    virtual unsigned int GetIndent() const;
    virtual void SetIndent(unsigned int indent);

    virtual void SetImageList(wxImageList *imageList);
    virtual void SetStateImageList(wxImageList *imageList);

    virtual wxString GetItemText(const wxTreeItemId& item) const;
    virtual int GetItemImage(const wxTreeItemId& item,
                        wxTreeItemIcon which = wxTreeItemIcon_Normal) const;
    virtual wxTreeItemData *GetItemData(const wxTreeItemId& item) const;
    virtual wxColour GetItemTextColour(const wxTreeItemId& item) const;
    virtual wxColour GetItemBackgroundColour(const wxTreeItemId& item) const;
    virtual wxFont GetItemFont(const wxTreeItemId& item) const;

    virtual void SetItemText(const wxTreeItemId& item, const wxString& text);
    virtual void SetItemImage(const wxTreeItemId& item, int image,
                      wxTreeItemIcon which = wxTreeItemIcon_Normal);
    virtual void SetItemData(const wxTreeItemId& item, wxTreeItemData *data);
    virtual void SetItemHasChildren(const wxTreeItemId& item, bool has = true);
    virtual void SetItemBold(const wxTreeItemId& item, bool bold = true);
    virtual void SetItemDropHighlight(const wxTreeItemId& item,
                                      bool highlight = true);
    virtual void SetItemTextColour(const wxTreeItemId& item,
                                   const wxColour& col);
    virtual void SetItemBackgroundColour(const wxTreeItemId& item,
                                         const wxColour& col);
    virtual void SetItemFont(const wxTreeItemId& item, const wxFont& font);

    // item status inquiries
    // ---------------------

    virtual bool IsVisible(const wxTreeItemId& item) const;
    virtual bool ItemHasChildren(const wxTreeItemId& item) const;
    virtual bool IsExpanded(const wxTreeItemId& item) const;
    virtual bool IsSelected(const wxTreeItemId& item) const;
    virtual bool IsBold(const wxTreeItemId& item) const;

    virtual size_t GetChildrenCount(const wxTreeItemId& item,
                                    bool recursively = true) const;

    // navigation
    // ----------

    virtual wxTreeItemId GetRootItem() const;
    virtual wxTreeItemId GetSelection() const;
    virtual size_t GetSelections(wxArrayTreeItemIds& selections) const;
    virtual wxTreeItemId GetFocusedItem() const;

    virtual void ClearFocusedItem();
    virtual void SetFocusedItem(const wxTreeItemId& item);


    virtual wxTreeItemId GetItemParent(const wxTreeItemId& item) const;
    virtual wxTreeItemId GetFirstChild(const wxTreeItemId& item,
                                       wxTreeItemIdValue& cookie) const;
    virtual wxTreeItemId GetNextChild(const wxTreeItemId& item,
                                      wxTreeItemIdValue& cookie) const;
    virtual wxTreeItemId GetLastChild(const wxTreeItemId& item) const;

    virtual wxTreeItemId GetNextSibling(const wxTreeItemId& item) const;
    virtual wxTreeItemId GetPrevSibling(const wxTreeItemId& item) const;

    virtual wxTreeItemId GetFirstVisibleItem() const;
    virtual wxTreeItemId GetNextVisible(const wxTreeItemId& item) const;
    virtual wxTreeItemId GetPrevVisible(const wxTreeItemId& item) const;

    // operations
    // ----------

    virtual wxTreeItemId AddRoot(const wxString& text,
                         int image = -1, int selectedImage = -1,
                         wxTreeItemData *data = NULL);

    virtual void Delete(const wxTreeItemId& item);
    virtual void DeleteChildren(const wxTreeItemId& item);
    virtual void DeleteAllItems();

    virtual void Expand(const wxTreeItemId& item);
    virtual void Collapse(const wxTreeItemId& item);
    virtual void CollapseAndReset(const wxTreeItemId& item);
    virtual void Toggle(const wxTreeItemId& item);

    virtual void Unselect();
    virtual void UnselectAll();
    virtual void SelectItem(const wxTreeItemId& item, bool select = true);
    virtual void SelectChildren(const wxTreeItemId& parent);

    virtual void EnsureVisible(const wxTreeItemId& item);
    virtual void ScrollTo(const wxTreeItemId& item);

    virtual wxTextCtrl *EditLabel(const wxTreeItemId& item,
                          wxClassInfo* textCtrlClass = wxCLASSINFO(wxTextCtrl));
    virtual wxTextCtrl *GetEditControl() const;
    virtual void EndEditLabel(const wxTreeItemId& WXUNUSED(item),
                              bool discardChanges = false)
    {
        DoEndEditLabel(discardChanges);
    }

    virtual void SortChildren(const wxTreeItemId& item);

    virtual bool GetBoundingRect(const wxTreeItemId& item,
                                 wxRect& rect,
                                 bool textOnly = false) const;

    // implementation
    // --------------

    virtual wxVisualAttributes GetDefaultAttributes() const
    {
        return GetClassDefaultAttributes(GetWindowVariant());
    }

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);


    virtual WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
    virtual WXLRESULT MSWDefWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
    virtual bool MSWCommand(WXUINT param, WXWORD id);
    virtual bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result);
    virtual bool MSWShouldPreProcessMessage(WXMSG* msg);

    // override some base class virtuals
    virtual bool SetBackgroundColour(const wxColour &colour);
    virtual bool SetForegroundColour(const wxColour &colour);

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

protected:
    // Implement "update locking" in a custom way for this control.
    virtual void DoFreeze();
    virtual void DoThaw();

    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);

    // SetImageList helper
    void SetAnyImageList(wxImageList *imageList, int which);

    // refresh a single item
    void RefreshItem(const wxTreeItemId& item);

    // end edit label
    void DoEndEditLabel(bool discardChanges = false);

    virtual int DoGetItemState(const wxTreeItemId& item) const;
    virtual void DoSetItemState(const wxTreeItemId& item, int state);

    virtual wxTreeItemId DoInsertItem(const wxTreeItemId& parent,
                                      size_t pos,
                                      const wxString& text,
                                      int image, int selectedImage,
                                      wxTreeItemData *data);
    virtual wxTreeItemId DoInsertAfter(const wxTreeItemId& parent,
                                       const wxTreeItemId& idPrevious,
                                       const wxString& text,
                                       int image = -1, int selImage = -1,
                                       wxTreeItemData *data = NULL);
    virtual wxTreeItemId DoTreeHitTest(const wxPoint& point, int& flags) const;

    // obtain the user data for the lParam member of TV_ITEM
    class wxTreeItemParam *GetItemParam(const wxTreeItemId& item) const;

    // update the event to include the items client data and pass it to
    // HandleWindowEvent(), return true if it processed it
    bool HandleTreeEvent(wxTreeEvent& event) const;

    // pass the event to HandleTreeEvent() and return true if the event was
    // either unprocessed or not vetoed
    bool IsTreeEventAllowed(wxTreeEvent& event) const
    {
        return !HandleTreeEvent(event) || event.IsAllowed();
    }

    // generate a wxEVT_KEY_DOWN event from the specified WPARAM/LPARAM values
    // having the same meaning as for WM_KEYDOWN, return true if it was
    // processed
    bool MSWHandleTreeKeyDownEvent(WXWPARAM wParam, WXLPARAM lParam);

    // handle a key event in a multi-selection control, should be only called
    // for keys which can be used to change the selection
    //
    // return true if the key was processed, false otherwise
    bool MSWHandleSelectionKey(unsigned vkey);


    // data used only while editing the item label:
    wxTextCtrl  *m_textCtrl;        // text control in which it is edited
    wxTreeItemId m_idEdited;        // the item being edited

private:
    // the common part of all ctors
    void Init();

    // helper functions
    bool DoGetItem(wxTreeViewItem *tvItem) const;
    void DoSetItem(wxTreeViewItem *tvItem);

    void DoExpand(const wxTreeItemId& item, int flag);

    void DoSelectItem(const wxTreeItemId& item, bool select = true);
    void DoUnselectItem(const wxTreeItemId& item);
    void DoToggleItemSelection(const wxTreeItemId& item);

    void DoUnselectAll();
    void DoSelectChildren(const wxTreeItemId& parent);

    void DeleteTextCtrl();

    // return true if the item is the hidden root one (i.e. it's the root item
    // and the tree has wxTR_HIDE_ROOT style)
    bool IsHiddenRoot(const wxTreeItemId& item) const;


    // check if the given flags (taken from TV_HITTESTINFO structure)
    // indicate a position "on item": this is less trivial than just checking
    // for TVHT_ONITEM because we consider that points to the left and right of
    // item text are also "on item" when wxTR_FULL_ROW_HIGHLIGHT is used as the
    // item visually spans the entire breadth of the window then
    bool MSWIsOnItem(unsigned flags) const;

    // Delete the given item from the native control.
    bool MSWDeleteItem(const wxTreeItemId& item);


    // the hash storing the items attributes (indexed by item ids)
    wxMapTreeAttr m_attrs;

    // true if the hash above is not empty
    bool m_hasAnyAttr;

#if wxUSE_DRAGIMAGE
    // used for dragging
    wxDragImage *m_dragImage;
#endif

    // Virtual root item, if wxTR_HIDE_ROOT is set.
    void* m_pVirtualRoot;

    // the starting item for selection with Shift
    wxTreeItemId m_htSelStart, m_htClickedItem;
    wxPoint m_ptClick;

    // whether dragging has started
    bool m_dragStarted;

    // whether focus was lost between subsequent clicks of a single item
    bool m_focusLost;

    // set when we are changing selection ourselves (only used in multi
    // selection mode)
    bool m_changingSelection;

    // whether we need to trigger a state image click event
    bool m_triggerStateImageClick;

    // whether we need to deselect other items on mouse up
    bool m_mouseUpDeselect;

    // The size to restore the control to when it is thawed, see DoThaw().
    wxSize m_thawnSize;

    friend class wxTreeItemIndirectData;
    friend class wxTreeSortHelper;

    wxDECLARE_DYNAMIC_CLASS(wxTreeCtrl);
    wxDECLARE_NO_COPY_CLASS(wxTreeCtrl);
};

#endif // wxUSE_TREECTRL

#endif // _WX_MSW_TREECTRL_H_
