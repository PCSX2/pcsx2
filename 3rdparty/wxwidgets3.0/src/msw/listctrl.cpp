/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/listctrl.cpp
// Purpose:     wxListCtrl
// Author:      Julian Smart
// Modified by: Agron Selimaj
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_LISTCTRL

#include "wx/listctrl.h"

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/app.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/settings.h"
    #include "wx/stopwatch.h"
    #include "wx/dcclient.h"
    #include "wx/textctrl.h"
#endif

#include "wx/imaglist.h"
#include "wx/vector.h"

#include "wx/msw/private.h"
#include "wx/msw/private/keyboard.h"

#if defined(__WXWINCE__) && !defined(__HANDHELDPC__)
  #include <ole2.h>
  #include <shellapi.h>
  #if _WIN32_WCE < 400
    #include <aygshell.h>
  #endif
#endif

// Currently gcc and watcom don't define NMLVFINDITEM, and DMC only defines
// it by its old name NM_FINDTIEM.
//
#if defined(__VISUALC__) || defined(__BORLANDC__) || defined(NMLVFINDITEM)
    #define HAVE_NMLVFINDITEM 1
#elif defined(__DMC__) || defined(NM_FINDITEM)
    #define HAVE_NMLVFINDITEM 1
    #define NMLVFINDITEM NM_FINDITEM
#endif

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// convert our state and mask flags to LV_ITEM constants
static void wxConvertToMSWFlags(long state, long mask, LV_ITEM& lvItem);

// convert wxListItem to LV_ITEM
static void wxConvertToMSWListItem(const wxListCtrl *ctrl,
                                   const wxListItem& info, LV_ITEM& lvItem);

// convert LV_ITEM to wxListItem
static void wxConvertFromMSWListItem(HWND hwndListCtrl,
                                     wxListItem& info,
                                     /* const */ LV_ITEM& lvItem);

// convert our wxListItem to LV_COLUMN
static void wxConvertToMSWListCol(HWND hwndList,
                                  int col,
                                  const wxListItem& item,
                                  LV_COLUMN& lvCol);

namespace
{

// replacement for ListView_GetSubItemRect() which provokes warnings like
// "the address of 'rc' will always evaluate as 'true'" when used with mingw32
// 4.3+
//
// this function does no error checking on item and subitem parameters, notice
// that subitem 0 means the whole item so there is no way to retrieve the
// rectangle of the first subitem using this function, in particular notice
// that the index is *not* 1-based, in spite of what MSDN says
inline bool
wxGetListCtrlSubItemRect(HWND hwnd, int item, int subitem, int flags, RECT& rect)
{
    rect.top = subitem;
    rect.left = flags;
    return ::SendMessage(hwnd, LVM_GETSUBITEMRECT, item, (LPARAM)&rect) != 0;
}

inline bool
wxGetListCtrlItemRect(HWND hwnd, int item, int flags, RECT& rect)
{
    return wxGetListCtrlSubItemRect(hwnd, item, 0, flags, rect);
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// private helper classes
// ----------------------------------------------------------------------------

// We have to handle both fooW and fooA notifications in several cases
// because of broken comctl32.dll and/or unicows.dll. This class is used to
// convert LV_ITEMA and LV_ITEMW to LV_ITEM (which is either LV_ITEMA or
// LV_ITEMW depending on wxUSE_UNICODE setting), so that it can be processed
// by wxConvertToMSWListItem().
#if wxUSE_UNICODE
    #define LV_ITEM_NATIVE  LV_ITEMW
    #define LV_ITEM_OTHER   LV_ITEMA

    #define LV_CONV_TO_WX   cMB2WX
    #define LV_CONV_BUF     wxMB2WXbuf
#else // ANSI
    #define LV_ITEM_NATIVE  LV_ITEMA
    #define LV_ITEM_OTHER   LV_ITEMW

    #define LV_CONV_TO_WX   cWC2WX
    #define LV_CONV_BUF     wxWC2WXbuf
#endif // Unicode/ANSI

class wxLV_ITEM
{
public:
    // default ctor, use Init() later
    wxLV_ITEM() { m_buf = NULL; m_pItem = NULL; }

    // init without conversion
    void Init(LV_ITEM_NATIVE& item)
    {
        wxASSERT_MSG( !m_pItem, wxT("Init() called twice?") );

        m_pItem = &item;
    }

    // init with conversion
    void Init(const LV_ITEM_OTHER& item)
    {
        // avoid unnecessary dynamic memory allocation, jjust make m_pItem
        // point to our own m_item

        // memcpy() can't work if the struct sizes are different
        wxCOMPILE_TIME_ASSERT( sizeof(LV_ITEM_OTHER) == sizeof(LV_ITEM_NATIVE),
                               CodeCantWorkIfDiffSizes);

        memcpy(&m_item, &item, sizeof(LV_ITEM_NATIVE));

        // convert text from ANSI to Unicod if necessary
        if ( (item.mask & LVIF_TEXT) && item.pszText )
        {
            m_buf = new LV_CONV_BUF(wxConvLocal.LV_CONV_TO_WX(item.pszText));
            m_item.pszText = (wxChar *)m_buf->data();
        }
    }

    // ctor without conversion
    wxLV_ITEM(LV_ITEM_NATIVE& item) : m_buf(NULL), m_pItem(&item) { }

    // ctor with conversion
    wxLV_ITEM(LV_ITEM_OTHER& item) : m_buf(NULL)
    {
        Init(item);
    }

    ~wxLV_ITEM() { delete m_buf; }

    // conversion to the real LV_ITEM
    operator LV_ITEM_NATIVE&() const { return *m_pItem; }

private:
    LV_CONV_BUF *m_buf;

    LV_ITEM_NATIVE *m_pItem;
    LV_ITEM_NATIVE m_item;

    wxDECLARE_NO_COPY_CLASS(wxLV_ITEM);
};

///////////////////////////////////////////////////////
// Problem:
// The MSW version had problems with SetTextColour() et
// al as the wxListItemAttr's were stored keyed on the
// item index. If a item was inserted anywhere but the end
// of the list the text attributes (colour etc) for
// the following items were out of sync.
//
// Solution:
// Under MSW the only way to associate data with a List
// item independent of its position in the list is to
// store a pointer to it in its lParam attribute. However
// user programs are already using this (via the
// SetItemData() GetItemData() calls).
//
// However what we can do is store a pointer to a
// structure which contains the attributes we want *and*
// a lParam -- and this is what wxMSWListItemData does.
//
// To conserve memory, a wxMSWListItemData is
// only allocated for a LV_ITEM if text attributes or
// user data(lparam) are being set.
class wxMSWListItemData
{
public:
   wxMSWListItemData() : attr(NULL), lParam(0) {}
   ~wxMSWListItemData() { delete attr; }

    wxListItemAttr *attr;
    LPARAM lParam; // real user data

    wxDECLARE_NO_COPY_CLASS(wxMSWListItemData);
};

BEGIN_EVENT_TABLE(wxListCtrl, wxListCtrlBase)
    EVT_PAINT(wxListCtrl::OnPaint)
    EVT_CHAR_HOOK(wxListCtrl::OnCharHook)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxListCtrl construction
// ----------------------------------------------------------------------------

void wxListCtrl::Init()
{
    m_imageListNormal =
    m_imageListSmall =
    m_imageListState = NULL;
    m_ownsImageListNormal =
    m_ownsImageListSmall =
    m_ownsImageListState = false;

    m_colCount = 0;
    m_count = 0;
    m_textCtrl = NULL;

    m_hasAnyAttr = false;
}

bool wxListCtrl::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxValidator& validator,
                        const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    if ( !MSWCreateControl(WC_LISTVIEW, wxEmptyString, pos, size) )
        return false;

    // explicitly say that we want to use Unicode because otherwise we get ANSI
    // versions of _some_ messages (notably LVN_GETDISPINFOA) in MSLU build
    wxSetCCUnicodeFormat(GetHwnd());

    // We must set the default text colour to the system/theme color, otherwise
    // GetTextColour will always return black
    SetTextColour(GetDefaultAttributes().colFg);

    if ( InReportView() )
        MSWSetExListStyles();

    return true;
}

void wxListCtrl::MSWSetExListStyles()
{
    // for comctl32.dll v 4.70+ we want to have some non default extended
    // styles because it's prettier (and also because wxGTK does it like this)
    if ( wxApp::GetComCtl32Version() >= 470 )
    {
        ::SendMessage
        (
            GetHwnd(), LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
            // LVS_EX_LABELTIP shouldn't be used under Windows CE where it's
            // not defined in the SDK headers
#ifdef LVS_EX_LABELTIP
            LVS_EX_LABELTIP |
#endif
            LVS_EX_FULLROWSELECT |
            LVS_EX_SUBITEMIMAGES |
            // normally this should be governed by a style as it's probably not
            // always appropriate, but we don't have any free styles left and
            // it seems better to enable it by default than disable
            LVS_EX_HEADERDRAGDROP
        );
    }
}

WXDWORD wxListCtrl::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    WXDWORD wstyle = wxListCtrlBase::MSWGetStyle(style, exstyle);

    wstyle |= LVS_SHAREIMAGELISTS | LVS_SHOWSELALWAYS;

#if wxDEBUG_LEVEL
    size_t nModes = 0;

    #define MAP_MODE_STYLE(wx, ms)                                            \
        if ( style & (wx) ) { wstyle |= (ms); nModes++; }
#else // !wxDEBUG_LEVEL
    #define MAP_MODE_STYLE(wx, ms)                                            \
        if ( style & (wx) ) wstyle |= (ms);
#endif // wxDEBUG_LEVEL/!wxDEBUG_LEVEL

    MAP_MODE_STYLE(wxLC_ICON, LVS_ICON)
    MAP_MODE_STYLE(wxLC_SMALL_ICON, LVS_SMALLICON)
    MAP_MODE_STYLE(wxLC_LIST, LVS_LIST)
    MAP_MODE_STYLE(wxLC_REPORT, LVS_REPORT)

    wxASSERT_MSG( nModes == 1,
                  wxT("wxListCtrl style should have exactly one mode bit set") );

#undef MAP_MODE_STYLE

    if ( style & wxLC_ALIGN_LEFT )
        wstyle |= LVS_ALIGNLEFT;

    if ( style & wxLC_ALIGN_TOP )
        wstyle |= LVS_ALIGNTOP;

    if ( style & wxLC_AUTOARRANGE )
        wstyle |= LVS_AUTOARRANGE;

    if ( style & wxLC_NO_SORT_HEADER )
        wstyle |= LVS_NOSORTHEADER;

    if ( style & wxLC_NO_HEADER )
        wstyle |= LVS_NOCOLUMNHEADER;

    if ( style & wxLC_EDIT_LABELS )
        wstyle |= LVS_EDITLABELS;

    if ( style & wxLC_SINGLE_SEL )
        wstyle |= LVS_SINGLESEL;

    if ( style & wxLC_SORT_ASCENDING )
    {
        wstyle |= LVS_SORTASCENDING;

        wxASSERT_MSG( !(style & wxLC_SORT_DESCENDING),
                      wxT("can't sort in ascending and descending orders at once") );
    }
    else if ( style & wxLC_SORT_DESCENDING )
        wstyle |= LVS_SORTDESCENDING;

#if !( defined(__GNUWIN32__) && !wxCHECK_W32API_VERSION( 1, 0 ) )
    if ( style & wxLC_VIRTUAL )
    {
        int ver = wxApp::GetComCtl32Version();
        if ( ver < 470 )
        {
            wxLogWarning(_("Please install a newer version of comctl32.dll\n(at least version 4.70 is required but you have %d.%02d)\nor this program won't operate correctly."),
                        ver / 100, ver % 100);
        }

        wstyle |= LVS_OWNERDATA;
    }
#endif // ancient cygwin

    return wstyle;
}

void wxListCtrl::UpdateStyle()
{
    if ( GetHwnd() )
    {
        // The new window view style
        DWORD dwStyleNew = MSWGetStyle(m_windowStyle, NULL);

        // some styles are not returned by MSWGetStyle()
        if ( IsShown() )
            dwStyleNew |= WS_VISIBLE;

        // Get the current window style.
        DWORD dwStyleOld = ::GetWindowLong(GetHwnd(), GWL_STYLE);

        // we don't have wxVSCROLL style, but the list control may have it,
        // don't change it then
        dwStyleNew |= dwStyleOld & (WS_HSCROLL | WS_VSCROLL);

        // Only set the window style if the view bits have changed.
        if ( dwStyleOld != dwStyleNew )
        {
            ::SetWindowLong(GetHwnd(), GWL_STYLE, dwStyleNew);

            // if we switched to the report view, set the extended styles for
            // it too
            if ( !(dwStyleOld & LVS_REPORT) && (dwStyleNew & LVS_REPORT) )
                MSWSetExListStyles();
        }
    }
}

void wxListCtrl::FreeAllInternalData()
{
    const unsigned count = m_internalData.size();
    for ( unsigned n = 0; n < count; n++ )
        delete m_internalData[n];

    m_internalData.clear();
}

void wxListCtrl::DeleteEditControl()
{
    if ( m_textCtrl )
    {
        m_textCtrl->UnsubclassWin();
        m_textCtrl->SetHWND(0);
        wxDELETE(m_textCtrl);
    }
}

wxListCtrl::~wxListCtrl()
{
    FreeAllInternalData();

    DeleteEditControl();

    if (m_ownsImageListNormal)
        delete m_imageListNormal;
    if (m_ownsImageListSmall)
        delete m_imageListSmall;
    if (m_ownsImageListState)
        delete m_imageListState;
}

// ----------------------------------------------------------------------------
// set/get/change style
// ----------------------------------------------------------------------------

// Add or remove a single window style
void wxListCtrl::SetSingleStyle(long style, bool add)
{
    long flag = GetWindowStyleFlag();

    // Get rid of conflicting styles
    if ( add )
    {
        if ( style & wxLC_MASK_TYPE)
            flag = flag & ~wxLC_MASK_TYPE;
        if ( style & wxLC_MASK_ALIGN )
            flag = flag & ~wxLC_MASK_ALIGN;
        if ( style & wxLC_MASK_SORT )
            flag = flag & ~wxLC_MASK_SORT;
    }

    if ( add )
        flag |= style;
    else
        flag &= ~style;

    SetWindowStyleFlag(flag);
}

// Set the whole window style
void wxListCtrl::SetWindowStyleFlag(long flag)
{
    if ( flag != m_windowStyle )
    {
        wxListCtrlBase::SetWindowStyleFlag(flag);

        UpdateStyle();

        Refresh();
    }
}

// ----------------------------------------------------------------------------
// accessors
// ----------------------------------------------------------------------------

/* static */ wxVisualAttributes
wxListCtrl::GetClassDefaultAttributes(wxWindowVariant variant)
{
    wxVisualAttributes attrs = GetCompositeControlsDefaultAttributes(variant);

    // common controls have their own default font
    attrs.font = wxGetCCDefaultFont();

    return attrs;
}

// Sets the foreground, i.e. text, colour
bool wxListCtrl::SetForegroundColour(const wxColour& col)
{
    if ( !wxWindow::SetForegroundColour(col) )
        return false;

    ListView_SetTextColor(GetHwnd(), wxColourToRGB(col));

    return true;
}

// Sets the background colour
bool wxListCtrl::SetBackgroundColour(const wxColour& col)
{
    if ( !wxWindow::SetBackgroundColour(col) )
        return false;

    // we set the same colour for both the "empty" background and the items
    // background
    COLORREF color = wxColourToRGB(col);
    ListView_SetBkColor(GetHwnd(), color);
    ListView_SetTextBkColor(GetHwnd(), color);

    return true;
}

// Gets information about this column
bool wxListCtrl::GetColumn(int col, wxListItem& item) const
{
    LV_COLUMN lvCol;
    wxZeroMemory(lvCol);

    lvCol.mask = LVCF_WIDTH;

    if ( item.m_mask & wxLIST_MASK_TEXT )
    {
        lvCol.mask |= LVCF_TEXT;
        lvCol.pszText = new wxChar[513];
        lvCol.cchTextMax = 512;
    }

    if ( item.m_mask & wxLIST_MASK_FORMAT )
    {
        lvCol.mask |= LVCF_FMT;
    }

    if ( item.m_mask & wxLIST_MASK_IMAGE )
    {
        lvCol.mask |= LVCF_IMAGE;
    }

    bool success = ListView_GetColumn(GetHwnd(), col, &lvCol) != 0;

    //  item.m_subItem = lvCol.iSubItem;
    item.m_width = lvCol.cx;

    if ( (item.m_mask & wxLIST_MASK_TEXT) && lvCol.pszText )
    {
        item.m_text = lvCol.pszText;
        delete[] lvCol.pszText;
    }

    if ( item.m_mask & wxLIST_MASK_FORMAT )
    {
        switch (lvCol.fmt & LVCFMT_JUSTIFYMASK) {
            case LVCFMT_LEFT:
                item.m_format = wxLIST_FORMAT_LEFT;
                break;
            case LVCFMT_RIGHT:
                item.m_format = wxLIST_FORMAT_RIGHT;
                break;
            case LVCFMT_CENTER:
                item.m_format = wxLIST_FORMAT_CENTRE;
                break;
            default:
                item.m_format = -1;  // Unknown?
                break;
        }
    }

    // the column images were not supported in older versions but how to check
    // for this? we can't use _WIN32_IE because we always define it to a very
    // high value, so see if another symbol which is only defined starting from
    // comctl32.dll 4.70 is available
#ifdef NM_CUSTOMDRAW // _WIN32_IE >= 0x0300
    if ( item.m_mask & wxLIST_MASK_IMAGE )
    {
        item.m_image = lvCol.iImage;
    }
#endif // LVCOLUMN::iImage exists

    return success;
}

// Sets information about this column
bool wxListCtrl::SetColumn(int col, const wxListItem& item)
{
    LV_COLUMN lvCol;
    wxConvertToMSWListCol(GetHwnd(), col, item, lvCol);

    return ListView_SetColumn(GetHwnd(), col, &lvCol) != 0;
}

// Gets the column width
int wxListCtrl::GetColumnWidth(int col) const
{
    return ListView_GetColumnWidth(GetHwnd(), col);
}

// Sets the column width
bool wxListCtrl::SetColumnWidth(int col, int width)
{
    if ( m_windowStyle & wxLC_LIST )
        col = 0;

    if ( width == wxLIST_AUTOSIZE)
        width = LVSCW_AUTOSIZE;
    else if ( width == wxLIST_AUTOSIZE_USEHEADER)
        width = LVSCW_AUTOSIZE_USEHEADER;

    if ( !ListView_SetColumnWidth(GetHwnd(), col, width) )
        return false;

    // Failure to explicitly refresh the control with horizontal rules results
    // in corrupted rules display.
    if ( HasFlag(wxLC_HRULES) )
        Refresh();

    return true;
}

// ----------------------------------------------------------------------------
// columns order
// ----------------------------------------------------------------------------

int wxListCtrl::GetColumnIndexFromOrder(int order) const
{
    const int numCols = GetColumnCount();
    wxCHECK_MSG( order >= 0 && order < numCols, -1,
                wxT("Column position out of bounds") );

    wxArrayInt indexArray(numCols);
    if ( !ListView_GetColumnOrderArray(GetHwnd(), numCols, &indexArray[0]) )
        return -1;

    return indexArray[order];
}

int wxListCtrl::GetColumnOrder(int col) const
{
    const int numCols = GetColumnCount();
    wxASSERT_MSG( col >= 0 && col < numCols, wxT("Column index out of bounds") );

    wxArrayInt indexArray(numCols);
    if ( !ListView_GetColumnOrderArray(GetHwnd(), numCols, &indexArray[0]) )
        return -1;

    for ( int pos = 0; pos < numCols; pos++ )
    {
        if ( indexArray[pos] == col )
            return pos;
    }

    wxFAIL_MSG( wxT("no column with with given order?") );

    return -1;
}

// Gets the column order for all columns
wxArrayInt wxListCtrl::GetColumnsOrder() const
{
    const int numCols = GetColumnCount();

    wxArrayInt orders(numCols);
    if ( !ListView_GetColumnOrderArray(GetHwnd(), numCols, &orders[0]) )
        orders.clear();

    return orders;
}

// Sets the column order for all columns
bool wxListCtrl::SetColumnsOrder(const wxArrayInt& orders)
{
    const int numCols = GetColumnCount();

    wxCHECK_MSG( orders.size() == (size_t)numCols, false,
                    wxT("wrong number of elements in column orders array") );

    return ListView_SetColumnOrderArray(GetHwnd(), numCols, &orders[0]) != 0;
}


// Gets the number of items that can fit vertically in the
// visible area of the list control (list or report view)
// or the total number of items in the list control (icon
// or small icon view)
int wxListCtrl::GetCountPerPage() const
{
    return ListView_GetCountPerPage(GetHwnd());
}

// Gets the edit control for editing labels.
wxTextCtrl* wxListCtrl::GetEditControl() const
{
    // first check corresponds to the case when the label editing was started
    // by user and hence m_textCtrl wasn't created by EditLabel() at all, while
    // the second case corresponds to us being called from inside EditLabel()
    // (e.g. from a user wxEVT_LIST_BEGIN_LABEL_EDIT handler): in this
    // case EditLabel() did create the control but it didn't have an HWND to
    // initialize it with yet
    if ( !m_textCtrl || !m_textCtrl->GetHWND() )
    {
        HWND hwndEdit = ListView_GetEditControl(GetHwnd());
        if ( hwndEdit )
        {
            wxListCtrl * const self = const_cast<wxListCtrl *>(this);

            if ( !m_textCtrl )
                self->m_textCtrl = new wxTextCtrl;
            self->InitEditControl((WXHWND)hwndEdit);
        }
    }

    return m_textCtrl;
}

// Gets information about the item
bool wxListCtrl::GetItem(wxListItem& info) const
{
    LV_ITEM lvItem;
    wxZeroMemory(lvItem);

    lvItem.iItem = info.m_itemId;
    lvItem.iSubItem = info.m_col;

    if ( info.m_mask & wxLIST_MASK_TEXT )
    {
        lvItem.mask |= LVIF_TEXT;
        lvItem.pszText = new wxChar[513];
        lvItem.cchTextMax = 512;
    }
    else
    {
        lvItem.pszText = NULL;
    }

    if (info.m_mask & wxLIST_MASK_DATA)
        lvItem.mask |= LVIF_PARAM;

    if (info.m_mask & wxLIST_MASK_IMAGE)
        lvItem.mask |= LVIF_IMAGE;

    if ( info.m_mask & wxLIST_MASK_STATE )
    {
        lvItem.mask |= LVIF_STATE;
        wxConvertToMSWFlags(0, info.m_stateMask, lvItem);
    }

    bool success = ListView_GetItem((HWND)GetHWND(), &lvItem) != 0;
    if ( !success )
    {
        wxLogError(_("Couldn't retrieve information about list control item %d."),
                lvItem.iItem);
    }
    else
    {
        // give NULL as hwnd as we already have everything we need
        wxConvertFromMSWListItem(NULL, info, lvItem);
    }

    if (lvItem.pszText)
        delete[] lvItem.pszText;

    return success;
}

// Sets information about the item
bool wxListCtrl::SetItem(wxListItem& info)
{
    const long id = info.GetId();
    wxCHECK_MSG( id >= 0 && id < GetItemCount(), false,
                 wxT("invalid item index in SetItem") );

    LV_ITEM item;
    wxConvertToMSWListItem(this, info, item);

    // we never update the lParam if it contains our pointer
    // to the wxMSWListItemData structure
    item.mask &= ~LVIF_PARAM;

    // check if setting attributes or lParam
    if ( info.HasAttributes() || (info.m_mask & wxLIST_MASK_DATA) )
    {
        // get internal item data
        wxMSWListItemData *data = MSWGetItemData(id);

        if ( !data )
        {
            // need to allocate the internal data object
            data = new wxMSWListItemData;
            m_internalData.push_back(data);
            item.lParam = (LPARAM) data;
            item.mask |= LVIF_PARAM;
        }


        // user data
        if ( info.m_mask & wxLIST_MASK_DATA )
            data->lParam = info.m_data;

        // attributes
        if ( info.HasAttributes() )
        {
            const wxListItemAttr& attrNew = *info.GetAttributes();

            // don't overwrite the already set attributes if we have them
            if ( data->attr )
                data->attr->AssignFrom(attrNew);
            else
                data->attr = new wxListItemAttr(attrNew);
        }
    }


    // we could be changing only the attribute in which case we don't need to
    // call ListView_SetItem() at all
    if ( item.mask )
    {
        if ( !ListView_SetItem(GetHwnd(), &item) )
        {
            wxLogDebug(wxT("ListView_SetItem() failed"));

            return false;
        }
    }

    // we need to update the item immediately to show the new image
    bool updateNow = (info.m_mask & wxLIST_MASK_IMAGE) != 0;

    // check whether it has any custom attributes
    if ( info.HasAttributes() )
    {
        m_hasAnyAttr = true;

        // if the colour has changed, we must redraw the item
        updateNow = true;
    }

    if ( updateNow )
    {
        // we need this to make the change visible right now
        RefreshItem(item.iItem);
    }

    return true;
}

long wxListCtrl::SetItem(long index, int col, const wxString& label, int imageId)
{
    wxListItem info;
    info.m_text = label;
    info.m_mask = wxLIST_MASK_TEXT;
    info.m_itemId = index;
    info.m_col = col;
    if ( imageId > -1 )
    {
        info.m_image = imageId;
        info.m_mask |= wxLIST_MASK_IMAGE;
    }
    return SetItem(info);
}


// Gets the item state
int wxListCtrl::GetItemState(long item, long stateMask) const
{
    wxListItem info;

    info.m_mask = wxLIST_MASK_STATE;
    info.m_stateMask = stateMask;
    info.m_itemId = item;

    if (!GetItem(info))
        return 0;

    return info.m_state;
}

// Sets the item state
bool wxListCtrl::SetItemState(long item, long state, long stateMask)
{
    // NB: don't use SetItem() here as it doesn't work with the virtual list
    //     controls
    LV_ITEM lvItem;
    wxZeroMemory(lvItem);

    wxConvertToMSWFlags(state, stateMask, lvItem);

    const bool changingFocus = (stateMask & wxLIST_STATE_FOCUSED) &&
                                    (state & wxLIST_STATE_FOCUSED);

    // for the virtual list controls we need to refresh the previously focused
    // item manually when changing focus without changing selection
    // programmatically because otherwise it keeps its focus rectangle until
    // next repaint (yet another comctl32 bug)
    long focusOld;
    if ( IsVirtual() && changingFocus )
    {
        focusOld = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_FOCUSED);
    }
    else
    {
        focusOld = -1;
    }

    if ( !::SendMessage(GetHwnd(), LVM_SETITEMSTATE,
                        (WPARAM)item, (LPARAM)&lvItem) )
    {
        wxLogLastError(wxT("ListView_SetItemState"));

        return false;
    }

    if ( focusOld != -1 )
    {
        // no need to refresh the item if it was previously selected, it would
        // only result in annoying flicker
        if ( !(GetItemState(focusOld,
                            wxLIST_STATE_SELECTED) & wxLIST_STATE_SELECTED) )
        {
            RefreshItem(focusOld);
        }
    }

    // we expect the selection anchor, i.e. the item from which multiple
    // selection (such as performed with e.g. Shift-arrows) starts, to be the
    // same as the currently focused item but the native control doesn't update
    // it when we change focus and leaves at the last item it set itself focus
    // to, so do it explicitly
    if ( changingFocus && !HasFlag(wxLC_SINGLE_SEL) )
    {
        ListView_SetSelectionMark(GetHwnd(), item);
    }

    return true;
}

// Sets the item image
bool wxListCtrl::SetItemImage(long item, int image, int WXUNUSED(selImage))
{
    return SetItemColumnImage(item, 0, image);
}

// Sets the item image
bool wxListCtrl::SetItemColumnImage(long item, long column, int image)
{
    wxListItem info;

    info.m_mask = wxLIST_MASK_IMAGE;
    info.m_image = image == -1 ? I_IMAGENONE : image;
    info.m_itemId = item;
    info.m_col = column;

    return SetItem(info);
}

// Gets the item text
wxString wxListCtrl::GetItemText(long item, int col) const
{
    wxListItem info;

    info.m_mask = wxLIST_MASK_TEXT;
    info.m_itemId = item;
    info.m_col = col;

    if (!GetItem(info))
        return wxEmptyString;
    return info.m_text;
}

// Sets the item text
void wxListCtrl::SetItemText(long item, const wxString& str)
{
    wxListItem info;

    info.m_mask = wxLIST_MASK_TEXT;
    info.m_itemId = item;
    info.m_text = str;

    SetItem(info);
}

// Gets the internal item data
wxMSWListItemData *wxListCtrl::MSWGetItemData(long itemId) const
{
    LV_ITEM it;
    it.mask = LVIF_PARAM;
    it.iItem = itemId;

    if ( !ListView_GetItem(GetHwnd(), &it) )
        return NULL;

    return (wxMSWListItemData *) it.lParam;
}

// Gets the item data
wxUIntPtr wxListCtrl::GetItemData(long item) const
{
    wxListItem info;

    info.m_mask = wxLIST_MASK_DATA;
    info.m_itemId = item;

    if (!GetItem(info))
        return 0;
    return info.m_data;
}

// Sets the item data
bool wxListCtrl::SetItemPtrData(long item, wxUIntPtr data)
{
    wxListItem info;

    info.m_mask = wxLIST_MASK_DATA;
    info.m_itemId = item;
    info.m_data = data;

    return SetItem(info);
}

wxRect wxListCtrl::GetViewRect() const
{
    wxRect rect;

    // ListView_GetViewRect() can only be used in icon and small icon views
    // (this is documented in MSDN and, indeed, it returns bogus results in
    // report view, at least with comctl32.dll v6 under Windows 2003)
    if ( HasFlag(wxLC_ICON | wxLC_SMALL_ICON) )
    {
        RECT rc;
        if ( !ListView_GetViewRect(GetHwnd(), &rc) )
        {
            wxLogDebug(wxT("ListView_GetViewRect() failed."));

            wxZeroMemory(rc);
        }

        wxCopyRECTToRect(rc, rect);
    }
    else if ( HasFlag(wxLC_REPORT) )
    {
        const long count = GetItemCount();
        if ( count )
        {
            GetItemRect(wxMin(GetTopItem() + GetCountPerPage(), count - 1), rect);

            // extend the rectangle to start at the top (we include the column
            // headers, if any, for compatibility with the generic version)
            rect.height += rect.y;
            rect.y = 0;
        }
    }
    else
    {
        wxFAIL_MSG( wxT("not implemented in this mode") );
    }

    return rect;
}

// Gets the item rectangle
bool wxListCtrl::GetItemRect(long item, wxRect& rect, int code) const
{
    return GetSubItemRect( item, wxLIST_GETSUBITEMRECT_WHOLEITEM, rect, code) ;
}

bool wxListCtrl::GetSubItemRect(long item, long subItem, wxRect& rect, int code) const
{
    // ListView_GetSubItemRect() doesn't do subItem error checking and returns
    // true even for the out of range values of it (even if the results are
    // completely bogus in this case), so we check item validity ourselves
    wxCHECK_MSG( subItem == wxLIST_GETSUBITEMRECT_WHOLEITEM ||
                    (subItem >= 0 && subItem < GetColumnCount()),
                 false, wxT("invalid sub item index") );

    // use wxCHECK_MSG against "item" too, for coherency with the generic implementation:
    wxCHECK_MSG( item >= 0 && item < GetItemCount(), false,
                 wxT("invalid item in GetSubItemRect") );

    int codeWin;
    if ( code == wxLIST_RECT_BOUNDS )
        codeWin = LVIR_BOUNDS;
    else if ( code == wxLIST_RECT_ICON )
        codeWin = LVIR_ICON;
    else if ( code == wxLIST_RECT_LABEL )
        codeWin = LVIR_LABEL;
    else
    {
        wxFAIL_MSG( wxT("incorrect code in GetItemRect() / GetSubItemRect()") );
        codeWin = LVIR_BOUNDS;
    }

    RECT rectWin;
    if ( !wxGetListCtrlSubItemRect
          (
            GetHwnd(),
            item,
            subItem == wxLIST_GETSUBITEMRECT_WHOLEITEM ? 0 : subItem,
            codeWin,
            rectWin
          ) )
    {
        return false;
    }

    wxCopyRECTToRect(rectWin, rect);

    // there is no way to retrieve the first sub item bounding rectangle using
    // wxGetListCtrlSubItemRect() as 0 means the whole item, so we need to
    // truncate it at first column ourselves
    if ( subItem == 0 && code == wxLIST_RECT_BOUNDS )
        rect.width = GetColumnWidth(0);

    return true;
}




// Gets the item position
bool wxListCtrl::GetItemPosition(long item, wxPoint& pos) const
{
    POINT pt;

    bool success = (ListView_GetItemPosition(GetHwnd(), (int) item, &pt) != 0);

    pos.x = pt.x; pos.y = pt.y;
    return success;
}

// Sets the item position.
bool wxListCtrl::SetItemPosition(long item, const wxPoint& pos)
{
    return (ListView_SetItemPosition(GetHwnd(), (int) item, pos.x, pos.y) != 0);
}

// Gets the number of items in the list control
int wxListCtrl::GetItemCount() const
{
    return m_count;
}

wxSize wxListCtrl::GetItemSpacing() const
{
    const int spacing = ListView_GetItemSpacing(GetHwnd(), (BOOL)HasFlag(wxLC_SMALL_ICON));

    return wxSize(LOWORD(spacing), HIWORD(spacing));
}

#if WXWIN_COMPATIBILITY_2_6

int wxListCtrl::GetItemSpacing(bool isSmall) const
{
    return ListView_GetItemSpacing(GetHwnd(), (BOOL) isSmall);
}

#endif // WXWIN_COMPATIBILITY_2_6

void wxListCtrl::SetItemTextColour( long item, const wxColour &col )
{
    wxListItem info;
    info.m_itemId = item;
    info.SetTextColour( col );
    SetItem( info );
}

wxColour wxListCtrl::GetItemTextColour( long item ) const
{
    wxColour col;
    wxMSWListItemData *data = MSWGetItemData(item);
    if ( data && data->attr )
        col = data->attr->GetTextColour();

    return col;
}

void wxListCtrl::SetItemBackgroundColour( long item, const wxColour &col )
{
    wxListItem info;
    info.m_itemId = item;
    info.SetBackgroundColour( col );
    SetItem( info );
}

wxColour wxListCtrl::GetItemBackgroundColour( long item ) const
{
    wxColour col;
    wxMSWListItemData *data = MSWGetItemData(item);
    if ( data && data->attr )
        col = data->attr->GetBackgroundColour();

    return col;
}

void wxListCtrl::SetItemFont( long item, const wxFont &f )
{
    wxListItem info;
    info.m_itemId = item;
    info.SetFont( f );
    SetItem( info );
}

wxFont wxListCtrl::GetItemFont( long item ) const
{
    wxFont f;
    wxMSWListItemData *data = MSWGetItemData(item);
    if ( data && data->attr )
        f = data->attr->GetFont();

    return f;
}

// Gets the number of selected items in the list control
int wxListCtrl::GetSelectedItemCount() const
{
    return ListView_GetSelectedCount(GetHwnd());
}

// Gets the text colour of the listview
wxColour wxListCtrl::GetTextColour() const
{
    COLORREF ref = ListView_GetTextColor(GetHwnd());
    wxColour col(GetRValue(ref), GetGValue(ref), GetBValue(ref));
    return col;
}

// Sets the text colour of the listview
void wxListCtrl::SetTextColour(const wxColour& col)
{
    ListView_SetTextColor(GetHwnd(), PALETTERGB(col.Red(), col.Green(), col.Blue()));
}

// Gets the index of the topmost visible item when in
// list or report view
long wxListCtrl::GetTopItem() const
{
    return (long) ListView_GetTopIndex(GetHwnd());
}

// Searches for an item, starting from 'item'.
// 'geometry' is one of
// wxLIST_NEXT_ABOVE/ALL/BELOW/LEFT/RIGHT.
// 'state' is a state bit flag, one or more of
// wxLIST_STATE_DROPHILITED/FOCUSED/SELECTED/CUT.
// item can be -1 to find the first item that matches the
// specified flags.
// Returns the item or -1 if unsuccessful.
long wxListCtrl::GetNextItem(long item, int geom, int state) const
{
    long flags = 0;

    if ( geom == wxLIST_NEXT_ABOVE )
        flags |= LVNI_ABOVE;
    if ( geom == wxLIST_NEXT_ALL )
        flags |= LVNI_ALL;
    if ( geom == wxLIST_NEXT_BELOW )
        flags |= LVNI_BELOW;
    if ( geom == wxLIST_NEXT_LEFT )
        flags |= LVNI_TOLEFT;
    if ( geom == wxLIST_NEXT_RIGHT )
        flags |= LVNI_TORIGHT;

    if ( state & wxLIST_STATE_CUT )
        flags |= LVNI_CUT;
    if ( state & wxLIST_STATE_DROPHILITED )
        flags |= LVNI_DROPHILITED;
    if ( state & wxLIST_STATE_FOCUSED )
        flags |= LVNI_FOCUSED;
    if ( state & wxLIST_STATE_SELECTED )
        flags |= LVNI_SELECTED;

    return (long) ListView_GetNextItem(GetHwnd(), item, flags);
}


wxImageList *wxListCtrl::GetImageList(int which) const
{
    if ( which == wxIMAGE_LIST_NORMAL )
    {
        return m_imageListNormal;
    }
    else if ( which == wxIMAGE_LIST_SMALL )
    {
        return m_imageListSmall;
    }
    else if ( which == wxIMAGE_LIST_STATE )
    {
        return m_imageListState;
    }
    return NULL;
}

void wxListCtrl::SetImageList(wxImageList *imageList, int which)
{
    int flags = 0;
    if ( which == wxIMAGE_LIST_NORMAL )
    {
        flags = LVSIL_NORMAL;
        if (m_ownsImageListNormal) delete m_imageListNormal;
        m_imageListNormal = imageList;
        m_ownsImageListNormal = false;
    }
    else if ( which == wxIMAGE_LIST_SMALL )
    {
        flags = LVSIL_SMALL;
        if (m_ownsImageListSmall) delete m_imageListSmall;
        m_imageListSmall = imageList;
        m_ownsImageListSmall = false;
    }
    else if ( which == wxIMAGE_LIST_STATE )
    {
        flags = LVSIL_STATE;
        if (m_ownsImageListState) delete m_imageListState;
        m_imageListState = imageList;
        m_ownsImageListState = false;
    }
    (void) ListView_SetImageList(GetHwnd(), (HIMAGELIST) imageList ? imageList->GetHIMAGELIST() : 0, flags);
}

void wxListCtrl::AssignImageList(wxImageList *imageList, int which)
{
    SetImageList(imageList, which);
    if ( which == wxIMAGE_LIST_NORMAL )
        m_ownsImageListNormal = true;
    else if ( which == wxIMAGE_LIST_SMALL )
        m_ownsImageListSmall = true;
    else if ( which == wxIMAGE_LIST_STATE )
        m_ownsImageListState = true;
}

// ----------------------------------------------------------------------------
// Geometry
// ----------------------------------------------------------------------------

wxSize wxListCtrl::MSWGetBestViewRect(int x, int y) const
{
    // The cast is necessary to suppress a MinGW warning due to a missing cast
    // to WPARAM in the definition of ListView_ApproximateViewRect() in its
    // own headers (this was the case up to at least MinGW 4.8).
    const DWORD rc = ListView_ApproximateViewRect(GetHwnd(), x, y, (WPARAM)-1);

    wxSize size(LOWORD(rc), HIWORD(rc));

    // We have to add space for the scrollbars ourselves, they're not taken
    // into account by ListView_ApproximateViewRect(), at least not with
    // commctrl32.dll v6.
    const DWORD mswStyle = ::GetWindowLong(GetHwnd(), GWL_STYLE);

    if ( mswStyle & WS_HSCROLL )
        size.y += wxSystemSettings::GetMetric(wxSYS_HSCROLL_Y);
    if ( mswStyle & WS_VSCROLL )
        size.x += wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);

    return size;
}

// ----------------------------------------------------------------------------
// Operations
// ----------------------------------------------------------------------------

// Arranges the items
bool wxListCtrl::Arrange(int flag)
{
    UINT code = 0;
    if ( flag == wxLIST_ALIGN_LEFT )
        code = LVA_ALIGNLEFT;
    else if ( flag == wxLIST_ALIGN_TOP )
        code = LVA_ALIGNTOP;
    else if ( flag == wxLIST_ALIGN_DEFAULT )
        code = LVA_DEFAULT;
    else if ( flag == wxLIST_ALIGN_SNAP_TO_GRID )
        code = LVA_SNAPTOGRID;

    return (ListView_Arrange(GetHwnd(), code) != 0);
}

// Deletes an item
bool wxListCtrl::DeleteItem(long item)
{
    if ( !ListView_DeleteItem(GetHwnd(), (int) item) )
    {
        wxLogLastError(wxT("ListView_DeleteItem"));
        return false;
    }

    m_count--;
    wxASSERT_MSG( m_count == ListView_GetItemCount(GetHwnd()),
                  wxT("m_count should match ListView_GetItemCount"));

    // the virtual list control doesn't refresh itself correctly, help it
    if ( IsVirtual() )
    {
        // we need to refresh all the lines below the one which was deleted
        wxRect rectItem;
        if ( item > 0 && GetItemCount() )
        {
            GetItemRect(item - 1, rectItem);
        }
        else
        {
            rectItem.y =
            rectItem.height = 0;
        }

        wxRect rectWin = GetRect();
        rectWin.height = rectWin.GetBottom() - rectItem.GetBottom();
        rectWin.y = rectItem.GetBottom();

        RefreshRect(rectWin);
    }

    return true;
}

// Deletes all items
bool wxListCtrl::DeleteAllItems()
{
    // Calling ListView_DeleteAllItems() will always generate an event but we
    // shouldn't do it if the control is empty
    return !GetItemCount() || ListView_DeleteAllItems(GetHwnd()) != 0;
}

// Deletes all items
bool wxListCtrl::DeleteAllColumns()
{
    while ( m_colCount > 0 )
    {
        if ( ListView_DeleteColumn(GetHwnd(), 0) == 0 )
        {
            wxLogLastError(wxT("ListView_DeleteColumn"));

            return false;
        }

        m_colCount--;
    }

    wxASSERT_MSG( m_colCount == 0, wxT("no columns should be left") );

    return true;
}

// Deletes a column
bool wxListCtrl::DeleteColumn(int col)
{
    bool success = (ListView_DeleteColumn(GetHwnd(), col) != 0);

    if ( success && (m_colCount > 0) )
        m_colCount --;
    return success;
}

// Clears items, and columns if there are any.
void wxListCtrl::ClearAll()
{
    DeleteAllItems();
    if ( m_colCount > 0 )
        DeleteAllColumns();
}

void wxListCtrl::InitEditControl(WXHWND hWnd)
{
    m_textCtrl->SetHWND(hWnd);
    m_textCtrl->SubclassWin(hWnd);
    m_textCtrl->SetParent(this);

    // we must disallow TABbing away from the control while the edit control is
    // shown because this leaves it in some strange state (just try removing
    // this line and then pressing TAB while editing an item in  listctrl
    // inside a panel)
    m_textCtrl->SetWindowStyle(m_textCtrl->GetWindowStyle() | wxTE_PROCESS_TAB);
}

wxTextCtrl* wxListCtrl::EditLabel(long item, wxClassInfo* textControlClass)
{
    wxCHECK_MSG( textControlClass->IsKindOf(wxCLASSINFO(wxTextCtrl)), NULL,
                  "control used for label editing must be a wxTextCtrl" );

    // ListView_EditLabel requires that the list has focus.
    SetFocus();

    // create m_textCtrl here before calling ListView_EditLabel() because it
    // generates wxEVT_LIST_BEGIN_LABEL_EDIT event from inside it and
    // the user handler for it can call GetEditControl() resulting in an on
    // demand creation of a stock wxTextCtrl instead of the control of a
    // (possibly) custom wxClassInfo
    DeleteEditControl();
    m_textCtrl = (wxTextCtrl *)textControlClass->CreateObject();

    WXHWND hWnd = (WXHWND) ListView_EditLabel(GetHwnd(), item);
    if ( !hWnd )
    {
        // failed to start editing
        wxDELETE(m_textCtrl);

        return NULL;
    }

    // if GetEditControl() hasn't been called, we need to initialize the edit
    // control ourselves
    if ( !m_textCtrl->GetHWND() )
        InitEditControl(hWnd);

    return m_textCtrl;
}

// End label editing, optionally cancelling the edit
bool wxListCtrl::EndEditLabel(bool cancel)
{
    // m_textCtrl is not always ready, ie. in EVT_LIST_BEGIN_LABEL_EDIT
    HWND hwnd = ListView_GetEditControl(GetHwnd());
    if ( !hwnd )
        return false;

    // Newer versions of Windows have a special ListView_CancelEditLabel()
    // message for cancelling editing but it, rather counter-intuitively, keeps
    // the last text entered in the dialog while cancelling as we do it below
    // restores the original text which is the more expected behaviour.

    // We shouldn't destroy the control ourselves according to MSDN, which
    // proposes WM_CANCELMODE to do this, but it doesn't seem to work so
    // emulate the corresponding user action instead.
    ::SendMessage(hwnd, WM_KEYDOWN, cancel ? VK_ESCAPE : VK_RETURN, 0);

    return true;
}

// Ensures this item is visible
bool wxListCtrl::EnsureVisible(long item)
{
    return ListView_EnsureVisible(GetHwnd(), (int) item, FALSE) != FALSE;
}

// Find an item whose label matches this string, starting from the item after 'start'
// or the beginning if 'start' is -1.
long wxListCtrl::FindItem(long start, const wxString& str, bool partial)
{
    LV_FINDINFO findInfo;

    findInfo.flags = LVFI_STRING;
    if ( partial )
        findInfo.flags |= LVFI_PARTIAL;
    findInfo.psz = str.t_str();

    // ListView_FindItem() excludes the first item from search and to look
    // through all the items you need to start from -1 which is unnatural and
    // inconsistent with the generic version - so we adjust the index
    if (start != -1)
        start --;
    return ListView_FindItem(GetHwnd(), start, &findInfo);
}

// Find an item whose data matches this data, starting from the item after
// 'start' or the beginning if 'start' is -1.
long wxListCtrl::FindItem(long start, wxUIntPtr data)
{
    // we can't use ListView_FindItem() directly as we don't store the data
    // pointer itself in the control but rather our own internal data, so first
    // we need to find the right value to search for (and there can be several
    // of them)
    int idx = wxNOT_FOUND;
    const unsigned count = m_internalData.size();
    for ( unsigned n = 0; n < count; n++ )
    {
        if ( m_internalData[n]->lParam == (LPARAM)data )
        {
            LV_FINDINFO findInfo;
            findInfo.flags = LVFI_PARAM;
            findInfo.lParam = (LPARAM)wxPtrToUInt(m_internalData[n]);

            int rc = ListView_FindItem(GetHwnd(), start, &findInfo);
            if ( rc != -1 )
            {
                if ( idx == wxNOT_FOUND || rc < idx )
                {
                    idx = rc;
                    if ( idx == start + 1 )
                    {
                        // we can stop here, we don't risk finding a closer
                        // match
                        break;
                    }
                }
                //else: this item is after the previously found one
            }
        }
    }

    return idx;
}

// Find an item nearest this position in the specified direction, starting from
// the item after 'start' or the beginning if 'start' is -1.
long wxListCtrl::FindItem(long start, const wxPoint& pt, int direction)
{
    LV_FINDINFO findInfo;

    findInfo.flags = LVFI_NEARESTXY;
    findInfo.pt.x = pt.x;
    findInfo.pt.y = pt.y;
    findInfo.vkDirection = VK_RIGHT;

    if ( direction == wxLIST_FIND_UP )
        findInfo.vkDirection = VK_UP;
    else if ( direction == wxLIST_FIND_DOWN )
        findInfo.vkDirection = VK_DOWN;
    else if ( direction == wxLIST_FIND_LEFT )
        findInfo.vkDirection = VK_LEFT;
    else if ( direction == wxLIST_FIND_RIGHT )
        findInfo.vkDirection = VK_RIGHT;

    return ListView_FindItem(GetHwnd(), start, &findInfo);
}

// Determines which item (if any) is at the specified point,
// giving details in 'flags' (see wxLIST_HITTEST_... flags above)
long
wxListCtrl::HitTest(const wxPoint& point, int& flags, long *ptrSubItem) const
{
    LV_HITTESTINFO hitTestInfo;
    hitTestInfo.pt.x = (int) point.x;
    hitTestInfo.pt.y = (int) point.y;

    long item;
#ifdef LVM_SUBITEMHITTEST
    if ( ptrSubItem && wxApp::GetComCtl32Version() >= 470 )
    {
        item = ListView_SubItemHitTest(GetHwnd(), &hitTestInfo);
        *ptrSubItem = hitTestInfo.iSubItem;
    }
    else
#endif // LVM_SUBITEMHITTEST
    {
        item = ListView_HitTest(GetHwnd(), &hitTestInfo);
    }

    flags = 0;

    if ( hitTestInfo.flags & LVHT_ABOVE )
        flags |= wxLIST_HITTEST_ABOVE;
    if ( hitTestInfo.flags & LVHT_BELOW )
        flags |= wxLIST_HITTEST_BELOW;
    if ( hitTestInfo.flags & LVHT_TOLEFT )
        flags |= wxLIST_HITTEST_TOLEFT;
    if ( hitTestInfo.flags & LVHT_TORIGHT )
        flags |= wxLIST_HITTEST_TORIGHT;

    if ( hitTestInfo.flags & LVHT_NOWHERE )
        flags |= wxLIST_HITTEST_NOWHERE;

    // note a bug or at least a very strange feature of comtl32.dll (tested
    // with version 4.0 under Win95 and 6.0 under Win 2003): if you click to
    // the right of the item label, ListView_HitTest() returns a combination of
    // LVHT_ONITEMICON, LVHT_ONITEMLABEL and LVHT_ONITEMSTATEICON -- filter out
    // the bits which don't make sense
    if ( hitTestInfo.flags & LVHT_ONITEMLABEL )
    {
        flags |= wxLIST_HITTEST_ONITEMLABEL;

        // do not translate LVHT_ONITEMICON here, as per above
    }
    else
    {
        if ( hitTestInfo.flags & LVHT_ONITEMICON )
            flags |= wxLIST_HITTEST_ONITEMICON;
        if ( hitTestInfo.flags & LVHT_ONITEMSTATEICON )
            flags |= wxLIST_HITTEST_ONITEMSTATEICON;
    }

    return item;
}


// Inserts an item, returning the index of the new item if successful,
// -1 otherwise.
long wxListCtrl::InsertItem(const wxListItem& info)
{
    wxASSERT_MSG( !IsVirtual(), wxT("can't be used with virtual controls") );

    // In 2.8 it was possible to succeed inserting an item without initializing
    // its ID as it defaulted to 0. This was however never supported and in 2.9
    // the ID is -1 by default and inserting it simply fails, but it might be
    // not obvious why does it happen, so check it proactively.
    wxASSERT_MSG( info.m_itemId != -1, wxS("Item ID must be set.") );

    LV_ITEM item;
    wxConvertToMSWListItem(this, info, item);
    item.mask &= ~LVIF_PARAM;

    // check whether we need to allocate our internal data
    bool needInternalData = (info.m_mask & wxLIST_MASK_DATA) ||
                                info.HasAttributes();
    if ( needInternalData )
    {
        item.mask |= LVIF_PARAM;

        wxMSWListItemData * const data = new wxMSWListItemData;
        m_internalData.push_back(data);
        item.lParam = (LPARAM)data;

        if ( info.m_mask & wxLIST_MASK_DATA )
            data->lParam = info.m_data;

        // check whether it has any custom attributes
        if ( info.HasAttributes() )
        {
            // take copy of attributes
            data->attr = new wxListItemAttr(*info.GetAttributes());

            // and remember that we have some now...
            m_hasAnyAttr = true;
        }
    }

    const long rv = ListView_InsertItem(GetHwnd(), & item);

    // failing to insert the item is really unexpected
    wxCHECK_MSG( rv != -1, rv, "failed to insert an item in wxListCtrl" );

    m_count++;
    wxASSERT_MSG( m_count == ListView_GetItemCount(GetHwnd()),
                  wxT("m_count should match ListView_GetItemCount"));

    return rv;
}

long wxListCtrl::InsertItem(long index, const wxString& label)
{
    wxListItem info;
    info.m_text = label;
    info.m_mask = wxLIST_MASK_TEXT;
    info.m_itemId = index;
    return InsertItem(info);
}

// Inserts an image item
long wxListCtrl::InsertItem(long index, int imageIndex)
{
    wxListItem info;
    info.m_image = imageIndex;
    info.m_mask = wxLIST_MASK_IMAGE;
    info.m_itemId = index;
    return InsertItem(info);
}

// Inserts an image/string item
long wxListCtrl::InsertItem(long index, const wxString& label, int imageIndex)
{
    wxListItem info;
    info.m_image = imageIndex == -1 ? I_IMAGENONE : imageIndex;
    info.m_text = label;
    info.m_mask = wxLIST_MASK_TEXT | wxLIST_MASK_IMAGE;
    info.m_itemId = index;
    return InsertItem(info);
}

// For list view mode (only), inserts a column.
long wxListCtrl::DoInsertColumn(long col, const wxListItem& item)
{
    LV_COLUMN lvCol;
    wxConvertToMSWListCol(GetHwnd(), col, item, lvCol);

    // LVSCW_AUTOSIZE_USEHEADER is not supported when inserting new column,
    // we'll deal with it below instead. Plain LVSCW_AUTOSIZE is not supported
    // neither but it doesn't need any special handling as we use fixed value
    // for it here, both because we can't do anything else (there are no items
    // with values in this column to compute the size from yet) and for
    // compatibility as wxLIST_AUTOSIZE == -1 and -1 as InsertColumn() width
    // parameter used to mean "arbitrary fixed width".
    if ( !(lvCol.mask & LVCF_WIDTH) || lvCol.cx < 0 )
    {
        // always give some width to the new column: this one is compatible
        // with the generic version
        lvCol.mask |= LVCF_WIDTH;
        lvCol.cx = 80;
    }

    long n = ListView_InsertColumn(GetHwnd(), col, &lvCol);
    if ( n == -1 )
    {
        wxLogDebug(wxT("Failed to insert the column '%s' into listview!"),
                   lvCol.pszText);
        return -1;
    }

    m_colCount++;

    // Now adjust the new column size.
    if ( (item.GetMask() & wxLIST_MASK_WIDTH) &&
            (item.GetWidth() == wxLIST_AUTOSIZE_USEHEADER) )
    {
        SetColumnWidth(n, wxLIST_AUTOSIZE_USEHEADER);
    }

    return n;
}

// scroll the control by the given number of pixels (exception: in list view,
// dx is interpreted as number of columns)
bool wxListCtrl::ScrollList(int dx, int dy)
{
    if ( !ListView_Scroll(GetHwnd(), dx, dy) )
    {
        wxLogDebug(wxT("ListView_Scroll(%d, %d) failed"), dx, dy);

        return false;
    }

    return true;
}

// Sort items.

// fn is a function which takes 3 long arguments: item1, item2, data.
// item1 is the long data associated with a first item (NOT the index).
// item2 is the long data associated with a second item (NOT the index).
// data is the same value as passed to SortItems.
// The return value is a negative number if the first item should precede the second
// item, a positive number of the second item should precede the first,
// or zero if the two items are equivalent.

// data is arbitrary data to be passed to the sort function.

// Internal structures for proxying the user compare function
// so that we can pass it the *real* user data

// translate lParam data and call user func
struct wxInternalDataSort
{
    wxListCtrlCompare user_fn;
    wxIntPtr data;
};

int CALLBACK wxInternalDataCompareFunc(LPARAM lParam1, LPARAM lParam2,  LPARAM lParamSort)
{
    wxInternalDataSort * const internalData = (wxInternalDataSort *) lParamSort;

    wxMSWListItemData *data1 = (wxMSWListItemData *) lParam1;
    wxMSWListItemData *data2 = (wxMSWListItemData *) lParam2;

    wxIntPtr d1 = (data1 == NULL ? 0 : data1->lParam);
    wxIntPtr d2 = (data2 == NULL ? 0 : data2->lParam);

    return internalData->user_fn(d1, d2, internalData->data);

}

bool wxListCtrl::SortItems(wxListCtrlCompare fn, wxIntPtr data)
{
    wxInternalDataSort internalData;
    internalData.user_fn = fn;
    internalData.data = data;

    // WPARAM cast is needed for mingw/cygwin
    if ( !ListView_SortItems(GetHwnd(),
                             wxInternalDataCompareFunc,
                             (WPARAM) &internalData) )
    {
        wxLogDebug(wxT("ListView_SortItems() failed"));

        return false;
    }

    return true;
}



// ----------------------------------------------------------------------------
// message processing
// ----------------------------------------------------------------------------

bool wxListCtrl::MSWShouldPreProcessMessage(WXMSG* msg)
{
    if ( msg->message == WM_KEYDOWN )
    {
        // Only eat VK_RETURN if not being used by the application in
        // conjunction with modifiers
        if ( msg->wParam == VK_RETURN && !wxIsAnyModifierDown() )
        {
            // we need VK_RETURN to generate wxEVT_LIST_ITEM_ACTIVATED
            return false;
        }
    }
    return wxListCtrlBase::MSWShouldPreProcessMessage(msg);
}

bool wxListCtrl::MSWCommand(WXUINT cmd, WXWORD id_)
{
    const int id = (signed short)id_;
    if (cmd == EN_UPDATE)
    {
        wxCommandEvent event(wxEVT_TEXT, id);
        event.SetEventObject( this );
        ProcessCommand(event);
        return true;
    }
    else if (cmd == EN_KILLFOCUS)
    {
        wxCommandEvent event(wxEVT_KILL_FOCUS, id);
        event.SetEventObject( this );
        ProcessCommand(event);
        return true;
    }
    else
        return false;
}

// utility used by wxListCtrl::MSWOnNotify and by wxDataViewHeaderWindowMSW::MSWOnNotify
int WXDLLIMPEXP_CORE wxMSWGetColumnClicked(NMHDR *nmhdr, POINT *ptClick)
{
    // find the column clicked: we have to search for it ourselves as the
    // notification message doesn't provide this info

    // where did the click occur?
#if defined(__WXWINCE__) && !defined(__HANDHELDPC__) && _WIN32_WCE < 400
    if ( nmhdr->code == GN_CONTEXTMENU )
    {
        *ptClick = ((NMRGINFO*)nmhdr)->ptAction;
    }
    else
#endif //__WXWINCE__
    {
       wxGetCursorPosMSW(ptClick);
    }

    // we need to use listctrl coordinates for the event point so this is what
    // we return in ptClick, but for comparison with Header_GetItemRect()
    // result below we need to use header window coordinates
    POINT ptClickHeader = *ptClick;
    if ( !::ScreenToClient(nmhdr->hwndFrom, &ptClickHeader) )
    {
        wxLogLastError(wxT("ScreenToClient(listctrl header)"));
    }

    if ( !::ScreenToClient(::GetParent(nmhdr->hwndFrom), ptClick) )
    {
        wxLogLastError(wxT("ScreenToClient(listctrl)"));
    }

    const int colCount = Header_GetItemCount(nmhdr->hwndFrom);
    for ( int col = 0; col < colCount; col++ )
    {
        RECT rect;
        if ( Header_GetItemRect(nmhdr->hwndFrom, col, &rect) )
        {
            if ( ::PtInRect(&rect, ptClickHeader) )
            {
                return col;
            }
        }
    }

    return wxNOT_FOUND;
}

bool wxListCtrl::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result)
{

    // prepare the event
    // -----------------

    wxListEvent event(wxEVT_NULL, m_windowId);
    event.SetEventObject(this);

    wxEventType eventType = wxEVT_NULL;

    NMHDR *nmhdr = (NMHDR *)lParam;

    // if your compiler is as broken as this, you should really change it: this
    // code is needed for normal operation! #ifdef below is only useful for
    // automatic rebuilds which are done with a very old compiler version
#ifdef HDN_BEGINTRACKA

    // check for messages from the header (in report view)
    HWND hwndHdr = ListView_GetHeader(GetHwnd());

    // is it a message from the header?
    if ( nmhdr->hwndFrom == hwndHdr )
    {
        HD_NOTIFY *nmHDR = (HD_NOTIFY *)nmhdr;

        event.m_itemIndex = -1;

        bool ignore = false;
        switch ( nmhdr->code )
        {
            // yet another comctl32.dll bug: under NT/W2K it sends Unicode
            // TRACK messages even to ANSI programs: on my system I get
            // HDN_BEGINTRACKW and HDN_ENDTRACKA!
            //
            // work around is to simply catch both versions and hope that it
            // works (why should this message exist in ANSI and Unicode is
            // beyond me as it doesn't deal with strings at all...)
            //
            // another problem is that HDN_TRACK is not sent at all by header
            // with HDS_FULLDRAG style which is used by default by wxListCtrl
            // under recent Windows versions (starting from at least XP) so we
            // need to use HDN_ITEMCHANGING instead of it
            case HDN_BEGINTRACKA:
            case HDN_BEGINTRACKW:
                eventType = wxEVT_LIST_COL_BEGIN_DRAG;
                // fall through

            case HDN_ITEMCHANGING:
                if ( eventType == wxEVT_NULL )
                {
                    if ( !nmHDR->pitem || !(nmHDR->pitem->mask & HDI_WIDTH) )
                    {
                        // something other than the width is being changed,
                        // ignore it
                        ignore = true;
                        break;
                    }

                    // also ignore the events sent when the width didn't really
                    // change: this is not just an optimization but also gets
                    // rid of a useless and unexpected DRAGGING event which
                    // would otherwise be sent after the END_DRAG one as we get
                    // an HDN_ITEMCHANGING after HDN_ENDTRACK for some reason
                    if ( nmHDR->pitem->cxy == GetColumnWidth(nmHDR->iItem) )
                    {
                        ignore = true;
                        break;
                    }

                    eventType = wxEVT_LIST_COL_DRAGGING;
                }
                // fall through

            case HDN_ENDTRACKA:
            case HDN_ENDTRACKW:
                if ( eventType == wxEVT_NULL )
                    eventType = wxEVT_LIST_COL_END_DRAG;

                event.m_item.m_width = nmHDR->pitem->cxy;
                event.m_col = nmHDR->iItem;
                break;

#if defined(__WXWINCE__) && !defined(__HANDHELDPC__) && _WIN32_WCE < 400
            case GN_CONTEXTMENU:
#endif //__WXWINCE__
            case NM_RCLICK:
                {
                    POINT ptClick;

                    eventType = wxEVT_LIST_COL_RIGHT_CLICK;
                    event.m_col = wxMSWGetColumnClicked(nmhdr, &ptClick);
                    event.m_pointDrag.x = ptClick.x;
                    event.m_pointDrag.y = ptClick.y;
                }
                break;

            case HDN_GETDISPINFOW:
                // letting Windows XP handle this message results in mysterious
                // crashes in comctl32.dll seemingly because of bad message
                // parameters
                //
                // I have no idea what is the real cause of the bug (which is,
                // just to make things interesting, impossible to reproduce
                // reliably) but ignoring all these messages does fix it and
                // doesn't seem to have any negative consequences
                return true;

            default:
                ignore = true;
        }

        if ( ignore )
            return wxListCtrlBase::MSWOnNotify(idCtrl, lParam, result);
    }
    else
#endif // defined(HDN_BEGINTRACKA)
    if ( nmhdr->hwndFrom == GetHwnd() )
    {
        // almost all messages use NM_LISTVIEW
        NM_LISTVIEW *nmLV = (NM_LISTVIEW *)nmhdr;

        const int iItem = nmLV->iItem;


        // If we have a valid item then check if there is a data value
        // associated with it and put it in the event.
        if ( iItem >= 0 && iItem < GetItemCount() )
        {
            wxMSWListItemData *internaldata =
                MSWGetItemData(iItem);

            if ( internaldata )
                event.m_item.m_data = internaldata->lParam;
        }

        bool processed = true;
        switch ( nmhdr->code )
        {
            case LVN_BEGINRDRAG:
                eventType = wxEVT_LIST_BEGIN_RDRAG;
                // fall through

            case LVN_BEGINDRAG:
                if ( eventType == wxEVT_NULL )
                {
                    eventType = wxEVT_LIST_BEGIN_DRAG;
                }

                event.m_itemIndex = iItem;
                event.m_pointDrag.x = nmLV->ptAction.x;
                event.m_pointDrag.y = nmLV->ptAction.y;
                break;

            // NB: we have to handle both *A and *W versions here because some
            //     versions of comctl32.dll send ANSI messages even to the
            //     Unicode windows
            case LVN_BEGINLABELEDITA:
            case LVN_BEGINLABELEDITW:
                {
                    wxLV_ITEM item;
                    if ( nmhdr->code == LVN_BEGINLABELEDITA )
                    {
                        item.Init(((LV_DISPINFOA *)lParam)->item);
                    }
                    else // LVN_BEGINLABELEDITW
                    {
                        item.Init(((LV_DISPINFOW *)lParam)->item);
                    }

                    eventType = wxEVT_LIST_BEGIN_LABEL_EDIT;
                    wxConvertFromMSWListItem(GetHwnd(), event.m_item, item);
                    event.m_itemIndex = event.m_item.m_itemId;
                }
                break;

            case LVN_ENDLABELEDITA:
            case LVN_ENDLABELEDITW:
                {
                    wxLV_ITEM item;
                    if ( nmhdr->code == LVN_ENDLABELEDITA )
                    {
                        item.Init(((LV_DISPINFOA *)lParam)->item);
                    }
                    else // LVN_ENDLABELEDITW
                    {
                        item.Init(((LV_DISPINFOW *)lParam)->item);
                    }

                    // was editing cancelled?
                    const LV_ITEM& lvi = (LV_ITEM)item;
                    if ( !lvi.pszText || lvi.iItem == -1 )
                    {
                        // EDIT control will be deleted by the list control
                        // itself so prevent us from deleting it as well
                        DeleteEditControl();

                        event.SetEditCanceled(true);
                    }

                    eventType = wxEVT_LIST_END_LABEL_EDIT;
                    wxConvertFromMSWListItem(NULL, event.m_item, item);
                    event.m_itemIndex = event.m_item.m_itemId;
                }
                break;

            case LVN_COLUMNCLICK:
                eventType = wxEVT_LIST_COL_CLICK;
                event.m_itemIndex = -1;
                event.m_col = nmLV->iSubItem;
                break;

            case LVN_DELETEALLITEMS:
                eventType = wxEVT_LIST_DELETE_ALL_ITEMS;
                event.m_itemIndex = -1;
                break;

            case LVN_DELETEITEM:
                if ( m_count == 0 )
                {
                    // this should be prevented by the post-processing code
                    // below, but "just in case"
                    return false;
                }

                eventType = wxEVT_LIST_DELETE_ITEM;
                event.m_itemIndex = iItem;

                break;

            case LVN_INSERTITEM:
                eventType = wxEVT_LIST_INSERT_ITEM;
                event.m_itemIndex = iItem;
                break;

            case LVN_ITEMCHANGED:
                // we translate this catch all message into more interesting
                // (and more easy to process) wxWidgets events

                // first of all, we deal with the state change events only and
                // only for valid items (item == -1 for the virtual list
                // control)
                if ( nmLV->uChanged & LVIF_STATE && iItem != -1 )
                {
                    // temp vars for readability
                    const UINT stOld = nmLV->uOldState;
                    const UINT stNew = nmLV->uNewState;

                    event.m_item.SetId(iItem);
                    event.m_item.SetMask(wxLIST_MASK_TEXT |
                                         wxLIST_MASK_IMAGE |
                                         wxLIST_MASK_DATA);
                    GetItem(event.m_item);

                    // has the focus changed?
                    if ( !(stOld & LVIS_FOCUSED) && (stNew & LVIS_FOCUSED) )
                    {
                        eventType = wxEVT_LIST_ITEM_FOCUSED;
                        event.m_itemIndex = iItem;
                    }

                    if ( (stNew & LVIS_SELECTED) != (stOld & LVIS_SELECTED) )
                    {
                        if ( eventType != wxEVT_NULL )
                        {
                            // focus and selection have both changed: send the
                            // focus event from here and the selection one
                            // below
                            event.SetEventType(eventType);
                            (void)HandleWindowEvent(event);
                        }
                        else // no focus event to send
                        {
                            // then need to set m_itemIndex as it wasn't done
                            // above
                            event.m_itemIndex = iItem;
                        }

                        eventType = stNew & LVIS_SELECTED
                                        ? wxEVT_LIST_ITEM_SELECTED
                                        : wxEVT_LIST_ITEM_DESELECTED;
                    }
                }

                if ( eventType == wxEVT_NULL )
                {
                    // not an interesting event for us
                    return false;
                }

                break;

            case LVN_KEYDOWN:
                {
                    LV_KEYDOWN *info = (LV_KEYDOWN *)lParam;
                    WORD wVKey = info->wVKey;

                    // get the current selection
                    long lItem = GetNextItem(-1,
                                             wxLIST_NEXT_ALL,
                                             wxLIST_STATE_SELECTED);

                    // <Enter> or <Space> activate the selected item if any (but
                    // not with any modifiers as they have a predefined meaning
                    // then)
                    if ( lItem != -1 &&
                         (wVKey == VK_RETURN || wVKey == VK_SPACE) &&
                         !wxIsAnyModifierDown() )
                    {
                        eventType = wxEVT_LIST_ITEM_ACTIVATED;
                    }
                    else
                    {
                        eventType = wxEVT_LIST_KEY_DOWN;

                        event.m_code = wxMSWKeyboard::VKToWX(wVKey);

                        if ( event.m_code == WXK_NONE )
                        {
                            // We can't translate this to a standard key code,
                            // until support for Unicode key codes is added to
                            // wxListEvent we just ignore them.
                            return false;
                        }
                    }

                    event.m_itemIndex =
                    event.m_item.m_itemId = lItem;

                    if ( lItem != -1 )
                    {
                        // fill the other fields too
                        event.m_item.m_text = GetItemText(lItem);
                        event.m_item.m_data = GetItemData(lItem);
                    }
                }
                break;

            case NM_DBLCLK:
                // if the user processes it in wxEVT_COMMAND_LEFT_CLICK(), don't do
                // anything else
                if ( wxListCtrlBase::MSWOnNotify(idCtrl, lParam, result) )
                {
                    return true;
                }

                // else translate it into wxEVT_LIST_ITEM_ACTIVATED event
                // if it happened on an item (and not on empty place)
                if ( iItem == -1 )
                {
                    // not on item
                    return false;
                }

                eventType = wxEVT_LIST_ITEM_ACTIVATED;
                event.m_itemIndex = iItem;
                event.m_item.m_text = GetItemText(iItem);
                event.m_item.m_data = GetItemData(iItem);
                break;

#if defined(__WXWINCE__) && !defined(__HANDHELDPC__) && _WIN32_WCE < 400
            case GN_CONTEXTMENU:
#endif //__WXWINCE__
            case NM_RCLICK:
                // if the user processes it in wxEVT_COMMAND_RIGHT_CLICK(),
                // don't do anything else
                if ( wxListCtrlBase::MSWOnNotify(idCtrl, lParam, result) )
                {
                    return true;
                }

                // else translate it into wxEVT_LIST_ITEM_RIGHT_CLICK event
                LV_HITTESTINFO lvhti;
                wxZeroMemory(lvhti);

#if defined(__WXWINCE__) && !defined(__HANDHELDPC__) && _WIN32_WCE < 400
                if ( nmhdr->code == GN_CONTEXTMENU )
                {
                    lvhti.pt = ((NMRGINFO*)nmhdr)->ptAction;
                }
                else
#endif //__WXWINCE__
                {
                    wxGetCursorPosMSW(&(lvhti.pt));
                }

                ::ScreenToClient(GetHwnd(), &lvhti.pt);
                if ( ListView_HitTest(GetHwnd(), &lvhti) != -1 )
                {
                    if ( lvhti.flags & LVHT_ONITEM )
                    {
                        eventType = wxEVT_LIST_ITEM_RIGHT_CLICK;
                        event.m_itemIndex = lvhti.iItem;
                        event.m_pointDrag.x = lvhti.pt.x;
                        event.m_pointDrag.y = lvhti.pt.y;
                    }
                }
                break;

#ifdef NM_CUSTOMDRAW
            case NM_CUSTOMDRAW:
                *result = OnCustomDraw(lParam);

                return *result != CDRF_DODEFAULT;
#endif // _WIN32_IE >= 0x300

            case LVN_ODCACHEHINT:
                {
                    const NM_CACHEHINT *cacheHint = (NM_CACHEHINT *)lParam;

                    eventType = wxEVT_LIST_CACHE_HINT;

                    // we get some really stupid cache hints like ones for
                    // items in range 0..0 for an empty control or, after
                    // deleting an item, for items in invalid range -- filter
                    // this garbage out
                    if ( cacheHint->iFrom > cacheHint->iTo )
                        return false;

                    event.m_oldItemIndex = cacheHint->iFrom;

                    const long iMax = GetItemCount();
                    event.m_itemIndex = cacheHint->iTo < iMax ? cacheHint->iTo
                                                              : iMax - 1;
                }
                break;

#ifdef HAVE_NMLVFINDITEM
            case LVN_ODFINDITEM:
                // Find an item in a (necessarily virtual) list control.
                if ( IsVirtual() )
                {
                    NMLVFINDITEM* pFindInfo = (NMLVFINDITEM*)lParam;

                    // no match by default
                    *result = -1;

                    // we only handle string-based searches here
                    //
                    // TODO: what about LVFI_PARTIAL, should we handle this?
                    if ( !(pFindInfo->lvfi.flags & LVFI_STRING) )
                    {
                        return false;
                    }

                    const wxChar * const searchstr = pFindInfo->lvfi.psz;
                    const size_t len = wxStrlen(searchstr);

                    // this is the first item we should examine, search from it
                    // wrapping if necessary
                    int startPos = pFindInfo->iStart;
                    const int maxPos = GetItemCount();

                    // Check that the index is valid to ensure that our loop
                    // below always terminates.
                    if ( startPos < 0 || startPos >= maxPos )
                    {
                        // When the last item in the control is selected,
                        // iStart is really set to (invalid) maxPos index so
                        // accept this silently.
                        if ( startPos != maxPos )
                        {
                            wxLogDebug(wxT("Ignoring invalid search start ")
                                       wxT("position %d in list control with ")
                                       wxT("%d items."), startPos, maxPos);
                        }

                        startPos = 0;
                    }

                    // Linear search in a control with a lot of items can take
                    // a long time so we limit the total time of the search to
                    // ensure that the program doesn't appear to hang.
#if wxUSE_STOPWATCH
                    wxStopWatch sw;
#endif // wxUSE_STOPWATCH
                    for ( int currentPos = startPos; ; )
                    {
                        // does this item begin with searchstr?
                        if ( wxStrnicmp(searchstr,
                                            GetItemText(currentPos), len) == 0 )
                        {
                            *result = currentPos;
                            break;
                        }

                        // Go to next item with wrapping if necessary.
                        if ( ++currentPos == maxPos )
                        {
                            // Surprisingly, LVFI_WRAP seems to be never set in
                            // the flags so wrap regardless of it.
                            currentPos = 0;
                        }

                        if ( currentPos == startPos )
                        {
                            // We examined all items without finding anything.
                            //
                            // Notice that we still return true as we did
                            // perform the search, if we didn't do this the
                            // message would have been considered unhandled and
                            // the control seems to always select the first
                            // item by default in this case.
                            return true;
                        }

#if wxUSE_STOPWATCH
                        // Check the time elapsed only every thousand
                        // iterations for performance reasons: if we did it
                        // more often calling wxStopWatch::Time() could take
                        // noticeable time on its own.
                        if ( !((currentPos - startPos)%1000) )
                        {
                            // We use half a second to limit the search time
                            // which is about as long as we can take without
                            // annoying the user.
                            if ( sw.Time() > 500 )
                            {
                                // As above, return true to prevent the control
                                // from selecting the first item by default.
                                return true;
                            }
                        }
#endif // wxUSE_STOPWATCH

                    }

                    SetItemState(*result,
                                 wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                 wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
                    EnsureVisible(*result);
                    return true;
                }
                else
                {
                    processed = false;
                }
                break;
#endif // HAVE_NMLVFINDITEM

            case LVN_GETDISPINFO:
                if ( IsVirtual() )
                {
                    LV_DISPINFO *info = (LV_DISPINFO *)lParam;

                    LV_ITEM& lvi = info->item;
                    long item = lvi.iItem;

                    if ( lvi.mask & LVIF_TEXT )
                    {
                        wxString text = OnGetItemText(item, lvi.iSubItem);
                        wxStrlcpy(lvi.pszText, text.c_str(), lvi.cchTextMax);
                    }

                    // see comment at the end of wxListCtrl::GetColumn()
#ifdef NM_CUSTOMDRAW
                    if ( lvi.mask & LVIF_IMAGE )
                    {
                        lvi.iImage = OnGetItemColumnImage(item, lvi.iSubItem);
                    }
#endif // NM_CUSTOMDRAW

                    // even though we never use LVM_SETCALLBACKMASK, we still
                    // can get messages with LVIF_STATE in lvi.mask under Vista
                    if ( lvi.mask & LVIF_STATE )
                    {
                        // we don't have anything to return from here...
                        lvi.stateMask = 0;
                    }

                    return true;
                }
                // fall through

            default:
                processed = false;
        }

        if ( !processed )
            return wxListCtrlBase::MSWOnNotify(idCtrl, lParam, result);
    }
    else
    {
        // where did this one come from?
        return false;
    }

    // process the event
    // -----------------

    event.SetEventType(eventType);

    // fill in the item before passing it to the event handler if we do have a
    // valid item index and haven't filled it yet (e.g. for LVN_ITEMCHANGED)
    // and we're not using a virtual control as in this case the program
    // already has the data anyhow and we don't want to call GetItem() for
    // potentially many items
    if ( event.m_itemIndex != -1 && !event.m_item.GetMask()
            && !IsVirtual() )
    {
        wxListItem& item = event.m_item;

        item.SetId(event.m_itemIndex);
        item.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_IMAGE | wxLIST_MASK_DATA);
        GetItem(item);
    }

    bool processed = HandleWindowEvent(event);

    // post processing
    // ---------------
    switch ( nmhdr->code )
    {
        case LVN_DELETEALLITEMS:
            // always return true to suppress all additional LVN_DELETEITEM
            // notifications - this makes deleting all items from a list ctrl
            // much faster
            *result = TRUE;

            // also, we may free all user data now (couldn't do it before as
            // the user should have access to it in OnDeleteAllItems() handler)
            FreeAllInternalData();

            // the control is empty now, synchronize the cached number of items
            // with the real one
            m_count = 0;
            return true;

        case LVN_DELETEITEM:
            // Delete the associated internal data. Notice that this can be
            // done only after the event has been handled as the data could be
            // accessed during the handling of the event.
            if ( wxMSWListItemData *data = MSWGetItemData(event.m_itemIndex) )
            {
                const unsigned count = m_internalData.size();
                for ( unsigned n = 0; n < count; n++ )
                {
                    if ( m_internalData[n] == data )
                    {
                        m_internalData.erase(m_internalData.begin() + n);
                        wxDELETE(data);
                        break;
                    }
                }

                wxASSERT_MSG( !data, "invalid internal data pointer?" );
            }
            break;

        case LVN_ENDLABELEDITA:
        case LVN_ENDLABELEDITW:
            // logic here is inverted compared to all the other messages
            *result = event.IsAllowed();

            // EDIT control will be deleted by the list control itself so
            // prevent us from deleting it as well
            DeleteEditControl();

            return true;
    }

    if ( processed )
        *result = !event.IsAllowed();

    return processed;
}

// ----------------------------------------------------------------------------
// custom draw stuff
// ----------------------------------------------------------------------------

// see comment at the end of wxListCtrl::GetColumn()
#ifdef NM_CUSTOMDRAW // _WIN32_IE >= 0x0300

static RECT GetCustomDrawnItemRect(const NMCUSTOMDRAW& nmcd)
{
    RECT rc;
    wxGetListCtrlItemRect(nmcd.hdr.hwndFrom, nmcd.dwItemSpec, LVIR_BOUNDS, rc);

    RECT rcIcon;
    wxGetListCtrlItemRect(nmcd.hdr.hwndFrom, nmcd.dwItemSpec, LVIR_ICON, rcIcon);

    // exclude the icon part, neither the selection background nor focus rect
    // should cover it
    rc.left = rcIcon.right;

    return rc;
}

static
bool HandleSubItemPrepaint(LPNMLVCUSTOMDRAW pLVCD, HFONT hfont, int colCount)
{
    NMCUSTOMDRAW& nmcd = pLVCD->nmcd;

    HDC hdc = nmcd.hdc;
    HWND hwndList = nmcd.hdr.hwndFrom;
    const int col = pLVCD->iSubItem;
    const DWORD item = nmcd.dwItemSpec;

    // the font must be valid, otherwise we wouldn't be painting the item at all
    SelectInHDC selFont(hdc, hfont);

    // get the rectangle to paint
    RECT rc;
    wxGetListCtrlSubItemRect(hwndList, item, col, LVIR_BOUNDS, rc);
    if ( !col && colCount > 1 )
    {
        // ListView_GetSubItemRect() returns the entire item rect for 0th
        // subitem while we really need just the part for this column
        RECT rc2;
        wxGetListCtrlSubItemRect(hwndList, item, 1, LVIR_BOUNDS, rc2);
        rc.right = rc2.left;
        rc.left += 4;
    }
    else // not first subitem
    {
        rc.left += 6;
    }

    // get the image and text to draw
    wxChar text[512];
    LV_ITEM it;
    wxZeroMemory(it);
    it.mask = LVIF_TEXT | LVIF_IMAGE;
    it.iItem = item;
    it.iSubItem = col;
    it.pszText = text;
    it.cchTextMax = WXSIZEOF(text);
    ListView_GetItem(hwndList, &it);

    HIMAGELIST himl = ListView_GetImageList(hwndList, LVSIL_SMALL);
    if ( himl && ImageList_GetImageCount(himl) )
    {
        if ( it.iImage != -1 )
        {
            ImageList_Draw(himl, it.iImage, hdc, rc.left, rc.top,
                           nmcd.uItemState & CDIS_SELECTED ? ILD_SELECTED
                                                           : ILD_TRANSPARENT);
        }

        // notice that even if this item doesn't have any image, the list
        // control still leaves space for the image in the first column if the
        // image list is not empty (presumably so that items with and without
        // images align?)
        if ( it.iImage != -1 || it.iSubItem == 0 )
        {
            int wImage, hImage;
            ImageList_GetIconSize(himl, &wImage, &hImage);

            rc.left += wImage + 2;
        }
    }

    ::SetBkMode(hdc, TRANSPARENT);

    UINT fmt = DT_SINGLELINE |
#ifndef __WXWINCE__
               DT_WORD_ELLIPSIS |
#endif // __WXWINCE__
               DT_NOPREFIX |
               DT_VCENTER;

    LV_COLUMN lvCol;
    wxZeroMemory(lvCol);
    lvCol.mask = LVCF_FMT;
    if ( ListView_GetColumn(hwndList, col, &lvCol) )
    {
        switch ( lvCol.fmt & LVCFMT_JUSTIFYMASK )
        {
            case LVCFMT_LEFT:
                fmt |= DT_LEFT;
                break;

            case LVCFMT_CENTER:
                fmt |= DT_CENTER;
                break;

            case LVCFMT_RIGHT:
                fmt |= DT_RIGHT;
                break;
        }
    }
    //else: failed to get alignment, assume it's DT_LEFT (default)

    DrawText(hdc, text, -1, &rc, fmt);

    return true;
}

static void HandleItemPostpaint(NMCUSTOMDRAW nmcd)
{
    if ( nmcd.uItemState & CDIS_FOCUS )
    {
        RECT rc = GetCustomDrawnItemRect(nmcd);

        // don't use the provided HDC, it's in some strange state by now
        ::DrawFocusRect(WindowHDC(nmcd.hdr.hwndFrom), &rc);
    }
}

// pLVCD->clrText and clrTextBk should contain the colours to use
static void HandleItemPaint(LPNMLVCUSTOMDRAW pLVCD, HFONT hfont)
{
    NMCUSTOMDRAW& nmcd = pLVCD->nmcd; // just a shortcut

    const HWND hwndList = nmcd.hdr.hwndFrom;
    const int item = nmcd.dwItemSpec;

    // unfortunately we can't trust CDIS_SELECTED, it is often set even when
    // the item is not at all selected for some reason (comctl32 6), but we
    // also can't always trust ListView_GetItem() as it could return the old
    // item status if we're called just after the (de)selection, so remember
    // the last item to gain selection and also check for it here
    for ( int i = -1;; )
    {
        i = ListView_GetNextItem(hwndList, i, LVNI_SELECTED);
        if ( i == -1 )
        {
            nmcd.uItemState &= ~CDIS_SELECTED;
            break;
        }

        if ( i == item )
        {
            nmcd.uItemState |= CDIS_SELECTED;
            break;
        }
    }

    // same thing for CDIS_FOCUS (except simpler as there is only one of them)
    //
    // NB: cast is needed to work around the bug in mingw32 headers which don't
    //     have it inside ListView_GetNextItem() itself (unlike SDK ones)
    if ( ::GetFocus() == hwndList &&
            ListView_GetNextItem(
                hwndList, static_cast<WPARAM>(-1), LVNI_FOCUSED) == item )
    {
        nmcd.uItemState |= CDIS_FOCUS;
    }
    else
    {
        nmcd.uItemState &= ~CDIS_FOCUS;
    }

    if ( nmcd.uItemState & CDIS_SELECTED )
    {
        int syscolFg, syscolBg;
        if ( ::GetFocus() == hwndList )
        {
            syscolFg = COLOR_HIGHLIGHTTEXT;
            syscolBg = COLOR_HIGHLIGHT;
        }
        else // selected but unfocused
        {
            syscolFg = COLOR_WINDOWTEXT;
            syscolBg = COLOR_BTNFACE;

            // don't grey out the icon in this case neither
            nmcd.uItemState &= ~CDIS_SELECTED;
        }

        pLVCD->clrText = ::GetSysColor(syscolFg);
        pLVCD->clrTextBk = ::GetSysColor(syscolBg);
    }
    //else: not selected, use normal colours from pLVCD

    HDC hdc = nmcd.hdc;
    RECT rc = GetCustomDrawnItemRect(nmcd);

    ::SetTextColor(hdc, pLVCD->clrText);
    ::FillRect(hdc, &rc, AutoHBRUSH(pLVCD->clrTextBk));

    // we could use CDRF_NOTIFYSUBITEMDRAW here but it results in weird repaint
    // problems so just draw everything except the focus rect from here instead
    const int colCount = Header_GetItemCount(ListView_GetHeader(hwndList));
    for ( int col = 0; col < colCount; col++ )
    {
        pLVCD->iSubItem = col;
        HandleSubItemPrepaint(pLVCD, hfont, colCount);
    }

    HandleItemPostpaint(nmcd);
}

static WXLPARAM HandleItemPrepaint(wxListCtrl *listctrl,
                                   LPNMLVCUSTOMDRAW pLVCD,
                                   wxListItemAttr *attr)
{
    if ( !attr )
    {
        // nothing to do for this item
        return CDRF_DODEFAULT;
    }


    // set the colours to use for text drawing
    pLVCD->clrText = attr->HasTextColour()
                     ? wxColourToRGB(attr->GetTextColour())
                     : wxColourToRGB(listctrl->GetTextColour());
    pLVCD->clrTextBk = attr->HasBackgroundColour()
                       ? wxColourToRGB(attr->GetBackgroundColour())
                       : wxColourToRGB(listctrl->GetBackgroundColour());

    // select the font if non default one is specified
    if ( attr->HasFont() )
    {
        wxFont font = attr->GetFont();
        if ( font.GetEncoding() != wxFONTENCODING_SYSTEM )
        {
            // the standard control ignores the font encoding/charset, at least
            // with recent comctl32.dll versions (5 and 6, it uses to work with
            // 4.something) so we have to draw the item entirely ourselves in
            // this case
            HandleItemPaint(pLVCD, GetHfontOf(font));
            return CDRF_SKIPDEFAULT;
        }

        ::SelectObject(pLVCD->nmcd.hdc, GetHfontOf(font));

        return CDRF_NEWFONT;
    }

    return CDRF_DODEFAULT;
}

WXLPARAM wxListCtrl::OnCustomDraw(WXLPARAM lParam)
{
    LPNMLVCUSTOMDRAW pLVCD = (LPNMLVCUSTOMDRAW)lParam;
    NMCUSTOMDRAW& nmcd = pLVCD->nmcd;
    switch ( nmcd.dwDrawStage )
    {
        case CDDS_PREPAINT:
            // if we've got any items with non standard attributes,
            // notify us before painting each item
            //
            // for virtual controls, always suppose that we have attributes as
            // there is no way to check for this
            if ( IsVirtual() || m_hasAnyAttr )
                return CDRF_NOTIFYITEMDRAW;
            break;

        case CDDS_ITEMPREPAINT:
            // get a message for each subitem
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
            const int item = nmcd.dwItemSpec;
            const int column = pLVCD->iSubItem;

            // we get this message with item == 0 for an empty control, we
            // must ignore it as calling OnGetItemAttr() would be wrong
            if ( item < 0 || item >= GetItemCount() )
                break;
            // same for columns
            if ( column < 0 || column >= GetColumnCount() )
                break;

            return HandleItemPrepaint(this, pLVCD, DoGetItemColumnAttr(item, column));
    }

    return CDRF_DODEFAULT;
}

#endif // NM_CUSTOMDRAW supported

// Necessary for drawing hrules and vrules, if specified
void wxListCtrl::OnPaint(wxPaintEvent& event)
{
    const int itemCount = GetItemCount();
    const bool drawHRules = HasFlag(wxLC_HRULES);
    const bool drawVRules = HasFlag(wxLC_VRULES);

    if (!InReportView() || !(drawHRules || drawVRules) || !itemCount)
    {
        event.Skip();
        return;
    }

    wxPaintDC dc(this);

    wxListCtrlBase::OnPaint(event);

    // Reset the device origin since it may have been set
    dc.SetDeviceOrigin(0, 0);

    wxPen pen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT));
    dc.SetPen(pen);
    dc.SetBrush(* wxTRANSPARENT_BRUSH);

    wxSize clientSize = GetClientSize();
    wxRect itemRect;

    if (drawHRules)
    {
        const long top = GetTopItem();
        for ( int i = top; i < top + GetCountPerPage() + 1; i++ )
        {
            if (GetItemRect(i, itemRect))
            {
                int cy = itemRect.GetTop();
                if (i != 0) // Don't draw the first one
                {
                    dc.DrawLine(0, cy, clientSize.x, cy);
                }
                // Draw last line
                if (i == itemCount - 1)
                {
                    cy = itemRect.GetBottom();
                    dc.DrawLine(0, cy, clientSize.x, cy);
                    break;
                }
            }
        }
    }

    if (drawVRules)
    {
        wxRect firstItemRect;
        GetItemRect(0, firstItemRect);

        if (GetItemRect(itemCount - 1, itemRect))
        {
            // this is a fix for bug 673394: erase the pixels which we would
            // otherwise leave on the screen
            static const int gap = 2;
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(GetBackgroundColour()));
            dc.DrawRectangle(0, firstItemRect.GetY() - gap,
                             clientSize.GetWidth(), gap);

            dc.SetPen(pen);
            dc.SetBrush(*wxTRANSPARENT_BRUSH);

            const int numCols = GetColumnCount();
            wxVector<int> indexArray(numCols);
            if ( !ListView_GetColumnOrderArray(GetHwnd(),
                                               numCols,
                                               &indexArray[0]) )
            {
                wxFAIL_MSG( wxT("invalid column index array in OnPaint()") );
                return;
            }

            int x = itemRect.GetX();
            for (int col = 0; col < numCols; col++)
            {
                int colWidth = GetColumnWidth(indexArray[col]);
                x += colWidth ;
                dc.DrawLine(x-1, firstItemRect.GetY() - gap,
                            x-1, itemRect.GetBottom());
            }
        }
    }
}

void wxListCtrl::OnCharHook(wxKeyEvent& event)
{
    if ( GetEditControl() )
    {
        // We need to ensure that Escape is not stolen from the in-place editor
        // by the containing dialog.
        //
        // Notice that we don't have to care about Enter key here as we return
        // false from MSWShouldPreProcessMessage() for it.
        if ( event.GetKeyCode() == WXK_ESCAPE )
        {
            EndEditLabel(true /* cancel */);

            // Don't call Skip() below.
            return;
        }
    }

    event.Skip();
}

WXLRESULT
wxListCtrl::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    switch ( nMsg )
    {
#ifdef WM_PRINT
        case WM_PRINT:
            // we should bypass our own WM_PRINT handling as we don't handle
            // PRF_CHILDREN flag, so leave it to the native control itself
            return MSWDefWindowProc(nMsg, wParam, lParam);
#endif // WM_PRINT

        case WM_CONTEXTMENU:
            // because this message is propagated upwards the child-parent
            // chain, we get it for the right clicks on the header window but
            // this is confusing in wx as right clicking there already
            // generates a separate wxEVT_LIST_COL_RIGHT_CLICK event
            // so just ignore them
            if ( (HWND)wParam == ListView_GetHeader(GetHwnd()) )
                return 0;
            //else: break
    }

    return wxListCtrlBase::MSWWindowProc(nMsg, wParam, lParam);
}

// ----------------------------------------------------------------------------
// virtual list controls
// ----------------------------------------------------------------------------

wxString wxListCtrl::OnGetItemText(long WXUNUSED(item), long WXUNUSED(col)) const
{
    // this is a pure virtual function, in fact - which is not really pure
    // because the controls which are not virtual don't need to implement it
    wxFAIL_MSG( wxT("wxListCtrl::OnGetItemText not supposed to be called") );

    return wxEmptyString;
}

int wxListCtrl::OnGetItemImage(long WXUNUSED(item)) const
{
    wxCHECK_MSG(!GetImageList(wxIMAGE_LIST_SMALL),
                -1,
                wxT("List control has an image list, OnGetItemImage or OnGetItemColumnImage should be overridden."));
    return -1;
}

int wxListCtrl::OnGetItemColumnImage(long item, long column) const
{
    if (!column)
        return OnGetItemImage(item);

    return -1;
}

wxListItemAttr *wxListCtrl::DoGetItemColumnAttr(long item, long column) const
{
    if ( IsVirtual() )
        return OnGetItemColumnAttr(item, column);

    wxMSWListItemData * const data = MSWGetItemData(item);
    return data ? data->attr : NULL;
}

void wxListCtrl::SetItemCount(long count)
{
    wxASSERT_MSG( IsVirtual(), wxT("this is for virtual controls only") );

    if ( !::SendMessage(GetHwnd(), LVM_SETITEMCOUNT, (WPARAM)count,
                        LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL) )
    {
        wxLogLastError(wxT("ListView_SetItemCount"));
    }
    m_count = count;
    wxASSERT_MSG( m_count == ListView_GetItemCount(GetHwnd()),
                  wxT("m_count should match ListView_GetItemCount"));
}

void wxListCtrl::RefreshItem(long item)
{
    RefreshItems(item, item);
}

void wxListCtrl::RefreshItems(long itemFrom, long itemTo)
{
    ListView_RedrawItems(GetHwnd(), itemFrom, itemTo);
}

// ----------------------------------------------------------------------------
// wxWin <-> MSW items conversions
// ----------------------------------------------------------------------------

static void wxConvertFromMSWListItem(HWND hwndListCtrl,
                                     wxListItem& info,
                                     LV_ITEM& lvItem)
{
    wxMSWListItemData *internaldata =
        (wxMSWListItemData *) lvItem.lParam;

    if (internaldata)
        info.m_data = internaldata->lParam;

    info.m_mask = 0;
    info.m_state = 0;
    info.m_stateMask = 0;
    info.m_itemId = lvItem.iItem;

    long oldMask = lvItem.mask;

    bool needText = false;
    if (hwndListCtrl != 0)
    {
        if ( lvItem.mask & LVIF_TEXT )
            needText = false;
        else
            needText = true;

        if ( needText )
        {
            lvItem.pszText = new wxChar[513];
            lvItem.cchTextMax = 512;
        }
        lvItem.mask |= LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        ::SendMessage(hwndListCtrl, LVM_GETITEM, 0, (LPARAM)& lvItem);
    }

    if ( lvItem.mask & LVIF_STATE )
    {
        info.m_mask |= wxLIST_MASK_STATE;

        if ( lvItem.stateMask & LVIS_CUT)
        {
            info.m_stateMask |= wxLIST_STATE_CUT;
            if ( lvItem.state & LVIS_CUT )
                info.m_state |= wxLIST_STATE_CUT;
        }
        if ( lvItem.stateMask & LVIS_DROPHILITED)
        {
            info.m_stateMask |= wxLIST_STATE_DROPHILITED;
            if ( lvItem.state & LVIS_DROPHILITED )
                info.m_state |= wxLIST_STATE_DROPHILITED;
        }
        if ( lvItem.stateMask & LVIS_FOCUSED)
        {
            info.m_stateMask |= wxLIST_STATE_FOCUSED;
            if ( lvItem.state & LVIS_FOCUSED )
                info.m_state |= wxLIST_STATE_FOCUSED;
        }
        if ( lvItem.stateMask & LVIS_SELECTED)
        {
            info.m_stateMask |= wxLIST_STATE_SELECTED;
            if ( lvItem.state & LVIS_SELECTED )
                info.m_state |= wxLIST_STATE_SELECTED;
        }
    }

    if ( lvItem.mask & LVIF_TEXT )
    {
        info.m_mask |= wxLIST_MASK_TEXT;
        info.m_text = lvItem.pszText;
    }
    if ( lvItem.mask & LVIF_IMAGE )
    {
        info.m_mask |= wxLIST_MASK_IMAGE;
        info.m_image = lvItem.iImage;
    }
    if ( lvItem.mask & LVIF_PARAM )
        info.m_mask |= wxLIST_MASK_DATA;
    if ( lvItem.mask & LVIF_DI_SETITEM )
        info.m_mask |= wxLIST_SET_ITEM;
    info.m_col = lvItem.iSubItem;

    if (needText)
    {
        if (lvItem.pszText)
            delete[] lvItem.pszText;
    }
    lvItem.mask = oldMask;
}

static void wxConvertToMSWFlags(long state, long stateMask, LV_ITEM& lvItem)
{
    if (stateMask & wxLIST_STATE_CUT)
    {
        lvItem.stateMask |= LVIS_CUT;
        if (state & wxLIST_STATE_CUT)
            lvItem.state |= LVIS_CUT;
    }
    if (stateMask & wxLIST_STATE_DROPHILITED)
    {
        lvItem.stateMask |= LVIS_DROPHILITED;
        if (state & wxLIST_STATE_DROPHILITED)
            lvItem.state |= LVIS_DROPHILITED;
    }
    if (stateMask & wxLIST_STATE_FOCUSED)
    {
        lvItem.stateMask |= LVIS_FOCUSED;
        if (state & wxLIST_STATE_FOCUSED)
            lvItem.state |= LVIS_FOCUSED;
    }
    if (stateMask & wxLIST_STATE_SELECTED)
    {
        lvItem.stateMask |= LVIS_SELECTED;
        if (state & wxLIST_STATE_SELECTED)
            lvItem.state |= LVIS_SELECTED;
    }
}

static void wxConvertToMSWListItem(const wxListCtrl *ctrl,
                                   const wxListItem& info,
                                   LV_ITEM& lvItem)
{
    if ( ctrl->InReportView() )
    {
        wxASSERT_MSG( 0 <= info.m_col && info.m_col < ctrl->GetColumnCount(),
                      "wxListCtrl column index out of bounds" );
    }
    else // not in report view
    {
        wxASSERT_MSG( info.m_col == 0, "columns only exist in report view" );
    }

    lvItem.iItem = (int) info.m_itemId;

    lvItem.iImage = info.m_image;
    lvItem.stateMask = 0;
    lvItem.state = 0;
    lvItem.mask = 0;
    lvItem.iSubItem = info.m_col;

    if (info.m_mask & wxLIST_MASK_STATE)
    {
        lvItem.mask |= LVIF_STATE;

        wxConvertToMSWFlags(info.m_state, info.m_stateMask, lvItem);
    }

    if (info.m_mask & wxLIST_MASK_TEXT)
    {
        lvItem.mask |= LVIF_TEXT;
        if ( ctrl->HasFlag(wxLC_USER_TEXT) )
        {
            lvItem.pszText = LPSTR_TEXTCALLBACK;
        }
        else
        {
            // pszText is not const, hence the cast
            lvItem.pszText = wxMSW_CONV_LPTSTR(info.m_text);
            if ( lvItem.pszText )
                lvItem.cchTextMax = info.m_text.length();
            else
                lvItem.cchTextMax = 0;
        }
    }
    if (info.m_mask & wxLIST_MASK_IMAGE)
        lvItem.mask |= LVIF_IMAGE;
}

static void wxConvertToMSWListCol(HWND hwndList,
                                  int col,
                                  const wxListItem& item,
                                  LV_COLUMN& lvCol)
{
    wxZeroMemory(lvCol);

    if ( item.m_mask & wxLIST_MASK_TEXT )
    {
        lvCol.mask |= LVCF_TEXT;
        lvCol.pszText = wxMSW_CONV_LPTSTR(item.m_text);
    }

    if ( item.m_mask & wxLIST_MASK_FORMAT )
    {
        lvCol.mask |= LVCF_FMT;

        if ( item.m_format == wxLIST_FORMAT_LEFT )
            lvCol.fmt = LVCFMT_LEFT;
        else if ( item.m_format == wxLIST_FORMAT_RIGHT )
            lvCol.fmt = LVCFMT_RIGHT;
        else if ( item.m_format == wxLIST_FORMAT_CENTRE )
            lvCol.fmt = LVCFMT_CENTER;
    }

    if ( item.m_mask & wxLIST_MASK_WIDTH )
    {
        lvCol.mask |= LVCF_WIDTH;
        if ( item.m_width == wxLIST_AUTOSIZE)
            lvCol.cx = LVSCW_AUTOSIZE;
        else if ( item.m_width == wxLIST_AUTOSIZE_USEHEADER)
            lvCol.cx = LVSCW_AUTOSIZE_USEHEADER;
        else
            lvCol.cx = item.m_width;
    }

    // see comment at the end of wxListCtrl::GetColumn()
#ifdef NM_CUSTOMDRAW // _WIN32_IE >= 0x0300
    if ( item.m_mask & wxLIST_MASK_IMAGE )
    {
        if ( wxApp::GetComCtl32Version() >= 470 )
        {
            lvCol.mask |= LVCF_IMAGE;

            // we use LVCFMT_BITMAP_ON_RIGHT because the images on the right
            // seem to be generally nicer than on the left and the generic
            // version only draws them on the right (we don't have a flag to
            // specify the image location anyhow)
            //
            // we don't use LVCFMT_COL_HAS_IMAGES because it doesn't seem to
            // make any difference in my tests -- but maybe we should?
            if ( item.m_image != -1 )
            {
                // as we're going to overwrite the format field, get its
                // current value first -- unless we want to overwrite it anyhow
                if ( !(lvCol.mask & LVCF_FMT) )
                {
                    LV_COLUMN lvColOld;
                    wxZeroMemory(lvColOld);
                    lvColOld.mask = LVCF_FMT;
                    if ( ListView_GetColumn(hwndList, col, &lvColOld) )
                    {
                        lvCol.fmt = lvColOld.fmt;
                    }

                    lvCol.mask |= LVCF_FMT;
                }

                lvCol.fmt |= LVCFMT_BITMAP_ON_RIGHT | LVCFMT_IMAGE;
            }

            lvCol.iImage = item.m_image;
        }
        //else: it doesn't support item images anyhow
    }
#endif // _WIN32_IE >= 0x0300
}

#endif // wxUSE_LISTCTRL
