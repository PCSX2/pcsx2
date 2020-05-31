/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/private/grid.h
// Purpose:     Private wxGrid structures
// Author:      Michael Bedward (based on code by Julian Smart, Robin Dunn)
// Modified by: Santiago Palacios
// Created:     1/08/1999
// Copyright:   (c) Michael Bedward
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_GRID_PRIVATE_H_
#define _WX_GENERIC_GRID_PRIVATE_H_

#include "wx/defs.h"

#if wxUSE_GRID

// Internally used (and hence intentionally not exported) event telling wxGrid
// to hide the currently shown editor.
wxDECLARE_EVENT( wxEVT_GRID_HIDE_EDITOR, wxCommandEvent );

// ----------------------------------------------------------------------------
// array classes
// ----------------------------------------------------------------------------

WX_DEFINE_ARRAY_WITH_DECL_PTR(wxGridCellAttr *, wxArrayAttrs,
                                 class WXDLLIMPEXP_ADV);

struct wxGridCellWithAttr
{
    wxGridCellWithAttr(int row, int col, wxGridCellAttr *attr_)
        : coords(row, col), attr(attr_)
    {
        wxASSERT( attr );
    }

    wxGridCellWithAttr(const wxGridCellWithAttr& other)
        : coords(other.coords),
          attr(other.attr)
    {
        attr->IncRef();
    }

    wxGridCellWithAttr& operator=(const wxGridCellWithAttr& other)
    {
        coords = other.coords;
        if (attr != other.attr)
        {
            attr->DecRef();
            attr = other.attr;
            attr->IncRef();
        }
        return *this;
    }

    void ChangeAttr(wxGridCellAttr* new_attr)
    {
        if (attr != new_attr)
        {
            // "Delete" (i.e. DecRef) the old attribute.
            attr->DecRef();
            attr = new_attr;
            // Take ownership of the new attribute, i.e. no IncRef.
        }
    }

    ~wxGridCellWithAttr()
    {
        attr->DecRef();
    }

    wxGridCellCoords coords;
    wxGridCellAttr  *attr;
};

WX_DECLARE_OBJARRAY_WITH_DECL(wxGridCellWithAttr, wxGridCellWithAttrArray,
                              class WXDLLIMPEXP_ADV);


// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// header column providing access to the column information stored in wxGrid
// via wxHeaderColumn interface
class wxGridHeaderColumn : public wxHeaderColumn
{
public:
    wxGridHeaderColumn(wxGrid *grid, int col)
        : m_grid(grid),
          m_col(col)
    {
    }

    virtual wxString GetTitle() const { return m_grid->GetColLabelValue(m_col); }
    virtual wxBitmap GetBitmap() const { return wxNullBitmap; }
    virtual int GetWidth() const { return m_grid->GetColSize(m_col); }
    virtual int GetMinWidth() const { return 0; }
    virtual wxAlignment GetAlignment() const
    {
        int horz,
            vert;
        m_grid->GetColLabelAlignment(&horz, &vert);

        return static_cast<wxAlignment>(horz);
    }

    virtual int GetFlags() const
    {
        // we can't know in advance whether we can sort by this column or not
        // with wxGrid API so suppose we can by default
        int flags = wxCOL_SORTABLE;
        if ( m_grid->CanDragColSize(m_col) )
            flags |= wxCOL_RESIZABLE;
        if ( m_grid->CanDragColMove() )
            flags |= wxCOL_REORDERABLE;
        if ( GetWidth() == 0 )
            flags |= wxCOL_HIDDEN;

        return flags;
    }

    virtual bool IsSortKey() const
    {
        return m_grid->IsSortingBy(m_col);
    }

    virtual bool IsSortOrderAscending() const
    {
        return m_grid->IsSortOrderAscending();
    }

private:
    // these really should be const but are not because the column needs to be
    // assignable to be used in a wxVector (in STL build, in non-STL build we
    // avoid the need for this)
    wxGrid *m_grid;
    int m_col;
};

// header control retreiving column information from the grid
class wxGridHeaderCtrl : public wxHeaderCtrl
{
public:
    wxGridHeaderCtrl(wxGrid *owner)
        : wxHeaderCtrl(owner,
                       wxID_ANY,
                       wxDefaultPosition,
                       wxDefaultSize,
                       wxHD_ALLOW_HIDE |
                       (owner->CanDragColMove() ? wxHD_ALLOW_REORDER : 0))
    {
    }

protected:
    virtual const wxHeaderColumn& GetColumn(unsigned int idx) const
    {
        return m_columns[idx];
    }

private:
    wxGrid *GetOwner() const { return static_cast<wxGrid *>(GetParent()); }

    static wxMouseEvent GetDummyMouseEvent()
    {
        // make up a dummy event for the grid event to use -- unfortunately we
        // can't do anything else here
        wxMouseEvent e;
        e.SetState(wxGetMouseState());
        return e;
    }

    // override the base class method to update our m_columns array
    virtual void OnColumnCountChanging(unsigned int count)
    {
        const unsigned countOld = m_columns.size();
        if ( count < countOld )
        {
            // just discard the columns which don't exist any more (notice that
            // we can't use resize() here as it would require the vector
            // value_type, i.e. wxGridHeaderColumn to be default constructible,
            // which it is not)
            m_columns.erase(m_columns.begin() + count, m_columns.end());
        }
        else // new columns added
        {
            // add columns for the new elements
            for ( unsigned n = countOld; n < count; n++ )
                m_columns.push_back(wxGridHeaderColumn(GetOwner(), n));
        }
    }

    // override to implement column auto sizing
    virtual bool UpdateColumnWidthToFit(unsigned int idx, int widthTitle)
    {
        // TODO: currently grid doesn't support computing the column best width
        //       from its contents so we just use the best label width as is
        GetOwner()->SetColSize(idx, widthTitle);

        return true;
    }

    // overridden to react to the actions using the columns popup menu
    virtual void UpdateColumnVisibility(unsigned int idx, bool show)
    {
        GetOwner()->SetColSize(idx, show ? wxGRID_AUTOSIZE : 0);

        // as this is done by the user we should notify the main program about
        // it
        GetOwner()->SendGridSizeEvent(wxEVT_GRID_COL_SIZE, -1, idx,
                                      GetDummyMouseEvent());
    }

    // overridden to react to the columns order changes in the customization
    // dialog
    virtual void UpdateColumnsOrder(const wxArrayInt& order)
    {
        GetOwner()->SetColumnsOrder(order);
    }


    // event handlers forwarding wxHeaderCtrl events to wxGrid
    void OnClick(wxHeaderCtrlEvent& event)
    {
        GetOwner()->SendEvent(wxEVT_GRID_LABEL_LEFT_CLICK,
                              -1, event.GetColumn(),
                              GetDummyMouseEvent());

        GetOwner()->DoColHeaderClick(event.GetColumn());
    }

    void OnDoubleClick(wxHeaderCtrlEvent& event)
    {
        if ( !GetOwner()->SendEvent(wxEVT_GRID_LABEL_LEFT_DCLICK,
                                    -1, event.GetColumn(),
                                    GetDummyMouseEvent()) )
        {
            event.Skip();
        }
    }

    void OnRightClick(wxHeaderCtrlEvent& event)
    {
        if ( !GetOwner()->SendEvent(wxEVT_GRID_LABEL_RIGHT_CLICK,
                                    -1, event.GetColumn(),
                                    GetDummyMouseEvent()) )
        {
            event.Skip();
        }
    }

    void OnBeginResize(wxHeaderCtrlEvent& event)
    {
        GetOwner()->DoStartResizeCol(event.GetColumn());

        event.Skip();
    }

    void OnResizing(wxHeaderCtrlEvent& event)
    {
        GetOwner()->DoUpdateResizeColWidth(event.GetWidth());
    }

    void OnEndResize(wxHeaderCtrlEvent& event)
    {
        // we again need to pass a mouse event to be used for the grid event
        // generation but we don't have it here so use a dummy one as in
        // UpdateColumnVisibility()
        wxMouseEvent e;
        e.SetState(wxGetMouseState());
        GetOwner()->DoEndDragResizeCol(e);

        event.Skip();
    }

    void OnBeginReorder(wxHeaderCtrlEvent& event)
    {
        GetOwner()->DoStartMoveCol(event.GetColumn());
    }

    void OnEndReorder(wxHeaderCtrlEvent& event)
    {
        GetOwner()->DoEndMoveCol(event.GetNewOrder());
    }

    wxVector<wxGridHeaderColumn> m_columns;

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxGridHeaderCtrl);
};

// common base class for various grid subwindows
class WXDLLIMPEXP_ADV wxGridSubwindow : public wxWindow
{
public:
    wxGridSubwindow(wxGrid *owner,
                    int additionalStyle = 0,
                    const wxString& name = wxPanelNameStr)
        : wxWindow(owner, wxID_ANY,
                   wxDefaultPosition, wxDefaultSize,
                   wxBORDER_NONE | additionalStyle,
                   name)
    {
        m_owner = owner;
    }

    virtual wxWindow *GetMainWindowOfCompositeControl() { return m_owner; }

    virtual bool AcceptsFocus() const { return false; }

    wxGrid *GetOwner() { return m_owner; }

protected:
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);

    wxGrid *m_owner;

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxGridSubwindow);
};

class WXDLLIMPEXP_ADV wxGridRowLabelWindow : public wxGridSubwindow
{
public:
    wxGridRowLabelWindow(wxGrid *parent)
      : wxGridSubwindow(parent)
    {
    }


private:
    void OnPaint( wxPaintEvent& event );
    void OnMouseEvent( wxMouseEvent& event );
    void OnMouseWheel( wxMouseEvent& event );

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxGridRowLabelWindow);
};


class WXDLLIMPEXP_ADV wxGridColLabelWindow : public wxGridSubwindow
{
public:
    wxGridColLabelWindow(wxGrid *parent)
        : wxGridSubwindow(parent)
    {
    }


private:
    void OnPaint( wxPaintEvent& event );
    void OnMouseEvent( wxMouseEvent& event );
    void OnMouseWheel( wxMouseEvent& event );

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxGridColLabelWindow);
};


class WXDLLIMPEXP_ADV wxGridCornerLabelWindow : public wxGridSubwindow
{
public:
    wxGridCornerLabelWindow(wxGrid *parent)
        : wxGridSubwindow(parent)
    {
    }

private:
    void OnMouseEvent( wxMouseEvent& event );
    void OnMouseWheel( wxMouseEvent& event );
    void OnPaint( wxPaintEvent& event );

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxGridCornerLabelWindow);
};

class WXDLLIMPEXP_ADV wxGridWindow : public wxGridSubwindow
{
public:
    wxGridWindow(wxGrid *parent)
        : wxGridSubwindow(parent,
                          wxWANTS_CHARS | wxCLIP_CHILDREN,
                          "GridWindow")
    {
    }


    virtual void ScrollWindow( int dx, int dy, const wxRect *rect );

    virtual bool AcceptsFocus() const { return true; }

private:
    void OnPaint( wxPaintEvent &event );
    void OnMouseWheel( wxMouseEvent& event );
    void OnMouseEvent( wxMouseEvent& event );
    void OnKeyDown( wxKeyEvent& );
    void OnKeyUp( wxKeyEvent& );
    void OnChar( wxKeyEvent& );
    void OnEraseBackground( wxEraseEvent& );
    void OnFocus( wxFocusEvent& );

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxGridWindow);
};

// ----------------------------------------------------------------------------
// the internal data representation used by wxGridCellAttrProvider
// ----------------------------------------------------------------------------

// this class stores attributes set for cells
class WXDLLIMPEXP_ADV wxGridCellAttrData
{
public:
    void SetAttr(wxGridCellAttr *attr, int row, int col);
    wxGridCellAttr *GetAttr(int row, int col) const;
    void UpdateAttrRows( size_t pos, int numRows );
    void UpdateAttrCols( size_t pos, int numCols );

private:
    // searches for the attr for given cell, returns wxNOT_FOUND if not found
    int FindIndex(int row, int col) const;

    wxGridCellWithAttrArray m_attrs;
};

// this class stores attributes set for rows or columns
class WXDLLIMPEXP_ADV wxGridRowOrColAttrData
{
public:
    // empty ctor to suppress warnings
    wxGridRowOrColAttrData() {}
    ~wxGridRowOrColAttrData();

    void SetAttr(wxGridCellAttr *attr, int rowOrCol);
    wxGridCellAttr *GetAttr(int rowOrCol) const;
    void UpdateAttrRowsOrCols( size_t pos, int numRowsOrCols );

private:
    wxArrayInt m_rowsOrCols;
    wxArrayAttrs m_attrs;
};

// NB: this is just a wrapper around 3 objects: one which stores cell
//     attributes, and 2 others for row/col ones
class WXDLLIMPEXP_ADV wxGridCellAttrProviderData
{
public:
    wxGridCellAttrData m_cellAttrs;
    wxGridRowOrColAttrData m_rowAttrs,
                           m_colAttrs;
};

// ----------------------------------------------------------------------------
// operations classes abstracting the difference between operating on rows and
// columns
// ----------------------------------------------------------------------------

// This class allows to write a function only once because by using its methods
// it will apply to both columns and rows.
//
// This is an abstract interface definition, the two concrete implementations
// below should be used when working with rows and columns respectively.
class wxGridOperations
{
public:
    // Returns the operations in the other direction, i.e. wxGridRowOperations
    // if this object is a wxGridColumnOperations and vice versa.
    virtual wxGridOperations& Dual() const = 0;

    // Return the number of rows or columns.
    virtual int GetNumberOfLines(const wxGrid *grid) const = 0;

    // Return the selection mode which allows selecting rows or columns.
    virtual wxGrid::wxGridSelectionModes GetSelectionMode() const = 0;

    // Make a wxGridCellCoords from the given components: thisDir is row or
    // column and otherDir is column or row
    virtual wxGridCellCoords MakeCoords(int thisDir, int otherDir) const = 0;

    // Calculate the scrolled position of the given abscissa or ordinate.
    virtual int CalcScrolledPosition(wxGrid *grid, int pos) const = 0;

    // Selects the horizontal or vertical component from the given object.
    virtual int Select(const wxGridCellCoords& coords) const = 0;
    virtual int Select(const wxPoint& pt) const = 0;
    virtual int Select(const wxSize& sz) const = 0;
    virtual int Select(const wxRect& r) const = 0;
    virtual int& Select(wxRect& r) const = 0;

    // Returns width or height of the rectangle
    virtual int& SelectSize(wxRect& r) const = 0;

    // Make a wxSize such that Select() applied to it returns first component
    virtual wxSize MakeSize(int first, int second) const = 0;

    // Sets the row or column component of the given cell coordinates
    virtual void Set(wxGridCellCoords& coords, int line) const = 0;


    // Draws a line parallel to the row or column, i.e. horizontal or vertical:
    // pos is the horizontal or vertical position of the line and start and end
    // are the coordinates of the line extremities in the other direction
    virtual void
        DrawParallelLine(wxDC& dc, int start, int end, int pos) const = 0;

    // Draw a horizontal or vertical line across the given rectangle
    // (this is implemented in terms of above and uses Select() to extract
    // start and end from the given rectangle)
    void DrawParallelLineInRect(wxDC& dc, const wxRect& rect, int pos) const
    {
        const int posStart = Select(rect.GetPosition());
        DrawParallelLine(dc, posStart, posStart + Select(rect.GetSize()), pos);
    }


    // Return the index of the row or column at the given pixel coordinate.
    virtual int
        PosToLine(const wxGrid *grid, int pos, bool clip = false) const = 0;

    // Get the top/left position, in pixels, of the given row or column
    virtual int GetLineStartPos(const wxGrid *grid, int line) const = 0;

    // Get the bottom/right position, in pixels, of the given row or column
    virtual int GetLineEndPos(const wxGrid *grid, int line) const = 0;

    // Get the height/width of the given row/column
    virtual int GetLineSize(const wxGrid *grid, int line) const = 0;

    // Get wxGrid::m_rowBottoms/m_colRights array
    virtual const wxArrayInt& GetLineEnds(const wxGrid *grid) const = 0;

    // Get default height row height or column width
    virtual int GetDefaultLineSize(const wxGrid *grid) const = 0;

    // Return the minimal acceptable row height or column width
    virtual int GetMinimalAcceptableLineSize(const wxGrid *grid) const = 0;

    // Return the minimal row height or column width
    virtual int GetMinimalLineSize(const wxGrid *grid, int line) const = 0;

    // Set the row height or column width
    virtual void SetLineSize(wxGrid *grid, int line, int size) const = 0;

    // Set the row default height or column default width
    virtual void SetDefaultLineSize(wxGrid *grid, int size, bool resizeExisting) const = 0;


    // Return the index of the line at the given position
    //
    // NB: currently this is always identity for the rows as reordering is only
    //     implemented for the lines
    virtual int GetLineAt(const wxGrid *grid, int pos) const = 0;

    // Return the display position of the line with the given index.
    //
    // NB: As GetLineAt(), currently this is always identity for rows.
    virtual int GetLinePos(const wxGrid *grid, int line) const = 0;

    // Return the index of the line just before the given one or wxNOT_FOUND.
    virtual int GetLineBefore(const wxGrid* grid, int line) const = 0;

    // Get the row or column label window
    virtual wxWindow *GetHeaderWindow(wxGrid *grid) const = 0;

    // Get the width or height of the row or column label window
    virtual int GetHeaderWindowSize(wxGrid *grid) const = 0;


    // This class is never used polymorphically but give it a virtual dtor
    // anyhow to suppress g++ complaints about it
    virtual ~wxGridOperations() { }
};

class wxGridRowOperations : public wxGridOperations
{
public:
    virtual wxGridOperations& Dual() const;

    virtual int GetNumberOfLines(const wxGrid *grid) const
        { return grid->GetNumberRows(); }

    virtual wxGrid::wxGridSelectionModes GetSelectionMode() const
        { return wxGrid::wxGridSelectRows; }

    virtual wxGridCellCoords MakeCoords(int thisDir, int otherDir) const
        { return wxGridCellCoords(thisDir, otherDir); }

    virtual int CalcScrolledPosition(wxGrid *grid, int pos) const
        { return grid->CalcScrolledPosition(wxPoint(pos, 0)).x; }

    virtual int Select(const wxGridCellCoords& c) const { return c.GetRow(); }
    virtual int Select(const wxPoint& pt) const { return pt.x; }
    virtual int Select(const wxSize& sz) const { return sz.x; }
    virtual int Select(const wxRect& r) const { return r.x; }
    virtual int& Select(wxRect& r) const { return r.x; }
    virtual int& SelectSize(wxRect& r) const { return r.width; }
    virtual wxSize MakeSize(int first, int second) const
        { return wxSize(first, second); }
    virtual void Set(wxGridCellCoords& coords, int line) const
        { coords.SetRow(line); }

    virtual void DrawParallelLine(wxDC& dc, int start, int end, int pos) const
        { dc.DrawLine(start, pos, end, pos); }

    virtual int PosToLine(const wxGrid *grid, int pos, bool clip = false) const
        { return grid->YToRow(pos, clip); }
    virtual int GetLineStartPos(const wxGrid *grid, int line) const
        { return grid->GetRowTop(line); }
    virtual int GetLineEndPos(const wxGrid *grid, int line) const
        { return grid->GetRowBottom(line); }
    virtual int GetLineSize(const wxGrid *grid, int line) const
        { return grid->GetRowHeight(line); }
    virtual const wxArrayInt& GetLineEnds(const wxGrid *grid) const
        { return grid->m_rowBottoms; }
    virtual int GetDefaultLineSize(const wxGrid *grid) const
        { return grid->GetDefaultRowSize(); }
    virtual int GetMinimalAcceptableLineSize(const wxGrid *grid) const
        { return grid->GetRowMinimalAcceptableHeight(); }
    virtual int GetMinimalLineSize(const wxGrid *grid, int line) const
        { return grid->GetRowMinimalHeight(line); }
    virtual void SetLineSize(wxGrid *grid, int line, int size) const
        { grid->SetRowSize(line, size); }
    virtual void SetDefaultLineSize(wxGrid *grid, int size, bool resizeExisting) const
        {  grid->SetDefaultRowSize(size, resizeExisting); }

    virtual int GetLineAt(const wxGrid * WXUNUSED(grid), int pos) const
        { return pos; } // TODO: implement row reordering
    virtual int GetLinePos(const wxGrid * WXUNUSED(grid), int line) const
        { return line; } // TODO: implement row reordering

    virtual int GetLineBefore(const wxGrid* WXUNUSED(grid), int line) const
        { return line - 1; }

    virtual wxWindow *GetHeaderWindow(wxGrid *grid) const
        { return grid->GetGridRowLabelWindow(); }
    virtual int GetHeaderWindowSize(wxGrid *grid) const
        { return grid->GetRowLabelSize(); }
};

class wxGridColumnOperations : public wxGridOperations
{
public:
    virtual wxGridOperations& Dual() const;

    virtual int GetNumberOfLines(const wxGrid *grid) const
        { return grid->GetNumberCols(); }

    virtual wxGrid::wxGridSelectionModes GetSelectionMode() const
        { return wxGrid::wxGridSelectColumns; }

    virtual wxGridCellCoords MakeCoords(int thisDir, int otherDir) const
        { return wxGridCellCoords(otherDir, thisDir); }

    virtual int CalcScrolledPosition(wxGrid *grid, int pos) const
        { return grid->CalcScrolledPosition(wxPoint(0, pos)).y; }

    virtual int Select(const wxGridCellCoords& c) const { return c.GetCol(); }
    virtual int Select(const wxPoint& pt) const { return pt.y; }
    virtual int Select(const wxSize& sz) const { return sz.y; }
    virtual int Select(const wxRect& r) const { return r.y; }
    virtual int& Select(wxRect& r) const { return r.y; }
    virtual int& SelectSize(wxRect& r) const { return r.height; }
    virtual wxSize MakeSize(int first, int second) const
        { return wxSize(second, first); }
    virtual void Set(wxGridCellCoords& coords, int line) const
        { coords.SetCol(line); }

    virtual void DrawParallelLine(wxDC& dc, int start, int end, int pos) const
        { dc.DrawLine(pos, start, pos, end); }

    virtual int PosToLine(const wxGrid *grid, int pos, bool clip = false) const
        { return grid->XToCol(pos, clip); }
    virtual int GetLineStartPos(const wxGrid *grid, int line) const
        { return grid->GetColLeft(line); }
    virtual int GetLineEndPos(const wxGrid *grid, int line) const
        { return grid->GetColRight(line); }
    virtual int GetLineSize(const wxGrid *grid, int line) const
        { return grid->GetColWidth(line); }
    virtual const wxArrayInt& GetLineEnds(const wxGrid *grid) const
        { return grid->m_colRights; }
    virtual int GetDefaultLineSize(const wxGrid *grid) const
        { return grid->GetDefaultColSize(); }
    virtual int GetMinimalAcceptableLineSize(const wxGrid *grid) const
        { return grid->GetColMinimalAcceptableWidth(); }
    virtual int GetMinimalLineSize(const wxGrid *grid, int line) const
        { return grid->GetColMinimalWidth(line); }
    virtual void SetLineSize(wxGrid *grid, int line, int size) const
        { grid->SetColSize(line, size); }
    virtual void SetDefaultLineSize(wxGrid *grid, int size, bool resizeExisting) const
        {  grid->SetDefaultColSize(size, resizeExisting); }

    virtual int GetLineAt(const wxGrid *grid, int pos) const
        { return grid->GetColAt(pos); }
    virtual int GetLinePos(const wxGrid *grid, int line) const
        { return grid->GetColPos(line); }

    virtual int GetLineBefore(const wxGrid* grid, int line) const
    {
        int posBefore = grid->GetColPos(line) - 1;
        return posBefore >= 0 ? grid->GetColAt(posBefore) : wxNOT_FOUND;
    }

    virtual wxWindow *GetHeaderWindow(wxGrid *grid) const
        { return grid->GetGridColLabelWindow(); }
    virtual int GetHeaderWindowSize(wxGrid *grid) const
        { return grid->GetColLabelSize(); }
};

// This class abstracts the difference between operations going forward
// (down/right) and backward (up/left) and allows to use the same code for
// functions which differ only in the direction of grid traversal.
//
// Notice that all operations in this class work with display positions and not
// internal indices which can be different if the columns were reordered.
//
// Like wxGridOperations it's an ABC with two concrete subclasses below. Unlike
// it, this is a normal object and not just a function dispatch table and has a
// non-default ctor.
//
// Note: the explanation of this discrepancy is the existence of (very useful)
// Dual() method in wxGridOperations which forces us to make wxGridOperations a
// function dispatcher only.
class wxGridDirectionOperations
{
public:
    // The oper parameter to ctor selects whether we work with rows or columns
    wxGridDirectionOperations(wxGrid *grid, const wxGridOperations& oper)
        : m_grid(grid),
          m_oper(oper)
    {
    }

    // Check if the component of this point in our direction is at the
    // boundary, i.e. is the first/last row/column
    virtual bool IsAtBoundary(const wxGridCellCoords& coords) const = 0;

    // Increment the component of this point in our direction
    virtual void Advance(wxGridCellCoords& coords) const = 0;

    // Find the line at the given distance, in pixels, away from this one
    // (this uses clipping, i.e. anything after the last line is counted as the
    // last one and anything before the first one as 0)
    //
    // TODO: Implementation of this method currently doesn't support column
    //       reordering as it mixes up indices and positions. But this doesn't
    //       really matter as it's only called for rows (Page Up/Down only work
    //       vertically) and row reordering is not currently supported. We'd
    //       need to fix it if this ever changes however.
    virtual int MoveByPixelDistance(int line, int distance) const = 0;

    // This class is never used polymorphically but give it a virtual dtor
    // anyhow to suppress g++ complaints about it
    virtual ~wxGridDirectionOperations() { }

protected:
    // Get the position of the row or column from the given coordinates pair.
    //
    // This is just a shortcut to avoid repeating m_oper and m_grid multiple
    // times in the derived classes code.
    int GetLinePos(const wxGridCellCoords& coords) const
    {
        return m_oper.GetLinePos(m_grid, m_oper.Select(coords));
    }

    // Get the index of the row or column from the position.
    int GetLineAt(int pos) const
    {
        return m_oper.GetLineAt(m_grid, pos);
    }

    // Check if the given line is visible, i.e. has non 0 size.
    bool IsLineVisible(int line) const
    {
        return m_oper.GetLineSize(m_grid, line) != 0;
    }


    wxGrid * const m_grid;
    const wxGridOperations& m_oper;
};

class wxGridBackwardOperations : public wxGridDirectionOperations
{
public:
    wxGridBackwardOperations(wxGrid *grid, const wxGridOperations& oper)
        : wxGridDirectionOperations(grid, oper)
    {
    }

    virtual bool IsAtBoundary(const wxGridCellCoords& coords) const
    {
        wxASSERT_MSG( m_oper.Select(coords) >= 0, "invalid row/column" );

        int pos = GetLinePos(coords);
        while ( pos )
        {
            // Check the previous line.
            int line = GetLineAt(--pos);
            if ( IsLineVisible(line) )
            {
                // There is another visible line before this one, hence it's
                // not at boundary.
                return false;
            }
        }

        // We reached the boundary without finding any visible lines.
        return true;
    }

    virtual void Advance(wxGridCellCoords& coords) const
    {
        int pos = GetLinePos(coords);
        for ( ;; )
        {
            // This is not supposed to happen if IsAtBoundary() returned false.
            wxCHECK_RET( pos, "can't advance when already at boundary" );

            int line = GetLineAt(--pos);
            if ( IsLineVisible(line) )
            {
                m_oper.Set(coords, line);
                break;
            }
        }
    }

    virtual int MoveByPixelDistance(int line, int distance) const
    {
        int pos = m_oper.GetLineStartPos(m_grid, line);
        return m_oper.PosToLine(m_grid, pos - distance + 1, true);
    }
};

// Please refer to the comments above when reading this class code, it's
// absolutely symmetrical to wxGridBackwardOperations.
class wxGridForwardOperations : public wxGridDirectionOperations
{
public:
    wxGridForwardOperations(wxGrid *grid, const wxGridOperations& oper)
        : wxGridDirectionOperations(grid, oper),
          m_numLines(oper.GetNumberOfLines(grid))
    {
    }

    virtual bool IsAtBoundary(const wxGridCellCoords& coords) const
    {
        wxASSERT_MSG( m_oper.Select(coords) < m_numLines, "invalid row/column" );

        int pos = GetLinePos(coords);
        while ( pos < m_numLines - 1 )
        {
            int line = GetLineAt(++pos);
            if ( IsLineVisible(line) )
                return false;
        }

        return true;
    }

    virtual void Advance(wxGridCellCoords& coords) const
    {
        int pos = GetLinePos(coords);
        for ( ;; )
        {
            wxCHECK_RET( pos < m_numLines - 1,
                         "can't advance when already at boundary" );

            int line = GetLineAt(++pos);
            if ( IsLineVisible(line) )
            {
                m_oper.Set(coords, line);
                break;
            }
        }
    }

    virtual int MoveByPixelDistance(int line, int distance) const
    {
        int pos = m_oper.GetLineStartPos(m_grid, line);
        return m_oper.PosToLine(m_grid, pos + distance, true);
    }

private:
    const int m_numLines;
};

// ----------------------------------------------------------------------------
// data structures used for the data type registry
// ----------------------------------------------------------------------------

struct wxGridDataTypeInfo
{
    wxGridDataTypeInfo(const wxString& typeName,
                       wxGridCellRenderer* renderer,
                       wxGridCellEditor* editor)
        : m_typeName(typeName), m_renderer(renderer), m_editor(editor)
        {}

    ~wxGridDataTypeInfo()
    {
        wxSafeDecRef(m_renderer);
        wxSafeDecRef(m_editor);
    }

    wxString            m_typeName;
    wxGridCellRenderer* m_renderer;
    wxGridCellEditor*   m_editor;

    wxDECLARE_NO_COPY_CLASS(wxGridDataTypeInfo);
};


WX_DEFINE_ARRAY_WITH_DECL_PTR(wxGridDataTypeInfo*, wxGridDataTypeInfoArray,
                                 class WXDLLIMPEXP_ADV);


class WXDLLIMPEXP_ADV wxGridTypeRegistry
{
public:
    wxGridTypeRegistry() {}
    ~wxGridTypeRegistry();

    void RegisterDataType(const wxString& typeName,
                     wxGridCellRenderer* renderer,
                     wxGridCellEditor* editor);

    // find one of already registered data types
    int FindRegisteredDataType(const wxString& typeName);

    // try to FindRegisteredDataType(), if this fails and typeName is one of
    // standard typenames, register it and return its index
    int FindDataType(const wxString& typeName);

    // try to FindDataType(), if it fails see if it is not one of already
    // registered data types with some params in which case clone the
    // registered data type and set params for it
    int FindOrCloneDataType(const wxString& typeName);

    wxGridCellRenderer* GetRenderer(int index);
    wxGridCellEditor*   GetEditor(int index);

private:
    wxGridDataTypeInfoArray m_typeinfo;
};

#endif // wxUSE_GRID
#endif // _WX_GENERIC_GRID_PRIVATE_H_
