/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dataview.h
// Purpose:     wxDataViewCtrl base classes
// Author:      Robert Roebling
// Modified by: Bo Yang
// Created:     08.01.06
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DATAVIEW_H_BASE_
#define _WX_DATAVIEW_H_BASE_

#include "wx/defs.h"

#if wxUSE_DATAVIEWCTRL

#include "wx/textctrl.h"
#include "wx/headercol.h"
#include "wx/variant.h"
#include "wx/dnd.h"             // For wxDragResult declaration only.
#include "wx/dynarray.h"
#include "wx/icon.h"
#include "wx/itemid.h"
#include "wx/weakref.h"
#include "wx/vector.h"
#include "wx/dataobj.h"
#include "wx/withimages.h"

class WXDLLIMPEXP_FWD_CORE wxImageList;

#if !(defined(__WXGTK20__) || defined(__WXOSX__)) || defined(__WXUNIVERSAL__)
// #if !(defined(__WXOSX__)) || defined(__WXUNIVERSAL__)
    #define wxHAS_GENERIC_DATAVIEWCTRL
#endif

#ifdef wxHAS_GENERIC_DATAVIEWCTRL
    // this symbol doesn't follow the convention for wxUSE_XXX symbols which
    // are normally always defined as either 0 or 1, so its use is deprecated
    // and it only exists for backwards compatibility, don't use it any more
    // and use wxHAS_GENERIC_DATAVIEWCTRL instead
    #define wxUSE_GENERICDATAVIEWCTRL
#endif

// ----------------------------------------------------------------------------
// wxDataViewCtrl globals
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_ADV wxDataViewModel;
class WXDLLIMPEXP_FWD_ADV wxDataViewCtrl;
class WXDLLIMPEXP_FWD_ADV wxDataViewColumn;
class WXDLLIMPEXP_FWD_ADV wxDataViewRenderer;
class WXDLLIMPEXP_FWD_ADV wxDataViewModelNotifier;

extern WXDLLIMPEXP_DATA_ADV(const char) wxDataViewCtrlNameStr[];

// ----------------------------------------------------------------------------
// wxDataViewCtrl flags
// ----------------------------------------------------------------------------

// size of a wxDataViewRenderer without contents:
#define wxDVC_DEFAULT_RENDERER_SIZE     20

// the default width of new (text) columns:
#define wxDVC_DEFAULT_WIDTH             80

// the default width of new toggle columns:
#define wxDVC_TOGGLE_DEFAULT_WIDTH      30

// the default minimal width of the columns:
#define wxDVC_DEFAULT_MINWIDTH          30

// The default alignment of wxDataViewRenderers is to take
// the alignment from the column it owns.
#define wxDVR_DEFAULT_ALIGNMENT         -1


// ---------------------------------------------------------
// wxDataViewItem
// ---------------------------------------------------------

// Make it a class and not a typedef to allow forward declaring it.
class wxDataViewItem : public wxItemId<void*>
{
public:
    wxDataViewItem() : wxItemId<void*>() { }
    wxEXPLICIT wxDataViewItem(void* pItem) : wxItemId<void*>(pItem) { }
};

WX_DEFINE_ARRAY(wxDataViewItem, wxDataViewItemArray);

// ---------------------------------------------------------
// wxDataViewModelNotifier
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewModelNotifier
{
public:
    wxDataViewModelNotifier() { m_owner = NULL; }
    virtual ~wxDataViewModelNotifier() { m_owner = NULL; }

    virtual bool ItemAdded( const wxDataViewItem &parent, const wxDataViewItem &item ) = 0;
    virtual bool ItemDeleted( const wxDataViewItem &parent, const wxDataViewItem &item ) = 0;
    virtual bool ItemChanged( const wxDataViewItem &item ) = 0;
    virtual bool ItemsAdded( const wxDataViewItem &parent, const wxDataViewItemArray &items );
    virtual bool ItemsDeleted( const wxDataViewItem &parent, const wxDataViewItemArray &items );
    virtual bool ItemsChanged( const wxDataViewItemArray &items );
    virtual bool ValueChanged( const wxDataViewItem &item, unsigned int col ) = 0;
    virtual bool Cleared() = 0;

    // some platforms, such as GTK+, may need a two step procedure for ::Reset()
    virtual bool BeforeReset() { return true; }
    virtual bool AfterReset() { return Cleared(); }

    virtual void Resort() = 0;

    void SetOwner( wxDataViewModel *owner ) { m_owner = owner; }
    wxDataViewModel *GetOwner() const       { return m_owner; }

private:
    wxDataViewModel *m_owner;
};



// ----------------------------------------------------------------------------
// wxDataViewItemAttr: a structure containing the visual attributes of an item
// ----------------------------------------------------------------------------

// TODO: this should be renamed to wxItemAttr or something general like this

class WXDLLIMPEXP_ADV wxDataViewItemAttr
{
public:
    // ctors
    wxDataViewItemAttr()
    {
        m_bold = false;
        m_italic = false;
    }

    // setters
    void SetColour(const wxColour& colour) { m_colour = colour; }
    void SetBold( bool set ) { m_bold = set; }
    void SetItalic( bool set ) { m_italic = set; }
    void SetBackgroundColour(const wxColour& colour)  { m_bgColour = colour; }

    // accessors
    bool HasColour() const { return m_colour.IsOk(); }
    const wxColour& GetColour() const { return m_colour; }

    bool HasFont() const { return m_bold || m_italic; }
    bool GetBold() const { return m_bold; }
    bool GetItalic() const { return m_italic; }

    bool HasBackgroundColour() const { return m_bgColour.IsOk(); }
    const wxColour& GetBackgroundColour() const { return m_bgColour; }

    bool IsDefault() const { return !(HasColour() || HasFont() || HasBackgroundColour()); }

    // Return the font based on the given one with this attribute applied to it.
    wxFont GetEffectiveFont(const wxFont& font) const;

private:
    wxColour m_colour;
    bool     m_bold;
    bool     m_italic;
    wxColour m_bgColour;
};


// ---------------------------------------------------------
// wxDataViewModel
// ---------------------------------------------------------

WX_DECLARE_LIST_WITH_DECL(wxDataViewModelNotifier, wxDataViewModelNotifiers,
                          class WXDLLIMPEXP_ADV);

class WXDLLIMPEXP_ADV wxDataViewModel: public wxRefCounter
{
public:
    wxDataViewModel();

    virtual unsigned int GetColumnCount() const = 0;

    // return type as reported by wxVariant
    virtual wxString GetColumnType( unsigned int col ) const = 0;

    // get value into a wxVariant
    virtual void GetValue( wxVariant &variant,
                           const wxDataViewItem &item, unsigned int col ) const = 0;

    // return true if the given item has a value to display in the given
    // column: this is always true except for container items which by default
    // only show their label in the first column (but see HasContainerColumns())
    bool HasValue(const wxDataViewItem& item, unsigned col) const
    {
        return col == 0 || !IsContainer(item) || HasContainerColumns(item);
    }

    // usually ValueChanged() should be called after changing the value in the
    // model to update the control, ChangeValue() does it on its own while
    // SetValue() does not -- so while you will override SetValue(), you should
    // be usually calling ChangeValue()
    virtual bool SetValue(const wxVariant &variant,
                          const wxDataViewItem &item,
                          unsigned int col) = 0;

    bool ChangeValue(const wxVariant& variant,
                     const wxDataViewItem& item,
                     unsigned int col)
    {
        return SetValue(variant, item, col) && ValueChanged(item, col);
    }

    // Get text attribute, return false of default attributes should be used
    virtual bool GetAttr(const wxDataViewItem &WXUNUSED(item),
                         unsigned int WXUNUSED(col),
                         wxDataViewItemAttr &WXUNUSED(attr)) const
    {
        return false;
    }

    // Override this if you want to disable specific items
    virtual bool IsEnabled(const wxDataViewItem &WXUNUSED(item),
                           unsigned int WXUNUSED(col)) const
    {
        return true;
    }

    // define hierarchy
    virtual wxDataViewItem GetParent( const wxDataViewItem &item ) const = 0;
    virtual bool IsContainer( const wxDataViewItem &item ) const = 0;
    // Is the container just a header or an item with all columns
    virtual bool HasContainerColumns(const wxDataViewItem& WXUNUSED(item)) const
        { return false; }
    virtual unsigned int GetChildren( const wxDataViewItem &item, wxDataViewItemArray &children ) const = 0;

    // delegated notifiers
    bool ItemAdded( const wxDataViewItem &parent, const wxDataViewItem &item );
    bool ItemsAdded( const wxDataViewItem &parent, const wxDataViewItemArray &items );
    bool ItemDeleted( const wxDataViewItem &parent, const wxDataViewItem &item );
    bool ItemsDeleted( const wxDataViewItem &parent, const wxDataViewItemArray &items );
    bool ItemChanged( const wxDataViewItem &item );
    bool ItemsChanged( const wxDataViewItemArray &items );
    bool ValueChanged( const wxDataViewItem &item, unsigned int col );
    bool Cleared();

    // some platforms, such as GTK+, may need a two step procedure for ::Reset()
    bool BeforeReset();
    bool AfterReset();


    // delegated action
    virtual void Resort();

    void AddNotifier( wxDataViewModelNotifier *notifier );
    void RemoveNotifier( wxDataViewModelNotifier *notifier );

    // default compare function
    virtual int Compare( const wxDataViewItem &item1, const wxDataViewItem &item2,
                         unsigned int column, bool ascending ) const;
    virtual bool HasDefaultCompare() const { return false; }

    // internal
    virtual bool IsListModel() const { return false; }
    virtual bool IsVirtualListModel() const { return false; }

protected:
    // the user should not delete this class directly: he should use DecRef() instead!
    virtual ~wxDataViewModel() { }

    wxDataViewModelNotifiers  m_notifiers;
};

// ----------------------------------------------------------------------------
// wxDataViewListModel: a model of a list, i.e. flat data structure without any
//      branches/containers, used as base class by wxDataViewIndexListModel and
//      wxDataViewVirtualListModel
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewListModel : public wxDataViewModel
{
public:
    // derived classes should override these methods instead of
    // {Get,Set}Value() and GetAttr() inherited from the base class

    virtual void GetValueByRow(wxVariant &variant,
                               unsigned row, unsigned col) const = 0;

    virtual bool SetValueByRow(const wxVariant &variant,
                               unsigned row, unsigned col) = 0;

    virtual bool
    GetAttrByRow(unsigned WXUNUSED(row), unsigned WXUNUSED(col),
                 wxDataViewItemAttr &WXUNUSED(attr)) const
    {
        return false;
    }

    virtual bool IsEnabledByRow(unsigned int WXUNUSED(row),
                                unsigned int WXUNUSED(col)) const
    {
        return true;
    }


    // helper methods provided by list models only
    virtual unsigned GetRow( const wxDataViewItem &item ) const = 0;

    // returns the number of rows
    virtual unsigned int GetCount() const = 0;

    // implement some base class pure virtual directly
    virtual wxDataViewItem
    GetParent( const wxDataViewItem & WXUNUSED(item) ) const
    {
        // items never have valid parent in this model
        return wxDataViewItem();
    }

    virtual bool IsContainer( const wxDataViewItem &item ) const
    {
        // only the invisible (and invalid) root item has children
        return !item.IsOk();
    }

    // and implement some others by forwarding them to our own ones
    virtual void GetValue( wxVariant &variant,
                           const wxDataViewItem &item, unsigned int col ) const
    {
        GetValueByRow(variant, GetRow(item), col);
    }

    virtual bool SetValue( const wxVariant &variant,
                           const wxDataViewItem &item, unsigned int col )
    {
        return SetValueByRow( variant, GetRow(item), col );
    }

    virtual bool GetAttr(const wxDataViewItem &item, unsigned int col,
                         wxDataViewItemAttr &attr) const
    {
        return GetAttrByRow( GetRow(item), col, attr );
    }

    virtual bool IsEnabled(const wxDataViewItem &item, unsigned int col) const
    {
        return IsEnabledByRow( GetRow(item), col );
    }


    virtual bool IsListModel() const { return true; }
};

// ---------------------------------------------------------
// wxDataViewIndexListModel
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewIndexListModel: public wxDataViewListModel
{
public:
    wxDataViewIndexListModel( unsigned int initial_size = 0 );

    void RowPrepended();
    void RowInserted( unsigned int before );
    void RowAppended();
    void RowDeleted( unsigned int row );
    void RowsDeleted( const wxArrayInt &rows );
    void RowChanged( unsigned int row );
    void RowValueChanged( unsigned int row, unsigned int col );
    void Reset( unsigned int new_size );

    // convert to/from row/wxDataViewItem

    virtual unsigned GetRow( const wxDataViewItem &item ) const;
    wxDataViewItem GetItem( unsigned int row ) const;

    // implement base methods
    virtual unsigned int GetChildren( const wxDataViewItem &item, wxDataViewItemArray &children ) const;

    unsigned int GetCount() const { return m_hash.GetCount(); }

private:
    wxDataViewItemArray m_hash;
    unsigned int m_nextFreeID;
    bool m_ordered;
};

// ---------------------------------------------------------
// wxDataViewVirtualListModel
// ---------------------------------------------------------

#ifdef __WXMAC__
// better than nothing
typedef wxDataViewIndexListModel wxDataViewVirtualListModel;
#else

class WXDLLIMPEXP_ADV wxDataViewVirtualListModel: public wxDataViewListModel
{
public:
    wxDataViewVirtualListModel( unsigned int initial_size = 0 );

    void RowPrepended();
    void RowInserted( unsigned int before );
    void RowAppended();
    void RowDeleted( unsigned int row );
    void RowsDeleted( const wxArrayInt &rows );
    void RowChanged( unsigned int row );
    void RowValueChanged( unsigned int row, unsigned int col );
    void Reset( unsigned int new_size );

    // convert to/from row/wxDataViewItem

    virtual unsigned GetRow( const wxDataViewItem &item ) const;
    wxDataViewItem GetItem( unsigned int row ) const;

    // compare based on index

    virtual int Compare( const wxDataViewItem &item1, const wxDataViewItem &item2,
                         unsigned int column, bool ascending ) const;
    virtual bool HasDefaultCompare() const;

    // implement base methods
    virtual unsigned int GetChildren( const wxDataViewItem &item, wxDataViewItemArray &children ) const;

    unsigned int GetCount() const { return m_size; }

    // internal
    virtual bool IsVirtualListModel() const { return true; }

private:
    unsigned int m_size;
};
#endif

// ----------------------------------------------------------------------------
// wxDataViewRenderer and related classes
// ----------------------------------------------------------------------------

#include "wx/dvrenderers.h"

// ---------------------------------------------------------
// wxDataViewColumnBase
// ---------------------------------------------------------

// for compatibility only, do not use
enum wxDataViewColumnFlags
{
    wxDATAVIEW_COL_RESIZABLE     = wxCOL_RESIZABLE,
    wxDATAVIEW_COL_SORTABLE      = wxCOL_SORTABLE,
    wxDATAVIEW_COL_REORDERABLE   = wxCOL_REORDERABLE,
    wxDATAVIEW_COL_HIDDEN        = wxCOL_HIDDEN
};

class WXDLLIMPEXP_ADV wxDataViewColumnBase : public wxSettableHeaderColumn
{
public:
    // ctor for the text columns: takes ownership of renderer
    wxDataViewColumnBase(wxDataViewRenderer *renderer,
                         unsigned int model_column)
    {
        Init(renderer, model_column);
    }

    // ctor for the bitmap columns
    wxDataViewColumnBase(const wxBitmap& bitmap,
                         wxDataViewRenderer *renderer,
                         unsigned int model_column)
        : m_bitmap(bitmap)
    {
        Init(renderer, model_column);
    }

    virtual ~wxDataViewColumnBase();

    // setters:
    virtual void SetOwner( wxDataViewCtrl *owner )
        { m_owner = owner; }

    // getters:
    unsigned int GetModelColumn() const { return static_cast<unsigned int>(m_model_column); }
    wxDataViewCtrl *GetOwner() const        { return m_owner; }
    wxDataViewRenderer* GetRenderer() const { return m_renderer; }

    // implement some of base class pure virtuals (the rest is port-dependent
    // and done differently in generic and native versions)
    virtual void SetBitmap( const wxBitmap& bitmap ) { m_bitmap = bitmap; }
    virtual wxBitmap GetBitmap() const { return m_bitmap; }

protected:
    wxDataViewRenderer      *m_renderer;
    int                      m_model_column;
    wxBitmap                 m_bitmap;
    wxDataViewCtrl          *m_owner;

private:
    // common part of all ctors
    void Init(wxDataViewRenderer *renderer, unsigned int model_column);
};

// ---------------------------------------------------------
// wxDataViewCtrlBase
// ---------------------------------------------------------

#define wxDV_SINGLE                  0x0000     // for convenience
#define wxDV_MULTIPLE                0x0001     // can select multiple items

#define wxDV_NO_HEADER               0x0002     // column titles not visible
#define wxDV_HORIZ_RULES             0x0004     // light horizontal rules between rows
#define wxDV_VERT_RULES              0x0008     // light vertical rules between columns

#define wxDV_ROW_LINES               0x0010     // alternating colour in rows
#define wxDV_VARIABLE_LINE_HEIGHT    0x0020     // variable line height

class WXDLLIMPEXP_ADV wxDataViewCtrlBase: public wxControl
{
public:
    wxDataViewCtrlBase();
    virtual ~wxDataViewCtrlBase();

    // model
    // -----

    virtual bool AssociateModel( wxDataViewModel *model );
    wxDataViewModel* GetModel();
    const wxDataViewModel* GetModel() const;


    // column management
    // -----------------

    wxDataViewColumn *PrependTextColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependIconTextColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependToggleColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = wxDVC_TOGGLE_DEFAULT_WIDTH,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependProgressColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = wxDVC_DEFAULT_WIDTH,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependDateColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_ACTIVATABLE, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependBitmapColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependTextColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependIconTextColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependToggleColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = wxDVC_TOGGLE_DEFAULT_WIDTH,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependProgressColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = wxDVC_DEFAULT_WIDTH,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependDateColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_ACTIVATABLE, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *PrependBitmapColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );

    wxDataViewColumn *AppendTextColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendIconTextColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendToggleColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = wxDVC_TOGGLE_DEFAULT_WIDTH,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendProgressColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = wxDVC_DEFAULT_WIDTH,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendDateColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_ACTIVATABLE, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendBitmapColumn( const wxString &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendTextColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendIconTextColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendToggleColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = wxDVC_TOGGLE_DEFAULT_WIDTH,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendProgressColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = wxDVC_DEFAULT_WIDTH,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendDateColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_ACTIVATABLE, int width = -1,
                    wxAlignment align = wxALIGN_NOT,
                    int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendBitmapColumn( const wxBitmap &label, unsigned int model_column,
                    wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT, int width = -1,
                    wxAlignment align = wxALIGN_CENTER,
                    int flags = wxDATAVIEW_COL_RESIZABLE );

    virtual bool PrependColumn( wxDataViewColumn *col );
    virtual bool InsertColumn( unsigned int pos, wxDataViewColumn *col );
    virtual bool AppendColumn( wxDataViewColumn *col );

    virtual unsigned int GetColumnCount() const = 0;
    virtual wxDataViewColumn* GetColumn( unsigned int pos ) const = 0;
    virtual int GetColumnPosition( const wxDataViewColumn *column ) const = 0;

    virtual bool DeleteColumn( wxDataViewColumn *column ) = 0;
    virtual bool ClearColumns() = 0;

    void SetExpanderColumn( wxDataViewColumn *col )
        { m_expander_column = col ; DoSetExpanderColumn(); }
    wxDataViewColumn *GetExpanderColumn() const
        { return m_expander_column; }

    virtual wxDataViewColumn *GetSortingColumn() const = 0;


    // items management
    // ----------------

    void SetIndent( int indent )
        { m_indent = indent ; DoSetIndent(); }
    int GetIndent() const
        { return m_indent; }

    // Current item is the one used by the keyboard navigation, it is the same
    // as the (unique) selected item in single selection mode so these
    // functions are mostly useful for controls with wxDV_MULTIPLE style.
    wxDataViewItem GetCurrentItem() const;
    void SetCurrentItem(const wxDataViewItem& item);

    // Currently focused column of the current item or NULL if no column has focus
    virtual wxDataViewColumn *GetCurrentColumn() const = 0;

    // Selection: both GetSelection() and GetSelections() can be used for the
    // controls both with and without wxDV_MULTIPLE style. For single selection
    // controls GetSelections() is not very useful however. And for multi
    // selection controls GetSelection() returns an invalid item if more than
    // one item is selected. Use GetSelectedItemsCount() or HasSelection() to
    // check if any items are selected at all.
    virtual int GetSelectedItemsCount() const = 0;
    bool HasSelection() const { return GetSelectedItemsCount() != 0; }
    wxDataViewItem GetSelection() const;
    virtual int GetSelections( wxDataViewItemArray & sel ) const = 0;
    virtual void SetSelections( const wxDataViewItemArray & sel ) = 0;
    virtual void Select( const wxDataViewItem & item ) = 0;
    virtual void Unselect( const wxDataViewItem & item ) = 0;
    virtual bool IsSelected( const wxDataViewItem & item ) const = 0;

    virtual void SelectAll() = 0;
    virtual void UnselectAll() = 0;

    virtual void Expand( const wxDataViewItem & item ) = 0;
    virtual void ExpandAncestors( const wxDataViewItem & item );
    virtual void Collapse( const wxDataViewItem & item ) = 0;
    virtual bool IsExpanded( const wxDataViewItem & item ) const = 0;

    virtual void EnsureVisible( const wxDataViewItem & item,
                                const wxDataViewColumn *column = NULL ) = 0;
    virtual void HitTest( const wxPoint & point, wxDataViewItem &item, wxDataViewColumn* &column ) const = 0;
    virtual wxRect GetItemRect( const wxDataViewItem & item, const wxDataViewColumn *column = NULL ) const = 0;

    virtual bool SetRowHeight( int WXUNUSED(rowHeight) ) { return false; }

    virtual void EditItem(const wxDataViewItem& item, const wxDataViewColumn *column) = 0;

    // Use EditItem() instead
    wxDEPRECATED( void StartEditor(const wxDataViewItem& item, unsigned int column) );

#if wxUSE_DRAG_AND_DROP
    virtual bool EnableDragSource(const wxDataFormat& WXUNUSED(format))
        { return false; }
    virtual bool EnableDropTarget(const wxDataFormat& WXUNUSED(format))
        { return false; }
#endif // wxUSE_DRAG_AND_DROP

    // define control visual attributes
    // --------------------------------

    virtual wxVisualAttributes GetDefaultAttributes() const
    {
        return GetClassDefaultAttributes(GetWindowVariant());
    }

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL)
    {
        return wxControl::GetCompositeControlsDefaultAttributes(variant);
    }

protected:
    virtual void DoSetExpanderColumn() = 0 ;
    virtual void DoSetIndent() = 0;

private:
    // Implementation of the public Set/GetCurrentItem() methods which are only
    // called in multi selection case (for single selection controls their
    // implementation is trivial and is done in the base class itself).
    virtual wxDataViewItem DoGetCurrentItem() const = 0;
    virtual void DoSetCurrentItem(const wxDataViewItem& item) = 0;

    wxDataViewModel        *m_model;
    wxDataViewColumn       *m_expander_column;
    int m_indent ;

protected:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewCtrlBase)
};

// ----------------------------------------------------------------------------
// wxDataViewEvent - the event class for the wxDataViewCtrl notifications
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewEvent : public wxNotifyEvent
{
public:
    wxDataViewEvent(wxEventType commandType = wxEVT_NULL, int winid = 0)
        : wxNotifyEvent(commandType, winid),
        m_item(0),
        m_col(-1),
        m_model(NULL),
        m_value(wxNullVariant),
        m_column(NULL),
        m_pos(-1,-1),
        m_cacheFrom(0),
        m_cacheTo(0),
        m_editCancelled(false)
#if wxUSE_DRAG_AND_DROP
        , m_dataObject(NULL),
        m_dataBuffer(NULL),
        m_dataSize(0),
        m_dragFlags(0),
        m_dropEffect(wxDragNone)
#endif
        { }

    wxDataViewEvent(const wxDataViewEvent& event)
        : wxNotifyEvent(event),
        m_item(event.m_item),
        m_col(event.m_col),
        m_model(event.m_model),
        m_value(event.m_value),
        m_column(event.m_column),
        m_pos(event.m_pos),
        m_cacheFrom(event.m_cacheFrom),
        m_cacheTo(event.m_cacheTo),
        m_editCancelled(event.m_editCancelled)
#if wxUSE_DRAG_AND_DROP
        , m_dataObject(event.m_dataObject),
        m_dataFormat(event.m_dataFormat),
        m_dataBuffer(event.m_dataBuffer),
        m_dataSize(event.m_dataSize),
        m_dragFlags(event.m_dragFlags),
        m_dropEffect(event.m_dropEffect)
#endif
        { }

    wxDataViewItem GetItem() const { return m_item; }
    void SetItem( const wxDataViewItem &item ) { m_item = item; }

    int GetColumn() const { return m_col; }
    void SetColumn( int col ) { m_col = col; }

    wxDataViewModel* GetModel() const { return m_model; }
    void SetModel( wxDataViewModel *model ) { m_model = model; }

    const wxVariant &GetValue() const { return m_value; }
    void SetValue( const wxVariant &value ) { m_value = value; }

    // for wxEVT_DATAVIEW_ITEM_EDITING_DONE only
    bool IsEditCancelled() const { return m_editCancelled; }
    void SetEditCanceled(bool editCancelled) { m_editCancelled = editCancelled; }

    // for wxEVT_DATAVIEW_COLUMN_HEADER_CLICKED only
    void SetDataViewColumn( wxDataViewColumn *col ) { m_column = col; }
    wxDataViewColumn *GetDataViewColumn() const { return m_column; }

    // for wxEVT_DATAVIEW_CONTEXT_MENU only
    wxPoint GetPosition() const { return m_pos; }
    void SetPosition( int x, int y ) { m_pos.x = x; m_pos.y = y; }

    // For wxEVT_DATAVIEW_CACHE_HINT
    int GetCacheFrom() const { return m_cacheFrom; }
    int GetCacheTo() const { return m_cacheTo; }
    void SetCache(int from, int to) { m_cacheFrom = from; m_cacheTo = to; }


#if wxUSE_DRAG_AND_DROP
    // For drag operations
    void SetDataObject( wxDataObject *obj ) { m_dataObject = obj; }
    wxDataObject *GetDataObject() const { return m_dataObject; }

    // For drop operations
    void SetDataFormat( const wxDataFormat &format ) { m_dataFormat = format; }
    wxDataFormat GetDataFormat() const { return m_dataFormat; }
    void SetDataSize( size_t size ) { m_dataSize = size; }
    size_t GetDataSize() const { return m_dataSize; }
    void SetDataBuffer( void* buf ) { m_dataBuffer = buf;}
    void *GetDataBuffer() const { return m_dataBuffer; }
    void SetDragFlags( int flags ) { m_dragFlags = flags; }
    int GetDragFlags() const { return m_dragFlags; }
    void SetDropEffect( wxDragResult effect ) { m_dropEffect = effect; }
    wxDragResult GetDropEffect() const { return m_dropEffect; }
#endif // wxUSE_DRAG_AND_DROP

    virtual wxEvent *Clone() const { return new wxDataViewEvent(*this); }

protected:
    wxDataViewItem      m_item;
    int                 m_col;
    wxDataViewModel    *m_model;
    wxVariant           m_value;
    wxDataViewColumn   *m_column;
    wxPoint             m_pos;
    int                 m_cacheFrom;
    int                 m_cacheTo;
    bool                m_editCancelled;

#if wxUSE_DRAG_AND_DROP
    wxDataObject       *m_dataObject;

    wxDataFormat        m_dataFormat;
    void*               m_dataBuffer;
    size_t              m_dataSize;

    int                 m_dragFlags;
    wxDragResult        m_dropEffect;
#endif // wxUSE_DRAG_AND_DROP

private:
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxDataViewEvent)
};

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_SELECTION_CHANGED, wxDataViewEvent );

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_ACTIVATED, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_COLLAPSED, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_EXPANDED, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_COLLAPSING, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_EXPANDING, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_START_EDITING, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_EDITING_STARTED, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_EDITING_DONE, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, wxDataViewEvent );

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, wxDataViewEvent );

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_COLUMN_HEADER_CLICK, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_COLUMN_HEADER_RIGHT_CLICK, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_COLUMN_SORTED, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_COLUMN_REORDERED, wxDataViewEvent );

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_CACHE_HINT, wxDataViewEvent );

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_BEGIN_DRAG, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_DROP_POSSIBLE, wxDataViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_DATAVIEW_ITEM_DROP, wxDataViewEvent );

typedef void (wxEvtHandler::*wxDataViewEventFunction)(wxDataViewEvent&);

#define wxDataViewEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxDataViewEventFunction, func)

#define wx__DECLARE_DATAVIEWEVT(evt, id, fn) \
    wx__DECLARE_EVT1(wxEVT_DATAVIEW_ ## evt, id, wxDataViewEventHandler(fn))

#define EVT_DATAVIEW_SELECTION_CHANGED(id, fn) wx__DECLARE_DATAVIEWEVT(SELECTION_CHANGED, id, fn)

#define EVT_DATAVIEW_ITEM_ACTIVATED(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_ACTIVATED, id, fn)
#define EVT_DATAVIEW_ITEM_COLLAPSING(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_COLLAPSING, id, fn)
#define EVT_DATAVIEW_ITEM_COLLAPSED(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_COLLAPSED, id, fn)
#define EVT_DATAVIEW_ITEM_EXPANDING(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_EXPANDING, id, fn)
#define EVT_DATAVIEW_ITEM_EXPANDED(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_EXPANDED, id, fn)
#define EVT_DATAVIEW_ITEM_START_EDITING(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_START_EDITING, id, fn)
#define EVT_DATAVIEW_ITEM_EDITING_STARTED(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_EDITING_STARTED, id, fn)
#define EVT_DATAVIEW_ITEM_EDITING_DONE(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_EDITING_DONE, id, fn)
#define EVT_DATAVIEW_ITEM_VALUE_CHANGED(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_VALUE_CHANGED, id, fn)

#define EVT_DATAVIEW_ITEM_CONTEXT_MENU(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_CONTEXT_MENU, id, fn)

#define EVT_DATAVIEW_COLUMN_HEADER_CLICK(id, fn) wx__DECLARE_DATAVIEWEVT(COLUMN_HEADER_CLICK, id, fn)
#define EVT_DATAVIEW_COLUMN_HEADER_RIGHT_CLICK(id, fn) wx__DECLARE_DATAVIEWEVT(COLUMN_HEADER_RIGHT_CLICK, id, fn)
#define EVT_DATAVIEW_COLUMN_SORTED(id, fn) wx__DECLARE_DATAVIEWEVT(COLUMN_SORTED, id, fn)
#define EVT_DATAVIEW_COLUMN_REORDERED(id, fn) wx__DECLARE_DATAVIEWEVT(COLUMN_REORDERED, id, fn)
#define EVT_DATAVIEW_CACHE_HINT(id, fn) wx__DECLARE_DATAVIEWEVT(CACHE_HINT, id, fn)

#define EVT_DATAVIEW_ITEM_BEGIN_DRAG(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_BEGIN_DRAG, id, fn)
#define EVT_DATAVIEW_ITEM_DROP_POSSIBLE(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_DROP_POSSIBLE, id, fn)
#define EVT_DATAVIEW_ITEM_DROP(id, fn) wx__DECLARE_DATAVIEWEVT(ITEM_DROP, id, fn)

// Old and not documented synonym, don't use.
#define EVT_DATAVIEW_COLUMN_HEADER_RIGHT_CLICKED(id, fn) EVT_DATAVIEW_COLUMN_HEADER_RIGHT_CLICK(id, fn)

#ifdef wxHAS_GENERIC_DATAVIEWCTRL
    #include "wx/generic/dataview.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/dataview.h"
#elif defined(__WXMAC__)
    #include "wx/osx/dataview.h"
#else
    #error "unknown native wxDataViewCtrl implementation"
#endif

//-----------------------------------------------------------------------------
// wxDataViewListStore
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewListStoreLine
{
public:
    wxDataViewListStoreLine( wxUIntPtr data = 0 )
    {
        m_data = data;
    }

    void SetData( wxUIntPtr data )
        { m_data = data; }
    wxUIntPtr GetData() const
        { return m_data; }

    wxVector<wxVariant>  m_values;

private:
    wxUIntPtr m_data;
};


class WXDLLIMPEXP_ADV wxDataViewListStore: public wxDataViewIndexListModel
{
public:
    wxDataViewListStore();
    ~wxDataViewListStore();

    void PrependColumn( const wxString &varianttype );
    void InsertColumn( unsigned int pos, const wxString &varianttype );
    void AppendColumn( const wxString &varianttype );

    void AppendItem( const wxVector<wxVariant> &values, wxUIntPtr data = 0 );
    void PrependItem( const wxVector<wxVariant> &values, wxUIntPtr data = 0 );
    void InsertItem(  unsigned int row, const wxVector<wxVariant> &values, wxUIntPtr data = 0 );
    void DeleteItem( unsigned int pos );
    void DeleteAllItems();
#if wxABI_VERSION >= 30001
    void ClearColumns();
#endif

    unsigned int GetItemCount() const;

    void SetItemData( const wxDataViewItem& item, wxUIntPtr data );
    wxUIntPtr GetItemData( const wxDataViewItem& item ) const;

    // override base virtuals

    virtual unsigned int GetColumnCount() const;

    virtual wxString GetColumnType( unsigned int col ) const;

    virtual void GetValueByRow( wxVariant &value,
                           unsigned int row, unsigned int col ) const;

    virtual bool SetValueByRow( const wxVariant &value,
                           unsigned int row, unsigned int col );


public:
    wxVector<wxDataViewListStoreLine*> m_data;
    wxArrayString                      m_cols;
};

//-----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewListCtrl: public wxDataViewCtrl
{
public:
    wxDataViewListCtrl();
    wxDataViewListCtrl( wxWindow *parent, wxWindowID id,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = wxDV_ROW_LINES,
           const wxValidator& validator = wxDefaultValidator );
    ~wxDataViewListCtrl();

    bool Create( wxWindow *parent, wxWindowID id,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = wxDV_ROW_LINES,
           const wxValidator& validator = wxDefaultValidator );

    wxDataViewListStore *GetStore()
        { return (wxDataViewListStore*) GetModel(); }
    const wxDataViewListStore *GetStore() const
        { return (const wxDataViewListStore*) GetModel(); }

    int ItemToRow(const wxDataViewItem &item) const
        { return item.IsOk() ? (int)GetStore()->GetRow(item) : wxNOT_FOUND; }
    wxDataViewItem RowToItem(int row) const
        { return row == wxNOT_FOUND ? wxDataViewItem() : GetStore()->GetItem(row); }

    int GetSelectedRow() const
        { return ItemToRow(GetSelection()); }
    void SelectRow(unsigned row)
        { Select(RowToItem(row)); }
    void UnselectRow(unsigned row)
        { Unselect(RowToItem(row)); }
    bool IsRowSelected(unsigned row) const
        { return IsSelected(RowToItem(row)); }

    bool AppendColumn( wxDataViewColumn *column, const wxString &varianttype );
    bool PrependColumn( wxDataViewColumn *column, const wxString &varianttype );
    bool InsertColumn( unsigned int pos, wxDataViewColumn *column, const wxString &varianttype );

    // overridden from base class
    virtual bool PrependColumn( wxDataViewColumn *col );
    virtual bool InsertColumn( unsigned int pos, wxDataViewColumn *col );
    virtual bool AppendColumn( wxDataViewColumn *col );
#if wxABI_VERSION >= 30001
    virtual bool ClearColumns();
#endif

    wxDataViewColumn *AppendTextColumn( const wxString &label,
          wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
          int width = -1, wxAlignment align = wxALIGN_LEFT, int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendToggleColumn( const wxString &label,
          wxDataViewCellMode mode = wxDATAVIEW_CELL_ACTIVATABLE,
          int width = -1, wxAlignment align = wxALIGN_LEFT, int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendProgressColumn( const wxString &label,
          wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
          int width = -1, wxAlignment align = wxALIGN_LEFT, int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn *AppendIconTextColumn( const wxString &label,
          wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
          int width = -1, wxAlignment align = wxALIGN_LEFT, int flags = wxDATAVIEW_COL_RESIZABLE );

    void AppendItem( const wxVector<wxVariant> &values, wxUIntPtr data = 0 )
        { GetStore()->AppendItem( values, data ); }
    void PrependItem( const wxVector<wxVariant> &values, wxUIntPtr data = 0 )
        { GetStore()->PrependItem( values, data ); }
    void InsertItem(  unsigned int row, const wxVector<wxVariant> &values, wxUIntPtr data = 0 )
        { GetStore()->InsertItem( row, values, data ); }
    void DeleteItem( unsigned row )
        { GetStore()->DeleteItem( row ); }
    void DeleteAllItems()
        { GetStore()->DeleteAllItems(); }

    void SetValue( const wxVariant &value, unsigned int row, unsigned int col )
        { GetStore()->SetValueByRow( value, row, col );
          GetStore()->RowValueChanged( row, col); }
    void GetValue( wxVariant &value, unsigned int row, unsigned int col )
        { GetStore()->GetValueByRow( value, row, col ); }

    void SetTextValue( const wxString &value, unsigned int row, unsigned int col )
        { GetStore()->SetValueByRow( value, row, col );
          GetStore()->RowValueChanged( row, col); }
    wxString GetTextValue( unsigned int row, unsigned int col ) const
        { wxVariant value; GetStore()->GetValueByRow( value, row, col ); return value.GetString(); }

    void SetToggleValue( bool value, unsigned int row, unsigned int col )
        { GetStore()->SetValueByRow( value, row, col );
          GetStore()->RowValueChanged( row, col); }
    bool GetToggleValue( unsigned int row, unsigned int col ) const
        { wxVariant value; GetStore()->GetValueByRow( value, row, col ); return value.GetBool(); }

    void SetItemData( const wxDataViewItem& item, wxUIntPtr data )
        { GetStore()->SetItemData( item, data ); }
    wxUIntPtr GetItemData( const wxDataViewItem& item ) const
        { return GetStore()->GetItemData( item ); }

    int GetItemCount() const
        { return GetStore()->GetItemCount(); }

    void OnSize( wxSizeEvent &event );

private:
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxDataViewListCtrl)
};

//-----------------------------------------------------------------------------
// wxDataViewTreeStore
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewTreeStoreNode
{
public:
    wxDataViewTreeStoreNode( wxDataViewTreeStoreNode *parent,
        const wxString &text, const wxIcon &icon = wxNullIcon, wxClientData *data = NULL );
    virtual ~wxDataViewTreeStoreNode();

    void SetText( const wxString &text )
        { m_text = text; }
    wxString GetText() const
        { return m_text; }
    void SetIcon( const wxIcon &icon )
        { m_icon = icon; }
    const wxIcon &GetIcon() const
        { return m_icon; }
    void SetData( wxClientData *data )
        { if (m_data) delete m_data; m_data = data; }
    wxClientData *GetData() const
        { return m_data; }

    wxDataViewItem GetItem() const
        { return wxDataViewItem( (void*) this ); }

    virtual bool IsContainer()
        { return false; }

    wxDataViewTreeStoreNode *GetParent()
        { return m_parent; }

private:
    wxDataViewTreeStoreNode  *m_parent;
    wxString                  m_text;
    wxIcon                    m_icon;
    wxClientData             *m_data;
};

WX_DECLARE_LIST_WITH_DECL(wxDataViewTreeStoreNode, wxDataViewTreeStoreNodeList,
                          class WXDLLIMPEXP_ADV);

class WXDLLIMPEXP_ADV wxDataViewTreeStoreContainerNode: public wxDataViewTreeStoreNode
{
public:
    wxDataViewTreeStoreContainerNode( wxDataViewTreeStoreNode *parent,
        const wxString &text, const wxIcon &icon = wxNullIcon, const wxIcon &expanded = wxNullIcon,
        wxClientData *data = NULL );
    virtual ~wxDataViewTreeStoreContainerNode();

    const wxDataViewTreeStoreNodeList &GetChildren() const
        { return m_children; }
    wxDataViewTreeStoreNodeList &GetChildren()
        { return m_children; }

    void SetExpandedIcon( const wxIcon &icon )
        { m_iconExpanded = icon; }
    const wxIcon &GetExpandedIcon() const
        { return m_iconExpanded; }

    void SetExpanded( bool expanded = true )
        { m_isExpanded = expanded; }
    bool IsExpanded() const
        { return m_isExpanded; }

    virtual bool IsContainer()
        { return true; }

private:
    wxDataViewTreeStoreNodeList  m_children;
    wxIcon                       m_iconExpanded;
    bool                         m_isExpanded;
};

//-----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewTreeStore: public wxDataViewModel
{
public:
    wxDataViewTreeStore();
    ~wxDataViewTreeStore();

    wxDataViewItem AppendItem( const wxDataViewItem& parent,
        const wxString &text, const wxIcon &icon = wxNullIcon, wxClientData *data = NULL );
    wxDataViewItem PrependItem( const wxDataViewItem& parent,
        const wxString &text, const wxIcon &icon = wxNullIcon, wxClientData *data = NULL );
    wxDataViewItem InsertItem( const wxDataViewItem& parent, const wxDataViewItem& previous,
        const wxString &text, const wxIcon &icon = wxNullIcon, wxClientData *data = NULL );

    wxDataViewItem PrependContainer( const wxDataViewItem& parent,
        const wxString &text, const wxIcon &icon = wxNullIcon, const wxIcon &expanded = wxNullIcon,
        wxClientData *data = NULL );
    wxDataViewItem AppendContainer( const wxDataViewItem& parent,
        const wxString &text, const wxIcon &icon = wxNullIcon, const wxIcon &expanded = wxNullIcon,
        wxClientData *data = NULL );
    wxDataViewItem InsertContainer( const wxDataViewItem& parent, const wxDataViewItem& previous,
        const wxString &text, const wxIcon &icon = wxNullIcon, const wxIcon &expanded = wxNullIcon,
        wxClientData *data = NULL );

    wxDataViewItem GetNthChild( const wxDataViewItem& parent, unsigned int pos ) const;
    int GetChildCount( const wxDataViewItem& parent ) const;

    void SetItemText( const wxDataViewItem& item, const wxString &text );
    wxString GetItemText( const wxDataViewItem& item ) const;
    void SetItemIcon( const wxDataViewItem& item, const wxIcon &icon );
    const wxIcon &GetItemIcon( const wxDataViewItem& item ) const;
    void SetItemExpandedIcon( const wxDataViewItem& item, const wxIcon &icon );
    const wxIcon &GetItemExpandedIcon( const wxDataViewItem& item ) const;
    void SetItemData( const wxDataViewItem& item, wxClientData *data );
    wxClientData *GetItemData( const wxDataViewItem& item ) const;

    void DeleteItem( const wxDataViewItem& item );
    void DeleteChildren( const wxDataViewItem& item );
    void DeleteAllItems();

    // implement base methods

    virtual void GetValue( wxVariant &variant,
                           const wxDataViewItem &item, unsigned int col ) const;
    virtual bool SetValue( const wxVariant &variant,
                           const wxDataViewItem &item, unsigned int col );
    virtual wxDataViewItem GetParent( const wxDataViewItem &item ) const;
    virtual bool IsContainer( const wxDataViewItem &item ) const;
    virtual unsigned int GetChildren( const wxDataViewItem &item, wxDataViewItemArray &children ) const;

    virtual int Compare( const wxDataViewItem &item1, const wxDataViewItem &item2,
                         unsigned int column, bool ascending ) const;

    virtual bool HasDefaultCompare() const
        { return true; }
    virtual unsigned int GetColumnCount() const
        { return 1; }
    virtual wxString GetColumnType( unsigned int WXUNUSED(col) ) const
        { return wxT("wxDataViewIconText"); }

    wxDataViewTreeStoreNode *FindNode( const wxDataViewItem &item ) const;
    wxDataViewTreeStoreContainerNode *FindContainerNode( const wxDataViewItem &item ) const;
    wxDataViewTreeStoreNode *GetRoot() const { return m_root; }

public:
    wxDataViewTreeStoreNode *m_root;
};

//-----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewTreeCtrl: public wxDataViewCtrl,
                                          public wxWithImages
{
public:
    wxDataViewTreeCtrl() { }
    wxDataViewTreeCtrl(wxWindow *parent,
                       wxWindowID id,
                       const wxPoint& pos = wxDefaultPosition,
                       const wxSize& size = wxDefaultSize,
                       long style = wxDV_NO_HEADER | wxDV_ROW_LINES,
                       const wxValidator& validator = wxDefaultValidator)
    {
        Create(parent, id, pos, size, style, validator);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDV_NO_HEADER | wxDV_ROW_LINES,
                const wxValidator& validator = wxDefaultValidator);

    wxDataViewTreeStore *GetStore()
        { return (wxDataViewTreeStore*) GetModel(); }
    const wxDataViewTreeStore *GetStore() const
        { return (const wxDataViewTreeStore*) GetModel(); }

    bool IsContainer( const wxDataViewItem& item ) const
        { return GetStore()->IsContainer(item); }

    wxDataViewItem AppendItem( const wxDataViewItem& parent,
        const wxString &text, int icon = NO_IMAGE, wxClientData *data = NULL );
    wxDataViewItem PrependItem( const wxDataViewItem& parent,
        const wxString &text, int icon = NO_IMAGE, wxClientData *data = NULL );
    wxDataViewItem InsertItem( const wxDataViewItem& parent, const wxDataViewItem& previous,
        const wxString &text, int icon = NO_IMAGE, wxClientData *data = NULL );

    wxDataViewItem PrependContainer( const wxDataViewItem& parent,
        const wxString &text, int icon = NO_IMAGE, int expanded = NO_IMAGE,
        wxClientData *data = NULL );
    wxDataViewItem AppendContainer( const wxDataViewItem& parent,
        const wxString &text, int icon = NO_IMAGE, int expanded = NO_IMAGE,
        wxClientData *data = NULL );
    wxDataViewItem InsertContainer( const wxDataViewItem& parent, const wxDataViewItem& previous,
        const wxString &text, int icon = NO_IMAGE, int expanded = NO_IMAGE,
        wxClientData *data = NULL );

    wxDataViewItem GetNthChild( const wxDataViewItem& parent, unsigned int pos ) const
        { return GetStore()->GetNthChild(parent, pos); }
    int GetChildCount( const wxDataViewItem& parent ) const
        { return GetStore()->GetChildCount(parent); }

    void SetItemText( const wxDataViewItem& item, const wxString &text );
    wxString GetItemText( const wxDataViewItem& item ) const
        { return GetStore()->GetItemText(item); }
    void SetItemIcon( const wxDataViewItem& item, const wxIcon &icon );
    const wxIcon &GetItemIcon( const wxDataViewItem& item ) const
        { return GetStore()->GetItemIcon(item); }
    void SetItemExpandedIcon( const wxDataViewItem& item, const wxIcon &icon );
    const wxIcon &GetItemExpandedIcon( const wxDataViewItem& item ) const
        { return GetStore()->GetItemExpandedIcon(item); }
    void SetItemData( const wxDataViewItem& item, wxClientData *data )
        { GetStore()->SetItemData(item,data); }
    wxClientData *GetItemData( const wxDataViewItem& item ) const
        { return GetStore()->GetItemData(item); }

    void DeleteItem( const wxDataViewItem& item );
    void DeleteChildren( const wxDataViewItem& item );
    void DeleteAllItems();

    void OnExpanded( wxDataViewEvent &event );
    void OnCollapsed( wxDataViewEvent &event );
    void OnSize( wxSizeEvent &event );

private:
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxDataViewTreeCtrl)
};

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED           wxEVT_DATAVIEW_SELECTION_CHANGED
#define wxEVT_COMMAND_DATAVIEW_ITEM_ACTIVATED              wxEVT_DATAVIEW_ITEM_ACTIVATED
#define wxEVT_COMMAND_DATAVIEW_ITEM_COLLAPSED              wxEVT_DATAVIEW_ITEM_COLLAPSED
#define wxEVT_COMMAND_DATAVIEW_ITEM_EXPANDED               wxEVT_DATAVIEW_ITEM_EXPANDED
#define wxEVT_COMMAND_DATAVIEW_ITEM_COLLAPSING             wxEVT_DATAVIEW_ITEM_COLLAPSING
#define wxEVT_COMMAND_DATAVIEW_ITEM_EXPANDING              wxEVT_DATAVIEW_ITEM_EXPANDING
#define wxEVT_COMMAND_DATAVIEW_ITEM_START_EDITING          wxEVT_DATAVIEW_ITEM_START_EDITING
#define wxEVT_COMMAND_DATAVIEW_ITEM_EDITING_STARTED        wxEVT_DATAVIEW_ITEM_EDITING_STARTED
#define wxEVT_COMMAND_DATAVIEW_ITEM_EDITING_DONE           wxEVT_DATAVIEW_ITEM_EDITING_DONE
#define wxEVT_COMMAND_DATAVIEW_ITEM_VALUE_CHANGED          wxEVT_DATAVIEW_ITEM_VALUE_CHANGED
#define wxEVT_COMMAND_DATAVIEW_ITEM_CONTEXT_MENU           wxEVT_DATAVIEW_ITEM_CONTEXT_MENU
#define wxEVT_COMMAND_DATAVIEW_COLUMN_HEADER_CLICK         wxEVT_DATAVIEW_COLUMN_HEADER_CLICK
#define wxEVT_COMMAND_DATAVIEW_COLUMN_HEADER_RIGHT_CLICK   wxEVT_DATAVIEW_COLUMN_HEADER_RIGHT_CLICK
#define wxEVT_COMMAND_DATAVIEW_COLUMN_SORTED               wxEVT_DATAVIEW_COLUMN_SORTED
#define wxEVT_COMMAND_DATAVIEW_COLUMN_REORDERED            wxEVT_DATAVIEW_COLUMN_REORDERED
#define wxEVT_COMMAND_DATAVIEW_CACHE_HINT                  wxEVT_DATAVIEW_CACHE_HINT
#define wxEVT_COMMAND_DATAVIEW_ITEM_BEGIN_DRAG             wxEVT_DATAVIEW_ITEM_BEGIN_DRAG
#define wxEVT_COMMAND_DATAVIEW_ITEM_DROP_POSSIBLE          wxEVT_DATAVIEW_ITEM_DROP_POSSIBLE
#define wxEVT_COMMAND_DATAVIEW_ITEM_DROP                   wxEVT_DATAVIEW_ITEM_DROP

#endif // wxUSE_DATAVIEWCTRL

#endif
    // _WX_DATAVIEW_H_BASE_
