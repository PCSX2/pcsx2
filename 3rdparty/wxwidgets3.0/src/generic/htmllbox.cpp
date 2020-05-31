///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/htmllbox.cpp
// Purpose:     implementation of wxHtmlListBox
// Author:      Vadim Zeitlin
// Modified by:
// Created:     31.05.03
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
#endif //WX_PRECOMP

#if wxUSE_HTML

#include "wx/htmllbox.h"

#include "wx/html/htmlcell.h"
#include "wx/html/winpars.h"

// this hack forces the linker to always link in m_* files
#include "wx/html/forcelnk.h"
FORCE_WXHTML_MODULES()

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// small border always added to the cells:
static const wxCoord CELL_BORDER = 2;

const char wxHtmlListBoxNameStr[] = "htmlListBox";
const char wxSimpleHtmlListBoxNameStr[] = "simpleHtmlListBox";

// ============================================================================
// private classes
// ============================================================================

// ----------------------------------------------------------------------------
// wxHtmlListBoxCache
// ----------------------------------------------------------------------------

// this class is used by wxHtmlListBox to cache the parsed representation of
// the items to avoid doing it anew each time an item must be drawn
class wxHtmlListBoxCache
{
private:
    // invalidate a single item, used by Clear() and InvalidateRange()
    void InvalidateItem(size_t n)
    {
        m_items[n] = (size_t)-1;
        wxDELETE(m_cells[n]);
    }

public:
    wxHtmlListBoxCache()
    {
        for ( size_t n = 0; n < SIZE; n++ )
        {
            m_items[n] = (size_t)-1;
            m_cells[n] = NULL;
        }

        m_next = 0;
    }

    ~wxHtmlListBoxCache()
    {
        for ( size_t n = 0; n < SIZE; n++ )
        {
            delete m_cells[n];
        }
    }

    // completely invalidate the cache
    void Clear()
    {
        for ( size_t n = 0; n < SIZE; n++ )
        {
            InvalidateItem(n);
        }
    }

    // return the cached cell for this index or NULL if none
    wxHtmlCell *Get(size_t item) const
    {
        for ( size_t n = 0; n < SIZE; n++ )
        {
            if ( m_items[n] == item )
                return m_cells[n];
        }

        return NULL;
    }

    // returns true if we already have this item cached
    bool Has(size_t item) const { return Get(item) != NULL; }

    // ensure that the item is cached
    void Store(size_t item, wxHtmlCell *cell)
    {
        delete m_cells[m_next];
        m_cells[m_next] = cell;
        m_items[m_next] = item;

        // advance to the next item wrapping around if there are no more
        if ( ++m_next == SIZE )
            m_next = 0;
    }

    // forget the cached value of the item(s) between the given ones (inclusive)
    void InvalidateRange(size_t from, size_t to)
    {
        for ( size_t n = 0; n < SIZE; n++ )
        {
            if ( m_items[n] >= from && m_items[n] <= to )
            {
                InvalidateItem(n);
            }
        }
    }

private:
    // the max number of the items we cache
    enum { SIZE = 50 };

    // the index of the LRU (oldest) cell
    size_t m_next;

    // the parsed representation of the cached item or NULL
    wxHtmlCell *m_cells[SIZE];

    // the index of the currently cached item (only valid if m_cells != NULL)
    size_t m_items[SIZE];
};

// ----------------------------------------------------------------------------
// wxHtmlListBoxStyle
// ----------------------------------------------------------------------------

// just forward wxDefaultHtmlRenderingStyle callbacks to the main class so that
// they could be overridden by the user code
class wxHtmlListBoxStyle : public wxDefaultHtmlRenderingStyle
{
public:
    wxHtmlListBoxStyle(const wxHtmlListBox& hlbox) : m_hlbox(hlbox) { }

    virtual wxColour GetSelectedTextColour(const wxColour& colFg)
    {
        // by default wxHtmlListBox doesn't implement GetSelectedTextColour()
        // and returns wxNullColour from it, so use the default HTML colour for
        // selection
        wxColour col = m_hlbox.GetSelectedTextColour(colFg);
        if ( !col.IsOk() )
        {
            col = wxDefaultHtmlRenderingStyle::GetSelectedTextColour(colFg);
        }

        return col;
    }

    virtual wxColour GetSelectedTextBgColour(const wxColour& colBg)
    {
        wxColour col = m_hlbox.GetSelectedTextBgColour(colBg);
        if ( !col.IsOk() )
        {
            col = wxDefaultHtmlRenderingStyle::GetSelectedTextBgColour(colBg);
        }

        return col;
    }

private:
    const wxHtmlListBox& m_hlbox;

    wxDECLARE_NO_COPY_CLASS(wxHtmlListBoxStyle);
};

// ----------------------------------------------------------------------------
// event tables
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxHtmlListBox, wxVListBox)
    EVT_SIZE(wxHtmlListBox::OnSize)
    EVT_MOTION(wxHtmlListBox::OnMouseMove)
    EVT_LEFT_DOWN(wxHtmlListBox::OnLeftDown)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_ABSTRACT_CLASS(wxHtmlListBox, wxVListBox)


// ----------------------------------------------------------------------------
// wxHtmlListBox creation
// ----------------------------------------------------------------------------

wxHtmlListBox::wxHtmlListBox()
    : wxHtmlWindowMouseHelper(this)
{
    Init();
}

// normal constructor which calls Create() internally
wxHtmlListBox::wxHtmlListBox(wxWindow *parent,
                             wxWindowID id,
                             const wxPoint& pos,
                             const wxSize& size,
                             long style,
                             const wxString& name)
    : wxHtmlWindowMouseHelper(this)
{
    Init();

    (void)Create(parent, id, pos, size, style, name);
}

void wxHtmlListBox::Init()
{
    m_htmlParser = NULL;
    m_htmlRendStyle = new wxHtmlListBoxStyle(*this);
    m_cache = new wxHtmlListBoxCache;
}

bool wxHtmlListBox::Create(wxWindow *parent,
                           wxWindowID id,
                           const wxPoint& pos,
                           const wxSize& size,
                           long style,
                           const wxString& name)
{
    return wxVListBox::Create(parent, id, pos, size, style, name);
}

wxHtmlListBox::~wxHtmlListBox()
{
    delete m_cache;

    if ( m_htmlParser )
    {
        delete m_htmlParser->GetDC();
        delete m_htmlParser;
    }

    delete m_htmlRendStyle;
}

// ----------------------------------------------------------------------------
// wxHtmlListBox appearance
// ----------------------------------------------------------------------------

wxColour
wxHtmlListBox::GetSelectedTextColour(const wxColour& WXUNUSED(colFg)) const
{
    return wxNullColour;
}

wxColour
wxHtmlListBox::GetSelectedTextBgColour(const wxColour& WXUNUSED(colBg)) const
{
    return GetSelectionBackground();
}

// ----------------------------------------------------------------------------
// wxHtmlListBox items markup
// ----------------------------------------------------------------------------

wxString wxHtmlListBox::OnGetItemMarkup(size_t n) const
{
    // we don't even need to wrap the value returned by OnGetItem() inside
    // "<html><body>" and "</body></html>" because wxHTML can parse it even
    // without these tags
    return OnGetItem(n);
}

// ----------------------------------------------------------------------------
// wxHtmlListBox cache handling
// ----------------------------------------------------------------------------

void wxHtmlListBox::CacheItem(size_t n) const
{
    if ( !m_cache->Has(n) )
    {
        if ( !m_htmlParser )
        {
            wxHtmlListBox *self = wxConstCast(this, wxHtmlListBox);

            self->m_htmlParser = new wxHtmlWinParser(self);
            m_htmlParser->SetDC(new wxClientDC(self));
            m_htmlParser->SetFS(&self->m_filesystem);
#if !wxUSE_UNICODE
            if (GetFont().IsOk())
                m_htmlParser->SetInputEncoding(GetFont().GetEncoding());
#endif
            // use system's default GUI font by default:
            m_htmlParser->SetStandardFonts();
        }

        wxHtmlContainerCell *cell = (wxHtmlContainerCell *)m_htmlParser->
                Parse(OnGetItemMarkup(n));
        wxCHECK_RET( cell, wxT("wxHtmlParser::Parse() returned NULL?") );

        // set the cell's ID to item's index so that CellCoordsToPhysical()
        // can quickly find the item:
        cell->SetId(wxString::Format(wxT("%lu"), (unsigned long)n));

        cell->Layout(GetClientSize().x - 2*GetMargins().x);

        m_cache->Store(n, cell);
    }
}

void wxHtmlListBox::OnSize(wxSizeEvent& event)
{
    // we need to relayout all the cached cells
    m_cache->Clear();

    event.Skip();
}

void wxHtmlListBox::RefreshRow(size_t line)
{
    m_cache->InvalidateRange(line, line);

    wxVListBox::RefreshRow(line);
}

void wxHtmlListBox::RefreshRows(size_t from, size_t to)
{
    m_cache->InvalidateRange(from, to);

    wxVListBox::RefreshRows(from, to);
}

void wxHtmlListBox::RefreshAll()
{
    m_cache->Clear();

    wxVListBox::RefreshAll();
}

void wxHtmlListBox::SetItemCount(size_t count)
{
    // the items are going to change, forget the old ones
    m_cache->Clear();

    wxVListBox::SetItemCount(count);
}

// ----------------------------------------------------------------------------
// wxHtmlListBox implementation of wxVListBox pure virtuals
// ----------------------------------------------------------------------------

void
wxHtmlListBox::OnDrawBackground(wxDC& dc, const wxRect& rect, size_t n) const
{
    if ( IsSelected(n) )
    {
        if ( DoDrawSolidBackground
             (
                GetSelectedTextBgColour(GetBackgroundColour()),
                dc,
                rect,
                n
             ) )
        {
            return;
        }
        //else: no custom selection background colour, use base class version
    }

    wxVListBox::OnDrawBackground(dc, rect, n);
}

void wxHtmlListBox::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const
{
    CacheItem(n);

    wxHtmlCell *cell = m_cache->Get(n);
    wxCHECK_RET( cell, wxT("this cell should be cached!") );

    wxHtmlRenderingInfo htmlRendInfo;

    // draw the selected cell in selected state ourselves if we're using custom
    // colours (to test for this, check the callbacks by passing them any dummy
    // (but valid, to avoid asserts) colour):
    if ( IsSelected(n) &&
            (GetSelectedTextColour(*wxBLACK).IsOk() ||
             GetSelectedTextBgColour(*wxWHITE).IsOk()) )
    {
        wxHtmlSelection htmlSel;
        htmlSel.Set(wxPoint(0,0), cell, wxPoint(INT_MAX, INT_MAX), cell);
        htmlRendInfo.SetSelection(&htmlSel);
        htmlRendInfo.SetStyle(m_htmlRendStyle);
        htmlRendInfo.GetState().SetSelectionState(wxHTML_SEL_IN);
    }
    //else: normal item or selected item with default colours, its background
    //      was already taken care of in the base class

    // note that we can't stop drawing exactly at the window boundary as then
    // even the visible cells part could be not drawn, so always draw the
    // entire cell
    cell->Draw(dc,
               rect.x + CELL_BORDER, rect.y + CELL_BORDER,
               0, INT_MAX, htmlRendInfo);
}

wxCoord wxHtmlListBox::OnMeasureItem(size_t n) const
{
    CacheItem(n);

    wxHtmlCell *cell = m_cache->Get(n);
    wxCHECK_MSG( cell, 0, wxT("this cell should be cached!") );

    return cell->GetHeight() + cell->GetDescent() + 4;
}

// ----------------------------------------------------------------------------
// wxHtmlListBox implementation of wxHtmlListBoxWinInterface
// ----------------------------------------------------------------------------

void wxHtmlListBox::SetHTMLWindowTitle(const wxString& WXUNUSED(title))
{
    // nothing to do
}

void wxHtmlListBox::OnHTMLLinkClicked(const wxHtmlLinkInfo& link)
{
    OnLinkClicked(GetItemForCell(link.GetHtmlCell()), link);
}

void wxHtmlListBox::OnLinkClicked(size_t WXUNUSED(n),
                                  const wxHtmlLinkInfo& link)
{
    wxHtmlLinkEvent event(GetId(), link);
    GetEventHandler()->ProcessEvent(event);
}

wxHtmlOpeningStatus
wxHtmlListBox::OnHTMLOpeningURL(wxHtmlURLType WXUNUSED(type),
                                const wxString& WXUNUSED(url),
                                wxString *WXUNUSED(redirect)) const
{
    return wxHTML_OPEN;
}

wxPoint wxHtmlListBox::HTMLCoordsToWindow(wxHtmlCell *cell,
                                          const wxPoint& pos) const
{
    return CellCoordsToPhysical(pos, cell);
}

wxWindow* wxHtmlListBox::GetHTMLWindow() { return this; }

wxColour wxHtmlListBox::GetHTMLBackgroundColour() const
{
    return GetBackgroundColour();
}

void wxHtmlListBox::SetHTMLBackgroundColour(const wxColour& WXUNUSED(clr))
{
    // nothing to do
}

void wxHtmlListBox::SetHTMLBackgroundImage(const wxBitmap& WXUNUSED(bmpBg))
{
    // nothing to do
}

void wxHtmlListBox::SetHTMLStatusText(const wxString& WXUNUSED(text))
{
    // nothing to do
}

wxCursor wxHtmlListBox::GetHTMLCursor(HTMLCursor type) const
{
    // we don't want to show text selection cursor in listboxes
    if (type == HTMLCursor_Text)
        return wxHtmlWindow::GetDefaultHTMLCursor(HTMLCursor_Default);

    // in all other cases, use the same cursor as wxHtmlWindow:
    return wxHtmlWindow::GetDefaultHTMLCursor(type);
}

// ----------------------------------------------------------------------------
// wxHtmlListBox handling of HTML links
// ----------------------------------------------------------------------------

wxPoint wxHtmlListBox::GetRootCellCoords(size_t n) const
{
    wxPoint pos(CELL_BORDER, CELL_BORDER);
    pos += GetMargins();
    pos.y += GetRowsHeight(GetVisibleBegin(), n);
    return pos;
}

bool wxHtmlListBox::PhysicalCoordsToCell(wxPoint& pos, wxHtmlCell*& cell) const
{
    int n = VirtualHitTest(pos.y);
    if ( n == wxNOT_FOUND )
        return false;

    // convert mouse coordinates to coords relative to item's wxHtmlCell:
    pos -= GetRootCellCoords(n);

    CacheItem(n);
    cell = m_cache->Get(n);

    return true;
}

size_t wxHtmlListBox::GetItemForCell(const wxHtmlCell *cell) const
{
    wxCHECK_MSG( cell, 0, wxT("no cell") );

    cell = cell->GetRootCell();

    wxCHECK_MSG( cell, 0, wxT("no root cell") );

    // the cell's ID contains item index, see CacheItem():
    unsigned long n;
    if ( !cell->GetId().ToULong(&n) )
    {
        wxFAIL_MSG( wxT("unexpected root cell's ID") );
        return 0;
    }

    return n;
}

wxPoint
wxHtmlListBox::CellCoordsToPhysical(const wxPoint& pos, wxHtmlCell *cell) const
{
    return pos + GetRootCellCoords(GetItemForCell(cell));
}

void wxHtmlListBox::OnInternalIdle()
{
    wxVListBox::OnInternalIdle();

    if ( wxHtmlWindowMouseHelper::DidMouseMove() )
    {
        wxPoint pos = ScreenToClient(wxGetMousePosition());
        wxHtmlCell *cell;

        if ( !PhysicalCoordsToCell(pos, cell) )
            return;

        wxHtmlWindowMouseHelper::HandleIdle(cell, pos);
    }
}

void wxHtmlListBox::OnMouseMove(wxMouseEvent& event)
{
    wxHtmlWindowMouseHelper::HandleMouseMoved();
    event.Skip();
}

void wxHtmlListBox::OnLeftDown(wxMouseEvent& event)
{
    wxPoint pos = event.GetPosition();
    wxHtmlCell *cell;

    if ( !PhysicalCoordsToCell(pos, cell) )
    {
        event.Skip();
        return;
    }

    if ( !wxHtmlWindowMouseHelper::HandleMouseClick(cell, pos, event) )
    {
        // no link was clicked, so let the listbox code handle the click (e.g.
        // by selecting another item in the list):
        event.Skip();
    }
}


// ----------------------------------------------------------------------------
// wxSimpleHtmlListBox
// ----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxSimpleHtmlListBox, wxHtmlListBox)


bool wxSimpleHtmlListBox::Create(wxWindow *parent, wxWindowID id,
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 int n, const wxString choices[],
                                 long style,
                                 const wxValidator& wxVALIDATOR_PARAM(validator),
                                 const wxString& name)
{
    if (!wxHtmlListBox::Create(parent, id, pos, size, style, name))
        return false;

#if wxUSE_VALIDATORS
    SetValidator(validator);
#endif

    Append(n, choices);

    return true;
}

bool wxSimpleHtmlListBox::Create(wxWindow *parent, wxWindowID id,
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 const wxArrayString& choices,
                                 long style,
                                 const wxValidator& wxVALIDATOR_PARAM(validator),
                                 const wxString& name)
{
    if (!wxHtmlListBox::Create(parent, id, pos, size, style, name))
        return false;

#if wxUSE_VALIDATORS
    SetValidator(validator);
#endif

    Append(choices);

    return true;
}

wxSimpleHtmlListBox::~wxSimpleHtmlListBox()
{
    wxItemContainer::Clear();
}

void wxSimpleHtmlListBox::DoClear()
{
    wxASSERT(m_items.GetCount() == m_HTMLclientData.GetCount());

    m_items.Clear();
    m_HTMLclientData.Clear();

    UpdateCount();
}

void wxSimpleHtmlListBox::Clear()
{
    DoClear();
}

void wxSimpleHtmlListBox::DoDeleteOneItem(unsigned int n)
{
    m_items.RemoveAt(n);

    m_HTMLclientData.RemoveAt(n);

    UpdateCount();
}

int wxSimpleHtmlListBox::DoInsertItems(const wxArrayStringsAdapter& items,
                                       unsigned int pos,
                                       void **clientData,
                                       wxClientDataType type)
{
    const unsigned int count = items.GetCount();

    m_items.Insert(wxEmptyString, pos, count);
    m_HTMLclientData.Insert(NULL, pos, count);

    for ( unsigned int i = 0; i < count; ++i, ++pos )
    {
        m_items[pos] = items[i];
        AssignNewItemClientData(pos, clientData, i, type);
    }

    UpdateCount();

    return pos - 1;
}

void wxSimpleHtmlListBox::SetString(unsigned int n, const wxString& s)
{
    wxCHECK_RET( IsValid(n),
                 wxT("invalid index in wxSimpleHtmlListBox::SetString") );

    m_items[n]=s;
    RefreshRow(n);
}

wxString wxSimpleHtmlListBox::GetString(unsigned int n) const
{
    wxCHECK_MSG( IsValid(n), wxEmptyString,
                 wxT("invalid index in wxSimpleHtmlListBox::GetString") );

    return m_items[n];
}

void wxSimpleHtmlListBox::UpdateCount()
{
    wxASSERT(m_items.GetCount() == m_HTMLclientData.GetCount());
    wxHtmlListBox::SetItemCount(m_items.GetCount());

    // very small optimization: if you need to add lot of items to
    // a wxSimpleHtmlListBox be sure to use the
    // wxSimpleHtmlListBox::Append(const wxArrayString&) method instead!
    if (!this->IsFrozen())
        RefreshAll();
}

#endif // wxUSE_HTML
