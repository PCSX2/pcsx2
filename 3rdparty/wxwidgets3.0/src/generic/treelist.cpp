///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/treelist.cpp
// Purpose:     Generic wxTreeListCtrl implementation.
// Author:      Vadim Zeitlin
// Created:     2011-08-19
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// Declarations
// ============================================================================

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TREELISTCTRL

#ifndef WX_PRECOMP
    #include "wx/dc.h"
#endif // WX_PRECOMP

#include "wx/treelist.h"

#include "wx/dataview.h"
#include "wx/renderer.h"
#include "wx/scopedarray.h"
#include "wx/scopedptr.h"

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

const char wxTreeListCtrlNameStr[] = "wxTreeListCtrl";

const wxTreeListItem wxTLI_FIRST(reinterpret_cast<wxTreeListModelNode*>(-1));
const wxTreeListItem wxTLI_LAST(reinterpret_cast<wxTreeListModelNode*>(-2));

// ----------------------------------------------------------------------------
// wxTreeListModelNode: a node in the internal tree representation.
// ----------------------------------------------------------------------------

class wxTreeListModelNode
{
public:
    wxTreeListModelNode(wxTreeListModelNode* parent,
                        const wxString& text = wxString(),
                        int imageClosed = wxWithImages::NO_IMAGE,
                        int imageOpened = wxWithImages::NO_IMAGE,
                        wxClientData* data = NULL)
        : m_text(text),
          m_parent(parent)
    {
        m_child =
        m_next = NULL;

        m_imageClosed = imageClosed;
        m_imageOpened = imageOpened;

        m_checkedState = wxCHK_UNCHECKED;

        m_data = data;

        m_columnsTexts = NULL;
    }

    // Destroying the node also (recursively) destroys its children.
    ~wxTreeListModelNode()
    {
        for ( wxTreeListModelNode* node = m_child; node; )
        {
            wxTreeListModelNode* child = node;
            node = node->m_next;
            delete child;
        }

        delete m_data;

        delete [] m_columnsTexts;
    }


    // Public fields for the first column text and other simple attributes:
    // there is no need to have accessors/mutators for those as there is no
    // encapsulation anyhow, all of those are exposed in our public API.
    wxString m_text;

    int m_imageClosed,
        m_imageOpened;

    wxCheckBoxState m_checkedState;


    // Accessors for the fields that are not directly exposed.

    // Client data is owned by us so delete the old value when setting the new
    // one.
    wxClientData* GetClientData() const { return m_data; }
    void SetClientData(wxClientData* data) { delete m_data; m_data = data; }

    // Setting or getting the non-first column text. Getting is simple but you
    // need to call HasColumnsTexts() first as the column data is only
    // allocated on demand. And when setting the text we require to be given
    // the total number of columns as we allocate the entire array at once,
    // this is more efficient than using dynamically-expandable wxVector that
    // we know won't be needed as the number of columns is usually fixed. But
    // if it does change, our OnInsertColumn() must be called.
    //
    // Notice the presence of -1 everywhere in these methods: this is because
    // the text for the first column is always stored in m_text and so we don't
    // store it in m_columnsTexts.

    bool HasColumnsTexts() const { return m_columnsTexts != NULL; }
    const wxString& GetColumnText(unsigned col) const
    {
        return m_columnsTexts[col - 1];
    }

    void SetColumnText(const wxString& text, unsigned col, unsigned numColumns)
    {
        if ( !m_columnsTexts )
            m_columnsTexts = new wxString[numColumns - 1];

        m_columnsTexts[col - 1] = text;
    }

    void OnInsertColumn(unsigned col, unsigned numColumns)
    {
        wxASSERT_MSG( col, "Shouldn't be called for the first column" );

        // Nothing to do if we don't have any text.
        if ( !m_columnsTexts )
            return;

        wxScopedArray<wxString> oldTexts(m_columnsTexts);
        m_columnsTexts = new wxString[numColumns - 1];

        // In the loop below n is the index in the new column texts array and m
        // is the index in the old one.
        for ( unsigned n = 1, m = 1; n < numColumns - 1; n++, m++ )
        {
            if ( n == col )
            {
                // Leave the new array text initially empty and just adjust the
                // index (to compensate for "m++" done by the loop anyhow).
                m--;
            }
            else // Not the newly inserted column.
            {
                // Copy the old text value.
                m_columnsTexts[n - 1] = oldTexts[m - 1];
            }
        }
    }

    void OnDeleteColumn(unsigned col, unsigned numColumns)
    {
        wxASSERT_MSG( col, "Shouldn't be called for the first column" );

        if ( !m_columnsTexts )
            return;

        wxScopedArray<wxString> oldTexts(m_columnsTexts);
        m_columnsTexts = new wxString[numColumns - 2];

        // As above, n is the index in the new column texts array and m is the
        // index in the old one.
        for ( unsigned n = 1, m = 1; n < numColumns - 1; n++, m++ )
        {
            if ( m == col )
            {
                // Skip copying the deleted column and keep the new index the
                // same (so compensate for "n++" done in the loop).
                n--;
            }
            else // Not the deleted column.
            {
                m_columnsTexts[n - 1] = oldTexts[m - 1];
            }
        }
    }

    void OnClearColumns()
    {
        if ( m_columnsTexts )
        {
            delete [] m_columnsTexts;
            m_columnsTexts = NULL;
        }
    }


    // Functions for modifying the tree.

    // Insert the given item as the first child of this one. The parent pointer
    // must have been already set correctly at creation and we take ownership
    // of the pointer and will delete it later.
    void InsertChild(wxTreeListModelNode* child)
    {
        wxASSERT( child->m_parent == this );

        // Our previous first child becomes the next sibling of the new child.
        child->m_next = m_child;
        m_child = child;
    }

    // Insert the given item as our next sibling. As above, the item must have
    // the correct parent pointer and we take ownership of it.
    void InsertNext(wxTreeListModelNode* next)
    {
        wxASSERT( next->m_parent == m_parent );

        next->m_next = m_next;
        m_next = next;
    }

    // Remove the first child of this item from the tree and delete it.
    void DeleteChild()
    {
        wxTreeListModelNode* const oldChild = m_child;
        m_child = m_child->m_next;
        delete oldChild;
    }

    // Remove the next sibling of this item from the tree and deletes it.
    void DeleteNext()
    {
        wxTreeListModelNode* const oldNext = m_next;
        m_next = m_next->m_next;
        delete oldNext;
    }


    // Functions for tree traversal. All of them can return NULL.

    // Only returns NULL when called on the root item.
    wxTreeListModelNode* GetParent() const { return m_parent; }

    // Returns the first child of this item.
    wxTreeListModelNode* GetChild() const { return m_child; }

    // Returns the next sibling of this item.
    wxTreeListModelNode* GetNext() const { return m_next; }

    // Unlike the previous two functions, this one is not a simple accessor
    // (hence it's not called "GetSomething") but computes the next node after
    // this one in tree order.
    wxTreeListModelNode* NextInTree() const
    {
        if ( m_child )
            return m_child;

        if ( m_next )
            return m_next;

        // Recurse upwards until we find the next sibling.
        for ( wxTreeListModelNode* node = m_parent; node; node = node->m_parent )
        {
            if ( node->m_next )
                return node->m_next;
        }

        return NULL;
    }


private:
    // The (never changing after creation) parent of this node and the possibly
    // NULL pointers to its first child and next sibling.
    wxTreeListModelNode* const m_parent;
    wxTreeListModelNode* m_child;
    wxTreeListModelNode* m_next;

    // Client data pointer owned by the control. May be NULL.
    wxClientData* m_data;

    // Array of column values for all the columns except the first one. May be
    // NULL if no values had been set for them.
    wxString* m_columnsTexts;
};

// ----------------------------------------------------------------------------
// wxTreeListModel: wxDataViewModel implementation used by wxTreeListCtrl.
// ----------------------------------------------------------------------------

class wxTreeListModel : public wxDataViewModel
{
public:
    typedef wxTreeListModelNode Node;

    // Unlike a general wxDataViewModel, this model can only be used with a
    // single control at once. The main reason for this is that we need to
    // support different icons for opened and closed items and the item state
    // is associated with the control, not the model, so our GetValue() is also
    // bound to it (otherwise, what would it return for an item expanded in one
    // associated control and collapsed in another one?).
    wxTreeListModel(wxTreeListCtrl* treelist);
    virtual ~wxTreeListModel();


    // Helpers for converting between wxDataViewItem and wxTreeListItem. These
    // methods simply cast the pointer to/from wxDataViewItem except for the
    // root node that we handle specially unless explicitly disabled.
    //
    // The advantage of using them is that they're greppable and stand out
    // better, hopefully making the code more clear.
    Node* FromNonRootDVI(wxDataViewItem dvi) const
    {
        return static_cast<Node*>(dvi.GetID());
    }

    Node* FromDVI(wxDataViewItem dvi) const
    {
        if ( !dvi.IsOk() )
            return m_root;

        return FromNonRootDVI(dvi);
    }

    wxDataViewItem ToNonRootDVI(Node* node) const
    {
        return wxDataViewItem(node);
    }

    wxDataViewItem ToDVI(Node* node) const
    {
        // Our root item must be represented as NULL at wxDVC level to map to
        // its own invisible root.
        if ( !node->GetParent() )
            return wxDataViewItem();

        return ToNonRootDVI(node);
    }


    // Methods called by wxTreeListCtrl.
    void InsertColumn(unsigned col);
    void DeleteColumn(unsigned col);
    void ClearColumns();

    Node* InsertItem(Node* parent,
                     Node* previous,
                     const wxString& text,
                     int imageClosed,
                     int imageOpened,
                     wxClientData* data);
    void DeleteItem(Node* item);
    void DeleteAllItems();

    Node* GetRootItem() const { return m_root; }

    const wxString& GetItemText(Node* item, unsigned col) const;
    void SetItemText(Node* item, unsigned col, const wxString& text);
    void SetItemImage(Node* item, int closed, int opened);
    wxClientData* GetItemData(Node* item) const;
    void SetItemData(Node* item, wxClientData* data);

    void CheckItem(Node* item, wxCheckBoxState checkedState);
    void ToggleItem(wxDataViewItem item);


    // Implement the base class pure virtual methods.
    virtual unsigned GetColumnCount() const;
    virtual wxString GetColumnType(unsigned col) const;
    virtual void GetValue(wxVariant& variant,
                          const wxDataViewItem& item,
                          unsigned col) const;
    virtual bool SetValue(const wxVariant& variant,
                          const wxDataViewItem& item,
                          unsigned col);
    virtual wxDataViewItem GetParent(const wxDataViewItem& item) const;
    virtual bool IsContainer(const wxDataViewItem& item) const;
    virtual bool HasContainerColumns(const wxDataViewItem& item) const;
    virtual unsigned GetChildren(const wxDataViewItem& item,
                                 wxDataViewItemArray& children) const;
    virtual bool IsListModel() const { return m_isFlat; }
    virtual int Compare(const wxDataViewItem& item1,
                        const wxDataViewItem& item2,
                        unsigned col,
                        bool ascending) const;

private:
    // The control we're associated with.
    wxTreeListCtrl* const m_treelist;

    // The unique invisible root element.
    Node* const m_root;

    // Number of columns we maintain.
    unsigned m_numColumns;

    // Set to false as soon as we have more than one level, i.e. as soon as any
    // items with non-root item as parent are added (and currently never reset
    // after this).
    bool m_isFlat;
};

// ============================================================================
// wxDataViewCheckIconText[Renderer]: special renderer for our first column.
// ============================================================================

// Currently this class is private but it could be extracted and made part of
// public API later as could be used directly with wxDataViewCtrl as well.
namespace
{

const char* CHECK_ICON_TEXT_TYPE = "wxDataViewCheckIconText";

// The value used by wxDataViewCheckIconTextRenderer
class wxDataViewCheckIconText : public wxDataViewIconText
{
public:
    wxDataViewCheckIconText(const wxString& text = wxString(),
                            const wxIcon& icon = wxNullIcon,
                            wxCheckBoxState checkedState = wxCHK_UNDETERMINED)
        : wxDataViewIconText(text, icon),
          m_checkedState(checkedState)
    {
    }

    wxDataViewCheckIconText(const wxDataViewCheckIconText& other)
        : wxDataViewIconText(other),
          m_checkedState(other.m_checkedState)
    {
    }

    bool IsSameAs(const wxDataViewCheckIconText& other) const
    {
        return wxDataViewIconText::IsSameAs(other) &&
                m_checkedState == other.m_checkedState;
    }

    // There is no encapsulation anyhow, so just expose this field directly.
    wxCheckBoxState m_checkedState;


private:
    wxDECLARE_DYNAMIC_CLASS(wxDataViewCheckIconText);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxDataViewCheckIconText, wxDataViewIconText);

DECLARE_VARIANT_OBJECT(wxDataViewCheckIconText)
IMPLEMENT_VARIANT_OBJECT(wxDataViewCheckIconText)


class wxDataViewCheckIconTextRenderer : public wxDataViewCustomRenderer
{
public:
    wxDataViewCheckIconTextRenderer()
        : wxDataViewCustomRenderer(CHECK_ICON_TEXT_TYPE,
                                   wxDATAVIEW_CELL_ACTIVATABLE)
    {
    }

    virtual bool SetValue(const wxVariant& value)
    {
        m_value << value;
        return true;
    }

    virtual bool GetValue(wxVariant& WXUNUSED(value)) const
    {
        return false;
    }

    wxSize GetSize() const
    {
        wxSize size = GetCheckSize();
        size.x += MARGIN_CHECK_ICON;

        if ( m_value.GetIcon().IsOk() )
        {
            const wxSize sizeIcon = m_value.GetIcon().GetSize();
            if ( sizeIcon.y > size.y )
                size.y = sizeIcon.y;

            size.x += sizeIcon.x + MARGIN_ICON_TEXT;
        }

        wxString text = m_value.GetText();
        if ( text.empty() )
            text = "Dummy";

        const wxSize sizeText = GetTextExtent(text);
        if ( sizeText.y > size.y )
            size.y = sizeText.y;

        size.x += sizeText.x;

        return size;
    }

    virtual bool Render(wxRect cell, wxDC* dc, int state)
    {
        // Draw the checkbox first.
        int renderFlags = 0;
        switch ( m_value.m_checkedState )
        {
            case wxCHK_UNCHECKED:
                break;

            case wxCHK_CHECKED:
                renderFlags |= wxCONTROL_CHECKED;
                break;

            case wxCHK_UNDETERMINED:
                renderFlags |= wxCONTROL_UNDETERMINED;
                break;
        }

        if ( state & wxDATAVIEW_CELL_PRELIT )
            renderFlags |= wxCONTROL_CURRENT;

        const wxSize sizeCheck = GetCheckSize();

        wxRect rectCheck(cell.GetPosition(), sizeCheck);
        rectCheck = rectCheck.CentreIn(cell, wxVERTICAL);

        wxRendererNative::Get().DrawCheckBox
                                (
                                    GetView(), *dc, rectCheck, renderFlags
                                );

        // Then the icon, if any.
        int xoffset = sizeCheck.x + MARGIN_CHECK_ICON;

        const wxIcon& icon = m_value.GetIcon();
        if ( icon.IsOk() )
        {
            const wxSize sizeIcon = icon.GetSize();
            wxRect rectIcon(cell.GetPosition(), sizeIcon);
            rectIcon.x += xoffset;
            rectIcon = rectIcon.CentreIn(cell, wxVERTICAL);

            dc->DrawIcon(icon, rectIcon.GetPosition());

            xoffset += sizeIcon.x + MARGIN_ICON_TEXT;
        }

        // Finally the text.
        RenderText(m_value.GetText(), xoffset, cell, dc, state);

        return true;
    }

    // Event handlers toggling the items checkbox if it was clicked.
    virtual bool ActivateCell(const wxRect& WXUNUSED(cell),
                              wxDataViewModel *model,
                              const wxDataViewItem & item,
                              unsigned int WXUNUSED(col),
                              const wxMouseEvent *mouseEvent)
    {
        if ( mouseEvent )
        {
            if ( !wxRect(GetCheckSize()).Contains(mouseEvent->GetPosition()) )
                return false;
        }

        static_cast<wxTreeListModel*>(model)->ToggleItem(item);
        return true;
    }

protected:
    wxSize GetCheckSize() const
    {
        return wxRendererNative::Get().GetCheckBoxSize(GetView());
    }

private:
    // Just some arbitrary constants defining margins, in pixels.
    enum
    {
        MARGIN_CHECK_ICON = 3,
        MARGIN_ICON_TEXT = 4
    };

    wxDataViewCheckIconText m_value;
};

} // anonymous namespace

// ============================================================================
// wxTreeListModel implementation
// ============================================================================

wxTreeListModel::wxTreeListModel(wxTreeListCtrl* treelist)
    : m_treelist(treelist),
      m_root(new Node(NULL))
{
    m_numColumns = 0;
    m_isFlat = true;
}

wxTreeListModel::~wxTreeListModel()
{
    delete m_root;
}

void wxTreeListModel::InsertColumn(unsigned col)
{
    m_numColumns++;

    // There is no need to update anything when inserting the first column.
    if ( m_numColumns == 1 )
        return;

    // Update all the items as they may have texts for the old columns.
    for ( Node* node = m_root->GetChild(); node; node = node->NextInTree() )
    {
        node->OnInsertColumn(col, m_numColumns);
    }
}

void wxTreeListModel::DeleteColumn(unsigned col)
{
    wxCHECK_RET( col < m_numColumns, "Invalid column index" );

    // Update all the items to remove the text for the non first columns.
    if ( col > 0 )
    {
        for ( Node* node = m_root->GetChild(); node; node = node->NextInTree() )
        {
            node->OnDeleteColumn(col, m_numColumns);
        }
    }

    m_numColumns--;
}

void wxTreeListModel::ClearColumns()
{
    m_numColumns = 0;

    for ( Node* node = m_root->GetChild(); node; node = node->NextInTree() )
    {
        node->OnClearColumns();
    }
}

wxTreeListModelNode*
wxTreeListModel::InsertItem(Node* parent,
                            Node* previous,
                            const wxString& text,
                            int imageClosed,
                            int imageOpened,
                            wxClientData* data)
{
    wxCHECK_MSG( parent, NULL,
                 "Must have a valid parent (maybe GetRootItem()?)" );

    wxCHECK_MSG( previous, NULL,
                 "Must have a valid previous item (maybe wxTLI_FIRST/LAST?)" );

    if ( m_isFlat && parent != m_root )
    {
        // Not flat any more, this is a second level child.
        m_isFlat = false;
    }

    wxScopedPtr<Node>
        newItem(new Node(parent, text, imageClosed, imageOpened, data));

    // FIXME-VC6: This compiler refuses to compare "Node* previous" with
    //            wxTLI_XXX without some help.
    const wxTreeListItem previousItem(previous);

    // If we have no children at all, then inserting as last child is the same
    // as inserting as the first one so check for it here too.
    if ( previousItem == wxTLI_FIRST ||
            (previousItem == wxTLI_LAST && !parent->GetChild()) )
    {
        parent->InsertChild(newItem.get());
    }
    else // Not the first item, find the previous one.
    {
        if ( previousItem == wxTLI_LAST )
        {
            previous = parent->GetChild();

            // Find the last child.
            for ( ;; )
            {
                Node* const next = previous->GetNext();
                if ( !next )
                    break;

                previous = next;
            }
        }
        else // We already have the previous item.
        {
            // Just check it's under the correct parent.
            wxCHECK_MSG( previous->GetParent() == parent, NULL,
                         "Previous item is not under the right parent" );
        }

        previous->InsertNext(newItem.get());
    }

    ItemAdded(ToDVI(parent), ToDVI(newItem.get()));

    // The item was successfully inserted in the tree and so will be deleted by
    // it, we can detach it now.
    return newItem.release();
}

void wxTreeListModel::DeleteItem(Node* item)
{
    wxCHECK_RET( item, "Invalid item" );

    wxCHECK_RET( item != m_root, "Can't delete the root item" );

    Node* const parent = item->GetParent();

    Node* previous = parent->GetChild();
    if ( previous == item )
    {
        parent->DeleteChild();
    }
    else // Not the first child of its parent.
    {
        // Find the sibling just before it.
        for ( ;; )
        {
            Node* const next = previous->GetNext();
            if ( next == item )
                break;

            wxCHECK_RET( next, "Item not a child of its parent?" );

            previous = next;
        }

        previous->DeleteNext();
    }

    // Note that the item is already deleted by now, so we can't use it in any
    // way, e.g. by calling ToDVI(item) which does dereference the pointer, but
    // ToNonRootDVI() that we use here does not.
    ItemDeleted(ToDVI(parent), ToNonRootDVI(item));
}

void wxTreeListModel::DeleteAllItems()
{
    while ( m_root->GetChild() )
    {
        m_root->DeleteChild();
    }

    Cleared();
}

const wxString& wxTreeListModel::GetItemText(Node* item, unsigned col) const
{
    // Returning root item text here is bogus, it just happens to be an always
    // empty string we can return reference to.
    wxCHECK_MSG( item, m_root->m_text, "Invalid item" );

    // Notice that asking for the text of a column of an item that doesn't have
    // any column texts is not an error so we simply return an empty string in
    // this case.
    return col == 0 ? item->m_text
                    : item->HasColumnsTexts() ? item->GetColumnText(col)
                                              : m_root->m_text;
}

void wxTreeListModel::SetItemText(Node* item, unsigned col, const wxString& text)
{
    wxCHECK_RET( item, "Invalid item" );

    if ( col == 0 )
        item->m_text = text;
    else
        item->SetColumnText(text, col, m_numColumns);

    ValueChanged(ToDVI(item), col);
}

void wxTreeListModel::SetItemImage(Node* item, int closed, int opened)
{
    wxCHECK_RET( item, "Invalid item" );

    item->m_imageClosed = closed;
    item->m_imageOpened = opened;

    ValueChanged(ToDVI(item), 0);
}

wxClientData* wxTreeListModel::GetItemData(Node* item) const
{
    wxCHECK_MSG( item, NULL, "Invalid item" );

    return item->GetClientData();
}

void wxTreeListModel::SetItemData(Node* item, wxClientData* data)
{
    wxCHECK_RET( item, "Invalid item" );

    item->SetClientData(data);
}

void wxTreeListModel::CheckItem(Node* item, wxCheckBoxState checkedState)
{
    wxCHECK_RET( item, "Invalid item" );

    item->m_checkedState = checkedState;

    ItemChanged(ToDVI(item));
}

void wxTreeListModel::ToggleItem(wxDataViewItem dvi)
{
    Node* const item = FromDVI(dvi);

    wxCHECK_RET( item, "Invalid item" );

    const wxCheckBoxState stateOld = item->m_checkedState;

    // If the 3rd state is user-settable then the cycle is
    // unchecked->checked->undetermined.
    switch ( stateOld )
    {
        case wxCHK_CHECKED:
            item->m_checkedState = m_treelist->HasFlag(wxTL_USER_3STATE)
                                        ? wxCHK_UNDETERMINED
                                        : wxCHK_UNCHECKED;
            break;

        case wxCHK_UNDETERMINED:
            // Whether 3rd state is user-settable or not, the next state is
            // unchecked.
            item->m_checkedState = wxCHK_UNCHECKED;
            break;

        case wxCHK_UNCHECKED:
            item->m_checkedState = wxCHK_CHECKED;
            break;
    }

    ItemChanged(ToDVI(item));

    m_treelist->OnItemToggled(item, stateOld);
}

unsigned wxTreeListModel::GetColumnCount() const
{
    return m_numColumns;
}

wxString wxTreeListModel::GetColumnType(unsigned col) const
{
    if ( col == 0 )
    {
        return m_treelist->HasFlag(wxTL_CHECKBOX)
                    ? wxS("wxDataViewCheckIconText")
                    : wxS("wxDataViewIconText");
    }
    else // All the other columns contain just text.
    {
        return wxS("string");
    }
}

void
wxTreeListModel::GetValue(wxVariant& variant,
                          const wxDataViewItem& item,
                          unsigned col) const
{
    Node* const node = FromDVI(item);

    if ( col == 0 )
    {
        // Determine the correct image to use depending on the item state.
        int image = wxWithImages::NO_IMAGE;
        if ( m_treelist->IsExpanded(node) )
            image = node->m_imageOpened;

        if ( image == wxWithImages::NO_IMAGE )
            image = node->m_imageClosed;

        wxIcon icon = m_treelist->GetImage(image);

        if ( m_treelist->HasFlag(wxTL_CHECKBOX) )
            variant << wxDataViewCheckIconText(node->m_text, icon,
                                               node->m_checkedState);
        else
            variant << wxDataViewIconText(node->m_text, icon);
    }
    else
    {
        // Notice that we must still assign wxString to wxVariant to ensure
        // that it at least has the correct type.
        wxString text;
        if ( node->HasColumnsTexts() )
            text = node->GetColumnText(col);

        variant = text;
    }
}

bool
wxTreeListModel::SetValue(const wxVariant& WXUNUSED(variant),
                          const wxDataViewItem& WXUNUSED(item),
                          unsigned WXUNUSED(col))
{
    // We are not editable currently.
    return false;
}

wxDataViewItem wxTreeListModel::GetParent(const wxDataViewItem& item) const
{
    Node* const node = FromDVI(item);

    return ToDVI(node->GetParent());
}

bool wxTreeListModel::IsContainer(const wxDataViewItem& item) const
{
    // FIXME: In the generic (and native OS X) versions we implement this
    //        method normally, i.e. only items with children are containers.
    //        But for the native GTK version we must pretend that all items are
    //        containers because otherwise adding children to them later would
    //        fail because wxGTK code calls IsContainer() too early (when
    //        adding the item itself) and we can't know whether we're container
    //        or not by then. Luckily, always returning true doesn't have any
    //        serious drawbacks for us.
#ifdef __WXGTK__
    wxUnusedVar(item);

    return true;
#else
    Node* const node = FromDVI(item);

    return node->GetChild() != NULL;
#endif
}

bool
wxTreeListModel::HasContainerColumns(const wxDataViewItem& WXUNUSED(item)) const
{
    return true;
}

unsigned
wxTreeListModel::GetChildren(const wxDataViewItem& item,
                             wxDataViewItemArray& children) const
{
    Node* const node = FromDVI(item);

    unsigned numChildren = 0;
    for ( Node* child = node->GetChild(); child; child = child->GetNext() )
    {
        children.push_back(ToDVI(child));
        numChildren++;
    }

    return numChildren;
}

int
wxTreeListModel::Compare(const wxDataViewItem& item1,
                         const wxDataViewItem& item2,
                         unsigned col,
                         bool ascending) const
{
    // Compare using default alphabetical order if no custom comparator.
    wxTreeListItemComparator* const comp = m_treelist->m_comparator;
    if ( !comp )
        return wxDataViewModel::Compare(item1, item2, col, ascending);

    // Forward comparison to the comparator:
    int result = comp->Compare(m_treelist, col, FromDVI(item1), FromDVI(item2));

    // And adjust by the sort order if necessary.
    if ( !ascending )
        result = -result;

    return result;
}

// ============================================================================
// wxTreeListCtrl implementation
// ============================================================================

BEGIN_EVENT_TABLE(wxTreeListCtrl, wxWindow)
    EVT_DATAVIEW_SELECTION_CHANGED(wxID_ANY, wxTreeListCtrl::OnSelectionChanged)
    EVT_DATAVIEW_ITEM_EXPANDING(wxID_ANY, wxTreeListCtrl::OnItemExpanding)
    EVT_DATAVIEW_ITEM_EXPANDED(wxID_ANY, wxTreeListCtrl::OnItemExpanded)
    EVT_DATAVIEW_ITEM_ACTIVATED(wxID_ANY, wxTreeListCtrl::OnItemActivated)
    EVT_DATAVIEW_ITEM_CONTEXT_MENU(wxID_ANY, wxTreeListCtrl::OnItemContextMenu)
    EVT_DATAVIEW_COLUMN_SORTED(wxID_ANY, wxTreeListCtrl::OnColumnSorted)

    EVT_SIZE(wxTreeListCtrl::OnSize)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// Creation
// ----------------------------------------------------------------------------

void wxTreeListCtrl::Init()
{
    m_view = NULL;
    m_model = NULL;
    m_comparator = NULL;
}

bool wxTreeListCtrl::Create(wxWindow* parent,
                            wxWindowID id,
                            const wxPoint& pos,
                            const wxSize& size,
                            long style,
                            const wxString& name)
{
    if ( style & wxTL_USER_3STATE )
        style |= wxTL_3STATE;

    if ( style & wxTL_3STATE )
        style |= wxTL_CHECKBOX;

    // Create the window itself and wxDataViewCtrl used by it.
    if ( !wxWindow::Create(parent, id,
                           pos, size,
                           style, name) )
    {
        return false;
    }

    m_view = new wxDataViewCtrl;
    long styleDataView = HasFlag(wxTL_MULTIPLE) ? wxDV_MULTIPLE
                                                : wxDV_SINGLE;
    if ( HasFlag(wxTL_NO_HEADER) )
        styleDataView |= wxDV_NO_HEADER;

    if ( !m_view->Create(this, wxID_ANY,
                         wxPoint(0, 0), GetClientSize(),
                         styleDataView) )
    {
        delete m_view;
        m_view = NULL;

        return false;
    }


    // Set up the model for wxDataViewCtrl.
    m_model = new wxTreeListModel(this);
    m_view->AssociateModel(m_model);

    return true;
}

wxTreeListCtrl::~wxTreeListCtrl()
{
    if ( m_model )
        m_model->DecRef();
}

wxWindowList wxTreeListCtrl::GetCompositeWindowParts() const
{
    wxWindowList parts;
    parts.push_back(m_view);
    return parts;
}

// ----------------------------------------------------------------------------
// Columns
// ----------------------------------------------------------------------------

int
wxTreeListCtrl::DoInsertColumn(const wxString& title,
                               int pos,
                               int width,
                               wxAlignment align,
                               int flags)
{
    wxCHECK_MSG( m_view, wxNOT_FOUND, "Must Create() first" );

    const unsigned oldNumColumns = m_view->GetColumnCount();

    if ( pos == wxNOT_FOUND )
        pos = oldNumColumns;

    wxDataViewRenderer* renderer;
    if ( pos == 0 )
    {
        // Inserting the first column which is special as it uses a different
        // renderer.

        // Also, currently it can be done only once.
        wxCHECK_MSG( !oldNumColumns, wxNOT_FOUND,
                     "Inserting column at position 0 currently not supported" );

        if ( HasFlag(wxTL_CHECKBOX) )
        {
            // Use our custom renderer to show the checkbox.
            renderer = new wxDataViewCheckIconTextRenderer;
        }
        else // We still need a special renderer to show the icons.
        {
            renderer = new wxDataViewIconTextRenderer;
        }
    }
    else // Not the first column.
    {
        // All the other ones use a simple text renderer.
        renderer = new wxDataViewTextRenderer;
    }

    wxDataViewColumn*
        column = new wxDataViewColumn(title, renderer, pos, width, align, flags);

    m_model->InsertColumn(pos);

    m_view->InsertColumn(pos, column);

    return pos;
}

unsigned wxTreeListCtrl::GetColumnCount() const
{
    return m_view ? m_view->GetColumnCount() : 0u;
}

bool wxTreeListCtrl::DeleteColumn(unsigned col)
{
    wxCHECK_MSG( col < GetColumnCount(), false, "Invalid column index" );

    if ( !m_view->DeleteColumn(m_view->GetColumn(col)) )
        return false;

    m_model->DeleteColumn(col);

    return true;
}

void wxTreeListCtrl::ClearColumns()
{
    // Don't assert here, clearing columns of the control before it's created
    // can be considered valid (just useless).
    if ( !m_model )
        return;

    m_view->ClearColumns();

    m_model->ClearColumns();
}

void wxTreeListCtrl::SetColumnWidth(unsigned col, int width)
{
    wxCHECK_RET( col < GetColumnCount(), "Invalid column index" );

    wxDataViewColumn* const column = m_view->GetColumn(col);
    wxCHECK_RET( column, "No such column?" );

    column->SetWidth(width);
}

int wxTreeListCtrl::GetColumnWidth(unsigned col) const
{
    wxCHECK_MSG( col < GetColumnCount(), -1, "Invalid column index" );

    wxDataViewColumn* column = m_view->GetColumn(col);
    wxCHECK_MSG( column, -1, "No such column?" );

    return column->GetWidth();
}

int wxTreeListCtrl::WidthFor(const wxString& text) const
{
    return GetTextExtent(text).x;
}

// ----------------------------------------------------------------------------
// Items
// ----------------------------------------------------------------------------

wxTreeListItem
wxTreeListCtrl::DoInsertItem(wxTreeListItem parent,
                             wxTreeListItem previous,
                             const wxString& text,
                             int imageClosed,
                             int imageOpened,
                             wxClientData* data)
{
    wxCHECK_MSG( m_model, wxTreeListItem(), "Must create first" );

    return wxTreeListItem(m_model->InsertItem(parent, previous, text,
                                              imageClosed, imageOpened, data));
}

void wxTreeListCtrl::DeleteItem(wxTreeListItem item)
{
    wxCHECK_RET( m_model, "Must create first" );

    m_model->DeleteItem(item);
}

void wxTreeListCtrl::DeleteAllItems()
{
    if ( m_model )
        m_model->DeleteAllItems();
}

// ----------------------------------------------------------------------------
// Tree navigation
// ----------------------------------------------------------------------------

// The simple accessors in this section are implemented directly using
// wxTreeListModelNode methods, without passing by the model. This is just a
// shortcut and avoids us the trouble of defining more trivial methods in
// wxTreeListModel.

wxTreeListItem wxTreeListCtrl::GetRootItem() const
{
    wxCHECK_MSG( m_model, wxTreeListItem(), "Must create first" );

    return m_model->GetRootItem();
}

wxTreeListItem wxTreeListCtrl::GetItemParent(wxTreeListItem item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeListItem(), "Invalid item" );

    return item->GetParent();
}

wxTreeListItem wxTreeListCtrl::GetFirstChild(wxTreeListItem item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeListItem(), "Invalid item" );

    return item->GetChild();
}

wxTreeListItem
wxTreeListCtrl::GetNextSibling(wxTreeListItem item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeListItem(), "Invalid item" );

    return item->GetNext();
}

wxTreeListItem wxTreeListCtrl::GetNextItem(wxTreeListItem item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeListItem(), "Invalid item" );

    return item->NextInTree();
}

// ----------------------------------------------------------------------------
// Item attributes
// ----------------------------------------------------------------------------

const wxString&
wxTreeListCtrl::GetItemText(wxTreeListItem item, unsigned col) const
{
    // We can't use wxCHECK_MSG() here because we don't have any empty string
    // reference to return so we use a static variable that exists just for the
    // purpose of this check -- and so we put it in its own scope so that it's
    // never even created during normal program execution.
    if ( !m_model || col >= m_model->GetColumnCount() )
    {
        static wxString s_empty;

        if ( !m_model )
        {
            wxFAIL_MSG( "Must create first" );
        }
        else if ( col >= m_model->GetColumnCount() )
        {
            wxFAIL_MSG( "Invalid column index" );
        }

        return s_empty;
    }

    return m_model->GetItemText(item, col);
}

void
wxTreeListCtrl::SetItemText(wxTreeListItem item,
                            unsigned col,
                            const wxString& text)
{
    wxCHECK_RET( m_model, "Must create first" );
    wxCHECK_RET( col < m_model->GetColumnCount(), "Invalid column index" );

    m_model->SetItemText(item, col, text);
}

void wxTreeListCtrl::SetItemImage(wxTreeListItem item, int closed, int opened)
{
    wxCHECK_RET( m_model, "Must create first" );

    if ( closed != NO_IMAGE || opened != NO_IMAGE )
    {
        wxImageList* const imageList = GetImageList();
        wxCHECK_RET( imageList, "Can't set images without image list" );

        const int imageCount = imageList->GetImageCount();

        wxCHECK_RET( closed < imageCount, "Invalid image index" );
        wxCHECK_RET( opened < imageCount, "Invalid opened image index" );
    }

    m_model->SetItemImage(item, closed, opened);
}

wxClientData* wxTreeListCtrl::GetItemData(wxTreeListItem item) const
{
    wxCHECK_MSG( m_model, NULL, "Must create first" );

    return m_model->GetItemData(item);
}

void wxTreeListCtrl::SetItemData(wxTreeListItem item, wxClientData* data)
{
    wxCHECK_RET( m_model, "Must create first" );

    m_model->SetItemData(item, data);
}

// ----------------------------------------------------------------------------
// Expanding and collapsing
// ----------------------------------------------------------------------------

void wxTreeListCtrl::Expand(wxTreeListItem item)
{
    wxCHECK_RET( m_view, "Must create first" );

    m_view->Expand(m_model->ToDVI(item));
}

void wxTreeListCtrl::Collapse(wxTreeListItem item)
{
    wxCHECK_RET( m_view, "Must create first" );

    m_view->Collapse(m_model->ToDVI(item));
}

bool wxTreeListCtrl::IsExpanded(wxTreeListItem item) const
{
    wxCHECK_MSG( m_view, false, "Must create first" );

    return m_view->IsExpanded(m_model->ToDVI(item));
}

// ----------------------------------------------------------------------------
// Selection
// ----------------------------------------------------------------------------

wxTreeListItem wxTreeListCtrl::GetSelection() const
{
    wxCHECK_MSG( m_view, wxTreeListItem(), "Must create first" );

    wxCHECK_MSG( !HasFlag(wxTL_MULTIPLE), wxTreeListItem(),
                 "Must use GetSelections() with multi-selection controls!" );

    const wxDataViewItem dvi = m_view->GetSelection();

    return m_model->FromNonRootDVI(dvi);
}

unsigned wxTreeListCtrl::GetSelections(wxTreeListItems& selections) const
{
    wxCHECK_MSG( m_view, 0, "Must create first" );

    wxDataViewItemArray selectionsDV;
    const unsigned numSelected = m_view->GetSelections(selectionsDV);
    selections.resize(numSelected);
    for ( unsigned n = 0; n < numSelected; n++ )
        selections[n] = m_model->FromNonRootDVI(selectionsDV[n]);

    return numSelected;
}

void wxTreeListCtrl::Select(wxTreeListItem item)
{
    wxCHECK_RET( m_view, "Must create first" );

    m_view->Select(m_model->ToNonRootDVI(item));
}

void wxTreeListCtrl::Unselect(wxTreeListItem item)
{
    wxCHECK_RET( m_view, "Must create first" );

    m_view->Unselect(m_model->ToNonRootDVI(item));
}

bool wxTreeListCtrl::IsSelected(wxTreeListItem item) const
{
    wxCHECK_MSG( m_view, false, "Must create first" );

    return m_view->IsSelected(m_model->ToNonRootDVI(item));
}

void wxTreeListCtrl::SelectAll()
{
    wxCHECK_RET( m_view, "Must create first" );

    m_view->SelectAll();
}

void wxTreeListCtrl::UnselectAll()
{
    wxCHECK_RET( m_view, "Must create first" );

    m_view->UnselectAll();
}

// ----------------------------------------------------------------------------
// Checkbox handling
// ----------------------------------------------------------------------------

void wxTreeListCtrl::CheckItem(wxTreeListItem item, wxCheckBoxState state)
{
    wxCHECK_RET( m_model, "Must create first" );

    m_model->CheckItem(item, state);
}

void
wxTreeListCtrl::CheckItemRecursively(wxTreeListItem item, wxCheckBoxState state)
{
    wxCHECK_RET( m_model, "Must create first" );

    m_model->CheckItem(item, state);

    for ( wxTreeListItem child = GetFirstChild(item);
          child.IsOk();
          child = GetNextSibling(child) )
    {
        CheckItemRecursively(child, state);
    }
}

void wxTreeListCtrl::UpdateItemParentStateRecursively(wxTreeListItem item)
{
    wxCHECK_RET( item.IsOk(), "Invalid item" );

    wxASSERT_MSG( HasFlag(wxTL_3STATE), "Can only be used with wxTL_3STATE" );

    for ( ;; )
    {
        wxTreeListItem parent = GetItemParent(item);
        if ( parent == GetRootItem() )
        {
            // There is no checked state associated with the root item.
            return;
        }

        // Set parent state to the state of this item if all the other children
        // have the same state too. Otherwise make it indeterminate.
        const wxCheckBoxState stateItem = GetCheckedState(item);
        CheckItem(parent, AreAllChildrenInState(parent, stateItem)
                            ? stateItem
                            : wxCHK_UNDETERMINED);

        // And do the same thing with the parent's parent too.
        item = parent;
    }
}

wxCheckBoxState wxTreeListCtrl::GetCheckedState(wxTreeListItem item) const
{
    wxCHECK_MSG( item.IsOk(), wxCHK_UNDETERMINED, "Invalid item" );

    return item->m_checkedState;
}

bool
wxTreeListCtrl::AreAllChildrenInState(wxTreeListItem item,
                                      wxCheckBoxState state) const
{
    wxCHECK_MSG( item.IsOk(), false, "Invalid item" );

    for ( wxTreeListItem child = GetFirstChild(item);
          child.IsOk();
          child = GetNextSibling(child) )
    {
        if ( GetCheckedState(child) != state )
            return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
// Sorting
// ----------------------------------------------------------------------------

void wxTreeListCtrl::SetSortColumn(unsigned col, bool ascendingOrder)
{
    wxCHECK_RET( col < m_view->GetColumnCount(), "Invalid column index" );

    m_view->GetColumn(col)->SetSortOrder(ascendingOrder);
}

bool wxTreeListCtrl::GetSortColumn(unsigned* col, bool* ascendingOrder)
{
    const unsigned numColumns = m_view->GetColumnCount();
    for ( unsigned n = 0; n < numColumns; n++ )
    {
        wxDataViewColumn* const column = m_view->GetColumn(n);
        if ( column->IsSortKey() )
        {
            if ( col )
                *col = n;

            if ( ascendingOrder )
                *ascendingOrder = column->IsSortOrderAscending();

            return true;
        }
    }

    return false;
}

void wxTreeListCtrl::SetItemComparator(wxTreeListItemComparator* comparator)
{
    m_comparator = comparator;
}

// ----------------------------------------------------------------------------
// Events
// ----------------------------------------------------------------------------

void wxTreeListCtrl::SendItemEvent(wxEventType evt, wxDataViewEvent& eventDV)
{
    wxTreeListEvent eventTL(evt, this, m_model->FromDVI(eventDV.GetItem()));

    if ( !ProcessWindowEvent(eventTL) )
    {
        eventDV.Skip();
        return;
    }

    if ( !eventTL.IsAllowed() )
    {
        eventDV.Veto();
    }
}

void wxTreeListCtrl::SendColumnEvent(wxEventType evt, wxDataViewEvent& eventDV)
{
    wxTreeListEvent eventTL(evt, this, wxTreeListItem());
    eventTL.SetColumn(eventDV.GetColumn());

    if ( !ProcessWindowEvent(eventTL) )
    {
        eventDV.Skip();
        return;
    }

    if ( !eventTL.IsAllowed() )
    {
        eventDV.Veto();
    }
}

void
wxTreeListCtrl::OnItemToggled(wxTreeListItem item, wxCheckBoxState stateOld)
{
    wxTreeListEvent event(wxEVT_TREELIST_ITEM_CHECKED, this, item);
    event.SetOldCheckedState(stateOld);

    ProcessWindowEvent(event);
}

void wxTreeListCtrl::OnSelectionChanged(wxDataViewEvent& event)
{
    SendItemEvent(wxEVT_TREELIST_SELECTION_CHANGED, event);
}

void wxTreeListCtrl::OnItemExpanding(wxDataViewEvent& event)
{
    SendItemEvent(wxEVT_TREELIST_ITEM_EXPANDING, event);
}

void wxTreeListCtrl::OnItemExpanded(wxDataViewEvent& event)
{
    SendItemEvent(wxEVT_TREELIST_ITEM_EXPANDED, event);
}

void wxTreeListCtrl::OnItemActivated(wxDataViewEvent& event)
{
    SendItemEvent(wxEVT_TREELIST_ITEM_ACTIVATED, event);
}

void wxTreeListCtrl::OnItemContextMenu(wxDataViewEvent& event)
{
    SendItemEvent(wxEVT_TREELIST_ITEM_CONTEXT_MENU, event);
}

void wxTreeListCtrl::OnColumnSorted(wxDataViewEvent& event)
{
    SendColumnEvent(wxEVT_TREELIST_COLUMN_SORTED, event);
}

// ----------------------------------------------------------------------------
// Geometry
// ----------------------------------------------------------------------------

void wxTreeListCtrl::OnSize(wxSizeEvent& event)
{
    event.Skip();

    if ( m_view )
    {
        // Resize the real control to cover our entire client area.
        const wxRect rect = GetClientRect();
        m_view->SetSize(rect);

#ifdef wxHAS_GENERIC_DATAVIEWCTRL
        // The generic implementation doesn't refresh itself immediately which
        // is annoying during "live resizing", so do it forcefully here to
        // ensure that the items are re-laid out and the focus rectangle is
        // redrawn correctly (instead of leaving traces) while our size is
        // being changed.
        wxWindow* const view = GetView();
        view->Refresh();
        view->Update();
#endif // wxHAS_GENERIC_DATAVIEWCTRL

        // Resize the first column to take the remaining available space.
        const unsigned numColumns = GetColumnCount();
        if ( !numColumns )
            return;

        // There is a bug in generic wxDataViewCtrl: if the column width sums
        // up to the total size, horizontal scrollbar (unnecessarily) appears,
        // so subtract a bit to ensure this doesn't happen.
        int remainingWidth = rect.width - 5;
        for ( unsigned n = 1; n < GetColumnCount(); n++ )
        {
            remainingWidth -= GetColumnWidth(n);
            if ( remainingWidth <= 0 )
            {
                // There is not enough space, as we're not going to give the
                // first column negative width anyhow, just don't do anything.
                return;
            }
        }

        SetColumnWidth(0, remainingWidth);
    }
}

wxWindow* wxTreeListCtrl::GetView() const
{
#ifdef wxHAS_GENERIC_DATAVIEWCTRL
    return m_view->GetMainWindow();
#else
    return m_view;
#endif
}

// ============================================================================
// wxTreeListEvent implementation
// ============================================================================

wxIMPLEMENT_DYNAMIC_CLASS(wxTreeListEvent, wxNotifyEvent)

#define wxDEFINE_TREELIST_EVENT(name) \
    wxDEFINE_EVENT(wxEVT_TREELIST_##name, wxTreeListEvent)

wxDEFINE_TREELIST_EVENT(SELECTION_CHANGED);
wxDEFINE_TREELIST_EVENT(ITEM_EXPANDING);
wxDEFINE_TREELIST_EVENT(ITEM_EXPANDED);
wxDEFINE_TREELIST_EVENT(ITEM_CHECKED);
wxDEFINE_TREELIST_EVENT(ITEM_ACTIVATED);
wxDEFINE_TREELIST_EVENT(ITEM_CONTEXT_MENU);
wxDEFINE_TREELIST_EVENT(COLUMN_SORTED);

#undef wxDEFINE_TREELIST_EVENT

#endif // wxUSE_TREELISTCTRL
