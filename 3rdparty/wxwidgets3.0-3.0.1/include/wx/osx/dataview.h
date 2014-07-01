/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/dataview.h
// Purpose:     wxDataViewCtrl native implementation header for OSX
// Author:
// Copyright:   (c) 2009
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DATAVIEWCTRL_OSX_H_
#define _WX_DATAVIEWCTRL_OSX_H_

#ifdef __WXMAC_CLASSIC__
#  error "Native wxDataViewCtrl for classic environment not defined. Please use generic control."
#endif

// --------------------------------------------------------
// Class declarations to mask native types
// --------------------------------------------------------
class wxDataViewColumnNativeData;   // class storing environment dependent data for the native implementation
class wxDataViewWidgetImpl;         // class used as a common interface for carbon and cocoa implementation

// ---------------------------------------------------------
// wxDataViewColumn
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewColumn: public wxDataViewColumnBase
{
public:
    // constructors / destructor
    wxDataViewColumn(const wxString& title,
                     wxDataViewRenderer* renderer,
                     unsigned int model_column,
                     int width = wxDVC_DEFAULT_WIDTH,
                     wxAlignment align = wxALIGN_CENTER,
                     int flags = wxDATAVIEW_COL_RESIZABLE);
    wxDataViewColumn(const wxBitmap& bitmap,
                     wxDataViewRenderer* renderer,
                     unsigned int model_column,
                     int width = wxDVC_DEFAULT_WIDTH,
                     wxAlignment align = wxALIGN_CENTER,
                     int flags = wxDATAVIEW_COL_RESIZABLE);
    virtual ~wxDataViewColumn();

    // implement wxHeaderColumnBase pure virtual methods
    virtual wxAlignment GetAlignment() const { return m_alignment; }
    virtual int GetFlags() const { return m_flags; }
    virtual int GetMaxWidth() const { return m_maxWidth; }
    virtual int GetMinWidth() const { return m_minWidth; }
    virtual wxString GetTitle() const { return m_title; }
    virtual int GetWidth() const;
    virtual bool IsSortOrderAscending() const { return m_ascending; }
    virtual bool IsSortKey() const;
    virtual bool IsHidden() const;

    virtual void SetAlignment  (wxAlignment align);
    virtual void SetBitmap     (wxBitmap const& bitmap);
    virtual void SetFlags      (int flags) { m_flags = flags; /*SetIndividualFlags(flags); */ }
    virtual void SetHidden     (bool hidden);
    virtual void SetMaxWidth   (int maxWidth);
    virtual void SetMinWidth   (int minWidth);
    virtual void SetReorderable(bool reorderable);
    virtual void SetResizeable (bool resizable);
    virtual void SetSortable   (bool sortable);
    virtual void SetSortOrder  (bool ascending);
    virtual void SetTitle      (wxString const& title);
    virtual void SetWidth      (int  width);

   // implementation only
    wxDataViewColumnNativeData* GetNativeData() const
    {
      return m_NativeDataPtr;
    }

    void SetNativeData(wxDataViewColumnNativeData* newNativeDataPtr); // class takes ownership of pointer
    int GetWidthVariable() const
    {
        return m_width;
    }
    void SetWidthVariable(int NewWidth)
    {
        m_width = NewWidth;
    }
    void SetSortOrderVariable(bool NewOrder)
    {
        m_ascending = NewOrder;
    }

private:
    // common part of all ctors
    void InitCommon(int width, wxAlignment align, int flags)
    {
        m_ascending = true;
        m_flags = flags & ~wxDATAVIEW_COL_HIDDEN; // TODO
        m_maxWidth = 30000;
        m_minWidth = 0;
        m_alignment = align;
        SetWidth(width);
    }

    bool m_ascending; // sorting order

    int m_flags;    // flags for the column
    int m_maxWidth; // maximum width for the column
    int m_minWidth; // minimum width for the column
    int m_width;    // column width

    wxAlignment m_alignment; // column header alignment

    wxDataViewColumnNativeData* m_NativeDataPtr; // storing environment dependent data for the native implementation

    wxString m_title; // column title
};

//
// type definitions related to wxDataViewColumn
//
WX_DEFINE_ARRAY(wxDataViewColumn*,wxDataViewColumnPtrArrayType);

// ---------------------------------------------------------
// wxDataViewCtrl
// ---------------------------------------------------------
class WXDLLIMPEXP_ADV wxDataViewCtrl: public wxDataViewCtrlBase
{
public:
 // Constructors / destructor:
  wxDataViewCtrl()
  {
    Init();
  }
  wxDataViewCtrl(wxWindow *parent,
                 wxWindowID winid,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = 0,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString& name = wxDataViewCtrlNameStr )
  {
    Init();
    Create(parent, winid, pos, size, style, validator, name);
  }

  ~wxDataViewCtrl();

  bool Create(wxWindow *parent,
              wxWindowID winid,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0,
              const wxValidator& validator = wxDefaultValidator,
              const wxString& name = wxDataViewCtrlNameStr);

  virtual wxWindow* GetMainWindow() // not used for the native implementation
  {
    return this;
  }

 // inherited methods from wxDataViewCtrlBase:
  virtual bool AssociateModel(wxDataViewModel* model);

  virtual bool              AppendColumn     (wxDataViewColumn* columnPtr);
  virtual bool              ClearColumns     ();
  virtual bool              DeleteColumn     (wxDataViewColumn* columnPtr);
  virtual wxDataViewColumn* GetColumn        (unsigned int pos) const;
  virtual unsigned int      GetColumnCount   () const;
  virtual int               GetColumnPosition(const wxDataViewColumn* columnPtr) const;
  virtual wxDataViewColumn* GetSortingColumn () const;
  virtual bool              InsertColumn     (unsigned int pos, wxDataViewColumn *col);
  virtual bool              PrependColumn    (wxDataViewColumn* columnPtr);

  virtual void Collapse( const wxDataViewItem& item);
  virtual void EnsureVisible(const wxDataViewItem& item, const wxDataViewColumn* columnPtr=NULL);
  virtual void Expand(const wxDataViewItem& item);
  virtual bool IsExpanded(const wxDataViewItem & item) const;

  virtual unsigned int GetCount() const;
  virtual wxRect GetItemRect(const wxDataViewItem& item,
                             const wxDataViewColumn* columnPtr = NULL) const;
  virtual int GetSelectedItemsCount() const;
  virtual int GetSelections(wxDataViewItemArray& sel) const;

  virtual void HitTest(const wxPoint& point, wxDataViewItem& item, wxDataViewColumn*& columnPtr) const;

  virtual bool IsSelected(const wxDataViewItem& item) const;

  virtual void SelectAll();
  virtual void Select(const wxDataViewItem& item);
  virtual void SetSelections(const wxDataViewItemArray& sel);

  virtual void Unselect(const wxDataViewItem& item);
  virtual void UnselectAll();

//
// implementation
//
 // returns a pointer to the native implementation
  wxDataViewWidgetImpl* GetDataViewPeer() const;

 // adds all children of the passed parent to the control; if 'parentItem' is invalid the root(s) is/are added:
  void AddChildren(wxDataViewItem const& parentItem);

 // finishes editing of custom items; if no custom item is currently edited the method does nothing
  void FinishCustomItemEditing();
  
  virtual void EditItem(const wxDataViewItem& item, const wxDataViewColumn *column);

 // returns the n-th pointer to a column;
 // this method is different from GetColumn(unsigned int pos) because here 'n' is not a position in the control but the n-th
 // position in the internal list/array of column pointers
  wxDataViewColumn* GetColumnPtr(size_t n) const
  {
    return m_ColumnPtrs[n];
  }
 // returns the current being rendered item of the customized renderer (this item is only valid during editing)
  wxDataViewItem const& GetCustomRendererItem() const
  {
    return m_CustomRendererItem;
  }
 // returns a pointer to a customized renderer (this pointer is only valid during editing)
  wxDataViewCustomRenderer* GetCustomRendererPtr() const
  {
    return m_CustomRendererPtr;
  }

 // checks if currently a delete process is running
  bool IsDeleting() const
  {
    return m_Deleting;
  }

 // with CG, we need to get the context from an kEventControlDraw event
 // unfortunately, the DataBrowser callbacks don't provide the context
 // and we need it, so we need to set/remove it before and after draw
 // events so we can access it in the callbacks.
  void MacSetDrawingContext(void* context)
  {
    m_cgContext = context;
  }
  void* MacGetDrawingContext() const
  {
    return m_cgContext;
  }

 // sets the currently being edited item of the custom renderer
  void SetCustomRendererItem(wxDataViewItem const& NewItem)
  {
    m_CustomRendererItem = NewItem;
  }
 // sets the custom renderer
  void SetCustomRendererPtr(wxDataViewCustomRenderer* NewCustomRendererPtr)
  {
    m_CustomRendererPtr = NewCustomRendererPtr;
  }
 // sets the flag indicating a deletion process:
  void SetDeleting(bool deleting)
  {
    m_Deleting = deleting;
  }

  virtual wxDataViewColumn *GetCurrentColumn() const;

  virtual wxVisualAttributes GetDefaultAttributes() const
  {
      return GetClassDefaultAttributes(GetWindowVariant());
  }

  static wxVisualAttributes
  GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

protected:
 // inherited methods from wxDataViewCtrlBase
  virtual void DoSetExpanderColumn();
  virtual void DoSetIndent();

  virtual wxSize DoGetBestSize() const;

 // event handling
  void OnSize(wxSizeEvent &event);
  void OnMouse(wxMouseEvent &event);

private:
 // initializing of local variables:
  void Init();

  virtual wxDataViewItem DoGetCurrentItem() const;
  virtual void DoSetCurrentItem(const wxDataViewItem& item);

 //
 // variables
 //
  bool m_Deleting; // flag indicating if a delete process is running; this flag is necessary because the notifier indicating an item deletion in the model may be called
                   // after the actual deletion of the item; then, native callback functions/delegates may try to update data of variables that are already deleted;
                   // if this flag is set all native variable update requests will be ignored

  void* m_cgContext; // pointer to core graphics context

  wxDataViewCustomRenderer* m_CustomRendererPtr; // pointer to a valid custom renderer while editing; this class does NOT own the pointer

  wxDataViewItem m_CustomRendererItem; // currently edited item by the customrenderer; it is invalid while not editing a custom item

  wxDataViewColumnPtrArrayType m_ColumnPtrs; // all column pointers are stored in an array

  wxDataViewModelNotifier* m_ModelNotifier; // stores the model notifier for the control (does not own the notifier)

 // wxWidget internal stuff:
  DECLARE_DYNAMIC_CLASS(wxDataViewCtrl)
  DECLARE_NO_COPY_CLASS(wxDataViewCtrl)
  DECLARE_EVENT_TABLE()
};

#endif // _WX_DATAVIEWCTRL_OSX_H_

