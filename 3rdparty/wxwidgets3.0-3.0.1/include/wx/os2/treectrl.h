/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/treectrl.h
// Purpose:     wxTreeCtrl class
// Author:      David Webster
// Modified by:
// Created:     01/23/03
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TREECTRL_H_
#define _WX_TREECTRL_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#if wxUSE_TREECTRL

#include "wx/textctrl.h"
#include "wx/dynarray.h"
#include "wx/treebase.h"
#include "wx/hashmap.h"

// the type for "untyped" data
typedef long wxDataType;

// fwd decl
class  WXDLLIMPEXP_CORE wxImageList;
class  WXDLLIMPEXP_CORE wxDragImage;
struct WXDLLIMPEXP_FWD_CORE wxTreeViewItem;

// a callback function used for sorting tree items, it should return -1 if the
// first item precedes the second, +1 if the second precedes the first or 0 if
// they're equivalent
class wxTreeItemData;

#if WXWIN_COMPATIBILITY_2_6
    // flags for deprecated InsertItem() variant
    #define wxTREE_INSERT_FIRST 0xFFFF0001
    #define wxTREE_INSERT_LAST  0xFFFF0002
#endif

// hash storing attributes for our items
WX_DECLARE_EXPORTED_VOIDPTR_HASH_MAP(wxTreeItemAttr *, wxMapTreeAttr);

// ----------------------------------------------------------------------------
// wxTreeCtrl
// ----------------------------------------------------------------------------
class WXDLLIMPEXP_CORE wxTreeCtrl : public wxControl
{
public:
    // creation
    // --------
    wxTreeCtrl() { Init(); }

    wxTreeCtrl( wxWindow*          pParent
               ,wxWindowID         vId = wxID_ANY
               ,const wxPoint&     rPos = wxDefaultPosition
               ,const wxSize&      rSize = wxDefaultSize
               ,long               lStyle = wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT
               ,const wxValidator& rValidator = wxDefaultValidator
               ,const wxString&    rsName = wxTreeCtrlNameStr
              )
    {
        Create( pParent
               ,vId
               ,rPos
               ,rSize
               ,lStyle
               ,rValidator
               ,rsName
              );
    }
    virtual ~wxTreeCtrl();

    bool Create( wxWindow*          pParent
                ,wxWindowID         vId = wxID_ANY
                ,const wxPoint&     rPos = wxDefaultPosition
                ,const wxSize&      rSize = wxDefaultSize
                ,long               lStyle = wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT
                ,const wxValidator& rValidator = wxDefaultValidator
                ,const wxString&    rsName = wxTreeCtrlNameStr
               );

    //
    // Accessors
    // ---------
    //

    //
    // Get the total number of items in the control
    //
    virtual unsigned int GetCount(void) const;

    //
    // Indent is the number of pixels the children are indented relative to
    // the parents position. SetIndent() also redraws the control
    // immediately.
    //
    unsigned int GetIndent(void) const;
    void         SetIndent(unsigned int uIndent);

    //
    // Spacing is the number of pixels between the start and the Text
    //
    unsigned int GetSpacing(void) const { return 18; } // return wxGTK default
    void SetSpacing(unsigned int uSpacing) { }

    //
    // Image list: these functions allow to associate an image list with
    // the control and retrieve it. Note that the control does _not_ delete
    // the associated image list when it's deleted in order to allow image
    // lists to be shared between different controls.
    //
    // OS/2 doesn't really use imagelists as MSW does, but since the MSW
    // control is the basis for this one, until I decide how to get rid of
    // the need for them they are here for now.
    //
    wxImageList* GetImageList(void) const;
    wxImageList* GetStateImageList(void) const;

    void         AssignImageList(wxImageList* pImageList);
    void         AssignStateImageList(wxImageList* pImageList);
    void         SetImageList(wxImageList* pImageList);
    void         SetStateImageList(wxImageList* pImageList);

    //
    // Functions to work with tree ctrl items. Unfortunately, they can _not_ be
    // member functions of wxTreeItem because they must know the tree the item
    // belongs to for Windows implementation and storing the pointer to
    // wxTreeCtrl in each wxTreeItem is just too much waste.

    //
    // Item's label
    //
    wxString GetItemText(const wxTreeItemId& rItem) const;
    void     SetItemText( const wxTreeItemId& rItem
                         ,const wxString&     rsText
                        );

    //
    // One of the images associated with the item (normal by default)
    //
    int  GetItemImage( const wxTreeItemId& rItem
                      ,wxTreeItemIcon      vWhich = wxTreeItemIcon_Normal
                     ) const;
    void SetItemImage( const wxTreeItemId& rItem
                      ,int                 nImage
                      ,wxTreeItemIcon      vWhich = wxTreeItemIcon_Normal
                     );

    //
    // Data associated with the item
    //
    wxTreeItemData* GetItemData(const wxTreeItemId& rItem) const;
    void            SetItemData( const wxTreeItemId& rItem
                                ,wxTreeItemData*     pData
                               );

    //
    // Item's text colour
    //
    wxColour GetItemTextColour(const wxTreeItemId& rItem) const;
    void     SetItemTextColour( const wxTreeItemId& rItem
                               ,const wxColour&     rColor
                              );

    //
    // Item's background colour
    //
    wxColour GetItemBackgroundColour(const wxTreeItemId& rItem) const;
    void     SetItemBackgroundColour( const wxTreeItemId& rItem
                                     ,const wxColour&     rColour
                                    );

    //
    // Item's font
    //
    wxFont GetItemFont(const wxTreeItemId& rItem) const;
    void   SetItemFont( const wxTreeItemId& rItem
                       ,const wxFont&       rFont
                      );

    //
    // Force appearance of [+] button near the item. This is useful to
    // allow the user to expand the items which don't have any children now
    // - but instead add them only when needed, thus minimizing memory
    // usage and loading time.
    //
    void SetItemHasChildren( const wxTreeItemId& rItem
                            ,bool                bHas = true
                           );

    //
    // The item will be shown in bold
    //
    void SetItemBold( const wxTreeItemId& rItem
                     ,bool                bBold = true
                    );

    //
    // The item will be shown with a drop highlight
    //
    void SetItemDropHighlight( const wxTreeItemId& rItem
                              ,bool                bHighlight = true
                             );

    //
    // Item status inquiries
    // ---------------------
    //

    //
    // Is the item visible (it might be outside the view or not expanded)?
    //
    bool IsVisible(const wxTreeItemId& rItem) const;

    //
    // Does the item has any children?
    //
    bool ItemHasChildren(const wxTreeItemId& rItem) const;

    //
    // Is the item expanded (only makes sense if HasChildren())?
    //
    bool IsExpanded(const wxTreeItemId& rItem) const;

    //
    // Is this item currently selected (the same as has focus)?
    //
    bool IsSelected(const wxTreeItemId& rItem) const;

    //
    // Is item text in bold font?
    //
    bool IsBold(const wxTreeItemId& rItem) const;

    //
    // Number of children
    // ------------------
    //

    //
    // If 'bRecursively' is false, only immediate children count, otherwise
    // the returned number is the number of all items in this branch
    //
    size_t GetChildrenCount( const wxTreeItemId& rItem
                            ,bool                bRecursively = true
                           ) const;

    //
    // Navigation
    // ----------
    //

    //
    // Get the root tree item
    //
    wxTreeItemId GetRootItem(void) const;

    //
    // Get the item currently selected (may return NULL if no selection)
    //
    wxTreeItemId GetSelection(void) const;

    //
    // Get the items currently selected, return the number of such item
    //
    size_t GetSelections(wxArrayTreeItemIds& rSelections) const;

    //
    // Get the parent of this item (may return NULL if root)
    //
    wxTreeItemId GetItemParent(const wxTreeItemId& rItem) const;

        // for this enumeration function you must pass in a "cookie" parameter
        // which is opaque for the application but is necessary for the library
        // to make these functions reentrant (i.e. allow more than one
        // enumeration on one and the same object simultaneously). Of course,
        // the "cookie" passed to GetFirstChild() and GetNextChild() should be
        // the same!

        // get the first child of this item
    wxTreeItemId GetFirstChild(const wxTreeItemId& item,
                               wxTreeItemIdValue& cookie) const;
        // get the next child
    wxTreeItemId GetNextChild(const wxTreeItemId& item,
                              wxTreeItemIdValue& cookie) const;

    //
    // Get the last child of this item - this method doesn't use cookies
    //
    wxTreeItemId GetLastChild(const wxTreeItemId& rItem) const;

    //
    // Get the next sibling of this item
    //
    wxTreeItemId GetNextSibling(const wxTreeItemId& rItem) const;

    //
    // Get the previous sibling
    //
    wxTreeItemId GetPrevSibling(const wxTreeItemId& rItem) const;

    //
    // Get first visible item
    //
    wxTreeItemId GetFirstVisibleItem(void) const;

    //
    // Get the next visible item: item must be visible itself!
    // see IsVisible() and wxTreeCtrl::GetFirstVisibleItem()
    //
    wxTreeItemId GetNextVisible(const wxTreeItemId& rItem) const;

    //
    // Get the previous visible item: item must be visible itself!
    //
    wxTreeItemId GetPrevVisible(const wxTreeItemId& rItem) const;

    //
    // Operations
    // ----------
    //

    //
    // Add the root node to the tree
    //
    wxTreeItemId AddRoot( const wxString& rsText
                         ,int             nImage = -1
                         ,int             nSelectedImage = -1
                         ,wxTreeItemData* pData = NULL
                        );

    //
    // Insert a new item in as the first child of the parent
    //
    wxTreeItemId PrependItem( const wxTreeItemId& rParent
                             ,const wxString&     rsText
                             ,int                 nImage = -1
                             ,int                 nSelectedImage = -1
                             ,wxTreeItemData*     pData = NULL
                            );

    //
    // Insert a new item after a given one
    //
    wxTreeItemId InsertItem( const wxTreeItemId& rParent
                            ,const wxTreeItemId& rIdPrevious
                            ,const wxString&     rsText
                            ,int                 nImage = -1
                            ,int                 nSelectedImage = -1
                            ,wxTreeItemData*     pData = NULL
                           );

    //
    // Insert a new item before the one with the given index
    //
    wxTreeItemId InsertItem( const wxTreeItemId& pParent
                            ,size_t              nIndex
                            ,const wxString&     rsText
                            ,int                 nImage = -1
                            ,int                 nSelectedImage = -1
                            ,wxTreeItemData*     pData = NULL
                           );

    //
    // Insert a new item in as the last child of the parent
    //
    wxTreeItemId AppendItem( const wxTreeItemId& rParent
                            ,const wxString&     rsText
                            ,int                 nImage = -1
                            ,int                 nSelectedImage = -1
                            ,wxTreeItemData*     pData = NULL
                           );

    //
    // Delete this item and associated data if any
    //
    void Delete(const wxTreeItemId& rItem);

    //
    // Delete all children (but don't delete the item itself)
    //
    void DeleteChildren(const wxTreeItemId& rItem);

    //
    // Delete all items from the tree
    //
    void DeleteAllItems(void);

    //
    // Expand this item
    //
    void Expand(const wxTreeItemId& rItem);

    //
    // Collapse the item without removing its children
    //
    void Collapse(const wxTreeItemId& rItem);

    //
    // Collapse the item and remove all children
    //
    void CollapseAndReset(const wxTreeItemId& rItem);

    //
    // Toggles the current state
    //
    void Toggle(const wxTreeItemId& rItem);

    //
    // Remove the selection from currently selected item (if any)
    //
    void Unselect(void);

    //
    // Unselect all items (only makes sense for multiple selection control)
    //
    void UnselectAll(void);

    //
    // Select this item
    //
    void SelectItem(const wxTreeItemId& rItem);

    //
    // Make sure this item is visible (expanding the parent item and/or
    // scrolling to this item if necessary)
    //
    void EnsureVisible(const wxTreeItemId& rItem);

    //
    // Scroll to this item (but don't expand its parent)
    //
    void ScrollTo(const wxTreeItemId& rItem);

    //
    // OS/2 does not use a separate edit field for editting text. Here for
    // interface compatibility, only.
    //
    wxTextCtrl* EditLabel( const wxTreeItemId& rItem
                          ,wxClassInfo*        pTextCtrlClass = wxCLASSINFO(wxTextCtrl)
                         );

    //
    // returns NULL for OS/2 in ALL cases
    //
    wxTextCtrl* GetEditControl(void) const {return NULL;}

    //
    // End editing and accept or discard the changes to item label
    //
    void EndEditLabel( const wxTreeItemId& rItem
                      ,bool                bDiscardChanges = false
                     );

    //
    // Sorting
    // -------
    //

    //
    // This function is called to compare 2 items and should return -1, 0
    // or +1 if the first item is less than, equal to or greater than the
    // second one. The base class version performs alphabetic comparaison
    // of item labels (GetText)
    //
    virtual int OnCompareItems( const wxTreeItemId& rItem1
                               ,const wxTreeItemId& rItem2
                              );

    //
    // Sort the children of this item using OnCompareItems
    //
    void SortChildren(const wxTreeItemId& rItem);

    //
    // Helpers
    // -------
    //

    //
    // Determine to which item (if any) belongs the given point (the
    // coordinates specified are relative to the client area of tree ctrl)
    // and fill the flags parameter with a bitmask of wxTREE_HITTEST_xxx
    // constants.

    //
    // The first function is more portable (because easier to implement
    // on other platforms), but the second one returns some extra info.
    //
    wxTreeItemId HitTest(const wxPoint& rPoint)
        { int nDummy = 0; return HitTest(rPoint, nDummy); }
    wxTreeItemId HitTest( const wxPoint& rPoint
                         ,int&           rFlags
                        );

    //
    // Get the bounding rectangle of the item (or of its label only)
    //
    bool GetBoundingRect( const wxTreeItemId& rItem
                         ,wxRect&             rRect
                         ,bool                bTextOnly = false
                        ) const;

    //
    // Implementation
    // --------------
    //

    virtual MRESULT OS2WindowProc( WXUINT   uMsg
                                  ,WXWPARAM wParam
                                  ,WXLPARAM lParam
                                 );
    virtual bool    OS2Command( WXUINT uParam
                               ,WXWORD wId
                              );
//    virtual bool    OMSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result);

    //
    // Override some base class virtuals
    //
    virtual bool SetBackgroundColour(const wxColour& rColour);
    virtual bool SetForegroundColour(const wxColour& rColour);

    //
    // Get/set the check state for the item (only for wxTR_MULTIPLE)
    //
    bool IsItemChecked(const wxTreeItemId& rItem) const;
    void SetItemCheck( const wxTreeItemId& rItem
                      ,bool                bCheck = true
                     );

protected:
    //
    // SetImageList helper
    //
    void SetAnyImageList( wxImageList* pImageList
                         ,int          nWhich
                        );

    //
    // Refresh a single item
    //
    void RefreshItem(const wxTreeItemId& rItem);

    wxImageList*                    m_pImageListNormal; // images for tree elements
    wxImageList*                    m_pImageListState;  // special images for app defined states
    bool                            m_bOwnsImageListNormal;
    bool                            m_bOwnsImageListState;

private:

    //
    // The common part of all ctors
    //
    void Init(void);

    //
    // Helper functions
    //
    inline bool DoGetItem(wxTreeViewItem* pTvItem) const;
    inline void DoSetItem(wxTreeViewItem* pTvItem);

    inline void DoExpand( const wxTreeItemId& rItem
                         ,int                 nFlag
                        );
    wxTreeItemId DoInsertItem( const wxTreeItemId& pParent
                              ,wxTreeItemId        hInsertAfter
                              ,const wxString&     rsText
                              ,int                 nImage
                              ,int                 nSelectedImage
                              ,wxTreeItemData*     pData
                             );
    int  DoGetItemImageFromData( const wxTreeItemId& rItem
                                ,wxTreeItemIcon      vWhich
                               ) const;
    void DoSetItemImageFromData( const wxTreeItemId& rItem
                                ,int                 nImage
                                ,wxTreeItemIcon      vWhich
                               ) const;
    void DoSetItemImages( const wxTreeItemId& rItem
                         ,int                 nImage
                         ,int                 nImageSel
                        );
    void DeleteTextCtrl() { }

    //
    // support for additional item images which we implement using
    // wxTreeItemIndirectData technique - see the comments in msw/treectrl.cpp
    //
    void SetIndirectItemData( const wxTreeItemId&           rItem
                             ,class wxTreeItemIndirectData* pData
                            );
    bool HasIndirectData(const wxTreeItemId& rItem) const;
    bool IsDataIndirect(wxTreeItemData* pData) const
        { return pData && pData->GetId().m_pItem == 0; }

    //
    // The hash storing the items attributes (indexed by items ids)
    //
    wxMapTreeAttr                   m_vAttrs;

    //
    // true if the hash above is not empty
    //
    bool                            m_bHasAnyAttr;

    //
    // Used for dragging
    //
    wxDragImage*                    m_pDragImage;

    // Virtual root item, if wxTR_HIDE_ROOT is set.
//    void* m_pVirtualRoot;

    // the starting item for selection with Shift
//    WXHTREEITEM m_htSelStart;
//
    friend class wxTreeItemIndirectData;
    friend class wxTreeSortHelper;

    DECLARE_DYNAMIC_CLASS(wxTreeCtrl)
    wxDECLARE_NO_COPY_CLASS(wxTreeCtrl);
}; // end of CLASS wxTreeCtrl

#endif // wxUSE_TREECTRL

#endif
    // _WX_TREECTRL_H_
