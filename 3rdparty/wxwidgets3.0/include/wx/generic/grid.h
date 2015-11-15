/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/grid.h
// Purpose:     wxGrid and related classes
// Author:      Michael Bedward (based on code by Julian Smart, Robin Dunn)
// Modified by: Santiago Palacios
// Created:     1/08/1999
// Copyright:   (c) Michael Bedward
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_GRID_H_
#define _WX_GENERIC_GRID_H_

#include "wx/defs.h"

#if wxUSE_GRID

#include "wx/hashmap.h"

#include "wx/scrolwin.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

extern WXDLLIMPEXP_DATA_ADV(const char) wxGridNameStr[];

// Default parameters for wxGrid
//
#define WXGRID_DEFAULT_NUMBER_ROWS            10
#define WXGRID_DEFAULT_NUMBER_COLS            10
#if defined(__WXMSW__) || defined(__WXGTK20__)
#define WXGRID_DEFAULT_ROW_HEIGHT             25
#else
#define WXGRID_DEFAULT_ROW_HEIGHT             30
#endif  // __WXMSW__
#define WXGRID_DEFAULT_COL_WIDTH              80
#define WXGRID_DEFAULT_COL_LABEL_HEIGHT       32
#define WXGRID_DEFAULT_ROW_LABEL_WIDTH        82
#define WXGRID_LABEL_EDGE_ZONE                 2
#define WXGRID_MIN_ROW_HEIGHT                 15
#define WXGRID_MIN_COL_WIDTH                  15
#define WXGRID_DEFAULT_SCROLLBAR_WIDTH        16

// type names for grid table values
#define wxGRID_VALUE_STRING     wxT("string")
#define wxGRID_VALUE_BOOL       wxT("bool")
#define wxGRID_VALUE_NUMBER     wxT("long")
#define wxGRID_VALUE_FLOAT      wxT("double")
#define wxGRID_VALUE_CHOICE     wxT("choice")

#define wxGRID_VALUE_TEXT wxGRID_VALUE_STRING
#define wxGRID_VALUE_LONG wxGRID_VALUE_NUMBER

// magic constant which tells (to some functions) to automatically calculate
// the appropriate size
#define wxGRID_AUTOSIZE (-1)

// many wxGrid methods work either with columns or rows, this enum is used for
// the parameter indicating which one should it be
enum wxGridDirection
{
    wxGRID_COLUMN,
    wxGRID_ROW
};

// Flags used with wxGrid::Render() to select parts of the grid to draw.
enum wxGridRenderStyle
{
    wxGRID_DRAW_ROWS_HEADER = 0x001,
    wxGRID_DRAW_COLS_HEADER = 0x002,
    wxGRID_DRAW_CELL_LINES = 0x004,
    wxGRID_DRAW_BOX_RECT = 0x008,
    wxGRID_DRAW_SELECTION = 0x010,
    wxGRID_DRAW_DEFAULT = wxGRID_DRAW_ROWS_HEADER |
                          wxGRID_DRAW_COLS_HEADER |
                          wxGRID_DRAW_CELL_LINES |
                          wxGRID_DRAW_BOX_RECT
};

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_ADV wxGrid;
class WXDLLIMPEXP_FWD_ADV wxGridCellAttr;
class WXDLLIMPEXP_FWD_ADV wxGridCellAttrProviderData;
class WXDLLIMPEXP_FWD_ADV wxGridColLabelWindow;
class WXDLLIMPEXP_FWD_ADV wxGridCornerLabelWindow;
class WXDLLIMPEXP_FWD_ADV wxGridRowLabelWindow;
class WXDLLIMPEXP_FWD_ADV wxGridWindow;
class WXDLLIMPEXP_FWD_ADV wxGridTypeRegistry;
class WXDLLIMPEXP_FWD_ADV wxGridSelection;

class WXDLLIMPEXP_FWD_CORE wxHeaderCtrl;
class WXDLLIMPEXP_FWD_CORE wxCheckBox;
class WXDLLIMPEXP_FWD_CORE wxComboBox;
class WXDLLIMPEXP_FWD_CORE wxTextCtrl;
#if wxUSE_SPINCTRL
class WXDLLIMPEXP_FWD_CORE wxSpinCtrl;
#endif

class wxGridFixedIndicesSet;

class wxGridOperations;
class wxGridRowOperations;
class wxGridColumnOperations;
class wxGridDirectionOperations;


// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

#define wxSafeIncRef(p) if ( p ) (p)->IncRef()
#define wxSafeDecRef(p) if ( p ) (p)->DecRef()

// ----------------------------------------------------------------------------
// wxGridCellWorker: common base class for wxGridCellRenderer and
// wxGridCellEditor
//
// NB: this is more an implementation convenience than a design issue, so this
//     class is not documented and is not public at all
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGridCellWorker : public wxClientDataContainer, public wxRefCounter
{
public:
    wxGridCellWorker() { }

    // interpret renderer parameters: arbitrary string whose interpretation is
    // left to the derived classes
    virtual void SetParameters(const wxString& params);

protected:
    // virtual dtor for any base class - private because only DecRef() can
    // delete us
    virtual ~wxGridCellWorker();

private:
    // suppress the stupid gcc warning about the class having private dtor and
    // no friends
    friend class wxGridCellWorkerDummyFriend;
};

// ----------------------------------------------------------------------------
// wxGridCellRenderer: this class is responsible for actually drawing the cell
// in the grid. You may pass it to the wxGridCellAttr (below) to change the
// format of one given cell or to wxGrid::SetDefaultRenderer() to change the
// view of all cells. This is an ABC, you will normally use one of the
// predefined derived classes or derive your own class from it.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGridCellRenderer : public wxGridCellWorker
{
public:
    // draw the given cell on the provided DC inside the given rectangle
    // using the style specified by the attribute and the default or selected
    // state corresponding to the isSelected value.
    //
    // this pure virtual function has a default implementation which will
    // prepare the DC using the given attribute: it will draw the rectangle
    // with the bg colour from attr and set the text colour and font
    virtual void Draw(wxGrid& grid,
                      wxGridCellAttr& attr,
                      wxDC& dc,
                      const wxRect& rect,
                      int row, int col,
                      bool isSelected) = 0;

    // get the preferred size of the cell for its contents
    virtual wxSize GetBestSize(wxGrid& grid,
                               wxGridCellAttr& attr,
                               wxDC& dc,
                               int row, int col) = 0;

    // create a new object which is the copy of this one
    virtual wxGridCellRenderer *Clone() const = 0;
};

// ----------------------------------------------------------------------------
// wxGridCellEditor:  This class is responsible for providing and manipulating
// the in-place edit controls for the grid.  Instances of wxGridCellEditor
// (actually, instances of derived classes since it is an ABC) can be
// associated with the cell attributes for individual cells, rows, columns, or
// even for the entire grid.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGridCellEditor : public wxGridCellWorker
{
public:
    wxGridCellEditor();

    bool IsCreated() const { return m_control != NULL; }
    wxControl* GetControl() const { return m_control; }
    void SetControl(wxControl* control) { m_control = control; }

    wxGridCellAttr* GetCellAttr() const { return m_attr; }
    void SetCellAttr(wxGridCellAttr* attr) { m_attr = attr; }

    // Creates the actual edit control
    virtual void Create(wxWindow* parent,
                        wxWindowID id,
                        wxEvtHandler* evtHandler) = 0;

    // Size and position the edit control
    virtual void SetSize(const wxRect& rect);

    // Show or hide the edit control, use the specified attributes to set
    // colours/fonts for it
    virtual void Show(bool show, wxGridCellAttr *attr = NULL);

    // Draws the part of the cell not occupied by the control: the base class
    // version just fills it with background colour from the attribute
    virtual void PaintBackground(wxDC& dc,
                                 const wxRect& rectCell,
                                 const wxGridCellAttr& attr);


    // The methods called by wxGrid when a cell is edited: first BeginEdit() is
    // called, then EndEdit() is and if it returns true and if the change is
    // not vetoed by a user-defined event handler, finally ApplyEdit() is called

    // Fetch the value from the table and prepare the edit control
    // to begin editing.  Set the focus to the edit control.
    virtual void BeginEdit(int row, int col, wxGrid* grid) = 0;

    // Returns false if nothing changed, otherwise returns true and return the
    // new value in its string form in the newval output parameter.
    //
    // This should also store the new value in its real type internally so that
    // it could be used by ApplyEdit() but it must not modify the grid as the
    // change could still be vetoed.
    virtual bool EndEdit(int row, int col, const wxGrid *grid,
                         const wxString& oldval, wxString *newval) = 0;

    // Complete the editing of the current cell by storing the value saved by
    // the previous call to EndEdit() in the grid
    virtual void ApplyEdit(int row, int col, wxGrid* grid) = 0;


    // Reset the value in the control back to its starting value
    virtual void Reset() = 0;

    // return true to allow the given key to start editing: the base class
    // version only checks that the event has no modifiers. The derived
    // classes are supposed to do "if ( base::IsAcceptedKey() && ... )" in
    // their IsAcceptedKey() implementation, although, of course, it is not a
    // mandatory requirment.
    //
    // NB: if the key is F2 (special), editing will always start and this
    //     method will not be called at all (but StartingKey() will)
    virtual bool IsAcceptedKey(wxKeyEvent& event);

    // If the editor is enabled by pressing keys on the grid, this will be
    // called to let the editor do something about that first key if desired
    virtual void StartingKey(wxKeyEvent& event);

    // if the editor is enabled by clicking on the cell, this method will be
    // called
    virtual void StartingClick();

    // Some types of controls on some platforms may need some help
    // with the Return key.
    virtual void HandleReturn(wxKeyEvent& event);

    // Final cleanup
    virtual void Destroy();

    // create a new object which is the copy of this one
    virtual wxGridCellEditor *Clone() const = 0;

    // added GetValue so we can get the value which is in the control
    virtual wxString GetValue() const = 0;

protected:
    // the dtor is private because only DecRef() can delete us
    virtual ~wxGridCellEditor();

    // the control we show on screen
    wxControl*  m_control;

    // a temporary pointer to the attribute being edited
    wxGridCellAttr* m_attr;

    // if we change the colours/font of the control from the default ones, we
    // must restore the default later and we save them here between calls to
    // Show(true) and Show(false)
    wxColour m_colFgOld,
             m_colBgOld;
    wxFont m_fontOld;

    // suppress the stupid gcc warning about the class having private dtor and
    // no friends
    friend class wxGridCellEditorDummyFriend;

    wxDECLARE_NO_COPY_CLASS(wxGridCellEditor);
};

// ----------------------------------------------------------------------------
// wxGridHeaderRenderer and company: like wxGridCellRenderer but for headers
// ----------------------------------------------------------------------------

// Base class for corner window renderer: it is the simplest of all renderers
// and only has a single function
class WXDLLIMPEXP_ADV wxGridCornerHeaderRenderer
{
public:
    // Draw the border around the corner window.
    virtual void DrawBorder(const wxGrid& grid,
                            wxDC& dc,
                            wxRect& rect) const = 0;

    // make the dtor of a class with virtual functions virtual to avoid g++
    // warnings, even though this class is not supposed to be used
    // polymorphically
    virtual ~wxGridCornerHeaderRenderer() { }
};


// Base class for the row/column header cells renderers
class WXDLLIMPEXP_ADV wxGridHeaderLabelsRenderer
    : public wxGridCornerHeaderRenderer
{
public:
    // Draw header cell label
    virtual void DrawLabel(const wxGrid& grid,
                           wxDC& dc,
                           const wxString& value,
                           const wxRect& rect,
                           int horizAlign,
                           int vertAlign,
                           int textOrientation) const;
};

// Currently the row/column/corner renders don't need any methods other than
// those already in wxGridHeaderLabelsRenderer but still define separate classes
// for them for future extensions and also for better type safety (i.e. to
// avoid inadvertently using a column header renderer for the row headers)
class WXDLLIMPEXP_ADV wxGridRowHeaderRenderer
    : public wxGridHeaderLabelsRenderer
{
};

class WXDLLIMPEXP_ADV wxGridColumnHeaderRenderer
    : public wxGridHeaderLabelsRenderer
{
};

// Also define the default renderers which are used by wxGridCellAttrProvider
// by default
class WXDLLIMPEXP_ADV wxGridRowHeaderRendererDefault
    : public wxGridRowHeaderRenderer
{
public:
    virtual void DrawBorder(const wxGrid& grid,
                            wxDC& dc,
                            wxRect& rect) const;
};

// Column header cells renderers
class WXDLLIMPEXP_ADV wxGridColumnHeaderRendererDefault
    : public wxGridColumnHeaderRenderer
{
public:
    virtual void DrawBorder(const wxGrid& grid,
                            wxDC& dc,
                            wxRect& rect) const;
};

// Header corner renderer
class WXDLLIMPEXP_ADV wxGridCornerHeaderRendererDefault
    : public wxGridCornerHeaderRenderer
{
public:
    virtual void DrawBorder(const wxGrid& grid,
                            wxDC& dc,
                            wxRect& rect) const;
};


// ----------------------------------------------------------------------------
// wxGridCellAttr: this class can be used to alter the cells appearance in
// the grid by changing their colour/font/... from default. An object of this
// class may be returned by wxGridTable::GetAttr().
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGridCellAttr : public wxClientDataContainer, public wxRefCounter
{
public:
    enum wxAttrKind
    {
        Any,
        Default,
        Cell,
        Row,
        Col,
        Merged
    };

    // ctors
    wxGridCellAttr(wxGridCellAttr *attrDefault = NULL)
    {
        Init(attrDefault);

        SetAlignment(wxALIGN_INVALID, wxALIGN_INVALID);
    }

    // VZ: considering the number of members wxGridCellAttr has now, this ctor
    //     seems to be pretty useless... may be we should just remove it?
    wxGridCellAttr(const wxColour& colText,
                   const wxColour& colBack,
                   const wxFont& font,
                   int hAlign,
                   int vAlign)
        : m_colText(colText), m_colBack(colBack), m_font(font)
    {
        Init();
        SetAlignment(hAlign, vAlign);
    }

    // creates a new copy of this object
    wxGridCellAttr *Clone() const;
    void MergeWith(wxGridCellAttr *mergefrom);

    // setters
    void SetTextColour(const wxColour& colText) { m_colText = colText; }
    void SetBackgroundColour(const wxColour& colBack) { m_colBack = colBack; }
    void SetFont(const wxFont& font) { m_font = font; }
    void SetAlignment(int hAlign, int vAlign)
    {
        m_hAlign = hAlign;
        m_vAlign = vAlign;
    }
    void SetSize(int num_rows, int num_cols);
    void SetOverflow(bool allow = true)
        { m_overflow = allow ? Overflow : SingleCell; }
    void SetReadOnly(bool isReadOnly = true)
        { m_isReadOnly = isReadOnly ? ReadOnly : ReadWrite; }

    // takes ownership of the pointer
    void SetRenderer(wxGridCellRenderer *renderer)
        { wxSafeDecRef(m_renderer); m_renderer = renderer; }
    void SetEditor(wxGridCellEditor* editor)
        { wxSafeDecRef(m_editor); m_editor = editor; }

    void SetKind(wxAttrKind kind) { m_attrkind = kind; }

    // accessors
    bool HasTextColour() const { return m_colText.IsOk(); }
    bool HasBackgroundColour() const { return m_colBack.IsOk(); }
    bool HasFont() const { return m_font.IsOk(); }
    bool HasAlignment() const
    {
        return m_hAlign != wxALIGN_INVALID || m_vAlign != wxALIGN_INVALID;
    }
    bool HasRenderer() const { return m_renderer != NULL; }
    bool HasEditor() const { return m_editor != NULL; }
    bool HasReadWriteMode() const { return m_isReadOnly != Unset; }
    bool HasOverflowMode() const { return m_overflow != UnsetOverflow; }
    bool HasSize() const { return m_sizeRows != 1 || m_sizeCols != 1; }

    const wxColour& GetTextColour() const;
    const wxColour& GetBackgroundColour() const;
    const wxFont& GetFont() const;
    void GetAlignment(int *hAlign, int *vAlign) const;

    // unlike GetAlignment() which always overwrites its output arguments with
    // the alignment values to use, falling back on default alignment if this
    // attribute doesn't have any, this function will preserve the values of
    // parameters on entry if the corresponding alignment is not set in this
    // attribute meaning that they can be initialized to default alignment (and
    // also that they must be initialized, unlike with GetAlignment())
    void GetNonDefaultAlignment(int *hAlign, int *vAlign) const;

    void GetSize(int *num_rows, int *num_cols) const;
    bool GetOverflow() const
        { return m_overflow != SingleCell; }
    wxGridCellRenderer *GetRenderer(const wxGrid* grid, int row, int col) const;
    wxGridCellEditor *GetEditor(const wxGrid* grid, int row, int col) const;

    bool IsReadOnly() const { return m_isReadOnly == wxGridCellAttr::ReadOnly; }

    wxAttrKind GetKind() { return m_attrkind; }

    void SetDefAttr(wxGridCellAttr* defAttr) { m_defGridAttr = defAttr; }

protected:
    // the dtor is private because only DecRef() can delete us
    virtual ~wxGridCellAttr()
    {
        wxSafeDecRef(m_renderer);
        wxSafeDecRef(m_editor);
    }

private:
    enum wxAttrReadMode
    {
        Unset = -1,
        ReadWrite,
        ReadOnly
    };

    enum wxAttrOverflowMode
    {
        UnsetOverflow = -1,
        Overflow,
        SingleCell
    };

    // the common part of all ctors
    void Init(wxGridCellAttr *attrDefault = NULL);


    wxColour m_colText,
             m_colBack;
    wxFont   m_font;
    int      m_hAlign,
             m_vAlign;
    int      m_sizeRows,
             m_sizeCols;

    wxAttrOverflowMode  m_overflow;

    wxGridCellRenderer* m_renderer;
    wxGridCellEditor*   m_editor;
    wxGridCellAttr*     m_defGridAttr;

    wxAttrReadMode m_isReadOnly;

    wxAttrKind m_attrkind;

    // use Clone() instead
    wxDECLARE_NO_COPY_CLASS(wxGridCellAttr);

    // suppress the stupid gcc warning about the class having private dtor and
    // no friends
    friend class wxGridCellAttrDummyFriend;
};

// ----------------------------------------------------------------------------
// wxGridCellAttrProvider: class used by wxGridTableBase to retrieve/store the
// cell attributes.
// ----------------------------------------------------------------------------

// implementation note: we separate it from wxGridTableBase because we wish to
// avoid deriving a new table class if possible, and sometimes it will be
// enough to just derive another wxGridCellAttrProvider instead
//
// the default implementation is reasonably efficient for the generic case,
// but you might still wish to implement your own for some specific situations
// if you have performance problems with the stock one
class WXDLLIMPEXP_ADV wxGridCellAttrProvider : public wxClientDataContainer
{
public:
    wxGridCellAttrProvider();
    virtual ~wxGridCellAttrProvider();

    // DecRef() must be called on the returned pointer
    virtual wxGridCellAttr *GetAttr(int row, int col,
                                    wxGridCellAttr::wxAttrKind  kind ) const;

    // all these functions take ownership of the pointer, don't call DecRef()
    // on it
    virtual void SetAttr(wxGridCellAttr *attr, int row, int col);
    virtual void SetRowAttr(wxGridCellAttr *attr, int row);
    virtual void SetColAttr(wxGridCellAttr *attr, int col);

    // these functions must be called whenever some rows/cols are deleted
    // because the internal data must be updated then
    void UpdateAttrRows( size_t pos, int numRows );
    void UpdateAttrCols( size_t pos, int numCols );


    // get renderers for the given row/column header label and the corner
    // window: unlike cell renderers, these objects are not reference counted
    // and are never NULL so they are returned by reference
    virtual const wxGridColumnHeaderRenderer& GetColumnHeaderRenderer(int col);
    virtual const wxGridRowHeaderRenderer& GetRowHeaderRenderer(int row);
    virtual const wxGridCornerHeaderRenderer& GetCornerRenderer();

private:
    void InitData();

    wxGridCellAttrProviderData *m_data;

    wxDECLARE_NO_COPY_CLASS(wxGridCellAttrProvider);
};

// ----------------------------------------------------------------------------
// wxGridCellCoords: location of a cell in the grid
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGridCellCoords
{
public:
    wxGridCellCoords() { m_row = m_col = -1; }
    wxGridCellCoords( int r, int c ) { m_row = r; m_col = c; }

    // default copy ctor is ok

    int GetRow() const { return m_row; }
    void SetRow( int n ) { m_row = n; }
    int GetCol() const { return m_col; }
    void SetCol( int n ) { m_col = n; }
    void Set( int row, int col ) { m_row = row; m_col = col; }

    wxGridCellCoords& operator=( const wxGridCellCoords& other )
    {
        if ( &other != this )
        {
            m_row=other.m_row;
            m_col=other.m_col;
        }
        return *this;
    }

    bool operator==( const wxGridCellCoords& other ) const
    {
        return (m_row == other.m_row  &&  m_col == other.m_col);
    }

    bool operator!=( const wxGridCellCoords& other ) const
    {
        return (m_row != other.m_row  ||  m_col != other.m_col);
    }

    bool operator!() const
    {
        return (m_row == -1 && m_col == -1 );
    }

private:
    int m_row;
    int m_col;
};


// For comparisons...
//
extern WXDLLIMPEXP_ADV wxGridCellCoords wxGridNoCellCoords;
extern WXDLLIMPEXP_ADV wxRect           wxGridNoCellRect;

// An array of cell coords...
//
WX_DECLARE_OBJARRAY_WITH_DECL(wxGridCellCoords, wxGridCellCoordsArray,
                              class WXDLLIMPEXP_ADV);

// ----------------------------------------------------------------------------
// Grid table classes
// ----------------------------------------------------------------------------

// the abstract base class
class WXDLLIMPEXP_ADV wxGridTableBase : public wxObject,
                                        public wxClientDataContainer
{
public:
    wxGridTableBase();
    virtual ~wxGridTableBase();

    // You must override these functions in a derived table class
    //

    // return the number of rows and columns in this table
    virtual int GetNumberRows() = 0;
    virtual int GetNumberCols() = 0;

    // the methods above are unfortunately non-const even though they should
    // have been const -- but changing it now is not possible any longer as it
    // would break the existing code overriding them, so instead we provide
    // these const synonyms which can be used from const-correct code
    int GetRowsCount() const
        { return const_cast<wxGridTableBase *>(this)->GetNumberRows(); }
    int GetColsCount() const
        { return const_cast<wxGridTableBase *>(this)->GetNumberCols(); }


    virtual bool IsEmptyCell( int row, int col )
    {
        return GetValue(row, col).empty();
    }

    bool IsEmpty(const wxGridCellCoords& coord)
    {
        return IsEmptyCell(coord.GetRow(), coord.GetCol());
    }

    virtual wxString GetValue( int row, int col ) = 0;
    virtual void SetValue( int row, int col, const wxString& value ) = 0;

    // Data type determination and value access
    virtual wxString GetTypeName( int row, int col );
    virtual bool CanGetValueAs( int row, int col, const wxString& typeName );
    virtual bool CanSetValueAs( int row, int col, const wxString& typeName );

    virtual long GetValueAsLong( int row, int col );
    virtual double GetValueAsDouble( int row, int col );
    virtual bool GetValueAsBool( int row, int col );

    virtual void SetValueAsLong( int row, int col, long value );
    virtual void SetValueAsDouble( int row, int col, double value );
    virtual void SetValueAsBool( int row, int col, bool value );

    // For user defined types
    virtual void* GetValueAsCustom( int row, int col, const wxString& typeName );
    virtual void  SetValueAsCustom( int row, int col, const wxString& typeName, void* value );


    // Overriding these is optional
    //
    virtual void SetView( wxGrid *grid ) { m_view = grid; }
    virtual wxGrid * GetView() const { return m_view; }

    virtual void Clear() {}
    virtual bool InsertRows( size_t pos = 0, size_t numRows = 1 );
    virtual bool AppendRows( size_t numRows = 1 );
    virtual bool DeleteRows( size_t pos = 0, size_t numRows = 1 );
    virtual bool InsertCols( size_t pos = 0, size_t numCols = 1 );
    virtual bool AppendCols( size_t numCols = 1 );
    virtual bool DeleteCols( size_t pos = 0, size_t numCols = 1 );

    virtual wxString GetRowLabelValue( int row );
    virtual wxString GetColLabelValue( int col );
    virtual void SetRowLabelValue( int WXUNUSED(row), const wxString& ) {}
    virtual void SetColLabelValue( int WXUNUSED(col), const wxString& ) {}

    // Attribute handling
    //

    // give us the attr provider to use - we take ownership of the pointer
    void SetAttrProvider(wxGridCellAttrProvider *attrProvider);

    // get the currently used attr provider (may be NULL)
    wxGridCellAttrProvider *GetAttrProvider() const { return m_attrProvider; }

    // Does this table allow attributes?  Default implementation creates
    // a wxGridCellAttrProvider if necessary.
    virtual bool CanHaveAttributes();

    // by default forwarded to wxGridCellAttrProvider if any. May be
    // overridden to handle attributes directly in the table.
    virtual wxGridCellAttr *GetAttr( int row, int col,
                                     wxGridCellAttr::wxAttrKind  kind );


    // these functions take ownership of the pointer
    virtual void SetAttr(wxGridCellAttr* attr, int row, int col);
    virtual void SetRowAttr(wxGridCellAttr *attr, int row);
    virtual void SetColAttr(wxGridCellAttr *attr, int col);

private:
    wxGrid * m_view;
    wxGridCellAttrProvider *m_attrProvider;

    DECLARE_ABSTRACT_CLASS(wxGridTableBase)
    wxDECLARE_NO_COPY_CLASS(wxGridTableBase);
};


// ----------------------------------------------------------------------------
// wxGridTableMessage
// ----------------------------------------------------------------------------

// IDs for messages sent from grid table to view
//
enum wxGridTableRequest
{
    wxGRIDTABLE_REQUEST_VIEW_GET_VALUES = 2000,
    wxGRIDTABLE_REQUEST_VIEW_SEND_VALUES,
    wxGRIDTABLE_NOTIFY_ROWS_INSERTED,
    wxGRIDTABLE_NOTIFY_ROWS_APPENDED,
    wxGRIDTABLE_NOTIFY_ROWS_DELETED,
    wxGRIDTABLE_NOTIFY_COLS_INSERTED,
    wxGRIDTABLE_NOTIFY_COLS_APPENDED,
    wxGRIDTABLE_NOTIFY_COLS_DELETED
};

class WXDLLIMPEXP_ADV wxGridTableMessage
{
public:
    wxGridTableMessage();
    wxGridTableMessage( wxGridTableBase *table, int id,
                        int comInt1 = -1,
                        int comInt2 = -1 );

    void SetTableObject( wxGridTableBase *table ) { m_table = table; }
    wxGridTableBase * GetTableObject() const { return m_table; }
    void SetId( int id ) { m_id = id; }
    int  GetId() { return m_id; }
    void SetCommandInt( int comInt1 ) { m_comInt1 = comInt1; }
    int  GetCommandInt() { return m_comInt1; }
    void SetCommandInt2( int comInt2 ) { m_comInt2 = comInt2; }
    int  GetCommandInt2() { return m_comInt2; }

private:
    wxGridTableBase *m_table;
    int m_id;
    int m_comInt1;
    int m_comInt2;

    wxDECLARE_NO_COPY_CLASS(wxGridTableMessage);
};



// ------ wxGridStringArray
// A 2-dimensional array of strings for data values
//

WX_DECLARE_OBJARRAY_WITH_DECL(wxArrayString, wxGridStringArray,
                              class WXDLLIMPEXP_ADV);



// ------ wxGridStringTable
//
// Simplest type of data table for a grid for small tables of strings
// that are stored in memory
//

class WXDLLIMPEXP_ADV wxGridStringTable : public wxGridTableBase
{
public:
    wxGridStringTable();
    wxGridStringTable( int numRows, int numCols );

    // these are pure virtual in wxGridTableBase
    //
    virtual int GetNumberRows() { return static_cast<int>(m_data.size()); }
    virtual int GetNumberCols() { return m_numCols; }
    virtual wxString GetValue( int row, int col );
    virtual void SetValue( int row, int col, const wxString& s );

    // overridden functions from wxGridTableBase
    //
    void Clear();
    bool InsertRows( size_t pos = 0, size_t numRows = 1 );
    bool AppendRows( size_t numRows = 1 );
    bool DeleteRows( size_t pos = 0, size_t numRows = 1 );
    bool InsertCols( size_t pos = 0, size_t numCols = 1 );
    bool AppendCols( size_t numCols = 1 );
    bool DeleteCols( size_t pos = 0, size_t numCols = 1 );

    void SetRowLabelValue( int row, const wxString& );
    void SetColLabelValue( int col, const wxString& );
    wxString GetRowLabelValue( int row );
    wxString GetColLabelValue( int col );

private:
    wxGridStringArray m_data;

    // notice that while we don't need to store the number of our rows as it's
    // always equal to the size of m_data array, we do need to store the number
    // of our columns as we can't retrieve it from m_data when the number of
    // rows is 0 (see #10818)
    int m_numCols;

    // These only get used if you set your own labels, otherwise the
    // GetRow/ColLabelValue functions return wxGridTableBase defaults
    //
    wxArrayString     m_rowLabels;
    wxArrayString     m_colLabels;

    DECLARE_DYNAMIC_CLASS_NO_COPY( wxGridStringTable )
};



// ============================================================================
//  Grid view classes
// ============================================================================

// ----------------------------------------------------------------------------
// wxGridSizesInfo stores information about sizes of the rows or columns.
//
// It assumes that most of the columns or rows have default size and so stores
// the default size separately and uses a hash to map column or row numbers to
// their non default size for those which don't have the default size.
// ----------------------------------------------------------------------------

// hash map to store positions as the keys and sizes as the values
WX_DECLARE_HASH_MAP_WITH_DECL( unsigned, int, wxIntegerHash, wxIntegerEqual,
                               wxUnsignedToIntHashMap, class WXDLLIMPEXP_ADV );

struct WXDLLIMPEXP_ADV wxGridSizesInfo
{
    // default ctor, initialize m_sizeDefault and m_customSizes later
    wxGridSizesInfo() { }

    // ctor used by wxGrid::Get{Col,Row}Sizes()
    wxGridSizesInfo(int defSize, const wxArrayInt& allSizes);

    // default copy ctor, assignment operator and dtor are ok

    // Get the size of the element with the given index
    int GetSize(unsigned pos) const;


    // default size
    int m_sizeDefault;

    // position -> size map containing all elements with non-default size
    wxUnsignedToIntHashMap m_customSizes;
};

// ----------------------------------------------------------------------------
// wxGrid
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGrid : public wxScrolledWindow
{
public:
    // possible selection modes
    enum wxGridSelectionModes
    {
        wxGridSelectCells         = 0,  // allow selecting anything
        wxGridSelectRows          = 1,  // allow selecting only entire rows
        wxGridSelectColumns       = 2,  // allow selecting only entire columns
        wxGridSelectRowsOrColumns = wxGridSelectRows | wxGridSelectColumns
    };

    // Different behaviour of the TAB key when the end (or the beginning, for
    // Shift-TAB) of the current row is reached:
    enum TabBehaviour
    {
        Tab_Stop,   // Do nothing, this is default.
        Tab_Wrap,   // Move to the next (or previous) row.
        Tab_Leave   // Move to the next (or previous) control.
    };

    // creation and destruction
    // ------------------------

    // ctor and Create() create the grid window, as with the other controls
    wxGrid() { Init(); }

    wxGrid(wxWindow *parent,
            wxWindowID id,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxWANTS_CHARS,
            const wxString& name = wxGridNameStr)
    {
        Init();

        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxWANTS_CHARS,
                const wxString& name = wxGridNameStr);

    virtual ~wxGrid();

    // however to initialize grid data either CreateGrid() or SetTable() must
    // be also called

    // this is basically equivalent to
    //
    //   SetTable(new wxGridStringTable(numRows, numCols), true, selmode)
    //
    bool CreateGrid( int numRows, int numCols,
                     wxGridSelectionModes selmode = wxGridSelectCells );

    bool SetTable( wxGridTableBase *table,
                   bool takeOwnership = false,
                   wxGridSelectionModes selmode = wxGridSelectCells );

    bool ProcessTableMessage(wxGridTableMessage&);

    wxGridTableBase *GetTable() const { return m_table; }


    void SetSelectionMode(wxGridSelectionModes selmode);
    wxGridSelectionModes GetSelectionMode() const;

    // ------ grid dimensions
    //
    int      GetNumberRows() const { return  m_numRows; }
    int      GetNumberCols() const { return  m_numCols; }


    // ------ display update functions
    //
    wxArrayInt CalcRowLabelsExposed( const wxRegion& reg ) const;

    wxArrayInt CalcColLabelsExposed( const wxRegion& reg ) const;
    wxGridCellCoordsArray CalcCellsExposed( const wxRegion& reg ) const;


    void ClearGrid();
    bool InsertRows(int pos = 0, int numRows = 1, bool updateLabels = true)
    {
        return DoModifyLines(&wxGridTableBase::InsertRows,
                             pos, numRows, updateLabels);
    }
    bool InsertCols(int pos = 0, int numCols = 1, bool updateLabels = true)
    {
        return DoModifyLines(&wxGridTableBase::InsertCols,
                             pos, numCols, updateLabels);
    }

    bool AppendRows(int numRows = 1, bool updateLabels = true)
    {
        return DoAppendLines(&wxGridTableBase::AppendRows, numRows, updateLabels);
    }
    bool AppendCols(int numCols = 1, bool updateLabels = true)
    {
        return DoAppendLines(&wxGridTableBase::AppendCols, numCols, updateLabels);
    }

    bool DeleteRows(int pos = 0, int numRows = 1, bool updateLabels = true)
    {
        return DoModifyLines(&wxGridTableBase::DeleteRows,
                             pos, numRows, updateLabels);
    }
    bool DeleteCols(int pos = 0, int numCols = 1, bool updateLabels = true)
    {
        return DoModifyLines(&wxGridTableBase::DeleteCols,
                             pos, numCols, updateLabels);
    }

    void DrawGridCellArea( wxDC& dc , const wxGridCellCoordsArray& cells );
    void DrawGridSpace( wxDC& dc );
    void DrawCellBorder( wxDC& dc, const wxGridCellCoords& );
    void DrawAllGridLines( wxDC& dc, const wxRegion & reg );
    void DrawCell( wxDC& dc, const wxGridCellCoords& );
    void DrawHighlight(wxDC& dc, const wxGridCellCoordsArray& cells);

    // this function is called when the current cell highlight must be redrawn
    // and may be overridden by the user
    virtual void DrawCellHighlight( wxDC& dc, const wxGridCellAttr *attr );

    virtual void DrawRowLabels( wxDC& dc, const wxArrayInt& rows );
    virtual void DrawRowLabel( wxDC& dc, int row );

    virtual void DrawColLabels( wxDC& dc, const wxArrayInt& cols );
    virtual void DrawColLabel( wxDC& dc, int col );

    virtual void DrawCornerLabel(wxDC& dc);

    // ------ Cell text drawing functions
    //
    void DrawTextRectangle( wxDC& dc, const wxString&, const wxRect&,
                            int horizontalAlignment = wxALIGN_LEFT,
                            int verticalAlignment = wxALIGN_TOP,
                            int textOrientation = wxHORIZONTAL ) const;

    void DrawTextRectangle( wxDC& dc, const wxArrayString& lines, const wxRect&,
                            int horizontalAlignment = wxALIGN_LEFT,
                            int verticalAlignment = wxALIGN_TOP,
                            int textOrientation = wxHORIZONTAL ) const;

    // ------ grid render function for printing
    //
    void Render( wxDC& dc,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 const wxGridCellCoords& topLeft = wxGridCellCoords(-1, -1),
                 const wxGridCellCoords& bottomRight = wxGridCellCoords(-1, -1),
                 int style = wxGRID_DRAW_DEFAULT );

    // Split a string containing newline characters into an array of
    // strings and return the number of lines
    //
    void StringToLines( const wxString& value, wxArrayString& lines ) const;

    void GetTextBoxSize( const wxDC& dc,
                         const wxArrayString& lines,
                         long *width, long *height ) const;


    // ------
    // Code that does a lot of grid modification can be enclosed
    // between BeginBatch() and EndBatch() calls to avoid screen
    // flicker
    //
    void     BeginBatch() { m_batchCount++; }
    void     EndBatch();

    int      GetBatchCount() { return m_batchCount; }

    virtual void Refresh(bool eraseb = true, const wxRect* rect = NULL);

    // Use this, rather than wxWindow::Refresh(), to force an
    // immediate repainting of the grid. Has no effect if you are
    // already inside a BeginBatch / EndBatch block.
    //
    // This function is necessary because wxGrid has a minimal OnPaint()
    // handler to reduce screen flicker.
    //
    void     ForceRefresh();


    // ------ edit control functions
    //
    bool IsEditable() const { return m_editable; }
    void EnableEditing( bool edit );

    void EnableCellEditControl( bool enable = true );
    void DisableCellEditControl() { EnableCellEditControl(false); }
    bool CanEnableCellControl() const;
    bool IsCellEditControlEnabled() const;
    bool IsCellEditControlShown() const;

    bool IsCurrentCellReadOnly() const;

    void ShowCellEditControl();
    void HideCellEditControl();
    void SaveEditControlValue();


    // ------ grid location functions
    //  Note that all of these functions work with the logical coordinates of
    //  grid cells and labels so you will need to convert from device
    //  coordinates for mouse events etc.
    //
    wxGridCellCoords XYToCell(int x, int y) const;
    void XYToCell(int x, int y, wxGridCellCoords& coords) const
        { coords = XYToCell(x, y); }
    wxGridCellCoords XYToCell(const wxPoint& pos) const
        { return XYToCell(pos.x, pos.y); }

    // these functions return the index of the row/columns corresponding to the
    // given logical position in pixels
    //
    // if clipToMinMax is false (default, wxNOT_FOUND is returned if the
    // position is outside any row/column, otherwise the first/last element is
    // returned in this case
    int  YToRow( int y, bool clipToMinMax = false ) const;
    int  XToCol( int x, bool clipToMinMax = false ) const;

    int  YToEdgeOfRow( int y ) const;
    int  XToEdgeOfCol( int x ) const;

    wxRect CellToRect( int row, int col ) const;
    wxRect CellToRect( const wxGridCellCoords& coords ) const
        { return CellToRect( coords.GetRow(), coords.GetCol() ); }

    int  GetGridCursorRow() const { return m_currentCellCoords.GetRow(); }
    int  GetGridCursorCol() const { return m_currentCellCoords.GetCol(); }

    // check to see if a cell is either wholly visible (the default arg) or
    // at least partially visible in the grid window
    //
    bool IsVisible( int row, int col, bool wholeCellVisible = true ) const;
    bool IsVisible( const wxGridCellCoords& coords, bool wholeCellVisible = true ) const
        { return IsVisible( coords.GetRow(), coords.GetCol(), wholeCellVisible ); }
    void MakeCellVisible( int row, int col );
    void MakeCellVisible( const wxGridCellCoords& coords )
        { MakeCellVisible( coords.GetRow(), coords.GetCol() ); }


    // ------ grid cursor movement functions
    //
    void SetGridCursor(int row, int col) { SetCurrentCell(row, col); }
    void SetGridCursor(const wxGridCellCoords& c) { SetCurrentCell(c); }

    void GoToCell(int row, int col)
    {
        if ( SetCurrentCell(row, col) )
            MakeCellVisible(row, col);
    }

    void GoToCell(const wxGridCellCoords& coords)
    {
        if ( SetCurrentCell(coords) )
            MakeCellVisible(coords);
    }

    bool MoveCursorUp( bool expandSelection );
    bool MoveCursorDown( bool expandSelection );
    bool MoveCursorLeft( bool expandSelection );
    bool MoveCursorRight( bool expandSelection );
    bool MovePageDown();
    bool MovePageUp();
    bool MoveCursorUpBlock( bool expandSelection );
    bool MoveCursorDownBlock( bool expandSelection );
    bool MoveCursorLeftBlock( bool expandSelection );
    bool MoveCursorRightBlock( bool expandSelection );

    void SetTabBehaviour(TabBehaviour behaviour) { m_tabBehaviour = behaviour; }


    // ------ label and gridline formatting
    //
    int      GetDefaultRowLabelSize() const { return WXGRID_DEFAULT_ROW_LABEL_WIDTH; }
    int      GetRowLabelSize() const { return m_rowLabelWidth; }
    int      GetDefaultColLabelSize() const { return WXGRID_DEFAULT_COL_LABEL_HEIGHT; }
    int      GetColLabelSize() const { return m_colLabelHeight; }
    wxColour GetLabelBackgroundColour() const { return m_labelBackgroundColour; }
    wxColour GetLabelTextColour() const { return m_labelTextColour; }
    wxFont   GetLabelFont() const { return m_labelFont; }
    void     GetRowLabelAlignment( int *horiz, int *vert ) const;
    void     GetColLabelAlignment( int *horiz, int *vert ) const;
    int      GetColLabelTextOrientation() const;
    wxString GetRowLabelValue( int row ) const;
    wxString GetColLabelValue( int col ) const;

    wxColour GetCellHighlightColour() const { return m_cellHighlightColour; }
    int      GetCellHighlightPenWidth() const { return m_cellHighlightPenWidth; }
    int      GetCellHighlightROPenWidth() const { return m_cellHighlightROPenWidth; }

    // this one will use wxHeaderCtrl for the column labels
    void UseNativeColHeader(bool native = true);

    // this one will still draw them manually but using the native renderer
    // instead of using the same appearance as for the row labels
    void SetUseNativeColLabels( bool native = true );

    void     SetRowLabelSize( int width );
    void     SetColLabelSize( int height );
    void     HideRowLabels() { SetRowLabelSize( 0 ); }
    void     HideColLabels() { SetColLabelSize( 0 ); }
    void     SetLabelBackgroundColour( const wxColour& );
    void     SetLabelTextColour( const wxColour& );
    void     SetLabelFont( const wxFont& );
    void     SetRowLabelAlignment( int horiz, int vert );
    void     SetColLabelAlignment( int horiz, int vert );
    void     SetColLabelTextOrientation( int textOrientation );
    void     SetRowLabelValue( int row, const wxString& );
    void     SetColLabelValue( int col, const wxString& );
    void     SetCellHighlightColour( const wxColour& );
    void     SetCellHighlightPenWidth(int width);
    void     SetCellHighlightROPenWidth(int width);


    // interactive grid mouse operations control
    // -----------------------------------------

    // functions globally enabling row/column interactive resizing (enabled by
    // default)
    void     EnableDragRowSize( bool enable = true );
    void     DisableDragRowSize() { EnableDragRowSize( false ); }

    void     EnableDragColSize( bool enable = true );
    void     DisableDragColSize() { EnableDragColSize( false ); }

        // if interactive resizing is enabled, some rows/columns can still have
        // fixed size
    void DisableRowResize(int row) { DoDisableLineResize(row, m_setFixedRows); }
    void DisableColResize(int col) { DoDisableLineResize(col, m_setFixedCols); }

        // these functions return whether the given row/column can be
        // effectively resized: for this interactive resizing must be enabled
        // and this index must not have been passed to DisableRow/ColResize()
    bool CanDragRowSize(int row) const
        { return m_canDragRowSize && DoCanResizeLine(row, m_setFixedRows); }
    bool CanDragColSize(int col) const
        { return m_canDragColSize && DoCanResizeLine(col, m_setFixedCols); }

    // interactive column reordering (disabled by default)
    void     EnableDragColMove( bool enable = true );
    void     DisableDragColMove() { EnableDragColMove( false ); }
    bool     CanDragColMove() const { return m_canDragColMove; }

    // interactive resizing of grid cells (enabled by default)
    void     EnableDragGridSize(bool enable = true);
    void     DisableDragGridSize() { EnableDragGridSize(false); }
    bool     CanDragGridSize() const { return m_canDragGridSize; }

    // interactive dragging of cells (disabled by default)
    void     EnableDragCell( bool enable = true );
    void     DisableDragCell() { EnableDragCell( false ); }
    bool     CanDragCell() const { return m_canDragCell; }


    // grid lines
    // ----------

    // enable or disable drawing of the lines
    void EnableGridLines(bool enable = true);
    bool GridLinesEnabled() const { return m_gridLinesEnabled; }

    // by default grid lines stop at last column/row, but this may be changed
    void ClipHorzGridLines(bool clip)
        { DoClipGridLines(m_gridLinesClipHorz, clip); }
    void ClipVertGridLines(bool clip)
        { DoClipGridLines(m_gridLinesClipVert, clip); }
    bool AreHorzGridLinesClipped() const { return m_gridLinesClipHorz; }
    bool AreVertGridLinesClipped() const { return m_gridLinesClipVert; }

    // this can be used to change the global grid lines colour
    void SetGridLineColour(const wxColour& col);
    wxColour GetGridLineColour() const { return m_gridLineColour; }

    // these methods may be overridden to customize individual grid lines
    // appearance
    virtual wxPen GetDefaultGridLinePen();
    virtual wxPen GetRowGridLinePen(int row);
    virtual wxPen GetColGridLinePen(int col);


    // attributes
    // ----------

    // this sets the specified attribute for this cell or in this row/col
    void     SetAttr(int row, int col, wxGridCellAttr *attr);
    void     SetRowAttr(int row, wxGridCellAttr *attr);
    void     SetColAttr(int col, wxGridCellAttr *attr);

    // the grid can cache attributes for the recently used cells (currently it
    // only caches one attribute for the most recently used one) and might
    // notice that its value in the attribute provider has changed -- if this
    // happens, call this function to force it
    void RefreshAttr(int row, int col);

    // returns the attribute we may modify in place: a new one if this cell
    // doesn't have any yet or the existing one if it does
    //
    // DecRef() must be called on the returned pointer, as usual
    wxGridCellAttr *GetOrCreateCellAttr(int row, int col) const;


    // shortcuts for setting the column parameters

    // set the format for the data in the column: default is string
    void     SetColFormatBool(int col);
    void     SetColFormatNumber(int col);
    void     SetColFormatFloat(int col, int width = -1, int precision = -1);
    void     SetColFormatCustom(int col, const wxString& typeName);

    // ------ row and col formatting
    //
    int      GetDefaultRowSize() const;
    int      GetRowSize( int row ) const;
    bool     IsRowShown(int row) const { return GetRowSize(row) != 0; }
    int      GetDefaultColSize() const;
    int      GetColSize( int col ) const;
    bool     IsColShown(int col) const { return GetColSize(col) != 0; }
    wxColour GetDefaultCellBackgroundColour() const;
    wxColour GetCellBackgroundColour( int row, int col ) const;
    wxColour GetDefaultCellTextColour() const;
    wxColour GetCellTextColour( int row, int col ) const;
    wxFont   GetDefaultCellFont() const;
    wxFont   GetCellFont( int row, int col ) const;
    void     GetDefaultCellAlignment( int *horiz, int *vert ) const;
    void     GetCellAlignment( int row, int col, int *horiz, int *vert ) const;
    bool     GetDefaultCellOverflow() const;
    bool     GetCellOverflow( int row, int col ) const;

    // this function returns 1 in num_rows and num_cols for normal cells,
    // positive numbers for a cell spanning multiple columns/rows (as set with
    // SetCellSize()) and _negative_ numbers corresponding to the offset of the
    // top left cell of the span from this one for the other cells covered by
    // this cell
    //
    // the return value is CellSpan_None, CellSpan_Main or CellSpan_Inside for
    // each of these cases respectively
    enum CellSpan
    {
        CellSpan_Inside = -1,
        CellSpan_None = 0,
        CellSpan_Main
    };

    CellSpan GetCellSize( int row, int col, int *num_rows, int *num_cols ) const;

    wxSize GetCellSize(const wxGridCellCoords& coords)
    {
        wxSize s;
        GetCellSize(coords.GetRow(), coords.GetCol(), &s.x, &s.y);
        return s;
    }

    // ------ row and col sizes
    void     SetDefaultRowSize( int height, bool resizeExistingRows = false );
    void     SetRowSize( int row, int height );
    void     HideRow(int row) { DoSetRowSize(row, 0); }
    void     ShowRow(int row) { DoSetRowSize(row, -1); }

    void     SetDefaultColSize( int width, bool resizeExistingCols = false );
    void     SetColSize( int col, int width );
    void     HideCol(int col) { DoSetColSize(col, 0); }
    void     ShowCol(int col) { DoSetColSize(col, -1); }

    // the row and column sizes can be also set all at once using
    // wxGridSizesInfo which holds all of them at once

    wxGridSizesInfo GetColSizes() const
        { return wxGridSizesInfo(GetDefaultColSize(), m_colWidths); }
    wxGridSizesInfo GetRowSizes() const
        { return wxGridSizesInfo(GetDefaultRowSize(), m_rowHeights); }

    void SetColSizes(const wxGridSizesInfo& sizeInfo);
    void SetRowSizes(const wxGridSizesInfo& sizeInfo);


    // ------- columns (only, for now) reordering

    // columns index <-> positions mapping: by default, the position of the
    // column is the same as its index, but the columns can also be reordered
    // (either by calling SetColPos() explicitly or by the user dragging the
    // columns around) in which case their indices don't correspond to their
    // positions on display any longer
    //
    // internally we always work with indices except for the functions which
    // have "Pos" in their names (and which work with columns, not pixels) and
    // only the display and hit testing code really cares about display
    // positions at all

    // set the positions of all columns at once (this method uses the same
    // conventions as wxHeaderCtrl::SetColumnsOrder() for the order array)
    void SetColumnsOrder(const wxArrayInt& order);

    // return the column index corresponding to the given (valid) position
    int GetColAt(int pos) const
    {
        return m_colAt.empty() ? pos : m_colAt[pos];
    }

    // reorder the columns so that the column with the given index is now shown
    // as the position pos
    void SetColPos(int idx, int pos);

    // return the position at which the column with the given index is
    // displayed: notice that this is a slow operation as we don't maintain the
    // reverse mapping currently
    int GetColPos(int idx) const
    {
        if ( m_colAt.IsEmpty() )
            return idx;

        for ( int i = 0; i < m_numCols; i++ )
        {
            if ( m_colAt[i] == idx )
                return i;
        }

        wxFAIL_MSG( "invalid column index" );

        return wxNOT_FOUND;
    }

    // reset the columns positions to the default order
    void ResetColPos();


    // automatically size the column or row to fit to its contents, if
    // setAsMin is true, this optimal width will also be set as minimal width
    // for this column
    void     AutoSizeColumn( int col, bool setAsMin = true )
        { AutoSizeColOrRow(col, setAsMin, wxGRID_COLUMN); }
    void     AutoSizeRow( int row, bool setAsMin = true )
        { AutoSizeColOrRow(row, setAsMin, wxGRID_ROW); }

    // auto size all columns (very ineffective for big grids!)
    void     AutoSizeColumns( bool setAsMin = true )
        { (void)SetOrCalcColumnSizes(false, setAsMin); }

    void     AutoSizeRows( bool setAsMin = true )
        { (void)SetOrCalcRowSizes(false, setAsMin); }

    // auto size the grid, that is make the columns/rows of the "right" size
    // and also set the grid size to just fit its contents
    void     AutoSize();

    // Note for both AutoSizeRowLabelSize and AutoSizeColLabelSize:
    // If col equals to wxGRID_AUTOSIZE value then function autosizes labels column
    // instead of data column. Note that this operation may be slow for large
    // tables.
    // autosize row height depending on label text
    void     AutoSizeRowLabelSize( int row );

    // autosize column width depending on label text
    void     AutoSizeColLabelSize( int col );

    // column won't be resized to be lesser width - this must be called during
    // the grid creation because it won't resize the column if it's already
    // narrower than the minimal width
    void     SetColMinimalWidth( int col, int width );
    void     SetRowMinimalHeight( int row, int width );

    /*  These members can be used to query and modify the minimal
     *  acceptable size of grid rows and columns. Call this function in
     *  your code which creates the grid if you want to display cells
     *  with a size smaller than the default acceptable minimum size.
     *  Like the members SetColMinimalWidth and SetRowMinimalWidth,
     *  the existing rows or columns will not be checked/resized.
     */
    void     SetColMinimalAcceptableWidth( int width );
    void     SetRowMinimalAcceptableHeight( int width );
    int      GetColMinimalAcceptableWidth() const;
    int      GetRowMinimalAcceptableHeight() const;

    void     SetDefaultCellBackgroundColour( const wxColour& );
    void     SetCellBackgroundColour( int row, int col, const wxColour& );
    void     SetDefaultCellTextColour( const wxColour& );

    void     SetCellTextColour( int row, int col, const wxColour& );
    void     SetDefaultCellFont( const wxFont& );
    void     SetCellFont( int row, int col, const wxFont& );
    void     SetDefaultCellAlignment( int horiz, int vert );
    void     SetCellAlignment( int row, int col, int horiz, int vert );
    void     SetDefaultCellOverflow( bool allow );
    void     SetCellOverflow( int row, int col, bool allow );
    void     SetCellSize( int row, int col, int num_rows, int num_cols );

    // takes ownership of the pointer
    void SetDefaultRenderer(wxGridCellRenderer *renderer);
    void SetCellRenderer(int row, int col, wxGridCellRenderer *renderer);
    wxGridCellRenderer *GetDefaultRenderer() const;
    wxGridCellRenderer* GetCellRenderer(int row, int col) const;

    // takes ownership of the pointer
    void SetDefaultEditor(wxGridCellEditor *editor);
    void SetCellEditor(int row, int col, wxGridCellEditor *editor);
    wxGridCellEditor *GetDefaultEditor() const;
    wxGridCellEditor* GetCellEditor(int row, int col) const;



    // ------ cell value accessors
    //
    wxString GetCellValue( int row, int col ) const
    {
        if ( m_table )
        {
            return m_table->GetValue( row, col );
        }
        else
        {
            return wxEmptyString;
        }
    }

    wxString GetCellValue( const wxGridCellCoords& coords ) const
        { return GetCellValue( coords.GetRow(), coords.GetCol() ); }

    void SetCellValue( int row, int col, const wxString& s );
    void SetCellValue( const wxGridCellCoords& coords, const wxString& s )
        { SetCellValue( coords.GetRow(), coords.GetCol(), s ); }

    // returns true if the cell can't be edited
    bool IsReadOnly(int row, int col) const;

    // make the cell editable/readonly
    void SetReadOnly(int row, int col, bool isReadOnly = true);

    // ------ select blocks of cells
    //
    void SelectRow( int row, bool addToSelected = false );
    void SelectCol( int col, bool addToSelected = false );

    void SelectBlock( int topRow, int leftCol, int bottomRow, int rightCol,
                      bool addToSelected = false );

    void SelectBlock( const wxGridCellCoords& topLeft,
                      const wxGridCellCoords& bottomRight,
                      bool addToSelected = false )
        { SelectBlock( topLeft.GetRow(), topLeft.GetCol(),
                       bottomRight.GetRow(), bottomRight.GetCol(),
                       addToSelected ); }

    void SelectAll();

    bool IsSelection() const;

    // ------ deselect blocks or cells
    //
    void DeselectRow( int row );
    void DeselectCol( int col );
    void DeselectCell( int row, int col );

    void ClearSelection();

    bool IsInSelection( int row, int col ) const;

    bool IsInSelection( const wxGridCellCoords& coords ) const
        { return IsInSelection( coords.GetRow(), coords.GetCol() ); }

    wxGridCellCoordsArray GetSelectedCells() const;
    wxGridCellCoordsArray GetSelectionBlockTopLeft() const;
    wxGridCellCoordsArray GetSelectionBlockBottomRight() const;
    wxArrayInt GetSelectedRows() const;
    wxArrayInt GetSelectedCols() const;

    // This function returns the rectangle that encloses the block of cells
    // limited by TopLeft and BottomRight cell in device coords and clipped
    //  to the client size of the grid window.
    //
    wxRect BlockToDeviceRect( const wxGridCellCoords & topLeft,
                              const wxGridCellCoords & bottomRight ) const;

    // Access or update the selection fore/back colours
    wxColour GetSelectionBackground() const
        { return m_selectionBackground; }
    wxColour GetSelectionForeground() const
        { return m_selectionForeground; }

    void SetSelectionBackground(const wxColour& c) { m_selectionBackground = c; }
    void SetSelectionForeground(const wxColour& c) { m_selectionForeground = c; }


    // Methods for a registry for mapping data types to Renderers/Editors
    void RegisterDataType(const wxString& typeName,
                          wxGridCellRenderer* renderer,
                          wxGridCellEditor* editor);
    // DJC MAPTEK
    virtual wxGridCellEditor* GetDefaultEditorForCell(int row, int col) const;
    wxGridCellEditor* GetDefaultEditorForCell(const wxGridCellCoords& c) const
        { return GetDefaultEditorForCell(c.GetRow(), c.GetCol()); }
    virtual wxGridCellRenderer* GetDefaultRendererForCell(int row, int col) const;
    virtual wxGridCellEditor* GetDefaultEditorForType(const wxString& typeName) const;
    virtual wxGridCellRenderer* GetDefaultRendererForType(const wxString& typeName) const;

    // grid may occupy more space than needed for its rows/columns, this
    // function allows to set how big this extra space is
    void SetMargins(int extraWidth, int extraHeight)
    {
        m_extraWidth = extraWidth;
        m_extraHeight = extraHeight;

        CalcDimensions();
    }

    // Accessors for component windows
    wxWindow* GetGridWindow() const            { return (wxWindow*)m_gridWin; }
    wxWindow* GetGridRowLabelWindow() const    { return (wxWindow*)m_rowLabelWin; }
    wxWindow* GetGridColLabelWindow() const    { return m_colWindow; }
    wxWindow* GetGridCornerLabelWindow() const { return (wxWindow*)m_cornerLabelWin; }

    // This one can only be called if we are using the native header window
    wxHeaderCtrl *GetGridColHeader() const
    {
        wxASSERT_MSG( m_useNativeHeader, "no column header window" );

        // static_cast<> doesn't work without the full class declaration in
        // view and we prefer to avoid adding more compile-time dependencies
        // even at the cost of using reinterpret_cast<>
        return reinterpret_cast<wxHeaderCtrl *>(m_colWindow);
    }

    // Allow adjustment of scroll increment. The default is (15, 15).
    void SetScrollLineX(int x) { m_xScrollPixelsPerLine = x; }
    void SetScrollLineY(int y) { m_yScrollPixelsPerLine = y; }
    int GetScrollLineX() const { return m_xScrollPixelsPerLine; }
    int GetScrollLineY() const { return m_yScrollPixelsPerLine; }

    // ------- drag and drop
#if wxUSE_DRAG_AND_DROP
    virtual void SetDropTarget(wxDropTarget *dropTarget);
#endif // wxUSE_DRAG_AND_DROP


    // ------- sorting support

    // wxGrid doesn't support sorting on its own but it can indicate the sort
    // order in the column header (currently only if native header control is
    // used though)

    // return the column currently displaying the sort indicator or wxNOT_FOUND
    // if none
    int GetSortingColumn() const { return m_sortCol; }

    // return true if this column is currently used for sorting
    bool IsSortingBy(int col) const { return GetSortingColumn() == col; }

    // return the current sorting order (on GetSortingColumn()): true for
    // ascending sort and false for descending; it doesn't make sense to call
    // it if GetSortingColumn() returns wxNOT_FOUND
    bool IsSortOrderAscending() const { return m_sortIsAscending; }

    // set the sorting column (or unsets any existing one if wxNOT_FOUND) and
    // the order in which to sort
    void SetSortingColumn(int col, bool ascending = true);

    // unset any existing sorting column
    void UnsetSortingColumn() { SetSortingColumn(wxNOT_FOUND); }

#if WXWIN_COMPATIBILITY_2_8
    // ------ For compatibility with previous wxGrid only...
    //
    //  ************************************************
    //  **  Don't use these in new code because they  **
    //  **  are liable to disappear in a future       **
    //  **  revision                                  **
    //  ************************************************
    //

    wxGrid( wxWindow *parent,
            int x, int y, int w = wxDefaultCoord, int h = wxDefaultCoord,
            long style = wxWANTS_CHARS,
            const wxString& name = wxPanelNameStr )
    {
        Init();
        Create(parent, wxID_ANY, wxPoint(x, y), wxSize(w, h), style, name);
    }

    void SetCellValue( const wxString& val, int row, int col )
        { SetCellValue( row, col, val ); }

    void UpdateDimensions()
        { CalcDimensions(); }

    int GetRows() const { return GetNumberRows(); }
    int GetCols() const { return GetNumberCols(); }
    int GetCursorRow() const { return GetGridCursorRow(); }
    int GetCursorColumn() const { return GetGridCursorCol(); }

    int GetScrollPosX() const { return 0; }
    int GetScrollPosY() const { return 0; }

    void SetScrollX( int WXUNUSED(x) ) { }
    void SetScrollY( int WXUNUSED(y) ) { }

    void SetColumnWidth( int col, int width )
        { SetColSize( col, width ); }

    int GetColumnWidth( int col ) const
        { return GetColSize( col ); }

    void SetRowHeight( int row, int height )
        { SetRowSize( row, height ); }

    // GetRowHeight() is below

    int GetViewHeight() const // returned num whole rows visible
        { return 0; }

    int GetViewWidth() const // returned num whole cols visible
        { return 0; }

    void SetLabelSize( int orientation, int sz )
        {
            if ( orientation == wxHORIZONTAL )
                SetColLabelSize( sz );
            else
                SetRowLabelSize( sz );
        }

    int GetLabelSize( int orientation ) const
        {
            if ( orientation == wxHORIZONTAL )
                return GetColLabelSize();
            else
                return GetRowLabelSize();
        }

    void SetLabelAlignment( int orientation, int align )
        {
            if ( orientation == wxHORIZONTAL )
                SetColLabelAlignment( align, wxALIGN_INVALID );
            else
                SetRowLabelAlignment( align, wxALIGN_INVALID );
        }

    int GetLabelAlignment( int orientation, int WXUNUSED(align) ) const
        {
            int h, v;
            if ( orientation == wxHORIZONTAL )
            {
                GetColLabelAlignment( &h, &v );
                return h;
            }
            else
            {
                GetRowLabelAlignment( &h, &v );
                return h;
            }
        }

    void SetLabelValue( int orientation, const wxString& val, int pos )
        {
            if ( orientation == wxHORIZONTAL )
                SetColLabelValue( pos, val );
            else
                SetRowLabelValue( pos, val );
        }

    wxString GetLabelValue( int orientation, int pos) const
        {
            if ( orientation == wxHORIZONTAL )
                return GetColLabelValue( pos );
            else
                return GetRowLabelValue( pos );
        }

    wxFont GetCellTextFont() const
        { return m_defaultCellAttr->GetFont(); }

    wxFont GetCellTextFont(int WXUNUSED(row), int WXUNUSED(col)) const
        { return m_defaultCellAttr->GetFont(); }

    void SetCellTextFont(const wxFont& fnt)
        { SetDefaultCellFont( fnt ); }

    void SetCellTextFont(const wxFont& fnt, int row, int col)
        { SetCellFont( row, col, fnt ); }

    void SetCellTextColour(const wxColour& val, int row, int col)
        { SetCellTextColour( row, col, val ); }

    void SetCellTextColour(const wxColour& col)
        { SetDefaultCellTextColour( col ); }

    void SetCellBackgroundColour(const wxColour& col)
        { SetDefaultCellBackgroundColour( col ); }

    void SetCellBackgroundColour(const wxColour& colour, int row, int col)
        { SetCellBackgroundColour( row, col, colour ); }

    bool GetEditable() const { return IsEditable(); }
    void SetEditable( bool edit = true ) { EnableEditing( edit ); }
    bool GetEditInPlace() const { return IsCellEditControlEnabled(); }

    void SetEditInPlace(bool WXUNUSED(edit) = true) { }

    void SetCellAlignment( int align, int row, int col)
    { SetCellAlignment(row, col, align, wxALIGN_CENTER); }
    void SetCellAlignment( int WXUNUSED(align) ) {}
    void SetCellBitmap(wxBitmap *WXUNUSED(bitmap), int WXUNUSED(row), int WXUNUSED(col))
    { }
    void SetDividerPen(const wxPen& WXUNUSED(pen)) { }
    wxPen& GetDividerPen() const;
    void OnActivate(bool WXUNUSED(active)) {}

    // ******** End of compatibility functions **********



    // ------ control IDs
    enum { wxGRID_CELLCTRL = 2000,
           wxGRID_TOPCTRL };

    // ------ control types
    enum { wxGRID_TEXTCTRL = 2100,
           wxGRID_CHECKBOX,
           wxGRID_CHOICE,
           wxGRID_COMBOBOX };

    wxDEPRECATED_INLINE(bool CanDragRowSize() const, return m_canDragRowSize; )
    wxDEPRECATED_INLINE(bool CanDragColSize() const, return m_canDragColSize; )
#endif // WXWIN_COMPATIBILITY_2_8


    // override some base class functions
    virtual bool Enable(bool enable = true);
    virtual wxWindow *GetMainWindowOfCompositeControl()
        { return (wxWindow*)m_gridWin; }
    virtual void Fit();

    // implementation only
    void CancelMouseCapture();

protected:
    virtual wxSize DoGetBestSize() const;

    bool m_created;

    wxGridWindow             *m_gridWin;
    wxGridCornerLabelWindow  *m_cornerLabelWin;
    wxGridRowLabelWindow     *m_rowLabelWin;

    // the real type of the column window depends on m_useNativeHeader value:
    // if it is true, its dynamic type is wxHeaderCtrl, otherwise it is
    // wxGridColLabelWindow, use accessors below when the real type matters
    wxWindow *m_colWindow;

    wxGridColLabelWindow *GetColLabelWindow() const
    {
        wxASSERT_MSG( !m_useNativeHeader, "no column label window" );

        return reinterpret_cast<wxGridColLabelWindow *>(m_colWindow);
    }

    wxGridTableBase          *m_table;
    bool                      m_ownTable;

    int m_numRows;
    int m_numCols;

    wxGridCellCoords m_currentCellCoords;

    // the corners of the block being currently selected or wxGridNoCellCoords
    wxGridCellCoords m_selectedBlockTopLeft;
    wxGridCellCoords m_selectedBlockBottomRight;

    // when selecting blocks of cells (either from the keyboard using Shift
    // with cursor keys, or by dragging the mouse), the selection is anchored
    // at m_currentCellCoords which defines one of the corners of the rectangle
    // being selected -- and this variable defines the other corner, i.e. it's
    // either m_selectedBlockTopLeft or m_selectedBlockBottomRight depending on
    // which of them is not m_currentCellCoords
    //
    // if no block selection is in process, it is set to wxGridNoCellCoords
    wxGridCellCoords m_selectedBlockCorner;

    wxGridSelection  *m_selection;

    wxColour    m_selectionBackground;
    wxColour    m_selectionForeground;

    // NB: *never* access m_row/col arrays directly because they are created
    //     on demand, *always* use accessor functions instead!

    // init the m_rowHeights/Bottoms arrays with default values
    void InitRowHeights();

    int        m_defaultRowHeight;
    int        m_minAcceptableRowHeight;
    wxArrayInt m_rowHeights;
    wxArrayInt m_rowBottoms;

    // init the m_colWidths/Rights arrays
    void InitColWidths();

    int        m_defaultColWidth;
    int        m_minAcceptableColWidth;
    wxArrayInt m_colWidths;
    wxArrayInt m_colRights;

    int m_sortCol;
    bool m_sortIsAscending;

    bool m_useNativeHeader,
         m_nativeColumnLabels;

    // get the col/row coords
    int GetColWidth(int col) const;
    int GetColLeft(int col) const;
    int GetColRight(int col) const;

    // this function must be public for compatibility...
public:
    int GetRowHeight(int row) const;
protected:

    int GetRowTop(int row) const;
    int GetRowBottom(int row) const;

    int m_rowLabelWidth;
    int m_colLabelHeight;

    // the size of the margin left to the right and bottom of the cell area
    int m_extraWidth,
        m_extraHeight;

    wxColour   m_labelBackgroundColour;
    wxColour   m_labelTextColour;
    wxFont     m_labelFont;

    int        m_rowLabelHorizAlign;
    int        m_rowLabelVertAlign;
    int        m_colLabelHorizAlign;
    int        m_colLabelVertAlign;
    int        m_colLabelTextOrientation;

    bool       m_defaultRowLabelValues;
    bool       m_defaultColLabelValues;

    wxColour   m_gridLineColour;
    bool       m_gridLinesEnabled;
    bool       m_gridLinesClipHorz,
               m_gridLinesClipVert;
    wxColour   m_cellHighlightColour;
    int        m_cellHighlightPenWidth;
    int        m_cellHighlightROPenWidth;


    // common part of AutoSizeColumn/Row() and GetBestSize()
    int SetOrCalcColumnSizes(bool calcOnly, bool setAsMin = true);
    int SetOrCalcRowSizes(bool calcOnly, bool setAsMin = true);

    // common part of AutoSizeColumn/Row()
    void AutoSizeColOrRow(int n, bool setAsMin, wxGridDirection direction);

    // Calculate the minimum acceptable size for labels area
    wxCoord CalcColOrRowLabelAreaMinSize(wxGridDirection direction);

    // if a column has a minimal width, it will be the value for it in this
    // hash table
    wxLongToLongHashMap m_colMinWidths,
                        m_rowMinHeights;

    // get the minimal width of the given column/row
    int GetColMinimalWidth(int col) const;
    int GetRowMinimalHeight(int col) const;

    // do we have some place to store attributes in?
    bool CanHaveAttributes() const;

    // cell attribute cache (currently we only cache 1, may be will do
    // more/better later)
    struct CachedAttr
    {
        int             row, col;
        wxGridCellAttr *attr;
    } m_attrCache;

    // invalidates the attribute cache
    void ClearAttrCache();

    // adds an attribute to cache
    void CacheAttr(int row, int col, wxGridCellAttr *attr) const;

    // looks for an attr in cache, returns true if found
    bool LookupAttr(int row, int col, wxGridCellAttr **attr) const;

    // looks for the attr in cache, if not found asks the table and caches the
    // result
    wxGridCellAttr *GetCellAttr(int row, int col) const;
    wxGridCellAttr *GetCellAttr(const wxGridCellCoords& coords ) const
        { return GetCellAttr( coords.GetRow(), coords.GetCol() ); }

    // the default cell attr object for cells that don't have their own
    wxGridCellAttr*     m_defaultCellAttr;


    bool m_inOnKeyDown;
    int  m_batchCount;


    wxGridTypeRegistry*    m_typeRegistry;

    enum CursorMode
    {
        WXGRID_CURSOR_SELECT_CELL,
        WXGRID_CURSOR_RESIZE_ROW,
        WXGRID_CURSOR_RESIZE_COL,
        WXGRID_CURSOR_SELECT_ROW,
        WXGRID_CURSOR_SELECT_COL,
        WXGRID_CURSOR_MOVE_COL
    };

    // this method not only sets m_cursorMode but also sets the correct cursor
    // for the given mode and, if captureMouse is not false releases the mouse
    // if it was captured and captures it if it must be captured
    //
    // for this to work, you should always use it and not set m_cursorMode
    // directly!
    void ChangeCursorMode(CursorMode mode,
                          wxWindow *win = NULL,
                          bool captureMouse = true);

    wxWindow *m_winCapture;     // the window which captured the mouse

    // this variable is used not for finding the correct current cursor but
    // mainly for finding out what is going to happen if the mouse starts being
    // dragged right now
    //
    // by default it is WXGRID_CURSOR_SELECT_CELL meaning that nothing else is
    // going on, and it is set to one of RESIZE/SELECT/MOVE values while the
    // corresponding operation will be started if the user starts dragging the
    // mouse from the current position
    CursorMode m_cursorMode;


    //Column positions
    wxArrayInt m_colAt;

    bool    m_canDragRowSize;
    bool    m_canDragColSize;
    bool    m_canDragColMove;
    bool    m_canDragGridSize;
    bool    m_canDragCell;

    // the last position (horizontal or vertical depending on whether the user
    // is resizing a column or a row) where a row or column separator line was
    // dragged by the user or -1 of there is no drag operation in progress
    int     m_dragLastPos;
    int     m_dragRowOrCol;

    // true if a drag operation is in progress; when this is true,
    // m_startDragPos is valid, i.e. not wxDefaultPosition
    bool    m_isDragging;

    // the position (in physical coordinates) where the user started dragging
    // the mouse or wxDefaultPosition if mouse isn't being dragged
    //
    // notice that this can be != wxDefaultPosition while m_isDragging is still
    // false because we wait until the mouse is moved some distance away before
    // setting m_isDragging to true
    wxPoint m_startDragPos;

    bool    m_waitForSlowClick;

    wxGridCellCoords m_selectionStart;

    wxCursor m_rowResizeCursor;
    wxCursor m_colResizeCursor;

    bool       m_editable;              // applies to whole grid
    bool       m_cellEditCtrlEnabled;   // is in-place edit currently shown?

    TabBehaviour m_tabBehaviour;        // determines how the TAB key behaves

    void Init();        // common part of all ctors
    void Create();
    void CreateColumnWindow();
    void CalcDimensions();
    void CalcWindowSizes();
    bool Redimension( wxGridTableMessage& );


    // generate the appropriate grid event and return -1 if it was vetoed, 1 if
    // it was processed (but not vetoed) and 0 if it wasn't processed
    int SendEvent(const wxEventType evtType,
                  int row, int col,
                  const wxMouseEvent& e);
    int SendEvent(const wxEventType evtType,
                  const wxGridCellCoords& coords,
                  const wxMouseEvent& e)
        { return SendEvent(evtType, coords.GetRow(), coords.GetCol(), e); }
    int SendEvent(const wxEventType evtType,
                  int row, int col,
                  const wxString& s = wxString());
    int SendEvent(const wxEventType evtType,
                  const wxGridCellCoords& coords,
                  const wxString& s = wxString())
        { return SendEvent(evtType, coords.GetRow(), coords.GetCol(), s); }
    int SendEvent(const wxEventType evtType, const wxString& s = wxString())
        { return SendEvent(evtType, m_currentCellCoords, s); }

    // send wxEVT_GRID_{ROW,COL}_SIZE or wxEVT_GRID_COL_AUTO_SIZE, return true
    // if the event was processed, false otherwise
    bool SendGridSizeEvent(wxEventType type,
                           int row, int col,
                           const wxMouseEvent& mouseEv);

    void OnPaint( wxPaintEvent& );
    void OnSize( wxSizeEvent& );
    void OnKeyDown( wxKeyEvent& );
    void OnKeyUp( wxKeyEvent& );
    void OnChar( wxKeyEvent& );
    void OnEraseBackground( wxEraseEvent& );
    void OnHideEditor( wxCommandEvent& );


    bool SetCurrentCell( const wxGridCellCoords& coords );
    bool SetCurrentCell( int row, int col )
        { return SetCurrentCell( wxGridCellCoords(row, col) ); }


    // this function is called to extend the block being currently selected
    // from mouse and keyboard event handlers
    void UpdateBlockBeingSelected(int topRow, int leftCol,
                                  int bottomRow, int rightCol);

    void UpdateBlockBeingSelected(const wxGridCellCoords& topLeft,
                        const wxGridCellCoords& bottomRight)
        { UpdateBlockBeingSelected(topLeft.GetRow(), topLeft.GetCol(),
                         bottomRight.GetRow(), bottomRight.GetCol()); }

    // ------ functions to get/send data (see also public functions)
    //
    bool GetModelValues();
    bool SetModelValues();

    friend class WXDLLIMPEXP_FWD_ADV wxGridSelection;
    friend class wxGridRowOperations;
    friend class wxGridColumnOperations;

    // they call our private Process{{Corner,Col,Row}Label,GridCell}MouseEvent()
    friend class wxGridCornerLabelWindow;
    friend class wxGridColLabelWindow;
    friend class wxGridRowLabelWindow;
    friend class wxGridWindow;
    friend class wxGridHeaderRenderer;

    friend class wxGridHeaderCtrl;

private:

    // implement wxScrolledWindow method to return m_gridWin size
    virtual wxSize GetSizeAvailableForScrollTarget(const wxSize& size);

    // redraw the grid lines, should be called after changing their attributes
    void RedrawGridLines();

    // draw all grid lines in the given cell region (unlike the public
    // DrawAllGridLines() which just draws all of them)
    void DrawRangeGridLines(wxDC& dc, const wxRegion& reg,
                            const wxGridCellCoords& topLeft,
                            const wxGridCellCoords& bottomRight);

    // draw all lines from top to bottom row and left to right column in the
    // rectangle determined by (top, left)-(bottom, right) -- but notice that
    // the caller must have set up the clipping correctly, this rectangle is
    // only used here for optimization
    void DoDrawGridLines(wxDC& dc,
                         int top, int left,
                         int bottom, int right,
                         int topRow, int leftCol,
                         int bottomRight, int rightCol);

    // common part of Clip{Horz,Vert}GridLines
    void DoClipGridLines(bool& var, bool clip);

    // update the sorting indicator shown in the specified column (whose index
    // must be valid)
    //
    // this will use GetSortingColumn() and IsSortOrderAscending() to determine
    // the sorting indicator to effectively show
    void UpdateColumnSortingIndicator(int col);

    // update the grid after changing the columns order (common part of
    // SetColPos() and ResetColPos())
    void RefreshAfterColPosChange();


    // return the position (not index) of the column at the given logical pixel
    // position
    //
    // this always returns a valid position, even if the coordinate is out of
    // bounds (in which case first/last column is returned)
    int XToPos(int x) const;


    // event handlers and their helpers
    // --------------------------------

    // process mouse drag event in WXGRID_CURSOR_SELECT_CELL mode
    bool DoGridCellDrag(wxMouseEvent& event,
                        const wxGridCellCoords& coords,
                        bool isFirstDrag);

    // process row/column resizing drag event
    void DoGridLineDrag(wxMouseEvent& event, const wxGridOperations& oper);

    // process mouse drag event in the grid window
    void DoGridDragEvent(wxMouseEvent& event, const wxGridCellCoords& coords);

    // process different clicks on grid cells
    void DoGridCellLeftDown(wxMouseEvent& event,
                            const wxGridCellCoords& coords,
                            const wxPoint& pos);
    void DoGridCellLeftDClick(wxMouseEvent& event,
                             const wxGridCellCoords& coords,
                             const wxPoint& pos);
    void DoGridCellLeftUp(wxMouseEvent& event, const wxGridCellCoords& coords);

    // process movement (but not dragging) event in the grid cell area
    void DoGridMouseMoveEvent(wxMouseEvent& event,
                              const wxGridCellCoords& coords,
                              const wxPoint& pos);

    // process mouse events in the grid window
    void ProcessGridCellMouseEvent(wxMouseEvent& event);

    // process mouse events in the row/column labels/corner windows
    void ProcessRowLabelMouseEvent(wxMouseEvent& event);
    void ProcessColLabelMouseEvent(wxMouseEvent& event);
    void ProcessCornerLabelMouseEvent(wxMouseEvent& event);

    void DoColHeaderClick(int col);

    void DoStartResizeCol(int col);
    void DoUpdateResizeCol(int x);
    void DoUpdateResizeColWidth(int w);
    void DoStartMoveCol(int col);

    void DoEndDragResizeRow(const wxMouseEvent& event);
    void DoEndDragResizeCol(const wxMouseEvent& event);
    void DoEndMoveCol(int pos);

    // process a TAB keypress
    void DoGridProcessTab(wxKeyboardState& kbdState);

    // common implementations of methods defined for both rows and columns
    void DeselectLine(int line, const wxGridOperations& oper);
    bool DoEndDragResizeLine(const wxGridOperations& oper);
    int PosToLinePos(int pos, bool clipToMinMax,
                     const wxGridOperations& oper) const;
    int PosToLine(int pos, bool clipToMinMax,
                  const wxGridOperations& oper) const;
    int PosToEdgeOfLine(int pos, const wxGridOperations& oper) const;

    bool DoMoveCursor(bool expandSelection,
                      const wxGridDirectionOperations& diroper);
    bool DoMoveCursorByPage(const wxGridDirectionOperations& diroper);
    bool DoMoveCursorByBlock(bool expandSelection,
                             const wxGridDirectionOperations& diroper);
    void AdvanceToNextNonEmpty(wxGridCellCoords& coords,
                               const wxGridDirectionOperations& diroper);

    // common part of {Insert,Delete}{Rows,Cols}
    bool DoModifyLines(bool (wxGridTableBase::*funcModify)(size_t, size_t),
                       int pos, int num, bool updateLabels);
    // Append{Rows,Cols} is a bit different because of one less parameter
    bool DoAppendLines(bool (wxGridTableBase::*funcAppend)(size_t),
                       int num, bool updateLabels);

    // common part of Set{Col,Row}Sizes
    void DoSetSizes(const wxGridSizesInfo& sizeInfo,
                    const wxGridOperations& oper);

    // common part of Disable{Row,Col}Resize and CanDrag{Row,Col}Size
    void DoDisableLineResize(int line, wxGridFixedIndicesSet *& setFixed);
    bool DoCanResizeLine(int line, const wxGridFixedIndicesSet *setFixed) const;

    // Helper of Render(): get grid size, origin offset and fill cell arrays
    void GetRenderSizes( const wxGridCellCoords& topLeft,
                         const wxGridCellCoords& bottomRight,
                         wxPoint& pointOffSet, wxSize& sizeGrid,
                         wxGridCellCoordsArray& renderCells,
                         wxArrayInt& arrayCols, wxArrayInt& arrayRows );

    // Helper of Render(): set the scale to draw the cells at the right size.
    void SetRenderScale( wxDC& dc, const wxPoint& pos, const wxSize& size,
                         const wxSize& sizeGrid );

    // Helper of Render(): get render start position from passed parameter
    wxPoint GetRenderPosition( wxDC& dc, const wxPoint& position );

    // Helper of Render(): draws a box around the rendered area
    void DoRenderBox( wxDC& dc, const int& style,
                      const wxPoint& pointOffSet,
                      const wxSize& sizeCellArea,
                      const wxGridCellCoords& topLeft,
                      const wxGridCellCoords& bottomRight );

    // Implementation of public Set{Row,Col}Size() and {Hide,Show}{Row,Col}().
    // They interpret their height or width parameter slightly different from
    // the public methods where -1 in it means "auto fit to the label" for the
    // compatibility reasons. Here it means "show a previously hidden row or
    // column" while 0 means "hide it" just as in the public methods. And any
    // positive values are handled naturally, i.e. they just specify the size.
    void DoSetRowSize( int row, int height );
    void DoSetColSize( int col, int width );


    // these sets contain the indices of fixed, i.e. non-resizable
    // interactively, grid rows or columns and are NULL if there are no fixed
    // elements (which is the default)
    wxGridFixedIndicesSet *m_setFixedRows,
                          *m_setFixedCols;

    DECLARE_DYNAMIC_CLASS( wxGrid )
    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxGrid);
};

// ----------------------------------------------------------------------------
// wxGridUpdateLocker prevents updates to a grid during its lifetime
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGridUpdateLocker
{
public:
    // if the pointer is NULL, Create() can be called later
    wxGridUpdateLocker(wxGrid *grid = NULL)
    {
        Init(grid);
    }

    // can be called if ctor was used with a NULL pointer, must not be called
    // more than once
    void Create(wxGrid *grid)
    {
        wxASSERT_MSG( !m_grid, wxT("shouldn't be called more than once") );

        Init(grid);
    }

    ~wxGridUpdateLocker()
    {
        if ( m_grid )
            m_grid->EndBatch();
    }

private:
    void Init(wxGrid *grid)
    {
        m_grid = grid;
        if ( m_grid )
            m_grid->BeginBatch();
    }

    wxGrid *m_grid;

    wxDECLARE_NO_COPY_CLASS(wxGridUpdateLocker);
};

// ----------------------------------------------------------------------------
// Grid event class and event types
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGridEvent : public wxNotifyEvent,
                                    public wxKeyboardState
{
public:
    wxGridEvent()
        : wxNotifyEvent()
    {
        Init(-1, -1, -1, -1, false);
    }

    wxGridEvent(int id,
                wxEventType type,
                wxObject* obj,
                int row = -1, int col = -1,
                int x = -1, int y = -1,
                bool sel = true,
                const wxKeyboardState& kbd = wxKeyboardState())
        : wxNotifyEvent(type, id),
          wxKeyboardState(kbd)
    {
        Init(row, col, x, y, sel);
        SetEventObject(obj);
    }

    // explicitly specifying inline allows gcc < 3.4 to
    // handle the deprecation attribute even in the constructor.
    wxDEPRECATED_CONSTRUCTOR(
    wxGridEvent(int id,
                wxEventType type,
                wxObject* obj,
                int row, int col,
                int x, int y,
                bool sel,
                bool control,
                bool shift = false, bool alt = false, bool meta = false));

    virtual int GetRow() { return m_row; }
    virtual int GetCol() { return m_col; }
    wxPoint     GetPosition() { return wxPoint( m_x, m_y ); }
    bool        Selecting() { return m_selecting; }

    virtual wxEvent *Clone() const { return new wxGridEvent(*this); }

protected:
    int         m_row;
    int         m_col;
    int         m_x;
    int         m_y;
    bool        m_selecting;

private:
    void Init(int row, int col, int x, int y, bool sel)
    {
        m_row = row;
        m_col = col;
        m_x = x;
        m_y = y;
        m_selecting = sel;
    }

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxGridEvent)
};

class WXDLLIMPEXP_ADV wxGridSizeEvent : public wxNotifyEvent,
                                        public wxKeyboardState
{
public:
    wxGridSizeEvent()
        : wxNotifyEvent()
    {
        Init(-1, -1, -1);
    }

    wxGridSizeEvent(int id,
                    wxEventType type,
                    wxObject* obj,
                    int rowOrCol = -1,
                    int x = -1, int y = -1,
                    const wxKeyboardState& kbd = wxKeyboardState())
        : wxNotifyEvent(type, id),
          wxKeyboardState(kbd)
    {
        Init(rowOrCol, x, y);

        SetEventObject(obj);
    }

    wxDEPRECATED_CONSTRUCTOR(
    wxGridSizeEvent(int id,
                    wxEventType type,
                    wxObject* obj,
                    int rowOrCol,
                    int x, int y,
                    bool control,
                    bool shift = false,
                    bool alt = false,
                    bool meta = false) );

    int         GetRowOrCol() { return m_rowOrCol; }
    wxPoint     GetPosition() { return wxPoint( m_x, m_y ); }

    virtual wxEvent *Clone() const { return new wxGridSizeEvent(*this); }

protected:
    int         m_rowOrCol;
    int         m_x;
    int         m_y;

private:
    void Init(int rowOrCol, int x, int y)
    {
        m_rowOrCol = rowOrCol;
        m_x = x;
        m_y = y;
    }

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxGridSizeEvent)
};


class WXDLLIMPEXP_ADV wxGridRangeSelectEvent : public wxNotifyEvent,
                                               public wxKeyboardState
{
public:
    wxGridRangeSelectEvent()
        : wxNotifyEvent()
    {
        Init(wxGridNoCellCoords, wxGridNoCellCoords, false);
    }

    wxGridRangeSelectEvent(int id,
                           wxEventType type,
                           wxObject* obj,
                           const wxGridCellCoords& topLeft,
                           const wxGridCellCoords& bottomRight,
                           bool sel = true,
                           const wxKeyboardState& kbd = wxKeyboardState())
        : wxNotifyEvent(type, id),
          wxKeyboardState(kbd)
    {
        Init(topLeft, bottomRight, sel);

        SetEventObject(obj);
    }

    wxDEPRECATED_CONSTRUCTOR(
    wxGridRangeSelectEvent(int id,
                           wxEventType type,
                           wxObject* obj,
                           const wxGridCellCoords& topLeft,
                           const wxGridCellCoords& bottomRight,
                           bool sel,
                           bool control,
                           bool shift = false,
                           bool alt = false,
                           bool meta = false) );

    wxGridCellCoords GetTopLeftCoords() { return m_topLeft; }
    wxGridCellCoords GetBottomRightCoords() { return m_bottomRight; }
    int         GetTopRow()    { return m_topLeft.GetRow(); }
    int         GetBottomRow() { return m_bottomRight.GetRow(); }
    int         GetLeftCol()   { return m_topLeft.GetCol(); }
    int         GetRightCol()  { return m_bottomRight.GetCol(); }
    bool        Selecting() { return m_selecting; }

    virtual wxEvent *Clone() const { return new wxGridRangeSelectEvent(*this); }

protected:
    void Init(const wxGridCellCoords& topLeft,
              const wxGridCellCoords& bottomRight,
              bool selecting)
    {
        m_topLeft = topLeft;
        m_bottomRight = bottomRight;
        m_selecting = selecting;
    }

    wxGridCellCoords  m_topLeft;
    wxGridCellCoords  m_bottomRight;
    bool              m_selecting;

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxGridRangeSelectEvent)
};


class WXDLLIMPEXP_ADV wxGridEditorCreatedEvent : public wxCommandEvent
{
public:
    wxGridEditorCreatedEvent()
        : wxCommandEvent()
        {
            m_row  = 0;
            m_col  = 0;
            m_ctrl = NULL;
        }

    wxGridEditorCreatedEvent(int id, wxEventType type, wxObject* obj,
                             int row, int col, wxControl* ctrl);

    int GetRow()                        { return m_row; }
    int GetCol()                        { return m_col; }
    wxControl* GetControl()             { return m_ctrl; }
    void SetRow(int row)                { m_row = row; }
    void SetCol(int col)                { m_col = col; }
    void SetControl(wxControl* ctrl)    { m_ctrl = ctrl; }

    virtual wxEvent *Clone() const { return new wxGridEditorCreatedEvent(*this); }

private:
    int m_row;
    int m_col;
    wxControl* m_ctrl;

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxGridEditorCreatedEvent)
};


wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_CELL_LEFT_CLICK, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_CELL_RIGHT_CLICK, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_CELL_LEFT_DCLICK, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_CELL_RIGHT_DCLICK, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_LABEL_LEFT_CLICK, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_LABEL_RIGHT_CLICK, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_LABEL_LEFT_DCLICK, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_LABEL_RIGHT_DCLICK, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_ROW_SIZE, wxGridSizeEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_COL_SIZE, wxGridSizeEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_COL_AUTO_SIZE, wxGridSizeEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_RANGE_SELECT, wxGridRangeSelectEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_CELL_CHANGING, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_CELL_CHANGED, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_SELECT_CELL, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_EDITOR_SHOWN, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_EDITOR_HIDDEN, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_EDITOR_CREATED, wxGridEditorCreatedEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_CELL_BEGIN_DRAG, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_COL_MOVE, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_COL_SORT, wxGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_GRID_TABBING, wxGridEvent );

typedef void (wxEvtHandler::*wxGridEventFunction)(wxGridEvent&);
typedef void (wxEvtHandler::*wxGridSizeEventFunction)(wxGridSizeEvent&);
typedef void (wxEvtHandler::*wxGridRangeSelectEventFunction)(wxGridRangeSelectEvent&);
typedef void (wxEvtHandler::*wxGridEditorCreatedEventFunction)(wxGridEditorCreatedEvent&);

#define wxGridEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxGridEventFunction, func)

#define wxGridSizeEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxGridSizeEventFunction, func)

#define wxGridRangeSelectEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxGridRangeSelectEventFunction, func)

#define wxGridEditorCreatedEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxGridEditorCreatedEventFunction, func)

#define wx__DECLARE_GRIDEVT(evt, id, fn) \
    wx__DECLARE_EVT1(wxEVT_GRID_ ## evt, id, wxGridEventHandler(fn))

#define wx__DECLARE_GRIDSIZEEVT(evt, id, fn) \
    wx__DECLARE_EVT1(wxEVT_GRID_ ## evt, id, wxGridSizeEventHandler(fn))

#define wx__DECLARE_GRIDRANGESELEVT(evt, id, fn) \
    wx__DECLARE_EVT1(wxEVT_GRID_ ## evt, id, wxGridRangeSelectEventHandler(fn))

#define wx__DECLARE_GRIDEDITOREVT(evt, id, fn) \
    wx__DECLARE_EVT1(wxEVT_GRID_ ## evt, id, wxGridEditorCreatedEventHandler(fn))

#define EVT_GRID_CMD_CELL_LEFT_CLICK(id, fn)     wx__DECLARE_GRIDEVT(CELL_LEFT_CLICK, id, fn)
#define EVT_GRID_CMD_CELL_RIGHT_CLICK(id, fn)    wx__DECLARE_GRIDEVT(CELL_RIGHT_CLICK, id, fn)
#define EVT_GRID_CMD_CELL_LEFT_DCLICK(id, fn)    wx__DECLARE_GRIDEVT(CELL_LEFT_DCLICK, id, fn)
#define EVT_GRID_CMD_CELL_RIGHT_DCLICK(id, fn)   wx__DECLARE_GRIDEVT(CELL_RIGHT_DCLICK, id, fn)
#define EVT_GRID_CMD_LABEL_LEFT_CLICK(id, fn)    wx__DECLARE_GRIDEVT(LABEL_LEFT_CLICK, id, fn)
#define EVT_GRID_CMD_LABEL_RIGHT_CLICK(id, fn)   wx__DECLARE_GRIDEVT(LABEL_RIGHT_CLICK, id, fn)
#define EVT_GRID_CMD_LABEL_LEFT_DCLICK(id, fn)   wx__DECLARE_GRIDEVT(LABEL_LEFT_DCLICK, id, fn)
#define EVT_GRID_CMD_LABEL_RIGHT_DCLICK(id, fn)  wx__DECLARE_GRIDEVT(LABEL_RIGHT_DCLICK, id, fn)
#define EVT_GRID_CMD_ROW_SIZE(id, fn)            wx__DECLARE_GRIDSIZEEVT(ROW_SIZE, id, fn)
#define EVT_GRID_CMD_COL_SIZE(id, fn)            wx__DECLARE_GRIDSIZEEVT(COL_SIZE, id, fn)
#define EVT_GRID_CMD_COL_AUTO_SIZE(id, fn)       wx__DECLARE_GRIDSIZEEVT(COL_AUTO_SIZE, id, fn)
#define EVT_GRID_CMD_COL_MOVE(id, fn)            wx__DECLARE_GRIDEVT(COL_MOVE, id, fn)
#define EVT_GRID_CMD_COL_SORT(id, fn)            wx__DECLARE_GRIDEVT(COL_SORT, id, fn)
#define EVT_GRID_CMD_RANGE_SELECT(id, fn)        wx__DECLARE_GRIDRANGESELEVT(RANGE_SELECT, id, fn)
#define EVT_GRID_CMD_CELL_CHANGING(id, fn)       wx__DECLARE_GRIDEVT(CELL_CHANGING, id, fn)
#define EVT_GRID_CMD_CELL_CHANGED(id, fn)        wx__DECLARE_GRIDEVT(CELL_CHANGED, id, fn)
#define EVT_GRID_CMD_SELECT_CELL(id, fn)         wx__DECLARE_GRIDEVT(SELECT_CELL, id, fn)
#define EVT_GRID_CMD_EDITOR_SHOWN(id, fn)        wx__DECLARE_GRIDEVT(EDITOR_SHOWN, id, fn)
#define EVT_GRID_CMD_EDITOR_HIDDEN(id, fn)       wx__DECLARE_GRIDEVT(EDITOR_HIDDEN, id, fn)
#define EVT_GRID_CMD_EDITOR_CREATED(id, fn)      wx__DECLARE_GRIDEDITOREVT(EDITOR_CREATED, id, fn)
#define EVT_GRID_CMD_CELL_BEGIN_DRAG(id, fn)     wx__DECLARE_GRIDEVT(CELL_BEGIN_DRAG, id, fn)
#define EVT_GRID_CMD_TABBING(id, fn)             wx__DECLARE_GRIDEVT(TABBING, id, fn)

// same as above but for any id (exists mainly for backwards compatibility but
// then it's also true that you rarely have multiple grid in the same window)
#define EVT_GRID_CELL_LEFT_CLICK(fn)     EVT_GRID_CMD_CELL_LEFT_CLICK(wxID_ANY, fn)
#define EVT_GRID_CELL_RIGHT_CLICK(fn)    EVT_GRID_CMD_CELL_RIGHT_CLICK(wxID_ANY, fn)
#define EVT_GRID_CELL_LEFT_DCLICK(fn)    EVT_GRID_CMD_CELL_LEFT_DCLICK(wxID_ANY, fn)
#define EVT_GRID_CELL_RIGHT_DCLICK(fn)   EVT_GRID_CMD_CELL_RIGHT_DCLICK(wxID_ANY, fn)
#define EVT_GRID_LABEL_LEFT_CLICK(fn)    EVT_GRID_CMD_LABEL_LEFT_CLICK(wxID_ANY, fn)
#define EVT_GRID_LABEL_RIGHT_CLICK(fn)   EVT_GRID_CMD_LABEL_RIGHT_CLICK(wxID_ANY, fn)
#define EVT_GRID_LABEL_LEFT_DCLICK(fn)   EVT_GRID_CMD_LABEL_LEFT_DCLICK(wxID_ANY, fn)
#define EVT_GRID_LABEL_RIGHT_DCLICK(fn)  EVT_GRID_CMD_LABEL_RIGHT_DCLICK(wxID_ANY, fn)
#define EVT_GRID_ROW_SIZE(fn)            EVT_GRID_CMD_ROW_SIZE(wxID_ANY, fn)
#define EVT_GRID_COL_SIZE(fn)            EVT_GRID_CMD_COL_SIZE(wxID_ANY, fn)
#define EVT_GRID_COL_AUTO_SIZE(fn)       EVT_GRID_CMD_COL_AUTO_SIZE(wxID_ANY, fn)
#define EVT_GRID_COL_MOVE(fn)            EVT_GRID_CMD_COL_MOVE(wxID_ANY, fn)
#define EVT_GRID_COL_SORT(fn)            EVT_GRID_CMD_COL_SORT(wxID_ANY, fn)
#define EVT_GRID_RANGE_SELECT(fn)        EVT_GRID_CMD_RANGE_SELECT(wxID_ANY, fn)
#define EVT_GRID_CELL_CHANGING(fn)       EVT_GRID_CMD_CELL_CHANGING(wxID_ANY, fn)
#define EVT_GRID_CELL_CHANGED(fn)        EVT_GRID_CMD_CELL_CHANGED(wxID_ANY, fn)
#define EVT_GRID_SELECT_CELL(fn)         EVT_GRID_CMD_SELECT_CELL(wxID_ANY, fn)
#define EVT_GRID_EDITOR_SHOWN(fn)        EVT_GRID_CMD_EDITOR_SHOWN(wxID_ANY, fn)
#define EVT_GRID_EDITOR_HIDDEN(fn)       EVT_GRID_CMD_EDITOR_HIDDEN(wxID_ANY, fn)
#define EVT_GRID_EDITOR_CREATED(fn)      EVT_GRID_CMD_EDITOR_CREATED(wxID_ANY, fn)
#define EVT_GRID_CELL_BEGIN_DRAG(fn)     EVT_GRID_CMD_CELL_BEGIN_DRAG(wxID_ANY, fn)
#define EVT_GRID_TABBING(fn)             EVT_GRID_CMD_TABBING(wxID_ANY, fn)

// we used to have a single wxEVT_GRID_CELL_CHANGE event but it was split into
// wxEVT_GRID_CELL_CHANGING and CHANGED ones in wx 2.9.0, however the CHANGED
// is basically the same as the old CHANGE event so we keep the name for
// compatibility
#if WXWIN_COMPATIBILITY_2_8
    #define wxEVT_GRID_CELL_CHANGE wxEVT_GRID_CELL_CHANGED

    #define EVT_GRID_CMD_CELL_CHANGE EVT_GRID_CMD_CELL_CHANGED
    #define EVT_GRID_CELL_CHANGE EVT_GRID_CELL_CHANGED
#endif // WXWIN_COMPATIBILITY_2_8

#if 0  // TODO: implement these ?  others ?

extern const int wxEVT_GRID_CREATE_CELL;
extern const int wxEVT_GRID_CHANGE_LABELS;
extern const int wxEVT_GRID_CHANGE_SEL_LABEL;

#define EVT_GRID_CREATE_CELL(fn)      wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_GRID_CREATE_CELL,      wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction)  wxStaticCastEvent( wxGridEventFunction, &fn ), NULL ),
#define EVT_GRID_CHANGE_LABELS(fn)    wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_GRID_CHANGE_LABELS,    wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction)  wxStaticCastEvent( wxGridEventFunction, &fn ), NULL ),
#define EVT_GRID_CHANGE_SEL_LABEL(fn) wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_GRID_CHANGE_SEL_LABEL, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction)  wxStaticCastEvent( wxGridEventFunction, &fn ), NULL ),

#endif

#endif // wxUSE_GRID
#endif // _WX_GENERIC_GRID_H_
