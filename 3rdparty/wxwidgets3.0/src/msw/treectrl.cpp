/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/treectrl.cpp
// Purpose:     wxTreeCtrl
// Author:      Julian Smart
// Modified by: Vadim Zeitlin to be less MSW-specific on 10.10.98
// Created:     1997
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TREECTRL

#include "wx/treectrl.h"

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/msw/missing.h"
    #include "wx/dynarray.h"
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/settings.h"
#endif

#include "wx/dynlib.h"
#include "wx/msw/private.h"

#include "wx/imaglist.h"
#include "wx/msw/dragimag.h"
#include "wx/msw/uxtheme.h"

// macros to hide the cast ugliness
// --------------------------------

// get HTREEITEM from wxTreeItemId
#define HITEM(item)     ((HTREEITEM)(((item).m_pItem)))


// older SDKs are missing these
#ifndef TVN_ITEMCHANGINGA

#define TVN_ITEMCHANGINGA (TVN_FIRST-16)
#define TVN_ITEMCHANGINGW (TVN_FIRST-17)

typedef struct tagNMTVITEMCHANGE
{
    NMHDR hdr;
    UINT uChanged;
    HTREEITEM hItem;
    UINT uStateNew;
    UINT uStateOld;
    LPARAM lParam;
} NMTVITEMCHANGE;

#endif


// this helper class is used on vista systems for preventing unwanted
// item state changes in the vista tree control.  It is only effective in
// multi-select mode on vista systems.

// The vista tree control includes some new code that originally broke the
// multi-selection tree, causing seemingly spurious item selection state changes
// during Shift or Ctrl-click item selection. (To witness the original broken
// behaviour, simply make IsLocked() below always return false). This problem was
// solved by using the following class to 'unlock' an item's selection state.

class TreeItemUnlocker
{
public:
    // unlock a single item
    TreeItemUnlocker(HTREEITEM item)
    {
        m_oldUnlockedItem = ms_unlockedItem;
        ms_unlockedItem = item;
    }

    // unlock all items, don't use unless absolutely necessary
    TreeItemUnlocker()
    {
        m_oldUnlockedItem = ms_unlockedItem;
        ms_unlockedItem = (HTREEITEM)-1;
    }

    // lock everything back
    ~TreeItemUnlocker() { ms_unlockedItem = m_oldUnlockedItem; }


    // check if the item state is currently locked
    static bool IsLocked(HTREEITEM item)
        { return ms_unlockedItem != (HTREEITEM)-1 && item != ms_unlockedItem; }

private:
    static HTREEITEM ms_unlockedItem;
    HTREEITEM m_oldUnlockedItem;

    wxDECLARE_NO_COPY_CLASS(TreeItemUnlocker);
};

HTREEITEM TreeItemUnlocker::ms_unlockedItem = NULL;

// another helper class: set the variable to true during its lifetime and reset
// it to false when it is destroyed
//
// it is currently always used with wxTreeCtrl::m_changingSelection
class TempSetter
{
public:
    TempSetter(bool& var) : m_var(var)
    {
        wxASSERT_MSG( !m_var, "variable shouldn't be already set" );
        m_var = true;
    }

    ~TempSetter()
    {
        m_var = false;
    }

private:
    bool& m_var;

    wxDECLARE_NO_COPY_CLASS(TempSetter);
};

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

namespace
{

// Work around a problem with TreeView_GetItemRect() when using MinGW/Cygwin:
// it results in warnings about breaking strict aliasing rules because HITEM is
// passed via a RECT pointer, so use a union to avoid them and define our own
// version of the standard macro using it.
union TVGetItemRectParam
{
    RECT rect;
    HTREEITEM hItem;
};

inline bool
wxTreeView_GetItemRect(HWND hwnd,
                       HTREEITEM hItem,
                       TVGetItemRectParam& param,
                       BOOL fItemRect)
{
    param.hItem = hItem;
    return ::SendMessage(hwnd, TVM_GETITEMRECT, fItemRect,
                        (LPARAM)&param) == TRUE;
}

} // anonymous namespace

// wrappers for TreeView_GetItem/TreeView_SetItem
static bool IsItemSelected(HWND hwndTV, HTREEITEM hItem)
{
    TV_ITEM tvi;
    tvi.mask = TVIF_STATE | TVIF_HANDLE;
    tvi.stateMask = TVIS_SELECTED;
    tvi.hItem = hItem;

    TreeItemUnlocker unlocker(hItem);

    if ( !TreeView_GetItem(hwndTV, &tvi) )
    {
        wxLogLastError(wxT("TreeView_GetItem"));
    }

    return (tvi.state & TVIS_SELECTED) != 0;
}

static bool SelectItem(HWND hwndTV, HTREEITEM hItem, bool select = true)
{
    TV_ITEM tvi;
    tvi.mask = TVIF_STATE | TVIF_HANDLE;
    tvi.stateMask = TVIS_SELECTED;
    tvi.state = select ? TVIS_SELECTED : 0;
    tvi.hItem = hItem;

    TreeItemUnlocker unlocker(hItem);

    if ( TreeView_SetItem(hwndTV, &tvi) == -1 )
    {
        wxLogLastError(wxT("TreeView_SetItem"));
        return false;
    }

    return true;
}

static inline void UnselectItem(HWND hwndTV, HTREEITEM htItem)
{
    SelectItem(hwndTV, htItem, false);
}

static inline void ToggleItemSelection(HWND hwndTV, HTREEITEM htItem)
{
    SelectItem(hwndTV, htItem, !IsItemSelected(hwndTV, htItem));
}

// helper function which selects all items in a range and, optionally,
// deselects all the other ones
//
// returns true if the selection changed at all or false if nothing changed

// flags for SelectRange()
enum
{
    SR_SIMULATE = 1,        // don't do anything, just return true or false
    SR_UNSELECT_OTHERS = 2  // deselect the items not in range
};

static bool SelectRange(HWND hwndTV,
                        HTREEITEM htFirst,
                        HTREEITEM htLast,
                        int flags)
{
    // find the first (or last) item and select it
    bool changed = false;
    bool cont = true;
    HTREEITEM htItem = (HTREEITEM)TreeView_GetRoot(hwndTV);

    while ( htItem && cont )
    {
        if ( (htItem == htFirst) || (htItem == htLast) )
        {
            if ( !IsItemSelected(hwndTV, htItem) )
            {
                if ( !(flags & SR_SIMULATE) )
                {
                    SelectItem(hwndTV, htItem);
                }

                changed = true;
            }

            cont = false;
        }
        else // not first or last
        {
            if ( flags & SR_UNSELECT_OTHERS )
            {
                if ( IsItemSelected(hwndTV, htItem) )
                {
                    if ( !(flags & SR_SIMULATE) )
                        UnselectItem(hwndTV, htItem);

                    changed = true;
                }
            }
        }

        htItem = (HTREEITEM)TreeView_GetNextVisible(hwndTV, htItem);
    }

    // select the items in range
    cont = htFirst != htLast;
    while ( htItem && cont )
    {
        if ( !IsItemSelected(hwndTV, htItem) )
        {
            if ( !(flags & SR_SIMULATE) )
            {
                SelectItem(hwndTV, htItem);
            }

            changed = true;
        }

        cont = (htItem != htFirst) && (htItem != htLast);

        htItem = (HTREEITEM)TreeView_GetNextVisible(hwndTV, htItem);
    }

    // optionally deselect the rest
    if ( flags & SR_UNSELECT_OTHERS )
    {
        while ( htItem )
        {
            if ( IsItemSelected(hwndTV, htItem) )
            {
                if ( !(flags & SR_SIMULATE) )
                {
                    UnselectItem(hwndTV, htItem);
                }

                changed = true;
            }

            htItem = (HTREEITEM)TreeView_GetNextVisible(hwndTV, htItem);
        }
    }

    // seems to be necessary - otherwise the just selected items don't always
    // appear as selected
    if ( !(flags & SR_SIMULATE) )
    {
        UpdateWindow(hwndTV);
    }

    return changed;
}

// helper function which tricks the standard control into changing the focused
// item without changing anything else (if someone knows why Microsoft doesn't
// allow to do it by just setting TVIS_FOCUSED flag, please tell me!)
//
// returns true if the focus was changed, false if the given item was already
// the focused one
static bool SetFocus(HWND hwndTV, HTREEITEM htItem)
{
    // the current focus
    HTREEITEM htFocus = (HTREEITEM)TreeView_GetSelection(hwndTV);

    if ( htItem == htFocus )
        return false;

    if ( htItem )
    {
        // remember the selection state of the item
        bool wasSelected = IsItemSelected(hwndTV, htItem);

        if ( htFocus && IsItemSelected(hwndTV, htFocus) )
        {
            // prevent the tree from unselecting the old focus which it
            // would do by default (TreeView_SelectItem unselects the
            // focused item)
            TreeView_SelectItem(hwndTV, 0);
            SelectItem(hwndTV, htFocus);
        }

        TreeView_SelectItem(hwndTV, htItem);

        if ( !wasSelected )
        {
            // need to clear the selection which TreeView_SelectItem() gave
            // us
            UnselectItem(hwndTV, htItem);
        }
        //else: was selected, still selected - ok
    }
    else // reset focus
    {
        bool wasFocusSelected = IsItemSelected(hwndTV, htFocus);

        // just clear the focus
        TreeView_SelectItem(hwndTV, 0);

        if ( wasFocusSelected )
        {
            // restore the selection state
            SelectItem(hwndTV, htFocus);
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// a convenient wrapper around TV_ITEM struct which adds a ctor
#ifdef __VISUALC__
#pragma warning( disable : 4097 ) // inheriting from typedef
#endif

struct wxTreeViewItem : public TV_ITEM
{
    wxTreeViewItem(const wxTreeItemId& item,    // the item handle
                   UINT mask_,                  // fields which are valid
                   UINT stateMask_ = 0)         // for TVIF_STATE only
    {
        wxZeroMemory(*this);

        // hItem member is always valid
        mask = mask_ | TVIF_HANDLE;
        stateMask = stateMask_;
        hItem = HITEM(item);
    }
};

// ----------------------------------------------------------------------------
// This class is our userdata/lParam for the TV_ITEMs stored in the treeview.
//
// We need this for a couple of reasons:
//
// 1) This class is needed for support of different images: the Win32 common
// control natively supports only 2 images (the normal one and another for the
// selected state). We wish to provide support for 2 more of them for folder
// items (i.e. those which have children): for expanded state and for expanded
// selected state. For this we use this structure to store the additional items
// images.
//
// 2) This class is also needed to hold the HITEM so that we can sort
// it correctly in the MSW sort callback.
//
// In addition it makes other workarounds such as this easier and helps
// simplify the code.
// ----------------------------------------------------------------------------

class wxTreeItemParam
{
public:
    wxTreeItemParam()
    {
        m_data = NULL;

        for ( size_t n = 0; n < WXSIZEOF(m_images); n++ )
        {
            m_images[n] = -1;
        }
    }

    // dtor deletes the associated data as well
    virtual ~wxTreeItemParam() { delete m_data; }

    // accessors
        // get the real data associated with the item
    wxTreeItemData *GetData() const { return m_data; }
        // change it
    void SetData(wxTreeItemData *data) { m_data = data; }

        // do we have such image?
    bool HasImage(wxTreeItemIcon which) const { return m_images[which] != -1; }
        // get image, falling back to the other images if this one is not
        // specified
    int GetImage(wxTreeItemIcon which) const
    {
        int image = m_images[which];
        if ( image == -1 )
        {
            switch ( which )
            {
                case wxTreeItemIcon_SelectedExpanded:
                    // We consider that expanded icon is more important than
                    // selected so test for it first.
                    image = m_images[wxTreeItemIcon_Expanded];
                    if ( image == -1 )
                        image = m_images[wxTreeItemIcon_Selected];
                    if ( image != -1 )
                        break;
                    //else: fall through

                case wxTreeItemIcon_Selected:
                case wxTreeItemIcon_Expanded:
                    image = m_images[wxTreeItemIcon_Normal];
                    break;

                case wxTreeItemIcon_Normal:
                    // no fallback
                    break;

                default:
                    wxFAIL_MSG( wxT("unsupported wxTreeItemIcon value") );
            }
        }

        return image;
    }
        // change the given image
    void SetImage(int image, wxTreeItemIcon which) { m_images[which] = image; }

        // get item
    const wxTreeItemId& GetItem() const { return m_item; }
        // set item
    void SetItem(const wxTreeItemId& item) { m_item = item; }

protected:
    // all the images associated with the item
    int m_images[wxTreeItemIcon_Max];

    // item for sort callbacks
    wxTreeItemId m_item;

    // the real client data
    wxTreeItemData *m_data;

    wxDECLARE_NO_COPY_CLASS(wxTreeItemParam);
};

// wxVirutalNode is used in place of a single root when 'hidden' root is
// specified.
class wxVirtualNode : public wxTreeViewItem
{
public:
    wxVirtualNode(wxTreeItemParam *param)
        : wxTreeViewItem(TVI_ROOT, 0)
    {
        m_param = param;
    }

    ~wxVirtualNode()
    {
        delete m_param;
    }

    wxTreeItemParam *GetParam() const { return m_param; }
    void SetParam(wxTreeItemParam *param) { delete m_param; m_param = param; }

private:
    wxTreeItemParam *m_param;

    wxDECLARE_NO_COPY_CLASS(wxVirtualNode);
};

#ifdef __VISUALC__
#pragma warning( default : 4097 )
#endif

// a macro to get the virtual root, returns NULL if none
#define GET_VIRTUAL_ROOT() ((wxVirtualNode *)m_pVirtualRoot)

// returns true if the item is the virtual root
#define IS_VIRTUAL_ROOT(item) (HITEM(item) == TVI_ROOT)

// a class which encapsulates the tree traversal logic: it vists all (unless
// OnVisit() returns false) items under the given one
class wxTreeTraversal
{
public:
    wxTreeTraversal(const wxTreeCtrl *tree)
    {
        m_tree = tree;
    }

    // give it a virtual dtor: not really needed as the class is never used
    // polymorphically and not even allocated on heap at all, but this is safer
    // (in case it ever is) and silences the compiler warnings for now
    virtual ~wxTreeTraversal() { }

    // do traverse the tree: visit all items (recursively by default) under the
    // given one; return true if all items were traversed or false if the
    // traversal was aborted because OnVisit returned false
    bool DoTraverse(const wxTreeItemId& root, bool recursively = true);

    // override this function to do whatever is needed for each item, return
    // false to stop traversing
    virtual bool OnVisit(const wxTreeItemId& item) = 0;

protected:
    const wxTreeCtrl *GetTree() const { return m_tree; }

private:
    bool Traverse(const wxTreeItemId& root, bool recursively);

    const wxTreeCtrl *m_tree;

    wxDECLARE_NO_COPY_CLASS(wxTreeTraversal);
};

// internal class for getting the selected items
class TraverseSelections : public wxTreeTraversal
{
public:
    TraverseSelections(const wxTreeCtrl *tree,
                       wxArrayTreeItemIds& selections)
        : wxTreeTraversal(tree), m_selections(selections)
        {
            m_selections.Empty();

            if (tree->GetCount() > 0)
                DoTraverse(tree->GetRootItem());
        }

    virtual bool OnVisit(const wxTreeItemId& item)
    {
        const wxTreeCtrl * const tree = GetTree();

        // can't visit a virtual node.
        if ( (tree->GetRootItem() == item) && tree->HasFlag(wxTR_HIDE_ROOT) )
        {
            return true;
        }

        if ( ::IsItemSelected(GetHwndOf(tree), HITEM(item)) )
        {
            m_selections.Add(item);
        }

        return true;
    }

    size_t GetCount() const { return m_selections.GetCount(); }

private:
    wxArrayTreeItemIds& m_selections;

    wxDECLARE_NO_COPY_CLASS(TraverseSelections);
};

// internal class for counting tree items
class TraverseCounter : public wxTreeTraversal
{
public:
    TraverseCounter(const wxTreeCtrl *tree,
                    const wxTreeItemId& root,
                    bool recursively)
        : wxTreeTraversal(tree)
        {
            m_count = 0;

            DoTraverse(root, recursively);
        }

    virtual bool OnVisit(const wxTreeItemId& WXUNUSED(item))
    {
        m_count++;

        return true;
    }

    size_t GetCount() const { return m_count; }

private:
    size_t m_count;

    wxDECLARE_NO_COPY_CLASS(TraverseCounter);
};

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// indices in gs_expandEvents table below
enum
{
    IDX_COLLAPSE,
    IDX_EXPAND,
    IDX_WHAT_MAX
};

enum
{
    IDX_DONE,
    IDX_DOING,
    IDX_HOW_MAX
};

// handy table for sending events - it has to be initialized during run-time
// now so can't be const any more
static /* const */ wxEventType gs_expandEvents[IDX_WHAT_MAX][IDX_HOW_MAX];

/*
   but logically it's a const table with the following entries:
=
{
    { wxEVT_TREE_ITEM_COLLAPSED, wxEVT_TREE_ITEM_COLLAPSING },
    { wxEVT_TREE_ITEM_EXPANDED,  wxEVT_TREE_ITEM_EXPANDING  }
};
*/

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// tree traversal
// ----------------------------------------------------------------------------

bool wxTreeTraversal::DoTraverse(const wxTreeItemId& root, bool recursively)
{
    if ( !OnVisit(root) )
        return false;

    return Traverse(root, recursively);
}

bool wxTreeTraversal::Traverse(const wxTreeItemId& root, bool recursively)
{
    wxTreeItemIdValue cookie;
    wxTreeItemId child = m_tree->GetFirstChild(root, cookie);
    while ( child.IsOk() )
    {
        // depth first traversal
        if ( recursively && !Traverse(child, true) )
            return false;

        if ( !OnVisit(child) )
            return false;

        child = m_tree->GetNextChild(root, cookie);
    }

    return true;
}

// ----------------------------------------------------------------------------
// construction and destruction
// ----------------------------------------------------------------------------

void wxTreeCtrl::Init()
{
    m_textCtrl = NULL;
    m_hasAnyAttr = false;
#if wxUSE_DRAGIMAGE
    m_dragImage = NULL;
#endif
    m_pVirtualRoot = NULL;
    m_dragStarted = false;
    m_focusLost = true;
    m_changingSelection = false;
    m_triggerStateImageClick = false;
    m_mouseUpDeselect = false;

    // initialize the global array of events now as it can't be done statically
    // with the wxEVT_XXX values being allocated during run-time only
    gs_expandEvents[IDX_COLLAPSE][IDX_DONE] = wxEVT_TREE_ITEM_COLLAPSED;
    gs_expandEvents[IDX_COLLAPSE][IDX_DOING] = wxEVT_TREE_ITEM_COLLAPSING;
    gs_expandEvents[IDX_EXPAND][IDX_DONE] = wxEVT_TREE_ITEM_EXPANDED;
    gs_expandEvents[IDX_EXPAND][IDX_DOING] = wxEVT_TREE_ITEM_EXPANDING;
}

bool wxTreeCtrl::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxValidator& validator,
                        const wxString& name)
{
    Init();

    if ( (style & wxBORDER_MASK) == wxBORDER_DEFAULT )
        style |= wxBORDER_SUNKEN;

    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    WXDWORD exStyle = 0;
    DWORD wstyle = MSWGetStyle(m_windowStyle, & exStyle);
    wstyle |= WS_TABSTOP | TVS_SHOWSELALWAYS;

    if ( !(m_windowStyle & wxTR_NO_LINES) )
        wstyle |= TVS_HASLINES;
    if ( m_windowStyle & wxTR_HAS_BUTTONS )
        wstyle |= TVS_HASBUTTONS;

    if ( m_windowStyle & wxTR_EDIT_LABELS )
        wstyle |= TVS_EDITLABELS;

    if ( m_windowStyle & wxTR_LINES_AT_ROOT )
        wstyle |= TVS_LINESATROOT;

    if ( m_windowStyle & wxTR_FULL_ROW_HIGHLIGHT )
    {
        if ( wxApp::GetComCtl32Version() >= 471 )
            wstyle |= TVS_FULLROWSELECT;
    }

#if !defined(__WXWINCE__) && defined(TVS_INFOTIP)
    // Need so that TVN_GETINFOTIP messages will be sent
    wstyle |= TVS_INFOTIP;
#endif

    // Create the tree control.
    if ( !MSWCreateControl(WC_TREEVIEW, wstyle, pos, size) )
        return false;

    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    SetForegroundColour(wxWindow::GetParent()->GetForegroundColour());

    wxSetCCUnicodeFormat(GetHwnd());

    if ( m_windowStyle & wxTR_TWIST_BUTTONS )
    {
        // Under Vista and later Explorer uses rotating ("twist") buttons
        // instead of the default "+/-" ones so apply its theme to the tree
        // control to implement this style.
        if ( wxGetWinVersion() >= wxWinVersion_Vista )
        {
            if ( wxUxThemeEngine *theme = wxUxThemeEngine::GetIfActive() )
            {
                theme->SetWindowTheme(GetHwnd(), L"EXPLORER", NULL);
            }
        }
    }

    return true;
}

wxTreeCtrl::~wxTreeCtrl()
{
    m_isBeingDeleted = true;

    // delete any attributes
    if ( m_hasAnyAttr )
    {
        WX_CLEAR_HASH_MAP(wxMapTreeAttr, m_attrs);

        // prevent TVN_DELETEITEM handler from deleting the attributes again!
        m_hasAnyAttr = false;
    }

    DeleteTextCtrl();

    // delete user data to prevent memory leaks
    // also deletes hidden root node storage.
    DeleteAllItems();
}

// ----------------------------------------------------------------------------
// accessors
// ----------------------------------------------------------------------------

/* static */ wxVisualAttributes
wxTreeCtrl::GetClassDefaultAttributes(wxWindowVariant variant)
{
    wxVisualAttributes attrs = GetCompositeControlsDefaultAttributes(variant);

    // common controls have their own default font
    attrs.font = wxGetCCDefaultFont();

    return attrs;
}


// simple wrappers which add error checking in debug mode

bool wxTreeCtrl::DoGetItem(wxTreeViewItem *tvItem) const
{
    wxCHECK_MSG( tvItem->hItem != TVI_ROOT, false,
                 wxT("can't retrieve virtual root item") );

    if ( !TreeView_GetItem(GetHwnd(), tvItem) )
    {
        wxLogLastError(wxT("TreeView_GetItem"));

        return false;
    }

    return true;
}

void wxTreeCtrl::DoSetItem(wxTreeViewItem *tvItem)
{
    TreeItemUnlocker unlocker(tvItem->hItem);

    if ( TreeView_SetItem(GetHwnd(), tvItem) == -1 )
    {
        wxLogLastError(wxT("TreeView_SetItem"));
    }
}

unsigned int wxTreeCtrl::GetCount() const
{
    return (unsigned int)TreeView_GetCount(GetHwnd());
}

unsigned int wxTreeCtrl::GetIndent() const
{
    return TreeView_GetIndent(GetHwnd());
}

void wxTreeCtrl::SetIndent(unsigned int indent)
{
    TreeView_SetIndent(GetHwnd(), indent);
}

void wxTreeCtrl::SetAnyImageList(wxImageList *imageList, int which)
{
    // no error return
    (void) TreeView_SetImageList(GetHwnd(),
                                 imageList ? imageList->GetHIMAGELIST() : 0,
                                 which);
}

void wxTreeCtrl::SetImageList(wxImageList *imageList)
{
    if (m_ownsImageListNormal)
        delete m_imageListNormal;

    SetAnyImageList(m_imageListNormal = imageList, TVSIL_NORMAL);
    m_ownsImageListNormal = false;
}

void wxTreeCtrl::SetStateImageList(wxImageList *imageList)
{
    if (m_ownsImageListState) delete m_imageListState;
    SetAnyImageList(m_imageListState = imageList, TVSIL_STATE);
    m_ownsImageListState = false;
}

size_t wxTreeCtrl::GetChildrenCount(const wxTreeItemId& item,
                                    bool recursively) const
{
    wxCHECK_MSG( item.IsOk(), 0u, wxT("invalid tree item") );

    TraverseCounter counter(this, item, recursively);
    return counter.GetCount() - 1;
}

// ----------------------------------------------------------------------------
// control colours
// ----------------------------------------------------------------------------

bool wxTreeCtrl::SetBackgroundColour(const wxColour &colour)
{
    if ( !wxWindowBase::SetBackgroundColour(colour) )
        return false;

    ::SendMessage(GetHwnd(), TVM_SETBKCOLOR, 0, colour.GetPixel());

    return true;
}

bool wxTreeCtrl::SetForegroundColour(const wxColour &colour)
{
    if ( !wxWindowBase::SetForegroundColour(colour) )
        return false;

    ::SendMessage(GetHwnd(), TVM_SETTEXTCOLOR, 0, colour.GetPixel());

    return true;
}

// ----------------------------------------------------------------------------
// Item access
// ----------------------------------------------------------------------------

bool wxTreeCtrl::IsHiddenRoot(const wxTreeItemId& item) const
{
    return HITEM(item) == TVI_ROOT && HasFlag(wxTR_HIDE_ROOT);
}

wxString wxTreeCtrl::GetItemText(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxEmptyString, wxT("invalid tree item") );

    wxChar buf[512];  // the size is arbitrary...

    wxTreeViewItem tvItem(item, TVIF_TEXT);
    tvItem.pszText = buf;
    tvItem.cchTextMax = WXSIZEOF(buf);
    if ( !DoGetItem(&tvItem) )
    {
        // don't return some garbage which was on stack, but an empty string
        buf[0] = wxT('\0');
    }

    return wxString(buf);
}

void wxTreeCtrl::SetItemText(const wxTreeItemId& item, const wxString& text)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    if ( IS_VIRTUAL_ROOT(item) )
        return;

    wxTreeViewItem tvItem(item, TVIF_TEXT);
    tvItem.pszText = wxMSW_CONV_LPTSTR(text);
    DoSetItem(&tvItem);

    // when setting the text of the item being edited, the text control should
    // be updated to reflect the new text as well, otherwise calling
    // SetItemText() in the OnBeginLabelEdit() handler doesn't have any effect
    //
    // don't use GetEditControl() here because m_textCtrl is not set yet
    HWND hwndEdit = TreeView_GetEditControl(GetHwnd());
    if ( hwndEdit )
    {
        if ( item == m_idEdited )
        {
            ::SetWindowText(hwndEdit, text.t_str());
        }
    }
}

int wxTreeCtrl::GetItemImage(const wxTreeItemId& item,
                             wxTreeItemIcon which) const
{
    wxCHECK_MSG( item.IsOk(), -1, wxT("invalid tree item") );

    if ( IsHiddenRoot(item) )
    {
        // no images for hidden root item
        return -1;
    }

    wxTreeItemParam *param = GetItemParam(item);

    return param && param->HasImage(which) ? param->GetImage(which) : -1;
}

void wxTreeCtrl::SetItemImage(const wxTreeItemId& item, int image,
                              wxTreeItemIcon which)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );
    wxCHECK_RET( which >= 0 &&
                 which < wxTreeItemIcon_Max,
                 wxT("invalid image index"));


    if ( IsHiddenRoot(item) )
    {
        // no images for hidden root item
        return;
    }

    wxTreeItemParam *data = GetItemParam(item);
    if ( !data )
        return;

    data->SetImage(image, which);

    RefreshItem(item);
}

wxTreeItemParam *wxTreeCtrl::GetItemParam(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), NULL, wxT("invalid tree item") );

    wxTreeViewItem tvItem(item, TVIF_PARAM);

    // hidden root may still have data.
    if ( IS_VIRTUAL_ROOT(item) )
    {
        return GET_VIRTUAL_ROOT()->GetParam();
    }

    // visible node.
    if ( !DoGetItem(&tvItem) )
    {
        return NULL;
    }

    return (wxTreeItemParam *)tvItem.lParam;
}

bool wxTreeCtrl::HandleTreeEvent(wxTreeEvent& event) const
{
    if ( event.m_item.IsOk() )
    {
        event.SetClientObject(GetItemData(event.m_item));
    }

    return HandleWindowEvent(event);
}

wxTreeItemData *wxTreeCtrl::GetItemData(const wxTreeItemId& item) const
{
    wxTreeItemParam *data = GetItemParam(item);

    return data ? data->GetData() : NULL;
}

void wxTreeCtrl::SetItemData(const wxTreeItemId& item, wxTreeItemData *data)
{
    // first, associate this piece of data with this item
    if ( data )
    {
        data->SetId(item);
    }

    wxTreeItemParam *param = GetItemParam(item);

    wxCHECK_RET( param, wxT("failed to change tree items data") );

    param->SetData(data);
}

void wxTreeCtrl::SetItemHasChildren(const wxTreeItemId& item, bool has)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    if ( IS_VIRTUAL_ROOT(item) )
        return;

    wxTreeViewItem tvItem(item, TVIF_CHILDREN);
    tvItem.cChildren = (int)has;
    DoSetItem(&tvItem);
}

void wxTreeCtrl::SetItemBold(const wxTreeItemId& item, bool bold)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    if ( IS_VIRTUAL_ROOT(item) )
        return;

    wxTreeViewItem tvItem(item, TVIF_STATE, TVIS_BOLD);
    tvItem.state = bold ? TVIS_BOLD : 0;
    DoSetItem(&tvItem);
}

void wxTreeCtrl::SetItemDropHighlight(const wxTreeItemId& item, bool highlight)
{
    if ( IS_VIRTUAL_ROOT(item) )
        return;

    wxTreeViewItem tvItem(item, TVIF_STATE, TVIS_DROPHILITED);
    tvItem.state = highlight ? TVIS_DROPHILITED : 0;
    DoSetItem(&tvItem);
}

void wxTreeCtrl::RefreshItem(const wxTreeItemId& item)
{
    if ( IS_VIRTUAL_ROOT(item) )
        return;

    wxRect rect;
    if ( GetBoundingRect(item, rect) )
    {
        RefreshRect(rect);
    }
}

wxColour wxTreeCtrl::GetItemTextColour(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxNullColour, wxT("invalid tree item") );

    wxMapTreeAttr::const_iterator it = m_attrs.find(item.m_pItem);
    return it == m_attrs.end() ? wxNullColour : it->second->GetTextColour();
}

wxColour wxTreeCtrl::GetItemBackgroundColour(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxNullColour, wxT("invalid tree item") );

    wxMapTreeAttr::const_iterator it = m_attrs.find(item.m_pItem);
    return it == m_attrs.end() ? wxNullColour : it->second->GetBackgroundColour();
}

wxFont wxTreeCtrl::GetItemFont(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxNullFont, wxT("invalid tree item") );

    wxMapTreeAttr::const_iterator it = m_attrs.find(item.m_pItem);
    return it == m_attrs.end() ? wxNullFont : it->second->GetFont();
}

void wxTreeCtrl::SetItemTextColour(const wxTreeItemId& item,
                                   const wxColour& col)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxTreeItemAttr *attr;
    wxMapTreeAttr::iterator it = m_attrs.find(item.m_pItem);
    if ( it == m_attrs.end() )
    {
        m_hasAnyAttr = true;

        m_attrs[item.m_pItem] =
        attr = new wxTreeItemAttr;
    }
    else
    {
        attr = it->second;
    }

    attr->SetTextColour(col);

    RefreshItem(item);
}

void wxTreeCtrl::SetItemBackgroundColour(const wxTreeItemId& item,
                                         const wxColour& col)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxTreeItemAttr *attr;
    wxMapTreeAttr::iterator it = m_attrs.find(item.m_pItem);
    if ( it == m_attrs.end() )
    {
        m_hasAnyAttr = true;

        m_attrs[item.m_pItem] =
        attr = new wxTreeItemAttr;
    }
    else // already in the hash
    {
        attr = it->second;
    }

    attr->SetBackgroundColour(col);

    RefreshItem(item);
}

void wxTreeCtrl::SetItemFont(const wxTreeItemId& item, const wxFont& font)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxTreeItemAttr *attr;
    wxMapTreeAttr::iterator it = m_attrs.find(item.m_pItem);
    if ( it == m_attrs.end() )
    {
        m_hasAnyAttr = true;

        m_attrs[item.m_pItem] =
        attr = new wxTreeItemAttr;
    }
    else // already in the hash
    {
        attr = it->second;
    }

    attr->SetFont(font);

    // Reset the item's text to ensure that the bounding rect will be adjusted
    // for the new font.
    SetItemText(item, GetItemText(item));

    RefreshItem(item);
}

// ----------------------------------------------------------------------------
// Item status
// ----------------------------------------------------------------------------

bool wxTreeCtrl::IsVisible(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), false, wxT("invalid tree item") );

    if ( item == wxTreeItemId(TVI_ROOT) )
    {
        // virtual (hidden) root is never visible
        return false;
    }

    // Bug in Gnu-Win32 headers, so don't use the macro TreeView_GetItemRect
    TVGetItemRectParam param;

    // true means to get rect for just the text, not the whole line
    if ( !wxTreeView_GetItemRect(GetHwnd(), HITEM(item), param, TRUE) )
    {
        // if TVM_GETITEMRECT returned false, then the item is definitely not
        // visible (because its parent is not expanded)
        return false;
    }

    // however if it returned true, the item might still be outside the
    // currently visible part of the tree, test for it (notice that partly
    // visible means visible here)
    return param.rect.bottom > 0 && param.rect.top < GetClientSize().y;
}

bool wxTreeCtrl::ItemHasChildren(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), false, wxT("invalid tree item") );

    if ( IS_VIRTUAL_ROOT(item) )
    {
        wxTreeItemIdValue cookie;
        return GetFirstChild(item, cookie).IsOk();
    }

    wxTreeViewItem tvItem(item, TVIF_CHILDREN);
    DoGetItem(&tvItem);

    return tvItem.cChildren != 0;
}

bool wxTreeCtrl::IsExpanded(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), false, wxT("invalid tree item") );

    wxTreeViewItem tvItem(item, TVIF_STATE, TVIS_EXPANDED);
    DoGetItem(&tvItem);

    return (tvItem.state & TVIS_EXPANDED) != 0;
}

bool wxTreeCtrl::IsSelected(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), false, wxT("invalid tree item") );

    wxTreeViewItem tvItem(item, TVIF_STATE, TVIS_SELECTED);
    DoGetItem(&tvItem);

    return (tvItem.state & TVIS_SELECTED) != 0;
}

bool wxTreeCtrl::IsBold(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), false, wxT("invalid tree item") );

    wxTreeViewItem tvItem(item, TVIF_STATE, TVIS_BOLD);
    DoGetItem(&tvItem);

    return (tvItem.state & TVIS_BOLD) != 0;
}

// ----------------------------------------------------------------------------
// navigation
// ----------------------------------------------------------------------------

wxTreeItemId wxTreeCtrl::GetRootItem() const
{
    // Root may be real (visible) or virtual (hidden).
    if ( GET_VIRTUAL_ROOT() )
        return TVI_ROOT;

    return wxTreeItemId(TreeView_GetRoot(GetHwnd()));
}

wxTreeItemId wxTreeCtrl::GetSelection() const
{
    wxCHECK_MSG( !HasFlag(wxTR_MULTIPLE), wxTreeItemId(),
                 wxT("this only works with single selection controls") );

    return GetFocusedItem();
}

wxTreeItemId wxTreeCtrl::GetFocusedItem() const
{
    return wxTreeItemId(TreeView_GetSelection(GetHwnd()));
}

wxTreeItemId wxTreeCtrl::GetItemParent(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    HTREEITEM hItem;

    if ( IS_VIRTUAL_ROOT(item) )
    {
        // no parent for the virtual root
        hItem = 0;
    }
    else // normal item
    {
        hItem = TreeView_GetParent(GetHwnd(), HITEM(item));
        if ( !hItem && HasFlag(wxTR_HIDE_ROOT) )
        {
            // the top level items should have the virtual root as their parent
            hItem = TVI_ROOT;
        }
    }

    return wxTreeItemId(hItem);
}

wxTreeItemId wxTreeCtrl::GetFirstChild(const wxTreeItemId& item,
                                       wxTreeItemIdValue& cookie) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    // remember the last child returned in 'cookie'
    cookie = TreeView_GetChild(GetHwnd(), HITEM(item));

    return wxTreeItemId(cookie);
}

wxTreeItemId wxTreeCtrl::GetNextChild(const wxTreeItemId& WXUNUSED(item),
                                      wxTreeItemIdValue& cookie) const
{
    wxTreeItemId fromCookie(cookie);

    HTREEITEM hitem = HITEM(fromCookie);

    hitem = TreeView_GetNextSibling(GetHwnd(), hitem);

    wxTreeItemId item(hitem);

    cookie = item.m_pItem;

    return item;
}

wxTreeItemId wxTreeCtrl::GetLastChild(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    // can this be done more efficiently?
    wxTreeItemIdValue cookie;

    wxTreeItemId childLast,
    child = GetFirstChild(item, cookie);
    while ( child.IsOk() )
    {
        childLast = child;
        child = GetNextChild(item, cookie);
    }

    return childLast;
}

wxTreeItemId wxTreeCtrl::GetNextSibling(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );
    return wxTreeItemId(TreeView_GetNextSibling(GetHwnd(), HITEM(item)));
}

wxTreeItemId wxTreeCtrl::GetPrevSibling(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );
    return wxTreeItemId(TreeView_GetPrevSibling(GetHwnd(), HITEM(item)));
}

wxTreeItemId wxTreeCtrl::GetFirstVisibleItem() const
{
    return wxTreeItemId(TreeView_GetFirstVisible(GetHwnd()));
}

wxTreeItemId wxTreeCtrl::GetNextVisible(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );
    wxASSERT_MSG( IsVisible(item), wxT("The item you call GetNextVisible() for must be visible itself!"));

    wxTreeItemId next(TreeView_GetNextVisible(GetHwnd(), HITEM(item)));
    if ( next.IsOk() && !IsVisible(next) )
    {
        // Win32 considers that any non-collapsed item is visible while we want
        // to return only really visible items
        next.Unset();
    }

    return next;
}

wxTreeItemId wxTreeCtrl::GetPrevVisible(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );
    wxASSERT_MSG( IsVisible(item), wxT("The item you call GetPrevVisible() for must be visible itself!"));

    wxTreeItemId prev(TreeView_GetPrevVisible(GetHwnd(), HITEM(item)));
    if ( prev.IsOk() && !IsVisible(prev) )
    {
        // just as above, Win32 function will happily return the previous item
        // in the tree for the first visible item too
        prev.Unset();
    }

    return prev;
}

// ----------------------------------------------------------------------------
// multiple selections emulation
// ----------------------------------------------------------------------------

size_t wxTreeCtrl::GetSelections(wxArrayTreeItemIds& selections) const
{
    TraverseSelections selector(this, selections);

    return selector.GetCount();
}

// ----------------------------------------------------------------------------
// Usual operations
// ----------------------------------------------------------------------------

wxTreeItemId wxTreeCtrl::DoInsertAfter(const wxTreeItemId& parent,
                                       const wxTreeItemId& hInsertAfter,
                                       const wxString& text,
                                       int image, int selectedImage,
                                       wxTreeItemData *data)
{
    wxCHECK_MSG( parent.IsOk() || !TreeView_GetRoot(GetHwnd()),
                 wxTreeItemId(),
                 wxT("can't have more than one root in the tree") );

    TV_INSERTSTRUCT tvIns;
    tvIns.hParent = HITEM(parent);
    tvIns.hInsertAfter = HITEM(hInsertAfter);

    // this is how we insert the item as the first child: supply a NULL
    // hInsertAfter
    if ( !tvIns.hInsertAfter )
    {
        tvIns.hInsertAfter = TVI_FIRST;
    }

    UINT mask = 0;
    if ( !text.empty() )
    {
        mask |= TVIF_TEXT;
        tvIns.item.pszText = wxMSW_CONV_LPTSTR(text);
    }
    else
    {
        tvIns.item.pszText = NULL;
        tvIns.item.cchTextMax = 0;
    }

    // create the param which will store the other item parameters
    wxTreeItemParam *param = new wxTreeItemParam;

    // we return the images on demand as they depend on whether the item is
    // expanded or collapsed too in our case
    mask |= TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvIns.item.iImage = I_IMAGECALLBACK;
    tvIns.item.iSelectedImage = I_IMAGECALLBACK;

    param->SetImage(image, wxTreeItemIcon_Normal);
    param->SetImage(selectedImage, wxTreeItemIcon_Selected);

    mask |= TVIF_PARAM;
    tvIns.item.lParam = (LPARAM)param;
    tvIns.item.mask = mask;

    // don't use the hack below for the children of hidden root: this results
    // in a crash inside comctl32.dll when we call TreeView_GetItemRect()
    const bool firstChild = !IsHiddenRoot(parent) &&
                                !TreeView_GetChild(GetHwnd(), HITEM(parent));

    HTREEITEM id = TreeView_InsertItem(GetHwnd(), &tvIns);
    if ( id == 0 )
    {
        wxLogLastError(wxT("TreeView_InsertItem"));
    }

    // apparently some Windows versions (2000 and XP are reported to do this)
    // sometimes don't refresh the tree after adding the first child and so we
    // need this to make the "[+]" appear
    if ( firstChild )
    {
        TVGetItemRectParam param2;

        wxTreeView_GetItemRect(GetHwnd(), HITEM(parent), param2, FALSE);
        ::InvalidateRect(GetHwnd(), &param2.rect, FALSE);
    }

    // associate the application tree item with Win32 tree item handle
    param->SetItem(id);

    // setup wxTreeItemData
    if ( data != NULL )
    {
        param->SetData(data);
        data->SetId(id);
    }

    return wxTreeItemId(id);
}

wxTreeItemId wxTreeCtrl::AddRoot(const wxString& text,
                                 int image, int selectedImage,
                                 wxTreeItemData *data)
{
    if ( HasFlag(wxTR_HIDE_ROOT) )
    {
        wxASSERT_MSG( !m_pVirtualRoot, wxT("tree can have only a single root") );

        // create a virtual root item, the parent for all the others
        wxTreeItemParam *param = new wxTreeItemParam;
        param->SetData(data);

        m_pVirtualRoot = new wxVirtualNode(param);

        return TVI_ROOT;
    }

    return DoInsertAfter(wxTreeItemId(), wxTreeItemId(),
                           text, image, selectedImage, data);
}

wxTreeItemId wxTreeCtrl::DoInsertItem(const wxTreeItemId& parent,
                                      size_t index,
                                      const wxString& text,
                                      int image, int selectedImage,
                                      wxTreeItemData *data)
{
    wxTreeItemId idPrev;
    if ( index == (size_t)-1 )
    {
        // special value: append to the end
        idPrev = TVI_LAST;
    }
    else // find the item from index
    {
        wxTreeItemIdValue cookie;
        wxTreeItemId idCur = GetFirstChild(parent, cookie);
        while ( index != 0 && idCur.IsOk() )
        {
            index--;

            idPrev = idCur;
            idCur = GetNextChild(parent, cookie);
        }

        // assert, not check: if the index is invalid, we will append the item
        // to the end
        wxASSERT_MSG( index == 0, wxT("bad index in wxTreeCtrl::InsertItem") );
    }

    return DoInsertAfter(parent, idPrev, text, image, selectedImage, data);
}

void wxTreeCtrl::Delete(const wxTreeItemId& item)
{
    // unlock tree selections on vista, without this the
    // tree ctrl will eventually crash after item deletion
    TreeItemUnlocker unlock_all;

    if ( HasFlag(wxTR_MULTIPLE) )
    {
        bool selected = IsSelected(item);
        wxTreeItemId next;

        if ( selected )
        {
            next = TreeView_GetNextVisible(GetHwnd(), HITEM(item));

            if ( !next.IsOk() )
            {
                next = TreeView_GetPrevVisible(GetHwnd(), HITEM(item));
            }
        }

        {
            TempSetter set(m_changingSelection);
            if ( !TreeView_DeleteItem(GetHwnd(), HITEM(item)) )
            {
                wxLogLastError(wxT("TreeView_DeleteItem"));
                return;
            }
        }

        if ( !selected )
        {
            return;
        }

        if ( item == m_htSelStart )
            m_htSelStart.Unset();

        if ( item == m_htClickedItem )
            m_htClickedItem.Unset();

        if ( next.IsOk() )
        {
            wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING, this, next);

            if ( IsTreeEventAllowed(changingEvent) )
            {
                wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED, this, next);
                (void)HandleTreeEvent(changedEvent);
            }
            else
            {
                DoUnselectItem(next);
                ClearFocusedItem();
            }
        }
    }
    else
    {
        TempSetter set(m_changingSelection);
        if ( !TreeView_DeleteItem(GetHwnd(), HITEM(item)) )
        {
            wxLogLastError(wxT("TreeView_DeleteItem"));
        }
    }
}

// delete all children (but don't delete the item itself)
void wxTreeCtrl::DeleteChildren(const wxTreeItemId& item)
{
    // unlock tree selections on vista for the duration of this call
    TreeItemUnlocker unlock_all;

    wxTreeItemIdValue cookie;

    wxArrayTreeItemIds children;
    wxTreeItemId child = GetFirstChild(item, cookie);
    while ( child.IsOk() )
    {
        children.Add(child);

        child = GetNextChild(item, cookie);
    }

    size_t nCount = children.Count();
    for ( size_t n = 0; n < nCount; n++ )
    {
        Delete(children[n]);
    }
}

void wxTreeCtrl::DeleteAllItems()
{
    // unlock tree selections on vista for the duration of this call
    TreeItemUnlocker unlock_all;

    // invalidate all the items we store as they're going to become invalid
    m_htSelStart =
    m_htClickedItem = wxTreeItemId();

    // delete the "virtual" root item.
    if ( GET_VIRTUAL_ROOT() )
    {
        delete GET_VIRTUAL_ROOT();
        m_pVirtualRoot = NULL;
    }

    // and all the real items

    if ( !TreeView_DeleteAllItems(GetHwnd()) )
    {
        wxLogLastError(wxT("TreeView_DeleteAllItems"));
    }
}

void wxTreeCtrl::DoExpand(const wxTreeItemId& item, int flag)
{
    wxASSERT_MSG( flag == TVE_COLLAPSE ||
                  flag == (TVE_COLLAPSE | TVE_COLLAPSERESET) ||
                  flag == TVE_EXPAND   ||
                  flag == TVE_TOGGLE,
                  wxT("Unknown flag in wxTreeCtrl::DoExpand") );

    // A hidden root can be neither expanded nor collapsed.
    wxCHECK_RET( !IsHiddenRoot(item),
                 wxT("Can't expand/collapse hidden root node!") );

    // TreeView_Expand doesn't send TVN_ITEMEXPAND(ING) messages, so we must
    // emulate them. This behaviour has changed slightly with comctl32.dll
    // v 4.70 - now it does send them but only the first time. To maintain
    // compatible behaviour and also in order to not have surprises with the
    // future versions, don't rely on this and still do everything ourselves.
    // To avoid that the messages be sent twice when the item is expanded for
    // the first time we must clear TVIS_EXPANDEDONCE style manually.

    wxTreeViewItem tvItem(item, TVIF_STATE, TVIS_EXPANDEDONCE);
    tvItem.state = 0;
    DoSetItem(&tvItem);

    if ( IsExpanded(item) )
    {
        wxTreeEvent event(wxEVT_TREE_ITEM_COLLAPSING,
                          this, wxTreeItemId(item));

        if ( !IsTreeEventAllowed(event) )
            return;
    }

    if ( TreeView_Expand(GetHwnd(), HITEM(item), flag) )
    {
        if ( IsExpanded(item) )
            return;

        wxTreeEvent event(wxEVT_TREE_ITEM_COLLAPSED, this, item);
        (void)HandleTreeEvent(event);
    }
    //else: change didn't took place, so do nothing at all
}

void wxTreeCtrl::Expand(const wxTreeItemId& item)
{
    DoExpand(item, TVE_EXPAND);
}

void wxTreeCtrl::Collapse(const wxTreeItemId& item)
{
    DoExpand(item, TVE_COLLAPSE);
}

void wxTreeCtrl::CollapseAndReset(const wxTreeItemId& item)
{
    DoExpand(item, TVE_COLLAPSE | TVE_COLLAPSERESET);
}

void wxTreeCtrl::Toggle(const wxTreeItemId& item)
{
    DoExpand(item, TVE_TOGGLE);
}

void wxTreeCtrl::Unselect()
{
    wxASSERT_MSG( !HasFlag(wxTR_MULTIPLE),
                  wxT("doesn't make sense, may be you want UnselectAll()?") );

    // the current focus
    HTREEITEM htFocus = (HTREEITEM)TreeView_GetSelection(GetHwnd());

    if ( !htFocus )
    {
        return;
    }

    if ( HasFlag(wxTR_MULTIPLE) )
    {
        wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                  this, wxTreeItemId());
        changingEvent.m_itemOld = htFocus;

        if ( IsTreeEventAllowed(changingEvent) )
        {
            ClearFocusedItem();

            wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                     this, wxTreeItemId());
            changedEvent.m_itemOld = htFocus;
            (void)HandleTreeEvent(changedEvent);
        }
    }
    else
    {
        ClearFocusedItem();
    }
}

void wxTreeCtrl::DoUnselectAll()
{
    wxArrayTreeItemIds selections;
    size_t count = GetSelections(selections);

    for ( size_t n = 0; n < count; n++ )
    {
        DoUnselectItem(selections[n]);
    }

    m_htSelStart.Unset();
}

void wxTreeCtrl::UnselectAll()
{
    if ( HasFlag(wxTR_MULTIPLE) )
    {
        HTREEITEM htFocus = (HTREEITEM)TreeView_GetSelection(GetHwnd());
        if ( !htFocus ) return;

        wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING, this);
        changingEvent.m_itemOld = htFocus;

        if ( IsTreeEventAllowed(changingEvent) )
        {
            DoUnselectAll();

            wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED, this);
            changedEvent.m_itemOld = htFocus;
            (void)HandleTreeEvent(changedEvent);
        }
    }
    else
    {
        Unselect();
    }
}

void wxTreeCtrl::DoSelectChildren(const wxTreeItemId& parent)
{
    DoUnselectAll();

    wxTreeItemIdValue cookie;
    wxTreeItemId child = GetFirstChild(parent, cookie);
    while ( child.IsOk() )
    {
        DoSelectItem(child, true);
        child = GetNextChild(child, cookie);
    }
}

void wxTreeCtrl::SelectChildren(const wxTreeItemId& parent)
{
    wxCHECK_RET( HasFlag(wxTR_MULTIPLE),
                 "this only works with multiple selection controls" );

    HTREEITEM htFocus = (HTREEITEM)TreeView_GetSelection(GetHwnd());

    wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING, this);
    changingEvent.m_itemOld = htFocus;

    if ( IsTreeEventAllowed(changingEvent) )
    {
        DoSelectChildren(parent);

        wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED, this);
        changedEvent.m_itemOld = htFocus;
        (void)HandleTreeEvent(changedEvent);
    }
}

void wxTreeCtrl::DoSelectItem(const wxTreeItemId& item, bool select)
{
    TempSetter set(m_changingSelection);

    ::SelectItem(GetHwnd(), HITEM(item), select);
}

void wxTreeCtrl::SelectItem(const wxTreeItemId& item, bool select)
{
    wxCHECK_RET( !IsHiddenRoot(item), wxT("can't select hidden root item") );

    if ( select == IsSelected(item) )
    {
        // nothing to do, the item is already in the requested state
        return;
    }

    if ( HasFlag(wxTR_MULTIPLE) )
    {
        wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING, this, item);

        if ( IsTreeEventAllowed(changingEvent) )
        {
            HTREEITEM htFocus = (HTREEITEM)TreeView_GetSelection(GetHwnd());
            DoSelectItem(item, select);

            if ( !htFocus )
            {
                SetFocusedItem(item);
            }

            wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                     this, item);
            (void)HandleTreeEvent(changedEvent);
        }
    }
    else // single selection
    {
        wxTreeItemId itemOld, itemNew;
        if ( select )
        {
            itemOld = GetSelection();
            itemNew = item;
        }
        else // deselecting the currently selected item
        {
            itemOld = item;
            // leave itemNew invalid
        }

        // Recent versions of comctl32.dll send TVN_SELCHANG{ED,ING} events
        // when we call TreeView_SelectItem() but apparently some old ones did
        // not so send the events ourselves and ignore those generated by
        // TreeView_SelectItem() if m_changingSelection is set.
        wxTreeEvent
            changingEvent(wxEVT_TREE_SEL_CHANGING, this, itemNew);
        changingEvent.SetOldItem(itemOld);

        if ( IsTreeEventAllowed(changingEvent) )
        {
            TempSetter set(m_changingSelection);

            if ( !TreeView_SelectItem(GetHwnd(), HITEM(itemNew)) )
            {
                wxLogLastError(wxT("TreeView_SelectItem"));
            }
            else // ok
            {
                ::SetFocus(GetHwnd(), HITEM(item));

                wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                         this, itemNew);
                changedEvent.SetOldItem(itemOld);
                (void)HandleTreeEvent(changedEvent);
            }
        }
        //else: program vetoed the change
    }
}

void wxTreeCtrl::EnsureVisible(const wxTreeItemId& item)
{
    wxCHECK_RET( !IsHiddenRoot(item), wxT("can't show hidden root item") );

    // no error return
    TreeView_EnsureVisible(GetHwnd(), HITEM(item));
}

void wxTreeCtrl::ScrollTo(const wxTreeItemId& item)
{
    if ( !TreeView_SelectSetFirstVisible(GetHwnd(), HITEM(item)) )
    {
        wxLogLastError(wxT("TreeView_SelectSetFirstVisible"));
    }
}

wxTextCtrl *wxTreeCtrl::GetEditControl() const
{
    return m_textCtrl;
}

void wxTreeCtrl::DeleteTextCtrl()
{
    if ( m_textCtrl )
    {
        // the HWND corresponding to this control is deleted by the tree
        // control itself and we don't know when exactly this happens, so check
        // if the window still exists before calling UnsubclassWin()
        if ( !::IsWindow(GetHwndOf(m_textCtrl)) )
        {
            m_textCtrl->SetHWND(0);
        }

        m_textCtrl->UnsubclassWin();
        m_textCtrl->SetHWND(0);
        wxDELETE(m_textCtrl);

        m_idEdited.Unset();
    }
}

wxTextCtrl *wxTreeCtrl::EditLabel(const wxTreeItemId& item,
                                  wxClassInfo *textControlClass)
{
    wxASSERT( textControlClass->IsKindOf(wxCLASSINFO(wxTextCtrl)) );

    DeleteTextCtrl();

    m_idEdited = item;
    m_textCtrl = (wxTextCtrl *)textControlClass->CreateObject();
    HWND hWnd = (HWND) TreeView_EditLabel(GetHwnd(), HITEM(item));

    // this is not an error - the TVN_BEGINLABELEDIT handler might have
    // returned false
    if ( !hWnd )
    {
        wxDELETE(m_textCtrl);
        return NULL;
    }

    // textctrl is subclassed in MSWOnNotify
    return m_textCtrl;
}

// End label editing, optionally cancelling the edit
void wxTreeCtrl::DoEndEditLabel(bool discardChanges)
{
    TreeView_EndEditLabelNow(GetHwnd(), discardChanges);

    DeleteTextCtrl();
}

wxTreeItemId wxTreeCtrl::DoTreeHitTest(const wxPoint& point, int& flags) const
{
    TV_HITTESTINFO hitTestInfo;
    hitTestInfo.pt.x = (int)point.x;
    hitTestInfo.pt.y = (int)point.y;

    (void) TreeView_HitTest(GetHwnd(), &hitTestInfo);

    flags = 0;

    // avoid repetition
    #define TRANSLATE_FLAG(flag) if ( hitTestInfo.flags & TVHT_##flag ) \
                                    flags |= wxTREE_HITTEST_##flag

    TRANSLATE_FLAG(ABOVE);
    TRANSLATE_FLAG(BELOW);
    TRANSLATE_FLAG(NOWHERE);
    TRANSLATE_FLAG(ONITEMBUTTON);
    TRANSLATE_FLAG(ONITEMICON);
    TRANSLATE_FLAG(ONITEMINDENT);
    TRANSLATE_FLAG(ONITEMLABEL);
    TRANSLATE_FLAG(ONITEMRIGHT);
    TRANSLATE_FLAG(ONITEMSTATEICON);
    TRANSLATE_FLAG(TOLEFT);
    TRANSLATE_FLAG(TORIGHT);

    #undef TRANSLATE_FLAG

    return wxTreeItemId(hitTestInfo.hItem);
}

bool wxTreeCtrl::GetBoundingRect(const wxTreeItemId& item,
                                 wxRect& rect,
                                 bool textOnly) const
{
    // Virtual root items have no bounding rectangle
    if ( IS_VIRTUAL_ROOT(item) )
    {
        return false;
    }

    TVGetItemRectParam param;

    if ( wxTreeView_GetItemRect(GetHwnd(), HITEM(item), param, textOnly) )
    {
        rect = wxRect(wxPoint(param.rect.left, param.rect.top),
                      wxPoint(param.rect.right, param.rect.bottom));

        return true;
    }
    else
    {
        // couldn't retrieve rect: for example, item isn't visible
        return false;
    }
}

void wxTreeCtrl::ClearFocusedItem()
{
    TempSetter set(m_changingSelection);

    if ( !TreeView_SelectItem(GetHwnd(), 0) )
    {
        wxLogLastError(wxT("TreeView_SelectItem"));
    }
}

void wxTreeCtrl::SetFocusedItem(const wxTreeItemId& item)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    TempSetter set(m_changingSelection);

    ::SetFocus(GetHwnd(), HITEM(item));
}

void wxTreeCtrl::DoUnselectItem(const wxTreeItemId& item)
{
    TempSetter set(m_changingSelection);

    ::UnselectItem(GetHwnd(), HITEM(item));
}

void wxTreeCtrl::DoToggleItemSelection(const wxTreeItemId& item)
{
    TempSetter set(m_changingSelection);

    ::ToggleItemSelection(GetHwnd(), HITEM(item));
}

// ----------------------------------------------------------------------------
// sorting stuff
// ----------------------------------------------------------------------------

// this is just a tiny namespace which is friend to wxTreeCtrl and so can use
// functions such as IsDataIndirect()
class wxTreeSortHelper
{
public:
    static int CALLBACK Compare(LPARAM data1, LPARAM data2, LPARAM tree);

private:
    static wxTreeItemId GetIdFromData(LPARAM lParam)
    {
        return ((wxTreeItemParam*)lParam)->GetItem();
        }
};

int CALLBACK wxTreeSortHelper::Compare(LPARAM pItem1,
                                       LPARAM pItem2,
                                       LPARAM htree)
{
    wxCHECK_MSG( pItem1 && pItem2, 0,
                 wxT("sorting tree without data doesn't make sense") );

    wxTreeCtrl *tree = (wxTreeCtrl *)htree;

    return tree->OnCompareItems(GetIdFromData(pItem1),
                                GetIdFromData(pItem2));
}

void wxTreeCtrl::SortChildren(const wxTreeItemId& item)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    // rely on the fact that TreeView_SortChildren does the same thing as our
    // default behaviour, i.e. sorts items alphabetically and so call it
    // directly if we're not in derived class (much more efficient!)
    // RN: Note that if you find you're code doesn't sort as expected this
    //     may be why as if you don't use the DECLARE_CLASS/IMPLEMENT_CLASS
    //     combo for your derived wxTreeCtrl if will sort without
    //     OnCompareItems
    if ( GetClassInfo() == wxCLASSINFO(wxTreeCtrl) )
    {
        TreeView_SortChildren(GetHwnd(), HITEM(item), 0);
    }
    else
    {
        TV_SORTCB tvSort;
        tvSort.hParent = HITEM(item);
        tvSort.lpfnCompare = wxTreeSortHelper::Compare;
        tvSort.lParam = (LPARAM)this;
        TreeView_SortChildrenCB(GetHwnd(), &tvSort, 0 /* reserved */);
    }
}

// ----------------------------------------------------------------------------
// implementation
// ----------------------------------------------------------------------------

bool wxTreeCtrl::MSWShouldPreProcessMessage(WXMSG* msg)
{
    if ( msg->message == WM_KEYDOWN )
    {
        // Only eat VK_RETURN if not being used by the application in
        // conjunction with modifiers
        if ( (msg->wParam == VK_RETURN) && !wxIsAnyModifierDown() )
        {
            // we need VK_RETURN to generate wxEVT_TREE_ITEM_ACTIVATED
            return false;
        }
    }

    return wxTreeCtrlBase::MSWShouldPreProcessMessage(msg);
}

bool wxTreeCtrl::MSWCommand(WXUINT cmd, WXWORD id_)
{
    const int id = (signed short)id_;

    if ( cmd == EN_UPDATE )
    {
        wxCommandEvent event(wxEVT_TEXT, id);
        event.SetEventObject( this );
        ProcessCommand(event);
    }
    else if ( cmd == EN_KILLFOCUS )
    {
        wxCommandEvent event(wxEVT_KILL_FOCUS, id);
        event.SetEventObject( this );
        ProcessCommand(event);
    }
    else
    {
        // nothing done
        return false;
    }

    // command processed
    return true;
}

bool wxTreeCtrl::MSWIsOnItem(unsigned flags) const
{
    unsigned mask = TVHT_ONITEM;
    if ( HasFlag(wxTR_FULL_ROW_HIGHLIGHT) )
        mask |= TVHT_ONITEMINDENT | TVHT_ONITEMRIGHT;

    return (flags & mask) != 0;
}

bool wxTreeCtrl::MSWHandleSelectionKey(unsigned vkey)
{
    const bool bCtrl = wxIsCtrlDown();
    const bool bShift = wxIsShiftDown();
    const HTREEITEM htSel = (HTREEITEM)TreeView_GetSelection(GetHwnd());

    switch ( vkey )
    {
        case VK_RETURN:
        case VK_SPACE:
            if ( !htSel )
                break;

            if ( vkey != VK_RETURN && bCtrl )
            {
                wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                          this, htSel);
                changingEvent.m_itemOld = htSel;

                if ( IsTreeEventAllowed(changingEvent) )
                {
                    DoToggleItemSelection(wxTreeItemId(htSel));

                    wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                             this, htSel);
                    changedEvent.m_itemOld = htSel;
                    (void)HandleTreeEvent(changedEvent);
                }
            }
            else
            {
                wxArrayTreeItemIds selections;
                size_t count = GetSelections(selections);

                if ( count != 1 || HITEM(selections[0]) != htSel )
                {
                    wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                              this, htSel);
                    changingEvent.m_itemOld = htSel;

                    if ( IsTreeEventAllowed(changingEvent) )
                    {
                        DoUnselectAll();
                        DoSelectItem(wxTreeItemId(htSel));

                        wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                                 this, htSel);
                        changedEvent.m_itemOld = htSel;
                        (void)HandleTreeEvent(changedEvent);
                    }
                }
            }
            break;

        case VK_UP:
        case VK_DOWN:
            if ( !bCtrl && !bShift )
            {
                wxArrayTreeItemIds selections;
                wxTreeItemId next;

                if ( htSel )
                {
                    next = vkey == VK_UP
                            ? TreeView_GetPrevVisible(GetHwnd(), htSel)
                            : TreeView_GetNextVisible(GetHwnd(), htSel);
                }
                else
                {
                    next = GetRootItem();

                    if ( IsHiddenRoot(next) )
                        next = TreeView_GetChild(GetHwnd(), HITEM(next));
                }

                if ( !next.IsOk() )
                {
                    break;
                }

                wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                          this, next);
                changingEvent.m_itemOld = htSel;

                if ( IsTreeEventAllowed(changingEvent) )
                {
                    DoUnselectAll();
                    DoSelectItem(next);
                    SetFocusedItem(next);

                    wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                             this, next);
                    changedEvent.m_itemOld = htSel;
                    (void)HandleTreeEvent(changedEvent);
                }
            }
            else if ( htSel )
            {
                wxTreeItemId next = vkey == VK_UP
                    ? TreeView_GetPrevVisible(GetHwnd(), htSel)
                    : TreeView_GetNextVisible(GetHwnd(), htSel);

                if ( !next.IsOk() )
                {
                    break;
                }

                if ( !m_htSelStart )
                {
                    m_htSelStart = htSel;
                }

                if ( bShift && SelectRange(GetHwnd(), HITEM(m_htSelStart), HITEM(next),
                     SR_UNSELECT_OTHERS | SR_SIMULATE) )
                {
                    wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING, this, next);
                    changingEvent.m_itemOld = htSel;

                    if ( IsTreeEventAllowed(changingEvent) )
                    {
                        SelectRange(GetHwnd(), HITEM(m_htSelStart), HITEM(next),
                                    SR_UNSELECT_OTHERS);

                        wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED, this, next);
                        changedEvent.m_itemOld = htSel;
                        (void)HandleTreeEvent(changedEvent);
                    }
                }

                SetFocusedItem(next);
            }
            break;

        case VK_LEFT:
            if ( HasChildren(htSel) && IsExpanded(htSel) )
            {
                Collapse(htSel);
            }
            else
            {
                wxTreeItemId next = GetItemParent(htSel);

                if ( next.IsOk() && !IsHiddenRoot(next) )
                {
                    wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                              this, next);
                    changingEvent.m_itemOld = htSel;

                    if ( IsTreeEventAllowed(changingEvent) )
                    {
                        DoUnselectAll();
                        DoSelectItem(next);
                        SetFocusedItem(next);

                        wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                                 this, next);
                        changedEvent.m_itemOld = htSel;
                        (void)HandleTreeEvent(changedEvent);
                    }
                }
            }
            break;

        case VK_RIGHT:
            if ( !IsVisible(htSel) )
            {
                EnsureVisible(htSel);
            }

            if ( !HasChildren(htSel) )
                break;

            if ( !IsExpanded(htSel) )
            {
                Expand(htSel);
            }
            else
            {
                wxTreeItemId next = TreeView_GetChild(GetHwnd(), htSel);

                wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING, this, next);
                changingEvent.m_itemOld = htSel;

                if ( IsTreeEventAllowed(changingEvent) )
                {
                    DoUnselectAll();
                    DoSelectItem(next);
                    SetFocusedItem(next);

                    wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED, this, next);
                    changedEvent.m_itemOld = htSel;
                    (void)HandleTreeEvent(changedEvent);
                }
            }
            break;

        case VK_HOME:
        case VK_END:
            {
                wxTreeItemId next = GetRootItem();

                if ( IsHiddenRoot(next) )
                {
                    next = TreeView_GetChild(GetHwnd(), HITEM(next));
                }

                if ( !next.IsOk() )
                    break;

                if ( vkey == VK_END )
                {
                    for ( ;; )
                    {
                        wxTreeItemId nextTemp = TreeView_GetNextVisible(
                                                    GetHwnd(), HITEM(next));

                        if ( !nextTemp.IsOk() )
                            break;

                        next = nextTemp;
                    }
                }

                if ( htSel == HITEM(next) )
                    break;

                if ( bShift )
                {
                    if ( !m_htSelStart )
                    {
                        m_htSelStart = htSel;
                    }

                    if ( SelectRange(GetHwnd(),
                                     HITEM(m_htSelStart), HITEM(next),
                                     SR_UNSELECT_OTHERS | SR_SIMULATE) )
                    {
                        wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                                  this, next);
                        changingEvent.m_itemOld = htSel;

                        if ( IsTreeEventAllowed(changingEvent) )
                        {
                            SelectRange(GetHwnd(),
                                        HITEM(m_htSelStart), HITEM(next),
                                        SR_UNSELECT_OTHERS);
                            SetFocusedItem(next);

                            wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                                     this, next);
                            changedEvent.m_itemOld = htSel;
                            (void)HandleTreeEvent(changedEvent);
                        }
                    }
                }
                else // no Shift
                {
                    wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                              this, next);
                    changingEvent.m_itemOld = htSel;

                    if ( IsTreeEventAllowed(changingEvent) )
                    {
                        DoUnselectAll();
                        DoSelectItem(next);
                        SetFocusedItem(next);

                        wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                                 this, next);
                        changedEvent.m_itemOld = htSel;
                        (void)HandleTreeEvent(changedEvent);
                    }
                }
            }
            break;

        case VK_PRIOR:
        case VK_NEXT:
            if ( bCtrl )
            {
                wxTreeItemId firstVisible = GetFirstVisibleItem();
                size_t visibleCount = TreeView_GetVisibleCount(GetHwnd());
                wxTreeItemId nextAdjacent = (vkey == VK_PRIOR) ?
                    TreeView_GetPrevVisible(GetHwnd(), HITEM(firstVisible)) :
                    TreeView_GetNextVisible(GetHwnd(), HITEM(firstVisible));

                if ( !nextAdjacent )
                {
                    break;
                }

                wxTreeItemId nextStart = firstVisible;

                for ( size_t n = 1; n < visibleCount; n++ )
                {
                    wxTreeItemId nextTemp = (vkey == VK_PRIOR) ?
                        TreeView_GetPrevVisible(GetHwnd(), HITEM(nextStart)) :
                        TreeView_GetNextVisible(GetHwnd(), HITEM(nextStart));

                    if ( nextTemp.IsOk() )
                    {
                        nextStart = nextTemp;
                    }
                    else
                    {
                        break;
                    }
                }

                EnsureVisible(nextStart);

                if ( vkey == VK_NEXT )
                {
                    wxTreeItemId nextEnd = nextStart;

                    for ( size_t n = 1; n < visibleCount; n++ )
                    {
                        wxTreeItemId nextTemp =
                            TreeView_GetNextVisible(GetHwnd(), HITEM(nextEnd));

                        if ( nextTemp.IsOk() )
                        {
                            nextEnd = nextTemp;
                        }
                        else
                        {
                            break;
                        }
                    }

                    EnsureVisible(nextEnd);
                }
            }
            else // no Ctrl
            {
                size_t visibleCount = TreeView_GetVisibleCount(GetHwnd());
                wxTreeItemId nextAdjacent = (vkey == VK_PRIOR) ?
                    TreeView_GetPrevVisible(GetHwnd(), htSel) :
                    TreeView_GetNextVisible(GetHwnd(), htSel);

                if ( !nextAdjacent )
                {
                    break;
                }

                wxTreeItemId next(htSel);

                for ( size_t n = 1; n < visibleCount; n++ )
                {
                    wxTreeItemId nextTemp = vkey == VK_PRIOR ?
                        TreeView_GetPrevVisible(GetHwnd(), HITEM(next)) :
                        TreeView_GetNextVisible(GetHwnd(), HITEM(next));

                    if ( !nextTemp.IsOk() )
                        break;

                    next = nextTemp;
                }

                wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                          this, next);
                changingEvent.m_itemOld = htSel;

                if ( IsTreeEventAllowed(changingEvent) )
                {
                    DoUnselectAll();
                    m_htSelStart.Unset();
                    DoSelectItem(next);
                    SetFocusedItem(next);

                    wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                             this, next);
                    changedEvent.m_itemOld = htSel;
                    (void)HandleTreeEvent(changedEvent);
                }
            }
            break;

        default:
            return false;
    }

    return true;
}

bool wxTreeCtrl::MSWHandleTreeKeyDownEvent(WXWPARAM wParam, WXLPARAM lParam)
{
    wxTreeEvent keyEvent(wxEVT_TREE_KEY_DOWN, this);
    keyEvent.m_evtKey = CreateKeyEvent(wxEVT_KEY_DOWN, wParam, lParam);

    bool processed = HandleTreeEvent(keyEvent);

    // generate a separate event for Space/Return
    if ( !wxIsCtrlDown() && !wxIsShiftDown() && !wxIsAltDown() &&
         ((wParam == VK_SPACE) || (wParam == VK_RETURN)) )
    {
        const HTREEITEM htSel = (HTREEITEM)TreeView_GetSelection(GetHwnd());
        if ( htSel )
        {
            wxTreeEvent activatedEvent(wxEVT_TREE_ITEM_ACTIVATED,
                                       this, htSel);
            (void)HandleTreeEvent(activatedEvent);
        }
    }

    return processed;
}

// we hook into WndProc to process WM_MOUSEMOVE/WM_BUTTONUP messages - as we
// only do it during dragging, minimize wxWin overhead (this is important for
// WM_MOUSEMOVE as they're a lot of them) by catching Windows messages directly
// instead of passing by wxWin events
WXLRESULT
wxTreeCtrl::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    bool processed = false;
    WXLRESULT rc = 0;
    bool isMultiple = HasFlag(wxTR_MULTIPLE);

    if ( nMsg == WM_CONTEXTMENU )
    {
        int x = GET_X_LPARAM(lParam),
            y = GET_Y_LPARAM(lParam);

        // the item for which the menu should be shown
        wxTreeItemId item;

        // the position where the menu should be shown in client coordinates
        // (so that it can be passed directly to PopupMenu())
        wxPoint pt;

        if ( x == -1 || y == -1 )
        {
            // this means that the event was generated from keyboard (e.g. with
            // Shift-F10 or special Windows menu key)
            //
            // use the Explorer standard of putting the menu at the left edge
            // of the text, in the vertical middle of the text
            item = wxTreeItemId(TreeView_GetSelection(GetHwnd()));
            if ( item.IsOk() )
            {
                // Use the bounding rectangle of only the text part
                wxRect rect;
                GetBoundingRect(item, rect, true);
                pt = wxPoint(rect.GetX(), rect.GetY() + rect.GetHeight() / 2);
            }
        }
        else // event from mouse, use mouse position
        {
            pt = ScreenToClient(wxPoint(x, y));

            TV_HITTESTINFO tvhti;
            tvhti.pt.x = pt.x;
            tvhti.pt.y = pt.y;

            if ( TreeView_HitTest(GetHwnd(), &tvhti) )
                item = wxTreeItemId(tvhti.hItem);
        }

        // create the event
        if ( item.IsOk() )
        {
            wxTreeEvent event(wxEVT_TREE_ITEM_MENU, this, item);

            event.m_pointDrag = pt;

            if ( HandleTreeEvent(event) )
                processed = true;
            //else: continue with generating wxEVT_CONTEXT_MENU in base class code
        }
    }
    else if ( (nMsg >= WM_MOUSEFIRST) && (nMsg <= WM_MOUSELAST) )
    {
        // we only process mouse messages here and these parameters have the
        // same meaning for all of them
        int x = GET_X_LPARAM(lParam),
            y = GET_Y_LPARAM(lParam);

        TV_HITTESTINFO tvht;
        tvht.pt.x = x;
        tvht.pt.y = y;

        HTREEITEM htOldItem = TreeView_GetSelection(GetHwnd());
        HTREEITEM htItem = TreeView_HitTest(GetHwnd(), &tvht);

        switch ( nMsg )
        {
            case WM_LBUTTONDOWN:
                if ( !isMultiple )
                    break;

                m_htClickedItem.Unset();

                if ( !MSWIsOnItem(tvht.flags) )
                {
                    if ( tvht.flags & TVHT_ONITEMBUTTON )
                    {
                        // either it's going to be handled by user code or
                        // we're going to use it ourselves to toggle the
                        // branch, in either case don't pass it to the base
                        // class which would generate another mouse click event
                        // for it even though it's already handled here
                        processed = true;
                        SetFocus();

                        if ( !HandleMouseEvent(nMsg, x, y, wParam) )
                        {
                            if ( !IsExpanded(htItem) )
                            {
                                Expand(htItem);
                            }
                            else
                            {
                                Collapse(htItem);
                            }
                        }
                    }

                    m_focusLost = false;
                    break;
                }

                processed = true;
                SetFocus();
                m_htClickedItem = (WXHTREEITEM) htItem;
                m_ptClick = wxPoint(x, y);

                if ( wParam & MK_CONTROL )
                {
                    if ( HandleMouseEvent(nMsg, x, y, wParam) )
                    {
                        m_htClickedItem.Unset();
                        break;
                    }

                    wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                              this, htItem);
                    changingEvent.m_itemOld = htOldItem;

                    if ( IsTreeEventAllowed(changingEvent) )
                    {
                        // toggle selected state
                        DoToggleItemSelection(wxTreeItemId(htItem));

                        SetFocusedItem(wxTreeItemId(htItem));

                        // reset on any click without Shift
                        m_htSelStart.Unset();

                        wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                                 this, htItem);
                        changedEvent.m_itemOld = htOldItem;
                        (void)HandleTreeEvent(changedEvent);
                    }
                }
                else if ( wParam & MK_SHIFT )
                {
                    if ( HandleMouseEvent(nMsg, x, y, wParam) )
                    {
                        m_htClickedItem.Unset();
                        break;
                    }

                    int srFlags = 0;
                    bool willChange = true;

                    if ( !(wParam & MK_CONTROL) )
                    {
                        srFlags |= SR_UNSELECT_OTHERS;
                    }

                    if ( !m_htSelStart )
                    {
                        // take the focused item
                        m_htSelStart = htOldItem;
                    }
                    else
                    {
                        willChange = SelectRange(GetHwnd(), HITEM(m_htSelStart),
                                                 htItem, srFlags | SR_SIMULATE);
                    }

                    if ( willChange )
                    {
                        wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                                  this, htItem);
                        changingEvent.m_itemOld = htOldItem;

                        if ( IsTreeEventAllowed(changingEvent) )
                        {
                            // this selects all items between the starting one
                            // and the current
                            if ( m_htSelStart )
                            {
                                SelectRange(GetHwnd(), HITEM(m_htSelStart),
                                            htItem, srFlags);
                            }
                            else
                            {
                                DoSelectItem(wxTreeItemId(htItem));
                            }

                            SetFocusedItem(wxTreeItemId(htItem));

                            wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                                     this, htItem);
                            changedEvent.m_itemOld = htOldItem;
                            (void)HandleTreeEvent(changedEvent);
                        }
                    }
                }
                else // normal click
                {
                    // avoid doing anything if we click on the only
                    // currently selected item

                    wxArrayTreeItemIds selections;
                    size_t count = GetSelections(selections);

                    if ( count == 0 ||
                         count > 1 ||
                         HITEM(selections[0]) != htItem )
                    {
                        if ( HandleMouseEvent(nMsg, x, y, wParam) )
                        {
                            m_htClickedItem.Unset();
                            break;
                        }

                        // clear the previously selected items, if the user
                        // clicked outside of the present selection, otherwise,
                        // perform the deselection on mouse-up, this allows
                        // multiple drag and drop to work.
                        if ( !IsItemSelected(GetHwnd(), htItem))
                        {
                            wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                                      this, htItem);
                            changingEvent.m_itemOld = htOldItem;

                            if ( IsTreeEventAllowed(changingEvent) )
                            {
                                DoUnselectAll();
                                DoSelectItem(wxTreeItemId(htItem));
                                SetFocusedItem(wxTreeItemId(htItem));

                                wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                                         this, htItem);
                                changedEvent.m_itemOld = htOldItem;
                                (void)HandleTreeEvent(changedEvent);
                            }
                        }
                        else
                        {
                            SetFocusedItem(wxTreeItemId(htItem));
                            m_mouseUpDeselect = true;
                        }
                    }
                    else // click on a single selected item
                    {
                        // don't interfere with the default processing in
                        // WM_MOUSEMOVE handler below as the default window
                        // proc will start the drag itself if we let have
                        // WM_LBUTTONDOWN
                        m_htClickedItem.Unset();

                        // prevent in-place editing from starting if focus lost
                        // since previous click
                        if ( m_focusLost )
                        {
                            ClearFocusedItem();
                            DoSelectItem(wxTreeItemId(htItem));
                            SetFocusedItem(wxTreeItemId(htItem));
                        }
                        else
                        {
                            processed = false;
                        }
                    }

                    // reset on any click without Shift
                    m_htSelStart.Unset();
                }

                m_focusLost = false;

                // we consumed the event so we need to trigger state image
                // click if needed
                if ( processed )
                {
                    if ( tvht.flags & TVHT_ONITEMSTATEICON )
                    {
                        m_triggerStateImageClick = true;
                    }
                }
                break;

            case WM_RBUTTONDOWN:
                if ( !isMultiple )
                    break;

                processed = true;
                SetFocus();

                if ( HandleMouseEvent(nMsg, x, y, wParam) || !htItem )
                {
                    break;
                }

                // default handler removes the highlight from the currently
                // focused item when right mouse button is pressed on another
                // one but keeps the remaining items highlighted, which is
                // confusing, so override this default behaviour
                if ( !IsItemSelected(GetHwnd(), htItem) )
                {
                    wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                              this, htItem);
                    changingEvent.m_itemOld = htOldItem;

                    if ( IsTreeEventAllowed(changingEvent) )
                    {
                        DoUnselectAll();
                        DoSelectItem(wxTreeItemId(htItem));
                        SetFocusedItem(wxTreeItemId(htItem));

                        wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                                 this, htItem);
                        changedEvent.m_itemOld = htOldItem;
                        (void)HandleTreeEvent(changedEvent);
                    }
                }

                break;

            case WM_MOUSEMOVE:
#ifndef __WXWINCE__
                if ( m_htClickedItem )
                {
                    int cx = abs(m_ptClick.x - x);
                    int cy = abs(m_ptClick.y - y);

                    if ( cx > ::GetSystemMetrics(SM_CXDRAG) ||
                            cy > ::GetSystemMetrics(SM_CYDRAG) )
                    {
                        NM_TREEVIEW tv;
                        wxZeroMemory(tv);

                        tv.hdr.hwndFrom = GetHwnd();
                        tv.hdr.idFrom = ::GetWindowLong(GetHwnd(), GWL_ID);
                        tv.hdr.code = TVN_BEGINDRAG;

                        tv.itemNew.hItem = HITEM(m_htClickedItem);


                        TVITEM tviAux;
                        wxZeroMemory(tviAux);

                        tviAux.hItem = HITEM(m_htClickedItem);
                        tviAux.mask = TVIF_STATE | TVIF_PARAM;
                        tviAux.stateMask = 0xffffffff;
                        TreeView_GetItem(GetHwnd(), &tviAux);

                        tv.itemNew.state = tviAux.state;
                        tv.itemNew.lParam = tviAux.lParam;

                        tv.ptDrag.x = x;
                        tv.ptDrag.y = y;

                        // do it before SendMessage() call below to avoid
                        // reentrancies here if there is another WM_MOUSEMOVE
                        // in the queue already
                        m_htClickedItem.Unset();

                        ::SendMessage(GetHwndOf(GetParent()), WM_NOTIFY,
                                      tv.hdr.idFrom, (LPARAM)&tv );

                        // don't pass it to the default window proc, it would
                        // start dragging again
                        processed = true;
                    }
                }
#endif // __WXWINCE__

#if wxUSE_DRAGIMAGE
                if ( m_dragImage )
                {
                    m_dragImage->Move(wxPoint(x, y));
                    if ( htItem )
                    {
                        // highlight the item as target (hiding drag image is
                        // necessary - otherwise the display will be corrupted)
                        m_dragImage->Hide();
                        TreeView_SelectDropTarget(GetHwnd(), htItem);
                        m_dragImage->Show();
                    }
                }
#endif // wxUSE_DRAGIMAGE
                break;

            case WM_LBUTTONUP:
                if ( isMultiple )
                {
                    // deselect other items if needed
                    if ( htItem )
                    {
                        if ( m_mouseUpDeselect )
                        {
                            m_mouseUpDeselect = false;

                            wxTreeEvent changingEvent(wxEVT_TREE_SEL_CHANGING,
                                                      this, htItem);
                            changingEvent.m_itemOld = htOldItem;

                            if ( IsTreeEventAllowed(changingEvent) )
                            {
                                DoUnselectAll();
                                DoSelectItem(wxTreeItemId(htItem));
                                SetFocusedItem(wxTreeItemId(htItem));

                                wxTreeEvent changedEvent(wxEVT_TREE_SEL_CHANGED,
                                                         this, htItem);
                                changedEvent.m_itemOld = htOldItem;
                                (void)HandleTreeEvent(changedEvent);
                            }
                        }
                    }

                    m_htClickedItem.Unset();

                    if ( m_triggerStateImageClick )
                    {
                        if ( tvht.flags & TVHT_ONITEMSTATEICON )
                        {
                            wxTreeEvent event(wxEVT_TREE_STATE_IMAGE_CLICK,
                                              this, htItem);
                            (void)HandleTreeEvent(event);

                            m_triggerStateImageClick = false;
                            processed = true;
                        }
                    }

                    if ( !m_dragStarted && MSWIsOnItem(tvht.flags) )
                    {
                        processed = true;
                    }
                }

                // fall through

            case WM_RBUTTONUP:
#if wxUSE_DRAGIMAGE
                if ( m_dragImage )
                {
                    m_dragImage->EndDrag();
                    wxDELETE(m_dragImage);

                    // generate the drag end event
                    wxTreeEvent event(wxEVT_TREE_END_DRAG,
                                      this, htItem);
                    event.m_pointDrag = wxPoint(x, y);
                    (void)HandleTreeEvent(event);

                    // if we don't do it, the tree seems to think that 2 items
                    // are selected simultaneously which is quite weird
                    TreeView_SelectDropTarget(GetHwnd(), 0);
                }
#endif // wxUSE_DRAGIMAGE

                if ( isMultiple && nMsg == WM_RBUTTONUP )
                {
                    // send NM_RCLICK
                    NMHDR nmhdr;
                    nmhdr.hwndFrom = GetHwnd();
                    nmhdr.idFrom = ::GetWindowLong(GetHwnd(), GWL_ID);
                    nmhdr.code = NM_RCLICK;
                    ::SendMessage(::GetParent(GetHwnd()), WM_NOTIFY,
                                  nmhdr.idFrom, (LPARAM)&nmhdr);
                    processed = true;
                }

                m_dragStarted = false;

                break;
        }
    }
    else if ( (nMsg == WM_SETFOCUS || nMsg == WM_KILLFOCUS) )
    {
        if ( isMultiple )
        {
            // the tree control greys out the selected item when it loses focus
            // and paints it as selected again when it regains it, but it won't
            // do it for the other items itself - help it
            wxArrayTreeItemIds selections;
            size_t count = GetSelections(selections);
            TVGetItemRectParam param;

            for ( size_t n = 0; n < count; n++ )
            {
                // TreeView_GetItemRect() will return false if item is not
                // visible, which may happen perfectly well
                if ( wxTreeView_GetItemRect(GetHwnd(), HITEM(selections[n]),
                                            param, TRUE) )
                {
                    ::InvalidateRect(GetHwnd(), &param.rect, FALSE);
                }
            }
        }

        if ( nMsg == WM_KILLFOCUS )
        {
            m_focusLost = true;
        }
    }
    else if ( (nMsg == WM_KEYDOWN || nMsg == WM_SYSKEYDOWN) && isMultiple )
    {
        // normally we want to generate wxEVT_KEY_DOWN events from TVN_KEYDOWN
        // notification but for the keys which can be used to change selection
        // we need to do it from here so as to not apply the default behaviour
        // if the events are handled by the user code
        switch ( wParam )
        {
            case VK_RETURN:
            case VK_SPACE:
            case VK_UP:
            case VK_DOWN:
            case VK_LEFT:
            case VK_RIGHT:
            case VK_HOME:
            case VK_END:
            case VK_PRIOR:
            case VK_NEXT:
                if ( !HandleKeyDown(wParam, lParam) &&
                        !MSWHandleTreeKeyDownEvent(wParam, lParam) )
                {
                    // use the key to update the selection if it was left
                    // unprocessed
                    MSWHandleSelectionKey(wParam);
                }

                // pretend that we did process it in any case as we already
                // generated an event for it
                processed = true;

            //default: for all the other keys leave processed as false so that
            //         the tree control generates a TVN_KEYDOWN for us
        }

    }
    else if ( nMsg == WM_COMMAND )
    {
        // if we receive a EN_KILLFOCUS command from the in-place edit control
        // used for label editing, make sure to end editing
        WORD id, cmd;
        WXHWND hwnd;
        UnpackCommand(wParam, lParam, &id, &hwnd, &cmd);

        if ( cmd == EN_KILLFOCUS )
        {
            if ( m_textCtrl && m_textCtrl->GetHandle() == hwnd )
            {
                DoEndEditLabel();

                processed = true;
            }
        }
    }

    if ( !processed )
        rc = wxControl::MSWWindowProc(nMsg, wParam, lParam);

    return rc;
}

WXLRESULT
wxTreeCtrl::MSWDefWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    if ( nMsg == WM_CHAR )
    {
        // don't let the control process Space and Return keys because it
        // doesn't do anything useful with them anyhow but always beeps
        // annoyingly when it receives them and there is no way to turn it off
        // simply if you just process TREEITEM_ACTIVATED event to which Space
        // and Enter presses are mapped in your code
        if ( wParam == VK_SPACE || wParam == VK_RETURN )
            return 0;
    }
#if wxUSE_DRAGIMAGE
    else if ( nMsg == WM_KEYDOWN )
    {
        if ( wParam == VK_ESCAPE )
        {
            if ( m_dragImage )
            {
                m_dragImage->EndDrag();
                wxDELETE(m_dragImage);

                // if we don't do it, the tree seems to think that 2 items
                // are selected simultaneously which is quite weird
                TreeView_SelectDropTarget(GetHwnd(), 0);
            }
        }
    }
#endif // wxUSE_DRAGIMAGE

    return wxControl::MSWDefWindowProc(nMsg, wParam, lParam);
}

// process WM_NOTIFY Windows message
bool wxTreeCtrl::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result)
{
    wxTreeEvent event(wxEVT_NULL, this);
    wxEventType eventType = wxEVT_NULL;
    NMHDR *hdr = (NMHDR *)lParam;

    switch ( hdr->code )
    {
        case TVN_BEGINDRAG:
            eventType = wxEVT_TREE_BEGIN_DRAG;
            // fall through

        case TVN_BEGINRDRAG:
            {
                if ( eventType == wxEVT_NULL )
                    eventType = wxEVT_TREE_BEGIN_RDRAG;
                //else: left drag, already set above

                NM_TREEVIEW *tv = (NM_TREEVIEW *)lParam;

                event.m_item = tv->itemNew.hItem;
                event.m_pointDrag = wxPoint(tv->ptDrag.x, tv->ptDrag.y);

                // don't allow dragging by default: the user code must
                // explicitly say that it wants to allow it to avoid breaking
                // the old apps
                event.Veto();
            }
            break;

        case TVN_BEGINLABELEDIT:
            {
                eventType = wxEVT_TREE_BEGIN_LABEL_EDIT;
                NMTVDISPINFO *info = (NMTVDISPINFO *)lParam;

                // although the user event handler may still veto it, it is
                // important to set it now so that calls to SetItemText() from
                // the event handler would change the text controls contents
                m_idEdited =
                event.m_item = info->item.hItem;
                event.m_label = info->item.pszText;
                event.m_editCancelled = false;
            }
            break;

        case TVN_DELETEITEM:
            {
                eventType = wxEVT_TREE_DELETE_ITEM;
                NM_TREEVIEW *tv = (NM_TREEVIEW *)lParam;

                event.m_item = tv->itemOld.hItem;

                if ( m_hasAnyAttr )
                {
                    wxMapTreeAttr::iterator it = m_attrs.find(tv->itemOld.hItem);
                    if ( it != m_attrs.end() )
                    {
                        delete it->second;
                        m_attrs.erase(it);
                    }
                }
            }
            break;

        case TVN_ENDLABELEDIT:
            {
                eventType = wxEVT_TREE_END_LABEL_EDIT;
                NMTVDISPINFO *info = (NMTVDISPINFO *)lParam;

                event.m_item = info->item.hItem;
                event.m_label = info->item.pszText;
                event.m_editCancelled = info->item.pszText == NULL;
                break;
            }

#ifndef __WXWINCE__
        // These *must* not be removed or TVN_GETINFOTIP will
        // not be processed each time the mouse is moved
        // and the tooltip will only ever update once.
        case TTN_NEEDTEXTA:
        case TTN_NEEDTEXTW:
            {
                *result = 0;

                break;
            }

#ifdef TVN_GETINFOTIP
        case TVN_GETINFOTIP:
            {
                eventType = wxEVT_TREE_ITEM_GETTOOLTIP;
                NMTVGETINFOTIP *info = (NMTVGETINFOTIP*)lParam;

                // Which item are we trying to get a tooltip for?
                event.m_item = info->hItem;

                break;
            }
#endif // TVN_GETINFOTIP
#endif // !__WXWINCE__

        case TVN_GETDISPINFO:
            eventType = wxEVT_TREE_GET_INFO;
            // fall through

        case TVN_SETDISPINFO:
            {
                if ( eventType == wxEVT_NULL )
                    eventType = wxEVT_TREE_SET_INFO;
                //else: get, already set above

                NMTVDISPINFO *info = (NMTVDISPINFO *)lParam;

                event.m_item = info->item.hItem;
                break;
            }

        case TVN_ITEMEXPANDING:
        case TVN_ITEMEXPANDED:
            {
                NM_TREEVIEW *tv = (NM_TREEVIEW*)lParam;

                int what;
                switch ( tv->action )
                {
                    default:
                        wxLogDebug(wxT("unexpected code %d in TVN_ITEMEXPAND message"), tv->action);
                        // fall through

                    case TVE_EXPAND:
                        what = IDX_EXPAND;
                        break;

                    case TVE_COLLAPSE:
                        what = IDX_COLLAPSE;
                        break;
                }

                int how = hdr->code == TVN_ITEMEXPANDING ? IDX_DOING
                                                         : IDX_DONE;

                eventType = gs_expandEvents[what][how];

                event.m_item = tv->itemNew.hItem;
            }
            break;

        case TVN_KEYDOWN:
            {
                TV_KEYDOWN *info = (TV_KEYDOWN *)lParam;

                // fabricate the lParam and wParam parameters sufficiently
                // similar to the ones from a "real" WM_KEYDOWN so that
                // CreateKeyEvent() works correctly
                return MSWHandleTreeKeyDownEvent(
                        info->wVKey, (wxIsAltDown() ? KF_ALTDOWN : 0) << 16);
            }


        // Vista's tree control has introduced some problems with our
        // multi-selection tree.  When TreeView_SelectItem() is called,
        // the wrong items are deselected.

        // Fortunately, Vista provides a new notification, TVN_ITEMCHANGING
        // that can be used to regulate this incorrect behaviour.  The
        // following messages will allow only the unlocked item's selection
        // state to change

        case TVN_ITEMCHANGINGA:
        case TVN_ITEMCHANGINGW:
            {
                // we only need to handles these in multi-select trees
                if ( HasFlag(wxTR_MULTIPLE) )
                {
                    // get info about the item about to be changed
                    NMTVITEMCHANGE* info = (NMTVITEMCHANGE*)lParam;
                    if (TreeItemUnlocker::IsLocked(info->hItem))
                    {
                        // item's state is locked, don't allow the change
                        // returning 1 will disallow the change
                        *result = 1;
                        return true;
                    }
                }

                // allow the state change
            }
            return false;

        // NB: MSLU is broken and sends TVN_SELCHANGEDA instead of
        //     TVN_SELCHANGEDW in Unicode mode under Win98. Therefore
        //     we have to handle both messages:
        case TVN_SELCHANGEDA:
        case TVN_SELCHANGEDW:
            if ( !m_changingSelection )
            {
                eventType = wxEVT_TREE_SEL_CHANGED;
            }
            // fall through

        case TVN_SELCHANGINGA:
        case TVN_SELCHANGINGW:
            if ( !m_changingSelection )
            {
                if ( eventType == wxEVT_NULL )
                    eventType = wxEVT_TREE_SEL_CHANGING;
                //else: already set above

                if (hdr->code == TVN_SELCHANGINGW ||
                    hdr->code == TVN_SELCHANGEDW)
                {
                    NM_TREEVIEWW *tv = (NM_TREEVIEWW *)lParam;
                    event.m_item = tv->itemNew.hItem;
                    event.m_itemOld = tv->itemOld.hItem;
                }
                else
                {
                    NM_TREEVIEWA *tv = (NM_TREEVIEWA *)lParam;
                    event.m_item = tv->itemNew.hItem;
                    event.m_itemOld = tv->itemOld.hItem;
                }
            }

            // we receive this message from WM_LBUTTONDOWN handler inside
            // comctl32.dll and so before the click is passed to
            // DefWindowProc() which sets the focus to the window which was
            // clicked and this can lead to unexpected event sequences: for
            // example, we may get a "selection change" event from the tree
            // before getting a "kill focus" event for the text control which
            // had the focus previously, thus breaking user code doing input
            // validation
            //
            // to avoid such surprises, we force the generation of focus events
            // now, before we generate the selection change ones
            if ( !m_changingSelection && !m_isBeingDeleted )
                SetFocus();
            break;

        // instead of explicitly checking for _WIN32_IE, check if the
        // required symbols are available in the headers
#if defined(CDDS_PREPAINT)
        case NM_CUSTOMDRAW:
            {
                LPNMTVCUSTOMDRAW lptvcd = (LPNMTVCUSTOMDRAW)lParam;
                NMCUSTOMDRAW& nmcd = lptvcd->nmcd;
                switch ( nmcd.dwDrawStage )
                {
                    case CDDS_PREPAINT:
                        // if we've got any items with non standard attributes,
                        // notify us before painting each item
                        *result = m_hasAnyAttr ? CDRF_NOTIFYITEMDRAW
                                               : CDRF_DODEFAULT;

                        // windows in TreeCtrl use one-based index for item state images,
                        // 0 indexed image is not being used, we're using zero-based index,
                        // so we have to add temp image (of zero index) to state image list
                        // before we draw any item, then after items are drawn we have to
                        // delete it (in POSTPAINT notify)
                        if (m_imageListState && m_imageListState->GetImageCount() > 0)
                        {
                            typedef BOOL (wxSTDCALL *ImageList_Copy_t)
                                (HIMAGELIST, int, HIMAGELIST, int, UINT);
                            static ImageList_Copy_t s_pfnImageList_Copy = NULL;
                            static bool loaded = false;

                            if ( !loaded )
                            {
                                wxLoadedDLL dllComCtl32(wxT("comctl32.dll"));
                                if ( dllComCtl32.IsLoaded() )
                                {
                                    wxDL_INIT_FUNC(s_pfn, ImageList_Copy, dllComCtl32);
                                    loaded = true;
                                }
                            }

                            if ( !s_pfnImageList_Copy )
                            {
                                // this code is broken with ImageList_Copy()
                                // but I don't care enough about Win95 support
                                // to write it now -- if anybody does, please
                                // do it
                                wxFAIL_MSG("TODO: implement this for Win95");
                                break;
                            }

                            const HIMAGELIST
                                hImageList = GetHimagelistOf(m_imageListState);

                            // add temporary image
                            int width, height;
                            m_imageListState->GetSize(0, width, height);

                            HBITMAP hbmpTemp = ::CreateBitmap(width, height, 1, 1, NULL);
                            int index = ::ImageList_Add(hImageList, hbmpTemp, hbmpTemp);
                            ::DeleteObject(hbmpTemp);

                            if ( index != -1 )
                            {
                                // move images to right
                                for ( int i = index; i > 0; i-- )
                                {
                                    (*s_pfnImageList_Copy)(hImageList, i,
                                                           hImageList, i-1,
                                                           ILCF_MOVE);
                                }

                                // we must remove the image in POSTPAINT notify
                                *result |= CDRF_NOTIFYPOSTPAINT;
                            }
                        }
                        break;

                    case CDDS_POSTPAINT:
                        // we are deleting temp image of 0 index, which was
                        // added before items were drawn (in PREPAINT notify)
                        if (m_imageListState && m_imageListState->GetImageCount() > 0)
                            m_imageListState->Remove(0);
                        break;

                    case CDDS_ITEMPREPAINT:
                        {
                            wxMapTreeAttr::iterator
                                it = m_attrs.find((void *)nmcd.dwItemSpec);

                            if ( it == m_attrs.end() )
                            {
                                // nothing to do for this item
                                *result = CDRF_DODEFAULT;
                                break;
                            }

                            wxTreeItemAttr * const attr = it->second;

                            wxTreeViewItem tvItem((void *)nmcd.dwItemSpec,
                                                  TVIF_STATE, TVIS_DROPHILITED);
                            DoGetItem(&tvItem);
                            const UINT tvItemState = tvItem.state;

                            // selection colours should override ours,
                            // otherwise it is too confusing to the user
                            if ( !(nmcd.uItemState & CDIS_SELECTED) &&
                                 !(tvItemState & TVIS_DROPHILITED) )
                            {
                                wxColour colBack;
                                if ( attr->HasBackgroundColour() )
                                {
                                    colBack = attr->GetBackgroundColour();
                                    lptvcd->clrTextBk = wxColourToRGB(colBack);
                                }
                            }

                            // but we still want to keep the special foreground
                            // colour when we don't have focus (we can't keep
                            // it when we do, it would usually be unreadable on
                            // the almost inverted bg colour...)
                            if ( ( !(nmcd.uItemState & CDIS_SELECTED) ||
                                    FindFocus() != this ) &&
                                 !(tvItemState & TVIS_DROPHILITED) )
                            {
                                wxColour colText;
                                if ( attr->HasTextColour() )
                                {
                                    colText = attr->GetTextColour();
                                    lptvcd->clrText = wxColourToRGB(colText);
                                }
                            }

                            if ( attr->HasFont() )
                            {
                                HFONT hFont = GetHfontOf(attr->GetFont());

                                ::SelectObject(nmcd.hdc, hFont);

                                *result = CDRF_NEWFONT;
                            }
                            else // no specific font
                            {
                                *result = CDRF_DODEFAULT;
                            }
                        }
                        break;

                    default:
                        *result = CDRF_DODEFAULT;
                }
            }

            // we always process it
            return true;
#endif // have owner drawn support in headers

        case NM_CLICK:
            {
                DWORD pos = GetMessagePos();
                POINT point;
                point.x = GET_X_LPARAM(pos);
                point.y = GET_Y_LPARAM(pos);
                ::MapWindowPoints(HWND_DESKTOP, GetHwnd(), &point, 1);
                int htFlags = 0;
                wxTreeItemId item = HitTest(wxPoint(point.x, point.y), htFlags);

                if ( htFlags & wxTREE_HITTEST_ONITEMSTATEICON )
                {
                    event.m_item = item;
                    eventType = wxEVT_TREE_STATE_IMAGE_CLICK;
                }

                break;
            }

        case NM_DBLCLK:
        case NM_RCLICK:
            {
                TV_HITTESTINFO tvhti;
                wxGetCursorPosMSW(&tvhti.pt);
                ::ScreenToClient(GetHwnd(), &tvhti.pt);
                if ( TreeView_HitTest(GetHwnd(), &tvhti) )
                {
                    if ( MSWIsOnItem(tvhti.flags) )
                    {
                        event.m_item = tvhti.hItem;
                        eventType = hdr->code == NM_DBLCLK
                                    ? wxEVT_TREE_ITEM_ACTIVATED
                                    : wxEVT_TREE_ITEM_RIGHT_CLICK;

                        event.m_pointDrag.x = tvhti.pt.x;
                        event.m_pointDrag.y = tvhti.pt.y;
                    }

                    break;
                }
            }
            // fall through

        default:
            return wxControl::MSWOnNotify(idCtrl, lParam, result);
    }

    event.SetEventType(eventType);

    bool processed = HandleTreeEvent(event);

    // post processing
    switch ( hdr->code )
    {
        case NM_DBLCLK:
            // we translate NM_DBLCLK into ACTIVATED event and if the user
            // handled the activation of the item we shouldn't proceed with
            // also using the same double click for toggling the item expanded
            // state -- but OTOH do let the user to expand/collapse the item by
            // double clicking on it if the activation is not handled specially
            *result = processed;
            break;

        case NM_RCLICK:
            // prevent tree control from sending WM_CONTEXTMENU to our parent
            // (which it does if NM_RCLICK is not handled) because we want to
            // send it to the control itself
            *result =
            processed = true;

            ::SendMessage(GetHwnd(), WM_CONTEXTMENU,
                          (WPARAM)GetHwnd(), ::GetMessagePos());
            break;

        case TVN_BEGINDRAG:
        case TVN_BEGINRDRAG:
#if wxUSE_DRAGIMAGE
            if ( event.IsAllowed() )
            {
                // normally this is impossible because the m_dragImage is
                // deleted once the drag operation is over
                wxASSERT_MSG( !m_dragImage, wxT("starting to drag once again?") );

                m_dragImage = new wxDragImage(*this, event.m_item);
                m_dragImage->BeginDrag(wxPoint(0,0), this);
                m_dragImage->Show();

                m_dragStarted = true;
            }
#endif // wxUSE_DRAGIMAGE
            break;

        case TVN_DELETEITEM:
            {
                // NB: we might process this message using wxWidgets event
                //     tables, but due to overhead of wxWin event system we
                //     prefer to do it here ourself (otherwise deleting a tree
                //     with many items is just too slow)
                NM_TREEVIEW *tv = (NM_TREEVIEW *)lParam;

                wxTreeItemParam *param =
                        (wxTreeItemParam *)tv->itemOld.lParam;
                delete param;

                processed = true; // Make sure we don't get called twice
            }
            break;

        case TVN_BEGINLABELEDIT:
            // return true to cancel label editing
            *result = !event.IsAllowed();

            // set ES_WANTRETURN ( like we do in BeginLabelEdit )
            if ( event.IsAllowed() )
            {
                HWND hText = TreeView_GetEditControl(GetHwnd());
                if ( hText )
                {
                    // MBN: if m_textCtrl already has an HWND, it is a stale
                    // pointer from a previous edit (because the user
                    // didn't modify the label before dismissing the control,
                    // and TVN_ENDLABELEDIT was not sent), so delete it
                    if ( m_textCtrl && m_textCtrl->GetHWND() )
                        DeleteTextCtrl();
                    if ( !m_textCtrl )
                        m_textCtrl = new wxTextCtrl();
                    m_textCtrl->SetParent(this);
                    m_textCtrl->SetHWND((WXHWND)hText);
                    m_textCtrl->SubclassWin((WXHWND)hText);

                    // set wxTE_PROCESS_ENTER style for the text control to
                    // force it to process the Enter presses itself, otherwise
                    // they could be stolen from it by the dialog
                    // navigation code
                    m_textCtrl->SetWindowStyle(m_textCtrl->GetWindowStyle()
                                               | wxTE_PROCESS_ENTER);
                }
            }
            else // we had set m_idEdited before
            {
                m_idEdited.Unset();
            }
            break;

        case TVN_ENDLABELEDIT:
            // return true to set the label to the new string: note that we
            // also must pretend that we did process the message or it is going
            // to be passed to DefWindowProc() which will happily return false
            // cancelling the label change
            *result = event.IsAllowed();
            processed = true;

            // ensure that we don't have the text ctrl which is going to be
            // deleted any more
            DeleteTextCtrl();
            break;

#ifndef __WXWINCE__
#ifdef TVN_GETINFOTIP
         case TVN_GETINFOTIP:
            {
                // If the user permitted a tooltip change, change it
                if (event.IsAllowed())
                {
                    SetToolTip(event.m_label);
                }
            }
            break;
#endif
#endif

        case TVN_SELCHANGING:
        case TVN_ITEMEXPANDING:
            // return true to prevent the action from happening
            *result = !event.IsAllowed();
            break;

        case TVN_ITEMEXPANDED:
            {
                NM_TREEVIEW *tv = (NM_TREEVIEW *)lParam;
                const wxTreeItemId id(tv->itemNew.hItem);

                if ( tv->action == TVE_COLLAPSE )
                {
                    if ( wxApp::GetComCtl32Version() >= 600 )
                    {
                        // for some reason the item selection rectangle depends
                        // on whether it is expanded or collapsed (at least
                        // with comctl32.dll v6): it is wider (by 3 pixels) in
                        // the expanded state, so when the item collapses and
                        // then is deselected the rightmost 3 pixels of the
                        // previously drawn selection are left on the screen
                        //
                        // it's not clear if it's a bug in comctl32.dll or in
                        // our code (because it does not happen in Explorer but
                        // OTOH we don't do anything which could result in this
                        // AFAICS) but we do need to work around it to avoid
                        // ugly artifacts
                        RefreshItem(id);
                    }
                }
                else // expand
                {
                    // the item is also not refreshed properly after expansion when
                    // it has an image depending on the expanded/collapsed state:
                    // again, it's not clear if the bug is in comctl32.dll or our
                    // code...
                    int image = GetItemImage(id, wxTreeItemIcon_Expanded);
                    if ( image != -1 )
                    {
                        RefreshItem(id);
                    }
                }
            }
            break;

        case TVN_GETDISPINFO:
            // NB: so far the user can't set the image himself anyhow, so do it
            //     anyway - but this may change later
            //if ( /* !processed && */ )
            {
                wxTreeItemId item = event.m_item;
                NMTVDISPINFO *info = (NMTVDISPINFO *)lParam;

                const wxTreeItemParam * const param = GetItemParam(item);
                if ( !param )
                    break;

                if ( info->item.mask & TVIF_IMAGE )
                {
                    info->item.iImage =
                        param->GetImage
                        (
                         IsExpanded(item) ? wxTreeItemIcon_Expanded
                                          : wxTreeItemIcon_Normal
                        );
                }
                if ( info->item.mask & TVIF_SELECTEDIMAGE )
                {
                    info->item.iSelectedImage =
                        param->GetImage
                        (
                         IsExpanded(item) ? wxTreeItemIcon_SelectedExpanded
                                          : wxTreeItemIcon_Selected
                        );
                }
            }
            break;

        //default:
            // for the other messages the return value is ignored and there is
            // nothing special to do
    }
    return processed;
}

// ----------------------------------------------------------------------------
// State control.
// ----------------------------------------------------------------------------

// why do they define INDEXTOSTATEIMAGEMASK but not the inverse?
#define STATEIMAGEMASKTOINDEX(state) (((state) & TVIS_STATEIMAGEMASK) >> 12)

int wxTreeCtrl::DoGetItemState(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTREE_ITEMSTATE_NONE, wxT("invalid tree item") );

    // receive the desired information
    wxTreeViewItem tvItem(item, TVIF_STATE, TVIS_STATEIMAGEMASK);
    DoGetItem(&tvItem);

    // state images are one-based
    return STATEIMAGEMASKTOINDEX(tvItem.state) - 1;
}

void wxTreeCtrl::DoSetItemState(const wxTreeItemId& item, int state)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxTreeViewItem tvItem(item, TVIF_STATE, TVIS_STATEIMAGEMASK);

    // state images are one-based
    // 0 if no state image display (wxTREE_ITEMSTATE_NONE = -1)
    tvItem.state = INDEXTOSTATEIMAGEMASK(state + 1);

    DoSetItem(&tvItem);
}

// ----------------------------------------------------------------------------
// Update locking.
// ----------------------------------------------------------------------------

// Using WM_SETREDRAW with the native control is a bad idea as it's broken in
// some Windows versions (see http://support.microsoft.com/kb/130611) and
// doesn't seem to do anything in other ones (e.g. under Windows 7 the tree
// control keeps updating its scrollbars while the items are added to it,
// resulting in horrible flicker when adding even a couple of dozen items).
// So we resize it to the smallest possible size instead of freezing -- this
// still flickers, but actually not as badly as it would if we didn't do it.

void wxTreeCtrl::DoFreeze()
{
    if ( IsShown() )
    {
        RECT rc;
        ::GetWindowRect(GetHwnd(), &rc);
        m_thawnSize = wxRectFromRECT(rc).GetSize();

        ::SetWindowPos(GetHwnd(), 0, 0, 0, 1, 1,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE);
    }
}

void wxTreeCtrl::DoThaw()
{
    if ( IsShown() )
    {
        if ( m_thawnSize != wxDefaultSize )
        {
            ::SetWindowPos(GetHwnd(), 0, 0, 0, m_thawnSize.x, m_thawnSize.y,
                           SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

// We also need to override DoSetSize() to ensure that m_thawnSize is reset if
// the window is resized while being frozen -- in this case, we need to avoid
// resizing it back to its original, pre-freeze, size when it's thawed.
void wxTreeCtrl::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    m_thawnSize = wxDefaultSize;

    wxTreeCtrlBase::DoSetSize(x, y, width, height, sizeFlags);
}

#endif // wxUSE_TREECTRL
