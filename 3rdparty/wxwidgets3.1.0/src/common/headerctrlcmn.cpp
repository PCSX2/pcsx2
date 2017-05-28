///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/headerctrlcmn.cpp
// Purpose:     implementation of wxHeaderCtrlBase
// Author:      Vadim Zeitlin
// Created:     2008-12-02
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_HEADERCTRL

#ifndef WX_PRECOMP
    #include "wx/menu.h"
#endif // WX_PRECOMP

#include "wx/headerctrl.h"
#include "wx/rearrangectrl.h"
#include "wx/renderer.h"

namespace
{

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

const unsigned int wxNO_COLUMN = static_cast<unsigned>(-1);

// ----------------------------------------------------------------------------
// wxHeaderColumnsRearrangeDialog: dialog for customizing our columns
// ----------------------------------------------------------------------------

#if wxUSE_REARRANGECTRL

class wxHeaderColumnsRearrangeDialog : public wxRearrangeDialog
{
public:
    wxHeaderColumnsRearrangeDialog(wxWindow *parent,
                                   const wxArrayInt& order,
                                   const wxArrayString& items)
        : wxRearrangeDialog
          (
            parent,
            _("Please select the columns to show and define their order:"),
            _("Customize Columns"),
            order,
            items
          )
    {
    }
};

#endif // wxUSE_REARRANGECTRL

} // anonymous namespace

// ============================================================================
// wxHeaderCtrlBase implementation
// ============================================================================

extern WXDLLIMPEXP_DATA_CORE(const char) wxHeaderCtrlNameStr[] = "wxHeaderCtrl";

wxBEGIN_EVENT_TABLE(wxHeaderCtrlBase, wxControl)
    EVT_HEADER_SEPARATOR_DCLICK(wxID_ANY, wxHeaderCtrlBase::OnSeparatorDClick)
#if wxUSE_MENUS
    EVT_HEADER_RIGHT_CLICK(wxID_ANY, wxHeaderCtrlBase::OnRClick)
#endif // wxUSE_MENUS
wxEND_EVENT_TABLE()

void wxHeaderCtrlBase::ScrollWindow(int dx,
                                    int WXUNUSED_UNLESS_DEBUG(dy),
                                    const wxRect * WXUNUSED_UNLESS_DEBUG(rect))

{
    // this doesn't make sense at all
    wxASSERT_MSG( !dy, "header window can't be scrolled vertically" );

    // this would actually be nice to support for "frozen" headers but it isn't
    // supported currently
    wxASSERT_MSG( !rect, "header window can't be scrolled partially" );

    DoScrollHorz(dx);
}

void wxHeaderCtrlBase::SetColumnCount(unsigned int count)
{
    if ( count != GetColumnCount() )
        OnColumnCountChanging(count);

    // still call DoSetCount() even if the count didn't really change in order
    // to update all the columns
    DoSetCount(count);
}

int wxHeaderCtrlBase::GetColumnTitleWidth(const wxHeaderColumn& col)
{
    int w = wxWindowBase::GetTextExtent(col.GetTitle()).x;

    // add some margin:
    w += wxRendererNative::Get().GetHeaderButtonMargin(this);

    // if a bitmap is used, add space for it and 2px border:
    wxBitmap bmp = col.GetBitmap();
    if ( bmp.IsOk() )
        w += bmp.GetWidth() + 2;

    return w;
}

// ----------------------------------------------------------------------------
// wxHeaderCtrlBase event handling
// ----------------------------------------------------------------------------

void wxHeaderCtrlBase::OnSeparatorDClick(wxHeaderCtrlEvent& event)
{
    const unsigned col = event.GetColumn();
    const wxHeaderColumn& column = GetColumn(col);

    if ( !column.IsResizeable() )
    {
        event.Skip();
        return;
    }

    int w = GetColumnTitleWidth(column);

    if ( !UpdateColumnWidthToFit(col, w) )
        event.Skip();
    else
        UpdateColumn(col);
}

#if wxUSE_MENUS

void wxHeaderCtrlBase::OnRClick(wxHeaderCtrlEvent& event)
{
    if ( !HasFlag(wxHD_ALLOW_HIDE) )
    {
        event.Skip();
        return;
    }

    ShowColumnsMenu(ScreenToClient(wxGetMousePosition()));
}

#endif // wxUSE_MENUS

// ----------------------------------------------------------------------------
// wxHeaderCtrlBase column reordering
// ----------------------------------------------------------------------------

void wxHeaderCtrlBase::SetColumnsOrder(const wxArrayInt& order)
{
    const unsigned count = GetColumnCount();
    wxCHECK_RET( order.size() == count, "wrong number of columns" );

    // check the array validity
    wxArrayInt seen(count, 0);
    for ( unsigned n = 0; n < count; n++ )
    {
        const unsigned idx = order[n];
        wxCHECK_RET( idx < count, "invalid column index" );
        wxCHECK_RET( !seen[idx], "duplicate column index" );

        seen[idx] = 1;
    }

    DoSetColumnsOrder(order);

    // TODO-RTL: do we need to reverse the array?
}

void wxHeaderCtrlBase::ResetColumnsOrder()
{
    const unsigned count = GetColumnCount();
    wxArrayInt order(count);
    for ( unsigned n = 0; n < count; n++ )
        order[n] = n;

    DoSetColumnsOrder(order);
}

wxArrayInt wxHeaderCtrlBase::GetColumnsOrder() const
{
    const wxArrayInt order = DoGetColumnsOrder();

    wxASSERT_MSG( order.size() == GetColumnCount(), "invalid order array" );

    return order;
}

unsigned int wxHeaderCtrlBase::GetColumnAt(unsigned int pos) const
{
    wxCHECK_MSG( pos < GetColumnCount(), wxNO_COLUMN, "invalid position" );

    return GetColumnsOrder()[pos];
}

unsigned int wxHeaderCtrlBase::GetColumnPos(unsigned int idx) const
{
    const unsigned count = GetColumnCount();

    wxCHECK_MSG( idx < count, wxNO_COLUMN, "invalid index" );

    const wxArrayInt order = GetColumnsOrder();
    int pos = order.Index(idx);
    wxCHECK_MSG( pos != wxNOT_FOUND, wxNO_COLUMN, "column unexpectedly not displayed at all" );

    return (unsigned int)pos;
}

/* static */
void wxHeaderCtrlBase::MoveColumnInOrderArray(wxArrayInt& order,
                                              unsigned int idx,
                                              unsigned int pos)
{
    int posOld = order.Index(idx);
    wxASSERT_MSG( posOld != wxNOT_FOUND, "invalid index" );

    if ( pos != (unsigned int)posOld )
    {
        order.RemoveAt(posOld);
        order.Insert(idx, pos);
    }
}

void
wxHeaderCtrlBase::DoResizeColumnIndices(wxArrayInt& colIndices, unsigned int count)
{
    // update the column indices array if necessary
    const unsigned countOld = colIndices.size();
    if ( count > countOld )
    {
        // all new columns have default positions equal to their indices
        for ( unsigned n = countOld; n < count; n++ )
            colIndices.push_back(n);
    }
    else if ( count < countOld )
    {
        // filter out all the positions which are invalid now while keeping the
        // order of the remaining ones
        wxArrayInt colIndicesNew;
        colIndicesNew.reserve(count);
        for ( unsigned n = 0; n < countOld; n++ )
        {
            const unsigned idx = colIndices[n];
            if ( idx < count )
                colIndicesNew.push_back(idx);
        }

        colIndices.swap(colIndicesNew);
    }
    //else: count didn't really change, nothing to do

    wxASSERT_MSG( colIndices.size() == count, "logic error" );
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl extra UI
// ----------------------------------------------------------------------------

#if wxUSE_MENUS

void wxHeaderCtrlBase::AddColumnsItems(wxMenu& menu, int idColumnsBase)
{
    const unsigned count = GetColumnCount();
    for ( unsigned n = 0; n < count; n++ )
    {
        const wxHeaderColumn& col = GetColumn(n);
        menu.AppendCheckItem(idColumnsBase + n, col.GetTitle());
        if ( col.IsShown() )
            menu.Check(n, true);
    }
}

bool wxHeaderCtrlBase::ShowColumnsMenu(const wxPoint& pt, const wxString& title)
{
    // construct the menu with the entries for all columns
    wxMenu menu;
    if ( !title.empty() )
        menu.SetTitle(title);

    AddColumnsItems(menu);

    // ... and an extra one to show the customization dialog if the user is
    // allowed to reorder the columns too
    const unsigned count = GetColumnCount();
    if ( HasFlag(wxHD_ALLOW_REORDER) )
    {
        menu.AppendSeparator();
        menu.Append(count, _("&Customize..."));
    }

    // do show the menu and get the user selection
    const int rc = GetPopupMenuSelectionFromUser(menu, pt);
    if ( rc == wxID_NONE )
        return false;

    if ( static_cast<unsigned>(rc) == count )
    {
        return ShowCustomizeDialog();
    }
    else // a column selected from the menu
    {
        UpdateColumnVisibility(rc, !GetColumn(rc).IsShown());
    }

    return true;
}

#endif // wxUSE_MENUS

bool wxHeaderCtrlBase::ShowCustomizeDialog()
{
#if wxUSE_REARRANGECTRL
    // prepare the data for showing the dialog
    wxArrayInt order = GetColumnsOrder();

    const unsigned count = GetColumnCount();

    // notice that titles are always in the index order, they will be shown
    // rearranged according to the display order in the dialog
    wxArrayString titles;
    titles.reserve(count);
    for ( unsigned n = 0; n < count; n++ )
        titles.push_back(GetColumn(n).GetTitle());

    // this loop is however over positions and not indices
    unsigned pos;
    for ( pos = 0; pos < count; pos++ )
    {
        int& idx = order[pos];
        if ( GetColumn(idx).IsHidden() )
        {
            // indicate that this one is hidden
            idx = ~idx;
        }
    }

    // do show it
    wxHeaderColumnsRearrangeDialog dlg(this, order, titles);
    if ( dlg.ShowModal() == wxID_OK )
    {
        // and apply the changes
        order = dlg.GetOrder();
        for ( pos = 0; pos < count; pos++ )
        {
            int& idx = order[pos];
            const bool show = idx >= 0;
            if ( !show )
            {
                // make all indices positive for passing them to SetColumnsOrder()
                idx = ~idx;
            }

            if ( show != GetColumn(idx).IsShown() )
                UpdateColumnVisibility(idx, show);
        }

        UpdateColumnsOrder(order);
        SetColumnsOrder(order);

        return true;
    }
#endif // wxUSE_REARRANGECTRL

    return false;
}

// ============================================================================
// wxHeaderCtrlSimple implementation
// ============================================================================

void wxHeaderCtrlSimple::Init()
{
    m_sortKey = wxNO_COLUMN;
}

const wxHeaderColumn& wxHeaderCtrlSimple::GetColumn(unsigned int idx) const
{
    return m_cols[idx];
}

void wxHeaderCtrlSimple::DoInsert(const wxHeaderColumnSimple& col, unsigned int idx)
{
    m_cols.insert(m_cols.begin() + idx, col);

    UpdateColumnCount();
}

void wxHeaderCtrlSimple::DoDelete(unsigned int idx)
{
    m_cols.erase(m_cols.begin() + idx);
    if ( idx == m_sortKey )
        m_sortKey = wxNO_COLUMN;

    UpdateColumnCount();
}

void wxHeaderCtrlSimple::DeleteAllColumns()
{
    m_cols.clear();
    m_sortKey = wxNO_COLUMN;

    UpdateColumnCount();
}


void wxHeaderCtrlSimple::DoShowColumn(unsigned int idx, bool show)
{
    if ( show != m_cols[idx].IsShown() )
    {
        m_cols[idx].SetHidden(!show);

        UpdateColumn(idx);
    }
}

void wxHeaderCtrlSimple::DoShowSortIndicator(unsigned int idx, bool ascending)
{
    RemoveSortIndicator();

    m_cols[idx].SetSortOrder(ascending);
    m_sortKey = idx;

    UpdateColumn(idx);
}

void wxHeaderCtrlSimple::RemoveSortIndicator()
{
    if ( m_sortKey != wxNO_COLUMN )
    {
        const unsigned sortOld = m_sortKey;
        m_sortKey = wxNO_COLUMN;

        m_cols[sortOld].UnsetAsSortKey();

        UpdateColumn(sortOld);
    }
}

bool
wxHeaderCtrlSimple::UpdateColumnWidthToFit(unsigned int idx, int widthTitle)
{
    const int widthContents = GetBestFittingWidth(idx);
    if ( widthContents == -1 )
        return false;

    m_cols[idx].SetWidth(wxMax(widthContents, widthTitle));

    return true;
}

// ============================================================================
// wxHeaderCtrlEvent implementation
// ============================================================================

wxIMPLEMENT_DYNAMIC_CLASS(wxHeaderCtrlEvent, wxNotifyEvent);

wxDEFINE_EVENT( wxEVT_HEADER_CLICK, wxHeaderCtrlEvent);
wxDEFINE_EVENT( wxEVT_HEADER_RIGHT_CLICK, wxHeaderCtrlEvent);
wxDEFINE_EVENT( wxEVT_HEADER_MIDDLE_CLICK, wxHeaderCtrlEvent);

wxDEFINE_EVENT( wxEVT_HEADER_DCLICK, wxHeaderCtrlEvent);
wxDEFINE_EVENT( wxEVT_HEADER_RIGHT_DCLICK, wxHeaderCtrlEvent);
wxDEFINE_EVENT( wxEVT_HEADER_MIDDLE_DCLICK, wxHeaderCtrlEvent);

wxDEFINE_EVENT( wxEVT_HEADER_SEPARATOR_DCLICK, wxHeaderCtrlEvent);

wxDEFINE_EVENT( wxEVT_HEADER_BEGIN_RESIZE, wxHeaderCtrlEvent);
wxDEFINE_EVENT( wxEVT_HEADER_RESIZING, wxHeaderCtrlEvent);
wxDEFINE_EVENT( wxEVT_HEADER_END_RESIZE, wxHeaderCtrlEvent);

wxDEFINE_EVENT( wxEVT_HEADER_BEGIN_REORDER, wxHeaderCtrlEvent);
wxDEFINE_EVENT( wxEVT_HEADER_END_REORDER, wxHeaderCtrlEvent);

wxDEFINE_EVENT( wxEVT_HEADER_DRAGGING_CANCELLED, wxHeaderCtrlEvent);

#endif // wxUSE_HEADERCTRL
