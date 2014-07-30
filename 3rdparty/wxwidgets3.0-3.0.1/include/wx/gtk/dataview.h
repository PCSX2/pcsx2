/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dataview.h
// Purpose:     wxDataViewCtrl GTK+2 implementation header
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKDATAVIEWCTRL_H_
#define _WX_GTKDATAVIEWCTRL_H_

#include "wx/list.h"

class WXDLLIMPEXP_FWD_ADV wxDataViewCtrlInternal;

struct _GtkTreePath;

// ---------------------------------------------------------
// wxDataViewColumn
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewColumn: public wxDataViewColumnBase
{
public:
    wxDataViewColumn( const wxString &title, wxDataViewRenderer *renderer,
                      unsigned int model_column, int width = wxDVC_DEFAULT_WIDTH,
                      wxAlignment align = wxALIGN_CENTER,
                      int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn( const wxBitmap &bitmap, wxDataViewRenderer *renderer,
                      unsigned int model_column, int width = wxDVC_DEFAULT_WIDTH,
                      wxAlignment align = wxALIGN_CENTER,
                      int flags = wxDATAVIEW_COL_RESIZABLE );


    // setters:

    virtual void SetTitle( const wxString &title );
    virtual void SetBitmap( const wxBitmap &bitmap );

    virtual void SetOwner( wxDataViewCtrl *owner );

    virtual void SetAlignment( wxAlignment align );

    virtual void SetSortable( bool sortable );
    virtual void SetSortOrder( bool ascending );

    virtual void SetResizeable( bool resizable );
    virtual void SetHidden( bool hidden );

    virtual void SetMinWidth( int minWidth );
    virtual void SetWidth( int width );

    virtual void SetReorderable( bool reorderable );

    virtual void SetFlags(int flags) { SetIndividualFlags(flags); }

    // getters:

    virtual wxString GetTitle() const;
    virtual wxAlignment GetAlignment() const;

    virtual bool IsSortable() const;
    virtual bool IsSortOrderAscending() const;
    virtual bool IsSortKey() const;

    virtual bool IsResizeable() const;
    virtual bool IsHidden() const;

    virtual int GetWidth() const;
    virtual int GetMinWidth() const;

    virtual bool IsReorderable() const;

    virtual int GetFlags() const { return GetFromIndividualFlags(); }

    // implementation
    GtkWidget* GetGtkHandle() const { return m_column; }

private:
    // holds the GTK handle
    GtkWidget   *m_column;

    // holds GTK handles for title/bitmap in the header
    GtkWidget   *m_image;
    GtkWidget   *m_label;

    // delayed connection to mouse events
    friend class wxDataViewCtrl;
    void OnInternalIdle();
    bool    m_isConnected;

    void Init(wxAlignment align, int flags, int width);
};

WX_DECLARE_LIST_WITH_DECL(wxDataViewColumn, wxDataViewColumnList,
                          class WXDLLIMPEXP_ADV);

// ---------------------------------------------------------
// wxDataViewCtrl
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewCtrl: public wxDataViewCtrlBase
{
public:
    wxDataViewCtrl()
    {
        Init();
    }

    wxDataViewCtrl( wxWindow *parent, wxWindowID id,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxDataViewCtrlNameStr )
    {
        Init();

        Create(parent, id, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxDataViewCtrlNameStr);

    virtual ~wxDataViewCtrl();

    virtual bool AssociateModel( wxDataViewModel *model );

    virtual bool PrependColumn( wxDataViewColumn *col );
    virtual bool AppendColumn( wxDataViewColumn *col );
    virtual bool InsertColumn( unsigned int pos, wxDataViewColumn *col );

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

    virtual void EnsureVisible( const wxDataViewItem& item,
                                const wxDataViewColumn *column = NULL );
    virtual void HitTest( const wxPoint &point,
                          wxDataViewItem &item,
                          wxDataViewColumn *&column ) const;
    virtual wxRect GetItemRect( const wxDataViewItem &item,
                                const wxDataViewColumn *column = NULL ) const;

    virtual bool SetRowHeight( int rowHeight );

    virtual void EditItem(const wxDataViewItem& item, const wxDataViewColumn *column);

    virtual void Expand( const wxDataViewItem & item );
    virtual void Collapse( const wxDataViewItem & item );
    virtual bool IsExpanded( const wxDataViewItem & item ) const;

    virtual bool EnableDragSource( const wxDataFormat &format );
    virtual bool EnableDropTarget( const wxDataFormat &format );

    virtual wxDataViewColumn *GetCurrentColumn() const;

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    wxWindow *GetMainWindow() { return (wxWindow*) this; }

    GtkWidget *GtkGetTreeView() { return m_treeview; }
    wxDataViewCtrlInternal* GtkGetInternal() { return m_internal; }

    // Convert GTK path to our item. Returned item may be invalid if get_iter()
    // failed.
    wxDataViewItem GTKPathToItem(struct _GtkTreePath *path) const;

    virtual void OnInternalIdle();

    int GTKGetUniformRowHeight() const { return m_uniformRowHeight; }

protected:
    virtual void DoSetExpanderColumn();
    virtual void DoSetIndent();

    virtual void DoApplyWidgetStyle(GtkRcStyle *style);

private:
    void Init();

    virtual wxDataViewItem DoGetCurrentItem() const;
    virtual void DoSetCurrentItem(const wxDataViewItem& item);

    // Return wxDataViewColumn matching the given GtkTreeViewColumn.
    //
    // If the input argument is NULL, return NULL too. Otherwise we must find
    // the matching column and assert if we didn't.
    wxDataViewColumn* FromGTKColumn(GtkTreeViewColumn *gtk_col) const;

    friend class wxDataViewCtrlDCImpl;
    friend class wxDataViewColumn;
    friend class wxDataViewCtrlInternal;

    GtkWidget               *m_treeview;
    wxDataViewCtrlInternal  *m_internal;
    wxDataViewColumnList     m_cols;
    wxDataViewItem           m_ensureVisibleDefered;

    // By default this is set to -1 and the height of the rows is determined by
    // GetRect() methods of the renderers but this can be set to a positive
    // value to force the height of all rows to the given value.
    int m_uniformRowHeight;

    virtual void AddChildGTK(wxWindowGTK* child);
    void GtkEnableSelectionEvents();
    void GtkDisableSelectionEvents();

    DECLARE_DYNAMIC_CLASS(wxDataViewCtrl)
    wxDECLARE_NO_COPY_CLASS(wxDataViewCtrl);
};


#endif // _WX_GTKDATAVIEWCTRL_H_
