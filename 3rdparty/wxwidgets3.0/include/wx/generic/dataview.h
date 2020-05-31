/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/dataview.h
// Purpose:     wxDataViewCtrl generic implementation header
// Author:      Robert Roebling
// Modified By: Bo Yang
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GENERICDATAVIEWCTRLH__
#define __GENERICDATAVIEWCTRLH__

#include "wx/defs.h"
#include "wx/object.h"
#include "wx/list.h"
#include "wx/control.h"
#include "wx/scrolwin.h"
#include "wx/icon.h"
#include "wx/vector.h"

class WXDLLIMPEXP_FWD_ADV wxDataViewMainWindow;
class WXDLLIMPEXP_FWD_ADV wxDataViewHeaderWindow;

// ---------------------------------------------------------
// wxDataViewColumn
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewColumn : public wxDataViewColumnBase
{
public:
    wxDataViewColumn(const wxString& title,
                     wxDataViewRenderer *renderer,
                     unsigned int model_column,
                     int width = wxDVC_DEFAULT_WIDTH,
                     wxAlignment align = wxALIGN_CENTER,
                     int flags = wxDATAVIEW_COL_RESIZABLE)
        : wxDataViewColumnBase(renderer, model_column),
          m_title(title)
    {
        Init(width, align, flags);
    }

    wxDataViewColumn(const wxBitmap& bitmap,
                     wxDataViewRenderer *renderer,
                     unsigned int model_column,
                     int width = wxDVC_DEFAULT_WIDTH,
                     wxAlignment align = wxALIGN_CENTER,
                     int flags = wxDATAVIEW_COL_RESIZABLE)
        : wxDataViewColumnBase(bitmap, renderer, model_column)
    {
        Init(width, align, flags);
    }

    // implement wxHeaderColumnBase methods
    virtual void SetTitle(const wxString& title) { m_title = title; UpdateDisplay(); }
    virtual wxString GetTitle() const { return m_title; }

    virtual void SetWidth(int width) { m_width = width; UpdateDisplay(); }
    virtual int GetWidth() const;

    virtual void SetMinWidth(int minWidth) { m_minWidth = minWidth; UpdateDisplay(); }
    virtual int GetMinWidth() const { return m_minWidth; }

    virtual void SetAlignment(wxAlignment align) { m_align = align; UpdateDisplay(); }
    virtual wxAlignment GetAlignment() const { return m_align; }

    virtual void SetFlags(int flags) { m_flags = flags; UpdateDisplay(); }
    virtual int GetFlags() const { return m_flags; }

    virtual bool IsSortKey() const { return m_sort; }

    virtual void UnsetAsSortKey();

    virtual void SetSortOrder(bool ascending);

    virtual bool IsSortOrderAscending() const { return m_sortAscending; }

    virtual void SetBitmap( const wxBitmap& bitmap ) { wxDataViewColumnBase::SetBitmap(bitmap); UpdateDisplay(); }


private:
    // common part of all ctors
    void Init(int width, wxAlignment align, int flags);

    void UpdateDisplay();

    wxString m_title;
    int m_width,
        m_minWidth;
    wxAlignment m_align;
    int m_flags;
    bool m_sort,
         m_sortAscending;

    friend class wxDataViewHeaderWindowBase;
    friend class wxDataViewHeaderWindow;
    friend class wxDataViewHeaderWindowMSW;
};

// ---------------------------------------------------------
// wxDataViewCtrl
// ---------------------------------------------------------

WX_DECLARE_LIST_WITH_DECL(wxDataViewColumn, wxDataViewColumnList,
                          class WXDLLIMPEXP_ADV);

class WXDLLIMPEXP_ADV wxDataViewCtrl : public wxDataViewCtrlBase,
                                       public wxScrollHelper
{
    friend class wxDataViewMainWindow;
    friend class wxDataViewHeaderWindowBase;
    friend class wxDataViewHeaderWindow;
    friend class wxDataViewHeaderWindowMSW;
    friend class wxDataViewColumn;

public:
    wxDataViewCtrl() : wxScrollHelper(this)
    {
        Init();
    }

    wxDataViewCtrl( wxWindow *parent, wxWindowID id,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxDataViewCtrlNameStr )
             : wxScrollHelper(this)
    {
        Create(parent, id, pos, size, style, validator, name);
    }

    virtual ~wxDataViewCtrl();

    void Init();

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize, long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxDataViewCtrlNameStr);

    virtual bool AssociateModel( wxDataViewModel *model );

    virtual bool AppendColumn( wxDataViewColumn *col );
    virtual bool PrependColumn( wxDataViewColumn *col );
    virtual bool InsertColumn( unsigned int pos, wxDataViewColumn *col );

    virtual void DoSetExpanderColumn();
    virtual void DoSetIndent();

    virtual unsigned int GetColumnCount() const;
    virtual wxDataViewColumn* GetColumn( unsigned int pos ) const;
    virtual bool DeleteColumn( wxDataViewColumn *column );
    virtual bool ClearColumns();
    virtual int GetColumnPosition( const wxDataViewColumn *column ) const;

    virtual wxDataViewColumn *GetSortingColumn() const;

    virtual int GetSelectedItemsCount() const;
    virtual int GetSelections( wxDataViewItemArray & sel ) const;
    virtual void SetSelections( const wxDataViewItemArray & sel );
    virtual void Select( const wxDataViewItem & item );
    virtual void Unselect( const wxDataViewItem & item );
    virtual bool IsSelected( const wxDataViewItem & item ) const;

    virtual void SelectAll();
    virtual void UnselectAll();

    virtual void EnsureVisible( const wxDataViewItem & item,
                                const wxDataViewColumn *column = NULL );
    virtual void HitTest( const wxPoint & point, wxDataViewItem & item,
                          wxDataViewColumn* &column ) const;
    virtual wxRect GetItemRect( const wxDataViewItem & item,
                                const wxDataViewColumn *column = NULL ) const;

    virtual bool SetRowHeight( int rowHeight );

    virtual void Expand( const wxDataViewItem & item );
    virtual void Collapse( const wxDataViewItem & item );
    virtual bool IsExpanded( const wxDataViewItem & item ) const;

    virtual void SetFocus();

    virtual bool SetFont(const wxFont & font);

#if wxUSE_DRAG_AND_DROP
    virtual bool EnableDragSource( const wxDataFormat &format );
    virtual bool EnableDropTarget( const wxDataFormat &format );
#endif // wxUSE_DRAG_AND_DROP

    virtual wxBorder GetDefaultBorder() const;

    virtual void EditItem(const wxDataViewItem& item, const wxDataViewColumn *column);

    // These methods are specific to generic wxDataViewCtrl implementation and
    // should not be used in portable code.
    wxColour GetAlternateRowColour() const { return m_alternateRowColour; }
    void SetAlternateRowColour(const wxColour& colour);

protected:
    virtual void EnsureVisible( int row, int column );

    // Notice that row here may be invalid (i.e. >= GetRowCount()), this is not
    // an error and this function simply returns an invalid item in this case.
    virtual wxDataViewItem GetItemByRow( unsigned int row ) const;
    virtual int GetRowByItem( const wxDataViewItem & item ) const;

    int GetSortingColumnIndex() const { return m_sortingColumnIdx; }
    void SetSortingColumnIndex(int idx) { m_sortingColumnIdx = idx; }

public:     // utility functions not part of the API

    // returns the "best" width for the idx-th column
    unsigned int GetBestColumnWidth(int idx) const;

    // called by header window after reorder
    void ColumnMoved( wxDataViewColumn* col, unsigned int new_pos );

    // update the display after a change to an individual column
    void OnColumnChange(unsigned int idx);

    // update after a change to the number of columns
    void OnColumnsCountChanged();

    wxWindow *GetMainWindow() { return (wxWindow*) m_clientArea; }

    // return the index of the given column in m_cols
    int GetColumnIndex(const wxDataViewColumn *column) const;

    // Return the index of the column having the given model index.
    int GetModelColumnIndex(unsigned int model_column) const;

    // return the column displayed at the given position in the control
    wxDataViewColumn *GetColumnAt(unsigned int pos) const;

    virtual wxDataViewColumn *GetCurrentColumn() const;

    virtual void OnInternalIdle();

private:
    virtual wxDataViewItem DoGetCurrentItem() const;
    virtual void DoSetCurrentItem(const wxDataViewItem& item);

    void InvalidateColBestWidths();
    void InvalidateColBestWidth(int idx);
    void UpdateColWidths();

    wxDataViewColumnList      m_cols;
    // cached column best widths information, values are for
    // respective columns from m_cols and the arrays have same size
    struct CachedColWidthInfo
    {
        CachedColWidthInfo() : width(0), dirty(true) {}
        int width;  // cached width or 0 if not computed
        bool dirty; // column was invalidated, header needs updating
    };
    wxVector<CachedColWidthInfo> m_colsBestWidths;
    // This indicates that at least one entry in m_colsBestWidths has 'dirty'
    // flag set. It's cheaper to check one flag in OnInternalIdle() than to
    // iterate over m_colsBestWidths to check if anything needs to be done.
    bool                      m_colsDirty;

    wxDataViewModelNotifier  *m_notifier;
    wxDataViewMainWindow     *m_clientArea;
    wxDataViewHeaderWindow   *m_headerArea;

    // user defined color to draw row lines, may be invalid
    wxColour m_alternateRowColour;

    // the index of the column currently used for sorting or -1
    int m_sortingColumnIdx;

private:
    void OnSize( wxSizeEvent &event );
    virtual wxSize GetSizeAvailableForScrollTarget(const wxSize& size);

    // we need to return a special WM_GETDLGCODE value to process just the
    // arrows but let the other navigation characters through
#ifdef __WXMSW__
    virtual WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
#endif // __WXMSW__

    WX_FORWARD_TO_SCROLL_HELPER()

private:
    DECLARE_DYNAMIC_CLASS(wxDataViewCtrl)
    wxDECLARE_NO_COPY_CLASS(wxDataViewCtrl);
    DECLARE_EVENT_TABLE()
};


#endif // __GENERICDATAVIEWCTRLH__
