///////////////////////////////////////////////////////////////////////////
// Name:        src/generic/grid.cpp
// Purpose:     wxGrid and related classes
// Author:      Michael Bedward (based on code by Julian Smart, Robin Dunn)
// Modified by: Robin Dunn, Vadim Zeitlin, Santiago Palacios
// Created:     1/08/1999
// Copyright:   (c) Michael Bedward (mbedward@ozemail.com.au)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

/*
    TODO:

    - Replace use of wxINVERT with wxOverlay
    - Make Begin/EndBatch() the same as the generic Freeze/Thaw()
    - Review the column reordering code, it's a mess.
    - Implement row reordering after dealing with the columns.
 */

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_GRID

#include "wx/grid.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
    #include "wx/log.h"
    #include "wx/textctrl.h"
    #include "wx/checkbox.h"
    #include "wx/combobox.h"
    #include "wx/valtext.h"
    #include "wx/intl.h"
    #include "wx/math.h"
    #include "wx/listbox.h"
#endif

#include "wx/textfile.h"
#include "wx/spinctrl.h"
#include "wx/tokenzr.h"
#include "wx/renderer.h"
#include "wx/headerctrl.h"
#include "wx/hashset.h"

#include "wx/generic/gridsel.h"
#include "wx/generic/gridctrl.h"
#include "wx/generic/grideditors.h"
#include "wx/generic/private/grid.h"

const char wxGridNameStr[] = "grid";

#if defined(__WXMOTIF__)
    #define WXUNUSED_MOTIF(identifier)  WXUNUSED(identifier)
#else
    #define WXUNUSED_MOTIF(identifier)  identifier
#endif

#if defined(__WXGTK__)
    #define WXUNUSED_GTK(identifier)    WXUNUSED(identifier)
#else
    #define WXUNUSED_GTK(identifier)    identifier
#endif

// Required for wxIs... functions
#include <ctype.h>

WX_DECLARE_HASH_SET_WITH_DECL_PTR(int, wxIntegerHash, wxIntegerEqual,
                                  wxGridFixedIndicesSet, class WXDLLIMPEXP_ADV);


// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

namespace
{

//#define DEBUG_ATTR_CACHE
#ifdef DEBUG_ATTR_CACHE
    static size_t gs_nAttrCacheHits = 0;
    static size_t gs_nAttrCacheMisses = 0;
#endif

// this struct simply combines together the default header renderers
//
// as the renderers ctors are trivial, there is no problem with making them
// globals
struct DefaultHeaderRenderers
{
    wxGridColumnHeaderRendererDefault colRenderer;
    wxGridRowHeaderRendererDefault rowRenderer;
    wxGridCornerHeaderRendererDefault cornerRenderer;
} gs_defaultHeaderRenderers;

} // anonymous namespace

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

wxGridCellCoords wxGridNoCellCoords( -1, -1 );
wxRect wxGridNoCellRect( -1, -1, -1, -1 );

namespace
{

// scroll line size
const size_t GRID_SCROLL_LINE_X = 15;
const size_t GRID_SCROLL_LINE_Y = GRID_SCROLL_LINE_X;

// the size of hash tables used a bit everywhere (the max number of elements
// in these hash tables is the number of rows/columns)
const int GRID_HASH_SIZE = 100;

// the minimal distance in pixels the mouse needs to move to start a drag
// operation
const int DRAG_SENSITIVITY = 3;

} // anonymous namespace

#include "wx/arrimpl.cpp"

WX_DEFINE_OBJARRAY(wxGridCellCoordsArray)
WX_DEFINE_OBJARRAY(wxGridCellWithAttrArray)

// ----------------------------------------------------------------------------
// events
// ----------------------------------------------------------------------------

wxDEFINE_EVENT( wxEVT_GRID_CELL_LEFT_CLICK, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_CELL_RIGHT_CLICK, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_CELL_LEFT_DCLICK, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_CELL_RIGHT_DCLICK, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_CELL_BEGIN_DRAG, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_LABEL_LEFT_CLICK, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_LABEL_RIGHT_CLICK, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_LABEL_LEFT_DCLICK, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_LABEL_RIGHT_DCLICK, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_ROW_SIZE, wxGridSizeEvent );
wxDEFINE_EVENT( wxEVT_GRID_COL_SIZE, wxGridSizeEvent );
wxDEFINE_EVENT( wxEVT_GRID_COL_AUTO_SIZE, wxGridSizeEvent );
wxDEFINE_EVENT( wxEVT_GRID_COL_MOVE, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_COL_SORT, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_RANGE_SELECT, wxGridRangeSelectEvent );
wxDEFINE_EVENT( wxEVT_GRID_CELL_CHANGING, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_CELL_CHANGED, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_SELECT_CELL, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_EDITOR_SHOWN, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_EDITOR_HIDDEN, wxGridEvent );
wxDEFINE_EVENT( wxEVT_GRID_EDITOR_CREATED, wxGridEditorCreatedEvent );
wxDEFINE_EVENT( wxEVT_GRID_TABBING, wxGridEvent );

// ----------------------------------------------------------------------------
// private helpers
// ----------------------------------------------------------------------------

namespace
{
    
    // ensure that first is less or equal to second, swapping the values if
    // necessary
    void EnsureFirstLessThanSecond(int& first, int& second)
    {
        if ( first > second )
            wxSwap(first, second);
    }
    
} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_ABSTRACT_CLASS(wxGridCellEditorEvtHandler, wxEvtHandler)

BEGIN_EVENT_TABLE( wxGridCellEditorEvtHandler, wxEvtHandler )
    EVT_KILL_FOCUS( wxGridCellEditorEvtHandler::OnKillFocus )
    EVT_KEY_DOWN( wxGridCellEditorEvtHandler::OnKeyDown )
    EVT_CHAR( wxGridCellEditorEvtHandler::OnChar )
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxGridHeaderCtrl, wxHeaderCtrl)
    EVT_HEADER_CLICK(wxID_ANY, wxGridHeaderCtrl::OnClick)
    EVT_HEADER_DCLICK(wxID_ANY, wxGridHeaderCtrl::OnDoubleClick)
    EVT_HEADER_RIGHT_CLICK(wxID_ANY, wxGridHeaderCtrl::OnRightClick)

    EVT_HEADER_BEGIN_RESIZE(wxID_ANY, wxGridHeaderCtrl::OnBeginResize)
    EVT_HEADER_RESIZING(wxID_ANY, wxGridHeaderCtrl::OnResizing)
    EVT_HEADER_END_RESIZE(wxID_ANY, wxGridHeaderCtrl::OnEndResize)

    EVT_HEADER_BEGIN_REORDER(wxID_ANY, wxGridHeaderCtrl::OnBeginReorder)
    EVT_HEADER_END_REORDER(wxID_ANY, wxGridHeaderCtrl::OnEndReorder)
END_EVENT_TABLE()

wxGridOperations& wxGridRowOperations::Dual() const
{
    static wxGridColumnOperations s_colOper;

    return s_colOper;
}

wxGridOperations& wxGridColumnOperations::Dual() const
{
    static wxGridRowOperations s_rowOper;

    return s_rowOper;
}

// ----------------------------------------------------------------------------
// wxGridCellWorker is an (almost) empty common base class for
// wxGridCellRenderer and wxGridCellEditor managing ref counting
// ----------------------------------------------------------------------------

void wxGridCellWorker::SetParameters(const wxString& WXUNUSED(params))
{
    // nothing to do
}

wxGridCellWorker::~wxGridCellWorker()
{
}

// ----------------------------------------------------------------------------
// wxGridHeaderLabelsRenderer and related classes
// ----------------------------------------------------------------------------

void wxGridHeaderLabelsRenderer::DrawLabel(const wxGrid& grid,
                                           wxDC& dc,
                                           const wxString& value,
                                           const wxRect& rect,
                                           int horizAlign,
                                           int vertAlign,
                                           int textOrientation) const
{
    dc.SetBackgroundMode(wxBRUSHSTYLE_TRANSPARENT);
    dc.SetTextForeground(grid.GetLabelTextColour());
    dc.SetFont(grid.GetLabelFont());
    grid.DrawTextRectangle(dc, value, rect, horizAlign, vertAlign, textOrientation);
}


void wxGridRowHeaderRendererDefault::DrawBorder(const wxGrid& WXUNUSED(grid),
                                                wxDC& dc,
                                                wxRect& rect) const
{
    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW)));
    dc.DrawLine(rect.GetRight(), rect.GetTop(),
                rect.GetRight(), rect.GetBottom());
    dc.DrawLine(rect.GetLeft(), rect.GetTop(),
                rect.GetLeft(), rect.GetBottom());
    dc.DrawLine(rect.GetLeft(), rect.GetBottom(),
                rect.GetRight() + 1, rect.GetBottom());

    dc.SetPen(*wxWHITE_PEN);
    dc.DrawLine(rect.GetLeft() + 1, rect.GetTop(),
                rect.GetLeft() + 1, rect.GetBottom());
    dc.DrawLine(rect.GetLeft() + 1, rect.GetTop(),
                rect.GetRight(), rect.GetTop());

    rect.Deflate(2);
}

void wxGridColumnHeaderRendererDefault::DrawBorder(const wxGrid& WXUNUSED(grid),
                                                   wxDC& dc,
                                                   wxRect& rect) const
{
    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW)));
    dc.DrawLine(rect.GetRight(), rect.GetTop(),
                rect.GetRight(), rect.GetBottom());
    dc.DrawLine(rect.GetLeft(), rect.GetTop(),
                rect.GetRight(), rect.GetTop());
    dc.DrawLine(rect.GetLeft(), rect.GetBottom(),
                rect.GetRight() + 1, rect.GetBottom());

    dc.SetPen(*wxWHITE_PEN);
    dc.DrawLine(rect.GetLeft(), rect.GetTop() + 1,
                rect.GetLeft(), rect.GetBottom());
    dc.DrawLine(rect.GetLeft(), rect.GetTop() + 1,
                rect.GetRight(), rect.GetTop() + 1);

    rect.Deflate(2);
}

void wxGridCornerHeaderRendererDefault::DrawBorder(const wxGrid& WXUNUSED(grid),
                                                   wxDC& dc,
                                                   wxRect& rect) const
{
    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW)));
    dc.DrawLine(rect.GetRight() - 1, rect.GetBottom() - 1,
                rect.GetRight() - 1, rect.GetTop());
    dc.DrawLine(rect.GetRight() - 1, rect.GetBottom() - 1,
                rect.GetLeft(), rect.GetBottom() - 1);
    dc.DrawLine(rect.GetLeft(), rect.GetTop(),
                rect.GetRight(), rect.GetTop());
    dc.DrawLine(rect.GetLeft(), rect.GetTop(),
                rect.GetLeft(), rect.GetBottom());

    dc.SetPen(*wxWHITE_PEN);
    dc.DrawLine(rect.GetLeft() + 1, rect.GetTop() + 1,
                rect.GetRight() - 1, rect.GetTop() + 1);
    dc.DrawLine(rect.GetLeft() + 1, rect.GetTop() + 1,
                rect.GetLeft() + 1, rect.GetBottom() - 1);

    rect.Deflate(2);
}

// ----------------------------------------------------------------------------
// wxGridCellAttr
// ----------------------------------------------------------------------------

void wxGridCellAttr::Init(wxGridCellAttr *attrDefault)
{
    m_isReadOnly = Unset;

    m_renderer = NULL;
    m_editor = NULL;

    m_attrkind = wxGridCellAttr::Cell;

    m_sizeRows = m_sizeCols = 1;
    m_overflow = UnsetOverflow;

    SetDefAttr(attrDefault);
}

wxGridCellAttr *wxGridCellAttr::Clone() const
{
    wxGridCellAttr *attr = new wxGridCellAttr(m_defGridAttr);

    if ( HasTextColour() )
        attr->SetTextColour(GetTextColour());
    if ( HasBackgroundColour() )
        attr->SetBackgroundColour(GetBackgroundColour());
    if ( HasFont() )
        attr->SetFont(GetFont());
    if ( HasAlignment() )
        attr->SetAlignment(m_hAlign, m_vAlign);

    attr->SetSize( m_sizeRows, m_sizeCols );

    if ( m_renderer )
    {
        attr->SetRenderer(m_renderer);
        m_renderer->IncRef();
    }
    if ( m_editor )
    {
        attr->SetEditor(m_editor);
        m_editor->IncRef();
    }

    if ( IsReadOnly() )
        attr->SetReadOnly();

    attr->SetOverflow( m_overflow == Overflow );
    attr->SetKind( m_attrkind );

    return attr;
}

void wxGridCellAttr::MergeWith(wxGridCellAttr *mergefrom)
{
    if ( !HasTextColour() && mergefrom->HasTextColour() )
        SetTextColour(mergefrom->GetTextColour());
    if ( !HasBackgroundColour() && mergefrom->HasBackgroundColour() )
        SetBackgroundColour(mergefrom->GetBackgroundColour());
    if ( !HasFont() && mergefrom->HasFont() )
        SetFont(mergefrom->GetFont());
    if ( !HasAlignment() && mergefrom->HasAlignment() )
    {
        int hAlign, vAlign;
        mergefrom->GetAlignment( &hAlign, &vAlign);
        SetAlignment(hAlign, vAlign);
    }
    if ( !HasSize() && mergefrom->HasSize() )
        mergefrom->GetSize( &m_sizeRows, &m_sizeCols );

    // Directly access member functions as GetRender/Editor don't just return
    // m_renderer/m_editor
    //
    // Maybe add support for merge of Render and Editor?
    if (!HasRenderer() && mergefrom->HasRenderer() )
    {
        m_renderer = mergefrom->m_renderer;
        m_renderer->IncRef();
    }
    if ( !HasEditor() && mergefrom->HasEditor() )
    {
        m_editor =  mergefrom->m_editor;
        m_editor->IncRef();
    }
    if ( !HasReadWriteMode() && mergefrom->HasReadWriteMode() )
        SetReadOnly(mergefrom->IsReadOnly());

    if (!HasOverflowMode() && mergefrom->HasOverflowMode() )
        SetOverflow(mergefrom->GetOverflow());

    SetDefAttr(mergefrom->m_defGridAttr);
}

void wxGridCellAttr::SetSize(int num_rows, int num_cols)
{
    // The size of a cell is normally 1,1

    // If this cell is larger (2,2) then this is the top left cell
    // the other cells that will be covered (lower right cells) must be
    // set to negative or zero values such that
    // row + num_rows of the covered cell points to the larger cell (this cell)
    // same goes for the col + num_cols.

    // Size of 0,0 is NOT valid, neither is <=0 and any positive value

    wxASSERT_MSG( (!((num_rows > 0) && (num_cols <= 0)) ||
                  !((num_rows <= 0) && (num_cols > 0)) ||
                  !((num_rows == 0) && (num_cols == 0))),
                  wxT("wxGridCellAttr::SetSize only takes two positive values or negative/zero values"));

    m_sizeRows = num_rows;
    m_sizeCols = num_cols;
}

const wxColour& wxGridCellAttr::GetTextColour() const
{
    if (HasTextColour())
    {
        return m_colText;
    }
    else if (m_defGridAttr && m_defGridAttr != this)
    {
        return m_defGridAttr->GetTextColour();
    }
    else
    {
        wxFAIL_MSG(wxT("Missing default cell attribute"));
        return wxNullColour;
    }
}

const wxColour& wxGridCellAttr::GetBackgroundColour() const
{
    if (HasBackgroundColour())
    {
        return m_colBack;
    }
    else if (m_defGridAttr && m_defGridAttr != this)
    {
        return m_defGridAttr->GetBackgroundColour();
    }
    else
    {
        wxFAIL_MSG(wxT("Missing default cell attribute"));
        return wxNullColour;
    }
}

const wxFont& wxGridCellAttr::GetFont() const
{
    if (HasFont())
    {
        return m_font;
    }
    else if (m_defGridAttr && m_defGridAttr != this)
    {
        return m_defGridAttr->GetFont();
    }
    else
    {
        wxFAIL_MSG(wxT("Missing default cell attribute"));
        return wxNullFont;
    }
}

void wxGridCellAttr::GetAlignment(int *hAlign, int *vAlign) const
{
    if (HasAlignment())
    {
        if ( hAlign )
            *hAlign = m_hAlign;
        if ( vAlign )
            *vAlign = m_vAlign;
    }
    else if (m_defGridAttr && m_defGridAttr != this)
    {
        m_defGridAttr->GetAlignment(hAlign, vAlign);
    }
    else
    {
        wxFAIL_MSG(wxT("Missing default cell attribute"));
    }
}

void wxGridCellAttr::GetNonDefaultAlignment(int *hAlign, int *vAlign) const
{
    if ( hAlign && m_hAlign != wxALIGN_INVALID )
        *hAlign = m_hAlign;

    if ( vAlign && m_vAlign != wxALIGN_INVALID )
        *vAlign = m_vAlign;
}

void wxGridCellAttr::GetSize( int *num_rows, int *num_cols ) const
{
    if ( num_rows )
        *num_rows = m_sizeRows;
    if ( num_cols )
        *num_cols = m_sizeCols;
}

// GetRenderer and GetEditor use a slightly different decision path about
// which attribute to use.  If a non-default attr object has one then it is
// used, otherwise the default editor or renderer is fetched from the grid and
// used.  It should be the default for the data type of the cell.  If it is
// NULL (because the table has a type that the grid does not have in its
// registry), then the grid's default editor or renderer is used.

wxGridCellRenderer* wxGridCellAttr::GetRenderer(const wxGrid* grid, int row, int col) const
{
    wxGridCellRenderer *renderer = NULL;

    if ( m_renderer && this != m_defGridAttr )
    {
        // use the cells renderer if it has one
        renderer = m_renderer;
        renderer->IncRef();
    }
    else // no non-default cell renderer
    {
        // get default renderer for the data type
        if ( grid )
        {
            // GetDefaultRendererForCell() will do IncRef() for us
            renderer = grid->GetDefaultRendererForCell(row, col);
        }

        if ( renderer == NULL )
        {
            if ( (m_defGridAttr != NULL) && (m_defGridAttr != this) )
            {
                // if we still don't have one then use the grid default
                // (no need for IncRef() here neither)
                renderer = m_defGridAttr->GetRenderer(NULL, 0, 0);
            }
            else // default grid attr
            {
                // use m_renderer which we had decided not to use initially
                renderer = m_renderer;
                if ( renderer )
                    renderer->IncRef();
            }
        }
    }

    // we're supposed to always find something
    wxASSERT_MSG(renderer, wxT("Missing default cell renderer"));

    return renderer;
}

// same as above, except for s/renderer/editor/g
wxGridCellEditor* wxGridCellAttr::GetEditor(const wxGrid* grid, int row, int col) const
{
    wxGridCellEditor *editor = NULL;

    if ( m_editor && this != m_defGridAttr )
    {
        // use the cells editor if it has one
        editor = m_editor;
        editor->IncRef();
    }
    else // no non default cell editor
    {
        // get default editor for the data type
        if ( grid )
        {
            // GetDefaultEditorForCell() will do IncRef() for us
            editor = grid->GetDefaultEditorForCell(row, col);
        }

        if ( editor == NULL )
        {
            if ( (m_defGridAttr != NULL) && (m_defGridAttr != this) )
            {
                // if we still don't have one then use the grid default
                // (no need for IncRef() here neither)
                editor = m_defGridAttr->GetEditor(NULL, 0, 0);
            }
            else // default grid attr
            {
                // use m_editor which we had decided not to use initially
                editor = m_editor;
                if ( editor )
                    editor->IncRef();
            }
        }
    }

    // we're supposed to always find something
    wxASSERT_MSG(editor, wxT("Missing default cell editor"));

    return editor;
}

// ----------------------------------------------------------------------------
// wxGridCellAttrData
// ----------------------------------------------------------------------------

void wxGridCellAttrData::SetAttr(wxGridCellAttr *attr, int row, int col)
{
    // Note: contrary to wxGridRowOrColAttrData::SetAttr, we must not
    //       touch attribute's reference counting explicitly, since this
    //       is managed by class wxGridCellWithAttr
    int n = FindIndex(row, col);
    if ( n == wxNOT_FOUND )
    {
        if ( attr )
        {
            // add the attribute
            m_attrs.Add(new wxGridCellWithAttr(row, col, attr));
        }
        //else: nothing to do
    }
    else // we already have an attribute for this cell
    {
        if ( attr )
        {
            // change the attribute
            m_attrs[(size_t)n].ChangeAttr(attr);
        }
        else
        {
            // remove this attribute
            m_attrs.RemoveAt((size_t)n);
        }
    }
}

wxGridCellAttr *wxGridCellAttrData::GetAttr(int row, int col) const
{
    wxGridCellAttr *attr = NULL;

    int n = FindIndex(row, col);
    if ( n != wxNOT_FOUND )
    {
        attr = m_attrs[(size_t)n].attr;
        attr->IncRef();
    }

    return attr;
}

void wxGridCellAttrData::UpdateAttrRows( size_t pos, int numRows )
{
    size_t count = m_attrs.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        wxGridCellCoords& coords = m_attrs[n].coords;
        wxCoord row = coords.GetRow();
        if ((size_t)row >= pos)
        {
            if (numRows > 0)
            {
                // If rows inserted, include row counter where necessary
                coords.SetRow(row + numRows);
            }
            else if (numRows < 0)
            {
                // If rows deleted ...
                if ((size_t)row >= pos - numRows)
                {
                    // ...either decrement row counter (if row still exists)...
                    coords.SetRow(row + numRows);
                }
                else
                {
                    // ...or remove the attribute
                    m_attrs.RemoveAt(n);
                    n--;
                    count--;
                }
            }
        }
    }
}

void wxGridCellAttrData::UpdateAttrCols( size_t pos, int numCols )
{
    size_t count = m_attrs.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        wxGridCellCoords& coords = m_attrs[n].coords;
        wxCoord col = coords.GetCol();
        if ( (size_t)col >= pos )
        {
            if ( numCols > 0 )
            {
                // If rows inserted, include row counter where necessary
                coords.SetCol(col + numCols);
            }
            else if (numCols < 0)
            {
                // If rows deleted ...
                if ((size_t)col >= pos - numCols)
                {
                    // ...either decrement row counter (if row still exists)...
                    coords.SetCol(col + numCols);
                }
                else
                {
                    // ...or remove the attribute
                    m_attrs.RemoveAt(n);
                    n--;
                    count--;
                }
            }
        }
    }
}

int wxGridCellAttrData::FindIndex(int row, int col) const
{
    size_t count = m_attrs.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        const wxGridCellCoords& coords = m_attrs[n].coords;
        if ( (coords.GetRow() == row) && (coords.GetCol() == col) )
        {
            return n;
        }
    }

    return wxNOT_FOUND;
}

// ----------------------------------------------------------------------------
// wxGridRowOrColAttrData
// ----------------------------------------------------------------------------

wxGridRowOrColAttrData::~wxGridRowOrColAttrData()
{
    size_t count = m_attrs.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        m_attrs[n]->DecRef();
    }
}

wxGridCellAttr *wxGridRowOrColAttrData::GetAttr(int rowOrCol) const
{
    wxGridCellAttr *attr = NULL;

    int n = m_rowsOrCols.Index(rowOrCol);
    if ( n != wxNOT_FOUND )
    {
        attr = m_attrs[(size_t)n];
        attr->IncRef();
    }

    return attr;
}

void wxGridRowOrColAttrData::SetAttr(wxGridCellAttr *attr, int rowOrCol)
{
    int i = m_rowsOrCols.Index(rowOrCol);
    if ( i == wxNOT_FOUND )
    {
        if ( attr )
        {
            // store the new attribute, taking its ownership
            m_rowsOrCols.Add(rowOrCol);
            m_attrs.Add(attr);
        }
        // nothing to remove
    }
    else // we have an attribute for this row or column
    {
        size_t n = (size_t)i;

        // notice that this code works correctly even when the old attribute is
        // the same as the new one: as we own of it, we must call DecRef() on
        // it in any case and this won't result in destruction of the new
        // attribute if it's the same as old one because it must have ref count
        // of at least 2 to be passed to us while we keep a reference to it too
        m_attrs[n]->DecRef();

        if ( attr )
        {
            // replace the attribute with the new one
            m_attrs[n] = attr;
        }
        else // remove the attribute
        {
            m_rowsOrCols.RemoveAt(n);
            m_attrs.RemoveAt(n);
        }
    }
}

void wxGridRowOrColAttrData::UpdateAttrRowsOrCols( size_t pos, int numRowsOrCols )
{
    size_t count = m_attrs.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        int & rowOrCol = m_rowsOrCols[n];
        if ( (size_t)rowOrCol >= pos )
        {
            if ( numRowsOrCols > 0 )
            {
                // If rows inserted, include row counter where necessary
                rowOrCol += numRowsOrCols;
            }
            else if ( numRowsOrCols < 0)
            {
                // If rows deleted, either decrement row counter (if row still exists)
                if ((size_t)rowOrCol >= pos - numRowsOrCols)
                    rowOrCol += numRowsOrCols;
                else
                {
                    m_rowsOrCols.RemoveAt(n);
                    m_attrs[n]->DecRef();
                    m_attrs.RemoveAt(n);
                    n--;
                    count--;
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// wxGridCellAttrProvider
// ----------------------------------------------------------------------------

wxGridCellAttrProvider::wxGridCellAttrProvider()
{
    m_data = NULL;
}

wxGridCellAttrProvider::~wxGridCellAttrProvider()
{
    delete m_data;
}

void wxGridCellAttrProvider::InitData()
{
    m_data = new wxGridCellAttrProviderData;
}

wxGridCellAttr *wxGridCellAttrProvider::GetAttr(int row, int col,
                                                wxGridCellAttr::wxAttrKind  kind ) const
{
    wxGridCellAttr *attr = NULL;
    if ( m_data )
    {
        switch (kind)
        {
            case (wxGridCellAttr::Any):
                // Get cached merge attributes.
                // Currently not used as no cache implemented as not mutable
                // attr = m_data->m_mergeAttr.GetAttr(row, col);
                if (!attr)
                {
                    // Basically implement old version.
                    // Also check merge cache, so we don't have to re-merge every time..
                    wxGridCellAttr *attrcell = m_data->m_cellAttrs.GetAttr(row, col);
                    wxGridCellAttr *attrrow = m_data->m_rowAttrs.GetAttr(row);
                    wxGridCellAttr *attrcol = m_data->m_colAttrs.GetAttr(col);

                    if ((attrcell != attrrow) && (attrrow != attrcol) && (attrcell != attrcol))
                    {
                        // Two or more are non NULL
                        attr = new wxGridCellAttr;
                        attr->SetKind(wxGridCellAttr::Merged);

                        // Order is important..
                        if (attrcell)
                        {
                            attr->MergeWith(attrcell);
                            attrcell->DecRef();
                        }
                        if (attrcol)
                        {
                            attr->MergeWith(attrcol);
                            attrcol->DecRef();
                        }
                        if (attrrow)
                        {
                            attr->MergeWith(attrrow);
                            attrrow->DecRef();
                        }

                        // store merge attr if cache implemented
                        //attr->IncRef();
                        //m_data->m_mergeAttr.SetAttr(attr, row, col);
                    }
                    else
                    {
                        // one or none is non null return it or null.
                        if (attrrow)
                            attr = attrrow;
                        if (attrcol)
                        {
                            if (attr)
                                attr->DecRef();
                            attr = attrcol;
                        }
                        if (attrcell)
                        {
                            if (attr)
                                attr->DecRef();
                            attr = attrcell;
                        }
                    }
                }
                break;

            case (wxGridCellAttr::Cell):
                attr = m_data->m_cellAttrs.GetAttr(row, col);
                break;

            case (wxGridCellAttr::Col):
                attr = m_data->m_colAttrs.GetAttr(col);
                break;

            case (wxGridCellAttr::Row):
                attr = m_data->m_rowAttrs.GetAttr(row);
                break;

            default:
                // unused as yet...
                // (wxGridCellAttr::Default):
                // (wxGridCellAttr::Merged):
                break;
        }
    }

    return attr;
}

void wxGridCellAttrProvider::SetAttr(wxGridCellAttr *attr,
                                     int row, int col)
{
    if ( !m_data )
        InitData();

    m_data->m_cellAttrs.SetAttr(attr, row, col);
}

void wxGridCellAttrProvider::SetRowAttr(wxGridCellAttr *attr, int row)
{
    if ( !m_data )
        InitData();

    m_data->m_rowAttrs.SetAttr(attr, row);
}

void wxGridCellAttrProvider::SetColAttr(wxGridCellAttr *attr, int col)
{
    if ( !m_data )
        InitData();

    m_data->m_colAttrs.SetAttr(attr, col);
}

void wxGridCellAttrProvider::UpdateAttrRows( size_t pos, int numRows )
{
    if ( m_data )
    {
        m_data->m_cellAttrs.UpdateAttrRows( pos, numRows );

        m_data->m_rowAttrs.UpdateAttrRowsOrCols( pos, numRows );
    }
}

void wxGridCellAttrProvider::UpdateAttrCols( size_t pos, int numCols )
{
    if ( m_data )
    {
        m_data->m_cellAttrs.UpdateAttrCols( pos, numCols );

        m_data->m_colAttrs.UpdateAttrRowsOrCols( pos, numCols );
    }
}

const wxGridColumnHeaderRenderer&
wxGridCellAttrProvider::GetColumnHeaderRenderer(int WXUNUSED(col))
{
    return gs_defaultHeaderRenderers.colRenderer;
}

const wxGridRowHeaderRenderer&
wxGridCellAttrProvider::GetRowHeaderRenderer(int WXUNUSED(row))
{
    return gs_defaultHeaderRenderers.rowRenderer;
}

const wxGridCornerHeaderRenderer& wxGridCellAttrProvider::GetCornerRenderer()
{
    return gs_defaultHeaderRenderers.cornerRenderer;
}

// ----------------------------------------------------------------------------
// wxGridTableBase
// ----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS( wxGridTableBase, wxObject )

wxGridTableBase::wxGridTableBase()
{
    m_view = NULL;
    m_attrProvider = NULL;
}

wxGridTableBase::~wxGridTableBase()
{
    delete m_attrProvider;
}

void wxGridTableBase::SetAttrProvider(wxGridCellAttrProvider *attrProvider)
{
    delete m_attrProvider;
    m_attrProvider = attrProvider;
}

bool wxGridTableBase::CanHaveAttributes()
{
    if ( ! GetAttrProvider() )
    {
        // use the default attr provider by default
        SetAttrProvider(new wxGridCellAttrProvider);
    }

    return true;
}

wxGridCellAttr *wxGridTableBase::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind  kind)
{
    if ( m_attrProvider )
        return m_attrProvider->GetAttr(row, col, kind);
    else
        return NULL;
}

void wxGridTableBase::SetAttr(wxGridCellAttr* attr, int row, int col)
{
    if ( m_attrProvider )
    {
        if ( attr )
            attr->SetKind(wxGridCellAttr::Cell);
        m_attrProvider->SetAttr(attr, row, col);
    }
    else
    {
        // as we take ownership of the pointer and don't store it, we must
        // free it now
        wxSafeDecRef(attr);
    }
}

void wxGridTableBase::SetRowAttr(wxGridCellAttr *attr, int row)
{
    if ( m_attrProvider )
    {
        if ( attr )
            attr->SetKind(wxGridCellAttr::Row);
        m_attrProvider->SetRowAttr(attr, row);
    }
    else
    {
        // as we take ownership of the pointer and don't store it, we must
        // free it now
        wxSafeDecRef(attr);
    }
}

void wxGridTableBase::SetColAttr(wxGridCellAttr *attr, int col)
{
    if ( m_attrProvider )
    {
        if ( attr )
            attr->SetKind(wxGridCellAttr::Col);
        m_attrProvider->SetColAttr(attr, col);
    }
    else
    {
        // as we take ownership of the pointer and don't store it, we must
        // free it now
        wxSafeDecRef(attr);
    }
}

bool wxGridTableBase::InsertRows( size_t WXUNUSED(pos),
                                  size_t WXUNUSED(numRows) )
{
    wxFAIL_MSG( wxT("Called grid table class function InsertRows\nbut your derived table class does not override this function") );

    return false;
}

bool wxGridTableBase::AppendRows( size_t WXUNUSED(numRows) )
{
    wxFAIL_MSG( wxT("Called grid table class function AppendRows\nbut your derived table class does not override this function"));

    return false;
}

bool wxGridTableBase::DeleteRows( size_t WXUNUSED(pos),
                                  size_t WXUNUSED(numRows) )
{
    wxFAIL_MSG( wxT("Called grid table class function DeleteRows\nbut your derived table class does not override this function"));

    return false;
}

bool wxGridTableBase::InsertCols( size_t WXUNUSED(pos),
                                  size_t WXUNUSED(numCols) )
{
    wxFAIL_MSG( wxT("Called grid table class function InsertCols\nbut your derived table class does not override this function"));

    return false;
}

bool wxGridTableBase::AppendCols( size_t WXUNUSED(numCols) )
{
    wxFAIL_MSG(wxT("Called grid table class function AppendCols\nbut your derived table class does not override this function"));

    return false;
}

bool wxGridTableBase::DeleteCols( size_t WXUNUSED(pos),
                                  size_t WXUNUSED(numCols) )
{
    wxFAIL_MSG( wxT("Called grid table class function DeleteCols\nbut your derived table class does not override this function"));

    return false;
}

wxString wxGridTableBase::GetRowLabelValue( int row )
{
    wxString s;

    // RD: Starting the rows at zero confuses users,
    // no matter how much it makes sense to us geeks.
    s << row + 1;

    return s;
}

wxString wxGridTableBase::GetColLabelValue( int col )
{
    // default col labels are:
    //   cols 0 to 25   : A-Z
    //   cols 26 to 675 : AA-ZZ
    //   etc.

    wxString s;
    unsigned int i, n;
    for ( n = 1; ; n++ )
    {
        s += (wxChar) (wxT('A') + (wxChar)(col % 26));
        col = col / 26 - 1;
        if ( col < 0 )
            break;
    }

    // reverse the string...
    wxString s2;
    for ( i = 0; i < n; i++ )
    {
        s2 += s[n - i - 1];
    }

    return s2;
}

wxString wxGridTableBase::GetTypeName( int WXUNUSED(row), int WXUNUSED(col) )
{
    return wxGRID_VALUE_STRING;
}

bool wxGridTableBase::CanGetValueAs( int WXUNUSED(row), int WXUNUSED(col),
                                     const wxString& typeName )
{
    return typeName == wxGRID_VALUE_STRING;
}

bool wxGridTableBase::CanSetValueAs( int row, int col, const wxString& typeName )
{
    return CanGetValueAs(row, col, typeName);
}

long wxGridTableBase::GetValueAsLong( int WXUNUSED(row), int WXUNUSED(col) )
{
    return 0;
}

double wxGridTableBase::GetValueAsDouble( int WXUNUSED(row), int WXUNUSED(col) )
{
    return 0.0;
}

bool wxGridTableBase::GetValueAsBool( int WXUNUSED(row), int WXUNUSED(col) )
{
    return false;
}

void wxGridTableBase::SetValueAsLong( int WXUNUSED(row), int WXUNUSED(col),
                                      long WXUNUSED(value) )
{
}

void wxGridTableBase::SetValueAsDouble( int WXUNUSED(row), int WXUNUSED(col),
                                        double WXUNUSED(value) )
{
}

void wxGridTableBase::SetValueAsBool( int WXUNUSED(row), int WXUNUSED(col),
                                      bool WXUNUSED(value) )
{
}

void* wxGridTableBase::GetValueAsCustom( int WXUNUSED(row), int WXUNUSED(col),
                                         const wxString& WXUNUSED(typeName) )
{
    return NULL;
}

void  wxGridTableBase::SetValueAsCustom( int WXUNUSED(row), int WXUNUSED(col),
                                         const wxString& WXUNUSED(typeName),
                                         void* WXUNUSED(value) )
{
}

//////////////////////////////////////////////////////////////////////
//
// Message class for the grid table to send requests and notifications
// to the grid view
//

wxGridTableMessage::wxGridTableMessage()
{
    m_table =  NULL;
    m_id = -1;
    m_comInt1 = -1;
    m_comInt2 = -1;
}

wxGridTableMessage::wxGridTableMessage( wxGridTableBase *table, int id,
                                        int commandInt1, int commandInt2 )
{
    m_table = table;
    m_id = id;
    m_comInt1 = commandInt1;
    m_comInt2 = commandInt2;
}

//////////////////////////////////////////////////////////////////////
//
// A basic grid table for string data. An object of this class will
// created by wxGrid if you don't specify an alternative table class.
//

WX_DEFINE_OBJARRAY(wxGridStringArray)

IMPLEMENT_DYNAMIC_CLASS( wxGridStringTable, wxGridTableBase )

wxGridStringTable::wxGridStringTable()
        : wxGridTableBase()
{
    m_numCols = 0;
}

wxGridStringTable::wxGridStringTable( int numRows, int numCols )
        : wxGridTableBase()
{
    m_numCols = numCols;

    m_data.Alloc( numRows );

    wxArrayString sa;
    sa.Alloc( numCols );
    sa.Add( wxEmptyString, numCols );

    m_data.Add( sa, numRows );
}

wxString wxGridStringTable::GetValue( int row, int col )
{
    wxCHECK_MSG( (row >= 0 && row < GetNumberRows()) &&
                 (col >= 0 && col < GetNumberCols()),
                 wxEmptyString,
                 wxT("invalid row or column index in wxGridStringTable") );

    return m_data[row][col];
}

void wxGridStringTable::SetValue( int row, int col, const wxString& value )
{
    wxCHECK_RET( (row >= 0 && row < GetNumberRows()) &&
                 (col >= 0 && col < GetNumberCols()),
                 wxT("invalid row or column index in wxGridStringTable") );

    m_data[row][col] = value;
}

void wxGridStringTable::Clear()
{
    int row, col;
    int numRows, numCols;

    numRows = m_data.GetCount();
    if ( numRows > 0 )
    {
        numCols = m_data[0].GetCount();

        for ( row = 0; row < numRows; row++ )
        {
            for ( col = 0; col < numCols; col++ )
            {
                m_data[row][col] = wxEmptyString;
            }
        }
    }
}

bool wxGridStringTable::InsertRows( size_t pos, size_t numRows )
{
    if ( pos >= m_data.size() )
    {
        return AppendRows( numRows );
    }

    wxArrayString sa;
    sa.Alloc( m_numCols );
    sa.Add( wxEmptyString, m_numCols );
    m_data.Insert( sa, pos, numRows );

    if ( GetView() )
    {
        wxGridTableMessage msg( this,
                                wxGRIDTABLE_NOTIFY_ROWS_INSERTED,
                                pos,
                                numRows );

        GetView()->ProcessTableMessage( msg );
    }

    return true;
}

bool wxGridStringTable::AppendRows( size_t numRows )
{
    wxArrayString sa;
    if ( m_numCols > 0 )
    {
        sa.Alloc( m_numCols );
        sa.Add( wxEmptyString, m_numCols );
    }

    m_data.Add( sa, numRows );

    if ( GetView() )
    {
        wxGridTableMessage msg( this,
                                wxGRIDTABLE_NOTIFY_ROWS_APPENDED,
                                numRows );

        GetView()->ProcessTableMessage( msg );
    }

    return true;
}

bool wxGridStringTable::DeleteRows( size_t pos, size_t numRows )
{
    size_t curNumRows = m_data.GetCount();

    if ( pos >= curNumRows )
    {
        wxFAIL_MSG( wxString::Format
                    (
                        wxT("Called wxGridStringTable::DeleteRows(pos=%lu, N=%lu)\nPos value is invalid for present table with %lu rows"),
                        (unsigned long)pos,
                        (unsigned long)numRows,
                        (unsigned long)curNumRows
                    ) );

        return false;
    }

    if ( numRows > curNumRows - pos )
    {
        numRows = curNumRows - pos;
    }

    if ( numRows >= curNumRows )
    {
        m_data.Clear();
    }
    else
    {
        m_data.RemoveAt( pos, numRows );
    }

    if ( GetView() )
    {
        wxGridTableMessage msg( this,
                                wxGRIDTABLE_NOTIFY_ROWS_DELETED,
                                pos,
                                numRows );

        GetView()->ProcessTableMessage( msg );
    }

    return true;
}

bool wxGridStringTable::InsertCols( size_t pos, size_t numCols )
{
    if ( pos >= static_cast<size_t>(m_numCols) )
    {
        return AppendCols( numCols );
    }

    if ( !m_colLabels.IsEmpty() )
    {
        m_colLabels.Insert( wxEmptyString, pos, numCols );

        for ( size_t i = pos; i < pos + numCols; i++ )
            m_colLabels[i] = wxGridTableBase::GetColLabelValue( i );
    }

    for ( size_t row = 0; row < m_data.size(); row++ )
    {
        for ( size_t col = pos; col < pos + numCols; col++ )
        {
            m_data[row].Insert( wxEmptyString, col );
        }
    }

    m_numCols += numCols;

    if ( GetView() )
    {
        wxGridTableMessage msg( this,
                                wxGRIDTABLE_NOTIFY_COLS_INSERTED,
                                pos,
                                numCols );

        GetView()->ProcessTableMessage( msg );
    }

    return true;
}

bool wxGridStringTable::AppendCols( size_t numCols )
{
    for ( size_t row = 0; row < m_data.size(); row++ )
    {
        m_data[row].Add( wxEmptyString, numCols );
    }

    m_numCols += numCols;

    if ( GetView() )
    {
        wxGridTableMessage msg( this,
                                wxGRIDTABLE_NOTIFY_COLS_APPENDED,
                                numCols );

        GetView()->ProcessTableMessage( msg );
    }

    return true;
}

bool wxGridStringTable::DeleteCols( size_t pos, size_t numCols )
{
    size_t row;

    size_t curNumRows = m_data.GetCount();
    size_t curNumCols = m_numCols;

    if ( pos >= curNumCols )
    {
        wxFAIL_MSG( wxString::Format
                    (
                        wxT("Called wxGridStringTable::DeleteCols(pos=%lu, N=%lu)\nPos value is invalid for present table with %lu cols"),
                        (unsigned long)pos,
                        (unsigned long)numCols,
                        (unsigned long)curNumCols
                    ) );
        return false;
    }

    int colID;
    if ( GetView() )
        colID = GetView()->GetColAt( pos );
    else
        colID = pos;

    if ( numCols > curNumCols - colID )
    {
        numCols = curNumCols - colID;
    }

    if ( !m_colLabels.IsEmpty() )
    {
        // m_colLabels stores just as many elements as it needs, e.g. if only
        // the label of the first column had been set it would have only one
        // element and not numCols, so account for it
        int numRemaining = m_colLabels.size() - colID;
        if (numRemaining > 0)
            m_colLabels.RemoveAt( colID, wxMin(numCols, numRemaining) );
    }

    if ( numCols >= curNumCols )
    {
        for ( row = 0; row < curNumRows; row++ )
        {
            m_data[row].Clear();
        }

        m_numCols = 0;
    }
    else // something will be left
    {
        for ( row = 0; row < curNumRows; row++ )
        {
            m_data[row].RemoveAt( colID, numCols );
        }

        m_numCols -= numCols;
    }

    if ( GetView() )
    {
        wxGridTableMessage msg( this,
                                wxGRIDTABLE_NOTIFY_COLS_DELETED,
                                pos,
                                numCols );

        GetView()->ProcessTableMessage( msg );
    }

    return true;
}

wxString wxGridStringTable::GetRowLabelValue( int row )
{
    if ( row > (int)(m_rowLabels.GetCount()) - 1 )
    {
        // using default label
        //
        return wxGridTableBase::GetRowLabelValue( row );
    }
    else
    {
        return m_rowLabels[row];
    }
}

wxString wxGridStringTable::GetColLabelValue( int col )
{
    if ( col > (int)(m_colLabels.GetCount()) - 1 )
    {
        // using default label
        //
        return wxGridTableBase::GetColLabelValue( col );
    }
    else
    {
        return m_colLabels[col];
    }
}

void wxGridStringTable::SetRowLabelValue( int row, const wxString& value )
{
    if ( row > (int)(m_rowLabels.GetCount()) - 1 )
    {
        int n = m_rowLabels.GetCount();
        int i;

        for ( i = n; i <= row; i++ )
        {
            m_rowLabels.Add( wxGridTableBase::GetRowLabelValue(i) );
        }
    }

    m_rowLabels[row] = value;
}

void wxGridStringTable::SetColLabelValue( int col, const wxString& value )
{
    if ( col > (int)(m_colLabels.GetCount()) - 1 )
    {
        int n = m_colLabels.GetCount();
        int i;

        for ( i = n; i <= col; i++ )
        {
            m_colLabels.Add( wxGridTableBase::GetColLabelValue(i) );
        }
    }

    m_colLabels[col] = value;
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(wxGridSubwindow, wxWindow)
    EVT_MOUSE_CAPTURE_LOST(wxGridSubwindow::OnMouseCaptureLost)
END_EVENT_TABLE()

void wxGridSubwindow::OnMouseCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(event))
{
    m_owner->CancelMouseCapture();
}

BEGIN_EVENT_TABLE( wxGridRowLabelWindow, wxGridSubwindow )
    EVT_PAINT( wxGridRowLabelWindow::OnPaint )
    EVT_MOUSEWHEEL( wxGridRowLabelWindow::OnMouseWheel )
    EVT_MOUSE_EVENTS( wxGridRowLabelWindow::OnMouseEvent )
END_EVENT_TABLE()

void wxGridRowLabelWindow::OnPaint( wxPaintEvent& WXUNUSED(event) )
{
    wxPaintDC dc(this);

    // NO - don't do this because it will set both the x and y origin
    // coords to match the parent scrolled window and we just want to
    // set the y coord  - MB
    //
    // m_owner->PrepareDC( dc );

    int x, y;
    m_owner->CalcUnscrolledPosition( 0, 0, &x, &y );
    wxPoint pt = dc.GetDeviceOrigin();
    dc.SetDeviceOrigin( pt.x, pt.y-y );

    wxArrayInt rows = m_owner->CalcRowLabelsExposed( GetUpdateRegion() );
    m_owner->DrawRowLabels( dc, rows );
}

void wxGridRowLabelWindow::OnMouseEvent( wxMouseEvent& event )
{
    m_owner->ProcessRowLabelMouseEvent( event );
}

void wxGridRowLabelWindow::OnMouseWheel( wxMouseEvent& event )
{
    if (!m_owner->GetEventHandler()->ProcessEvent( event ))
        event.Skip();
}

//////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( wxGridColLabelWindow, wxGridSubwindow )
    EVT_PAINT( wxGridColLabelWindow::OnPaint )
    EVT_MOUSEWHEEL( wxGridColLabelWindow::OnMouseWheel )
    EVT_MOUSE_EVENTS( wxGridColLabelWindow::OnMouseEvent )
END_EVENT_TABLE()

void wxGridColLabelWindow::OnPaint( wxPaintEvent& WXUNUSED(event) )
{
    wxPaintDC dc(this);

    // NO - don't do this because it will set both the x and y origin
    // coords to match the parent scrolled window and we just want to
    // set the x coord  - MB
    //
    // m_owner->PrepareDC( dc );

    int x, y;
    m_owner->CalcUnscrolledPosition( 0, 0, &x, &y );
    wxPoint pt = dc.GetDeviceOrigin();
    dc.SetDeviceOrigin( pt.x-x, pt.y );

    wxArrayInt cols = m_owner->CalcColLabelsExposed( GetUpdateRegion() );
    m_owner->DrawColLabels( dc, cols );
}

void wxGridColLabelWindow::OnMouseEvent( wxMouseEvent& event )
{
    m_owner->ProcessColLabelMouseEvent( event );
}

void wxGridColLabelWindow::OnMouseWheel( wxMouseEvent& event )
{
    if (!m_owner->GetEventHandler()->ProcessEvent( event ))
        event.Skip();
}

//////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( wxGridCornerLabelWindow, wxGridSubwindow )
    EVT_MOUSEWHEEL( wxGridCornerLabelWindow::OnMouseWheel )
    EVT_MOUSE_EVENTS( wxGridCornerLabelWindow::OnMouseEvent )
    EVT_PAINT( wxGridCornerLabelWindow::OnPaint )
END_EVENT_TABLE()

void wxGridCornerLabelWindow::OnPaint( wxPaintEvent& WXUNUSED(event) )
{
    wxPaintDC dc(this);

    m_owner->DrawCornerLabel(dc);
}

void wxGridCornerLabelWindow::OnMouseEvent( wxMouseEvent& event )
{
    m_owner->ProcessCornerLabelMouseEvent( event );
}

void wxGridCornerLabelWindow::OnMouseWheel( wxMouseEvent& event )
{
    if (!m_owner->GetEventHandler()->ProcessEvent(event))
        event.Skip();
}

//////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( wxGridWindow, wxGridSubwindow )
    EVT_PAINT( wxGridWindow::OnPaint )
    EVT_MOUSEWHEEL( wxGridWindow::OnMouseWheel )
    EVT_MOUSE_EVENTS( wxGridWindow::OnMouseEvent )
    EVT_KEY_DOWN( wxGridWindow::OnKeyDown )
    EVT_KEY_UP( wxGridWindow::OnKeyUp )
    EVT_CHAR( wxGridWindow::OnChar )
    EVT_SET_FOCUS( wxGridWindow::OnFocus )
    EVT_KILL_FOCUS( wxGridWindow::OnFocus )
    EVT_ERASE_BACKGROUND( wxGridWindow::OnEraseBackground )
END_EVENT_TABLE()

void wxGridWindow::OnPaint( wxPaintEvent &WXUNUSED(event) )
{
    wxPaintDC dc( this );
    m_owner->PrepareDC( dc );
    wxRegion reg = GetUpdateRegion();
    wxGridCellCoordsArray dirtyCells = m_owner->CalcCellsExposed( reg );
    m_owner->DrawGridCellArea( dc, dirtyCells );

    m_owner->DrawGridSpace( dc );

    m_owner->DrawAllGridLines( dc, reg );

    m_owner->DrawHighlight( dc, dirtyCells );
}

void wxGrid::Render( wxDC& dc,
                     const wxPoint& position,
                     const wxSize& size,
                     const wxGridCellCoords& topLeft,
                     const wxGridCellCoords& bottomRight,
                     int style )
{
    wxCHECK_RET( bottomRight.GetCol() < GetNumberCols(),
                 "Invalid right column" );
    wxCHECK_RET( bottomRight.GetRow() < GetNumberRows(),
                 "Invalid bottom row" );

    // store user settings and reset later

    // remove grid selection, don't paint selection colour
    // unless we have wxGRID_DRAW_SELECTION
    // block selections are the only ones catered for here
    wxGridCellCoordsArray selectedCells;
    bool hasSelection = IsSelection();
    if ( hasSelection && !( style & wxGRID_DRAW_SELECTION ) )
    {
        selectedCells = GetSelectionBlockTopLeft();
        // non block selections may not have a bottom right
        if ( GetSelectionBlockBottomRight().size() )
            selectedCells.Add( GetSelectionBlockBottomRight()[ 0 ] );

        ClearSelection();
    }

    // store user device origin
    wxCoord userOriginX, userOriginY;
    dc.GetDeviceOrigin( &userOriginX, &userOriginY );

    // store user scale
    double scaleUserX, scaleUserY;
    dc.GetUserScale( &scaleUserX, &scaleUserY );

    // set defaults if necessary
    wxGridCellCoords leftTop( topLeft ), rightBottom( bottomRight );
    if ( leftTop.GetCol() < 0 )
        leftTop.SetCol(0);
    if ( leftTop.GetRow() < 0 )
        leftTop.SetRow(0);
    if ( rightBottom.GetCol() < 0 )
        rightBottom.SetCol(GetNumberCols() - 1);
    if ( rightBottom.GetRow() < 0 )
        rightBottom.SetRow(GetNumberRows() - 1);

    // get grid offset, size and cell parameters
    wxPoint pointOffSet;
    wxSize sizeGrid;
    wxGridCellCoordsArray renderCells;
    wxArrayInt arrayCols;
    wxArrayInt arrayRows;

    GetRenderSizes( leftTop, rightBottom,
                    pointOffSet, sizeGrid,
                    renderCells,
                    arrayCols, arrayRows );

    // add headers/labels to dimensions
    if ( style & wxGRID_DRAW_ROWS_HEADER )
        sizeGrid.x += GetRowLabelSize();
    if ( style & wxGRID_DRAW_COLS_HEADER )
        sizeGrid.y += GetColLabelSize();

    // get render start position in logical units
    wxPoint positionRender = GetRenderPosition( dc, position );

    wxCoord originX = dc.LogicalToDeviceX( positionRender.x );
    wxCoord originY = dc.LogicalToDeviceY( positionRender.y );

    dc.SetDeviceOrigin( originX, originY );

    SetRenderScale( dc, positionRender, size, sizeGrid );

    // draw row headers at specified origin
    if ( GetRowLabelSize() > 0 && ( style & wxGRID_DRAW_ROWS_HEADER ) )
    {
        if ( style & wxGRID_DRAW_COLS_HEADER )
        {
            DrawCornerLabel( dc ); // do only if both col and row labels drawn
            originY += dc.LogicalToDeviceYRel( GetColLabelSize() );
        }

        originY -= dc.LogicalToDeviceYRel( pointOffSet.y );
        dc.SetDeviceOrigin( originX, originY );

        DrawRowLabels( dc, arrayRows );

        // reset for columns
        if ( style & wxGRID_DRAW_COLS_HEADER )
            originY -= dc.LogicalToDeviceYRel( GetColLabelSize() );

        originY += dc.LogicalToDeviceYRel( pointOffSet.y );
        // X offset so we don't overwrite row labels
        originX += dc.LogicalToDeviceXRel( GetRowLabelSize() );
    }

    // subtract col offset where startcol > 0
    originX -= dc.LogicalToDeviceXRel( pointOffSet.x );
    // no y offset for col labels, they are at the Y origin

    // draw column labels
    if ( style & wxGRID_DRAW_COLS_HEADER )
    {
        dc.SetDeviceOrigin( originX, originY );
        DrawColLabels( dc, arrayCols );
        // don't overwrite the labels, increment originY
        originY += dc.LogicalToDeviceYRel( GetColLabelSize() );
    }

    // set device origin to draw grid cells and lines
    originY -= dc.LogicalToDeviceYRel( pointOffSet.y );
    dc.SetDeviceOrigin( originX, originY );

    // draw cell area background
    dc.SetBrush( GetDefaultCellBackgroundColour() );
    dc.SetPen( *wxTRANSPARENT_PEN );
    // subtract headers from grid area dimensions
    wxSize sizeCells( sizeGrid );
    if ( style & wxGRID_DRAW_ROWS_HEADER )
        sizeCells.x -= GetRowLabelSize();
    if ( style & wxGRID_DRAW_COLS_HEADER )
        sizeCells.y -= GetColLabelSize();

    dc.DrawRectangle( pointOffSet, sizeCells );

    // draw cells
    DrawGridCellArea( dc, renderCells );

    // draw grid lines
    if ( style & wxGRID_DRAW_CELL_LINES )
    {
        wxRegion regionClip( pointOffSet.x, pointOffSet.y,
                             sizeCells.x, sizeCells.y );

        DrawRangeGridLines(dc, regionClip, renderCells[0], renderCells.Last());
    }

    // draw render rectangle bounding lines
    DoRenderBox( dc, style,
                 pointOffSet, sizeCells,
                 leftTop, rightBottom );

    // restore user setings
    dc.SetDeviceOrigin( userOriginX, userOriginY );
    dc.SetUserScale( scaleUserX, scaleUserY );

    if ( selectedCells.size() && !( style & wxGRID_DRAW_SELECTION ) )
    {
        SelectBlock( selectedCells[ 0 ].GetRow(),
                     selectedCells[ 0 ].GetCol(),
                     selectedCells[ selectedCells.size() -1 ].GetRow(),
                     selectedCells[ selectedCells.size() -1 ].GetCol() );
    }
}

void
wxGrid::SetRenderScale(wxDC& dc,
                       const wxPoint& pos, const wxSize& size,
                       const wxSize& sizeGrid )
{
    double scaleX, scaleY;
    wxSize sizeTemp;

    if ( size.GetWidth() != wxDefaultSize.GetWidth() ) // size.x was specified
        sizeTemp.SetWidth( size.GetWidth() );
    else
        sizeTemp.SetWidth( dc.DeviceToLogicalXRel( dc.GetSize().GetWidth() )
                           - pos.x );

    if ( size.GetHeight() != wxDefaultSize.GetHeight() ) // size.y was specified
        sizeTemp.SetHeight( size.GetHeight() );
    else
        sizeTemp.SetHeight( dc.DeviceToLogicalYRel( dc.GetSize().GetHeight() )
                            - pos.y );

    scaleX = (double)( (double) sizeTemp.GetWidth() / (double) sizeGrid.GetWidth() );
    scaleY = (double)( (double) sizeTemp.GetHeight() / (double) sizeGrid.GetHeight() );

    dc.SetUserScale( wxMin( scaleX, scaleY), wxMin( scaleX, scaleY ) );
}

// get grid rendered size, origin offset and fill cell arrays
void wxGrid::GetRenderSizes( const wxGridCellCoords& topLeft,
                             const wxGridCellCoords& bottomRight,
                             wxPoint& pointOffSet, wxSize& sizeGrid,
                             wxGridCellCoordsArray& renderCells,
                             wxArrayInt& arrayCols, wxArrayInt& arrayRows )
{
    pointOffSet.x = 0;
    pointOffSet.y = 0;
    sizeGrid.SetWidth( 0 );
    sizeGrid.SetHeight( 0 );

    int col, row;

    wxGridSizesInfo sizeinfo = GetColSizes();
    for ( col = 0; col <= bottomRight.GetCol(); col++ )
    {
        if ( col < topLeft.GetCol() )
        {
            pointOffSet.x += sizeinfo.GetSize( col );
        }
        else
        {
            for ( row = topLeft.GetRow(); row <= bottomRight.GetRow(); row++ )
            {
                renderCells.Add( wxGridCellCoords( row, col ));
                arrayRows.Add( row ); // column labels rendered in DrawColLabels
            }
            arrayCols.Add( col ); // row labels rendered in DrawRowLabels
            sizeGrid.x += sizeinfo.GetSize( col );
        }
    }

    sizeinfo = GetRowSizes();
    for ( row = 0; row <= bottomRight.GetRow(); row++ )
    {
        if ( row < topLeft.GetRow() )
            pointOffSet.y += sizeinfo.GetSize( row );
        else
            sizeGrid.y += sizeinfo.GetSize( row );
    }
}

// get render start position
// if position not specified use dc draw extents MaxX and MaxY
wxPoint wxGrid::GetRenderPosition( wxDC& dc, const wxPoint& position )
{
    wxPoint positionRender( position );

    if ( !positionRender.IsFullySpecified() )
    {
        if ( positionRender.x == wxDefaultPosition.x )
            positionRender.x = dc.MaxX();

        if ( positionRender.y == wxDefaultPosition.y )
            positionRender.y = dc.MaxY();
    }

    return positionRender;
}

// draw render rectangle bounding lines
// useful where there is multi cell row or col clipping and no cell border
void wxGrid::DoRenderBox( wxDC& dc, const int& style,
                          const wxPoint& pointOffSet,
                          const wxSize& sizeCells,
                          const wxGridCellCoords& topLeft,
                          const wxGridCellCoords& bottomRight )
{
    if ( !( style & wxGRID_DRAW_BOX_RECT ) )
        return;

    int bottom = pointOffSet.y + sizeCells.GetY(),
        right = pointOffSet.x + sizeCells.GetX() - 1;

    // horiz top line if we are not drawing column header/labels
    if ( !( style & wxGRID_DRAW_COLS_HEADER ) )
    {
        int left = pointOffSet.x;
        left += ( style & wxGRID_DRAW_COLS_HEADER )
                     ? - GetRowLabelSize() : 0;
        dc.SetPen( GetRowGridLinePen( topLeft.GetRow() ) );
        dc.DrawLine( left,
                     pointOffSet.y,
                     right,
                     pointOffSet.y );
    }

    // horiz bottom line
    dc.SetPen( GetRowGridLinePen( bottomRight.GetRow() ) );
    dc.DrawLine( pointOffSet.x, bottom - 1, right, bottom - 1 );

    // left vertical line if we are not drawing row header/labels
    if ( !( style & wxGRID_DRAW_ROWS_HEADER ) )
    {
        int top = pointOffSet.y;
        top += ( style & wxGRID_DRAW_COLS_HEADER )
                     ? - GetColLabelSize() : 0;
        dc.SetPen( GetColGridLinePen( topLeft.GetCol() ) );
        dc.DrawLine( pointOffSet.x -1,
                     top,
                     pointOffSet.x - 1,
                     bottom - 1 );
    }

    // right vertical line
    dc.SetPen( GetColGridLinePen( bottomRight.GetCol() ) );
    dc.DrawLine( right, pointOffSet.y, right, bottom - 1 );
}

void wxGridWindow::ScrollWindow( int dx, int dy, const wxRect *rect )
{
    wxWindow::ScrollWindow( dx, dy, rect );
    m_owner->GetGridRowLabelWindow()->ScrollWindow( 0, dy, rect );
    m_owner->GetGridColLabelWindow()->ScrollWindow( dx, 0, rect );
}

void wxGridWindow::OnMouseEvent( wxMouseEvent& event )
{
    if (event.ButtonDown(wxMOUSE_BTN_LEFT) && FindFocus() != this)
        SetFocus();

    m_owner->ProcessGridCellMouseEvent( event );
}

void wxGridWindow::OnMouseWheel( wxMouseEvent& event )
{
    if (!m_owner->GetEventHandler()->ProcessEvent( event ))
        event.Skip();
}

// This seems to be required for wxMotif/wxGTK otherwise the mouse
// cursor must be in the cell edit control to get key events
//
void wxGridWindow::OnKeyDown( wxKeyEvent& event )
{
    if ( !m_owner->GetEventHandler()->ProcessEvent( event ) )
        event.Skip();
}

void wxGridWindow::OnKeyUp( wxKeyEvent& event )
{
    if ( !m_owner->GetEventHandler()->ProcessEvent( event ) )
        event.Skip();
}

void wxGridWindow::OnChar( wxKeyEvent& event )
{
    if ( !m_owner->GetEventHandler()->ProcessEvent( event ) )
        event.Skip();
}

void wxGridWindow::OnEraseBackground( wxEraseEvent& WXUNUSED(event) )
{
}

void wxGridWindow::OnFocus(wxFocusEvent& event)
{
    // and if we have any selection, it has to be repainted, because it
    // uses different colour when the grid is not focused:
    if ( m_owner->IsSelection() )
    {
        Refresh();
    }
    else
    {
        // NB: Note that this code is in "else" branch only because the other
        //     branch refreshes everything and so there's no point in calling
        //     Refresh() again, *not* because it should only be done if
        //     !IsSelection(). If the above code is ever optimized to refresh
        //     only selected area, this needs to be moved out of the "else"
        //     branch so that it's always executed.

        // current cell cursor {dis,re}appears on focus change:
        const wxGridCellCoords cursorCoords(m_owner->GetGridCursorRow(),
                                            m_owner->GetGridCursorCol());
        const wxRect cursor =
            m_owner->BlockToDeviceRect(cursorCoords, cursorCoords);
        if (cursor != wxGridNoCellRect)
            Refresh(true, &cursor);
    }

    if ( !m_owner->GetEventHandler()->ProcessEvent( event ) )
        event.Skip();
}

#define internalXToCol(x) XToCol(x, true)
#define internalYToRow(y) YToRow(y, true)

/////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( wxGrid, wxScrolledWindow )
    EVT_PAINT( wxGrid::OnPaint )
    EVT_SIZE( wxGrid::OnSize )
    EVT_KEY_DOWN( wxGrid::OnKeyDown )
    EVT_KEY_UP( wxGrid::OnKeyUp )
    EVT_CHAR ( wxGrid::OnChar )
    EVT_ERASE_BACKGROUND( wxGrid::OnEraseBackground )
    EVT_COMMAND(wxID_ANY, wxEVT_GRID_HIDE_EDITOR, wxGrid::OnHideEditor )
END_EVENT_TABLE()

bool wxGrid::Create(wxWindow *parent, wxWindowID id,
                          const wxPoint& pos, const wxSize& size,
                          long style, const wxString& name)
{
    if (!wxScrolledWindow::Create(parent, id, pos, size,
                                  style | wxWANTS_CHARS, name))
        return false;

    m_colMinWidths = wxLongToLongHashMap(GRID_HASH_SIZE);
    m_rowMinHeights = wxLongToLongHashMap(GRID_HASH_SIZE);

    Create();
    SetInitialSize(size);
    CalcDimensions();

    return true;
}

wxGrid::~wxGrid()
{
    if ( m_winCapture )
        m_winCapture->ReleaseMouse();

    // Ensure that the editor control is destroyed before the grid is,
    // otherwise we crash later when the editor tries to do something with the
    // half destroyed grid
    HideCellEditControl();

    // Must do this or ~wxScrollHelper will pop the wrong event handler
    SetTargetWindow(this);
    ClearAttrCache();
    wxSafeDecRef(m_defaultCellAttr);

#ifdef DEBUG_ATTR_CACHE
    size_t total = gs_nAttrCacheHits + gs_nAttrCacheMisses;
    wxPrintf(wxT("wxGrid attribute cache statistics: "
                "total: %u, hits: %u (%u%%)\n"),
             total, gs_nAttrCacheHits,
             total ? (gs_nAttrCacheHits*100) / total : 0);
#endif

    // if we own the table, just delete it, otherwise at least don't leave it
    // with dangling view pointer
    if ( m_ownTable )
        delete m_table;
    else if ( m_table && m_table->GetView() == this )
        m_table->SetView(NULL);

    delete m_typeRegistry;
    delete m_selection;

    delete m_setFixedRows;
    delete m_setFixedCols;
}

//
// ----- internal init and update functions
//

// NOTE: If using the default visual attributes works everywhere then this can
// be removed as well as the #else cases below.
#define _USE_VISATTR 0

void wxGrid::Create()
{
    // create the type registry
    m_typeRegistry = new wxGridTypeRegistry;

    m_cellEditCtrlEnabled = false;

    m_defaultCellAttr = new wxGridCellAttr();

    // Set default cell attributes
    m_defaultCellAttr->SetDefAttr(m_defaultCellAttr);
    m_defaultCellAttr->SetKind(wxGridCellAttr::Default);
    m_defaultCellAttr->SetFont(GetFont());
    m_defaultCellAttr->SetAlignment(wxALIGN_LEFT, wxALIGN_TOP);
    m_defaultCellAttr->SetRenderer(new wxGridCellStringRenderer);
    m_defaultCellAttr->SetEditor(new wxGridCellTextEditor);

#if _USE_VISATTR
    wxVisualAttributes gva = wxListBox::GetClassDefaultAttributes();
    wxVisualAttributes lva = wxPanel::GetClassDefaultAttributes();

    m_defaultCellAttr->SetTextColour(gva.colFg);
    m_defaultCellAttr->SetBackgroundColour(gva.colBg);

#else
    m_defaultCellAttr->SetTextColour(
        wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    m_defaultCellAttr->SetBackgroundColour(
        wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#endif

    m_numRows = 0;
    m_numCols = 0;
    m_currentCellCoords = wxGridNoCellCoords;

    // subwindow components that make up the wxGrid
    m_rowLabelWin = new wxGridRowLabelWindow(this);
    CreateColumnWindow();
    m_cornerLabelWin = new wxGridCornerLabelWindow(this);
    m_gridWin = new wxGridWindow( this );

    SetTargetWindow( m_gridWin );

#if _USE_VISATTR
    wxColour gfg = gva.colFg;
    wxColour gbg = gva.colBg;
    wxColour lfg = lva.colFg;
    wxColour lbg = lva.colBg;
#else
    wxColour gfg = wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOWTEXT );
    wxColour gbg = wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW );
    wxColour lfg = wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOWTEXT );
    wxColour lbg = wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE );
#endif

    m_cornerLabelWin->SetOwnForegroundColour(lfg);
    m_cornerLabelWin->SetOwnBackgroundColour(lbg);
    m_rowLabelWin->SetOwnForegroundColour(lfg);
    m_rowLabelWin->SetOwnBackgroundColour(lbg);
    m_colWindow->SetOwnForegroundColour(lfg);
    m_colWindow->SetOwnBackgroundColour(lbg);

    m_gridWin->SetOwnForegroundColour(gfg);
    m_gridWin->SetOwnBackgroundColour(gbg);

    m_labelBackgroundColour = m_rowLabelWin->GetBackgroundColour();
    m_labelTextColour = m_rowLabelWin->GetForegroundColour();

    // now that we have the grid window, use its font to compute the default
    // row height
    m_defaultRowHeight = m_gridWin->GetCharHeight();
#if defined(__WXMOTIF__) || defined(__WXGTK__)  // see also text ctrl sizing in ShowCellEditControl()
    m_defaultRowHeight += 8;
#else
    m_defaultRowHeight += 4;
#endif

}

void wxGrid::CreateColumnWindow()
{
    if ( m_useNativeHeader )
    {
        m_colWindow = new wxGridHeaderCtrl(this);
        m_colLabelHeight = m_colWindow->GetBestSize().y;
    }
    else // draw labels ourselves
    {
        m_colWindow = new wxGridColLabelWindow(this);
        m_colLabelHeight = WXGRID_DEFAULT_COL_LABEL_HEIGHT;
    }
}

bool wxGrid::CreateGrid( int numRows, int numCols,
                         wxGridSelectionModes selmode )
{
    wxCHECK_MSG( !m_created,
                 false,
                 wxT("wxGrid::CreateGrid or wxGrid::SetTable called more than once") );

    return SetTable(new wxGridStringTable(numRows, numCols), true, selmode);
}

void wxGrid::SetSelectionMode(wxGridSelectionModes selmode)
{
    wxCHECK_RET( m_created,
                 wxT("Called wxGrid::SetSelectionMode() before calling CreateGrid()") );

    m_selection->SetSelectionMode( selmode );
}

wxGrid::wxGridSelectionModes wxGrid::GetSelectionMode() const
{
    wxCHECK_MSG( m_created, wxGridSelectCells,
                 wxT("Called wxGrid::GetSelectionMode() before calling CreateGrid()") );

    return m_selection->GetSelectionMode();
}

bool
wxGrid::SetTable(wxGridTableBase *table,
                 bool takeOwnership,
                 wxGrid::wxGridSelectionModes selmode )
{
    bool checkSelection = false;
    if ( m_created )
    {
        // stop all processing
        m_created = false;

        if (m_table)
        {
            m_table->SetView(0);
            if( m_ownTable )
                delete m_table;
            m_table = NULL;
        }

        wxDELETE(m_selection);

        m_ownTable = false;
        m_numRows = 0;
        m_numCols = 0;
        checkSelection = true;

        // kill row and column size arrays
        m_colWidths.Empty();
        m_colRights.Empty();
        m_rowHeights.Empty();
        m_rowBottoms.Empty();
    }

    if (table)
    {
        m_numRows = table->GetNumberRows();
        m_numCols = table->GetNumberCols();

        m_table = table;
        m_table->SetView( this );
        m_ownTable = takeOwnership;

        // Notice that this must be called after setting m_table as it uses it
        // indirectly, via wxGrid::GetColLabelValue().
        if ( m_useNativeHeader )
            GetGridColHeader()->SetColumnCount(m_numCols);

        m_selection = new wxGridSelection( this, selmode );
        if (checkSelection)
        {
            // If the newly set table is smaller than the
            // original one current cell and selection regions
            // might be invalid,
            m_selectedBlockCorner = wxGridNoCellCoords;
            m_currentCellCoords =
              wxGridCellCoords(wxMin(m_numRows, m_currentCellCoords.GetRow()),
                               wxMin(m_numCols, m_currentCellCoords.GetCol()));
            if (m_selectedBlockTopLeft.GetRow() >= m_numRows ||
                m_selectedBlockTopLeft.GetCol() >= m_numCols)
            {
                m_selectedBlockTopLeft = wxGridNoCellCoords;
                m_selectedBlockBottomRight = wxGridNoCellCoords;
            }
            else
                m_selectedBlockBottomRight =
                  wxGridCellCoords(wxMin(m_numRows,
                                         m_selectedBlockBottomRight.GetRow()),
                                   wxMin(m_numCols,
                                         m_selectedBlockBottomRight.GetCol()));
        }
        CalcDimensions();

        m_created = true;
    }

    InvalidateBestSize();

    return m_created;
}

void wxGrid::Init()
{
    m_created = false;

    m_cornerLabelWin = NULL;
    m_rowLabelWin = NULL;
    m_colWindow = NULL;
    m_gridWin = NULL;

    m_table = NULL;
    m_ownTable = false;

    m_selection = NULL;
    m_defaultCellAttr = NULL;
    m_typeRegistry = NULL;
    m_winCapture = NULL;

    m_rowLabelWidth  = WXGRID_DEFAULT_ROW_LABEL_WIDTH;
    m_colLabelHeight = WXGRID_DEFAULT_COL_LABEL_HEIGHT;

    m_setFixedRows =
    m_setFixedCols = NULL;

    // init attr cache
    m_attrCache.row = -1;
    m_attrCache.col = -1;
    m_attrCache.attr = NULL;

    m_labelFont = GetFont();
    m_labelFont.SetWeight( wxBOLD );

    m_rowLabelHorizAlign = wxALIGN_CENTRE;
    m_rowLabelVertAlign  = wxALIGN_CENTRE;

    m_colLabelHorizAlign = wxALIGN_CENTRE;
    m_colLabelVertAlign  = wxALIGN_CENTRE;
    m_colLabelTextOrientation = wxHORIZONTAL;

    m_defaultColWidth  = WXGRID_DEFAULT_COL_WIDTH;
    m_defaultRowHeight = 0; // this will be initialized after creation

    m_minAcceptableColWidth  = WXGRID_MIN_COL_WIDTH;
    m_minAcceptableRowHeight = WXGRID_MIN_ROW_HEIGHT;

    m_gridLineColour = wxColour( 192,192,192 );
    m_gridLinesEnabled = true;
    m_gridLinesClipHorz =
    m_gridLinesClipVert = true;
    m_cellHighlightColour = *wxBLACK;
    m_cellHighlightPenWidth = 2;
    m_cellHighlightROPenWidth = 1;

    m_canDragColMove = false;

    m_cursorMode  = WXGRID_CURSOR_SELECT_CELL;
    m_winCapture = NULL;
    m_canDragRowSize = true;
    m_canDragColSize = true;
    m_canDragGridSize = true;
    m_canDragCell = false;
    m_dragLastPos  = -1;
    m_dragRowOrCol = -1;
    m_isDragging = false;
    m_startDragPos = wxDefaultPosition;

    m_sortCol = wxNOT_FOUND;
    m_sortIsAscending = true;

    m_useNativeHeader =
    m_nativeColumnLabels = false;

    m_waitForSlowClick = false;

    m_rowResizeCursor = wxCursor( wxCURSOR_SIZENS );
    m_colResizeCursor = wxCursor( wxCURSOR_SIZEWE );

    m_currentCellCoords = wxGridNoCellCoords;

    m_selectedBlockTopLeft =
    m_selectedBlockBottomRight =
    m_selectedBlockCorner = wxGridNoCellCoords;

    m_selectionBackground = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
    m_selectionForeground = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);

    m_editable = true;  // default for whole grid

    m_inOnKeyDown = false;
    m_batchCount = 0;

    m_extraWidth =
    m_extraHeight = 0;

    // we can't call SetScrollRate() as the window isn't created yet but OTOH
    // we don't need to call it neither as the scroll position is (0, 0) right
    // now anyhow, so just set the parameters directly
    m_xScrollPixelsPerLine = GRID_SCROLL_LINE_X;
    m_yScrollPixelsPerLine = GRID_SCROLL_LINE_Y;

    m_tabBehaviour = Tab_Stop;
}

// ----------------------------------------------------------------------------
// the idea is to call these functions only when necessary because they create
// quite big arrays which eat memory mostly unnecessary - in particular, if
// default widths/heights are used for all rows/columns, we may not use these
// arrays at all
//
// with some extra code, it should be possible to only store the widths/heights
// different from default ones (resulting in space savings for huge grids) but
// this is not done currently
// ----------------------------------------------------------------------------

void wxGrid::InitRowHeights()
{
    m_rowHeights.Empty();
    m_rowBottoms.Empty();

    m_rowHeights.Alloc( m_numRows );
    m_rowBottoms.Alloc( m_numRows );

    m_rowHeights.Add( m_defaultRowHeight, m_numRows );

    int rowBottom = 0;
    for ( int i = 0; i < m_numRows; i++ )
    {
        rowBottom += m_defaultRowHeight;
        m_rowBottoms.Add( rowBottom );
    }
}

void wxGrid::InitColWidths()
{
    m_colWidths.Empty();
    m_colRights.Empty();

    m_colWidths.Alloc( m_numCols );
    m_colRights.Alloc( m_numCols );

    m_colWidths.Add( m_defaultColWidth, m_numCols );

    for ( int i = 0; i < m_numCols; i++ )
    {
        int colRight = ( GetColPos( i ) + 1 ) * m_defaultColWidth;
        m_colRights.Add( colRight );
    }
}

int wxGrid::GetColWidth(int col) const
{
    if ( m_colWidths.IsEmpty() )
        return m_defaultColWidth;

    // a negative width indicates a hidden column
    return m_colWidths[col] > 0 ? m_colWidths[col] : 0;
}

int wxGrid::GetColLeft(int col) const
{
    if ( m_colRights.IsEmpty() )
        return GetColPos( col ) * m_defaultColWidth;

    return m_colRights[col] - GetColWidth(col);
}

int wxGrid::GetColRight(int col) const
{
    return m_colRights.IsEmpty() ? (GetColPos( col ) + 1) * m_defaultColWidth
                                 : m_colRights[col];
}

int wxGrid::GetRowHeight(int row) const
{
    // no custom heights / hidden rows
    if ( m_rowHeights.IsEmpty() )
        return m_defaultRowHeight;

    // a negative height indicates a hidden row
    return m_rowHeights[row] > 0 ? m_rowHeights[row] : 0;
}

int wxGrid::GetRowTop(int row) const
{
    if ( m_rowBottoms.IsEmpty() )
        return row * m_defaultRowHeight;

    return m_rowBottoms[row] - GetRowHeight(row);
}

int wxGrid::GetRowBottom(int row) const
{
    return m_rowBottoms.IsEmpty() ? (row + 1) * m_defaultRowHeight
                                  : m_rowBottoms[row];
}

void wxGrid::CalcDimensions()
{
    // compute the size of the scrollable area
    int w = m_numCols > 0 ? GetColRight(GetColAt(m_numCols - 1)) : 0;
    int h = m_numRows > 0 ? GetRowBottom(m_numRows - 1) : 0;

    w += m_extraWidth;
    h += m_extraHeight;

    // take into account editor if shown
    if ( IsCellEditControlShown() )
    {
        int w2, h2;
        int r = m_currentCellCoords.GetRow();
        int c = m_currentCellCoords.GetCol();
        int x = GetColLeft(c);
        int y = GetRowTop(r);

        // how big is the editor
        wxGridCellAttr* attr = GetCellAttr(r, c);
        wxGridCellEditor* editor = attr->GetEditor(this, r, c);
        editor->GetControl()->GetSize(&w2, &h2);
        w2 += x;
        h2 += y;
        if ( w2 > w )
            w = w2;
        if ( h2 > h )
            h = h2;
        editor->DecRef();
        attr->DecRef();
    }

    // preserve (more or less) the previous position
    int x, y;
    GetViewStart( &x, &y );

    // ensure the position is valid for the new scroll ranges
    if ( x >= w )
        x = wxMax( w - 1, 0 );
    if ( y >= h )
        y = wxMax( h - 1, 0 );

    // update the virtual size and refresh the scrollbars to reflect it
    m_gridWin->SetVirtualSize(w, h);
    Scroll(x, y);
    AdjustScrollbars();

    // if our OnSize() hadn't been called (it would if we have scrollbars), we
    // still must reposition the children
    CalcWindowSizes();
}

wxSize wxGrid::GetSizeAvailableForScrollTarget(const wxSize& size)
{
    wxSize sizeGridWin(size);
    sizeGridWin.x -= m_rowLabelWidth;
    sizeGridWin.y -= m_colLabelHeight;

    return sizeGridWin;
}

void wxGrid::CalcWindowSizes()
{
    // escape if the window is has not been fully created yet

    if ( m_cornerLabelWin == NULL )
        return;

    int cw, ch;
    GetClientSize( &cw, &ch );

    // the grid may be too small to have enough space for the labels yet, don't
    // size the windows to negative sizes in this case
    int gw = cw - m_rowLabelWidth;
    int gh = ch - m_colLabelHeight;
    if (gw < 0)
        gw = 0;
    if (gh < 0)
        gh = 0;

    if ( m_cornerLabelWin && m_cornerLabelWin->IsShown() )
        m_cornerLabelWin->SetSize( 0, 0, m_rowLabelWidth, m_colLabelHeight );

    if ( m_colWindow && m_colWindow->IsShown() )
        m_colWindow->SetSize( m_rowLabelWidth, 0, gw, m_colLabelHeight );

    if ( m_rowLabelWin && m_rowLabelWin->IsShown() )
        m_rowLabelWin->SetSize( 0, m_colLabelHeight, m_rowLabelWidth, gh );

    if ( m_gridWin && m_gridWin->IsShown() )
        m_gridWin->SetSize( m_rowLabelWidth, m_colLabelHeight, gw, gh );
}

// this is called when the grid table sends a message
// to indicate that it has been redimensioned
//
bool wxGrid::Redimension( wxGridTableMessage& msg )
{
    int i;
    bool result = false;

    // Clear the attribute cache as the attribute might refer to a different
    // cell than stored in the cache after adding/removing rows/columns.
    ClearAttrCache();

    // By the same reasoning, the editor should be dismissed if columns are
    // added or removed. And for consistency, it should IMHO always be
    // removed, not only if the cell "underneath" it actually changes.
    // For now, I intentionally do not save the editor's content as the
    // cell it might want to save that stuff to might no longer exist.
    HideCellEditControl();

    switch ( msg.GetId() )
    {
        case wxGRIDTABLE_NOTIFY_ROWS_INSERTED:
        {
            size_t pos = msg.GetCommandInt();
            int numRows = msg.GetCommandInt2();

            m_numRows += numRows;

            if ( !m_rowHeights.IsEmpty() )
            {
                m_rowHeights.Insert( m_defaultRowHeight, pos, numRows );
                m_rowBottoms.Insert( 0, pos, numRows );

                int bottom = 0;
                if ( pos > 0 )
                    bottom = m_rowBottoms[pos - 1];

                for ( i = pos; i < m_numRows; i++ )
                {
                    bottom += GetRowHeight(i);
                    m_rowBottoms[i] = bottom;
                }
            }

            if ( m_currentCellCoords == wxGridNoCellCoords )
            {
                // if we have just inserted cols into an empty grid the current
                // cell will be undefined...
                //
                SetCurrentCell( 0, 0 );
            }

            if ( m_selection )
                m_selection->UpdateRows( pos, numRows );
            wxGridCellAttrProvider * attrProvider = m_table->GetAttrProvider();
            if (attrProvider)
                attrProvider->UpdateAttrRows( pos, numRows );

            if ( !GetBatchCount() )
            {
                CalcDimensions();
                m_rowLabelWin->Refresh();
            }
        }
        result = true;
        break;

        case wxGRIDTABLE_NOTIFY_ROWS_APPENDED:
        {
            int numRows = msg.GetCommandInt();
            int oldNumRows = m_numRows;
            m_numRows += numRows;

            if ( !m_rowHeights.IsEmpty() )
            {
                m_rowHeights.Add( m_defaultRowHeight, numRows );
                m_rowBottoms.Add( 0, numRows );

                int bottom = 0;
                if ( oldNumRows > 0 )
                    bottom = m_rowBottoms[oldNumRows - 1];

                for ( i = oldNumRows; i < m_numRows; i++ )
                {
                    bottom += GetRowHeight(i);
                    m_rowBottoms[i] = bottom;
                }
            }

            if ( m_currentCellCoords == wxGridNoCellCoords )
            {
                // if we have just inserted cols into an empty grid the current
                // cell will be undefined...
                //
                SetCurrentCell( 0, 0 );
            }

            if ( !GetBatchCount() )
            {
                CalcDimensions();
                m_rowLabelWin->Refresh();
            }
        }
        result = true;
        break;

        case wxGRIDTABLE_NOTIFY_ROWS_DELETED:
        {
            size_t pos = msg.GetCommandInt();
            int numRows = msg.GetCommandInt2();
            m_numRows -= numRows;

            if ( !m_rowHeights.IsEmpty() )
            {
                m_rowHeights.RemoveAt( pos, numRows );
                m_rowBottoms.RemoveAt( pos, numRows );

                int h = 0;
                for ( i = 0; i < m_numRows; i++ )
                {
                    h += GetRowHeight(i);
                    m_rowBottoms[i] = h;
                }
            }

            if ( !m_numRows )
            {
                m_currentCellCoords = wxGridNoCellCoords;
            }
            else
            {
                if ( m_currentCellCoords.GetRow() >= m_numRows )
                    m_currentCellCoords.Set( 0, 0 );
            }

            if ( m_selection )
                m_selection->UpdateRows( pos, -((int)numRows) );
            wxGridCellAttrProvider * attrProvider = m_table->GetAttrProvider();
            if (attrProvider)
            {
                attrProvider->UpdateAttrRows( pos, -((int)numRows) );

// ifdef'd out following patch from Paul Gammans
#if 0
                // No need to touch column attributes, unless we
                // removed _all_ rows, in this case, we remove
                // all column attributes.
                // I hate to do this here, but the
                // needed data is not available inside UpdateAttrRows.
                if ( !GetNumberRows() )
                    attrProvider->UpdateAttrCols( 0, -GetNumberCols() );
#endif
            }

            if ( !GetBatchCount() )
            {
                CalcDimensions();
                m_rowLabelWin->Refresh();
            }
        }
        result = true;
        break;

        case wxGRIDTABLE_NOTIFY_COLS_INSERTED:
        {
            size_t pos = msg.GetCommandInt();
            int numCols = msg.GetCommandInt2();
            m_numCols += numCols;

            if ( m_useNativeHeader )
                GetGridColHeader()->SetColumnCount(m_numCols);

            if ( !m_colAt.IsEmpty() )
            {
                //Shift the column IDs
                for ( i = 0; i < m_numCols - numCols; i++ )
                {
                    if ( m_colAt[i] >= (int)pos )
                        m_colAt[i] += numCols;
                }

                m_colAt.Insert( pos, pos, numCols );

                //Set the new columns' positions
                for ( i = pos + 1; i < (int)pos + numCols; i++ )
                {
                    m_colAt[i] = i;
                }
            }

            if ( !m_colWidths.IsEmpty() )
            {
                m_colWidths.Insert( m_defaultColWidth, pos, numCols );
                m_colRights.Insert( 0, pos, numCols );

                int right = 0;
                if ( pos > 0 )
                    right = m_colRights[GetColAt( pos - 1 )];

                int colPos;
                for ( colPos = pos; colPos < m_numCols; colPos++ )
                {
                    i = GetColAt( colPos );

                    right += GetColWidth(i);
                    m_colRights[i] = right;
                }
            }

            if ( m_currentCellCoords == wxGridNoCellCoords )
            {
                // if we have just inserted cols into an empty grid the current
                // cell will be undefined...
                //
                SetCurrentCell( 0, 0 );
            }

            if ( m_selection )
                m_selection->UpdateCols( pos, numCols );
            wxGridCellAttrProvider * attrProvider = m_table->GetAttrProvider();
            if (attrProvider)
                attrProvider->UpdateAttrCols( pos, numCols );
            if ( !GetBatchCount() )
            {
                CalcDimensions();
                m_colWindow->Refresh();
            }
        }
        result = true;
        break;

        case wxGRIDTABLE_NOTIFY_COLS_APPENDED:
        {
            int numCols = msg.GetCommandInt();
            int oldNumCols = m_numCols;
            m_numCols += numCols;

            if ( !m_colAt.IsEmpty() )
            {
                m_colAt.Add( 0, numCols );

                //Set the new columns' positions
                for ( i = oldNumCols; i < m_numCols; i++ )
                {
                    m_colAt[i] = i;
                }
            }

            if ( !m_colWidths.IsEmpty() )
            {
                m_colWidths.Add( m_defaultColWidth, numCols );
                m_colRights.Add( 0, numCols );

                int right = 0;
                if ( oldNumCols > 0 )
                    right = m_colRights[GetColAt( oldNumCols - 1 )];

                int colPos;
                for ( colPos = oldNumCols; colPos < m_numCols; colPos++ )
                {
                    i = GetColAt( colPos );

                    right += GetColWidth(i);
                    m_colRights[i] = right;
                }
            }

            // Notice that this must be called after updating m_colWidths above
            // as the native grid control will check whether the new columns
            // are shown which results in accessing m_colWidths array.
            if ( m_useNativeHeader )
                GetGridColHeader()->SetColumnCount(m_numCols);

            if ( m_currentCellCoords == wxGridNoCellCoords )
            {
                // if we have just inserted cols into an empty grid the current
                // cell will be undefined...
                //
                SetCurrentCell( 0, 0 );
            }
            if ( !GetBatchCount() )
            {
                CalcDimensions();
                m_colWindow->Refresh();
            }
        }
        result = true;
        break;

        case wxGRIDTABLE_NOTIFY_COLS_DELETED:
        {
            size_t pos = msg.GetCommandInt();
            int numCols = msg.GetCommandInt2();
            m_numCols -= numCols;
            if ( m_useNativeHeader )
                GetGridColHeader()->SetColumnCount(m_numCols);

            if ( !m_colAt.IsEmpty() )
            {
                int colID = GetColAt( pos );

                m_colAt.RemoveAt( pos, numCols );

                //Shift the column IDs
                int colPos;
                for ( colPos = 0; colPos < m_numCols; colPos++ )
                {
                    if ( m_colAt[colPos] > colID )
                        m_colAt[colPos] -= numCols;
                }
            }

            if ( !m_colWidths.IsEmpty() )
            {
                m_colWidths.RemoveAt( pos, numCols );
                m_colRights.RemoveAt( pos, numCols );

                int w = 0;
                int colPos;
                for ( colPos = 0; colPos < m_numCols; colPos++ )
                {
                    i = GetColAt( colPos );

                    w += GetColWidth(i);
                    m_colRights[i] = w;
                }
            }

            if ( !m_numCols )
            {
                m_currentCellCoords = wxGridNoCellCoords;
            }
            else
            {
                if ( m_currentCellCoords.GetCol() >= m_numCols )
                  m_currentCellCoords.Set( 0, 0 );
            }

            if ( m_selection )
                m_selection->UpdateCols( pos, -((int)numCols) );
            wxGridCellAttrProvider * attrProvider = m_table->GetAttrProvider();
            if (attrProvider)
            {
                attrProvider->UpdateAttrCols( pos, -((int)numCols) );

// ifdef'd out following patch from Paul Gammans
#if 0
                // No need to touch row attributes, unless we
                // removed _all_ columns, in this case, we remove
                // all row attributes.
                // I hate to do this here, but the
                // needed data is not available inside UpdateAttrCols.
                if ( !GetNumberCols() )
                    attrProvider->UpdateAttrRows( 0, -GetNumberRows() );
#endif
            }

            if ( !GetBatchCount() )
            {
                CalcDimensions();
                m_colWindow->Refresh();
            }
        }
        result = true;
        break;
    }

    InvalidateBestSize();

    if (result && !GetBatchCount() )
        m_gridWin->Refresh();

    return result;
}

wxArrayInt wxGrid::CalcRowLabelsExposed( const wxRegion& reg ) const
{
    wxRegionIterator iter( reg );
    wxRect r;

    wxArrayInt  rowlabels;

    int top, bottom;
    while ( iter )
    {
        r = iter.GetRect();

        // TODO: remove this when we can...
        // There is a bug in wxMotif that gives garbage update
        // rectangles if you jump-scroll a long way by clicking the
        // scrollbar with middle button.  This is a work-around
        //
#if defined(__WXMOTIF__)
        int cw, ch;
        m_gridWin->GetClientSize( &cw, &ch );
        if ( r.GetTop() > ch )
            r.SetTop( 0 );
        r.SetBottom( wxMin( r.GetBottom(), ch ) );
#endif

        // logical bounds of update region
        //
        int dummy;
        CalcUnscrolledPosition( 0, r.GetTop(), &dummy, &top );
        CalcUnscrolledPosition( 0, r.GetBottom(), &dummy, &bottom );

        // find the row labels within these bounds
        //
        int row;
        for ( row = internalYToRow(top); row < m_numRows; row++ )
        {
            if ( GetRowBottom(row) < top )
                continue;

            if ( GetRowTop(row) > bottom )
                break;

            rowlabels.Add( row );
        }

        ++iter;
    }

    return rowlabels;
}

wxArrayInt wxGrid::CalcColLabelsExposed( const wxRegion& reg ) const
{
    wxRegionIterator iter( reg );
    wxRect r;

    wxArrayInt colLabels;

    int left, right;
    while ( iter )
    {
        r = iter.GetRect();

        // TODO: remove this when we can...
        // There is a bug in wxMotif that gives garbage update
        // rectangles if you jump-scroll a long way by clicking the
        // scrollbar with middle button.  This is a work-around
        //
#if defined(__WXMOTIF__)
        int cw, ch;
        m_gridWin->GetClientSize( &cw, &ch );
        if ( r.GetLeft() > cw )
            r.SetLeft( 0 );
        r.SetRight( wxMin( r.GetRight(), cw ) );
#endif

        // logical bounds of update region
        //
        int dummy;
        CalcUnscrolledPosition( r.GetLeft(), 0, &left, &dummy );
        CalcUnscrolledPosition( r.GetRight(), 0, &right, &dummy );

        // find the cells within these bounds
        //
        int col;
        int colPos;
        for ( colPos = GetColPos( internalXToCol(left) ); colPos < m_numCols; colPos++ )
        {
            col = GetColAt( colPos );

            if ( GetColRight(col) < left )
                continue;

            if ( GetColLeft(col) > right )
                break;

            colLabels.Add( col );
        }

        ++iter;
    }

    return colLabels;
}

wxGridCellCoordsArray wxGrid::CalcCellsExposed( const wxRegion& reg ) const
{
    wxRect r;

    wxGridCellCoordsArray  cellsExposed;

    int left, top, right, bottom;
    for ( wxRegionIterator iter(reg); iter; ++iter )
    {
        r = iter.GetRect();

        // Skip 0-height cells, they're invisible anyhow, don't waste time
        // getting their rectangles and so on.
        if ( !r.GetHeight() )
            continue;

        // TODO: remove this when we can...
        // There is a bug in wxMotif that gives garbage update
        // rectangles if you jump-scroll a long way by clicking the
        // scrollbar with middle button.  This is a work-around
        //
#if defined(__WXMOTIF__)
        int cw, ch;
        m_gridWin->GetClientSize( &cw, &ch );
        if ( r.GetTop() > ch ) r.SetTop( 0 );
        if ( r.GetLeft() > cw ) r.SetLeft( 0 );
        r.SetRight( wxMin( r.GetRight(), cw ) );
        r.SetBottom( wxMin( r.GetBottom(), ch ) );
#endif

        // logical bounds of update region
        //
        CalcUnscrolledPosition( r.GetLeft(), r.GetTop(), &left, &top );
        CalcUnscrolledPosition( r.GetRight(), r.GetBottom(), &right, &bottom );

        // find the cells within these bounds
        wxArrayInt cols;
        for ( int row = internalYToRow(top); row < m_numRows; row++ )
        {
            if ( GetRowBottom(row) <= top )
                continue;

            if ( GetRowTop(row) > bottom )
                break;

            // add all dirty cells in this row: notice that the columns which
            // are dirty don't depend on the row so we compute them only once
            // for the first dirty row and then reuse for all the next ones
            if ( cols.empty() )
            {
                // do determine the dirty columns
                for ( int pos = XToPos(left); pos <= XToPos(right); pos++ )
                    cols.push_back(GetColAt(pos));

                // if there are no dirty columns at all, nothing to do
                if ( cols.empty() )
                    break;
            }

            const size_t count = cols.size();
            for ( size_t n = 0; n < count; n++ )
                cellsExposed.Add(wxGridCellCoords(row, cols[n]));
        }
    }

    return cellsExposed;
}


void wxGrid::ProcessRowLabelMouseEvent( wxMouseEvent& event )
{
    int x, y, row;
    wxPoint pos( event.GetPosition() );
    CalcUnscrolledPosition( pos.x, pos.y, &x, &y );

    if ( event.Dragging() )
    {
        if (!m_isDragging)
            m_isDragging = true;

        if ( event.LeftIsDown() )
        {
            switch ( m_cursorMode )
            {
                case WXGRID_CURSOR_RESIZE_ROW:
                {
                    int cw, ch, left, dummy;
                    m_gridWin->GetClientSize( &cw, &ch );
                    CalcUnscrolledPosition( 0, 0, &left, &dummy );

                    wxClientDC dc( m_gridWin );
                    PrepareDC( dc );
                    y = wxMax( y,
                               GetRowTop(m_dragRowOrCol) +
                               GetRowMinimalHeight(m_dragRowOrCol) );
                    dc.SetLogicalFunction(wxINVERT);
                    if ( m_dragLastPos >= 0 )
                    {
                        dc.DrawLine( left, m_dragLastPos, left+cw, m_dragLastPos );
                    }
                    dc.DrawLine( left, y, left+cw, y );
                    m_dragLastPos = y;
                }
                break;

                case WXGRID_CURSOR_SELECT_ROW:
                {
                    if ( (row = YToRow( y )) >= 0 )
                    {
                        if ( m_selection )
                            m_selection->SelectRow(row, event);
                    }
                }
                break;

                // default label to suppress warnings about "enumeration value
                // 'xxx' not handled in switch
                default:
                    break;
            }
        }
        return;
    }

    if ( m_isDragging && (event.Entering() || event.Leaving()) )
        return;

    if (m_isDragging)
        m_isDragging = false;

    // ------------ Entering or leaving the window
    //
    if ( event.Entering() || event.Leaving() )
    {
        ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL, m_rowLabelWin);
    }

    // ------------ Left button pressed
    //
    else if ( event.LeftDown() )
    {
        row = YToEdgeOfRow(y);
        if ( row != wxNOT_FOUND && CanDragRowSize(row) )
        {
            ChangeCursorMode(WXGRID_CURSOR_RESIZE_ROW, m_rowLabelWin);
        }
        else // not a request to start resizing
        {
            row = YToRow(y);
            if ( row >= 0 &&
                 !SendEvent( wxEVT_GRID_LABEL_LEFT_CLICK, row, -1, event ) )
            {
                if ( !event.ShiftDown() && !event.CmdDown() )
                    ClearSelection();
                if ( m_selection )
                {
                    if ( event.ShiftDown() )
                    {
                        m_selection->SelectBlock
                                     (
                                        m_currentCellCoords.GetRow(), 0,
                                        row, GetNumberCols() - 1,
                                        event
                                     );
                    }
                    else
                    {
                        m_selection->SelectRow(row, event);
                    }
                }

                ChangeCursorMode(WXGRID_CURSOR_SELECT_ROW, m_rowLabelWin);
            }
        }
    }

    // ------------ Left double click
    //
    else if (event.LeftDClick() )
    {
        row = YToEdgeOfRow(y);
        if ( row != wxNOT_FOUND && CanDragRowSize(row) )
        {
            // adjust row height depending on label text
            //
            // TODO: generate RESIZING event, see #10754
            AutoSizeRowLabelSize( row );

            SendGridSizeEvent(wxEVT_GRID_ROW_SIZE, row, -1, event);

            ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL, GetColLabelWindow());
            m_dragLastPos = -1;
        }
        else // not on row separator or it's not resizable
        {
            row = YToRow(y);
            if ( row >=0 &&
                 !SendEvent( wxEVT_GRID_LABEL_LEFT_DCLICK, row, -1, event ) )
            {
                // no default action at the moment
            }
        }
    }

    // ------------ Left button released
    //
    else if ( event.LeftUp() )
    {
        if ( m_cursorMode == WXGRID_CURSOR_RESIZE_ROW )
            DoEndDragResizeRow(event);

        ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL, m_rowLabelWin);
        m_dragLastPos = -1;
    }

    // ------------ Right button down
    //
    else if ( event.RightDown() )
    {
        row = YToRow(y);
        if ( row >=0 &&
             !SendEvent( wxEVT_GRID_LABEL_RIGHT_CLICK, row, -1, event ) )
        {
            // no default action at the moment
        }
    }

    // ------------ Right double click
    //
    else if ( event.RightDClick() )
    {
        row = YToRow(y);
        if ( row >= 0 &&
             !SendEvent( wxEVT_GRID_LABEL_RIGHT_DCLICK, row, -1, event ) )
        {
            // no default action at the moment
        }
    }

    // ------------ No buttons down and mouse moving
    //
    else if ( event.Moving() )
    {
        m_dragRowOrCol = YToEdgeOfRow( y );
        if ( m_dragRowOrCol != wxNOT_FOUND )
        {
            if ( m_cursorMode == WXGRID_CURSOR_SELECT_CELL )
            {
                if ( CanDragRowSize(m_dragRowOrCol) )
                    ChangeCursorMode(WXGRID_CURSOR_RESIZE_ROW, m_rowLabelWin, false);
            }
        }
        else if ( m_cursorMode != WXGRID_CURSOR_SELECT_CELL )
        {
            ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL, m_rowLabelWin, false);
        }
    }
}

void wxGrid::UpdateColumnSortingIndicator(int col)
{
    wxCHECK_RET( col != wxNOT_FOUND, "invalid column index" );

    if ( m_useNativeHeader )
        GetGridColHeader()->UpdateColumn(col);
    else if ( m_nativeColumnLabels )
        m_colWindow->Refresh();
    //else: sorting indicator display not yet implemented in grid version
}

void wxGrid::SetSortingColumn(int col, bool ascending)
{
    if ( col == m_sortCol )
    {
        // we are already using this column for sorting (or not sorting at all)
        // but we might still change the sorting order, check for it
        if ( m_sortCol != wxNOT_FOUND && ascending != m_sortIsAscending )
        {
            m_sortIsAscending = ascending;

            UpdateColumnSortingIndicator(m_sortCol);
        }
    }
    else // we're changing the column used for sorting
    {
        const int sortColOld = m_sortCol;

        // change it before updating the column as we want GetSortingColumn()
        // to return the correct new value
        m_sortCol = col;

        if ( sortColOld != wxNOT_FOUND )
            UpdateColumnSortingIndicator(sortColOld);

        if ( m_sortCol != wxNOT_FOUND )
        {
            m_sortIsAscending = ascending;
            UpdateColumnSortingIndicator(m_sortCol);
        }
    }
}

void wxGrid::DoColHeaderClick(int col)
{
    // we consider that the grid was resorted if this event is processed and
    // not vetoed
    if ( SendEvent(wxEVT_GRID_COL_SORT, -1, col) == 1 )
    {
        SetSortingColumn(col, IsSortingBy(col) ? !m_sortIsAscending : true);
        Refresh();
    }
}

void wxGrid::DoStartResizeCol(int col)
{
    m_dragRowOrCol = col;
    m_dragLastPos = -1;
    DoUpdateResizeColWidth(GetColWidth(m_dragRowOrCol));
}

void wxGrid::DoUpdateResizeCol(int x)
{
    int cw, ch, dummy, top;
    m_gridWin->GetClientSize( &cw, &ch );
    CalcUnscrolledPosition( 0, 0, &dummy, &top );

    wxClientDC dc( m_gridWin );
    PrepareDC( dc );

    x = wxMax( x, GetColLeft(m_dragRowOrCol) + GetColMinimalWidth(m_dragRowOrCol));
    dc.SetLogicalFunction(wxINVERT);
    if ( m_dragLastPos >= 0 )
    {
        dc.DrawLine( m_dragLastPos, top, m_dragLastPos, top + ch );
    }
    dc.DrawLine( x, top, x, top + ch );
    m_dragLastPos = x;
}

void wxGrid::DoUpdateResizeColWidth(int w)
{
    DoUpdateResizeCol(GetColLeft(m_dragRowOrCol) + w);
}

void wxGrid::ProcessColLabelMouseEvent( wxMouseEvent& event )
{
    int x;
    CalcUnscrolledPosition( event.GetPosition().x, 0, &x, NULL );

    int col = XToCol(x);
    if ( event.Dragging() )
    {
        if (!m_isDragging)
        {
            m_isDragging = true;

            if ( m_cursorMode == WXGRID_CURSOR_MOVE_COL && col != -1 )
                DoStartMoveCol(col);
        }

        if ( event.LeftIsDown() )
        {
            switch ( m_cursorMode )
            {
                case WXGRID_CURSOR_RESIZE_COL:
                DoUpdateResizeCol(x);
                break;

                case WXGRID_CURSOR_SELECT_COL:
                {
                    if ( col != -1 )
                    {
                        if ( m_selection )
                            m_selection->SelectCol(col, event);
                    }
                }
                break;

                case WXGRID_CURSOR_MOVE_COL:
                {
                    int posNew = XToPos(x);
                    int colNew = GetColAt(posNew);

                    // determine the position of the drop marker
                    int markerX;
                    if ( x >= GetColLeft(colNew) + (GetColWidth(colNew) / 2) )
                        markerX = GetColRight(colNew);
                    else
                        markerX = GetColLeft(colNew);

                    if ( markerX != m_dragLastPos )
                    {
                        wxClientDC dc( GetColLabelWindow() );
                        DoPrepareDC(dc);

                        int cw, ch;
                        GetColLabelWindow()->GetClientSize( &cw, &ch );

                        markerX++;

                        //Clean up the last indicator
                        if ( m_dragLastPos >= 0 )
                        {
                            wxPen pen( GetColLabelWindow()->GetBackgroundColour(), 2 );
                            dc.SetPen(pen);
                            dc.DrawLine( m_dragLastPos + 1, 0, m_dragLastPos + 1, ch );
                            dc.SetPen(wxNullPen);

                            if ( XToCol( m_dragLastPos ) != -1 )
                                DrawColLabel( dc, XToCol( m_dragLastPos ) );
                        }

                        const wxColour *color;
                        //Moving to the same place? Don't draw a marker
                        if ( colNew == m_dragRowOrCol )
                            color = wxLIGHT_GREY;
                        else
                            color = wxBLUE;

                        //Draw the marker
                        wxPen pen( *color, 2 );
                        dc.SetPen(pen);

                        dc.DrawLine( markerX, 0, markerX, ch );

                        dc.SetPen(wxNullPen);

                        m_dragLastPos = markerX - 1;
                    }
                }
                break;

                // default label to suppress warnings about "enumeration value
                // 'xxx' not handled in switch
                default:
                    break;
            }
        }
        return;
    }

    if ( m_isDragging && (event.Entering() || event.Leaving()) )
        return;

    if (m_isDragging)
        m_isDragging = false;

    // ------------ Entering or leaving the window
    //
    if ( event.Entering() || event.Leaving() )
    {
        ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL, GetColLabelWindow());
    }

    // ------------ Left button pressed
    //
    else if ( event.LeftDown() )
    {
        int colEdge = XToEdgeOfCol(x);
        if ( colEdge != wxNOT_FOUND && CanDragColSize(colEdge) )
        {
            ChangeCursorMode(WXGRID_CURSOR_RESIZE_COL, GetColLabelWindow());
        }
        else // not a request to start resizing
        {
            if ( col >= 0 &&
                 !SendEvent( wxEVT_GRID_LABEL_LEFT_CLICK, -1, col, event ) )
            {
                if ( m_canDragColMove )
                {
                    //Show button as pressed
                    wxClientDC dc( GetColLabelWindow() );
                    int colLeft = GetColLeft( col );
                    int colRight = GetColRight( col ) - 1;
                    dc.SetPen( wxPen( GetColLabelWindow()->GetBackgroundColour(), 1 ) );
                    dc.DrawLine( colLeft, 1, colLeft, m_colLabelHeight-1 );
                    dc.DrawLine( colLeft, 1, colRight, 1 );

                    ChangeCursorMode(WXGRID_CURSOR_MOVE_COL, GetColLabelWindow());
                }
                else
                {
                    if ( !event.ShiftDown() && !event.CmdDown() )
                        ClearSelection();
                    if ( m_selection )
                    {
                        if ( event.ShiftDown() )
                        {
                            m_selection->SelectBlock
                                         (
                                            0, m_currentCellCoords.GetCol(),
                                            GetNumberRows() - 1, col,
                                            event
                                         );
                        }
                        else
                        {
                            m_selection->SelectCol(col, event);
                        }
                    }

                    ChangeCursorMode(WXGRID_CURSOR_SELECT_COL, GetColLabelWindow());
                }
            }
        }
    }

    // ------------ Left double click
    //
    if ( event.LeftDClick() )
    {
        const int colEdge = XToEdgeOfCol(x);
        if ( colEdge == -1 )
        {
            if ( col >= 0 &&
                 ! SendEvent( wxEVT_GRID_LABEL_LEFT_DCLICK, -1, col, event ) )
            {
                // no default action at the moment
            }
        }
        else
        {
            // adjust column width depending on label text
            //
            // TODO: generate RESIZING event, see #10754
            if ( !SendGridSizeEvent(wxEVT_GRID_COL_AUTO_SIZE, -1, colEdge, event) )
                AutoSizeColLabelSize( colEdge );

            SendGridSizeEvent(wxEVT_GRID_COL_SIZE, -1, colEdge, event);

            ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL, GetColLabelWindow());
            m_dragLastPos = -1;
        }
    }

    // ------------ Left button released
    //
    else if ( event.LeftUp() )
    {
        switch ( m_cursorMode )
        {
            case WXGRID_CURSOR_RESIZE_COL:
                DoEndDragResizeCol(event);
                break;

            case WXGRID_CURSOR_MOVE_COL:
                if ( m_dragLastPos == -1 || col == m_dragRowOrCol )
                {
                    // the column didn't actually move anywhere
                    if ( col != -1 )
                        DoColHeaderClick(col);
                    m_colWindow->Refresh();   // "unpress" the column
                }
                else
                {
                    // get the position of the column we're over
                    int pos = XToPos(x);

                    // insert the column being dragged either before or after
                    // it, depending on where exactly it was dropped, so
                    // find the index of the column we're over: notice
                    // that the existing "col" variable may be invalid but
                    // we need a valid one here
                    const int colValid = GetColAt(pos);

                    // and check if we're on the "near" (usually left but right
                    // in RTL case) part of the column
                    const int middle = GetColLeft(colValid) +
                                            GetColWidth(colValid)/2;
                    const bool onNearPart = (x <= middle);

                    // adjust for the column being dragged itself
                    if ( pos < GetColPos(m_dragRowOrCol) )
                        pos++;

                    // and if it's on the near part of the target column,
                    // insert it before it, not after
                    if ( onNearPart )
                        pos--;

                    DoEndMoveCol(pos);
                }
                break;

            case WXGRID_CURSOR_SELECT_COL:
            case WXGRID_CURSOR_SELECT_CELL:
            case WXGRID_CURSOR_RESIZE_ROW:
            case WXGRID_CURSOR_SELECT_ROW:
                if ( col != -1 )
                    DoColHeaderClick(col);
                break;
        }

        ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL, GetColLabelWindow());
        m_dragLastPos = -1;
    }

    // ------------ Right button down
    //
    else if ( event.RightDown() )
    {
        if ( col >= 0 &&
             !SendEvent( wxEVT_GRID_LABEL_RIGHT_CLICK, -1, col, event ) )
        {
            // no default action at the moment
        }
    }

    // ------------ Right double click
    //
    else if ( event.RightDClick() )
    {
        if ( col >= 0 &&
             !SendEvent( wxEVT_GRID_LABEL_RIGHT_DCLICK, -1, col, event ) )
        {
            // no default action at the moment
        }
    }

    // ------------ No buttons down and mouse moving
    //
    else if ( event.Moving() )
    {
        m_dragRowOrCol = XToEdgeOfCol( x );
        if ( m_dragRowOrCol >= 0 )
        {
            if ( m_cursorMode == WXGRID_CURSOR_SELECT_CELL )
            {
                if ( CanDragColSize(m_dragRowOrCol) )
                    ChangeCursorMode(WXGRID_CURSOR_RESIZE_COL, GetColLabelWindow(), false);
            }
        }
        else if ( m_cursorMode != WXGRID_CURSOR_SELECT_CELL )
        {
            ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL, GetColLabelWindow(), false);
        }
    }
}

void wxGrid::ProcessCornerLabelMouseEvent( wxMouseEvent& event )
{
    if ( event.LeftDown() )
    {
        // indicate corner label by having both row and
        // col args == -1
        //
        if ( !SendEvent( wxEVT_GRID_LABEL_LEFT_CLICK, -1, -1, event ) )
        {
            SelectAll();
        }
    }
    else if ( event.LeftDClick() )
    {
        SendEvent( wxEVT_GRID_LABEL_LEFT_DCLICK, -1, -1, event );
    }
    else if ( event.RightDown() )
    {
        if ( !SendEvent( wxEVT_GRID_LABEL_RIGHT_CLICK, -1, -1, event ) )
        {
            // no default action at the moment
        }
    }
    else if ( event.RightDClick() )
    {
        if ( !SendEvent( wxEVT_GRID_LABEL_RIGHT_DCLICK, -1, -1, event ) )
        {
            // no default action at the moment
        }
    }
}

void wxGrid::CancelMouseCapture()
{
    // cancel operation currently in progress, whatever it is
    if ( m_winCapture )
    {
        m_isDragging = false;
        m_startDragPos = wxDefaultPosition;

        m_cursorMode = WXGRID_CURSOR_SELECT_CELL;
        m_winCapture->SetCursor( *wxSTANDARD_CURSOR );
        m_winCapture = NULL;

        // remove traces of whatever we drew on screen
        Refresh();
    }
}

void wxGrid::ChangeCursorMode(CursorMode mode,
                              wxWindow *win,
                              bool captureMouse)
{
#if wxUSE_LOG_TRACE
    static const wxChar *const cursorModes[] =
    {
        wxT("SELECT_CELL"),
        wxT("RESIZE_ROW"),
        wxT("RESIZE_COL"),
        wxT("SELECT_ROW"),
        wxT("SELECT_COL"),
        wxT("MOVE_COL"),
    };

    wxLogTrace(wxT("grid"),
               wxT("wxGrid cursor mode (mouse capture for %s): %s -> %s"),
               win == m_colWindow ? wxT("colLabelWin")
                                  : win ? wxT("rowLabelWin")
                                        : wxT("gridWin"),
               cursorModes[m_cursorMode], cursorModes[mode]);
#endif // wxUSE_LOG_TRACE

    if ( mode == m_cursorMode &&
         win == m_winCapture &&
         captureMouse == (m_winCapture != NULL))
        return;

    if ( !win )
    {
        // by default use the grid itself
        win = m_gridWin;
    }

    if ( m_winCapture )
    {
        m_winCapture->ReleaseMouse();
        m_winCapture = NULL;
    }

    m_cursorMode = mode;

    switch ( m_cursorMode )
    {
        case WXGRID_CURSOR_RESIZE_ROW:
            win->SetCursor( m_rowResizeCursor );
            break;

        case WXGRID_CURSOR_RESIZE_COL:
            win->SetCursor( m_colResizeCursor );
            break;

        case WXGRID_CURSOR_MOVE_COL:
            win->SetCursor( wxCursor(wxCURSOR_HAND) );
            break;

        default:
            win->SetCursor( *wxSTANDARD_CURSOR );
            break;
    }

    // we need to capture mouse when resizing
    bool resize = m_cursorMode == WXGRID_CURSOR_RESIZE_ROW ||
                  m_cursorMode == WXGRID_CURSOR_RESIZE_COL;

    if ( captureMouse && resize )
    {
        win->CaptureMouse();
        m_winCapture = win;
    }
}

// ----------------------------------------------------------------------------
// grid mouse event processing
// ----------------------------------------------------------------------------

bool
wxGrid::DoGridCellDrag(wxMouseEvent& event,
                       const wxGridCellCoords& coords,
                       bool isFirstDrag)
{
    bool performDefault = true ;
    
    if ( coords == wxGridNoCellCoords )
        return performDefault; // we're outside any valid cell

    // Hide the edit control, so it won't interfere with drag-shrinking.
    if ( IsCellEditControlShown() )
    {
        HideCellEditControl();
        SaveEditControlValue();
    }

    switch ( event.GetModifiers() )
    {
        case wxMOD_CONTROL:
            if ( m_selectedBlockCorner == wxGridNoCellCoords)
                m_selectedBlockCorner = coords;
            if ( isFirstDrag )
                SetGridCursor(coords);
            UpdateBlockBeingSelected(m_currentCellCoords, coords);
            break;

        case wxMOD_NONE:
            if ( CanDragCell() )
            {
                if ( isFirstDrag )
                {
                    if ( m_selectedBlockCorner == wxGridNoCellCoords)
                        m_selectedBlockCorner = coords;

                    // if event is handled by user code, no further processing
                    if ( SendEvent(wxEVT_GRID_CELL_BEGIN_DRAG, coords, event) != 0 )
                        performDefault = false;
                    
                    return performDefault;
                }
            }

            UpdateBlockBeingSelected(m_currentCellCoords, coords);
            break;

        default:
            // we don't handle the other key modifiers
            event.Skip();
    }
    
    return performDefault;
}

void wxGrid::DoGridLineDrag(wxMouseEvent& event, const wxGridOperations& oper)
{
    wxClientDC dc(m_gridWin);
    PrepareDC(dc);
    dc.SetLogicalFunction(wxINVERT);

    const wxRect rectWin(CalcUnscrolledPosition(wxPoint(0, 0)),
                         m_gridWin->GetClientSize());

    // erase the previously drawn line, if any
    if ( m_dragLastPos >= 0 )
        oper.DrawParallelLineInRect(dc, rectWin, m_dragLastPos);

    // we need the vertical position for rows and horizontal for columns here
    m_dragLastPos = oper.Dual().Select(CalcUnscrolledPosition(event.GetPosition()));

    // don't allow resizing beneath the minimal size
    const int posMin = oper.GetLineStartPos(this, m_dragRowOrCol) +
                        oper.GetMinimalLineSize(this, m_dragRowOrCol);
    if ( m_dragLastPos < posMin )
        m_dragLastPos = posMin;

    // and draw it at the new position
    oper.DrawParallelLineInRect(dc, rectWin, m_dragLastPos);
}

void wxGrid::DoGridDragEvent(wxMouseEvent& event, const wxGridCellCoords& coords)
{
    if ( !m_isDragging )
    {
        // Don't start doing anything until the mouse has been dragged far
        // enough
        const wxPoint& pt = event.GetPosition();
        if ( m_startDragPos == wxDefaultPosition )
        {
            m_startDragPos = pt;
            return;
        }

        if ( abs(m_startDragPos.x - pt.x) <= DRAG_SENSITIVITY &&
                abs(m_startDragPos.y - pt.y) <= DRAG_SENSITIVITY )
            return;
    }

    const bool isFirstDrag = !m_isDragging;
    m_isDragging = true;

    switch ( m_cursorMode )
    {
        case WXGRID_CURSOR_SELECT_CELL:
            // no further handling if handled by user
            if ( DoGridCellDrag(event, coords, isFirstDrag) == false )
                return;
            break;

        case WXGRID_CURSOR_RESIZE_ROW:
            DoGridLineDrag(event, wxGridRowOperations());
            break;

        case WXGRID_CURSOR_RESIZE_COL:
            DoGridLineDrag(event, wxGridColumnOperations());
            break;

        default:
            event.Skip();
    }

    if ( isFirstDrag )
    {
        wxASSERT_MSG( !m_winCapture, "shouldn't capture the mouse twice" );

        m_winCapture = m_gridWin;
        m_winCapture->CaptureMouse();
    }
}

void
wxGrid::DoGridCellLeftDown(wxMouseEvent& event,
                           const wxGridCellCoords& coords,
                           const wxPoint& pos)
{
    if ( SendEvent(wxEVT_GRID_CELL_LEFT_CLICK, coords, event) )
    {
        // event handled by user code, no need to do anything here
        return;
    }

    if ( !event.CmdDown() )
        ClearSelection();

    if ( event.ShiftDown() )
    {
        if ( m_selection )
        {
            m_selection->SelectBlock(m_currentCellCoords, coords, event);
            m_selectedBlockCorner = coords;
        }
    }
    else if ( XToEdgeOfCol(pos.x) < 0 && YToEdgeOfRow(pos.y) < 0 )
    {
        DisableCellEditControl();
        MakeCellVisible( coords );

        if ( event.CmdDown() )
        {
            if ( m_selection )
            {
                m_selection->ToggleCellSelection(coords, event);
            }

            m_selectedBlockTopLeft = wxGridNoCellCoords;
            m_selectedBlockBottomRight = wxGridNoCellCoords;
            m_selectedBlockCorner = coords;
        }
        else
        {
            if ( m_selection )
            {
                // In row or column selection mode just clicking on the cell
                // should select the row or column containing it: this is more
                // convenient for the kinds of controls that use such selection
                // mode and is compatible with 2.8 behaviour (see #12062).
                switch ( m_selection->GetSelectionMode() )
                {
                    case wxGridSelectCells:
                    case wxGridSelectRowsOrColumns:
                        // nothing to do in these cases
                        break;

                    case wxGridSelectRows:
                        m_selection->SelectRow(coords.GetRow());
                        break;

                    case wxGridSelectColumns:
                        m_selection->SelectCol(coords.GetCol());
                        break;
                }
            }

            m_waitForSlowClick = m_currentCellCoords == coords &&
                                        coords != wxGridNoCellCoords;
            SetCurrentCell( coords );
        }
    }
}

void
wxGrid::DoGridCellLeftDClick(wxMouseEvent& event,
                             const wxGridCellCoords& coords,
                             const wxPoint& pos)
{
    if ( XToEdgeOfCol(pos.x) < 0 && YToEdgeOfRow(pos.y) < 0 )
    {
        if ( !SendEvent(wxEVT_GRID_CELL_LEFT_DCLICK, coords, event) )
        {
            // we want double click to select a cell and start editing
            // (i.e. to behave in same way as sequence of two slow clicks):
            m_waitForSlowClick = true;
        }
    }
}

void
wxGrid::DoGridCellLeftUp(wxMouseEvent& event, const wxGridCellCoords& coords)
{
    if ( m_cursorMode == WXGRID_CURSOR_SELECT_CELL )
    {
        if (m_winCapture)
        {
            m_winCapture->ReleaseMouse();
            m_winCapture = NULL;
        }

        if ( coords == m_currentCellCoords && m_waitForSlowClick && CanEnableCellControl() )
        {
            ClearSelection();
            EnableCellEditControl();

            wxGridCellAttr *attr = GetCellAttr(coords);
            wxGridCellEditor *editor = attr->GetEditor(this, coords.GetRow(), coords.GetCol());
            editor->StartingClick();
            editor->DecRef();
            attr->DecRef();

            m_waitForSlowClick = false;
        }
        else if ( m_selectedBlockTopLeft != wxGridNoCellCoords &&
             m_selectedBlockBottomRight != wxGridNoCellCoords )
        {
            if ( m_selection )
            {
                m_selection->SelectBlock( m_selectedBlockTopLeft,
                                          m_selectedBlockBottomRight,
                                          event );
            }

            m_selectedBlockTopLeft = wxGridNoCellCoords;
            m_selectedBlockBottomRight = wxGridNoCellCoords;

            // Show the edit control, if it has been hidden for
            // drag-shrinking.
            ShowCellEditControl();
        }
    }
    else if ( m_cursorMode == WXGRID_CURSOR_RESIZE_ROW )
    {
        ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL);
        DoEndDragResizeRow(event);
    }
    else if ( m_cursorMode == WXGRID_CURSOR_RESIZE_COL )
    {
        ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL);
        DoEndDragResizeCol(event);
    }

    m_dragLastPos = -1;
}

void
wxGrid::DoGridMouseMoveEvent(wxMouseEvent& WXUNUSED(event),
                             const wxGridCellCoords& coords,
                             const wxPoint& pos)
{
    if ( coords.GetRow() < 0 || coords.GetCol() < 0 )
    {
        // out of grid cell area
        ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL);
        return;
    }

    int dragRow = YToEdgeOfRow( pos.y );
    int dragCol = XToEdgeOfCol( pos.x );

    // Dragging on the corner of a cell to resize in both
    // directions is not implemented yet...
    //
    if ( dragRow >= 0 && dragCol >= 0 )
    {
        ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL);
        return;
    }

    if ( dragRow >= 0 && CanDragGridSize() && CanDragRowSize(dragRow) )
    {
        if ( m_cursorMode == WXGRID_CURSOR_SELECT_CELL )
        {
            m_dragRowOrCol = dragRow;
            ChangeCursorMode(WXGRID_CURSOR_RESIZE_ROW, NULL, false);
        }
    }
    // When using the native header window we can only resize the columns by
    // dragging the dividers in it because we can't make it enter into the
    // column resizing mode programmatically
    else if ( dragCol >= 0 && !m_useNativeHeader &&
                CanDragGridSize() && CanDragColSize(dragCol) )
    {
        if ( m_cursorMode == WXGRID_CURSOR_SELECT_CELL )
        {
            m_dragRowOrCol = dragCol;
            ChangeCursorMode(WXGRID_CURSOR_RESIZE_COL, NULL, false);
        }
    }
    else // Neither on a row or col edge
    {
        if ( m_cursorMode != WXGRID_CURSOR_SELECT_CELL )
        {
            ChangeCursorMode(WXGRID_CURSOR_SELECT_CELL);
        }
    }
}

void wxGrid::ProcessGridCellMouseEvent(wxMouseEvent& event)
{
    if ( event.Entering() || event.Leaving() )
    {
        // we don't care about these events but we must not reset m_isDragging
        // if they happen so return before anything else is done
        event.Skip();
        return;
    }

    const wxPoint pos = CalcUnscrolledPosition(event.GetPosition());

    // coordinates of the cell under mouse
    wxGridCellCoords coords = XYToCell(pos);

    int cell_rows, cell_cols;
    GetCellSize( coords.GetRow(), coords.GetCol(), &cell_rows, &cell_cols );
    if ( (cell_rows < 0) || (cell_cols < 0) )
    {
        coords.SetRow(coords.GetRow() + cell_rows);
        coords.SetCol(coords.GetCol() + cell_cols);
    }

    if ( event.Dragging() )
    {
        if ( event.LeftIsDown() )
            DoGridDragEvent(event, coords);
        else
            event.Skip();
        return;
    }

    m_isDragging = false;
    m_startDragPos = wxDefaultPosition;

    // deal with various button presses
    if ( event.IsButton() )
    {
        if ( coords != wxGridNoCellCoords )
        {
            DisableCellEditControl();

            if ( event.LeftDown() )
                DoGridCellLeftDown(event, coords, pos);
            else if ( event.LeftDClick() )
                DoGridCellLeftDClick(event, coords, pos);
            else if ( event.RightDown() )
                SendEvent(wxEVT_GRID_CELL_RIGHT_CLICK, coords, event);
            else if ( event.RightDClick() )
                SendEvent(wxEVT_GRID_CELL_RIGHT_DCLICK, coords, event);
        }

        // this one should be called even if we're not over any cell
        if ( event.LeftUp() )
        {
            DoGridCellLeftUp(event, coords);
        }
    }
    else if ( event.Moving() )
    {
        DoGridMouseMoveEvent(event, coords, pos);
    }
    else // unknown mouse event?
    {
        event.Skip();
    }
}

// this function returns true only if the size really changed
bool wxGrid::DoEndDragResizeLine(const wxGridOperations& oper)
{
    if ( m_dragLastPos == -1 )
        return false;

    const wxGridOperations& doper = oper.Dual();

    const wxSize size = m_gridWin->GetClientSize();

    const wxPoint ptOrigin = CalcUnscrolledPosition(wxPoint(0, 0));

    // erase the last line we drew
    wxClientDC dc(m_gridWin);
    PrepareDC(dc);
    dc.SetLogicalFunction(wxINVERT);

    const int posLineStart = oper.Select(ptOrigin);
    const int posLineEnd = oper.Select(ptOrigin) + oper.Select(size);

    oper.DrawParallelLine(dc, posLineStart, posLineEnd, m_dragLastPos);

    // temporarily hide the edit control before resizing
    HideCellEditControl();
    SaveEditControlValue();

    // do resize the line
    const int lineStart = oper.GetLineStartPos(this, m_dragRowOrCol);
    const int lineSizeOld = oper.GetLineSize(this, m_dragRowOrCol);
    oper.SetLineSize(this, m_dragRowOrCol,
                     wxMax(m_dragLastPos - lineStart,
                           oper.GetMinimalLineSize(this, m_dragRowOrCol)));
    const bool
        sizeChanged = oper.GetLineSize(this, m_dragRowOrCol) != lineSizeOld;

    m_dragLastPos = -1;

    // refresh now if we're not frozen
    if ( !GetBatchCount() )
    {
        // we need to refresh everything beyond the resized line in the header
        // window

        // get the position from which to refresh in the other direction
        wxRect rect(CellToRect(oper.MakeCoords(m_dragRowOrCol, 0)));
        rect.SetPosition(CalcScrolledPosition(rect.GetPosition()));

        // we only need the ordinate (for rows) or abscissa (for columns) here,
        // and need to cover the entire window in the other direction
        oper.Select(rect) = 0;

        wxRect rectHeader(rect.GetPosition(),
                          oper.MakeSize
                               (
                                    oper.GetHeaderWindowSize(this),
                                    doper.Select(size) - doper.Select(rect)
                               ));

        oper.GetHeaderWindow(this)->Refresh(true, &rectHeader);


        // also refresh the grid window: extend the rectangle
        if ( m_table )
        {
            oper.SelectSize(rect) = oper.Select(size);

            int subtractLines = 0;
            int line = doper.PosToLine(this, posLineStart);
            if ( line >= 0 )
            {
                // ensure that if we have a multi-cell block we redraw all of
                // it by increasing the refresh area to cover it entirely if a
                // part of it is affected
                const int lineEnd = doper.PosToLine(this, posLineEnd, true);
                for ( ; line < lineEnd; line++ )
                {
                    int cellLines = oper.Select(
                        GetCellSize(oper.MakeCoords(m_dragRowOrCol, line)));
                    if ( cellLines < subtractLines )
                        subtractLines = cellLines;
                }
            }

            int startPos =
                oper.GetLineStartPos(this, m_dragRowOrCol + subtractLines);
            startPos = doper.CalcScrolledPosition(this, startPos);

            doper.Select(rect) = startPos;
            doper.SelectSize(rect) = doper.Select(size) - startPos;

            m_gridWin->Refresh(false, &rect);
        }
    }

    // show the edit control back again
    ShowCellEditControl();

    return sizeChanged;
}

void wxGrid::DoEndDragResizeRow(const wxMouseEvent& event)
{
    // TODO: generate RESIZING event, see #10754

    if ( DoEndDragResizeLine(wxGridRowOperations()) )
        SendGridSizeEvent(wxEVT_GRID_ROW_SIZE, m_dragRowOrCol, -1, event);
}

void wxGrid::DoEndDragResizeCol(const wxMouseEvent& event)
{
    // TODO: generate RESIZING event, see #10754

    if ( DoEndDragResizeLine(wxGridColumnOperations()) )
        SendGridSizeEvent(wxEVT_GRID_COL_SIZE, -1, m_dragRowOrCol, event);
}

void wxGrid::DoStartMoveCol(int col)
{
    m_dragRowOrCol = col;
}

void wxGrid::DoEndMoveCol(int pos)
{
    wxASSERT_MSG( m_dragRowOrCol != -1, "no matching DoStartMoveCol?" );

    if ( SendEvent(wxEVT_GRID_COL_MOVE, -1, m_dragRowOrCol) != -1 )
        SetColPos(m_dragRowOrCol, pos);
    //else: vetoed by user

    m_dragRowOrCol = -1;
}

void wxGrid::RefreshAfterColPosChange()
{
    // recalculate the column rights as the column positions have changed,
    // unless we calculate them dynamically because all columns widths are the
    // same and it's easy to do
    if ( !m_colWidths.empty() )
    {
        int colRight = 0;
        for ( int colPos = 0; colPos < m_numCols; colPos++ )
        {
            int colID = GetColAt( colPos );

            // Ignore the currently hidden columns.
            const int width = m_colWidths[colID];
            if ( width > 0 )
                colRight += width;

            m_colRights[colID] = colRight;
        }
    }

    // and make the changes visible
    if ( m_useNativeHeader )
    {
        if ( m_colAt.empty() )
            GetGridColHeader()->ResetColumnsOrder();
        else
            GetGridColHeader()->SetColumnsOrder(m_colAt);
    }
    else
    {
        m_colWindow->Refresh();
    }
    m_gridWin->Refresh();
}

void wxGrid::SetColumnsOrder(const wxArrayInt& order)
{
    m_colAt = order;

    RefreshAfterColPosChange();
}

void wxGrid::SetColPos(int idx, int pos)
{
    // we're going to need m_colAt now, initialize it if needed
    if ( m_colAt.empty() )
    {
        m_colAt.reserve(m_numCols);
        for ( int i = 0; i < m_numCols; i++ )
            m_colAt.push_back(i);
    }

    wxHeaderCtrl::MoveColumnInOrderArray(m_colAt, idx, pos);

    RefreshAfterColPosChange();
}

void wxGrid::ResetColPos()
{
    m_colAt.clear();

    RefreshAfterColPosChange();
}

void wxGrid::EnableDragColMove( bool enable )
{
    if ( m_canDragColMove == enable )
        return;

    if ( m_useNativeHeader )
    {
        // update all columns to make them [not] reorderable
        GetGridColHeader()->SetColumnCount(m_numCols);
    }

    m_canDragColMove = enable;

    // we use to call ResetColPos() from here if !enable but this doesn't seem
    // right as it would mean there would be no way to "freeze" the current
    // columns order by disabling moving them after putting them in the desired
    // order, whereas now you can always call ResetColPos() manually if needed
}


//
// ------ interaction with data model
//
bool wxGrid::ProcessTableMessage( wxGridTableMessage& msg )
{
    switch ( msg.GetId() )
    {
        case wxGRIDTABLE_REQUEST_VIEW_GET_VALUES:
            return GetModelValues();

        case wxGRIDTABLE_REQUEST_VIEW_SEND_VALUES:
            return SetModelValues();

        case wxGRIDTABLE_NOTIFY_ROWS_INSERTED:
        case wxGRIDTABLE_NOTIFY_ROWS_APPENDED:
        case wxGRIDTABLE_NOTIFY_ROWS_DELETED:
        case wxGRIDTABLE_NOTIFY_COLS_INSERTED:
        case wxGRIDTABLE_NOTIFY_COLS_APPENDED:
        case wxGRIDTABLE_NOTIFY_COLS_DELETED:
            return Redimension( msg );

        default:
            return false;
    }
}

// The behaviour of this function depends on the grid table class
// Clear() function. For the default wxGridStringTable class the
// behaviour is to replace all cell contents with wxEmptyString but
// not to change the number of rows or cols.
//
void wxGrid::ClearGrid()
{
    if ( m_table )
    {
        if (IsCellEditControlEnabled())
            DisableCellEditControl();

        m_table->Clear();
        if (!GetBatchCount())
            m_gridWin->Refresh();
    }
}

bool
wxGrid::DoModifyLines(bool (wxGridTableBase::*funcModify)(size_t, size_t),
                      int pos, int num, bool WXUNUSED(updateLabels) )
{
    wxCHECK_MSG( m_created, false, "must finish creating the grid first" );

    if ( !m_table )
        return false;

    if ( IsCellEditControlEnabled() )
        DisableCellEditControl();

    return (m_table->*funcModify)(pos, num);

    // the table will have sent the results of the insert row
    // operation to this view object as a grid table message
}

bool
wxGrid::DoAppendLines(bool (wxGridTableBase::*funcAppend)(size_t),
                      int num, bool WXUNUSED(updateLabels))
{
    wxCHECK_MSG( m_created, false, "must finish creating the grid first" );

    if ( !m_table )
        return false;

    return (m_table->*funcAppend)(num);
}

// ----------------------------------------------------------------------------
// event generation helpers
// ----------------------------------------------------------------------------

bool
wxGrid::SendGridSizeEvent(wxEventType type,
                      int row, int col,
                      const wxMouseEvent& mouseEv)
{
   int rowOrCol = row == -1 ? col : row;

   wxGridSizeEvent gridEvt( GetId(),
           type,
           this,
           rowOrCol,
           mouseEv.GetX() + GetRowLabelSize(),
           mouseEv.GetY() + GetColLabelSize(),
           mouseEv);

   return GetEventHandler()->ProcessEvent(gridEvt);
}

// Generate a grid event based on a mouse event and return:
//  -1 if the event was vetoed
//  +1 if the event was processed (but not vetoed)
//   0 if the event wasn't handled
int
wxGrid::SendEvent(const wxEventType type,
                  int row, int col,
                  const wxMouseEvent& mouseEv)
{
   bool claimed, vetoed;

   if ( type == wxEVT_GRID_RANGE_SELECT )
   {
       // Right now, it should _never_ end up here!
       wxGridRangeSelectEvent gridEvt( GetId(),
               type,
               this,
               m_selectedBlockTopLeft,
               m_selectedBlockBottomRight,
               true,
               mouseEv);

       claimed = GetEventHandler()->ProcessEvent(gridEvt);
       vetoed = !gridEvt.IsAllowed();
   }
   else if ( type == wxEVT_GRID_LABEL_LEFT_CLICK ||
             type == wxEVT_GRID_LABEL_LEFT_DCLICK ||
             type == wxEVT_GRID_LABEL_RIGHT_CLICK ||
             type == wxEVT_GRID_LABEL_RIGHT_DCLICK )
   {
       wxPoint pos = mouseEv.GetPosition();

       if ( mouseEv.GetEventObject() == GetGridRowLabelWindow() )
           pos.y += GetColLabelSize();
       if ( mouseEv.GetEventObject() == GetGridColLabelWindow() )
           pos.x += GetRowLabelSize();

       wxGridEvent gridEvt( GetId(),
               type,
               this,
               row, col,
               pos.x,
               pos.y,
               false,
               mouseEv);
       claimed = GetEventHandler()->ProcessEvent(gridEvt);
       vetoed = !gridEvt.IsAllowed();
   }
   else
   {
       wxGridEvent gridEvt( GetId(),
               type,
               this,
               row, col,
               mouseEv.GetX() + GetRowLabelSize(),
               mouseEv.GetY() + GetColLabelSize(),
               false,
               mouseEv);

       if ( type == wxEVT_GRID_CELL_BEGIN_DRAG )
       {
           // by default the dragging is not supported, the user code must
           // explicitly allow the event for it to take place
           gridEvt.Veto();
       }
              
       claimed = GetEventHandler()->ProcessEvent(gridEvt);
       vetoed = !gridEvt.IsAllowed();
   }

   // A Veto'd event may not be `claimed' so test this first
   if (vetoed)
       return -1;

   return claimed ? 1 : 0;
}

// Generate a grid event of specified type, return value same as above
//
int
wxGrid::SendEvent(const wxEventType type, int row, int col, const wxString& s)
{
    wxGridEvent gridEvt( GetId(), type, this, row, col );
    gridEvt.SetString(s);

    const bool claimed = GetEventHandler()->ProcessEvent(gridEvt);

    // A Veto'd event may not be `claimed' so test this first
    if ( !gridEvt.IsAllowed() )
        return -1;

    return claimed ? 1 : 0;
}

void wxGrid::OnPaint( wxPaintEvent& WXUNUSED(event) )
{
    // needed to prevent zillions of paint events on MSW
    wxPaintDC dc(this);
}

void wxGrid::Refresh(bool eraseb, const wxRect* rect)
{
    // Don't do anything if between Begin/EndBatch...
    // EndBatch() will do all this on the last nested one anyway.
    if ( m_created && !GetBatchCount() )
    {
        // Refresh to get correct scrolled position:
        wxScrolledWindow::Refresh(eraseb, rect);

        if (rect)
        {
            int rect_x, rect_y, rectWidth, rectHeight;
            int width_label, width_cell, height_label, height_cell;
            int x, y;

            // Copy rectangle can get scroll offsets..
            rect_x = rect->GetX();
            rect_y = rect->GetY();
            rectWidth = rect->GetWidth();
            rectHeight = rect->GetHeight();

            width_label = m_rowLabelWidth - rect_x;
            if (width_label > rectWidth)
                width_label = rectWidth;

            height_label = m_colLabelHeight - rect_y;
            if (height_label > rectHeight)
                height_label = rectHeight;

            if (rect_x > m_rowLabelWidth)
            {
                x = rect_x - m_rowLabelWidth;
                width_cell = rectWidth;
            }
            else
            {
                x = 0;
                width_cell = rectWidth - (m_rowLabelWidth - rect_x);
            }

            if (rect_y > m_colLabelHeight)
            {
                y = rect_y - m_colLabelHeight;
                height_cell = rectHeight;
            }
            else
            {
                y = 0;
                height_cell = rectHeight - (m_colLabelHeight - rect_y);
            }

            // Paint corner label part intersecting rect.
            if ( width_label > 0 && height_label > 0 )
            {
                wxRect anotherrect(rect_x, rect_y, width_label, height_label);
                m_cornerLabelWin->Refresh(eraseb, &anotherrect);
            }

            // Paint col labels part intersecting rect.
            if ( width_cell > 0 && height_label > 0 )
            {
                wxRect anotherrect(x, rect_y, width_cell, height_label);
                m_colWindow->Refresh(eraseb, &anotherrect);
            }

            // Paint row labels part intersecting rect.
            if ( width_label > 0 && height_cell > 0 )
            {
                wxRect anotherrect(rect_x, y, width_label, height_cell);
                m_rowLabelWin->Refresh(eraseb, &anotherrect);
            }

            // Paint cell area part intersecting rect.
            if ( width_cell > 0 && height_cell > 0 )
            {
                wxRect anotherrect(x, y, width_cell, height_cell);
                m_gridWin->Refresh(eraseb, &anotherrect);
            }
        }
        else
        {
            m_cornerLabelWin->Refresh(eraseb, NULL);
            m_colWindow->Refresh(eraseb, NULL);
            m_rowLabelWin->Refresh(eraseb, NULL);
            m_gridWin->Refresh(eraseb, NULL);
        }
    }
}

void wxGrid::OnSize(wxSizeEvent& WXUNUSED(event))
{
    if (m_targetWindow != this) // check whether initialisation has been done
    {
        // reposition our children windows
        CalcWindowSizes();
    }
}

void wxGrid::OnKeyDown( wxKeyEvent& event )
{
    if ( m_inOnKeyDown )
    {
        // shouldn't be here - we are going round in circles...
        //
        wxFAIL_MSG( wxT("wxGrid::OnKeyDown called while already active") );
    }

    m_inOnKeyDown = true;

    // propagate the event up and see if it gets processed
    wxWindow *parent = GetParent();
    wxKeyEvent keyEvt( event );
    keyEvt.SetEventObject( parent );

    if ( !parent->GetEventHandler()->ProcessEvent( keyEvt ) )
    {
        if (GetLayoutDirection() == wxLayout_RightToLeft)
        {
            if (event.GetKeyCode() == WXK_RIGHT)
                event.m_keyCode = WXK_LEFT;
            else if (event.GetKeyCode() == WXK_LEFT)
                event.m_keyCode = WXK_RIGHT;
        }

        // try local handlers
        switch ( event.GetKeyCode() )
        {
            case WXK_UP:
                if ( event.ControlDown() )
                    MoveCursorUpBlock( event.ShiftDown() );
                else
                    MoveCursorUp( event.ShiftDown() );
                break;

            case WXK_DOWN:
                if ( event.ControlDown() )
                    MoveCursorDownBlock( event.ShiftDown() );
                else
                    MoveCursorDown( event.ShiftDown() );
                break;

            case WXK_LEFT:
                if ( event.ControlDown() )
                    MoveCursorLeftBlock( event.ShiftDown() );
                else
                    MoveCursorLeft( event.ShiftDown() );
                break;

            case WXK_RIGHT:
                if ( event.ControlDown() )
                    MoveCursorRightBlock( event.ShiftDown() );
                else
                    MoveCursorRight( event.ShiftDown() );
                break;

            case WXK_RETURN:
            case WXK_NUMPAD_ENTER:
                if ( event.ControlDown() )
                {
                    event.Skip();  // to let the edit control have the return
                }
                else
                {
                    if ( GetGridCursorRow() < GetNumberRows()-1 )
                    {
                        MoveCursorDown( event.ShiftDown() );
                    }
                    else
                    {
                        // at the bottom of a column
                        DisableCellEditControl();
                    }
                }
                break;

            case WXK_ESCAPE:
                ClearSelection();
                break;

            case WXK_TAB:
                {
                    // send an event to the grid's parents for custom handling
                    wxGridEvent gridEvt(GetId(), wxEVT_GRID_TABBING, this,
                                        GetGridCursorRow(), GetGridCursorCol(),
                                        -1, -1, false, event);
                    if ( ProcessWindowEvent(gridEvt) )
                    {
                        // the event has been handled so no need for more processing
                        break;
                    }
                }
                DoGridProcessTab( event );
                break;

            case WXK_HOME:
                GoToCell(event.ControlDown() ? 0
                                             : m_currentCellCoords.GetRow(),
                         0);
                break;

            case WXK_END:
                GoToCell(event.ControlDown() ? m_numRows - 1
                                             : m_currentCellCoords.GetRow(),
                         m_numCols - 1);
                break;

            case WXK_PAGEUP:
                MovePageUp();
                break;

            case WXK_PAGEDOWN:
                MovePageDown();
                break;

            case WXK_SPACE:
                // Ctrl-Space selects the current column, Shift-Space -- the
                // current row and Ctrl-Shift-Space -- everything
                switch ( m_selection ? event.GetModifiers() : wxMOD_NONE )
                {
                    case wxMOD_CONTROL:
                        m_selection->SelectCol(m_currentCellCoords.GetCol());
                        break;

                    case wxMOD_SHIFT:
                        m_selection->SelectRow(m_currentCellCoords.GetRow());
                        break;

                    case wxMOD_CONTROL | wxMOD_SHIFT:
                        m_selection->SelectBlock(0, 0,
                                                 m_numRows - 1, m_numCols - 1);
                        break;

                    case wxMOD_NONE:
                        if ( !IsEditable() )
                        {
                            MoveCursorRight(false);
                            break;
                        }
                        //else: fall through

                    default:
                        event.Skip();
                }
                break;

            default:
                event.Skip();
                break;
        }
    }

    m_inOnKeyDown = false;
}

void wxGrid::OnKeyUp( wxKeyEvent& event )
{
    // try local handlers
    //
    if ( event.GetKeyCode() == WXK_SHIFT )
    {
        if ( m_selectedBlockTopLeft != wxGridNoCellCoords &&
             m_selectedBlockBottomRight != wxGridNoCellCoords )
        {
            if ( m_selection )
            {
                m_selection->SelectBlock(
                    m_selectedBlockTopLeft,
                    m_selectedBlockBottomRight,
                    event);
            }
        }

        m_selectedBlockTopLeft = wxGridNoCellCoords;
        m_selectedBlockBottomRight = wxGridNoCellCoords;
        m_selectedBlockCorner = wxGridNoCellCoords;
    }
}

void wxGrid::OnChar( wxKeyEvent& event )
{
    // is it possible to edit the current cell at all?
    if ( !IsCellEditControlEnabled() && CanEnableCellControl() )
    {
        // yes, now check whether the cells editor accepts the key
        int row = m_currentCellCoords.GetRow();
        int col = m_currentCellCoords.GetCol();
        wxGridCellAttr *attr = GetCellAttr(row, col);
        wxGridCellEditor *editor = attr->GetEditor(this, row, col);

        // <F2> is special and will always start editing, for
        // other keys - ask the editor itself
        if ( (event.GetKeyCode() == WXK_F2 && !event.HasModifiers())
             || editor->IsAcceptedKey(event) )
        {
            // ensure cell is visble
            MakeCellVisible(row, col);
            EnableCellEditControl();

            // a problem can arise if the cell is not completely
            // visible (even after calling MakeCellVisible the
            // control is not created and calling StartingKey will
            // crash the app
            if ( event.GetKeyCode() != WXK_F2 && editor->IsCreated() && m_cellEditCtrlEnabled )
                editor->StartingKey(event);
        }
        else
        {
            event.Skip();
        }

        editor->DecRef();
        attr->DecRef();
    }
    else
    {
        event.Skip();
    }
}

void wxGrid::OnEraseBackground(wxEraseEvent&)
{
}

void wxGrid::DoGridProcessTab(wxKeyboardState& kbdState)
{
    const bool isForwardTab = !kbdState.ShiftDown();

    // TAB processing only changes when we are at the borders of the grid, so
    // let's first handle the common behaviour when we are not at the border.
    if ( isForwardTab )
    {
        if ( GetGridCursorCol() < GetNumberCols() - 1 )
        {
            MoveCursorRight( false );
            return;
        }
    }
    else // going back
    {
        if ( GetGridCursorCol() )
        {
            MoveCursorLeft( false );
            return;
        }
    }


    // We only get here if the cursor is at the border of the grid, apply the
    // configured behaviour.
    switch ( m_tabBehaviour )
    {
        case Tab_Stop:
            // Nothing special to do, we remain at the current cell.
            break;

        case Tab_Wrap:
            // Go to the beginning of the next or the end of the previous row.
            if ( isForwardTab )
            {
                if ( GetGridCursorRow() < GetNumberRows() - 1 )
                {
                    GoToCell( GetGridCursorRow() + 1, 0 );
                    return;
                }
            }
            else
            {
                if ( GetGridCursorRow() > 0 )
                {
                    GoToCell( GetGridCursorRow() - 1, GetNumberCols() - 1 );
                    return;
                }
            }
            break;

        case Tab_Leave:
            if ( Navigate( isForwardTab ? wxNavigationKeyEvent::IsForward
                                        : wxNavigationKeyEvent::IsBackward ) )
                return;
            break;
    }

    // If we remain in this cell, stop editing it if we were doing so.
    DisableCellEditControl();
}

bool wxGrid::SetCurrentCell( const wxGridCellCoords& coords )
{
    if ( SendEvent(wxEVT_GRID_SELECT_CELL, coords) == -1 )
    {
        // the event has been vetoed - do nothing
        return false;
    }

#if !defined(__WXMAC__)
    wxClientDC dc( m_gridWin );
    PrepareDC( dc );
#endif

    if ( m_currentCellCoords != wxGridNoCellCoords )
    {
        DisableCellEditControl();

        if ( IsVisible( m_currentCellCoords, false ) )
        {
            wxRect r;
            r = BlockToDeviceRect( m_currentCellCoords, m_currentCellCoords );
            if ( !m_gridLinesEnabled )
            {
                r.x--;
                r.y--;
                r.width++;
                r.height++;
            }

            wxGridCellCoordsArray cells = CalcCellsExposed( r );

            // Otherwise refresh redraws the highlight!
            m_currentCellCoords = coords;

#if defined(__WXMAC__)
            m_gridWin->Refresh(true /*, & r */);
#else
            DrawGridCellArea( dc, cells );
            DrawAllGridLines( dc, r );
#endif
        }
    }

    m_currentCellCoords = coords;

    wxGridCellAttr *attr = GetCellAttr( coords );
#if !defined(__WXMAC__)
    DrawCellHighlight( dc, attr );
#endif
    attr->DecRef();

    return true;
}

void
wxGrid::UpdateBlockBeingSelected(int topRow, int leftCol,
                                 int bottomRow, int rightCol)
{
    MakeCellVisible(m_selectedBlockCorner);
    m_selectedBlockCorner = wxGridCellCoords(bottomRow, rightCol);

    if ( m_selection )
    {
        switch ( m_selection->GetSelectionMode() )
        {
            default:
                wxFAIL_MSG( "unknown selection mode" );
                // fall through

            case wxGridSelectCells:
                // arbitrary blocks selection allowed so just use the cell
                // coordinates as is
                break;

            case wxGridSelectRows:
                // only full rows selection allowd, ensure that we do select
                // full rows
                leftCol = 0;
                rightCol = GetNumberCols() - 1;
                break;

            case wxGridSelectColumns:
                // same as above but for columns
                topRow = 0;
                bottomRow = GetNumberRows() - 1;
                break;

            case wxGridSelectRowsOrColumns:
                // in this mode we can select only full rows or full columns so
                // it doesn't make sense to select blocks at all (and we can't
                // extend the block because there is no preferred direction, we
                // could only extend it to cover the entire grid but this is
                // not useful)
                return;
        }
    }

    EnsureFirstLessThanSecond(topRow, bottomRow);
    EnsureFirstLessThanSecond(leftCol, rightCol);

    wxGridCellCoords updateTopLeft = wxGridCellCoords(topRow, leftCol),
                     updateBottomRight = wxGridCellCoords(bottomRow, rightCol);

    // First the case that we selected a completely new area
    if ( m_selectedBlockTopLeft == wxGridNoCellCoords ||
         m_selectedBlockBottomRight == wxGridNoCellCoords )
    {
        wxRect rect;
        rect = BlockToDeviceRect( wxGridCellCoords ( topRow, leftCol ),
                                  wxGridCellCoords ( bottomRow, rightCol ) );
        m_gridWin->Refresh( false, &rect );
    }

    // Now handle changing an existing selection area.
    else if ( m_selectedBlockTopLeft != updateTopLeft ||
              m_selectedBlockBottomRight != updateBottomRight )
    {
        // Compute two optimal update rectangles:
        // Either one rectangle is a real subset of the
        // other, or they are (almost) disjoint!
        wxRect  rect[4];
        bool    need_refresh[4];
        need_refresh[0] =
        need_refresh[1] =
        need_refresh[2] =
        need_refresh[3] = false;
        int     i;

        // Store intermediate values
        wxCoord oldLeft = m_selectedBlockTopLeft.GetCol();
        wxCoord oldTop = m_selectedBlockTopLeft.GetRow();
        wxCoord oldRight = m_selectedBlockBottomRight.GetCol();
        wxCoord oldBottom = m_selectedBlockBottomRight.GetRow();

        // Determine the outer/inner coordinates.
        EnsureFirstLessThanSecond(oldLeft, leftCol);
        EnsureFirstLessThanSecond(oldTop, topRow);
        EnsureFirstLessThanSecond(rightCol, oldRight);
        EnsureFirstLessThanSecond(bottomRow, oldBottom);

        // Now, either the stuff marked old is the outer
        // rectangle or we don't have a situation where one
        // is contained in the other.

        if ( oldLeft < leftCol )
        {
            // Refresh the newly selected or deselected
            // area to the left of the old or new selection.
            need_refresh[0] = true;
            rect[0] = BlockToDeviceRect(
                wxGridCellCoords( oldTop,  oldLeft ),
                wxGridCellCoords( oldBottom, leftCol - 1 ) );
        }

        if ( oldTop < topRow )
        {
            // Refresh the newly selected or deselected
            // area above the old or new selection.
            need_refresh[1] = true;
            rect[1] = BlockToDeviceRect(
                wxGridCellCoords( oldTop, leftCol ),
                wxGridCellCoords( topRow - 1, rightCol ) );
        }

        if ( oldRight > rightCol )
        {
            // Refresh the newly selected or deselected
            // area to the right of the old or new selection.
            need_refresh[2] = true;
            rect[2] = BlockToDeviceRect(
                wxGridCellCoords( oldTop, rightCol + 1 ),
                wxGridCellCoords( oldBottom, oldRight ) );
        }

        if ( oldBottom > bottomRow )
        {
            // Refresh the newly selected or deselected
            // area below the old or new selection.
            need_refresh[3] = true;
            rect[3] = BlockToDeviceRect(
                wxGridCellCoords( bottomRow + 1, leftCol ),
                wxGridCellCoords( oldBottom, rightCol ) );
        }

        // various Refresh() calls
        for (i = 0; i < 4; i++ )
            if ( need_refresh[i] && rect[i] != wxGridNoCellRect )
                m_gridWin->Refresh( false, &(rect[i]) );
    }

    // change selection
    m_selectedBlockTopLeft = updateTopLeft;
    m_selectedBlockBottomRight = updateBottomRight;
}

//
// ------ functions to get/send data (see also public functions)
//

bool wxGrid::GetModelValues()
{
    // Hide the editor, so it won't hide a changed value.
    HideCellEditControl();

    if ( m_table )
    {
        // all we need to do is repaint the grid
        //
        m_gridWin->Refresh();
        return true;
    }

    return false;
}

bool wxGrid::SetModelValues()
{
    int row, col;

    // Disable the editor, so it won't hide a changed value.
    // Do we also want to save the current value of the editor first?
    // I think so ...
    DisableCellEditControl();

    if ( m_table )
    {
        for ( row = 0; row < m_numRows; row++ )
        {
            for ( col = 0; col < m_numCols; col++ )
            {
                m_table->SetValue( row, col, GetCellValue(row, col) );
            }
        }

        return true;
    }

    return false;
}

// Note - this function only draws cells that are in the list of
// exposed cells (usually set from the update region by
// CalcExposedCells)
//
void wxGrid::DrawGridCellArea( wxDC& dc, const wxGridCellCoordsArray& cells )
{
    if ( !m_numRows || !m_numCols )
        return;

    int i, numCells = cells.GetCount();
    int row, col, cell_rows, cell_cols;
    wxGridCellCoordsArray redrawCells;

    for ( i = numCells - 1; i >= 0; i-- )
    {
        row = cells[i].GetRow();
        col = cells[i].GetCol();
        GetCellSize( row, col, &cell_rows, &cell_cols );

        // If this cell is part of a multicell block, find owner for repaint
        if ( cell_rows <= 0 || cell_cols <= 0 )
        {
            wxGridCellCoords cell( row + cell_rows, col + cell_cols );
            bool marked = false;
            for ( int j = 0; j < numCells; j++ )
            {
                if ( cell == cells[j] )
                {
                    marked = true;
                    break;
                }
            }

            if (!marked)
            {
                int count = redrawCells.GetCount();
                for (int j = 0; j < count; j++)
                {
                    if ( cell == redrawCells[j] )
                    {
                        marked = true;
                        break;
                    }
                }

                if (!marked)
                    redrawCells.Add( cell );
            }

            // don't bother drawing this cell
            continue;
        }

        // If this cell is empty, find cell to left that might want to overflow
        if (m_table && m_table->IsEmptyCell(row, col))
        {
            for ( int l = 0; l < cell_rows; l++ )
            {
                // find a cell in this row to leave already marked for repaint
                int left = col;
                for (int k = 0; k < int(redrawCells.GetCount()); k++)
                    if ((redrawCells[k].GetCol() < left) &&
                        (redrawCells[k].GetRow() == row))
                    {
                        left = redrawCells[k].GetCol();
                    }

                if (left == col)
                    left = 0; // oh well

                for (int j = col - 1; j >= left; j--)
                {
                    if (!m_table->IsEmptyCell(row + l, j))
                    {
                        if (GetCellOverflow(row + l, j))
                        {
                            wxGridCellCoords cell(row + l, j);
                            bool marked = false;

                            for (int k = 0; k < numCells; k++)
                            {
                                if ( cell == cells[k] )
                                {
                                    marked = true;
                                    break;
                                }
                            }

                            if (!marked)
                            {
                                int count = redrawCells.GetCount();
                                for (int k = 0; k < count; k++)
                                {
                                    if ( cell == redrawCells[k] )
                                    {
                                        marked = true;
                                        break;
                                    }
                                }
                                if (!marked)
                                    redrawCells.Add( cell );
                            }
                        }
                        break;
                    }
                }
            }
        }

        DrawCell( dc, cells[i] );
    }

    numCells = redrawCells.GetCount();

    for ( i = numCells - 1; i >= 0; i-- )
    {
        DrawCell( dc, redrawCells[i] );
    }
}

void wxGrid::DrawGridSpace( wxDC& dc )
{
  int cw, ch;
  m_gridWin->GetClientSize( &cw, &ch );

  int right, bottom;
  CalcUnscrolledPosition( cw, ch, &right, &bottom );

  int rightCol = m_numCols > 0 ? GetColRight(GetColAt( m_numCols - 1 )) : 0;
  int bottomRow = m_numRows > 0 ? GetRowBottom(m_numRows - 1) : 0;

  if ( right > rightCol || bottom > bottomRow )
  {
      int left, top;
      CalcUnscrolledPosition( 0, 0, &left, &top );

      dc.SetBrush(GetDefaultCellBackgroundColour());
      dc.SetPen( *wxTRANSPARENT_PEN );

      if ( right > rightCol )
      {
          dc.DrawRectangle( rightCol, top, right - rightCol, ch );
      }

      if ( bottom > bottomRow )
      {
          dc.DrawRectangle( left, bottomRow, cw, bottom - bottomRow );
      }
  }
}

void wxGrid::DrawCell( wxDC& dc, const wxGridCellCoords& coords )
{
    int row = coords.GetRow();
    int col = coords.GetCol();

    if ( GetColWidth(col) <= 0 || GetRowHeight(row) <= 0 )
        return;

    // we draw the cell border ourselves
    wxGridCellAttr* attr = GetCellAttr(row, col);

    bool isCurrent = coords == m_currentCellCoords;

    wxRect rect = CellToRect( row, col );

    // if the editor is shown, we should use it and not the renderer
    // Note: However, only if it is really _shown_, i.e. not hidden!
    if ( isCurrent && IsCellEditControlShown() )
    {
        // NB: this "#if..." is temporary and fixes a problem where the
        // edit control is erased by this code after being rendered.
        // On wxMac (QD build only), the cell editor is a wxTextCntl and is rendered
        // implicitly, causing this out-of order render.
#if !defined(__WXMAC__)
        wxGridCellEditor *editor = attr->GetEditor(this, row, col);
        editor->PaintBackground(dc, rect, *attr);
        editor->DecRef();
#endif
    }
    else
    {
        // but all the rest is drawn by the cell renderer and hence may be customized
        wxGridCellRenderer *renderer = attr->GetRenderer(this, row, col);
        renderer->Draw(*this, *attr, dc, rect, row, col, IsInSelection(coords));
        renderer->DecRef();
    }

    attr->DecRef();
}

void wxGrid::DrawCellHighlight( wxDC& dc, const wxGridCellAttr *attr )
{
    // don't show highlight when the grid doesn't have focus
    if ( !HasFocus() )
        return;

    int row = m_currentCellCoords.GetRow();
    int col = m_currentCellCoords.GetCol();

    if ( GetColWidth(col) <= 0 || GetRowHeight(row) <= 0 )
        return;

    wxRect rect = CellToRect(row, col);

    // hmmm... what could we do here to show that the cell is disabled?
    // for now, I just draw a thinner border than for the other ones, but
    // it doesn't look really good

    int penWidth = attr->IsReadOnly() ? m_cellHighlightROPenWidth : m_cellHighlightPenWidth;

    if (penWidth > 0)
    {
        // The center of the drawn line is where the position/width/height of
        // the rectangle is actually at (on wxMSW at least), so the
        // size of the rectangle is reduced to compensate for the thickness of
        // the line. If this is too strange on non-wxMSW platforms then
        // please #ifdef this appropriately.
        rect.x += penWidth / 2;
        rect.y += penWidth / 2;
        rect.width -= penWidth - 1;
        rect.height -= penWidth - 1;

        // Now draw the rectangle
        // use the cellHighlightColour if the cell is inside a selection, this
        // will ensure the cell is always visible.
        dc.SetPen(wxPen(IsInSelection(row,col) ? m_selectionForeground
                                               : m_cellHighlightColour,
                        penWidth));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(rect);
    }
}

wxPen wxGrid::GetDefaultGridLinePen()
{
    return wxPen(GetGridLineColour());
}

wxPen wxGrid::GetRowGridLinePen(int WXUNUSED(row))
{
    return GetDefaultGridLinePen();
}

wxPen wxGrid::GetColGridLinePen(int WXUNUSED(col))
{
    return GetDefaultGridLinePen();
}

void wxGrid::DrawCellBorder( wxDC& dc, const wxGridCellCoords& coords )
{
    int row = coords.GetRow();
    int col = coords.GetCol();
    if ( GetColWidth(col) <= 0 || GetRowHeight(row) <= 0 )
        return;


    wxRect rect = CellToRect( row, col );

    // right hand border
    dc.SetPen( GetColGridLinePen(col) );
    dc.DrawLine( rect.x + rect.width, rect.y,
                 rect.x + rect.width, rect.y + rect.height + 1 );

    // bottom border
    dc.SetPen( GetRowGridLinePen(row) );
    dc.DrawLine( rect.x, rect.y + rect.height,
                 rect.x + rect.width, rect.y + rect.height);
}

void wxGrid::DrawHighlight(wxDC& dc, const wxGridCellCoordsArray& cells)
{
    // This if block was previously in wxGrid::OnPaint but that doesn't
    // seem to get called under wxGTK - MB
    //
    if ( m_currentCellCoords == wxGridNoCellCoords &&
         m_numRows && m_numCols )
    {
        m_currentCellCoords.Set(0, 0);
    }

    if ( IsCellEditControlShown() )
    {
        // don't show highlight when the edit control is shown
        return;
    }

    // if the active cell was repainted, repaint its highlight too because it
    // might have been damaged by the grid lines
    size_t count = cells.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        wxGridCellCoords cell = cells[n];

        // If we are using attributes, then we may have just exposed another
        // cell in a partially-visible merged cluster of cells. If the "anchor"
        // (upper left) cell of this merged cluster is the cell indicated by
        // m_currentCellCoords, then we need to refresh the cell highlight even
        // though the "anchor" itself is not part of our update segment.
        if ( CanHaveAttributes() )
        {
            int rows = 0,
                cols = 0;
            GetCellSize(cell.GetRow(), cell.GetCol(), &rows, &cols);

            if ( rows < 0 )
                cell.SetRow(cell.GetRow() + rows);

            if ( cols < 0 )
                cell.SetCol(cell.GetCol() + cols);
        }

        if ( cell == m_currentCellCoords )
        {
            wxGridCellAttr* attr = GetCellAttr(m_currentCellCoords);
            DrawCellHighlight(dc, attr);
            attr->DecRef();

            break;
        }
    }
}

// Used by wxGrid::Render() to draw the grid lines only for the cells in the
// specified range.
void
wxGrid::DrawRangeGridLines(wxDC& dc,
                           const wxRegion& reg,
                           const wxGridCellCoords& topLeft,
                           const wxGridCellCoords& bottomRight)
{
    if ( !m_gridLinesEnabled )
         return;

    int top, left, width, height;
    reg.GetBox( left, top, width, height );

    // create a clipping region
    wxRegion clippedcells( dc.LogicalToDeviceX( left ),
                           dc.LogicalToDeviceY( top ),
                           dc.LogicalToDeviceXRel( width ),
                           dc.LogicalToDeviceYRel( height ) );

    // subtract multi cell span area from clipping region for lines
    wxRect rect;
    for ( int row = topLeft.GetRow(); row <= bottomRight.GetRow(); row++ )
    {
        for ( int col = topLeft.GetCol(); col <= bottomRight.GetCol(); col++ )
        {
            int cell_rows, cell_cols;
            GetCellSize( row, col, &cell_rows, &cell_cols );
            if ( cell_rows > 1 || cell_cols > 1 ) // multi cell
            {
                rect = CellToRect( row, col );
                // cater for scaling
                // device origin already set in ::Render() for x, y
                rect.x = dc.LogicalToDeviceX( rect.x );
                rect.y = dc.LogicalToDeviceY( rect.y );
                rect.width = dc.LogicalToDeviceXRel( rect.width );
                rect.height = dc.LogicalToDeviceYRel( rect.height ) - 1;
                clippedcells.Subtract( rect );
            }
            else if ( cell_rows < 0 || cell_cols < 0 ) // part of multicell
            {
                rect = CellToRect( row + cell_rows, col + cell_cols );
                rect.x = dc.LogicalToDeviceX( rect.x );
                rect.y = dc.LogicalToDeviceY( rect.y );
                rect.width = dc.LogicalToDeviceXRel( rect.width );
                rect.height = dc.LogicalToDeviceYRel( rect.height ) - 1;
                clippedcells.Subtract( rect );
            }
        }
    }

    dc.SetDeviceClippingRegion( clippedcells );

    DoDrawGridLines(dc,
                    top, left, top + height, left + width,
                    topLeft.GetRow(), topLeft.GetCol(),
                    bottomRight.GetRow(), bottomRight.GetCol());

    dc.DestroyClippingRegion();
}

// This is used to redraw all grid lines e.g. when the grid line colour
// has been changed
//
void wxGrid::DrawAllGridLines( wxDC& dc, const wxRegion & WXUNUSED(reg) )
{
    if ( !m_gridLinesEnabled )
         return;

    int top, bottom, left, right;

    int cw, ch;
    m_gridWin->GetClientSize(&cw, &ch);
    CalcUnscrolledPosition( 0, 0, &left, &top );
    CalcUnscrolledPosition( cw, ch, &right, &bottom );

    // avoid drawing grid lines past the last row and col
    if ( m_gridLinesClipHorz )
    {
        if ( !m_numCols )
            return;

        const int lastColRight = GetColRight(GetColAt(m_numCols - 1));
        if ( right > lastColRight )
            right = lastColRight;
    }

    if ( m_gridLinesClipVert )
    {
        if ( !m_numRows )
            return;

        const int lastRowBottom = GetRowBottom(m_numRows - 1);
        if ( bottom > lastRowBottom )
            bottom = lastRowBottom;
    }

    // no gridlines inside multicells, clip them out
    int leftCol = GetColPos( internalXToCol(left) );
    int topRow = internalYToRow(top);
    int rightCol = GetColPos( internalXToCol(right) );
    int bottomRow = internalYToRow(bottom);

    wxRegion clippedcells(0, 0, cw, ch);

    int cell_rows, cell_cols;
    wxRect rect;

    for ( int j = topRow; j <= bottomRow; j++ )
    {
        for ( int colPos = leftCol; colPos <= rightCol; colPos++ )
        {
            int i = GetColAt( colPos );

            GetCellSize( j, i, &cell_rows, &cell_cols );
            if ((cell_rows > 1) || (cell_cols > 1))
            {
                rect = CellToRect(j,i);
                CalcScrolledPosition( rect.x, rect.y, &rect.x, &rect.y );
                clippedcells.Subtract(rect);
            }
            else if ((cell_rows < 0) || (cell_cols < 0))
            {
                rect = CellToRect(j + cell_rows, i + cell_cols);
                CalcScrolledPosition( rect.x, rect.y, &rect.x, &rect.y );
                clippedcells.Subtract(rect);
            }
        }
    }

    dc.SetDeviceClippingRegion( clippedcells );

    DoDrawGridLines(dc,
                    top, left, bottom, right,
                    topRow, leftCol, m_numRows, m_numCols);

    dc.DestroyClippingRegion();
}

void
wxGrid::DoDrawGridLines(wxDC& dc,
                        int top, int left,
                        int bottom, int right,
                        int topRow, int leftCol,
                        int bottomRow, int rightCol)
{
    // horizontal grid lines
    for ( int i = topRow; i < bottomRow; i++ )
    {
        int bot = GetRowBottom(i) - 1;

        if ( bot > bottom )
            break;

        if ( bot >= top )
        {
            dc.SetPen( GetRowGridLinePen(i) );
            dc.DrawLine( left, bot, right, bot );
        }
    }

    // vertical grid lines
    for ( int colPos = leftCol; colPos < rightCol; colPos++ )
    {
        int i = GetColAt( colPos );

        int colRight = GetColRight(i);
#ifdef __WXGTK__
        if (GetLayoutDirection() != wxLayout_RightToLeft)
#endif
            colRight--;

        if ( colRight > right )
            break;

        if ( colRight >= left )
        {
            dc.SetPen( GetColGridLinePen(i) );
            dc.DrawLine( colRight, top, colRight, bottom );
        }
    }
}

void wxGrid::DrawRowLabels( wxDC& dc, const wxArrayInt& rows)
{
    if ( !m_numRows )
        return;

    const size_t numLabels = rows.GetCount();
    for ( size_t i = 0; i < numLabels; i++ )
    {
        DrawRowLabel( dc, rows[i] );
    }
}

void wxGrid::DrawRowLabel( wxDC& dc, int row )
{
    if ( GetRowHeight(row) <= 0 || m_rowLabelWidth <= 0 )
        return;

    wxGridCellAttrProvider * const
        attrProvider = m_table ? m_table->GetAttrProvider() : NULL;

    // notice that an explicit static_cast is needed to avoid a compilation
    // error with VC7.1 which, for some reason, tries to instantiate (abstract)
    // wxGridRowHeaderRenderer class without it
    const wxGridRowHeaderRenderer&
        rend = attrProvider ? attrProvider->GetRowHeaderRenderer(row)
                            : static_cast<const wxGridRowHeaderRenderer&>
                                (gs_defaultHeaderRenderers.rowRenderer);

    wxRect rect(0, GetRowTop(row), m_rowLabelWidth, GetRowHeight(row));
    rend.DrawBorder(*this, dc, rect);

    int hAlign, vAlign;
    GetRowLabelAlignment(&hAlign, &vAlign);

    rend.DrawLabel(*this, dc, GetRowLabelValue(row),
                   rect, hAlign, vAlign, wxHORIZONTAL);
}

void wxGrid::UseNativeColHeader(bool native)
{
    if ( native == m_useNativeHeader )
        return;

    delete m_colWindow;
    m_useNativeHeader = native;

    CreateColumnWindow();

    if ( m_useNativeHeader )
        GetGridColHeader()->SetColumnCount(m_numCols);
    CalcWindowSizes();
}

void wxGrid::SetUseNativeColLabels( bool native )
{
    wxASSERT_MSG( !m_useNativeHeader,
                  "doesn't make sense when using native header" );

    m_nativeColumnLabels = native;
    if (native)
    {
        int height = wxRendererNative::Get().GetHeaderButtonHeight( this );
        SetColLabelSize( height );
    }

    GetColLabelWindow()->Refresh();
    m_cornerLabelWin->Refresh();
}

void wxGrid::DrawColLabels( wxDC& dc,const wxArrayInt& cols )
{
    if ( !m_numCols )
        return;

    const size_t numLabels = cols.GetCount();
    for ( size_t i = 0; i < numLabels; i++ )
    {
        DrawColLabel( dc, cols[i] );
    }
}

void wxGrid::DrawCornerLabel(wxDC& dc)
{
    wxRect rect(wxSize(m_rowLabelWidth, m_colLabelHeight));

    if ( m_nativeColumnLabels )
    {
        rect.Deflate(1);

        wxRendererNative::Get().DrawHeaderButton(m_cornerLabelWin, dc, rect, 0);
    }
    else
    {
        rect.width++;
        rect.height++;

        wxGridCellAttrProvider * const
            attrProvider = m_table ? m_table->GetAttrProvider() : NULL;
        const wxGridCornerHeaderRenderer&
            rend = attrProvider ? attrProvider->GetCornerRenderer()
                                : static_cast<wxGridCornerHeaderRenderer&>
                                    (gs_defaultHeaderRenderers.cornerRenderer);

        rend.DrawBorder(*this, dc, rect);
    }
}

void wxGrid::DrawColLabel(wxDC& dc, int col)
{
    if ( GetColWidth(col) <= 0 || m_colLabelHeight <= 0 )
        return;

    int colLeft = GetColLeft(col);

    wxRect rect(colLeft, 0, GetColWidth(col), m_colLabelHeight);
    wxGridCellAttrProvider * const
        attrProvider = m_table ? m_table->GetAttrProvider() : NULL;
    const wxGridColumnHeaderRenderer&
        rend = attrProvider ? attrProvider->GetColumnHeaderRenderer(col)
                            : static_cast<wxGridColumnHeaderRenderer&>
                                (gs_defaultHeaderRenderers.colRenderer);

    if ( m_nativeColumnLabels )
    {
        wxRendererNative::Get().DrawHeaderButton
                                (
                                    GetColLabelWindow(),
                                    dc,
                                    rect,
                                    0,
                                    IsSortingBy(col)
                                        ? IsSortOrderAscending()
                                            ? wxHDR_SORT_ICON_UP
                                            : wxHDR_SORT_ICON_DOWN
                                        : wxHDR_SORT_ICON_NONE
                                );
        rect.Deflate(2);
    }
    else
    {
        // It is reported that we need to erase the background to avoid display
        // artefacts, see #12055.
        wxDCBrushChanger setBrush(dc, m_colWindow->GetBackgroundColour());
        dc.DrawRectangle(rect);

        rend.DrawBorder(*this, dc, rect);
    }

    int hAlign, vAlign;
    GetColLabelAlignment(&hAlign, &vAlign);
    const int orient = GetColLabelTextOrientation();

    rend.DrawLabel(*this, dc, GetColLabelValue(col), rect, hAlign, vAlign, orient);
}

// TODO: these 2 functions should be replaced with wxDC::DrawLabel() to which
//       we just have to add textOrientation support
void wxGrid::DrawTextRectangle( wxDC& dc,
                                const wxString& value,
                                const wxRect& rect,
                                int horizAlign,
                                int vertAlign,
                                int textOrientation ) const
{
    wxArrayString lines;

    StringToLines( value, lines );

    DrawTextRectangle(dc, lines, rect, horizAlign, vertAlign, textOrientation);
}

void wxGrid::DrawTextRectangle(wxDC& dc,
                               const wxArrayString& lines,
                               const wxRect& rect,
                               int horizAlign,
                               int vertAlign,
                               int textOrientation) const
{
    if ( lines.empty() )
        return;

    wxDCClipper clip(dc, rect);

    long textWidth,
         textHeight;

    if ( textOrientation == wxHORIZONTAL )
        GetTextBoxSize( dc, lines, &textWidth, &textHeight );
    else
        GetTextBoxSize( dc, lines, &textHeight, &textWidth );

    int x = 0,
        y = 0;
    switch ( vertAlign )
    {
        case wxALIGN_BOTTOM:
            if ( textOrientation == wxHORIZONTAL )
                y = rect.y + (rect.height - textHeight - 1);
            else
                x = rect.x + rect.width - textWidth;
            break;

        case wxALIGN_CENTRE:
            if ( textOrientation == wxHORIZONTAL )
                y = rect.y + ((rect.height - textHeight) / 2);
            else
                x = rect.x + ((rect.width - textWidth) / 2);
            break;

        case wxALIGN_TOP:
        default:
            if ( textOrientation == wxHORIZONTAL )
                y = rect.y + 1;
            else
                x = rect.x + 1;
            break;
    }

    // Align each line of a multi-line label
    size_t nLines = lines.GetCount();
    for ( size_t l = 0; l < nLines; l++ )
    {
        const wxString& line = lines[l];

        if ( line.empty() )
        {
            *(textOrientation == wxHORIZONTAL ? &y : &x) += dc.GetCharHeight();
            continue;
        }

        wxCoord lineWidth = 0,
             lineHeight = 0;
        dc.GetTextExtent(line, &lineWidth, &lineHeight);

        switch ( horizAlign )
        {
            case wxALIGN_RIGHT:
                if ( textOrientation == wxHORIZONTAL )
                    x = rect.x + (rect.width - lineWidth - 1);
                else
                    y = rect.y + lineWidth + 1;
                break;

            case wxALIGN_CENTRE:
                if ( textOrientation == wxHORIZONTAL )
                    x = rect.x + ((rect.width - lineWidth) / 2);
                else
                    y = rect.y + rect.height - ((rect.height - lineWidth) / 2);
                break;

            case wxALIGN_LEFT:
            default:
                if ( textOrientation == wxHORIZONTAL )
                    x = rect.x + 1;
                else
                    y = rect.y + rect.height - 1;
                break;
        }

        if ( textOrientation == wxHORIZONTAL )
        {
            dc.DrawText( line, x, y );
            y += lineHeight;
        }
        else
        {
            dc.DrawRotatedText( line, x, y, 90.0 );
            x += lineHeight;
        }
    }
}

// Split multi-line text up into an array of strings.
// Any existing contents of the string array are preserved.
//
// TODO: refactor wxTextFile::Read() and reuse the same code from here
void wxGrid::StringToLines( const wxString& value, wxArrayString& lines ) const
{
    int startPos = 0;
    int pos;
    wxString eol = wxTextFile::GetEOL( wxTextFileType_Unix );
    wxString tVal = wxTextFile::Translate( value, wxTextFileType_Unix );

    while ( startPos < (int)tVal.length() )
    {
        pos = tVal.Mid(startPos).Find( eol );
        if ( pos < 0 )
        {
            break;
        }
        else if ( pos == 0 )
        {
            lines.Add( wxEmptyString );
        }
        else
        {
            lines.Add( tVal.Mid(startPos, pos) );
        }

        startPos += pos + 1;
    }

    if ( startPos < (int)tVal.length() )
    {
        lines.Add( tVal.Mid( startPos ) );
    }
}

void wxGrid::GetTextBoxSize( const wxDC& dc,
                             const wxArrayString& lines,
                             long *width, long *height ) const
{
    wxCoord w = 0;
    wxCoord h = 0;
    wxCoord lineW = 0, lineH = 0;

    size_t i;
    for ( i = 0; i < lines.GetCount(); i++ )
    {
        dc.GetTextExtent( lines[i], &lineW, &lineH );
        w = wxMax( w, lineW );
        h += lineH;
    }

    *width = w;
    *height = h;
}

//
// ------ Batch processing.
//
void wxGrid::EndBatch()
{
    if ( m_batchCount > 0 )
    {
        m_batchCount--;
        if ( !m_batchCount )
        {
            CalcDimensions();
            m_rowLabelWin->Refresh();
            m_colWindow->Refresh();
            m_cornerLabelWin->Refresh();
            m_gridWin->Refresh();
        }
    }
}

// Use this, rather than wxWindow::Refresh(), to force an immediate
// repainting of the grid. Has no effect if you are already inside a
// BeginBatch / EndBatch block.
//
void wxGrid::ForceRefresh()
{
    BeginBatch();
    EndBatch();
}

bool wxGrid::Enable(bool enable)
{
    if ( !wxScrolledWindow::Enable(enable) )
        return false;

    // redraw in the new state
    m_gridWin->Refresh();

    return true;
}

//
// ------ Edit control functions
//

void wxGrid::EnableEditing( bool edit )
{
    if ( edit != m_editable )
    {
        if (!edit)
            EnableCellEditControl(edit);
        m_editable = edit;
    }
}

void wxGrid::EnableCellEditControl( bool enable )
{
    if (! m_editable)
        return;

    if ( enable != m_cellEditCtrlEnabled )
    {
        if ( enable )
        {
            if ( SendEvent(wxEVT_GRID_EDITOR_SHOWN) == -1 )
                return;

            // this should be checked by the caller!
            wxASSERT_MSG( CanEnableCellControl(), wxT("can't enable editing for this cell!") );

            // do it before ShowCellEditControl()
            m_cellEditCtrlEnabled = enable;

            ShowCellEditControl();
        }
        else
        {
            SendEvent(wxEVT_GRID_EDITOR_HIDDEN);

            HideCellEditControl();
            SaveEditControlValue();

            // do it after HideCellEditControl()
            m_cellEditCtrlEnabled = enable;
        }
    }
}

bool wxGrid::IsCurrentCellReadOnly() const
{
    wxGridCellAttr*
        attr = const_cast<wxGrid *>(this)->GetCellAttr(m_currentCellCoords);
    bool readonly = attr->IsReadOnly();
    attr->DecRef();

    return readonly;
}

bool wxGrid::CanEnableCellControl() const
{
    return m_editable && (m_currentCellCoords != wxGridNoCellCoords) &&
        !IsCurrentCellReadOnly();
}

bool wxGrid::IsCellEditControlEnabled() const
{
    // the cell edit control might be disable for all cells or just for the
    // current one if it's read only
    return m_cellEditCtrlEnabled ? !IsCurrentCellReadOnly() : false;
}

bool wxGrid::IsCellEditControlShown() const
{
    bool isShown = false;

    if ( m_cellEditCtrlEnabled )
    {
        int row = m_currentCellCoords.GetRow();
        int col = m_currentCellCoords.GetCol();
        wxGridCellAttr* attr = GetCellAttr(row, col);
        wxGridCellEditor* editor = attr->GetEditor((wxGrid*) this, row, col);
        attr->DecRef();

        if ( editor )
        {
            if ( editor->IsCreated() )
            {
                isShown = editor->GetControl()->IsShown();
            }

            editor->DecRef();
        }
    }

    return isShown;
}

void wxGrid::ShowCellEditControl()
{
    if ( IsCellEditControlEnabled() )
    {
        if ( !IsVisible( m_currentCellCoords, false ) )
        {
            m_cellEditCtrlEnabled = false;
            return;
        }
        else
        {
            wxRect rect = CellToRect( m_currentCellCoords );
            int row = m_currentCellCoords.GetRow();
            int col = m_currentCellCoords.GetCol();

            // if this is part of a multicell, find owner (topleft)
            int cell_rows, cell_cols;
            GetCellSize( row, col, &cell_rows, &cell_cols );
            if ( cell_rows <= 0 || cell_cols <= 0 )
            {
                row += cell_rows;
                col += cell_cols;
                m_currentCellCoords.SetRow( row );
                m_currentCellCoords.SetCol( col );
            }

            // erase the highlight and the cell contents because the editor
            // might not cover the entire cell
            wxClientDC dc( m_gridWin );
            PrepareDC( dc );
            wxGridCellAttr* attr = GetCellAttr(row, col);
            dc.SetBrush(wxBrush(attr->GetBackgroundColour()));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(rect);

            // convert to scrolled coords
            CalcScrolledPosition( rect.x, rect.y, &rect.x, &rect.y );

            int nXMove = 0;
            if (rect.x < 0)
                nXMove = rect.x;

            // cell is shifted by one pixel
            // However, don't allow x or y to become negative
            // since the SetSize() method interprets that as
            // "don't change."
            if (rect.x > 0)
                rect.x--;
            if (rect.y > 0)
                rect.y--;

            wxGridCellEditor* editor = attr->GetEditor(this, row, col);
            if ( !editor->IsCreated() )
            {
                editor->Create(m_gridWin, wxID_ANY,
                               new wxGridCellEditorEvtHandler(this, editor));

                wxGridEditorCreatedEvent evt(GetId(),
                                             wxEVT_GRID_EDITOR_CREATED,
                                             this,
                                             row,
                                             col,
                                             editor->GetControl());
                GetEventHandler()->ProcessEvent(evt);
            }

            // resize editor to overflow into righthand cells if allowed
            int maxWidth = rect.width;
            wxString value = GetCellValue(row, col);
            if ( (value != wxEmptyString) && (attr->GetOverflow()) )
            {
                int y;
                GetTextExtent(value, &maxWidth, &y, NULL, NULL, &attr->GetFont());
                if (maxWidth < rect.width)
                    maxWidth = rect.width;
            }

            int client_right = m_gridWin->GetClientSize().GetWidth();
            if (rect.x + maxWidth > client_right)
                maxWidth = client_right - rect.x;

            if ((maxWidth > rect.width) && (col < m_numCols) && m_table)
            {
                GetCellSize( row, col, &cell_rows, &cell_cols );
                // may have changed earlier
                for (int i = col + cell_cols; i < m_numCols; i++)
                {
                    int c_rows, c_cols;
                    GetCellSize( row, i, &c_rows, &c_cols );

                    // looks weird going over a multicell
                    if (m_table->IsEmptyCell( row, i ) &&
                            (rect.width < maxWidth) && (c_rows == 1))
                    {
                        rect.width += GetColWidth( i );
                    }
                    else
                        break;
                }

                if (rect.GetRight() > client_right)
                    rect.SetRight( client_right - 1 );
            }

            editor->SetCellAttr( attr );
            editor->SetSize( rect );
            if (nXMove != 0)
                editor->GetControl()->Move(
                    editor->GetControl()->GetPosition().x + nXMove,
                    editor->GetControl()->GetPosition().y );
            editor->Show( true, attr );

            // recalc dimensions in case we need to
            // expand the scrolled window to account for editor
            CalcDimensions();

            editor->BeginEdit(row, col, this);
            editor->SetCellAttr(NULL);

            editor->DecRef();
            attr->DecRef();
        }
    }
}

void wxGrid::HideCellEditControl()
{
    if ( IsCellEditControlEnabled() )
    {
        int row = m_currentCellCoords.GetRow();
        int col = m_currentCellCoords.GetCol();

        wxGridCellAttr *attr = GetCellAttr(row, col);
        wxGridCellEditor *editor = attr->GetEditor(this, row, col);
        const bool editorHadFocus = editor->GetControl()->HasFocus();
        editor->Show( false );
        editor->DecRef();
        attr->DecRef();

        // return the focus to the grid itself if the editor had it
        //
        // note that we must not do this unconditionally to avoid stealing
        // focus from the window which just received it if we are hiding the
        // editor precisely because we lost focus
        if ( editorHadFocus )
            m_gridWin->SetFocus();

        // refresh whole row to the right
        wxRect rect( CellToRect(row, col) );
        CalcScrolledPosition(rect.x, rect.y, &rect.x, &rect.y );
        rect.width = m_gridWin->GetClientSize().GetWidth() - rect.x;

#ifdef __WXMAC__
        // ensure that the pixels under the focus ring get refreshed as well
        rect.Inflate(10, 10);
#endif

        m_gridWin->Refresh( false, &rect );
    }
}

void wxGrid::SaveEditControlValue()
{
    if ( IsCellEditControlEnabled() )
    {
        int row = m_currentCellCoords.GetRow();
        int col = m_currentCellCoords.GetCol();

        wxString oldval = GetCellValue(row, col);

        wxGridCellAttr* attr = GetCellAttr(row, col);
        wxGridCellEditor* editor = attr->GetEditor(this, row, col);

        wxString newval;
        bool changed = editor->EndEdit(row, col, this, oldval, &newval);

        if ( changed && SendEvent(wxEVT_GRID_CELL_CHANGING, newval) != -1 )
        {
            editor->ApplyEdit(row, col, this);

            // for compatibility reasons dating back to wx 2.8 when this event
            // was called wxEVT_GRID_CELL_CHANGE and wxEVT_GRID_CELL_CHANGING
            // didn't exist we allow vetoing this one too
            if ( SendEvent(wxEVT_GRID_CELL_CHANGED, oldval) == -1 )
            {
                // Event has been vetoed, set the data back.
                SetCellValue(row, col, oldval);
            }
        }

        editor->DecRef();
        attr->DecRef();
    }
}

void wxGrid::OnHideEditor(wxCommandEvent& WXUNUSED(event))
{
    DisableCellEditControl();
}

//
// ------ Grid location functions
//  Note that all of these functions work with the logical coordinates of
//  grid cells and labels so you will need to convert from device
//  coordinates for mouse events etc.
//

wxGridCellCoords wxGrid::XYToCell(int x, int y) const
{
    int row = YToRow(y);
    int col = XToCol(x);

    return row == -1 || col == -1 ? wxGridNoCellCoords
                                  : wxGridCellCoords(row, col);
}

// compute row or column from some (unscrolled) coordinate value, using either
// m_defaultRowHeight/m_defaultColWidth or binary search on array of
// m_rowBottoms/m_colRights to do it quickly in O(log n) time.
// NOTE: This may not work correctly for reordered columns.
int wxGrid::PosToLinePos(int coord,
                         bool clipToMinMax,
                         const wxGridOperations& oper) const
{
    const int numLines = oper.GetNumberOfLines(this);

    if ( coord < 0 )
        return clipToMinMax && numLines > 0 ? 0 : wxNOT_FOUND;

    const int defaultLineSize = oper.GetDefaultLineSize(this);
    wxCHECK_MSG( defaultLineSize, -1, "can't have 0 default line size" );

    int maxPos = coord / defaultLineSize,
        minPos = 0;

    // check for the simplest case: if we have no explicit line sizes
    // configured, then we already know the line this position falls in
    const wxArrayInt& lineEnds = oper.GetLineEnds(this);
    if ( lineEnds.empty() )
    {
        if ( maxPos < numLines )
            return maxPos;

        return clipToMinMax ? numLines - 1 : -1;
    }


    // binary search is quite efficient and we can't really make any assumptions
    // on where to start here since row and columns could be of size 0 if they are
    // hidden. While this could be made more efficient, some profiling will be
    // necessary to determine if it really is a performance bottleneck
    maxPos = numLines  - 1;

    // check if the position is beyond the last column
    const int lineAtMaxPos = oper.GetLineAt(this, maxPos);
    if ( coord >= lineEnds[lineAtMaxPos] )
        return clipToMinMax ? maxPos : -1;

    // or before the first one
    const int lineAt0 = oper.GetLineAt(this, 0);
    if ( coord < lineEnds[lineAt0] )
        return 0;


    // finally do perform the binary search
    while ( minPos < maxPos )
    {
        wxCHECK_MSG( lineEnds[oper.GetLineAt(this, minPos)] <= coord &&
                        coord < lineEnds[oper.GetLineAt(this, maxPos)],
                     -1,
                     "wxGrid: internal error in PosToLinePos()" );

        if ( coord >= lineEnds[oper.GetLineAt(this, maxPos - 1)] )
            return maxPos;
        else
            maxPos--;

        const int median = minPos + (maxPos - minPos + 1) / 2;
        if ( coord < lineEnds[oper.GetLineAt(this, median)] )
            maxPos = median;
        else
            minPos = median;
    }

    return maxPos;
}

int
wxGrid::PosToLine(int coord,
                  bool clipToMinMax,
                  const wxGridOperations& oper) const
{
    int pos = PosToLinePos(coord, clipToMinMax, oper);

    return pos == wxNOT_FOUND ? wxNOT_FOUND : oper.GetLineAt(this, pos);
}

int wxGrid::YToRow(int y, bool clipToMinMax) const
{
    return PosToLine(y, clipToMinMax, wxGridRowOperations());
}

int wxGrid::XToCol(int x, bool clipToMinMax) const
{
    return PosToLine(x, clipToMinMax, wxGridColumnOperations());
}

int wxGrid::XToPos(int x) const
{
    return PosToLinePos(x, true /* clip */, wxGridColumnOperations());
}

// return the row/col number such that the pos is near the edge of, or -1 if
// not near an edge.
//
// notice that position can only possibly be near an edge if the row/column is
// large enough to still allow for an "inner" area that is _not_ near the edge
// (i.e., if the height/width is smaller than WXGRID_LABEL_EDGE_ZONE, pos will
// _never_ be considered to be near the edge).
int wxGrid::PosToEdgeOfLine(int pos, const wxGridOperations& oper) const
{
    // Get the bottom or rightmost line that could match.
    int line = oper.PosToLine(this, pos, true);

    if ( oper.GetLineSize(this, line) > WXGRID_LABEL_EDGE_ZONE )
    {
        // We know that we are in this line, test whether we are close enough
        // to start or end border, respectively.
        if ( abs(oper.GetLineEndPos(this, line) - pos) < WXGRID_LABEL_EDGE_ZONE )
            return line;
        else if ( line > 0 &&
                    pos - oper.GetLineStartPos(this,
                                               line) < WXGRID_LABEL_EDGE_ZONE )
        {
            // We need to find the previous visible line, so skip all the
            // hidden (of size 0) ones.
            do
            {
                line = oper.GetLineBefore(this, line);
            }
            while ( line >= 0 && oper.GetLineSize(this, line) == 0 );

            // It can possibly be -1 here.
            return line;
        }
    }

    return -1;
}

int wxGrid::YToEdgeOfRow(int y) const
{
    return PosToEdgeOfLine(y, wxGridRowOperations());
}

int wxGrid::XToEdgeOfCol(int x) const
{
    return PosToEdgeOfLine(x, wxGridColumnOperations());
}

wxRect wxGrid::CellToRect( int row, int col ) const
{
    wxRect rect( -1, -1, -1, -1 );

    if ( row >= 0 && row < m_numRows &&
         col >= 0 && col < m_numCols )
    {
        int i, cell_rows, cell_cols;
        rect.width = rect.height = 0;
        GetCellSize( row, col, &cell_rows, &cell_cols );
        // if negative then find multicell owner
        if (cell_rows < 0)
            row += cell_rows;
        if (cell_cols < 0)
             col += cell_cols;
        GetCellSize( row, col, &cell_rows, &cell_cols );

        rect.x = GetColLeft(col);
        rect.y = GetRowTop(row);
        for (i=col; i < col + cell_cols; i++)
            rect.width += GetColWidth(i);
        for (i=row; i < row + cell_rows; i++)
            rect.height += GetRowHeight(i);

        // if grid lines are enabled, then the area of the cell is a bit smaller
        if (m_gridLinesEnabled)
        {
            rect.width -= 1;
            rect.height -= 1;
        }
    }

    return rect;
}

bool wxGrid::IsVisible( int row, int col, bool wholeCellVisible ) const
{
    // get the cell rectangle in logical coords
    //
    wxRect r( CellToRect( row, col ) );

    // convert to device coords
    //
    int left, top, right, bottom;
    CalcScrolledPosition( r.GetLeft(), r.GetTop(), &left, &top );
    CalcScrolledPosition( r.GetRight(), r.GetBottom(), &right, &bottom );

    // check against the client area of the grid window
    int cw, ch;
    m_gridWin->GetClientSize( &cw, &ch );

    if ( wholeCellVisible )
    {
        // is the cell wholly visible ?
        return ( left >= 0 && right <= cw &&
                 top >= 0 && bottom <= ch );
    }
    else
    {
        // is the cell partly visible ?
        //
        return ( ((left >= 0 && left < cw) || (right > 0 && right <= cw)) &&
                 ((top >= 0 && top < ch) || (bottom > 0 && bottom <= ch)) );
    }
}

// make the specified cell location visible by doing a minimal amount
// of scrolling
//
void wxGrid::MakeCellVisible( int row, int col )
{
    int i;
    int xpos = -1, ypos = -1;

    if ( row >= 0 && row < m_numRows &&
         col >= 0 && col < m_numCols )
    {
        // get the cell rectangle in logical coords
        wxRect r( CellToRect( row, col ) );

        // convert to device coords
        int left, top, right, bottom;
        CalcScrolledPosition( r.GetLeft(), r.GetTop(), &left, &top );
        CalcScrolledPosition( r.GetRight(), r.GetBottom(), &right, &bottom );

        int cw, ch;
        m_gridWin->GetClientSize( &cw, &ch );

        if ( top < 0 )
        {
            ypos = r.GetTop();
        }
        else if ( bottom > ch )
        {
            int h = r.GetHeight();
            ypos = r.GetTop();
            for ( i = row - 1; i >= 0; i-- )
            {
                int rowHeight = GetRowHeight(i);
                if ( h + rowHeight > ch )
                    break;

                h += rowHeight;
                ypos -= rowHeight;
            }

            // we divide it later by GRID_SCROLL_LINE, make sure that we don't
            // have rounding errors (this is important, because if we do,
            // we might not scroll at all and some cells won't be redrawn)
            //
            // Sometimes GRID_SCROLL_LINE / 2 is not enough,
            // so just add a full scroll unit...
            ypos += m_yScrollPixelsPerLine;
        }

        // special handling for wide cells - show always left part of the cell!
        // Otherwise, e.g. when stepping from row to row, it would jump between
        // left and right part of the cell on every step!
//      if ( left < 0 )
        if ( left < 0 || (right - left) >= cw )
        {
            xpos = r.GetLeft();
        }
        else if ( right > cw )
        {
            // position the view so that the cell is on the right
            int x0, y0;
            CalcUnscrolledPosition(0, 0, &x0, &y0);
            xpos = x0 + (right - cw);

            // see comment for ypos above
            xpos += m_xScrollPixelsPerLine;
        }

        if ( xpos != -1 || ypos != -1 )
        {
            if ( xpos != -1 )
                xpos /= m_xScrollPixelsPerLine;
            if ( ypos != -1 )
                ypos /= m_yScrollPixelsPerLine;
            Scroll( xpos, ypos );
            AdjustScrollbars();
        }
    }
}

//
// ------ Grid cursor movement functions
//

bool
wxGrid::DoMoveCursor(bool expandSelection,
                     const wxGridDirectionOperations& diroper)
{
    if ( m_currentCellCoords == wxGridNoCellCoords )
        return false;

    if ( expandSelection )
    {
        wxGridCellCoords coords = m_selectedBlockCorner;
        if ( coords == wxGridNoCellCoords )
            coords = m_currentCellCoords;

        if ( diroper.IsAtBoundary(coords) )
            return false;

        diroper.Advance(coords);

        UpdateBlockBeingSelected(m_currentCellCoords, coords);
    }
    else // don't expand selection
    {
        ClearSelection();

        if ( diroper.IsAtBoundary(m_currentCellCoords) )
            return false;

        wxGridCellCoords coords = m_currentCellCoords;
        diroper.Advance(coords);

        GoToCell(coords);
    }

    return true;
}

bool wxGrid::MoveCursorUp(bool expandSelection)
{
    return DoMoveCursor(expandSelection,
                        wxGridBackwardOperations(this, wxGridRowOperations()));
}

bool wxGrid::MoveCursorDown(bool expandSelection)
{
    return DoMoveCursor(expandSelection,
                        wxGridForwardOperations(this, wxGridRowOperations()));
}

bool wxGrid::MoveCursorLeft(bool expandSelection)
{
    return DoMoveCursor(expandSelection,
                        wxGridBackwardOperations(this, wxGridColumnOperations()));
}

bool wxGrid::MoveCursorRight(bool expandSelection)
{
    return DoMoveCursor(expandSelection,
                        wxGridForwardOperations(this, wxGridColumnOperations()));
}

bool wxGrid::DoMoveCursorByPage(const wxGridDirectionOperations& diroper)
{
    if ( m_currentCellCoords == wxGridNoCellCoords )
        return false;

    if ( diroper.IsAtBoundary(m_currentCellCoords) )
        return false;

    const int oldRow = m_currentCellCoords.GetRow();
    int newRow = diroper.MoveByPixelDistance(oldRow, m_gridWin->GetClientSize().y);
    if ( newRow == oldRow )
    {
        wxGridCellCoords coords(m_currentCellCoords);
        diroper.Advance(coords);
        newRow = coords.GetRow();
    }

    GoToCell(newRow, m_currentCellCoords.GetCol());

    return true;
}

bool wxGrid::MovePageUp()
{
    return DoMoveCursorByPage(
                wxGridBackwardOperations(this, wxGridRowOperations()));
}

bool wxGrid::MovePageDown()
{
    return DoMoveCursorByPage(
                wxGridForwardOperations(this, wxGridRowOperations()));
}

// helper of DoMoveCursorByBlock(): advance the cell coordinates using diroper
// until we find a non-empty cell or reach the grid end
void
wxGrid::AdvanceToNextNonEmpty(wxGridCellCoords& coords,
                              const wxGridDirectionOperations& diroper)
{
    while ( !diroper.IsAtBoundary(coords) )
    {
        diroper.Advance(coords);
        if ( !m_table->IsEmpty(coords) )
            break;
    }
}

bool
wxGrid::DoMoveCursorByBlock(bool expandSelection,
                            const wxGridDirectionOperations& diroper)
{
    if ( !m_table || m_currentCellCoords == wxGridNoCellCoords )
        return false;

    if ( diroper.IsAtBoundary(m_currentCellCoords) )
        return false;

    wxGridCellCoords coords(m_currentCellCoords);
    if ( m_table->IsEmpty(coords) )
    {
        // we are in an empty cell: find the next block of non-empty cells
        AdvanceToNextNonEmpty(coords, diroper);
    }
    else // current cell is not empty
    {
        diroper.Advance(coords);
        if ( m_table->IsEmpty(coords) )
        {
            // we started at the end of a block, find the next one
            AdvanceToNextNonEmpty(coords, diroper);
        }
        else // we're in a middle of a block
        {
            // go to the end of it, i.e. find the last cell before the next
            // empty one
            while ( !diroper.IsAtBoundary(coords) )
            {
                wxGridCellCoords coordsNext(coords);
                diroper.Advance(coordsNext);
                if ( m_table->IsEmpty(coordsNext) )
                    break;

                coords = coordsNext;
            }
        }
    }

    if ( expandSelection )
    {
        UpdateBlockBeingSelected(m_currentCellCoords, coords);
    }
    else
    {
        ClearSelection();
        GoToCell(coords);
    }

    return true;
}

bool wxGrid::MoveCursorUpBlock(bool expandSelection)
{
    return DoMoveCursorByBlock(
                expandSelection,
                wxGridBackwardOperations(this, wxGridRowOperations())
           );
}

bool wxGrid::MoveCursorDownBlock( bool expandSelection )
{
    return DoMoveCursorByBlock(
                expandSelection,
                wxGridForwardOperations(this, wxGridRowOperations())
           );
}

bool wxGrid::MoveCursorLeftBlock( bool expandSelection )
{
    return DoMoveCursorByBlock(
                expandSelection,
                wxGridBackwardOperations(this, wxGridColumnOperations())
           );
}

bool wxGrid::MoveCursorRightBlock( bool expandSelection )
{
    return DoMoveCursorByBlock(
                expandSelection,
                wxGridForwardOperations(this, wxGridColumnOperations())
           );
}

//
// ------ Label values and formatting
//

void wxGrid::GetRowLabelAlignment( int *horiz, int *vert ) const
{
    if ( horiz )
        *horiz = m_rowLabelHorizAlign;
    if ( vert )
        *vert  = m_rowLabelVertAlign;
}

void wxGrid::GetColLabelAlignment( int *horiz, int *vert ) const
{
    if ( horiz )
        *horiz = m_colLabelHorizAlign;
    if ( vert )
        *vert  = m_colLabelVertAlign;
}

int wxGrid::GetColLabelTextOrientation() const
{
    return m_colLabelTextOrientation;
}

wxString wxGrid::GetRowLabelValue( int row ) const
{
    if ( m_table )
    {
        return m_table->GetRowLabelValue( row );
    }
    else
    {
        wxString s;
        s << row;
        return s;
    }
}

wxString wxGrid::GetColLabelValue( int col ) const
{
    if ( m_table )
    {
        return m_table->GetColLabelValue( col );
    }
    else
    {
        wxString s;
        s << col;
        return s;
    }
}

void wxGrid::SetRowLabelSize( int width )
{
    wxASSERT( width >= 0 || width == wxGRID_AUTOSIZE );

    if ( width == wxGRID_AUTOSIZE )
    {
        width = CalcColOrRowLabelAreaMinSize(wxGRID_ROW);
    }

    if ( width != m_rowLabelWidth )
    {
        if ( width == 0 )
        {
            m_rowLabelWin->Show( false );
            m_cornerLabelWin->Show( false );
        }
        else if ( m_rowLabelWidth == 0 )
        {
            m_rowLabelWin->Show( true );
            if ( m_colLabelHeight > 0 )
                m_cornerLabelWin->Show( true );
        }

        m_rowLabelWidth = width;
        InvalidateBestSize();
        CalcWindowSizes();
        wxScrolledWindow::Refresh( true );
    }
}

void wxGrid::SetColLabelSize( int height )
{
    wxASSERT( height >=0 || height == wxGRID_AUTOSIZE );

    if ( height == wxGRID_AUTOSIZE )
    {
        height = CalcColOrRowLabelAreaMinSize(wxGRID_COLUMN);
    }

    if ( height != m_colLabelHeight )
    {
        if ( height == 0 )
        {
            m_colWindow->Show( false );
            m_cornerLabelWin->Show( false );
        }
        else if ( m_colLabelHeight == 0 )
        {
            m_colWindow->Show( true );
            if ( m_rowLabelWidth > 0 )
                m_cornerLabelWin->Show( true );
        }

        m_colLabelHeight = height;
        InvalidateBestSize();
        CalcWindowSizes();
        wxScrolledWindow::Refresh( true );
    }
}

void wxGrid::SetLabelBackgroundColour( const wxColour& colour )
{
    if ( m_labelBackgroundColour != colour )
    {
        m_labelBackgroundColour = colour;
        m_rowLabelWin->SetBackgroundColour( colour );
        m_colWindow->SetBackgroundColour( colour );
        m_cornerLabelWin->SetBackgroundColour( colour );

        if ( !GetBatchCount() )
        {
            m_rowLabelWin->Refresh();
            m_colWindow->Refresh();
            m_cornerLabelWin->Refresh();
        }
    }
}

void wxGrid::SetLabelTextColour( const wxColour& colour )
{
    if ( m_labelTextColour != colour )
    {
        m_labelTextColour = colour;
        if ( !GetBatchCount() )
        {
            m_rowLabelWin->Refresh();
            m_colWindow->Refresh();
        }
    }
}

void wxGrid::SetLabelFont( const wxFont& font )
{
    m_labelFont = font;
    if ( !GetBatchCount() )
    {
        m_rowLabelWin->Refresh();
        m_colWindow->Refresh();
    }
}

void wxGrid::SetRowLabelAlignment( int horiz, int vert )
{
    // allow old (incorrect) defs to be used
    switch ( horiz )
    {
        case wxLEFT:   horiz = wxALIGN_LEFT; break;
        case wxRIGHT:  horiz = wxALIGN_RIGHT; break;
        case wxCENTRE: horiz = wxALIGN_CENTRE; break;
    }

    switch ( vert )
    {
        case wxTOP:    vert = wxALIGN_TOP;    break;
        case wxBOTTOM: vert = wxALIGN_BOTTOM; break;
        case wxCENTRE: vert = wxALIGN_CENTRE; break;
    }

    if ( horiz == wxALIGN_LEFT || horiz == wxALIGN_CENTRE || horiz == wxALIGN_RIGHT )
    {
        m_rowLabelHorizAlign = horiz;
    }

    if ( vert == wxALIGN_TOP || vert == wxALIGN_CENTRE || vert == wxALIGN_BOTTOM )
    {
        m_rowLabelVertAlign = vert;
    }

    if ( !GetBatchCount() )
    {
        m_rowLabelWin->Refresh();
    }
}

void wxGrid::SetColLabelAlignment( int horiz, int vert )
{
    // allow old (incorrect) defs to be used
    switch ( horiz )
    {
        case wxLEFT:   horiz = wxALIGN_LEFT; break;
        case wxRIGHT:  horiz = wxALIGN_RIGHT; break;
        case wxCENTRE: horiz = wxALIGN_CENTRE; break;
    }

    switch ( vert )
    {
        case wxTOP:    vert = wxALIGN_TOP;    break;
        case wxBOTTOM: vert = wxALIGN_BOTTOM; break;
        case wxCENTRE: vert = wxALIGN_CENTRE; break;
    }

    if ( horiz == wxALIGN_LEFT || horiz == wxALIGN_CENTRE || horiz == wxALIGN_RIGHT )
    {
        m_colLabelHorizAlign = horiz;
    }

    if ( vert == wxALIGN_TOP || vert == wxALIGN_CENTRE || vert == wxALIGN_BOTTOM )
    {
        m_colLabelVertAlign = vert;
    }

    if ( !GetBatchCount() )
    {
        m_colWindow->Refresh();
    }
}

// Note: under MSW, the default column label font must be changed because it
//       does not support vertical printing
//
// Example: wxFont font(9, wxSWISS, wxNORMAL, wxBOLD);
//                      pGrid->SetLabelFont(font);
//                      pGrid->SetColLabelTextOrientation(wxVERTICAL);
//
void wxGrid::SetColLabelTextOrientation( int textOrientation )
{
    if ( textOrientation == wxHORIZONTAL || textOrientation == wxVERTICAL )
        m_colLabelTextOrientation = textOrientation;

    if ( !GetBatchCount() )
        m_colWindow->Refresh();
}

void wxGrid::SetRowLabelValue( int row, const wxString& s )
{
    if ( m_table )
    {
        m_table->SetRowLabelValue( row, s );
        if ( !GetBatchCount() )
        {
            wxRect rect = CellToRect( row, 0 );
            if ( rect.height > 0 )
            {
                CalcScrolledPosition(0, rect.y, &rect.x, &rect.y);
                rect.x = 0;
                rect.width = m_rowLabelWidth;
                m_rowLabelWin->Refresh( true, &rect );
            }
        }
    }
}

void wxGrid::SetColLabelValue( int col, const wxString& s )
{
    if ( m_table )
    {
        m_table->SetColLabelValue( col, s );
        if ( !GetBatchCount() )
        {
            if ( m_useNativeHeader )
            {
                GetGridColHeader()->UpdateColumn(col);
            }
            else
            {
                wxRect rect = CellToRect( 0, col );
                if ( rect.width > 0 )
                {
                    CalcScrolledPosition(rect.x, 0, &rect.x, &rect.y);
                    rect.y = 0;
                    rect.height = m_colLabelHeight;
                    GetColLabelWindow()->Refresh( true, &rect );
                }
            }
        }
    }
}

void wxGrid::SetGridLineColour( const wxColour& colour )
{
    if ( m_gridLineColour != colour )
    {
        m_gridLineColour = colour;

        if ( GridLinesEnabled() )
            RedrawGridLines();
    }
}

void wxGrid::SetCellHighlightColour( const wxColour& colour )
{
    if ( m_cellHighlightColour != colour )
    {
        m_cellHighlightColour = colour;

        wxClientDC dc( m_gridWin );
        PrepareDC( dc );
        wxGridCellAttr* attr = GetCellAttr(m_currentCellCoords);
        DrawCellHighlight(dc, attr);
        attr->DecRef();
    }
}

void wxGrid::SetCellHighlightPenWidth(int width)
{
    if (m_cellHighlightPenWidth != width)
    {
        m_cellHighlightPenWidth = width;

        // Just redrawing the cell highlight is not enough since that won't
        // make any visible change if the thickness is getting smaller.
        int row = m_currentCellCoords.GetRow();
        int col = m_currentCellCoords.GetCol();
        if ( row == -1 || col == -1 || GetColWidth(col) <= 0 || GetRowHeight(row) <= 0 )
            return;

        wxRect rect = CellToRect(row, col);
        m_gridWin->Refresh(true, &rect);
    }
}

void wxGrid::SetCellHighlightROPenWidth(int width)
{
    if (m_cellHighlightROPenWidth != width)
    {
        m_cellHighlightROPenWidth = width;

        // Just redrawing the cell highlight is not enough since that won't
        // make any visible change if the thickness is getting smaller.
        int row = m_currentCellCoords.GetRow();
        int col = m_currentCellCoords.GetCol();
        if ( row == -1 || col == -1 ||
                GetColWidth(col) <= 0 || GetRowHeight(row) <= 0 )
            return;

        wxRect rect = CellToRect(row, col);
        m_gridWin->Refresh(true, &rect);
    }
}

void wxGrid::RedrawGridLines()
{
    // the lines will be redrawn when the window is thawn
    if ( GetBatchCount() )
        return;

    if ( GridLinesEnabled() )
    {
        wxClientDC dc( m_gridWin );
        PrepareDC( dc );
        DrawAllGridLines( dc, wxRegion() );
    }
    else // remove the grid lines
    {
        m_gridWin->Refresh();
    }
}

void wxGrid::EnableGridLines( bool enable )
{
    if ( enable != m_gridLinesEnabled )
    {
        m_gridLinesEnabled = enable;

        RedrawGridLines();
    }
}

void wxGrid::DoClipGridLines(bool& var, bool clip)
{
    if ( clip != var )
    {
        var = clip;

        if ( GridLinesEnabled() )
            RedrawGridLines();
    }
}

int wxGrid::GetDefaultRowSize() const
{
    return m_defaultRowHeight;
}

int wxGrid::GetRowSize( int row ) const
{
    wxCHECK_MSG( row >= 0 && row < m_numRows, 0, wxT("invalid row index") );

    return GetRowHeight(row);
}

int wxGrid::GetDefaultColSize() const
{
    return m_defaultColWidth;
}

int wxGrid::GetColSize( int col ) const
{
    wxCHECK_MSG( col >= 0 && col < m_numCols, 0, wxT("invalid column index") );

    return GetColWidth(col);
}

// ============================================================================
// access to the grid attributes: each of them has a default value in the grid
// itself and may be overidden on a per-cell basis
// ============================================================================

// ----------------------------------------------------------------------------
// setting default attributes
// ----------------------------------------------------------------------------

void wxGrid::SetDefaultCellBackgroundColour( const wxColour& col )
{
    m_defaultCellAttr->SetBackgroundColour(col);
#ifdef __WXGTK__
    m_gridWin->SetBackgroundColour(col);
#endif
}

void wxGrid::SetDefaultCellTextColour( const wxColour& col )
{
    m_defaultCellAttr->SetTextColour(col);
}

void wxGrid::SetDefaultCellAlignment( int horiz, int vert )
{
    m_defaultCellAttr->SetAlignment(horiz, vert);
}

void wxGrid::SetDefaultCellOverflow( bool allow )
{
    m_defaultCellAttr->SetOverflow(allow);
}

void wxGrid::SetDefaultCellFont( const wxFont& font )
{
    m_defaultCellAttr->SetFont(font);
}

// For editors and renderers the type registry takes precedence over the
// default attr, so we need to register the new editor/renderer for the string
// data type in order to make setting a default editor/renderer appear to
// work correctly.

void wxGrid::SetDefaultRenderer(wxGridCellRenderer *renderer)
{
    RegisterDataType(wxGRID_VALUE_STRING,
                     renderer,
                     GetDefaultEditorForType(wxGRID_VALUE_STRING));
}

void wxGrid::SetDefaultEditor(wxGridCellEditor *editor)
{
    RegisterDataType(wxGRID_VALUE_STRING,
                     GetDefaultRendererForType(wxGRID_VALUE_STRING),
                     editor);
}

// ----------------------------------------------------------------------------
// access to the default attributes
// ----------------------------------------------------------------------------

wxColour wxGrid::GetDefaultCellBackgroundColour() const
{
    return m_defaultCellAttr->GetBackgroundColour();
}

wxColour wxGrid::GetDefaultCellTextColour() const
{
    return m_defaultCellAttr->GetTextColour();
}

wxFont wxGrid::GetDefaultCellFont() const
{
    return m_defaultCellAttr->GetFont();
}

void wxGrid::GetDefaultCellAlignment( int *horiz, int *vert ) const
{
    m_defaultCellAttr->GetAlignment(horiz, vert);
}

bool wxGrid::GetDefaultCellOverflow() const
{
    return m_defaultCellAttr->GetOverflow();
}

wxGridCellRenderer *wxGrid::GetDefaultRenderer() const
{
    return m_defaultCellAttr->GetRenderer(NULL, 0, 0);
}

wxGridCellEditor *wxGrid::GetDefaultEditor() const
{
    return m_defaultCellAttr->GetEditor(NULL, 0, 0);
}

// ----------------------------------------------------------------------------
// access to cell attributes
// ----------------------------------------------------------------------------

wxColour wxGrid::GetCellBackgroundColour(int row, int col) const
{
    wxGridCellAttr *attr = GetCellAttr(row, col);
    wxColour colour = attr->GetBackgroundColour();
    attr->DecRef();

    return colour;
}

wxColour wxGrid::GetCellTextColour( int row, int col ) const
{
    wxGridCellAttr *attr = GetCellAttr(row, col);
    wxColour colour = attr->GetTextColour();
    attr->DecRef();

    return colour;
}

wxFont wxGrid::GetCellFont( int row, int col ) const
{
    wxGridCellAttr *attr = GetCellAttr(row, col);
    wxFont font = attr->GetFont();
    attr->DecRef();

    return font;
}

void wxGrid::GetCellAlignment( int row, int col, int *horiz, int *vert ) const
{
    wxGridCellAttr *attr = GetCellAttr(row, col);
    attr->GetAlignment(horiz, vert);
    attr->DecRef();
}

bool wxGrid::GetCellOverflow( int row, int col ) const
{
    wxGridCellAttr *attr = GetCellAttr(row, col);
    bool allow = attr->GetOverflow();
    attr->DecRef();

    return allow;
}

wxGrid::CellSpan
wxGrid::GetCellSize( int row, int col, int *num_rows, int *num_cols ) const
{
    wxGridCellAttr *attr = GetCellAttr(row, col);
    attr->GetSize( num_rows, num_cols );
    attr->DecRef();

    if ( *num_rows == 1 && *num_cols == 1 )
        return CellSpan_None; // just a normal cell

    if ( *num_rows < 0 || *num_cols < 0 )
        return CellSpan_Inside; // covered by a multi-span cell

    // this cell spans multiple cells to its right/bottom
    return CellSpan_Main;
}

wxGridCellRenderer* wxGrid::GetCellRenderer(int row, int col) const
{
    wxGridCellAttr* attr = GetCellAttr(row, col);
    wxGridCellRenderer* renderer = attr->GetRenderer(this, row, col);
    attr->DecRef();

    return renderer;
}

wxGridCellEditor* wxGrid::GetCellEditor(int row, int col) const
{
    wxGridCellAttr* attr = GetCellAttr(row, col);
    wxGridCellEditor* editor = attr->GetEditor(this, row, col);
    attr->DecRef();

    return editor;
}

bool wxGrid::IsReadOnly(int row, int col) const
{
    wxGridCellAttr* attr = GetCellAttr(row, col);
    bool isReadOnly = attr->IsReadOnly();
    attr->DecRef();

    return isReadOnly;
}

// ----------------------------------------------------------------------------
// attribute support: cache, automatic provider creation, ...
// ----------------------------------------------------------------------------

bool wxGrid::CanHaveAttributes() const
{
    if ( !m_table )
    {
        return false;
    }

    return m_table->CanHaveAttributes();
}

void wxGrid::ClearAttrCache()
{
    if ( m_attrCache.row != -1 )
    {
        wxGridCellAttr *oldAttr = m_attrCache.attr;
        m_attrCache.attr = NULL;
        m_attrCache.row = -1;
        // wxSafeDecRec(...) might cause event processing that accesses
        // the cached attribute, if one exists (e.g. by deleting the
        // editor stored within the attribute). Therefore it is important
        // to invalidate the cache  before calling wxSafeDecRef!
        wxSafeDecRef(oldAttr);
    }
}

void wxGrid::RefreshAttr(int row, int col)
{
    if ( m_attrCache.row == row && m_attrCache.col == col )
        ClearAttrCache();
}


void wxGrid::CacheAttr(int row, int col, wxGridCellAttr *attr) const
{
    if ( attr != NULL )
    {
        wxGrid * const self = const_cast<wxGrid *>(this);

        self->ClearAttrCache();
        self->m_attrCache.row = row;
        self->m_attrCache.col = col;
        self->m_attrCache.attr = attr;
        wxSafeIncRef(attr);
    }
}

bool wxGrid::LookupAttr(int row, int col, wxGridCellAttr **attr) const
{
    if ( row == m_attrCache.row && col == m_attrCache.col )
    {
        *attr = m_attrCache.attr;
        wxSafeIncRef(m_attrCache.attr);

#ifdef DEBUG_ATTR_CACHE
        gs_nAttrCacheHits++;
#endif

        return true;
    }
    else
    {
#ifdef DEBUG_ATTR_CACHE
        gs_nAttrCacheMisses++;
#endif

        return false;
    }
}

wxGridCellAttr *wxGrid::GetCellAttr(int row, int col) const
{
    wxGridCellAttr *attr = NULL;
    // Additional test to avoid looking at the cache e.g. for
    // wxNoCellCoords, as this will confuse memory management.
    if ( row >= 0 )
    {
        if ( !LookupAttr(row, col, &attr) )
        {
            attr = m_table ? m_table->GetAttr(row, col, wxGridCellAttr::Any)
                           : NULL;
            CacheAttr(row, col, attr);
        }
    }

    if (attr)
    {
        attr->SetDefAttr(m_defaultCellAttr);
    }
    else
    {
        attr = m_defaultCellAttr;
        attr->IncRef();
    }

    return attr;
}

wxGridCellAttr *wxGrid::GetOrCreateCellAttr(int row, int col) const
{
    wxGridCellAttr *attr = NULL;
    bool canHave = ((wxGrid*)this)->CanHaveAttributes();

    wxCHECK_MSG( canHave, attr, wxT("Cell attributes not allowed"));
    wxCHECK_MSG( m_table, attr, wxT("must have a table") );

    attr = m_table->GetAttr(row, col, wxGridCellAttr::Cell);
    if ( !attr )
    {
        attr = new wxGridCellAttr(m_defaultCellAttr);

        // artificially inc the ref count to match DecRef() in caller
        attr->IncRef();
        m_table->SetAttr(attr, row, col);
    }

    return attr;
}

// ----------------------------------------------------------------------------
// setting column attributes (wrappers around SetColAttr)
// ----------------------------------------------------------------------------

void wxGrid::SetColFormatBool(int col)
{
    SetColFormatCustom(col, wxGRID_VALUE_BOOL);
}

void wxGrid::SetColFormatNumber(int col)
{
    SetColFormatCustom(col, wxGRID_VALUE_NUMBER);
}

void wxGrid::SetColFormatFloat(int col, int width, int precision)
{
    wxString typeName = wxGRID_VALUE_FLOAT;
    if ( (width != -1) || (precision != -1) )
    {
        typeName << wxT(':') << width << wxT(',') << precision;
    }

    SetColFormatCustom(col, typeName);
}

void wxGrid::SetColFormatCustom(int col, const wxString& typeName)
{
    wxGridCellAttr *attr = m_table->GetAttr(-1, col, wxGridCellAttr::Col );
    if (!attr)
        attr = new wxGridCellAttr;
    wxGridCellRenderer *renderer = GetDefaultRendererForType(typeName);
    attr->SetRenderer(renderer);
    wxGridCellEditor *editor = GetDefaultEditorForType(typeName);
    attr->SetEditor(editor);

    SetColAttr(col, attr);

}

// ----------------------------------------------------------------------------
// setting cell attributes: this is forwarded to the table
// ----------------------------------------------------------------------------

void wxGrid::SetAttr(int row, int col, wxGridCellAttr *attr)
{
    if ( CanHaveAttributes() )
    {
        m_table->SetAttr(attr, row, col);
        ClearAttrCache();
    }
    else
    {
        wxSafeDecRef(attr);
    }
}

void wxGrid::SetRowAttr(int row, wxGridCellAttr *attr)
{
    if ( CanHaveAttributes() )
    {
        m_table->SetRowAttr(attr, row);
        ClearAttrCache();
    }
    else
    {
        wxSafeDecRef(attr);
    }
}

void wxGrid::SetColAttr(int col, wxGridCellAttr *attr)
{
    if ( CanHaveAttributes() )
    {
        m_table->SetColAttr(attr, col);
        ClearAttrCache();
    }
    else
    {
        wxSafeDecRef(attr);
    }
}

void wxGrid::SetCellBackgroundColour( int row, int col, const wxColour& colour )
{
    if ( CanHaveAttributes() )
    {
        wxGridCellAttr *attr = GetOrCreateCellAttr(row, col);
        attr->SetBackgroundColour(colour);
        attr->DecRef();
    }
}

void wxGrid::SetCellTextColour( int row, int col, const wxColour& colour )
{
    if ( CanHaveAttributes() )
    {
        wxGridCellAttr *attr = GetOrCreateCellAttr(row, col);
        attr->SetTextColour(colour);
        attr->DecRef();
    }
}

void wxGrid::SetCellFont( int row, int col, const wxFont& font )
{
    if ( CanHaveAttributes() )
    {
        wxGridCellAttr *attr = GetOrCreateCellAttr(row, col);
        attr->SetFont(font);
        attr->DecRef();
    }
}

void wxGrid::SetCellAlignment( int row, int col, int horiz, int vert )
{
    if ( CanHaveAttributes() )
    {
        wxGridCellAttr *attr = GetOrCreateCellAttr(row, col);
        attr->SetAlignment(horiz, vert);
        attr->DecRef();
    }
}

void wxGrid::SetCellOverflow( int row, int col, bool allow )
{
    if ( CanHaveAttributes() )
    {
        wxGridCellAttr *attr = GetOrCreateCellAttr(row, col);
        attr->SetOverflow(allow);
        attr->DecRef();
    }
}

void wxGrid::SetCellSize( int row, int col, int num_rows, int num_cols )
{
    if ( CanHaveAttributes() )
    {
        int cell_rows, cell_cols;

        wxGridCellAttr *attr = GetOrCreateCellAttr(row, col);
        attr->GetSize(&cell_rows, &cell_cols);
        attr->SetSize(num_rows, num_cols);
        attr->DecRef();

        // Cannot set the size of a cell to 0 or negative values
        // While it is perfectly legal to do that, this function cannot
        // handle all the possibilies, do it by hand by getting the CellAttr.
        // You can only set the size of a cell to 1,1 or greater with this fn
        wxASSERT_MSG( !((cell_rows < 1) || (cell_cols < 1)),
                      wxT("wxGrid::SetCellSize setting cell size that is already part of another cell"));
        wxASSERT_MSG( !((num_rows < 1) || (num_cols < 1)),
                      wxT("wxGrid::SetCellSize setting cell size to < 1"));

        // if this was already a multicell then "turn off" the other cells first
        if ((cell_rows > 1) || (cell_cols > 1))
        {
            int i, j;
            for (j=row; j < row + cell_rows; j++)
            {
                for (i=col; i < col + cell_cols; i++)
                {
                    if ((i != col) || (j != row))
                    {
                        wxGridCellAttr *attr_stub = GetOrCreateCellAttr(j, i);
                        attr_stub->SetSize( 1, 1 );
                        attr_stub->DecRef();
                    }
                }
            }
        }

        // mark the cells that will be covered by this cell to
        // negative or zero values to point back at this cell
        if (((num_rows > 1) || (num_cols > 1)) && (num_rows >= 1) && (num_cols >= 1))
        {
            int i, j;
            for (j=row; j < row + num_rows; j++)
            {
                for (i=col; i < col + num_cols; i++)
                {
                    if ((i != col) || (j != row))
                    {
                        wxGridCellAttr *attr_stub = GetOrCreateCellAttr(j, i);
                        attr_stub->SetSize( row - j, col - i );
                        attr_stub->DecRef();
                    }
                }
            }
        }
    }
}

void wxGrid::SetCellRenderer(int row, int col, wxGridCellRenderer *renderer)
{
    if ( CanHaveAttributes() )
    {
        wxGridCellAttr *attr = GetOrCreateCellAttr(row, col);
        attr->SetRenderer(renderer);
        attr->DecRef();
    }
}

void wxGrid::SetCellEditor(int row, int col, wxGridCellEditor* editor)
{
    if ( CanHaveAttributes() )
    {
        wxGridCellAttr *attr = GetOrCreateCellAttr(row, col);
        attr->SetEditor(editor);
        attr->DecRef();
    }
}

void wxGrid::SetReadOnly(int row, int col, bool isReadOnly)
{
    if ( CanHaveAttributes() )
    {
        wxGridCellAttr *attr = GetOrCreateCellAttr(row, col);
        attr->SetReadOnly(isReadOnly);
        attr->DecRef();
    }
}

// ----------------------------------------------------------------------------
// Data type registration
// ----------------------------------------------------------------------------

void wxGrid::RegisterDataType(const wxString& typeName,
                              wxGridCellRenderer* renderer,
                              wxGridCellEditor* editor)
{
    m_typeRegistry->RegisterDataType(typeName, renderer, editor);
}


wxGridCellEditor * wxGrid::GetDefaultEditorForCell(int row, int col) const
{
    wxString typeName = m_table->GetTypeName(row, col);
    return GetDefaultEditorForType(typeName);
}

wxGridCellRenderer * wxGrid::GetDefaultRendererForCell(int row, int col) const
{
    wxString typeName = m_table->GetTypeName(row, col);
    return GetDefaultRendererForType(typeName);
}

wxGridCellEditor * wxGrid::GetDefaultEditorForType(const wxString& typeName) const
{
    int index = m_typeRegistry->FindOrCloneDataType(typeName);
    if ( index == wxNOT_FOUND )
    {
        wxFAIL_MSG(wxString::Format(wxT("Unknown data type name [%s]"), typeName.c_str()));

        return NULL;
    }

    return m_typeRegistry->GetEditor(index);
}

wxGridCellRenderer * wxGrid::GetDefaultRendererForType(const wxString& typeName) const
{
    int index = m_typeRegistry->FindOrCloneDataType(typeName);
    if ( index == wxNOT_FOUND )
    {
        wxFAIL_MSG(wxString::Format(wxT("Unknown data type name [%s]"), typeName.c_str()));

        return NULL;
    }

    return m_typeRegistry->GetRenderer(index);
}

// ----------------------------------------------------------------------------
// row/col size
// ----------------------------------------------------------------------------

void wxGrid::DoDisableLineResize(int line, wxGridFixedIndicesSet *& setFixed)
{
    if ( !setFixed )
    {
        setFixed = new wxGridFixedIndicesSet;
    }

    setFixed->insert(line);
}

bool
wxGrid::DoCanResizeLine(int line, const wxGridFixedIndicesSet *setFixed) const
{
    return !setFixed || !setFixed->count(line);
}

void wxGrid::EnableDragRowSize( bool enable )
{
    m_canDragRowSize = enable;
}

void wxGrid::EnableDragColSize( bool enable )
{
    m_canDragColSize = enable;
}

void wxGrid::EnableDragGridSize( bool enable )
{
    m_canDragGridSize = enable;
}

void wxGrid::EnableDragCell( bool enable )
{
    m_canDragCell = enable;
}

void wxGrid::SetDefaultRowSize( int height, bool resizeExistingRows )
{
    m_defaultRowHeight = wxMax( height, m_minAcceptableRowHeight );

    if ( resizeExistingRows )
    {
        // since we are resizing all rows to the default row size,
        // we can simply clear the row heights and row bottoms
        // arrays (which also allows us to take advantage of
        // some speed optimisations)
        m_rowHeights.Empty();
        m_rowBottoms.Empty();
        if ( !GetBatchCount() )
            CalcDimensions();
    }
}

namespace
{

// This is a common part of SetRowSize() and SetColSize() which takes care of
// updating the height/width of a row/column depending on its current value and
// the new one.
//
// Returns the difference between the new and the old size.
int UpdateRowOrColSize(int& sizeCurrent, int sizeNew)
{
    // On input here sizeCurrent can be negative if it's currently hidden (the
    // real size is its absolute value then). And sizeNew can be 0 to indicate
    // that the row/column should be hidden or -1 to indicate that it should be
    // shown again.

    if ( sizeNew < 0 )
    {
        // We're showing back a previously hidden row/column.
        wxASSERT_MSG( sizeNew == -1, wxS("New size must be positive or -1.") );

        // If it's already visible, simply do nothing.
        if ( sizeCurrent >= 0 )
            return 0;

        // Otherwise show it by restoring its old size.
        sizeCurrent = -sizeCurrent;

        // This is positive which is correct.
        return sizeCurrent;
    }
    else if ( sizeNew == 0 )
    {
        // We're hiding a row/column.

        // If it's already hidden, simply do nothing.
        if ( sizeCurrent <= 0 )
            return 0;

        // Otherwise hide it and also remember the shown size to be able to
        // restore it later.
        sizeCurrent = -sizeCurrent;

        // This is negative which is correct.
        return sizeCurrent;
    }
    else // We're just changing the row/column size.
    {
        // Here it could have been hidden or not previously.
        const int sizeOld = sizeCurrent < 0 ? 0 : sizeCurrent;

        sizeCurrent = sizeNew;

        return sizeCurrent - sizeOld;
    }
}

} // anonymous namespace

void wxGrid::SetRowSize( int row, int height )
{
    // See comment in SetColSize
    if ( height > 0 && height < GetRowMinimalAcceptableHeight())
        return;

    // The value of -1 is special and means to fit the height to the row label.
    // As with the columns, ignore attempts to auto-size the hidden rows.
    if ( height == -1 && GetRowHeight(row) != 0 )
    {
        long w, h;
        wxArrayString lines;
        wxClientDC dc(m_rowLabelWin);
        dc.SetFont(GetLabelFont());
        StringToLines(GetRowLabelValue( row ), lines);
        GetTextBoxSize( dc, lines, &w, &h );

        // As with the columns, don't make the row smaller than minimal height.
        height = wxMax(h, GetRowMinimalHeight(row));
    }

    DoSetRowSize(row, height);
}

void wxGrid::DoSetRowSize( int row, int height )
{
    wxCHECK_RET( row >= 0 && row < m_numRows, wxT("invalid row index") );

    if ( m_rowHeights.IsEmpty() )
    {
        // need to really create the array
        InitRowHeights();
    }

    const int diff = UpdateRowOrColSize(m_rowHeights[row], height);
    if ( !diff )
        return;

    for ( int i = row; i < m_numRows; i++ )
    {
        m_rowBottoms[i] += diff;
    }

    InvalidateBestSize();

    if ( !GetBatchCount() )
    {
        CalcDimensions();
        Refresh();
    }
}

void wxGrid::SetDefaultColSize( int width, bool resizeExistingCols )
{
    // we dont allow zero default column width
    m_defaultColWidth = wxMax( wxMax( width, m_minAcceptableColWidth ), 1 );

    if ( resizeExistingCols )
    {
        // since we are resizing all columns to the default column size,
        // we can simply clear the col widths and col rights
        // arrays (which also allows us to take advantage of
        // some speed optimisations)
        m_colWidths.Empty();
        m_colRights.Empty();
        if ( !GetBatchCount() )
            CalcDimensions();
    }
}

void wxGrid::SetColSize( int col, int width )
{
    // we intentionally don't test whether the width is less than
    // GetColMinimalWidth() here but we do compare it with
    // GetColMinimalAcceptableWidth() as otherwise things currently break (see
    // #651) -- and we also always allow the width of 0 as it has the special
    // sense of hiding the column
    if ( width > 0 && width < GetColMinimalAcceptableWidth() )
        return;

    // The value of -1 is special and means to fit the width to the column label.
    //
    // Notice that we currently don't support auto-sizing hidden columns (we
    // could, but it's not clear whether this is really needed and it would
    // make the code more complex), and for them passing -1 simply means to
    // show the column back using its old size.
    if ( width == -1 && GetColWidth(col) != 0 )
    {
        long w, h;
        wxArrayString lines;
        wxClientDC dc(m_colWindow);
        dc.SetFont(GetLabelFont());
        StringToLines(GetColLabelValue(col), lines);
        if ( GetColLabelTextOrientation() == wxHORIZONTAL )
            GetTextBoxSize( dc, lines, &w, &h );
        else
            GetTextBoxSize( dc, lines, &h, &w );
        width = w + 6;

        // Check that it is not less than the minimal width and do use the
        // possibly greater than minimal-acceptable-width minimal-width itself
        // here as we shouldn't become too small when auto-sizing, otherwise
        // the column could be resized to be too small by double clicking its
        // divider line (which ends up in a call to this function) even though
        // it couldn't be resized to this size by dragging it.
        width = wxMax(width, GetColMinimalWidth(col));
    }

    DoSetColSize(col, width);
}

void wxGrid::DoSetColSize( int col, int width )
{
    wxCHECK_RET( col >= 0 && col < m_numCols, wxT("invalid column index") );

    if ( m_colWidths.IsEmpty() )
    {
        // need to really create the array
        InitColWidths();
    }

    const int diff = UpdateRowOrColSize(m_colWidths[col], width);
    if ( !diff )
        return;

    if ( m_useNativeHeader )
        GetGridColHeader()->UpdateColumn(col);
    //else: will be refreshed when the header is redrawn

    for ( int colPos = GetColPos(col); colPos < m_numCols; colPos++ )
    {
        m_colRights[GetColAt(colPos)] += diff;
    }

    InvalidateBestSize();

    if ( !GetBatchCount() )
    {
        CalcDimensions();
        Refresh();
    }
}

void wxGrid::SetColMinimalWidth( int col, int width )
{
    if (width > GetColMinimalAcceptableWidth())
    {
        wxLongToLongHashMap::key_type key = (wxLongToLongHashMap::key_type)col;
        m_colMinWidths[key] = width;
    }
}

void wxGrid::SetRowMinimalHeight( int row, int width )
{
    if (width > GetRowMinimalAcceptableHeight())
    {
        wxLongToLongHashMap::key_type key = (wxLongToLongHashMap::key_type)row;
        m_rowMinHeights[key] = width;
    }
}

int wxGrid::GetColMinimalWidth(int col) const
{
    wxLongToLongHashMap::key_type key = (wxLongToLongHashMap::key_type)col;
    wxLongToLongHashMap::const_iterator it = m_colMinWidths.find(key);

    return it != m_colMinWidths.end() ? (int)it->second : m_minAcceptableColWidth;
}

int wxGrid::GetRowMinimalHeight(int row) const
{
    wxLongToLongHashMap::key_type key = (wxLongToLongHashMap::key_type)row;
    wxLongToLongHashMap::const_iterator it = m_rowMinHeights.find(key);

    return it != m_rowMinHeights.end() ? (int)it->second : m_minAcceptableRowHeight;
}

void wxGrid::SetColMinimalAcceptableWidth( int width )
{
    // We do allow a width of 0 since this gives us
    // an easy way to temporarily hiding columns.
    if ( width >= 0 )
        m_minAcceptableColWidth = width;
}

void wxGrid::SetRowMinimalAcceptableHeight( int height )
{
    // We do allow a height of 0 since this gives us
    // an easy way to temporarily hiding rows.
    if ( height >= 0 )
        m_minAcceptableRowHeight = height;
}

int  wxGrid::GetColMinimalAcceptableWidth() const
{
    return m_minAcceptableColWidth;
}

int  wxGrid::GetRowMinimalAcceptableHeight() const
{
    return m_minAcceptableRowHeight;
}

// ----------------------------------------------------------------------------
// auto sizing
// ----------------------------------------------------------------------------

void
wxGrid::AutoSizeColOrRow(int colOrRow, bool setAsMin, wxGridDirection direction)
{
    const bool column = direction == wxGRID_COLUMN;

    // We don't support auto-sizing hidden rows or columns, this doesn't seem
    // to make much sense.
    if ( column )
    {
        if ( GetColWidth(colOrRow) == 0 )
            return;
    }
    else
    {
        if ( GetRowHeight(colOrRow) == 0 )
            return;
    }

    wxClientDC dc(m_gridWin);

    // cancel editing of cell
    HideCellEditControl();
    SaveEditControlValue();

    // initialize both of them just to avoid compiler warnings even if only
    // really needs to be initialized here
    int row,
        col;
    if ( column )
    {
        row = -1;
        col = colOrRow;
    }
    else
    {
        row = colOrRow;
        col = -1;
    }

    wxCoord extent, extentMax = 0;
    int max = column ? m_numRows : m_numCols;
    for ( int rowOrCol = 0; rowOrCol < max; rowOrCol++ )
    {
        if ( column )
        {
            if ( !IsRowShown(rowOrCol) )
                continue;

            row = rowOrCol;
            col = colOrRow;
        }
        else
        {
            if ( !IsColShown(rowOrCol) )
                continue;

            row = colOrRow;
            col = rowOrCol;
        }

        // we need to account for the cells spanning multiple columns/rows:
        // while they may need a lot of space, they don't need all of it in
        // this column/row
        int numRows, numCols;
        const CellSpan span = GetCellSize(row, col, &numRows, &numCols);
        if ( span == CellSpan_Inside )
        {
            // we need to get the size of the main cell, not of a cell hidden
            // by it
            row += numRows;
            col += numCols;

            // get the size of the main cell too
            GetCellSize(row, col, &numRows, &numCols);
        }

        // get cell ( main cell if CellSpan_Inside ) renderer best size
        wxGridCellAttr *attr = GetCellAttr(row, col);
        wxGridCellRenderer *renderer = attr->GetRenderer(this, row, col);
        if ( renderer )
        {
            wxSize size = renderer->GetBestSize(*this, *attr, dc, row, col);
            extent = column ? size.x : size.y;

            if ( span != CellSpan_None )
            {
                // we spread the size of a spanning cell over all the cells it
                // covers evenly -- this is probably not ideal but we can't
                // really do much better here
                //
                // notice that numCols and numRows are never 0 as they
                // correspond to the size of the main cell of the span and not
                // of the cell inside it
                extent /= column ? numCols : numRows;
            }

            if ( extent > extentMax )
                extentMax = extent;

            renderer->DecRef();
        }

        attr->DecRef();
    }

    // now also compare with the column label extent
    wxCoord w, h;
    dc.SetFont( GetLabelFont() );

    if ( column )
    {
        dc.GetMultiLineTextExtent( GetColLabelValue(colOrRow), &w, &h );
        if ( GetColLabelTextOrientation() == wxVERTICAL )
            w = h;
    }
    else
        dc.GetMultiLineTextExtent( GetRowLabelValue(colOrRow), &w, &h );

    extent = column ? w : h;
    if ( extent > extentMax )
        extentMax = extent;

    if ( !extentMax )
    {
        // empty column - give default extent (notice that if extentMax is less
        // than default extent but != 0, it's OK)
        extentMax = column ? m_defaultColWidth : m_defaultRowHeight;
    }
    else
    {
        if ( column )
            // leave some space around text
            extentMax += 10;
        else
            extentMax += 6;
    }

    if ( column )
    {
        // Ensure automatic width is not less than minimal width. See the
        // comment in SetColSize() for explanation of why this isn't done
        // in SetColSize().
        if ( !setAsMin )
            extentMax = wxMax(extentMax, GetColMinimalWidth(colOrRow));

        SetColSize( colOrRow, extentMax );
        if ( !GetBatchCount() )
        {
            if ( m_useNativeHeader )
            {
                GetGridColHeader()->UpdateColumn(colOrRow);
            }
            else
            {
                int cw, ch, dummy;
                m_gridWin->GetClientSize( &cw, &ch );
                wxRect rect ( CellToRect( 0, colOrRow ) );
                rect.y = 0;
                CalcScrolledPosition(rect.x, 0, &rect.x, &dummy);
                rect.width = cw - rect.x;
                rect.height = m_colLabelHeight;
                GetColLabelWindow()->Refresh( true, &rect );
            }
        }
    }
    else
    {
        // Ensure automatic width is not less than minimal height. See the
        // comment in SetColSize() for explanation of why this isn't done
        // in SetRowSize().
        if ( !setAsMin )
            extentMax = wxMax(extentMax, GetRowMinimalHeight(colOrRow));

        SetRowSize(colOrRow, extentMax);
        if ( !GetBatchCount() )
        {
            int cw, ch, dummy;
            m_gridWin->GetClientSize( &cw, &ch );
            wxRect rect( CellToRect( colOrRow, 0 ) );
            rect.x = 0;
            CalcScrolledPosition(0, rect.y, &dummy, &rect.y);
            rect.width = m_rowLabelWidth;
            rect.height = ch - rect.y;
            m_rowLabelWin->Refresh( true, &rect );
        }
    }

    if ( setAsMin )
    {
        if ( column )
            SetColMinimalWidth(colOrRow, extentMax);
        else
            SetRowMinimalHeight(colOrRow, extentMax);
    }
}

wxCoord wxGrid::CalcColOrRowLabelAreaMinSize(wxGridDirection direction)
{
    // calculate size for the rows or columns?
    const bool calcRows = direction == wxGRID_ROW;

    wxClientDC dc(calcRows ? GetGridRowLabelWindow()
                           : GetGridColLabelWindow());
    dc.SetFont(GetLabelFont());

    // which dimension should we take into account for calculations?
    //
    // for columns, the text can be only horizontal so it's easy but for rows
    // we also have to take into account the text orientation
    const bool
        useWidth = calcRows || (GetColLabelTextOrientation() == wxVERTICAL);

    wxArrayString lines;
    wxCoord extentMax = 0;

    const int numRowsOrCols = calcRows ? m_numRows : m_numCols;
    for ( int rowOrCol = 0; rowOrCol < numRowsOrCols; rowOrCol++ )
    {
        lines.Clear();

        wxString label = calcRows ? GetRowLabelValue(rowOrCol)
                                  : GetColLabelValue(rowOrCol);
        StringToLines(label, lines);

        long w, h;
        GetTextBoxSize(dc, lines, &w, &h);

        const wxCoord extent = useWidth ? w : h;
        if ( extent > extentMax )
            extentMax = extent;
    }

    if ( !extentMax )
    {
        // empty column - give default extent (notice that if extentMax is less
        // than default extent but != 0, it's OK)
        extentMax = calcRows ? GetDefaultRowLabelSize()
                             : GetDefaultColLabelSize();
    }

    // leave some space around text (taken from AutoSizeColOrRow)
    if ( calcRows )
        extentMax += 10;
    else
        extentMax += 6;

    return extentMax;
}

int wxGrid::SetOrCalcColumnSizes(bool calcOnly, bool setAsMin)
{
    int width = m_rowLabelWidth;

    wxGridUpdateLocker locker;
    if(!calcOnly)
        locker.Create(this);

    for ( int col = 0; col < m_numCols; col++ )
    {
        if ( !calcOnly )
            AutoSizeColumn(col, setAsMin);

        width += GetColWidth(col);
    }

    return width;
}

int wxGrid::SetOrCalcRowSizes(bool calcOnly, bool setAsMin)
{
    int height = m_colLabelHeight;

    wxGridUpdateLocker locker;
    if(!calcOnly)
        locker.Create(this);

    for ( int row = 0; row < m_numRows; row++ )
    {
        if ( !calcOnly )
            AutoSizeRow(row, setAsMin);

        height += GetRowHeight(row);
    }

    return height;
}

void wxGrid::AutoSize()
{
    wxGridUpdateLocker locker(this);

    wxSize size(SetOrCalcColumnSizes(false) - m_rowLabelWidth + m_extraWidth,
                SetOrCalcRowSizes(false) - m_colLabelHeight + m_extraHeight);

    // we know that we're not going to have scrollbars so disable them now to
    // avoid trouble in SetClientSize() which can otherwise set the correct
    // client size but also leave space for (not needed any more) scrollbars
    SetScrollbars(m_xScrollPixelsPerLine, m_yScrollPixelsPerLine,
                  0, 0, 0, 0, true);

    SetClientSize(size.x + m_rowLabelWidth, size.y + m_colLabelHeight);
}

void wxGrid::AutoSizeRowLabelSize( int row )
{
    // Hide the edit control, so it
    // won't interfere with drag-shrinking.
    if ( IsCellEditControlShown() )
    {
        HideCellEditControl();
        SaveEditControlValue();
    }

    // autosize row height depending on label text
    SetRowSize(row, -1);

    ForceRefresh();
}

void wxGrid::AutoSizeColLabelSize( int col )
{
    // Hide the edit control, so it
    // won't interfere with drag-shrinking.
    if ( IsCellEditControlShown() )
    {
        HideCellEditControl();
        SaveEditControlValue();
    }

    // autosize column width depending on label text
    SetColSize(col, -1);

    ForceRefresh();
}

wxSize wxGrid::DoGetBestSize() const
{
    wxGrid * const self = const_cast<wxGrid *>(this);

    // we do the same as in AutoSize() here with the exception that we don't
    // change the column/row sizes, only calculate them
    wxSize size(self->SetOrCalcColumnSizes(true) - m_rowLabelWidth + m_extraWidth,
                self->SetOrCalcRowSizes(true) - m_colLabelHeight + m_extraHeight);

    return wxSize(size.x + m_rowLabelWidth, size.y + m_colLabelHeight)
            + GetWindowBorderSize();
}

void wxGrid::Fit()
{
    AutoSize();
}

#if WXWIN_COMPATIBILITY_2_8
wxPen& wxGrid::GetDividerPen() const
{
    return wxNullPen;
}
#endif // WXWIN_COMPATIBILITY_2_8

// ----------------------------------------------------------------------------
// cell value accessor functions
// ----------------------------------------------------------------------------

void wxGrid::SetCellValue( int row, int col, const wxString& s )
{
    if ( m_table )
    {
        m_table->SetValue( row, col, s );
        if ( !GetBatchCount() )
        {
            int dummy;
            wxRect rect( CellToRect( row, col ) );
            rect.x = 0;
            rect.width = m_gridWin->GetClientSize().GetWidth();
            CalcScrolledPosition(0, rect.y, &dummy, &rect.y);
            m_gridWin->Refresh( false, &rect );
        }

        if ( m_currentCellCoords.GetRow() == row &&
             m_currentCellCoords.GetCol() == col &&
             IsCellEditControlShown())
             // Note: If we are using IsCellEditControlEnabled,
             // this interacts badly with calling SetCellValue from
             // an EVT_GRID_CELL_CHANGE handler.
        {
            HideCellEditControl();
            ShowCellEditControl(); // will reread data from table
        }
    }
}

// ----------------------------------------------------------------------------
// block, row and column selection
// ----------------------------------------------------------------------------

void wxGrid::SelectRow( int row, bool addToSelected )
{
    if ( !m_selection )
        return;

    if ( !addToSelected )
        ClearSelection();

    m_selection->SelectRow(row);
}

void wxGrid::SelectCol( int col, bool addToSelected )
{
    if ( !m_selection )
        return;

    if ( !addToSelected )
        ClearSelection();

    m_selection->SelectCol(col);
}

void wxGrid::SelectBlock(int topRow, int leftCol, int bottomRow, int rightCol,
                         bool addToSelected)
{
    if ( !m_selection )
        return;

    if ( !addToSelected )
        ClearSelection();

    m_selection->SelectBlock(topRow, leftCol, bottomRow, rightCol);
}

void wxGrid::SelectAll()
{
    if ( m_numRows > 0 && m_numCols > 0 )
    {
        if ( m_selection )
            m_selection->SelectBlock( 0, 0, m_numRows - 1, m_numCols - 1 );
    }
}

// ----------------------------------------------------------------------------
// cell, row and col deselection
// ----------------------------------------------------------------------------

void wxGrid::DeselectLine(int line, const wxGridOperations& oper)
{
    if ( !m_selection )
        return;

    const wxGridSelectionModes mode = m_selection->GetSelectionMode();
    if ( mode == oper.GetSelectionMode() ||
            mode == wxGrid::wxGridSelectRowsOrColumns )
    {
        const wxGridCellCoords c(oper.MakeCoords(line, 0));
        if ( m_selection->IsInSelection(c) )
            m_selection->ToggleCellSelection(c);
    }
    else if ( mode != oper.Dual().GetSelectionMode() )
    {
        const int nOther = oper.Dual().GetNumberOfLines(this);
        for ( int i = 0; i < nOther; i++ )
        {
            const wxGridCellCoords c(oper.MakeCoords(line, i));
            if ( m_selection->IsInSelection(c) )
                m_selection->ToggleCellSelection(c);
        }
    }
    //else: can only select orthogonal lines so no lines in this direction
    //      could have been selected anyhow
}

void wxGrid::DeselectRow(int row)
{
    DeselectLine(row, wxGridRowOperations());
}

void wxGrid::DeselectCol(int col)
{
    DeselectLine(col, wxGridColumnOperations());
}

void wxGrid::DeselectCell( int row, int col )
{
    if ( m_selection && m_selection->IsInSelection(row, col) )
        m_selection->ToggleCellSelection(row, col);
}

bool wxGrid::IsSelection() const
{
    return ( m_selection && (m_selection->IsSelection() ||
             ( m_selectedBlockTopLeft != wxGridNoCellCoords &&
               m_selectedBlockBottomRight != wxGridNoCellCoords) ) );
}

bool wxGrid::IsInSelection( int row, int col ) const
{
    return ( m_selection && (m_selection->IsInSelection( row, col ) ||
             ( row >= m_selectedBlockTopLeft.GetRow() &&
               col >= m_selectedBlockTopLeft.GetCol() &&
               row <= m_selectedBlockBottomRight.GetRow() &&
               col <= m_selectedBlockBottomRight.GetCol() )) );
}

wxGridCellCoordsArray wxGrid::GetSelectedCells() const
{
    if (!m_selection)
    {
        wxGridCellCoordsArray a;
        return a;
    }

    return m_selection->m_cellSelection;
}

wxGridCellCoordsArray wxGrid::GetSelectionBlockTopLeft() const
{
    if (!m_selection)
    {
        wxGridCellCoordsArray a;
        return a;
    }

    return m_selection->m_blockSelectionTopLeft;
}

wxGridCellCoordsArray wxGrid::GetSelectionBlockBottomRight() const
{
    if (!m_selection)
    {
        wxGridCellCoordsArray a;
        return a;
    }

    return m_selection->m_blockSelectionBottomRight;
}

wxArrayInt wxGrid::GetSelectedRows() const
{
    if (!m_selection)
    {
        wxArrayInt a;
        return a;
    }

    return m_selection->m_rowSelection;
}

wxArrayInt wxGrid::GetSelectedCols() const
{
    if (!m_selection)
    {
        wxArrayInt a;
        return a;
    }

    return m_selection->m_colSelection;
}

void wxGrid::ClearSelection()
{
    wxRect r1 = BlockToDeviceRect(m_selectedBlockTopLeft,
                                  m_selectedBlockBottomRight);
    wxRect r2 = BlockToDeviceRect(m_currentCellCoords,
                                  m_selectedBlockCorner);

    m_selectedBlockTopLeft =
    m_selectedBlockBottomRight =
    m_selectedBlockCorner = wxGridNoCellCoords;

    if ( !r1.IsEmpty() )
        RefreshRect(r1, false);
    if ( !r2.IsEmpty() )
        RefreshRect(r2, false);

    if ( m_selection )
        m_selection->ClearSelection();
}

// This function returns the rectangle that encloses the given block
// in device coords clipped to the client size of the grid window.
//
wxRect wxGrid::BlockToDeviceRect( const wxGridCellCoords& topLeft,
                                  const wxGridCellCoords& bottomRight ) const
{
    wxRect resultRect;
    wxRect tempCellRect = CellToRect(topLeft);
    if ( tempCellRect != wxGridNoCellRect )
    {
        resultRect = tempCellRect;
    }
    else
    {
        resultRect = wxRect(0, 0, 0, 0);
    }

    tempCellRect = CellToRect(bottomRight);
    if ( tempCellRect != wxGridNoCellRect )
    {
        resultRect += tempCellRect;
    }
    else
    {
        // If both inputs were "wxGridNoCellRect," then there's nothing to do.
        return wxGridNoCellRect;
    }

    // Ensure that left/right and top/bottom pairs are in order.
    int left = resultRect.GetLeft();
    int top = resultRect.GetTop();
    int right = resultRect.GetRight();
    int bottom = resultRect.GetBottom();

    int leftCol = topLeft.GetCol();
    int topRow = topLeft.GetRow();
    int rightCol = bottomRight.GetCol();
    int bottomRow = bottomRight.GetRow();

    if (left > right)
    {
        int tmp = left;
        left = right;
        right = tmp;

        tmp = leftCol;
        leftCol = rightCol;
        rightCol = tmp;
    }

    if (top > bottom)
    {
        int tmp = top;
        top = bottom;
        bottom = tmp;

        tmp = topRow;
        topRow = bottomRow;
        bottomRow = tmp;
    }

    // The following loop is ONLY necessary to detect and handle merged cells.
    int cw, ch;
    m_gridWin->GetClientSize( &cw, &ch );

    // Get the origin coordinates: notice that they will be negative if the
    // grid is scrolled downwards/to the right.
    int gridOriginX = 0;
    int gridOriginY = 0;
    CalcScrolledPosition(gridOriginX, gridOriginY, &gridOriginX, &gridOriginY);

    int onScreenLeftmostCol = internalXToCol(-gridOriginX);
    int onScreenUppermostRow = internalYToRow(-gridOriginY);

    int onScreenRightmostCol = internalXToCol(-gridOriginX + cw);
    int onScreenBottommostRow = internalYToRow(-gridOriginY + ch);

    // Bound our loop so that we only examine the portion of the selected block
    // that is shown on screen. Therefore, we compare the Top-Left block values
    // to the Top-Left screen values, and the Bottom-Right block values to the
    // Bottom-Right screen values, choosing appropriately.
    const int visibleTopRow = wxMax(topRow, onScreenUppermostRow);
    const int visibleBottomRow = wxMin(bottomRow, onScreenBottommostRow);
    const int visibleLeftCol = wxMax(leftCol, onScreenLeftmostCol);
    const int visibleRightCol = wxMin(rightCol, onScreenRightmostCol);

    for ( int j = visibleTopRow; j <= visibleBottomRow; j++ )
    {
        for ( int i = visibleLeftCol; i <= visibleRightCol; i++ )
        {
            if ( (j == visibleTopRow) || (j == visibleBottomRow) ||
                    (i == visibleLeftCol) || (i == visibleRightCol) )
            {
                tempCellRect = CellToRect( j, i );

                if (tempCellRect.x < left)
                    left = tempCellRect.x;
                if (tempCellRect.y < top)
                    top = tempCellRect.y;
                if (tempCellRect.x + tempCellRect.width > right)
                    right = tempCellRect.x + tempCellRect.width;
                if (tempCellRect.y + tempCellRect.height > bottom)
                    bottom = tempCellRect.y + tempCellRect.height;
            }
            else
            {
                i = visibleRightCol; // jump over inner cells.
            }
        }
    }

    // Convert to scrolled coords
    CalcScrolledPosition( left, top, &left, &top );
    CalcScrolledPosition( right, bottom, &right, &bottom );

    if (right < 0 || bottom < 0 || left > cw || top > ch)
        return wxRect(0,0,0,0);

    resultRect.SetLeft( wxMax(0, left) );
    resultRect.SetTop( wxMax(0, top) );
    resultRect.SetRight( wxMin(cw, right) );
    resultRect.SetBottom( wxMin(ch, bottom) );

    return resultRect;
}

void wxGrid::DoSetSizes(const wxGridSizesInfo& sizeInfo,
                        const wxGridOperations& oper)
{
    BeginBatch();
    oper.SetDefaultLineSize(this, sizeInfo.m_sizeDefault, true);
    const int numLines = oper.GetNumberOfLines(this);
    for ( int i = 0; i < numLines; i++ )
    {
        int size = sizeInfo.GetSize(i);
        if ( size != sizeInfo.m_sizeDefault)
            oper.SetLineSize(this, i, size);
    }
    EndBatch();
}

void wxGrid::SetColSizes(const wxGridSizesInfo& sizeInfo)
{
    DoSetSizes(sizeInfo, wxGridColumnOperations());
}

void wxGrid::SetRowSizes(const wxGridSizesInfo& sizeInfo)
{
    DoSetSizes(sizeInfo, wxGridRowOperations());
}

wxGridSizesInfo::wxGridSizesInfo(int defSize, const wxArrayInt& allSizes)
{
    m_sizeDefault = defSize;
    for ( size_t i = 0; i < allSizes.size(); i++ )
    {
        if ( allSizes[i] != defSize )
            m_customSizes[i] = allSizes[i];
    }
}

int wxGridSizesInfo::GetSize(unsigned pos) const
{
    wxUnsignedToIntHashMap::const_iterator it = m_customSizes.find(pos);

    // if it's not found return the default
    if ( it == m_customSizes.end() )
      return m_sizeDefault;

    // otherwise return 0 if it's hidden, currently there is no way to get
    // its size before it had been hidden
    if ( it->second < 0 )
      return 0;

    return it->second;
}

// ----------------------------------------------------------------------------
// drop target
// ----------------------------------------------------------------------------

#if wxUSE_DRAG_AND_DROP

// this allow setting drop target directly on wxGrid
void wxGrid::SetDropTarget(wxDropTarget *dropTarget)
{
    GetGridWindow()->SetDropTarget(dropTarget);
}

#endif // wxUSE_DRAG_AND_DROP

// ----------------------------------------------------------------------------
// grid event classes
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS( wxGridEvent, wxNotifyEvent )

wxGridEvent::wxGridEvent( int id, wxEventType type, wxObject* obj,
                          int row, int col, int x, int y, bool sel,
                          bool control, bool shift, bool alt, bool meta )
        : wxNotifyEvent( type, id ),
          wxKeyboardState(control, shift, alt, meta)
{
    Init(row, col, x, y, sel);

    SetEventObject(obj);
}

IMPLEMENT_DYNAMIC_CLASS( wxGridSizeEvent, wxNotifyEvent )

wxGridSizeEvent::wxGridSizeEvent( int id, wxEventType type, wxObject* obj,
                                  int rowOrCol, int x, int y,
                                  bool control, bool shift, bool alt, bool meta )
        : wxNotifyEvent( type, id ),
          wxKeyboardState(control, shift, alt, meta)
{
    Init(rowOrCol, x, y);

    SetEventObject(obj);
}


IMPLEMENT_DYNAMIC_CLASS( wxGridRangeSelectEvent, wxNotifyEvent )

wxGridRangeSelectEvent::wxGridRangeSelectEvent(int id, wxEventType type, wxObject* obj,
                                               const wxGridCellCoords& topLeft,
                                               const wxGridCellCoords& bottomRight,
                                               bool sel, bool control,
                                               bool shift, bool alt, bool meta )
        : wxNotifyEvent( type, id ),
          wxKeyboardState(control, shift, alt, meta)
{
    Init(topLeft, bottomRight, sel);

    SetEventObject(obj);
}


IMPLEMENT_DYNAMIC_CLASS(wxGridEditorCreatedEvent, wxCommandEvent)

wxGridEditorCreatedEvent::wxGridEditorCreatedEvent(int id, wxEventType type,
                                                   wxObject* obj, int row,
                                                   int col, wxControl* ctrl)
    : wxCommandEvent(type, id)
{
    SetEventObject(obj);
    m_row = row;
    m_col = col;
    m_ctrl = ctrl;
}


// ----------------------------------------------------------------------------
// wxGridTypeRegistry
// ----------------------------------------------------------------------------

wxGridTypeRegistry::~wxGridTypeRegistry()
{
    size_t count = m_typeinfo.GetCount();
    for ( size_t i = 0; i < count; i++ )
        delete m_typeinfo[i];
}

void wxGridTypeRegistry::RegisterDataType(const wxString& typeName,
                                          wxGridCellRenderer* renderer,
                                          wxGridCellEditor* editor)
{
    wxGridDataTypeInfo* info = new wxGridDataTypeInfo(typeName, renderer, editor);

    // is it already registered?
    int loc = FindRegisteredDataType(typeName);
    if ( loc != wxNOT_FOUND )
    {
        delete m_typeinfo[loc];
        m_typeinfo[loc] = info;
    }
    else
    {
        m_typeinfo.Add(info);
    }
}

int wxGridTypeRegistry::FindRegisteredDataType(const wxString& typeName)
{
    size_t count = m_typeinfo.GetCount();
    for ( size_t i = 0; i < count; i++ )
    {
        if ( typeName == m_typeinfo[i]->m_typeName )
        {
            return i;
        }
    }

    return wxNOT_FOUND;
}

int wxGridTypeRegistry::FindDataType(const wxString& typeName)
{
    int index = FindRegisteredDataType(typeName);
    if ( index == wxNOT_FOUND )
    {
        // check whether this is one of the standard ones, in which case
        // register it "on the fly"
#if wxUSE_TEXTCTRL
        if ( typeName == wxGRID_VALUE_STRING )
        {
            RegisterDataType(wxGRID_VALUE_STRING,
                             new wxGridCellStringRenderer,
                             new wxGridCellTextEditor);
        }
        else
#endif // wxUSE_TEXTCTRL
#if wxUSE_CHECKBOX
        if ( typeName == wxGRID_VALUE_BOOL )
        {
            RegisterDataType(wxGRID_VALUE_BOOL,
                             new wxGridCellBoolRenderer,
                             new wxGridCellBoolEditor);
        }
        else
#endif // wxUSE_CHECKBOX
#if wxUSE_TEXTCTRL
        if ( typeName == wxGRID_VALUE_NUMBER )
        {
            RegisterDataType(wxGRID_VALUE_NUMBER,
                             new wxGridCellNumberRenderer,
                             new wxGridCellNumberEditor);
        }
        else if ( typeName == wxGRID_VALUE_FLOAT )
        {
            RegisterDataType(wxGRID_VALUE_FLOAT,
                             new wxGridCellFloatRenderer,
                             new wxGridCellFloatEditor);
        }
        else
#endif // wxUSE_TEXTCTRL
#if wxUSE_COMBOBOX
        if ( typeName == wxGRID_VALUE_CHOICE )
        {
            RegisterDataType(wxGRID_VALUE_CHOICE,
                             new wxGridCellStringRenderer,
                             new wxGridCellChoiceEditor);
        }
        else
#endif // wxUSE_COMBOBOX
        {
            return wxNOT_FOUND;
        }

        // we get here only if just added the entry for this type, so return
        // the last index
        index = m_typeinfo.GetCount() - 1;
    }

    return index;
}

int wxGridTypeRegistry::FindOrCloneDataType(const wxString& typeName)
{
    int index = FindDataType(typeName);
    if ( index == wxNOT_FOUND )
    {
        // the first part of the typename is the "real" type, anything after ':'
        // are the parameters for the renderer
        index = FindDataType(typeName.BeforeFirst(wxT(':')));
        if ( index == wxNOT_FOUND )
        {
            return wxNOT_FOUND;
        }

        wxGridCellRenderer *renderer = GetRenderer(index);
        wxGridCellRenderer *rendererOld = renderer;
        renderer = renderer->Clone();
        rendererOld->DecRef();

        wxGridCellEditor *editor = GetEditor(index);
        wxGridCellEditor *editorOld = editor;
        editor = editor->Clone();
        editorOld->DecRef();

        // do it even if there are no parameters to reset them to defaults
        wxString params = typeName.AfterFirst(wxT(':'));
        renderer->SetParameters(params);
        editor->SetParameters(params);

        // register the new typename
        RegisterDataType(typeName, renderer, editor);

        // we just registered it, it's the last one
        index = m_typeinfo.GetCount() - 1;
    }

    return index;
}

wxGridCellRenderer* wxGridTypeRegistry::GetRenderer(int index)
{
    wxGridCellRenderer* renderer = m_typeinfo[index]->m_renderer;
    if (renderer)
        renderer->IncRef();

    return renderer;
}

wxGridCellEditor* wxGridTypeRegistry::GetEditor(int index)
{
    wxGridCellEditor* editor = m_typeinfo[index]->m_editor;
    if (editor)
        editor->IncRef();

    return editor;
}

#endif // wxUSE_GRID
