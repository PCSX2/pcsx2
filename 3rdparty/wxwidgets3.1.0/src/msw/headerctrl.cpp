///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/headerctrl.cpp
// Purpose:     implementation of wxHeaderCtrl for wxMSW
// Author:      Vadim Zeitlin
// Created:     2008-12-01
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
    #include "wx/app.h"
    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/headerctrl.h"

#ifndef wxHAS_GENERIC_HEADERCTRL

#include "wx/imaglist.h"

#include "wx/msw/wrapcctl.h"
#include "wx/msw/private.h"

#ifndef HDM_SETBITMAPMARGIN
    #define HDM_SETBITMAPMARGIN 0x1234
#endif

#ifndef Header_SetBitmapMargin
    #define Header_SetBitmapMargin(hwnd, margin) \
            ::SendMessage((hwnd), HDM_SETBITMAPMARGIN, (WPARAM)(margin), 0)
#endif

// from src/msw/listctrl.cpp
extern int WXDLLIMPEXP_CORE wxMSWGetColumnClicked(NMHDR *nmhdr, POINT *ptClick);

// ============================================================================
// wxHeaderCtrl implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxHeaderCtrl construction/destruction
// ----------------------------------------------------------------------------

void wxHeaderCtrl::Init()
{
    m_numColumns = 0;
    m_imageList = NULL;
    m_scrollOffset = 0;
    m_colBeingDragged = -1;
}

bool wxHeaderCtrl::Create(wxWindow *parent,
                          wxWindowID id,
                          const wxPoint& pos,
                          const wxSize& size,
                          long style,
                          const wxString& name)
{
    // notice that we don't need InitCommonControlsEx(ICC_LISTVIEW_CLASSES)
    // here as we already call InitCommonControls() in wxApp initialization
    // code which covers this

    if ( !CreateControl(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;

    if ( !MSWCreateControl(WC_HEADER, wxT(""), pos, size) )
        return false;

    // special hack for margins when using comctl32.dll v6 or later: the
    // default margin is too big and results in label truncation when the
    // column width is just about right to show it together with the sort
    // indicator, so reduce it to a smaller value (in principle we could even
    // use 0 here but this starts to look ugly)
    if ( wxApp::GetComCtl32Version() >= 600 )
    {
        Header_SetBitmapMargin(GetHwnd(), ::GetSystemMetrics(SM_CXEDGE));
    }

    return true;
}

WXDWORD wxHeaderCtrl::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    WXDWORD msStyle = wxControl::MSWGetStyle(style, exstyle);

    if ( style & wxHD_ALLOW_REORDER )
        msStyle |= HDS_DRAGDROP;

    // the control looks nicer with these styles and there doesn't seem to be
    // any reason to not use them so we always do (as for HDS_HORZ it is 0
    // anyhow but include it for clarity)
    // NOTE: don't use however HDS_FLAT because it makes the control look
    //       non-native when running WinXP in classic mode
    msStyle |= HDS_HORZ | HDS_BUTTONS | HDS_FULLDRAG | HDS_HOTTRACK;

    return msStyle;
}

wxHeaderCtrl::~wxHeaderCtrl()
{
    delete m_imageList;
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl scrolling
// ----------------------------------------------------------------------------

void wxHeaderCtrl::DoSetSize(int x, int y,
                             int w, int h,
                             int sizeFlags)
{
    wxHeaderCtrlBase::DoSetSize(x + m_scrollOffset, y, w - m_scrollOffset, h,
                                sizeFlags);
}

void wxHeaderCtrl::DoScrollHorz(int dx)
{
    // as the native control doesn't support offsetting its contents, we use a
    // hack here to make it appear correctly when the parent is scrolled:
    // instead of scrolling or repainting we simply move the control window
    // itself: to be precise, offset it by the scroll increment to the left and
    // increment its width to still extend to the right boundary to compensate
    // for it (notice that dx is negative when scrolling to the right)
    m_scrollOffset += dx;

    wxHeaderCtrlBase::DoSetSize(GetPosition().x + dx, -1,
                                GetSize().x - dx, -1,
                                wxSIZE_USE_EXISTING);
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl geometry calculation
// ----------------------------------------------------------------------------

wxSize wxHeaderCtrl::DoGetBestSize() const
{
    RECT rc = wxGetClientRect(GetHwndOf(GetParent()));
    WINDOWPOS wpos;
    HDLAYOUT layout = { &rc, &wpos };
    if ( !Header_Layout(GetHwnd(), &layout) )
    {
        wxLogLastError(wxT("Header_Layout"));
        return wxControl::DoGetBestSize();
    }

    return wxSize(wpos.cx, wpos.cy);
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl columns managements
// ----------------------------------------------------------------------------

unsigned int wxHeaderCtrl::DoGetCount() const
{
    // we can't use Header_GetItemCount() here because it doesn't take the
    // hidden columns into account and we can't find the hidden columns after
    // the last shown one in MSWFromNativeIdx() without knowing where to stop
    // so we have to store the columns count internally
    return m_numColumns;
}

int wxHeaderCtrl::GetShownColumnsCount() const
{
    const int numItems = Header_GetItemCount(GetHwnd());

    wxASSERT_MSG( numItems >= 0 && (unsigned)numItems <= m_numColumns,
                  "unexpected number of items in the native control" );

    return numItems;
}

void wxHeaderCtrl::DoSetCount(unsigned int count)
{
    unsigned n;

    // first delete all old columns
    const unsigned countOld = GetShownColumnsCount();
    for ( n = 0; n < countOld; n++ )
    {
        if ( !Header_DeleteItem(GetHwnd(), 0) )
        {
            wxLogLastError(wxT("Header_DeleteItem"));
        }
    }

    // update the column indices order array before changing m_numColumns
    DoResizeColumnIndices(m_colIndices, count);

    // and add the new ones
    m_numColumns = count;
    m_isHidden.resize(m_numColumns);
    for ( n = 0; n < count; n++ )
    {
        const wxHeaderColumn& col = GetColumn(n);
        if ( col.IsShown() )
        {
            m_isHidden[n] = false;

            DoInsertItem(col, n);
        }
        else // hidden initially
        {
            m_isHidden[n] = true;
        }
    }
}

void wxHeaderCtrl::DoUpdate(unsigned int idx)
{
    // the native control does provide Header_SetItem() but it's inconvenient
    // to use it because it sends HDN_ITEMCHANGING messages and we'd have to
    // arrange not to block setting the width from there and the logic would be
    // more complicated as we'd have to reset the old values as well as setting
    // the new ones -- so instead just recreate the column

    const wxHeaderColumn& col = GetColumn(idx);
    if ( col.IsHidden() )
    {
        // column is hidden now
        if ( !m_isHidden[idx] )
        {
            // but it wasn't hidden before, so remove it
            Header_DeleteItem(GetHwnd(), MSWToNativeIdx(idx));

            m_isHidden[idx] = true;
        }
        //else: nothing to do, updating hidden column doesn't have any effect
    }
    else // column is shown now
    {
        if ( m_isHidden[idx] )
        {
            m_isHidden[idx] = false;
        }
        else // and it was shown before as well
        {
            // we need to remove the old column
            Header_DeleteItem(GetHwnd(), MSWToNativeIdx(idx));
        }

        DoInsertItem(col, idx);
    }
}

void wxHeaderCtrl::DoInsertItem(const wxHeaderColumn& col, unsigned int idx)
{
    wxASSERT_MSG( !col.IsHidden(), "should only be called for shown columns" );

    wxHDITEM hdi;

    // notice that we need to store the string we use the pointer to until we
    // pass it to the control
    hdi.mask |= HDI_TEXT;
    wxWxCharBuffer buf = col.GetTitle().t_str();
    hdi.pszText = buf.data();
    hdi.cchTextMax = wxStrlen(buf);

    const wxBitmap bmp = col.GetBitmap();
    if ( bmp.IsOk() )
    {
        hdi.mask |= HDI_IMAGE;

        if ( bmp.IsOk() )
        {
            const int bmpWidth = bmp.GetWidth(),
                      bmpHeight = bmp.GetHeight();

            if ( !m_imageList )
            {
                m_imageList = new wxImageList(bmpWidth, bmpHeight);
                (void) // suppress mingw32 warning about unused computed value
                Header_SetImageList(GetHwnd(), GetHimagelistOf(m_imageList));
            }
            else // already have an image list
            {
                // check that all bitmaps we use have the same size
                int imageWidth,
                    imageHeight;
                m_imageList->GetSize(0, imageWidth, imageHeight);

                wxASSERT_MSG( imageWidth == bmpWidth && imageHeight == bmpHeight,
                              "all column bitmaps must have the same size" );
            }

            m_imageList->Add(bmp);
            hdi.iImage = m_imageList->GetImageCount() - 1;
        }
        else // no bitmap but we still need to update the item
        {
            hdi.iImage = I_IMAGENONE;
        }
    }

    if ( col.GetAlignment() != wxALIGN_NOT )
    {
        hdi.mask |= HDI_FORMAT | HDF_LEFT;
        switch ( col.GetAlignment() )
        {
            case wxALIGN_LEFT:
                hdi.fmt |= HDF_LEFT;
                break;

            case wxALIGN_CENTER:
            case wxALIGN_CENTER_HORIZONTAL:
                hdi.fmt |= HDF_CENTER;
                break;

            case wxALIGN_RIGHT:
                hdi.fmt |= HDF_RIGHT;
                break;

            default:
                wxFAIL_MSG( "invalid column header alignment" );
        }
    }

    if ( col.IsSortKey() )
    {
        hdi.mask |= HDI_FORMAT;
        hdi.fmt |= col.IsSortOrderAscending() ? HDF_SORTUP : HDF_SORTDOWN;
    }

    if ( col.GetWidth() != wxCOL_WIDTH_DEFAULT )
    {
        hdi.mask |= HDI_WIDTH;
        hdi.cxy = col.GetWidth();
    }

    hdi.mask |= HDI_ORDER;
    hdi.iOrder = MSWToNativeOrder(m_colIndices.Index(idx));

    if ( ::SendMessage(GetHwnd(), HDM_INSERTITEM,
                       MSWToNativeIdx(idx), (LPARAM)&hdi) == -1 )
    {
        wxLogLastError(wxT("Header_InsertItem()"));
    }
}

void wxHeaderCtrl::DoSetColumnsOrder(const wxArrayInt& order)
{
    wxArrayInt orderShown;
    orderShown.reserve(m_numColumns);

    for ( unsigned n = 0; n < m_numColumns; n++ )
    {
        const int idx = order[n];
        if ( GetColumn(idx).IsShown() )
            orderShown.push_back(MSWToNativeIdx(idx));
    }

    if ( !Header_SetOrderArray(GetHwnd(), orderShown.size(), &orderShown[0]) )
    {
        wxLogLastError(wxT("Header_GetOrderArray"));
    }

    m_colIndices = order;
}

wxArrayInt wxHeaderCtrl::DoGetColumnsOrder() const
{
    // we don't use Header_GetOrderArray() here because it doesn't return
    // information about the hidden columns, instead we just save the columns
    // order array in DoSetColumnsOrder() and update it when they're reordered
    return m_colIndices;
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl indexes and positions translation
// ----------------------------------------------------------------------------

int wxHeaderCtrl::MSWToNativeIdx(int idx)
{
    // don't check for GetColumn(idx).IsShown() as it could have just became
    // false and we may be called from DoUpdate() to delete the old column
    wxASSERT_MSG( !m_isHidden[idx],
                  "column must be visible to have an "
                  "index in the native control" );

    int item = idx;
    for ( int i = 0; i < idx; i++ )
    {
        if ( GetColumn(i).IsHidden() )
            item--; // one less column the native control knows about
    }

    wxASSERT_MSG( item >= 0 && item <= GetShownColumnsCount(), "logic error" );

    return item;
}

int wxHeaderCtrl::MSWFromNativeIdx(int item)
{
    wxASSERT_MSG( item >= 0 && item < GetShownColumnsCount(),
                  "column index out of range" );

    // reverse the above function

    unsigned idx = item;
    for ( unsigned n = 0; n < m_numColumns; n++ )
    {
        if ( n > idx )
            break;

        if ( GetColumn(n).IsHidden() )
            idx++;
    }

    wxASSERT_MSG( MSWToNativeIdx(idx) == item, "logic error" );

    return idx;
}

int wxHeaderCtrl::MSWToNativeOrder(int pos)
{
    wxASSERT_MSG( pos >= 0 && static_cast<unsigned>(pos) < m_numColumns,
                  "column position out of range" );

    int order = pos;
    for ( int n = 0; n < pos; n++ )
    {
        if ( GetColumn(m_colIndices[n]).IsHidden() )
            order--;
    }

    wxASSERT_MSG( order >= 0 && order <= GetShownColumnsCount(), "logic error" );

    return order;
}

int wxHeaderCtrl::MSWFromNativeOrder(int order)
{
    wxASSERT_MSG( order >= 0 && order < GetShownColumnsCount(),
                  "native column position out of range" );

    unsigned pos = order;
    for ( unsigned n = 0; n < m_numColumns; n++ )
    {
        if ( n > pos )
            break;

        if ( GetColumn(m_colIndices[n]).IsHidden() )
            pos++;
    }

    wxASSERT_MSG( MSWToNativeOrder(pos) == order, "logic error" );

    return pos;
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl events
// ----------------------------------------------------------------------------

wxEventType wxHeaderCtrl::GetClickEventType(bool dblclk, int button)
{
    wxEventType evtType;
    switch ( button )
    {
        case 0:
            evtType = dblclk ? wxEVT_HEADER_DCLICK
                             : wxEVT_HEADER_CLICK;
            break;

        case 1:
            evtType = dblclk ? wxEVT_HEADER_RIGHT_DCLICK
                             : wxEVT_HEADER_RIGHT_CLICK;
            break;

        case 2:
            evtType = dblclk ? wxEVT_HEADER_MIDDLE_DCLICK
                             : wxEVT_HEADER_MIDDLE_CLICK;
            break;

        default:
            wxFAIL_MSG( wxS("unexpected event type") );
            evtType = wxEVT_NULL;
    }

    return evtType;
}

bool wxHeaderCtrl::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result)
{
    NMHEADER * const nmhdr = (NMHEADER *)lParam;

    wxEventType evtType = wxEVT_NULL;
    int width = 0;
    int order = -1;
    bool veto = false;
    const UINT code = nmhdr->hdr.code;

    // we don't have the index for all events, e.g. not for NM_RELEASEDCAPTURE
    // so only access for header control events (and yes, the direction of
    // comparisons with FIRST/LAST is correct even if it seems inverted)
    int idx = code <= HDN_FIRST && code > HDN_LAST ? nmhdr->iItem : -1;
    if ( idx != -1 )
    {
        // we also get bogus HDN_BEGINDRAG with -1 index so don't call
        // MSWFromNativeIdx() unconditionally for nmhdr->iItem
        idx = MSWFromNativeIdx(idx);
    }

    switch ( code )
    {
        // click events
        // ------------

        case HDN_ITEMCLICK:
        case HDN_ITEMDBLCLICK:
            evtType = GetClickEventType(code == HDN_ITEMDBLCLICK, nmhdr->iButton);

            // We're not dragging any more.
            m_colBeingDragged = -1;
            break;

            // although we should get the notifications about the right clicks
            // via HDN_ITEM[DBL]CLICK too according to MSDN this simply doesn't
            // happen in practice on any Windows system up to 2003
        case NM_RCLICK:
        case NM_RDBLCLK:
            {
                POINT pt;
                idx = wxMSWGetColumnClicked(&nmhdr->hdr, &pt);
                if ( idx != wxNOT_FOUND )
                {
                    idx = MSWFromNativeIdx(idx);

                    // due to a bug in mingw32 headers NM_RDBLCLK is signed
                    // there so we need a cast to avoid warnings about signed/
                    // unsigned comparison
                    evtType = GetClickEventType(
                                code == static_cast<UINT>(NM_RDBLCLK), 1);
                }
                //else: ignore clicks outside any column
            }
            break;

        case HDN_DIVIDERDBLCLICK:
            evtType = wxEVT_HEADER_SEPARATOR_DCLICK;
            break;


        // column resizing events
        // ----------------------

        // see comments in wxListCtrl::MSWOnNotify() for why we catch both
        // ASCII and Unicode versions of this message
        case HDN_BEGINTRACKA:
        case HDN_BEGINTRACKW:
            // non-resizable columns can't be resized no matter what, don't
            // even generate any events for them
            if ( !GetColumn(idx).IsResizeable() )
            {
                veto = true;
                break;
            }

            evtType = wxEVT_HEADER_BEGIN_RESIZE;
            // fall through

        case HDN_ENDTRACKA:
        case HDN_ENDTRACKW:
            width = nmhdr->pitem->cxy;

            if ( evtType == wxEVT_NULL )
            {
                evtType = wxEVT_HEADER_END_RESIZE;

                // don't generate events with invalid width
                const int minWidth = GetColumn(idx).GetMinWidth();
                if ( width < minWidth )
                    width = minWidth;
            }
            break;

            // The control is not supposed to send HDN_TRACK when using
            // HDS_FULLDRAG (which we do use) but apparently some versions of
            // comctl32.dll still do it, see #13506, so catch both messages
            // just in case we are dealing with one of these buggy versions.
        case HDN_TRACK:
        case HDN_ITEMCHANGING:
            if ( nmhdr->pitem && (nmhdr->pitem->mask & HDI_WIDTH) )
            {
                // prevent the column from being shrunk beneath its min width
                width = nmhdr->pitem->cxy;
                if ( width < GetColumn(idx).GetMinWidth() )
                {
                    // don't generate any events and prevent the change from
                    // happening
                    veto = true;
                }
                else // width is acceptable
                {
                    // generate the resizing event from here as we don't seem
                    // to be getting HDN_TRACK events at all, at least with
                    // comctl32.dll v6
                    evtType = wxEVT_HEADER_RESIZING;
                }
            }
            break;


        // column reordering events
        // ------------------------

        case HDN_BEGINDRAG:
            // Windows sometimes sends us events with invalid column indices
            if ( nmhdr->iItem == -1 )
                break;

            // If we are dragging a column that is not draggable and the mouse
            // is moved over a different column then we get the column number from
            // the column under the mouse. This results in an unexpected behaviour
            // if this column is draggable. To prevent this remember the column we
            // are dragging for the complete drag and drop cycle.
            if ( m_colBeingDragged == -1 )
            {
                m_colBeingDragged = idx;
            }

            // column must have the appropriate flag to be draggable
            if ( !GetColumn(m_colBeingDragged).IsReorderable() )
            {
                veto = true;
                break;
            }

            evtType = wxEVT_HEADER_BEGIN_REORDER;
            break;

        case HDN_ENDDRAG:
            wxASSERT_MSG( nmhdr->pitem->mask & HDI_ORDER, "should have order" );
            order = nmhdr->pitem->iOrder;

            // we also get messages with invalid order when column reordering
            // is cancelled (e.g. by pressing Esc)
            if ( order == -1 )
                break;

            order = MSWFromNativeOrder(order);

            evtType = wxEVT_HEADER_END_REORDER;

            // We (successfully) ended dragging the column.
            m_colBeingDragged = -1;
            break;

        case NM_RELEASEDCAPTURE:
            evtType = wxEVT_HEADER_DRAGGING_CANCELLED;

            // Dragging the column was cancelled.
            m_colBeingDragged = -1;
            break;
    }


    // do generate the corresponding wx event
    if ( evtType != wxEVT_NULL )
    {
        wxHeaderCtrlEvent event(evtType, GetId());
        event.SetEventObject(this);
        event.SetColumn(idx);
        event.SetWidth(width);
        if ( order != -1 )
            event.SetNewOrder(order);

        const bool processed = GetEventHandler()->ProcessEvent(event);

        if ( processed && !event.IsAllowed() )
            veto = true;

        if ( !veto )
        {
            // special post-processing for HDN_ENDDRAG: we need to update the
            // internal column indices array if this is allowed to go ahead as
            // the native control is going to reorder its columns now
            if ( evtType == wxEVT_HEADER_END_REORDER )
                MoveColumnInOrderArray(m_colIndices, idx, order);

            if ( processed )
            {
                // skip default processing below
                return true;
            }
        }
    }

    if ( veto )
    {
        // all of HDN_BEGIN{DRAG,TRACK}, HDN_TRACK and HDN_ITEMCHANGING
        // interpret TRUE return value as meaning to stop the control
        // default handling of the message
        *result = TRUE;

        return true;
    }

    return wxHeaderCtrlBase::MSWOnNotify(idCtrl, lParam, result);
}

#endif // wxHAS_GENERIC_HEADERCTRL

#endif // wxUSE_HEADERCTRL
