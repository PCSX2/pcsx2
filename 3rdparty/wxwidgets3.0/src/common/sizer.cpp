/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/sizer.cpp
// Purpose:     provide new wxSizer class for layout
// Author:      Robert Roebling and Robin Dunn, contributions by
//              Dirk Holtwick, Ron Lee
// Modified by: Ron Lee
// Created:
// Copyright:   (c) Robin Dunn, Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/sizer.h"
#include "wx/private/flagscheck.h"

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/math.h"
    #include "wx/utils.h"
    #include "wx/settings.h"
    #include "wx/button.h"
    #include "wx/statbox.h"
    #include "wx/toplevel.h"
#endif // WX_PRECOMP

#include "wx/display.h"
#include "wx/vector.h"
#include "wx/listimpl.cpp"


//---------------------------------------------------------------------------

IMPLEMENT_CLASS(wxSizerItem, wxObject)
IMPLEMENT_CLASS(wxSizer, wxObject)
IMPLEMENT_CLASS(wxGridSizer, wxSizer)
IMPLEMENT_CLASS(wxFlexGridSizer, wxGridSizer)
IMPLEMENT_CLASS(wxBoxSizer, wxSizer)
#if wxUSE_STATBOX
IMPLEMENT_CLASS(wxStaticBoxSizer, wxBoxSizer)
#endif
#if wxUSE_BUTTON
IMPLEMENT_CLASS(wxStdDialogButtonSizer, wxBoxSizer)
#endif

WX_DEFINE_EXPORTED_LIST( wxSizerItemList )

/*
    TODO PROPERTIES
      sizeritem
        object
        object_ref
          minsize
          option
          flag
          border
     spacer
        option
        flag
        borfder
    boxsizer
       orient
    staticboxsizer
       orient
       label
    gridsizer
       rows
       cols
       vgap
       hgap
    flexgridsizer
       rows
       cols
       vgap
       hgap
       growablerows
       growablecols
    minsize
*/

// ----------------------------------------------------------------------------
// wxSizerItem
// ----------------------------------------------------------------------------

// check for flags conflicts
static const int SIZER_FLAGS_MASK =
    wxADD_FLAG(wxCENTRE,
    wxADD_FLAG(wxHORIZONTAL,
    wxADD_FLAG(wxVERTICAL,
    wxADD_FLAG(wxLEFT,
    wxADD_FLAG(wxRIGHT,
    wxADD_FLAG(wxUP,
    wxADD_FLAG(wxDOWN,
    wxADD_FLAG(wxALIGN_NOT,
    wxADD_FLAG(wxALIGN_CENTER_HORIZONTAL,
    wxADD_FLAG(wxALIGN_RIGHT,
    wxADD_FLAG(wxALIGN_BOTTOM,
    wxADD_FLAG(wxALIGN_CENTER_VERTICAL,
    wxADD_FLAG(wxFIXED_MINSIZE,
    wxADD_FLAG(wxRESERVE_SPACE_EVEN_IF_HIDDEN,
    wxADD_FLAG(wxSTRETCH_NOT,
    wxADD_FLAG(wxSHRINK,
    wxADD_FLAG(wxGROW,
    wxADD_FLAG(wxSHAPED,
    0))))))))))))))))));

#define ASSERT_VALID_SIZER_FLAGS(f)  wxASSERT_VALID_FLAGS(f, SIZER_FLAGS_MASK)


void wxSizerItem::Init(const wxSizerFlags& flags)
{
    Init();

    m_proportion = flags.GetProportion();
    m_flag = flags.GetFlags();
    m_border = flags.GetBorderInPixels();

    ASSERT_VALID_SIZER_FLAGS( m_flag );
}

wxSizerItem::wxSizerItem()
{
    Init();

    m_proportion = 0;
    m_border = 0;
    m_flag = 0;
    m_id = wxID_NONE;
}

// window item
void wxSizerItem::DoSetWindow(wxWindow *window)
{
    wxCHECK_RET( window, wxT("NULL window in wxSizerItem::SetWindow()") );

    m_kind = Item_Window;
    m_window = window;

    // window doesn't become smaller than its initial size, whatever happens
    m_minSize = window->GetSize();

    if ( m_flag & wxFIXED_MINSIZE )
        window->SetMinSize(m_minSize);

    // aspect ratio calculated from initial size
    SetRatio(m_minSize);
}

wxSizerItem::wxSizerItem(wxWindow *window,
                         int proportion,
                         int flag,
                         int border,
                         wxObject* userData)
           : m_kind(Item_None),
             m_proportion(proportion),
             m_border(border),
             m_flag(flag),
             m_id(wxID_NONE),
             m_userData(userData)
{
    ASSERT_VALID_SIZER_FLAGS( m_flag );

    DoSetWindow(window);
}

// sizer item
void wxSizerItem::DoSetSizer(wxSizer *sizer)
{
    m_kind = Item_Sizer;
    m_sizer = sizer;
}

wxSizerItem::wxSizerItem(wxSizer *sizer,
                         int proportion,
                         int flag,
                         int border,
                         wxObject* userData)
           : m_kind(Item_None),
             m_sizer(NULL),
             m_proportion(proportion),
             m_border(border),
             m_flag(flag),
             m_id(wxID_NONE),
             m_ratio(0.0),
             m_userData(userData)
{
    ASSERT_VALID_SIZER_FLAGS( m_flag );

    DoSetSizer(sizer);

    // m_minSize is set later
}

// spacer item
void wxSizerItem::DoSetSpacer(const wxSize& size)
{
    m_kind = Item_Spacer;
    m_spacer = new wxSizerSpacer(size);
    m_minSize = size;
    SetRatio(size);
}

wxSize wxSizerItem::AddBorderToSize(const wxSize& size) const
{
    wxSize result = size;

    // Notice that we shouldn't modify the unspecified component(s) of the
    // size, it's perfectly valid to have either min or max size specified in
    // one direction only and it shouldn't be applied in the other one then.

    if ( result.x != wxDefaultCoord )
    {
        if (m_flag & wxWEST)
            result.x += m_border;
        if (m_flag & wxEAST)
            result.x += m_border;
    }

    if ( result.y != wxDefaultCoord )
    {
        if (m_flag & wxNORTH)
            result.y += m_border;
        if (m_flag & wxSOUTH)
            result.y += m_border;
    }

    return result;
}

wxSizerItem::wxSizerItem(int width,
                         int height,
                         int proportion,
                         int flag,
                         int border,
                         wxObject* userData)
           : m_kind(Item_None),
             m_sizer(NULL),
             m_minSize(width, height), // minimal size is the initial size
             m_proportion(proportion),
             m_border(border),
             m_flag(flag),
             m_id(wxID_NONE),
             m_userData(userData)
{
    ASSERT_VALID_SIZER_FLAGS( m_flag );

    DoSetSpacer(wxSize(width, height));
}

wxSizerItem::~wxSizerItem()
{
    delete m_userData;
    Free();
}

void wxSizerItem::Free()
{
    switch ( m_kind )
    {
        case Item_None:
            break;

        case Item_Window:
            m_window->SetContainingSizer(NULL);
            break;

        case Item_Sizer:
            delete m_sizer;
            break;

        case Item_Spacer:
            delete m_spacer;
            break;

        case Item_Max:
        default:
            wxFAIL_MSG( wxT("unexpected wxSizerItem::m_kind") );
    }

    m_kind = Item_None;
}

wxSize wxSizerItem::GetSpacer() const
{
    wxSize size;
    if ( m_kind == Item_Spacer )
        size = m_spacer->GetSize();

    return size;
}


wxSize wxSizerItem::GetSize() const
{
    wxSize ret;
    switch ( m_kind )
    {
        case Item_None:
            break;

        case Item_Window:
            ret = m_window->GetSize();
            break;

        case Item_Sizer:
            ret = m_sizer->GetSize();
            break;

        case Item_Spacer:
            ret = m_spacer->GetSize();
            break;

        case Item_Max:
        default:
            wxFAIL_MSG( wxT("unexpected wxSizerItem::m_kind") );
    }

    if (m_flag & wxWEST)
        ret.x += m_border;
    if (m_flag & wxEAST)
        ret.x += m_border;
    if (m_flag & wxNORTH)
        ret.y += m_border;
    if (m_flag & wxSOUTH)
        ret.y += m_border;

    return ret;
}

bool wxSizerItem::InformFirstDirection(int direction, int size, int availableOtherDir)
{
    // The size that come here will be including borders. Child items should get it
    // without borders.
    if( size>0 )
    {
        if( direction==wxHORIZONTAL )
        {
            if (m_flag & wxWEST)
                size -= m_border;
            if (m_flag & wxEAST)
                size -= m_border;
        }
        else if( direction==wxVERTICAL )
        {
            if (m_flag & wxNORTH)
                size -= m_border;
            if (m_flag & wxSOUTH)
                size -= m_border;
        }
    }

    bool didUse = false;
    // Pass the information along to the held object
    if (IsSizer())
    {
        didUse = GetSizer()->InformFirstDirection(direction,size,availableOtherDir);
        if (didUse)
            m_minSize = GetSizer()->CalcMin();
    }
    else if (IsWindow())
    {
        didUse =  GetWindow()->InformFirstDirection(direction,size,availableOtherDir);
        if (didUse)
            m_minSize = m_window->GetEffectiveMinSize();

        // This information is useful for items with wxSHAPED flag, since
        // we can request an optimal min size for such an item. Even if
        // we overwrite the m_minSize member here, we can read it back from
        // the owned window (happens automatically).
        if( (m_flag & wxSHAPED) && (m_flag & wxEXPAND) && direction )
        {
            if( !wxIsNullDouble(m_ratio) )
            {
                wxCHECK_MSG( (m_proportion==0), false, wxT("Shaped item, non-zero proportion in wxSizerItem::InformFirstDirection()") );
                if( direction==wxHORIZONTAL && !wxIsNullDouble(m_ratio) )
                {
                    // Clip size so that we don't take too much
                    if( availableOtherDir>=0 && int(size/m_ratio)-m_minSize.y>availableOtherDir )
                        size = int((availableOtherDir+m_minSize.y)*m_ratio);
                    m_minSize = wxSize(size,int(size/m_ratio));
                }
                else if( direction==wxVERTICAL )
                {
                    // Clip size so that we don't take too much
                    if( availableOtherDir>=0 && int(size*m_ratio)-m_minSize.x>availableOtherDir )
                        size = int((availableOtherDir+m_minSize.x)/m_ratio);
                    m_minSize = wxSize(int(size*m_ratio),size);
                }
                didUse = true;
            }
        }
    }

    return didUse;
}

wxSize wxSizerItem::CalcMin()
{
    if (IsSizer())
    {
        m_minSize = m_sizer->GetMinSize();

        // if we have to preserve aspect ratio _AND_ this is
        // the first-time calculation, consider ret to be initial size
        if ( (m_flag & wxSHAPED) && wxIsNullDouble(m_ratio) )
            SetRatio(m_minSize);
    }
    else if ( IsWindow() )
    {
        // Since the size of the window may change during runtime, we
        // should use the current minimal/best size.
        m_minSize = m_window->GetEffectiveMinSize();
    }

    return GetMinSizeWithBorder();
}

wxSize wxSizerItem::GetMinSizeWithBorder() const
{
    return AddBorderToSize(m_minSize);
}

wxSize wxSizerItem::GetMaxSizeWithBorder() const
{
    return AddBorderToSize(GetMaxSize());
}

void wxSizerItem::SetDimension( const wxPoint& pos_, const wxSize& size_ )
{
    wxPoint pos = pos_;
    wxSize size = size_;
    if (m_flag & wxSHAPED)
    {
        // adjust aspect ratio
        int rwidth = (int) (size.y * m_ratio);
        if (rwidth > size.x)
        {
            // fit horizontally
            int rheight = (int) (size.x / m_ratio);
            // add vertical space
            if (m_flag & wxALIGN_CENTER_VERTICAL)
                pos.y += (size.y - rheight) / 2;
            else if (m_flag & wxALIGN_BOTTOM)
                pos.y += (size.y - rheight);
            // use reduced dimensions
            size.y =rheight;
        }
        else if (rwidth < size.x)
        {
            // add horizontal space
            if (m_flag & wxALIGN_CENTER_HORIZONTAL)
                pos.x += (size.x - rwidth) / 2;
            else if (m_flag & wxALIGN_RIGHT)
                pos.x += (size.x - rwidth);
            size.x = rwidth;
        }
    }

    // This is what GetPosition() returns. Since we calculate
    // borders afterwards, GetPosition() will be the left/top
    // corner of the surrounding border.
    m_pos = pos;

    if (m_flag & wxWEST)
    {
        pos.x += m_border;
        size.x -= m_border;
    }
    if (m_flag & wxEAST)
    {
        size.x -= m_border;
    }
    if (m_flag & wxNORTH)
    {
        pos.y += m_border;
        size.y -= m_border;
    }
    if (m_flag & wxSOUTH)
    {
        size.y -= m_border;
    }

    if (size.x < 0)
        size.x = 0;
    if (size.y < 0)
        size.y = 0;

    m_rect = wxRect(pos, size);

    switch ( m_kind )
    {
        case Item_None:
            wxFAIL_MSG( wxT("can't set size of uninitialized sizer item") );
            break;

        case Item_Window:
        {
            // Use wxSIZE_FORCE_EVENT here since a sizer item might
            // have changed alignment or some other property which would
            // not change the size of the window. In such a case, no
            // wxSizeEvent would normally be generated and thus the
            // control wouldn't get laid out correctly here.
#if 1
            m_window->SetSize(pos.x, pos.y, size.x, size.y,
                              wxSIZE_ALLOW_MINUS_ONE|wxSIZE_FORCE_EVENT );
#else
            m_window->SetSize(pos.x, pos.y, size.x, size.y,
                              wxSIZE_ALLOW_MINUS_ONE );
#endif
            break;
        }
        case Item_Sizer:
            m_sizer->SetDimension(pos, size);
            break;

        case Item_Spacer:
            m_spacer->SetSize(size);
            break;

        case Item_Max:
        default:
            wxFAIL_MSG( wxT("unexpected wxSizerItem::m_kind") );
    }
}

void wxSizerItem::DeleteWindows()
{
    switch ( m_kind )
    {
        case Item_None:
        case Item_Spacer:
            break;

        case Item_Window:
            //We are deleting the window from this sizer - normally
            //the window destroys the sizer associated with it,
            //which might destroy this, which we don't want
            m_window->SetContainingSizer(NULL);
            m_window->Destroy();
            //Putting this after the switch will result in a spacer
            //not being deleted properly on destruction
            m_kind = Item_None;
            break;

        case Item_Sizer:
            m_sizer->DeleteWindows();
            break;

        case Item_Max:
        default:
            wxFAIL_MSG( wxT("unexpected wxSizerItem::m_kind") );
    }

}

void wxSizerItem::Show( bool show )
{
    switch ( m_kind )
    {
        case Item_None:
            wxFAIL_MSG( wxT("can't show uninitialized sizer item") );
            break;

        case Item_Window:
            m_window->Show(show);
            break;

        case Item_Sizer:
            m_sizer->Show(show);
            break;

        case Item_Spacer:
            m_spacer->Show(show);
            break;

        case Item_Max:
        default:
            wxFAIL_MSG( wxT("unexpected wxSizerItem::m_kind") );
    }
}

bool wxSizerItem::IsShown() const
{
    if ( m_flag & wxRESERVE_SPACE_EVEN_IF_HIDDEN )
        return true;

    switch ( m_kind )
    {
        case Item_None:
            // we may be called from CalcMin(), just return false so that we're
            // not used
            break;

        case Item_Window:
            return m_window->IsShown();

        case Item_Sizer:
            // arbitrarily decide that if at least one of our elements is
            // shown, so are we (this arbitrariness is the reason for
            // deprecating this function)
            return m_sizer->AreAnyItemsShown();

        case Item_Spacer:
            return m_spacer->IsShown();

        case Item_Max:
        default:
            wxFAIL_MSG( wxT("unexpected wxSizerItem::m_kind") );
    }

    return false;
}

#if WXWIN_COMPATIBILITY_2_6
void wxSizerItem::SetOption( int option )
{
    SetProportion( option );
}

int wxSizerItem::GetOption() const
{
    return GetProportion();
}
#endif // WXWIN_COMPATIBILITY_2_6


//---------------------------------------------------------------------------
// wxSizer
//---------------------------------------------------------------------------

wxSizer::~wxSizer()
{
    WX_CLEAR_LIST(wxSizerItemList, m_children);
}

wxSizerItem* wxSizer::DoInsert( size_t index, wxSizerItem *item )
{
    m_children.Insert( index, item );

    if ( item->GetWindow() )
        item->GetWindow()->SetContainingSizer( this );

    if ( item->GetSizer() )
        item->GetSizer()->SetContainingWindow( m_containingWindow );

    return item;
}

void wxSizer::SetContainingWindow(wxWindow *win)
{
    if ( win == m_containingWindow )
        return;

    m_containingWindow = win;

    // set the same window for all nested sizers as well, they also are in the
    // same window
    for ( wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
          node;
          node = node->GetNext() )
    {
        wxSizerItem *const item = node->GetData();
        wxSizer *const sizer = item->GetSizer();

        if ( sizer )
        {
            sizer->SetContainingWindow(win);
        }
    }
}

#if WXWIN_COMPATIBILITY_2_6
bool wxSizer::Remove( wxWindow *window )
{
    return Detach( window );
}
#endif // WXWIN_COMPATIBILITY_2_6

bool wxSizer::Remove( wxSizer *sizer )
{
    wxASSERT_MSG( sizer, wxT("Removing NULL sizer") );

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetSizer() == sizer)
        {
            delete item;
            m_children.Erase( node );
            return true;
        }

        node = node->GetNext();
    }

    return false;
}

bool wxSizer::Remove( int index )
{
    wxCHECK_MSG( index >= 0 && (size_t)index < m_children.GetCount(),
                 false,
                 wxT("Remove index is out of range") );

    wxSizerItemList::compatibility_iterator node = m_children.Item( index );

    wxCHECK_MSG( node, false, wxT("Failed to find child node") );

    delete node->GetData();
    m_children.Erase( node );

    return true;
}

bool wxSizer::Detach( wxSizer *sizer )
{
    wxASSERT_MSG( sizer, wxT("Detaching NULL sizer") );

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetSizer() == sizer)
        {
            item->DetachSizer();
            delete item;
            m_children.Erase( node );
            return true;
        }
        node = node->GetNext();
    }

    return false;
}

bool wxSizer::Detach( wxWindow *window )
{
    wxASSERT_MSG( window, wxT("Detaching NULL window") );

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetWindow() == window)
        {
            delete item;
            m_children.Erase( node );
            return true;
        }
        node = node->GetNext();
    }

    return false;
}

bool wxSizer::Detach( int index )
{
    wxCHECK_MSG( index >= 0 && (size_t)index < m_children.GetCount(),
                 false,
                 wxT("Detach index is out of range") );

    wxSizerItemList::compatibility_iterator node = m_children.Item( index );

    wxCHECK_MSG( node, false, wxT("Failed to find child node") );

    wxSizerItem *item = node->GetData();

    if ( item->IsSizer() )
        item->DetachSizer();

    delete item;
    m_children.Erase( node );
    return true;
}

bool wxSizer::Replace( wxWindow *oldwin, wxWindow *newwin, bool recursive )
{
    wxASSERT_MSG( oldwin, wxT("Replacing NULL window") );
    wxASSERT_MSG( newwin, wxT("Replacing with NULL window") );

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetWindow() == oldwin)
        {
            item->AssignWindow(newwin);
            newwin->SetContainingSizer( this );
            return true;
        }
        else if (recursive && item->IsSizer())
        {
            if (item->GetSizer()->Replace( oldwin, newwin, true ))
                return true;
        }

        node = node->GetNext();
    }

    return false;
}

bool wxSizer::Replace( wxSizer *oldsz, wxSizer *newsz, bool recursive )
{
    wxASSERT_MSG( oldsz, wxT("Replacing NULL sizer") );
    wxASSERT_MSG( newsz, wxT("Replacing with NULL sizer") );

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetSizer() == oldsz)
        {
            item->AssignSizer(newsz);
            return true;
        }
        else if (recursive && item->IsSizer())
        {
            if (item->GetSizer()->Replace( oldsz, newsz, true ))
                return true;
        }

        node = node->GetNext();
    }

    return false;
}

bool wxSizer::Replace( size_t old, wxSizerItem *newitem )
{
    wxCHECK_MSG( old < m_children.GetCount(), false, wxT("Replace index is out of range") );
    wxASSERT_MSG( newitem, wxT("Replacing with NULL item") );

    wxSizerItemList::compatibility_iterator node = m_children.Item( old );

    wxCHECK_MSG( node, false, wxT("Failed to find child node") );

    wxSizerItem *item = node->GetData();
    node->SetData(newitem);

    if (item->IsWindow() && item->GetWindow())
        item->GetWindow()->SetContainingSizer(NULL);

    delete item;

    return true;
}

void wxSizer::Clear( bool delete_windows )
{
    // First clear the ContainingSizer pointers
    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->IsWindow())
            item->GetWindow()->SetContainingSizer( NULL );
        node = node->GetNext();
    }

    // Destroy the windows if needed
    if (delete_windows)
        DeleteWindows();

    // Now empty the list
    WX_CLEAR_LIST(wxSizerItemList, m_children);
}

void wxSizer::DeleteWindows()
{
    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        item->DeleteWindows();
        node = node->GetNext();
    }
}

wxSize wxSizer::ComputeFittingClientSize(wxWindow *window)
{
    wxCHECK_MSG( window, wxDefaultSize, "window can't be NULL" );

    // take the min size by default and limit it by max size
    wxSize size = GetMinClientSize(window);
    wxSize sizeMax;

    wxTopLevelWindow *tlw = wxDynamicCast(window, wxTopLevelWindow);
    if ( tlw )
    {
        // hack for small screen devices where TLWs are always full screen
        if ( tlw->IsAlwaysMaximized() )
        {
            return tlw->GetClientSize();
        }

        // limit the window to the size of the display it is on
        int disp = wxDisplay::GetFromWindow(window);
        if ( disp == wxNOT_FOUND )
        {
            // or, if we don't know which one it is, of the main one
            disp = 0;
        }

        sizeMax = wxDisplay(disp).GetClientArea().GetSize();

        // If determining the display size failed, skip the max size checks as
        // we really don't want to create windows of (0, 0) size.
        if ( !sizeMax.x || !sizeMax.y )
            return size;

        // space for decorations and toolbars etc.
        sizeMax = tlw->WindowToClientSize(sizeMax);
    }
    else
    {
        sizeMax = GetMaxClientSize(window);
    }

    if ( sizeMax.x != wxDefaultCoord && size.x > sizeMax.x )
            size.x = sizeMax.x;
    if ( sizeMax.y != wxDefaultCoord && size.y > sizeMax.y )
            size.y = sizeMax.y;

    return size;
}

wxSize wxSizer::ComputeFittingWindowSize(wxWindow *window)
{
    wxCHECK_MSG( window, wxDefaultSize, "window can't be NULL" );

    return window->ClientToWindowSize(ComputeFittingClientSize(window));
}

wxSize wxSizer::Fit( wxWindow *window )
{
    wxCHECK_MSG( window, wxDefaultSize, "window can't be NULL" );

    // set client size
    window->SetClientSize(ComputeFittingClientSize(window));

    // return entire size
    return window->GetSize();
}

void wxSizer::FitInside( wxWindow *window )
{
    wxSize size;
    if (window->IsTopLevel())
        size = VirtualFitSize( window );
    else
        size = GetMinClientSize( window );

    window->SetVirtualSize( size );
}

void wxSizer::Layout()
{
    // (re)calculates minimums needed for each item and other preparations
    // for layout
    CalcMin();

    // Applies the layout and repositions/resizes the items
    wxWindow::ChildrenRepositioningGuard repositionGuard(m_containingWindow);

    RecalcSizes();
}

void wxSizer::SetSizeHints( wxWindow *window )
{
    // Preserve the window's max size hints, but set the
    // lower bound according to the sizer calculations.

    // This is equivalent to calling Fit(), except that we need to set
    // the size hints _in between_ the two steps performed by Fit
    // (1. ComputeFittingClientSize, 2. SetClientSize). That's because
    // otherwise SetClientSize() could have no effect if there already are
    // size hints in effect that forbid requested client size.

    const wxSize clientSize = ComputeFittingClientSize(window);

    window->SetMinClientSize(clientSize);
    window->SetClientSize(clientSize);
}

#if WXWIN_COMPATIBILITY_2_8
void wxSizer::SetVirtualSizeHints( wxWindow *window )
{
    FitInside( window );
}
#endif // WXWIN_COMPATIBILITY_2_8

// TODO on mac we need a function that determines how much free space this
// min size contains, in order to make sure that we have 20 pixels of free
// space around the controls
wxSize wxSizer::GetMaxClientSize( wxWindow *window ) const
{
    return window->WindowToClientSize(window->GetMaxSize());
}

wxSize wxSizer::GetMinClientSize( wxWindow *WXUNUSED(window) )
{
    return GetMinSize();  // Already returns client size.
}

wxSize wxSizer::VirtualFitSize( wxWindow *window )
{
    wxSize size     = GetMinClientSize( window );
    wxSize sizeMax  = GetMaxClientSize( window );

    // Limit the size if sizeMax != wxDefaultSize

    if ( size.x > sizeMax.x && sizeMax.x != wxDefaultCoord )
        size.x = sizeMax.x;
    if ( size.y > sizeMax.y && sizeMax.y != wxDefaultCoord )
        size.y = sizeMax.y;

    return size;
}

wxSize wxSizer::GetMinSize()
{
    wxSize ret( CalcMin() );
    if (ret.x < m_minSize.x) ret.x = m_minSize.x;
    if (ret.y < m_minSize.y) ret.y = m_minSize.y;
    return ret;
}

void wxSizer::DoSetMinSize( int width, int height )
{
    m_minSize.x = width;
    m_minSize.y = height;
}

bool wxSizer::DoSetItemMinSize( wxWindow *window, int width, int height )
{
    wxASSERT_MSG( window, wxT("SetMinSize for NULL window") );

    // Is it our immediate child?

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetWindow() == window)
        {
            item->SetMinSize( width, height );
            return true;
        }
        node = node->GetNext();
    }

    // No?  Search any subsizers we own then

    node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if ( item->GetSizer() &&
             item->GetSizer()->DoSetItemMinSize( window, width, height ) )
        {
            // A child sizer found the requested windw, exit.
            return true;
        }
        node = node->GetNext();
    }

    return false;
}

bool wxSizer::DoSetItemMinSize( wxSizer *sizer, int width, int height )
{
    wxASSERT_MSG( sizer, wxT("SetMinSize for NULL sizer") );

    // Is it our immediate child?

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetSizer() == sizer)
        {
            item->GetSizer()->DoSetMinSize( width, height );
            return true;
        }
        node = node->GetNext();
    }

    // No?  Search any subsizers we own then

    node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if ( item->GetSizer() &&
             item->GetSizer()->DoSetItemMinSize( sizer, width, height ) )
        {
            // A child found the requested sizer, exit.
            return true;
        }
        node = node->GetNext();
    }

    return false;
}

bool wxSizer::DoSetItemMinSize( size_t index, int width, int height )
{
    wxSizerItemList::compatibility_iterator node = m_children.Item( index );

    wxCHECK_MSG( node, false, wxT("Failed to find child node") );

    wxSizerItem     *item = node->GetData();

    if (item->GetSizer())
    {
        // Sizers contains the minimal size in them, if not calculated ...
        item->GetSizer()->DoSetMinSize( width, height );
    }
    else
    {
        // ... but the minimal size of spacers and windows is stored via the item
        item->SetMinSize( width, height );
    }

    return true;
}

wxSizerItem* wxSizer::GetItem( wxWindow *window, bool recursive )
{
    wxASSERT_MSG( window, wxT("GetItem for NULL window") );

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetWindow() == window)
        {
            return item;
        }
        else if (recursive && item->IsSizer())
        {
            wxSizerItem *subitem = item->GetSizer()->GetItem( window, true );
            if (subitem)
                return subitem;
        }

        node = node->GetNext();
    }

    return NULL;
}

wxSizerItem* wxSizer::GetItem( wxSizer *sizer, bool recursive )
{
    wxASSERT_MSG( sizer, wxT("GetItem for NULL sizer") );

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem *item = node->GetData();

        if (item->GetSizer() == sizer)
        {
            return item;
        }
        else if (recursive && item->IsSizer())
        {
            wxSizerItem *subitem = item->GetSizer()->GetItem( sizer, true );
            if (subitem)
                return subitem;
        }

        node = node->GetNext();
    }

    return NULL;
}

wxSizerItem* wxSizer::GetItem( size_t index )
{
    wxCHECK_MSG( index < m_children.GetCount(),
                 NULL,
                 wxT("GetItem index is out of range") );

    return m_children.Item( index )->GetData();
}

wxSizerItem* wxSizer::GetItemById( int id, bool recursive )
{
    // This gets a sizer item by the id of the sizer item
    // and NOT the id of a window if the item is a window.

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetId() == id)
        {
            return item;
        }
        else if (recursive && item->IsSizer())
        {
            wxSizerItem *subitem = item->GetSizer()->GetItemById( id, true );
            if (subitem)
                return subitem;
        }

        node = node->GetNext();
    }

    return NULL;
}

bool wxSizer::Show( wxWindow *window, bool show, bool recursive )
{
    wxSizerItem *item = GetItem( window, recursive );

    if ( item )
    {
         item->Show( show );
         return true;
    }

    return false;
}

bool wxSizer::Show( wxSizer *sizer, bool show, bool recursive )
{
    wxSizerItem *item = GetItem( sizer, recursive );

    if ( item )
    {
         item->Show( show );
         return true;
    }

    return false;
}

bool wxSizer::Show( size_t index, bool show)
{
    wxSizerItem *item = GetItem( index );

    if ( item )
    {
         item->Show( show );
         return true;
    }

    return false;
}

void wxSizer::ShowItems( bool show )
{
    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        node->GetData()->Show( show );
        node = node->GetNext();
    }
}

bool wxSizer::AreAnyItemsShown() const
{
    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        if ( node->GetData()->IsShown() )
            return true;
        node = node->GetNext();
    }

    return false;
}

bool wxSizer::IsShown( wxWindow *window ) const
{
    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetWindow() == window)
        {
            return item->IsShown();
        }
        node = node->GetNext();
    }

    wxFAIL_MSG( wxT("IsShown failed to find sizer item") );

    return false;
}

bool wxSizer::IsShown( wxSizer *sizer ) const
{
    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();

        if (item->GetSizer() == sizer)
        {
            return item->IsShown();
        }
        node = node->GetNext();
    }

    wxFAIL_MSG( wxT("IsShown failed to find sizer item") );

    return false;
}

bool wxSizer::IsShown( size_t index ) const
{
    wxCHECK_MSG( index < m_children.GetCount(),
                 false,
                 wxT("IsShown index is out of range") );

    return m_children.Item( index )->GetData()->IsShown();
}


//---------------------------------------------------------------------------
// wxGridSizer
//---------------------------------------------------------------------------

wxGridSizer::wxGridSizer( int cols, int vgap, int hgap )
    : m_rows( cols == 0 ? 1 : 0 ),
      m_cols( cols ),
      m_vgap( vgap ),
      m_hgap( hgap )
{
    wxASSERT(cols >= 0);
}

wxGridSizer::wxGridSizer( int cols, const wxSize& gap )
    : m_rows( cols == 0 ? 1 : 0 ),
      m_cols( cols ),
      m_vgap( gap.GetHeight() ),
      m_hgap( gap.GetWidth() )
{
    wxASSERT(cols >= 0);
}

wxGridSizer::wxGridSizer( int rows, int cols, int vgap, int hgap )
    : m_rows( rows || cols ? rows : 1 ),
      m_cols( cols ),
      m_vgap( vgap ),
      m_hgap( hgap )
{
    wxASSERT(rows >= 0 && cols >= 0);
}

wxGridSizer::wxGridSizer( int rows, int cols, const wxSize& gap )
    : m_rows( rows || cols ? rows : 1 ),
      m_cols( cols ),
      m_vgap( gap.GetHeight() ),
      m_hgap( gap.GetWidth() )
{
    wxASSERT(rows >= 0 && cols >= 0);
}

wxSizerItem *wxGridSizer::DoInsert(size_t index, wxSizerItem *item)
{
    // if only the number of columns or the number of rows is specified for a
    // sizer, arbitrarily many items can be added to it but if both of them are
    // fixed, then the sizer can't have more than that many items -- check for
    // this here to ensure that we detect errors as soon as possible
    if ( m_cols && m_rows )
    {
        const int nitems = m_children.GetCount();
        if ( nitems == m_cols*m_rows )
        {
            wxFAIL_MSG(
                wxString::Format(
                    "too many items (%d > %d*%d) in grid sizer (maybe you "
                    "should omit the number of either rows or columns?)",
                nitems + 1, m_cols, m_rows)
            );

            // additionally, continuing to use the specified number of columns
            // and rows is not a good idea as callers of CalcRowsCols() expect
            // that all sizer items can fit into m_cols-/m_rows-sized arrays
            // which is not the case if there are too many items and results in
            // crashes, so let it compute the number of rows automatically by
            // forgetting the (wrong) number of rows specified (this also has a
            // nice side effect of giving only one assert even if there are
            // many more items than allowed in this sizer)
            m_rows = 0;
        }
    }

    return wxSizer::DoInsert(index, item);
}

int wxGridSizer::CalcRowsCols(int& nrows, int& ncols) const
{
    const int nitems = m_children.GetCount();

    ncols = GetEffectiveColsCount();
    nrows = GetEffectiveRowsCount();

    // Since Insert() checks for overpopulation, the following
    // should only assert if the grid was shrunk via SetRows() / SetCols()
    wxASSERT_MSG( nitems <= ncols*nrows, "logic error in wxGridSizer" );

    return nitems;
}

void wxGridSizer::RecalcSizes()
{
    int nitems, nrows, ncols;
    if ( (nitems = CalcRowsCols(nrows, ncols)) == 0 )
        return;

    wxSize sz( GetSize() );
    wxPoint pt( GetPosition() );

    int w = (sz.x - (ncols - 1) * m_hgap) / ncols;
    int h = (sz.y - (nrows - 1) * m_vgap) / nrows;

    int x = pt.x;
    for (int c = 0; c < ncols; c++)
    {
        int y = pt.y;
        for (int r = 0; r < nrows; r++)
        {
            int i = r * ncols + c;
            if (i < nitems)
            {
                wxSizerItemList::compatibility_iterator node = m_children.Item( i );

                wxASSERT_MSG( node, wxT("Failed to find SizerItemList node") );

                SetItemBounds( node->GetData(), x, y, w, h);
            }
            y = y + h + m_vgap;
        }
        x = x + w + m_hgap;
    }
}

wxSize wxGridSizer::CalcMin()
{
    int nrows, ncols;
    if ( CalcRowsCols(nrows, ncols) == 0 )
        return wxSize();

    // Find the max width and height for any component
    int w = 0;
    int h = 0;

    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem     *item = node->GetData();
        wxSize           sz( item->CalcMin() );

        w = wxMax( w, sz.x );
        h = wxMax( h, sz.y );

        node = node->GetNext();
    }

    // In case we have a nested sizer with a two step algo , give it
    // a chance to adjust to that (we give it width component)
    node = m_children.GetFirst();
    bool didChangeMinSize = false;
    while (node)
    {
        wxSizerItem     *item = node->GetData();
        didChangeMinSize |= item->InformFirstDirection( wxHORIZONTAL, w, -1 );

        node = node->GetNext();
    }

    // And redo iteration in case min size changed
    if( didChangeMinSize )
    {
        node = m_children.GetFirst();
        w = h = 0;
        while (node)
        {
            wxSizerItem     *item = node->GetData();
            wxSize           sz( item->GetMinSizeWithBorder() );

            w = wxMax( w, sz.x );
            h = wxMax( h, sz.y );

            node = node->GetNext();
        }
    }

    return wxSize( ncols * w + (ncols-1) * m_hgap,
                   nrows * h + (nrows-1) * m_vgap );
}

void wxGridSizer::SetItemBounds( wxSizerItem *item, int x, int y, int w, int h )
{
    wxPoint pt( x,y );
    wxSize sz( item->GetMinSizeWithBorder() );
    int flag = item->GetFlag();

    if ((flag & wxEXPAND) || (flag & wxSHAPED))
    {
       sz = wxSize(w, h);
    }
    else
    {
        if (flag & wxALIGN_CENTER_HORIZONTAL)
        {
            pt.x = x + (w - sz.x) / 2;
        }
        else if (flag & wxALIGN_RIGHT)
        {
            pt.x = x + (w - sz.x);
        }

        if (flag & wxALIGN_CENTER_VERTICAL)
        {
            pt.y = y + (h - sz.y) / 2;
        }
        else if (flag & wxALIGN_BOTTOM)
        {
            pt.y = y + (h - sz.y);
        }
    }

    item->SetDimension(pt, sz);
}

//---------------------------------------------------------------------------
// wxFlexGridSizer
//---------------------------------------------------------------------------

wxFlexGridSizer::wxFlexGridSizer( int cols, int vgap, int hgap )
               : wxGridSizer( cols, vgap, hgap ),
                 m_flexDirection(wxBOTH),
                 m_growMode(wxFLEX_GROWMODE_SPECIFIED)
{
}

wxFlexGridSizer::wxFlexGridSizer( int cols, const wxSize& gap )
               : wxGridSizer( cols, gap ),
                 m_flexDirection(wxBOTH),
                 m_growMode(wxFLEX_GROWMODE_SPECIFIED)
{
}

wxFlexGridSizer::wxFlexGridSizer( int rows, int cols, int vgap, int hgap )
               : wxGridSizer( rows, cols, vgap, hgap ),
                 m_flexDirection(wxBOTH),
                 m_growMode(wxFLEX_GROWMODE_SPECIFIED)
{
}

wxFlexGridSizer::wxFlexGridSizer( int rows, int cols, const wxSize& gap )
               : wxGridSizer( rows, cols, gap ),
                 m_flexDirection(wxBOTH),
                 m_growMode(wxFLEX_GROWMODE_SPECIFIED)
{
}

wxFlexGridSizer::~wxFlexGridSizer()
{
}

void wxFlexGridSizer::RecalcSizes()
{
    int nrows, ncols;
    if ( !CalcRowsCols(nrows, ncols) )
        return;

    const wxPoint pt(GetPosition());
    const wxSize sz(GetSize());

    AdjustForGrowables(sz);

    wxSizerItemList::const_iterator i = m_children.begin();
    const wxSizerItemList::const_iterator end = m_children.end();

    int y = 0;
    for ( int r = 0; r < nrows; r++ )
    {
        if ( m_rowHeights[r] == -1 )
        {
            // this row is entirely hidden, skip it
            for ( int c = 0; c < ncols; c++ )
            {
                if ( i == end )
                    return;

                ++i;
            }

            continue;
        }

        const int hrow = m_rowHeights[r];
        int h = sz.y - y; // max remaining height, don't overflow it
        if ( hrow < h )
            h = hrow;

        int x = 0;
        for ( int c = 0; c < ncols && i != end; c++, ++i )
        {
            const int wcol = m_colWidths[c];

            if ( wcol == -1 )
                continue;

            int w = sz.x - x; // max possible value, ensure we don't overflow
            if ( wcol < w )
                w = wcol;

            SetItemBounds(*i, pt.x + x, pt.y + y, w, h);

            x += wcol + m_hgap;
        }

        if ( i == end )
            return;

        y += hrow + m_vgap;
    }
}

// helper function used in CalcMin() to sum up the sizes of non-hidden items
static int SumArraySizes(const wxArrayInt& sizes, int gap)
{
    // Sum total minimum size, including gaps between rows/columns.
    // -1 is used as a magic number meaning empty row/column.
    int total = 0;

    const size_t count = sizes.size();
    for ( size_t n = 0; n < count; n++ )
    {
        if ( sizes[n] != -1 )
        {
            if ( total )
                total += gap; // separate from the previous column

            total += sizes[n];
        }
    }

    return total;
}

void wxFlexGridSizer::FindWidthsAndHeights(int nrows, int ncols)
{
    // We have to recalculate the sizes in case the item minimum size has
    // changed since the previous layout, or the item has been hidden using
    // wxSizer::Show(). If all the items in a row/column are hidden, the final
    // dimension of the row/column will be -1, indicating that the column
    // itself is hidden.
    m_rowHeights.assign(nrows, -1);
    m_colWidths.assign(ncols, -1);

    // n is the index of the item in left-to-right top-to-bottom order
    size_t n = 0;
    for ( wxSizerItemList::iterator i = m_children.begin();
          i != m_children.end();
          ++i, ++n )
    {
        wxSizerItem * const item = *i;
        if ( item->IsShown() )
        {
            // NOTE: Not doing the calculation here, this is just
            // for finding max values.
            const wxSize sz(item->GetMinSizeWithBorder());

            const int row = n / ncols;
            const int col = n % ncols;

            if ( sz.y > m_rowHeights[row] )
                m_rowHeights[row] = sz.y;
            if ( sz.x > m_colWidths[col] )
                m_colWidths[col] = sz.x;
        }
    }

    AdjustForFlexDirection();

    m_calculatedMinSize = wxSize(SumArraySizes(m_colWidths, m_hgap),
                                 SumArraySizes(m_rowHeights, m_vgap));
}

wxSize wxFlexGridSizer::CalcMin()
{
    int nrows,
        ncols;

    // Number of rows/columns can change as items are added or removed.
    if ( !CalcRowsCols(nrows, ncols) )
        return wxSize();


    // We have to recalculate the sizes in case the item minimum size has
    // changed since the previous layout, or the item has been hidden using
    // wxSizer::Show(). If all the items in a row/column are hidden, the final
    // dimension of the row/column will be -1, indicating that the column
    // itself is hidden.
    m_rowHeights.assign(nrows, -1);
    m_colWidths.assign(ncols, -1);

    for ( wxSizerItemList::iterator i = m_children.begin();
          i != m_children.end();
          ++i)
    {
        wxSizerItem * const item = *i;
        if ( item->IsShown() )
        {
            item->CalcMin();
        }
    }

    // The stage of looking for max values in each row/column has been
    // made a separate function, since it's reused in AdjustForGrowables.
    FindWidthsAndHeights(nrows,ncols);

    return m_calculatedMinSize;
}

void wxFlexGridSizer::AdjustForFlexDirection()
{
    // the logic in CalcMin works when we resize flexibly in both directions
    // but maybe this is not the case
    if ( m_flexDirection != wxBOTH )
    {
        // select the array corresponding to the direction in which we do *not*
        // resize flexibly
        wxArrayInt& array = m_flexDirection == wxVERTICAL ? m_colWidths
                                                          : m_rowHeights;

        const size_t count = array.GetCount();

        // find the largest value in this array
        size_t n;
        int largest = 0;

        for ( n = 0; n < count; ++n )
        {
            if ( array[n] > largest )
                largest = array[n];
        }

        // and now fill it with the largest value
        for ( n = 0; n < count; ++n )
        {
            // don't touch hidden rows
            if ( array[n] != -1 )
                array[n] = largest;
        }
    }
}

// helper of AdjustForGrowables() which is called for rows/columns separately
//
// parameters:
//      delta: the extra space, we do nothing unless it's positive
//      growable: indices or growable rows/cols in sizes array
//      sizes: the height/widths of rows/cols to adjust
//      proportions: proportions of the growable rows/cols or NULL if they all
//                   should be assumed to have proportion of 1
static void
DoAdjustForGrowables(int delta,
                     const wxArrayInt& growable,
                     wxArrayInt& sizes,
                     const wxArrayInt *proportions)
{
    if ( delta <= 0 )
        return;

    // total sum of proportions of all non-hidden rows
    int sum_proportions = 0;

    // number of currently shown growable rows
    int num = 0;

    const int max_idx = sizes.size();

    const size_t count = growable.size();
    size_t idx;
    for ( idx = 0; idx < count; idx++ )
    {
        // Since the number of rows/columns can change as items are
        // inserted/deleted, we need to verify at runtime that the
        // requested growable rows/columns are still valid.
        if ( growable[idx] >= max_idx )
            continue;

        // If all items in a row/column are hidden, that row/column will
        // have a dimension of -1.  This causes the row/column to be
        // hidden completely.
        if ( sizes[growable[idx]] == -1 )
            continue;

        if ( proportions )
            sum_proportions += (*proportions)[idx];

        num++;
    }

    if ( !num )
        return;

    // the remaining extra free space, adjusted during each iteration
    for ( idx = 0; idx < count; idx++ )
    {
        if ( growable[idx] >= max_idx )
            continue;

        if ( sizes[ growable[idx] ] == -1 )
            continue;

        int cur_delta;
        if ( sum_proportions == 0 )
        {
            // no growable rows -- divide extra space evenly among all
            cur_delta = delta/num;
            num--;
        }
        else // allocate extra space proportionally
        {
            const int cur_prop = (*proportions)[idx];
            cur_delta = (delta*cur_prop)/sum_proportions;
            sum_proportions -= cur_prop;
        }

        sizes[growable[idx]] += cur_delta;
        delta -= cur_delta;
    }
}

void wxFlexGridSizer::AdjustForGrowables(const wxSize& sz)
{
#if wxDEBUG_LEVEL
    // by the time this function is called, the sizer should be already fully
    // initialized and hence the number of its columns and rows is known and we
    // can check that all indices in m_growableCols/Rows are valid (see also
    // comments in AddGrowableCol/Row())
    if ( !m_rows || !m_cols )
    {
        if ( !m_rows )
        {
            int nrows = CalcRows();

            for ( size_t n = 0; n < m_growableRows.size(); n++ )
            {
                wxASSERT_MSG( m_growableRows[n] < nrows,
                              "invalid growable row index" );
            }
        }

        if ( !m_cols )
        {
            int ncols = CalcCols();

            for ( size_t n = 0; n < m_growableCols.size(); n++ )
            {
                wxASSERT_MSG( m_growableCols[n] < ncols,
                              "invalid growable column index" );
            }
        }
    }
#endif // wxDEBUG_LEVEL


    if ( (m_flexDirection & wxHORIZONTAL) || (m_growMode != wxFLEX_GROWMODE_NONE) )
    {
        DoAdjustForGrowables
        (
            sz.x - m_calculatedMinSize.x,
            m_growableCols,
            m_colWidths,
            m_growMode == wxFLEX_GROWMODE_SPECIFIED ? &m_growableColsProportions
                                                    : NULL
        );

        // This gives nested objects that benefit from knowing one size
        // component in advance the chance to use that.
        bool didAdjustMinSize = false;

        // Iterate over all items and inform about column width
        const int ncols = GetEffectiveColsCount();
        int col = 0;
        for ( wxSizerItemList::iterator i = m_children.begin();
              i != m_children.end();
              ++i )
        {
            didAdjustMinSize |= (*i)->InformFirstDirection(wxHORIZONTAL, m_colWidths[col], sz.y - m_calculatedMinSize.y);
            if ( ++col == ncols )
                col = 0;
        }

        // Only redo if info was actually used
        if( didAdjustMinSize )
        {
            DoAdjustForGrowables
            (
                sz.x - m_calculatedMinSize.x,
                m_growableCols,
                m_colWidths,
                m_growMode == wxFLEX_GROWMODE_SPECIFIED ? &m_growableColsProportions
                                                        : NULL
            );
        }
    }

    if ( (m_flexDirection & wxVERTICAL) || (m_growMode != wxFLEX_GROWMODE_NONE) )
    {
        // pass NULL instead of proportions if the grow mode is ALL as we
        // should treat all rows as having proportion of 1 then
        DoAdjustForGrowables
        (
            sz.y - m_calculatedMinSize.y,
            m_growableRows,
            m_rowHeights,
            m_growMode == wxFLEX_GROWMODE_SPECIFIED ? &m_growableRowsProportions
                                                    : NULL
        );
    }
}

bool wxFlexGridSizer::IsRowGrowable( size_t idx )
{
    return m_growableRows.Index( idx ) != wxNOT_FOUND;
}

bool wxFlexGridSizer::IsColGrowable( size_t idx )
{
    return m_growableCols.Index( idx ) != wxNOT_FOUND;
}

void wxFlexGridSizer::AddGrowableRow( size_t idx, int proportion )
{
    wxASSERT_MSG( !IsRowGrowable( idx ),
                  "AddGrowableRow() called for growable row" );

    // notice that we intentionally don't check the index validity here in (the
    // common) case when the number of rows was not specified in the ctor -- in
    // this case it will be computed only later, when all items are added to
    // the sizer, and the check will be done in AdjustForGrowables()
    wxCHECK_RET( !m_rows || idx < (size_t)m_rows, "invalid row index" );

    m_growableRows.Add( idx );
    m_growableRowsProportions.Add( proportion );
}

void wxFlexGridSizer::AddGrowableCol( size_t idx, int proportion )
{
    wxASSERT_MSG( !IsColGrowable( idx ),
                  "AddGrowableCol() called for growable column" );

    // see comment in AddGrowableRow(): although it's less common to omit the
    // specification of the number of columns, it still can also happen
    wxCHECK_RET( !m_cols || idx < (size_t)m_cols, "invalid column index" );

    m_growableCols.Add( idx );
    m_growableColsProportions.Add( proportion );
}

// helper function for RemoveGrowableCol/Row()
static void
DoRemoveFromArrays(size_t idx, wxArrayInt& items, wxArrayInt& proportions)
{
    const size_t count = items.size();
    for ( size_t n = 0; n < count; n++ )
    {
        if ( (size_t)items[n] == idx )
        {
            items.RemoveAt(n);
            proportions.RemoveAt(n);
            return;
        }
    }

    wxFAIL_MSG( wxT("column/row is already not growable") );
}

void wxFlexGridSizer::RemoveGrowableCol( size_t idx )
{
    DoRemoveFromArrays(idx, m_growableCols, m_growableColsProportions);
}

void wxFlexGridSizer::RemoveGrowableRow( size_t idx )
{
    DoRemoveFromArrays(idx, m_growableRows, m_growableRowsProportions);
}

//---------------------------------------------------------------------------
// wxBoxSizer
//---------------------------------------------------------------------------

wxSizerItem *wxBoxSizer::AddSpacer(int size)
{
    return IsVertical() ? Add(0, size) : Add(size, 0);
}

namespace
{

/*
    Helper of RecalcSizes(): checks if there is enough remaining space for the
    min size of the given item and returns its min size or the entire remaining
    space depending on which one is greater.

    This function updates the remaining space parameter to account for the size
    effectively allocated to the item.
 */
int
GetMinOrRemainingSize(int orient, const wxSizerItem *item, int *remainingSpace_)
{
    int& remainingSpace = *remainingSpace_;

    wxCoord size;
    if ( remainingSpace > 0 )
    {
        const wxSize sizeMin = item->GetMinSizeWithBorder();
        size = orient == wxHORIZONTAL ? sizeMin.x : sizeMin.y;

        if ( size >= remainingSpace )
        {
            // truncate the item to fit in the remaining space, this is better
            // than showing it only partially in general, even if both choices
            // are bad -- but there is nothing else we can do
            size = remainingSpace;
        }

        remainingSpace -= size;
    }
    else // no remaining space
    {
        // no space at all left, no need to even query the item for its min
        // size as we can't give it to it anyhow
        size = 0;
    }

    return size;
}

} // anonymous namespace

void wxBoxSizer::RecalcSizes()
{
    if ( m_children.empty() )
        return;

    const wxCoord totalMinorSize = GetSizeInMinorDir(m_size);
    const wxCoord totalMajorSize = GetSizeInMajorDir(m_size);

    // the amount of free space which we should redistribute among the
    // stretchable items (i.e. those with non zero proportion)
    int delta = totalMajorSize - GetSizeInMajorDir(m_minSize);

    // declare loop variables used below:
    wxSizerItemList::const_iterator i;  // iterator in m_children list
    unsigned n = 0;                     // item index in majorSizes array


    // First, inform item about the available size in minor direction as this
    // can change their size in the major direction. Also compute the number of
    // visible items and sum of their min sizes in major direction.

    int minMajorSize = 0;
    for ( i = m_children.begin(); i != m_children.end(); ++i )
    {
        wxSizerItem * const item = *i;

        if ( !item->IsShown() )
            continue;

        wxSize szMinPrev = item->GetMinSizeWithBorder();
        item->InformFirstDirection(m_orient^wxBOTH,totalMinorSize,delta);
        wxSize szMin = item->GetMinSizeWithBorder();
        int deltaChange = GetSizeInMajorDir(szMin-szMinPrev);
        if( deltaChange )
        {
            // Since we passed available space along to the item, it should not
            // take too much, so delta should not become negative.
            delta -= deltaChange;
        }
        minMajorSize += GetSizeInMajorDir(item->GetMinSizeWithBorder());
    }

    // update our min size have changed
    SizeInMajorDir(m_minSize) = minMajorSize;


    // space and sum of proportions for the remaining items, both may change
    // below
    wxCoord remaining = totalMajorSize;
    int totalProportion = m_totalProportion;

    // size of the (visible) items in major direction, -1 means "not fixed yet"
    wxVector<int> majorSizes(GetItemCount(), wxDefaultCoord);


    // Check for the degenerated case when we don't have enough space for even
    // the min sizes of all the items: in this case we really can't do much
    // more than to allocate the min size to as many of fixed size items as
    // possible (on the assumption that variable size items such as text zones
    // or list boxes may use scrollbars to show their content even if their
    // size is less than min size but that fixed size items such as buttons
    // will suffer even more if we don't give them their min size)
    if ( totalMajorSize < minMajorSize )
    {
        // Second degenerated case pass: allocate min size to all fixed size
        // items.
        for ( i = m_children.begin(), n = 0; i != m_children.end(); ++i, ++n )
        {
            wxSizerItem * const item = *i;

            if ( !item->IsShown() )
                continue;

            // deal with fixed size items only during this pass
            if ( item->GetProportion() )
                continue;

            majorSizes[n] = GetMinOrRemainingSize(m_orient, item, &remaining);
        }


        // Third degenerated case pass: allocate min size to all the remaining,
        // i.e. non-fixed size, items.
        for ( i = m_children.begin(), n = 0; i != m_children.end(); ++i, ++n )
        {
            wxSizerItem * const item = *i;

            if ( !item->IsShown() )
                continue;

            // we've already dealt with fixed size items above
            if ( !item->GetProportion() )
                continue;

            majorSizes[n] = GetMinOrRemainingSize(m_orient, item, &remaining);
        }
    }
    else // we do have enough space to give at least min sizes to all items
    {
        // Second and maybe more passes in the non-degenerated case: deal with
        // fixed size items and items whose min size is greater than what we
        // would allocate to them taking their proportion into account. For
        // both of them, we will just use their min size, but for the latter we
        // also need to reexamine all the items as the items which fitted
        // before we adjusted their size upwards might not fit any more. This
        // does make for a quadratic algorithm but it's not obvious how to
        // avoid it and hopefully it's not a huge problem in practice as the
        // sizers don't have many items usually (and, of course, the algorithm
        // still reduces into a linear one if there is enough space for all the
        // min sizes).
        bool nonFixedSpaceChanged = false;
        for ( i = m_children.begin(), n = 0; ; ++i, ++n )
        {
            if ( nonFixedSpaceChanged )
            {
                i = m_children.begin();
                n = 0;
                nonFixedSpaceChanged = false;
            }

            // check for the end of the loop only after the check above as
            // otherwise we wouldn't do another pass if the last child resulted
            // in non fixed space reduction
            if ( i == m_children.end() )
                break;

            wxSizerItem * const item = *i;

            if ( !item->IsShown() )
                continue;

            // don't check the item which we had already dealt with during a
            // previous pass (this is more than an optimization, the code
            // wouldn't work correctly if we kept adjusting for the same item
            // over and over again)
            if ( majorSizes[n] != wxDefaultCoord )
                continue;

            wxCoord minMajor = GetSizeInMajorDir(item->GetMinSizeWithBorder());

            // it doesn't make sense for min size to be negative but right now
            // it's possible to create e.g. a spacer with (-1, 10) as size and
            // people do it in their code apparently (see #11842) so ensure
            // that we don't use this -1 as real min size as it conflicts with
            // the meaning we use for it here and negative min sizes just don't
            // make sense anyhow (which is why it might be a better idea to
            // deal with them at wxSizerItem level in the future but for now
            // this is the minimal fix for the bug)
            if ( minMajor < 0 )
                minMajor = 0;

            const int propItem = item->GetProportion();
            if ( propItem )
            {
                // is the desired size of this item big enough?
                if ( (remaining*propItem)/totalProportion >= minMajor )
                {
                    // yes, it is, we'll determine the real size of this
                    // item later, for now just leave it as wxDefaultCoord
                    continue;
                }

                // the proportion of this item won't count, it has
                // effectively become fixed
                totalProportion -= propItem;
            }

            // we can already allocate space for this item
            majorSizes[n] = minMajor;

            // change the amount of the space remaining to the other items,
            // as this can result in not being able to satisfy their
            // proportions any more we will need to redo another loop
            // iteration
            remaining -= minMajor;

            nonFixedSpaceChanged = true;
        }

        // Similar to the previous loop, but dealing with items whose max size
        // is less than what we would allocate to them taking their proportion
        // into account.
        nonFixedSpaceChanged = false;
        for ( i = m_children.begin(), n = 0; ; ++i, ++n )
        {
            if ( nonFixedSpaceChanged )
            {
                i = m_children.begin();
                n = 0;
                nonFixedSpaceChanged = false;
            }

            // check for the end of the loop only after the check above as
            // otherwise we wouldn't do another pass if the last child resulted
            // in non fixed space reduction
            if ( i == m_children.end() )
                break;

            wxSizerItem * const item = *i;

            if ( !item->IsShown() )
                continue;

            // don't check the item which we had already dealt with during a
            // previous pass (this is more than an optimization, the code
            // wouldn't work correctly if we kept adjusting for the same item
            // over and over again)
            if ( majorSizes[n] != wxDefaultCoord )
                continue;

            wxCoord maxMajor = GetSizeInMajorDir(item->GetMaxSizeWithBorder());

            // must be nonzero, fixed-size items were dealt with in previous loop
            const int propItem = item->GetProportion();

            // is the desired size of this item small enough?
            if ( maxMajor < 0 ||
                    (remaining*propItem)/totalProportion <= maxMajor )
            {
                // yes, it is, we'll determine the real size of this
                // item later, for now just leave it as wxDefaultCoord
                continue;
            }

            // the proportion of this item won't count, it has
            // effectively become fixed
            totalProportion -= propItem;

            // we can already allocate space for this item
            majorSizes[n] = maxMajor;

            // change the amount of the space remaining to the other items,
            // as this can result in not being able to satisfy their
            // proportions any more we will need to redo another loop
            // iteration
            remaining -= maxMajor;

            nonFixedSpaceChanged = true;
        }

        // Last by one pass: distribute the remaining space among the non-fixed
        // items whose size weren't fixed yet according to their proportions.
        for ( i = m_children.begin(), n = 0; i != m_children.end(); ++i, ++n )
        {
            wxSizerItem * const item = *i;

            if ( !item->IsShown() )
                continue;

            if ( majorSizes[n] == wxDefaultCoord )
            {
                const int propItem = item->GetProportion();
                majorSizes[n] = (remaining*propItem)/totalProportion;

                remaining -= majorSizes[n];
                totalProportion -= propItem;
            }
        }
    }


    // the position at which we put the next child
    wxPoint pt(m_position);


    // Final pass: finally do position the items correctly using their sizes as
    // determined above.
    for ( i = m_children.begin(), n = 0; i != m_children.end(); ++i, ++n )
    {
        wxSizerItem * const item = *i;

        if ( !item->IsShown() )
            continue;

        const int majorSize = majorSizes[n];

        const wxSize sizeThis(item->GetMinSizeWithBorder());

        // apply the alignment in the minor direction
        wxPoint posChild(pt);

        wxCoord minorSize = GetSizeInMinorDir(sizeThis);
        const int flag = item->GetFlag();
        if ( (flag & (wxEXPAND | wxSHAPED)) || (minorSize > totalMinorSize) )
        {
            // occupy all the available space if wxEXPAND was given and also if
            // the item is too big to fit -- in this case we truncate it below
            // its minimal size which is bad but better than not showing parts
            // of the window at all
            minorSize = totalMinorSize;

            // do not allow the size in the minor direction to grow beyond the max
            // size of the item in the minor direction
            const wxCoord maxMinorSize = GetSizeInMinorDir(item->GetMaxSizeWithBorder());
            if ( maxMinorSize >= 0 && minorSize > maxMinorSize )
                minorSize = maxMinorSize;
        }

        if ( flag & (IsVertical() ? wxALIGN_RIGHT : wxALIGN_BOTTOM) )
        {
            PosInMinorDir(posChild) += totalMinorSize - minorSize;
        }
        // NB: wxCENTRE is used here only for backwards compatibility,
        //     wxALIGN_CENTRE should be used in new code
        else if ( flag & (wxCENTER | (IsVertical() ? wxALIGN_CENTRE_HORIZONTAL
                                                   : wxALIGN_CENTRE_VERTICAL)) )
        {
            PosInMinorDir(posChild) += (totalMinorSize - minorSize) / 2;
        }


        // apply RTL adjustment for horizontal sizers:
        if ( !IsVertical() && m_containingWindow )
        {
            posChild.x = m_containingWindow->AdjustForLayoutDirection
                                             (
                                                posChild.x,
                                                majorSize,
                                                m_size.x
                                             );
        }

        // finally set size of this child and advance to the next one
        item->SetDimension(posChild, SizeFromMajorMinor(majorSize, minorSize));

        PosInMajorDir(pt) += majorSize;
    }
}

wxSize wxBoxSizer::CalcMin()
{
    m_totalProportion = 0;
    m_minSize = wxSize(0, 0);

    // The minimal size for the sizer should be big enough to allocate its
    // element at least its minimal size but also, and this is the non trivial
    // part, to respect the children proportion. To satisfy the latter
    // condition we must find the greatest min-size-to-proportion ratio for all
    // elements with non-zero proportion.
    float maxMinSizeToProp = 0.;
    for ( wxSizerItemList::const_iterator i = m_children.begin();
          i != m_children.end();
          ++i )
    {
        wxSizerItem * const item = *i;

        if ( !item->IsShown() )
            continue;

        const wxSize sizeMinThis = item->CalcMin();
        if ( const int propThis = item->GetProportion() )
        {
            float minSizeToProp = GetSizeInMajorDir(sizeMinThis);
            minSizeToProp /= propThis;

            if ( minSizeToProp > maxMinSizeToProp )
                maxMinSizeToProp = minSizeToProp;

            m_totalProportion += item->GetProportion();
        }
        else // fixed size item
        {
            // Just account for its size directly
            SizeInMajorDir(m_minSize) += GetSizeInMajorDir(sizeMinThis);
        }

        // In the transversal direction we just need to find the maximum.
        if ( GetSizeInMinorDir(sizeMinThis) > GetSizeInMinorDir(m_minSize) )
            SizeInMinorDir(m_minSize) = GetSizeInMinorDir(sizeMinThis);
    }

    // Using the max ratio ensures that the min size is big enough for all
    // items to have their min size and satisfy the proportions among them.
    SizeInMajorDir(m_minSize) += (int)(maxMinSizeToProp*m_totalProportion);

    return m_minSize;
}

//---------------------------------------------------------------------------
// wxStaticBoxSizer
//---------------------------------------------------------------------------

#if wxUSE_STATBOX

wxStaticBoxSizer::wxStaticBoxSizer( wxStaticBox *box, int orient )
    : wxBoxSizer( orient ),
      m_staticBox( box )
{
    wxASSERT_MSG( box, wxT("wxStaticBoxSizer needs a static box") );

    // do this so that our Detach() is called if the static box is destroyed
    // before we are
    m_staticBox->SetContainingSizer(this);
}

wxStaticBoxSizer::wxStaticBoxSizer(int orient, wxWindow *win, const wxString& s)
                : wxBoxSizer(orient),
                  m_staticBox(new wxStaticBox(win, wxID_ANY, s))
{
    // same as above
    m_staticBox->SetContainingSizer(this);
}

wxStaticBoxSizer::~wxStaticBoxSizer()
{
    // As an exception to the general rule that sizers own other sizers that
    // they contain but not the windows managed by them, this sizer does own
    // the static box associated with it (which is not very logical but
    // convenient in practice and, most importantly, can't be changed any more
    // because of compatibility). However we definitely should not destroy the
    // children of this static box when we're being destroyed, as this would be
    // unexpected and break the existing code which worked with the windows
    // created as siblings of the static box instead of its children in the
    // previous wxWidgets versions, so ensure they are left alive.

    if ( m_staticBox )
    {
        // Notice that we must make a copy of the list as it will be changed by
        // Reparent() calls in the loop.
        const wxWindowList children = m_staticBox->GetChildren();
        wxWindow* const parent = m_staticBox->GetParent();
        for ( wxWindowList::const_iterator i = children.begin();
              i != children.end();
              ++i )
        {
            (*i)->Reparent(parent);
        }

        delete m_staticBox;
    }
}

void wxStaticBoxSizer::RecalcSizes()
{
    int top_border, other_border;
    m_staticBox->GetBordersForSizer(&top_border, &other_border);

    m_staticBox->SetSize( m_position.x, m_position.y, m_size.x, m_size.y );

    wxSize old_size( m_size );
    m_size.x -= 2*other_border;
    m_size.y -= top_border + other_border;

    wxPoint old_pos( m_position );
    if (m_staticBox->GetChildren().GetCount() > 0)
    {
#if defined( __WXGTK20__ )
        // if the wxStaticBox has created a wxPizza to contain its children
        // (see wxStaticBox::AddChild) then we need to place the items it contains
        // in the wxBoxSizer::RecalcSizes() call below using coordinates relative
        // to the top-left corner of the staticbox:
        m_position.x = m_position.y = 0;
#elif defined(__WXOSX__) && wxOSX_USE_COCOA
        // the distance from the 'inner' content view to the embedded controls
        // this is independent of the title, therefore top_border is not relevant
        m_position.x = m_position.y = 10;
#else
        // if the wxStaticBox has children, then these windows must be placed
        // by the wxBoxSizer::RecalcSizes() call below using coordinates relative
        // to the top-left corner of the staticbox (but unlike wxGTK, we need
        // to keep in count the static borders here!):
        m_position.x = other_border;
        m_position.y = top_border;
#endif
    }
    else
    {
        // the windows contained in the staticbox have been created as siblings of the
        // staticbox (this is the "old" way of staticbox contents creation); in this
        // case we need to position them with coordinates relative to our common parent
        m_position.x += other_border;
        m_position.y += top_border;
    }

    wxBoxSizer::RecalcSizes();

    m_position = old_pos;
    m_size = old_size;
}

wxSize wxStaticBoxSizer::CalcMin()
{
    int top_border, other_border;
    m_staticBox->GetBordersForSizer(&top_border, &other_border);

    wxSize ret( wxBoxSizer::CalcMin() );
    ret.x += 2*other_border;

    // ensure that we're wide enough to show the static box label (there is no
    // need to check for the static box best size in vertical direction though)
    const int boxWidth = m_staticBox->GetBestSize().x;
    if ( ret.x < boxWidth )
        ret.x = boxWidth;

    ret.y += other_border + top_border;

    return ret;
}

void wxStaticBoxSizer::ShowItems( bool show )
{
    m_staticBox->Show( show );
    wxBoxSizer::ShowItems( show );
}

bool wxStaticBoxSizer::AreAnyItemsShown() const
{
    // We don't need to check the status of our child items: if the box is
    // shown, this sizer should be considered shown even if all its elements
    // are hidden (or, more prosaically, there are no elements at all). And,
    // conversely, if the box is hidden then all our items, which are its
    // children, are hidden too.
    return m_staticBox->IsShown();
}

bool wxStaticBoxSizer::Detach( wxWindow *window )
{
    // avoid deleting m_staticBox in our dtor if it's being detached from the
    // sizer (which can happen because it's being already destroyed for
    // example)
    if ( window == m_staticBox )
    {
        m_staticBox = NULL;
        return true;
    }

    return wxSizer::Detach( window );
}

#endif // wxUSE_STATBOX

//---------------------------------------------------------------------------
// wxStdDialogButtonSizer
//---------------------------------------------------------------------------

#if wxUSE_BUTTON

wxStdDialogButtonSizer::wxStdDialogButtonSizer()
    : wxBoxSizer(wxHORIZONTAL)
{
    // Vertical buttons with lots of space on either side
    // looks rubbish on WinCE, so let's not do this for now.
    // If we are going to use vertical buttons, we should
    // put the sizer to the right of other controls in the dialog,
    // and that's beyond the scope of this sizer.
#ifndef __WXWINCE__
    bool is_pda = (wxSystemSettings::GetScreenType() <= wxSYS_SCREEN_PDA);
    // If we have a PDA screen, put yes/no button over
    // all other buttons, otherwise on the left side.
    if (is_pda)
        m_orient = wxVERTICAL;
#endif

    m_buttonAffirmative = NULL;
    m_buttonApply = NULL;
    m_buttonNegative = NULL;
    m_buttonCancel = NULL;
    m_buttonHelp = NULL;
}

void wxStdDialogButtonSizer::AddButton(wxButton *mybutton)
{
    switch (mybutton->GetId())
    {
        case wxID_OK:
        case wxID_YES:
        case wxID_SAVE:
            m_buttonAffirmative = mybutton;
            break;
        case wxID_APPLY:
            m_buttonApply = mybutton;
            break;
        case wxID_NO:
            m_buttonNegative = mybutton;
            break;
        case wxID_CANCEL:
        case wxID_CLOSE:
            m_buttonCancel = mybutton;
            break;
        case wxID_HELP:
        case wxID_CONTEXT_HELP:
            m_buttonHelp = mybutton;
            break;
        default:
            break;
    }
}

void wxStdDialogButtonSizer::SetAffirmativeButton( wxButton *button )
{
    m_buttonAffirmative = button;
}

void wxStdDialogButtonSizer::SetNegativeButton( wxButton *button )
{
    m_buttonNegative = button;
}

void wxStdDialogButtonSizer::SetCancelButton( wxButton *button )
{
    m_buttonCancel = button;
}

void wxStdDialogButtonSizer::Realize()
{
#ifdef __WXMAC__
        Add(0, 0, 0, wxLEFT, 6);
        if (m_buttonHelp)
            Add((wxWindow*)m_buttonHelp, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, 6);

        if (m_buttonNegative){
            // HIG POLICE BULLETIN - destructive buttons need extra padding
            // 24 pixels on either side
            Add((wxWindow*)m_buttonNegative, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, 12);
        }

        // extra whitespace between help/negative and cancel/ok buttons
        Add(0, 0, 1, wxEXPAND, 0);

        if (m_buttonCancel){
            Add((wxWindow*)m_buttonCancel, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, 6);
            // Cancel or help should be default
            // m_buttonCancel->SetDefaultButton();
        }

        // Ugh, Mac doesn't really have apply dialogs, so I'll just
        // figure the best place is between Cancel and OK
        if (m_buttonApply)
            Add((wxWindow*)m_buttonApply, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, 6);

        if (m_buttonAffirmative){
            Add((wxWindow*)m_buttonAffirmative, 0, wxALIGN_CENTRE | wxLEFT, 6);

            if (m_buttonAffirmative->GetId() == wxID_SAVE){
                // these buttons have set labels under Mac so we should use them
                m_buttonAffirmative->SetLabel(_("Save"));
                if (m_buttonNegative)
                    m_buttonNegative->SetLabel(_("Don't Save"));
            }
        }

        // Extra space around and at the right
        Add(12, 40);
#elif defined(__WXGTK20__)
        // http://library.gnome.org/devel/hig-book/stable/windows-alert.html.en
        // says that the correct button order is
        //
        //      [Help]                  [Alternative] [Cancel] [Affirmative]

        // Flags ensuring that margins between the buttons are 6 pixels.
        const wxSizerFlags
            flagsBtn = wxSizerFlags().Centre().Border(wxLEFT | wxRIGHT, 3);

        // Margin around the entire sizer button should be 12.
        AddSpacer(9);

        if (m_buttonHelp)
            Add(m_buttonHelp, flagsBtn);

        // Align the rest of the buttons to the right.
        AddStretchSpacer();

        if (m_buttonNegative)
            Add(m_buttonNegative, flagsBtn);

        if (m_buttonApply)
            Add(m_buttonApply, flagsBtn);

        if (m_buttonCancel)
            Add(m_buttonCancel, flagsBtn);

        if (m_buttonAffirmative)
            Add(m_buttonAffirmative, flagsBtn);

        // Ensure that the right margin is 12 as well.
        AddSpacer(9);
#elif defined(__WXMSW__)
        // Windows

        // right-justify buttons
        Add(0, 0, 1, wxEXPAND, 0);

        if (m_buttonAffirmative){
            Add((wxWindow*)m_buttonAffirmative, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, m_buttonAffirmative->ConvertDialogToPixels(wxSize(2, 0)).x);
        }

        if (m_buttonNegative){
            Add((wxWindow*)m_buttonNegative, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, m_buttonNegative->ConvertDialogToPixels(wxSize(2, 0)).x);
        }

        if (m_buttonCancel){
            Add((wxWindow*)m_buttonCancel, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, m_buttonCancel->ConvertDialogToPixels(wxSize(2, 0)).x);
        }
        if (m_buttonApply)
            Add((wxWindow*)m_buttonApply, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, m_buttonApply->ConvertDialogToPixels(wxSize(2, 0)).x);

        if (m_buttonHelp)
            Add((wxWindow*)m_buttonHelp, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, m_buttonHelp->ConvertDialogToPixels(wxSize(2, 0)).x);
#else
        // GTK+1 and any other platform

        // Add(0, 0, 0, wxLEFT, 5); // Not sure what this was for but it unbalances the dialog
        if (m_buttonHelp)
            Add((wxWindow*)m_buttonHelp, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, m_buttonHelp->ConvertDialogToPixels(wxSize(4, 0)).x);

        // extra whitespace between help and cancel/ok buttons
        Add(0, 0, 1, wxEXPAND, 0);

        if (m_buttonApply)
            Add((wxWindow*)m_buttonApply, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, m_buttonApply->ConvertDialogToPixels(wxSize(4, 0)).x);

        if (m_buttonAffirmative){
            Add((wxWindow*)m_buttonAffirmative, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, m_buttonAffirmative->ConvertDialogToPixels(wxSize(4, 0)).x);
        }

        if (m_buttonNegative){
            Add((wxWindow*)m_buttonNegative, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, m_buttonNegative->ConvertDialogToPixels(wxSize(4, 0)).x);
        }

        if (m_buttonCancel){
            Add((wxWindow*)m_buttonCancel, 0, wxALIGN_CENTRE | wxLEFT | wxRIGHT, m_buttonCancel->ConvertDialogToPixels(wxSize(4, 0)).x);
            // Cancel or help should be default
            // m_buttonCancel->SetDefaultButton();
        }

#endif
}

#endif // wxUSE_BUTTON
