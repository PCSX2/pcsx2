///////////////////////////////////////////////////////////////////////////////
// Name:        src/ribbon/page.cpp
// Purpose:     Container for ribbon-bar-style interface panels
// Author:      Peter Cawley
// Modified by:
// Created:     2009-05-25
// Copyright:   (C) Peter Cawley
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_RIBBON

#include "wx/ribbon/page.h"
#include "wx/ribbon/art.h"
#include "wx/ribbon/bar.h"
#include "wx/dcbuffer.h"

#ifndef WX_PRECOMP
#endif

#ifdef __WXMSW__
#include "wx/msw/private.h"
#endif

static int GetSizeInOrientation(wxSize size, wxOrientation orientation);

// As scroll buttons need to be rendered on top of a page's child windows, the
// buttons themselves have to be proper child windows (rather than just painted
// onto the page). In order to get proper clipping of a page's children (with
// regard to the scroll button), the scroll buttons are created as children of
// the ribbon bar rather than children of the page. This could not have been
// achieved by creating buttons as children of the page and then doing some Z-order
// manipulation, as this causes problems on win32 due to ribbon panels having the
// transparent flag set.
class wxRibbonPageScrollButton : public wxRibbonControl
{
public:
    wxRibbonPageScrollButton(wxRibbonPage* sibling,
                 wxWindowID id = wxID_ANY,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = 0);

    virtual ~wxRibbonPageScrollButton();

protected:
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }

    void OnEraseBackground(wxEraseEvent& evt);
    void OnPaint(wxPaintEvent& evt);
    void OnMouseEnter(wxMouseEvent& evt);
    void OnMouseLeave(wxMouseEvent& evt);
    void OnMouseDown(wxMouseEvent& evt);
    void OnMouseUp(wxMouseEvent& evt);

    wxRibbonPage* m_sibling;
    long m_flags;

    DECLARE_CLASS(wxRibbonPageScrollButton)
    DECLARE_EVENT_TABLE()
};

IMPLEMENT_CLASS(wxRibbonPageScrollButton, wxRibbonControl)

BEGIN_EVENT_TABLE(wxRibbonPageScrollButton, wxRibbonControl)
    EVT_ENTER_WINDOW(wxRibbonPageScrollButton::OnMouseEnter)
    EVT_ERASE_BACKGROUND(wxRibbonPageScrollButton::OnEraseBackground)
    EVT_LEAVE_WINDOW(wxRibbonPageScrollButton::OnMouseLeave)
    EVT_LEFT_DOWN(wxRibbonPageScrollButton::OnMouseDown)
    EVT_LEFT_UP(wxRibbonPageScrollButton::OnMouseUp)
    EVT_PAINT(wxRibbonPageScrollButton::OnPaint)
END_EVENT_TABLE()

wxRibbonPageScrollButton::wxRibbonPageScrollButton(wxRibbonPage* sibling,
                 wxWindowID id,
                 const wxPoint& pos,
                 const wxSize& size,
                 long style) : wxRibbonControl(sibling->GetParent(), id, pos, size, wxBORDER_NONE)
{
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    m_sibling = sibling;
    m_flags = (style & wxRIBBON_SCROLL_BTN_DIRECTION_MASK) | wxRIBBON_SCROLL_BTN_FOR_PAGE;
}

wxRibbonPageScrollButton::~wxRibbonPageScrollButton()
{
}

void wxRibbonPageScrollButton::OnEraseBackground(wxEraseEvent& WXUNUSED(evt))
{
    // Do nothing - all painting done in main paint handler
}

void wxRibbonPageScrollButton::OnPaint(wxPaintEvent& WXUNUSED(evt))
{
    wxAutoBufferedPaintDC dc(this);
    if(m_art)
    {
        m_art->DrawScrollButton(dc, this, GetSize(), m_flags);
    }
}

void wxRibbonPageScrollButton::OnMouseEnter(wxMouseEvent& WXUNUSED(evt))
{
    m_flags |= wxRIBBON_SCROLL_BTN_HOVERED;
    Refresh(false);
}

void wxRibbonPageScrollButton::OnMouseLeave(wxMouseEvent& WXUNUSED(evt))
{
    m_flags &= ~wxRIBBON_SCROLL_BTN_HOVERED;
    m_flags &= ~wxRIBBON_SCROLL_BTN_ACTIVE;
    Refresh(false);
}

void wxRibbonPageScrollButton::OnMouseDown(wxMouseEvent& WXUNUSED(evt))
{
    m_flags |= wxRIBBON_SCROLL_BTN_ACTIVE;
    Refresh(false);
}

void wxRibbonPageScrollButton::OnMouseUp(wxMouseEvent& WXUNUSED(evt))
{
    if(m_flags & wxRIBBON_SCROLL_BTN_ACTIVE)
    {
        m_flags &= ~wxRIBBON_SCROLL_BTN_ACTIVE;
        Refresh(false);
        switch(m_flags & wxRIBBON_SCROLL_BTN_DIRECTION_MASK)
        {
        case wxRIBBON_SCROLL_BTN_DOWN:
        case wxRIBBON_SCROLL_BTN_RIGHT:
            m_sibling->ScrollSections(1);
            break;
        case wxRIBBON_SCROLL_BTN_UP:
        case wxRIBBON_SCROLL_BTN_LEFT:
            m_sibling->ScrollSections(-1);
            break;
        default:
            break;
        }
    }
}

IMPLEMENT_CLASS(wxRibbonPage, wxRibbonControl)

BEGIN_EVENT_TABLE(wxRibbonPage, wxRibbonControl)
    EVT_ERASE_BACKGROUND(wxRibbonPage::OnEraseBackground)
    EVT_PAINT(wxRibbonPage::OnPaint)
    EVT_SIZE(wxRibbonPage::OnSize)
END_EVENT_TABLE()

wxRibbonPage::wxRibbonPage()
{
    m_scroll_left_btn = NULL;
    m_scroll_right_btn = NULL;
    m_scroll_amount = 0;
    m_scroll_buttons_visible = false;
}

wxRibbonPage::wxRibbonPage(wxRibbonBar* parent,
                   wxWindowID id,
                   const wxString& label,
                   const wxBitmap& icon,
                   long WXUNUSED(style))
    : wxRibbonControl(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    CommonInit(label, icon);
}

wxRibbonPage::~wxRibbonPage()
{
    delete[] m_size_calc_array;
}

bool wxRibbonPage::Create(wxRibbonBar* parent,
                wxWindowID id,
                const wxString& label,
                const wxBitmap& icon,
                long WXUNUSED(style))
{
    if(!wxRibbonControl::Create(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE))
        return false;

    CommonInit(label, icon);

    return true;
}

void wxRibbonPage::CommonInit(const wxString& label, const wxBitmap& icon)
{
    SetName(label);

    SetLabel(label);
    m_old_size = wxSize(0, 0);
    m_icon = icon;
    m_scroll_left_btn = NULL;
    m_scroll_right_btn = NULL;
    m_size_calc_array = NULL;
    m_size_calc_array_size = 0;
    m_scroll_amount = 0;
    m_scroll_buttons_visible = false;

    SetBackgroundStyle(wxBG_STYLE_CUSTOM);

    wxDynamicCast(GetParent(), wxRibbonBar)->AddPage(this);
}

void wxRibbonPage::SetArtProvider(wxRibbonArtProvider* art)
{
    m_art = art;
    for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
          node;
          node = node->GetNext() )
    {
        wxWindow* child = node->GetData();
        wxRibbonControl* ribbon_child = wxDynamicCast(child, wxRibbonControl);
        if(ribbon_child)
        {
            ribbon_child->SetArtProvider(art);
        }
    }
}

void wxRibbonPage::AdjustRectToIncludeScrollButtons(wxRect* rect) const
{
    if(m_scroll_buttons_visible)
    {
        if(GetMajorAxis() == wxVERTICAL)
        {
            if(m_scroll_left_btn)
            {
                rect->SetY(rect->GetY() -
                    m_scroll_left_btn->GetSize().GetHeight());
                rect->SetHeight(rect->GetHeight() +
                    m_scroll_left_btn->GetSize().GetHeight());
            }
            if(m_scroll_right_btn)
            {
                rect->SetHeight(rect->GetHeight() +
                    m_scroll_right_btn->GetSize().GetHeight());
            }
        }
        else
        {
            if(m_scroll_left_btn)
            {
                rect->SetX(rect->GetX() -
                    m_scroll_left_btn->GetSize().GetWidth());
                rect->SetWidth(rect->GetWidth() +
                    m_scroll_left_btn->GetSize().GetWidth());
            }
            if(m_scroll_right_btn)
            {
                rect->SetWidth(rect->GetWidth() +
                    m_scroll_right_btn->GetSize().GetWidth());
            }
        }
    }
}

void wxRibbonPage::OnEraseBackground(wxEraseEvent& WXUNUSED(evt))
{
    // All painting done in main paint handler to minimise flicker
}

void wxRibbonPage::OnPaint(wxPaintEvent& WXUNUSED(evt))
{
    // No foreground painting done by the page itself, but a paint DC
    // must be created anyway.
    wxAutoBufferedPaintDC dc(this);
    wxRect rect(GetSize());
    AdjustRectToIncludeScrollButtons(&rect);
    m_art->DrawPageBackground(dc, this, rect);
}

wxOrientation wxRibbonPage::GetMajorAxis() const
{
    if(m_art && (m_art->GetFlags() & wxRIBBON_BAR_FLOW_VERTICAL))
    {
        return wxVERTICAL;
    }
    else
    {
        return wxHORIZONTAL;
    }
}

bool wxRibbonPage::ScrollLines(int lines)
{
    return ScrollPixels(lines * 8);
}

bool wxRibbonPage::ScrollPixels(int pixels)
{
    if(pixels < 0)
    {
        if(m_scroll_amount == 0)
            return false;
        if(m_scroll_amount < -pixels)
            pixels = -m_scroll_amount;
    }
    else if(pixels > 0)
    {
        if(m_scroll_amount == m_scroll_amount_limit)
            return false;
        if(m_scroll_amount + pixels > m_scroll_amount_limit)
            pixels = m_scroll_amount_limit - m_scroll_amount;
    }
    else
        return false;

    m_scroll_amount += pixels;

    for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
              node;
              node = node->GetNext() )
    {
        wxWindow* child = node->GetData();
        int x, y;
        child->GetPosition(&x, &y);
        if(GetMajorAxis() == wxHORIZONTAL)
            x -= pixels;
        else
            y -= pixels;
        child->SetPosition(wxPoint(x, y));
    }

    ShowScrollButtons();
    Refresh();
    return true;
}

bool wxRibbonPage::ScrollSections(int sections)
{
    // Currently the only valid values are -1 and 1 for scrolling left and
    // right, respectively.
    const bool scrollForward = sections >= 1;

    // Determine by how many pixels to scroll. If something on the page
    // is partially visible, scroll to make it fully visible. Otherwise
    // find the next item that will become visible and scroll to make it
    // fully visible. The ScrollPixel call will correct if we scroll too
    // much if the available width is smaller than the items.

    // Scroll at minimum the same amount as ScrollLines(1):
    int minscroll = sections * 8;
    // How many pixels to scroll:
    int pixels = 0;

    // Determine the scroll position, that is, the page border where items
    // are appearing.
    int scrollpos = 0;

    wxOrientation major_axis = GetMajorAxis();
    int gap = 0;

    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    GetSize(&width, &height);
    GetPosition(&x, &y);
    if(major_axis == wxHORIZONTAL)
    {
        gap = m_art->GetMetric(wxRIBBON_ART_PANEL_X_SEPARATION_SIZE);
        if (scrollForward)
        {
            scrollpos = width - m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_RIGHT_SIZE);
        }
        else
        {
            scrollpos = m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_LEFT_SIZE);
        }
    }
    else
    {
        gap = m_art->GetMetric(wxRIBBON_ART_PANEL_Y_SEPARATION_SIZE);
        if (scrollForward)
        {
            scrollpos = width - m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_BOTTOM_SIZE);
        }
        else
        {
            scrollpos = m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_TOP_SIZE);
        }
    }

    // Find the child that is partially shown or just beyond the scroll position
    for(wxWindowList::compatibility_iterator
            node = scrollForward ? GetChildren().GetFirst()
                                 : GetChildren().GetLast();
        node;
        node = scrollForward ? node->GetNext()
                             : node->GetPrevious())
    {
        wxWindow* child = node->GetData();
        child->GetSize(&width, &height);
        child->GetPosition(&x, &y);
        int pos0 = 0;
        int pos1 = 0;
        if (major_axis == wxHORIZONTAL)
        {
            pos0 = x;
            pos1 = x + width + gap;
        }
        else
        {
            pos0 = y;
            pos1 = y + height + gap;
        }
        if (scrollpos >= pos0 && scrollpos <= pos1)
        {
            // This section is partially visible, scroll to make it fully visible.
            if (scrollForward)
            {
                pixels += pos1 - scrollpos;
            }
            else
            {
                pixels += pos0 - scrollpos;
            }
            if (abs(pixels) >= abs(minscroll))
                break;
        }
        if (scrollpos <= pos0 && scrollForward)
        {
            // This section is next, scroll the entire section width
            pixels += (pos1 - pos0);
            break;
        }
        if (scrollpos >= pos1 && !scrollForward)
        {
            // This section is next, scroll the entire section width
            pixels += (pos0 - pos1);
            break;
        }
    }
    // Do a final safety sanity check, should not be necessary, but will not hurt either.
    if (pixels == 0)
    {
        pixels = minscroll;
    }
    if (pixels * minscroll < 0)
    {
        pixels = -pixels;
    }

    return ScrollPixels(pixels);
}

void wxRibbonPage::SetSizeWithScrollButtonAdjustment(int x, int y, int width, int height)
{
    if(m_scroll_buttons_visible)
    {
        if(GetMajorAxis() == wxHORIZONTAL)
        {
            if(m_scroll_left_btn)
            {
                int w = m_scroll_left_btn->GetSize().GetWidth();
                m_scroll_left_btn->SetPosition(wxPoint(x, y));
                x += w;
                width -= w;
            }
            if(m_scroll_right_btn)
            {
                int w = m_scroll_right_btn->GetSize().GetWidth();
                width -= w;
                m_scroll_right_btn->SetPosition(wxPoint(x + width, y));
            }
        }
        else
        {
            if(m_scroll_left_btn)
            {
                int h = m_scroll_left_btn->GetSize().GetHeight();
                m_scroll_left_btn->SetPosition(wxPoint(x, y));
                y += h;
                height -= h;
            }
            if(m_scroll_right_btn)
            {
                int h = m_scroll_right_btn->GetSize().GetHeight();
                height -= h;
                m_scroll_right_btn->SetPosition(wxPoint(x, y + height));
            }
        }
    }
    if (width < 0) width = 0;
    if (height < 0) height = 0;
    SetSize(x, y, width, height);
}

void wxRibbonPage::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    // When a resize triggers the scroll buttons to become visible, the page is resized.
    // This resize from within a resize event can cause (MSW) wxWidgets some confusion,
    // and report the 1st size to the 2nd size event. Hence the most recent size is
    // remembered internally and used in Layout() where appropriate.

    if(GetMajorAxis() == wxHORIZONTAL)
    {
        m_size_in_major_axis_for_children = width;
        if(m_scroll_buttons_visible)
        {
            if(m_scroll_left_btn)
                m_size_in_major_axis_for_children += m_scroll_left_btn->GetSize().GetWidth();
            if(m_scroll_right_btn)
                m_size_in_major_axis_for_children += m_scroll_right_btn->GetSize().GetWidth();
        }
    }
    else
    {
        m_size_in_major_axis_for_children = height;
        if(m_scroll_buttons_visible)
        {
            if(m_scroll_left_btn)
                m_size_in_major_axis_for_children += m_scroll_left_btn->GetSize().GetHeight();
            if(m_scroll_right_btn)
                m_size_in_major_axis_for_children += m_scroll_right_btn->GetSize().GetHeight();
        }
    }

    wxRibbonControl::DoSetSize(x, y, width, height, sizeFlags);
}

void wxRibbonPage::OnSize(wxSizeEvent& evt)
{
    wxSize new_size = evt.GetSize();

    if (m_art)
    {
        wxMemoryDC temp_dc;
        wxRect invalid_rect = m_art->GetPageBackgroundRedrawArea(temp_dc, this, m_old_size, new_size);
        Refresh(true, &invalid_rect);
    }

    m_old_size = new_size;

    if(new_size.GetX() > 0 && new_size.GetY() > 0)
    {
        Layout();
    }
    else
    {
        // Simplify other calculations by pretending new size is zero in both
        // X and Y
        new_size.Set(0, 0);
        // When size == 0, no point in doing any layout
    }

    evt.Skip();
}

void wxRibbonPage::RemoveChild(wxWindowBase *child)
{
    // Remove all references to the child from the collapse stack
    size_t count = m_collapse_stack.GetCount();
    size_t src, dst;
    for(src = 0, dst = 0; src < count; ++src, ++dst)
    {
        wxRibbonControl *item = m_collapse_stack.Item(src);
        if(item == child)
        {
            ++src;
            if(src == count)
            {
                break;
            }
        }
        if(src != dst)
        {
            m_collapse_stack.Item(dst) = item;
        }
    }
    if(src > dst)
    {
        m_collapse_stack.RemoveAt(dst, src - dst);
    }

    // ... and then proceed as normal
    wxRibbonControl::RemoveChild(child);
}

bool wxRibbonPage::Realize()
{
    bool status = true;

    m_collapse_stack.Clear();
    for (wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
                  node;
                  node = node->GetNext())
    {
        wxRibbonControl* child = wxDynamicCast(node->GetData(), wxRibbonControl);
        if(child == NULL)
        {
            continue;
        }
        if(!child->Realize())
        {
            status = false;
        }
    }
    PopulateSizeCalcArray(&wxWindow::GetMinSize);

    return DoActualLayout() && status;
}

void wxRibbonPage::PopulateSizeCalcArray(wxSize (wxWindow::*get_size)(void) const)
{
    wxSize parentSize = GetSize();
    parentSize.x -= m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_LEFT_SIZE);
    parentSize.x -= m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_RIGHT_SIZE);
    parentSize.y -= m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_TOP_SIZE);
    parentSize.y -= m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_BOTTOM_SIZE);

    if(m_size_calc_array_size != GetChildren().GetCount())
    {
        delete[] m_size_calc_array;
        m_size_calc_array_size = GetChildren().GetCount();
        m_size_calc_array = new wxSize[m_size_calc_array_size];
    }

    wxSize* node_size = m_size_calc_array;
    for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
          node;
          node = node->GetNext(), ++node_size )
    {
        wxWindow* child = node->GetData();
        wxRibbonPanel* panel = wxDynamicCast(child, wxRibbonPanel);
        if (panel && panel->GetFlags() & wxRIBBON_PANEL_FLEXIBLE)
            *node_size = panel->GetBestSizeForParentSize(parentSize);
        else
            *node_size = (child->*get_size)();
    }
}

bool wxRibbonPage::Layout()
{
    if(GetChildren().GetCount() == 0)
    {
        return true;
    }
    else
    {
        PopulateSizeCalcArray(&wxWindow::GetSize);
        return DoActualLayout();
    }
}

bool wxRibbonPage::DoActualLayout()
{
    wxPoint origin(m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_LEFT_SIZE), m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_TOP_SIZE));
    wxOrientation major_axis = GetMajorAxis();
    int gap;
    int minor_axis_size;
    int available_space;
    if(major_axis == wxHORIZONTAL)
    {
        gap = m_art->GetMetric(wxRIBBON_ART_PANEL_X_SEPARATION_SIZE);
        minor_axis_size = GetSize().GetHeight() - origin.y - m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_BOTTOM_SIZE);
        available_space = m_size_in_major_axis_for_children - m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_RIGHT_SIZE) - origin.x;
    }
    else
    {
        gap = m_art->GetMetric(wxRIBBON_ART_PANEL_Y_SEPARATION_SIZE);
        minor_axis_size = GetSize().GetWidth() - origin.x - m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_RIGHT_SIZE);
        available_space = m_size_in_major_axis_for_children - m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_BOTTOM_SIZE) - origin.y;
    }
    if (minor_axis_size < 0) minor_axis_size = 0;
    size_t size_index;
    for(size_index = 0; size_index < m_size_calc_array_size; ++size_index)
    {
        if(major_axis == wxHORIZONTAL)
        {
            available_space -= m_size_calc_array[size_index].GetWidth();
            m_size_calc_array[size_index].SetHeight(minor_axis_size);
        }
        else
        {
            available_space -= m_size_calc_array[size_index].GetHeight();
            m_size_calc_array[size_index].SetWidth(minor_axis_size);
        }
        if(size_index != 0)
            available_space -= gap;
    }
    bool todo_hide_scroll_buttons = false;
    bool todo_show_scroll_buttons = false;
    if(available_space >= 0)
    {
        if(m_scroll_buttons_visible)
            todo_hide_scroll_buttons = true;
        if(available_space > 0)
            ExpandPanels(major_axis, available_space);
    }
    else
    {
        if(m_scroll_buttons_visible)
        {
            // Scroll buttons already visible - not going to be able to downsize any more
            m_scroll_amount_limit = -available_space;
            if(m_scroll_amount > m_scroll_amount_limit)
            {
                m_scroll_amount = m_scroll_amount_limit;
                todo_show_scroll_buttons = true;
            }
        }
        else
        {
            if(!CollapsePanels(major_axis, -available_space))
            {
                m_scroll_amount = 0;
                m_scroll_amount_limit = -available_space;
                todo_show_scroll_buttons = true;
            }
        }
    }
    if(m_scroll_buttons_visible)
    {
        if(major_axis == wxHORIZONTAL)
        {
            origin.x -= m_scroll_amount;
            if(m_scroll_left_btn)
                origin.x -= m_scroll_left_btn->GetSize().GetWidth();
        }
        else
        {
            origin.y -= m_scroll_amount;
            if(m_scroll_left_btn)
                origin.y -= m_scroll_left_btn->GetSize().GetHeight();
        }
    }
    size_index = 0;
    for(wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
        node;
        node = node->GetNext(), ++size_index )
    {
        wxWindow* child = node->GetData();
        int w = m_size_calc_array[size_index].GetWidth();
        int h = m_size_calc_array[size_index].GetHeight();
        child->SetSize(origin.x, origin.y, w, h);
        if(major_axis == wxHORIZONTAL)
        {
            origin.x += w + gap;
        }
        else
        {
            origin.y += h + gap;
        }
    }

    if(todo_show_scroll_buttons)
        ShowScrollButtons();
    else if(todo_hide_scroll_buttons)
        HideScrollButtons();
    else if(m_scroll_buttons_visible)
        ShowScrollButtons();

    Refresh();
    return true;
}

bool wxRibbonPage::Show(bool show)
{
    if(m_scroll_left_btn)
        m_scroll_left_btn->Show(show);
    if(m_scroll_right_btn)
        m_scroll_right_btn->Show(show);
    return wxRibbonControl::Show(show);
}

void wxRibbonPage::HideScrollButtons()
{
    m_scroll_amount = 0;
    m_scroll_amount_limit = 0;
    ShowScrollButtons();
}

void wxRibbonPage::ShowScrollButtons()
{
    bool show_left = true;
    bool show_right = true;
    bool reposition = false;
    if(m_scroll_amount == 0)
    {
        show_left = false;
    }
    if(m_scroll_amount >= m_scroll_amount_limit)
    {
        show_right = false;
        m_scroll_amount = m_scroll_amount_limit;
    }
    m_scroll_buttons_visible = show_left || show_right;

    if(show_left)
    {
        wxMemoryDC temp_dc;
        wxSize size;
        long direction;
        if(GetMajorAxis() == wxHORIZONTAL)
        {
              direction = wxRIBBON_SCROLL_BTN_LEFT;
              size = m_art->GetScrollButtonMinimumSize(temp_dc, GetParent(), direction);
              size.SetHeight(GetSize().GetHeight());
        }
        else
        {
              direction = wxRIBBON_SCROLL_BTN_UP;
              size = m_art->GetScrollButtonMinimumSize(temp_dc, GetParent(), direction);
              size.SetWidth(GetSize().GetWidth());
        }
        if (m_scroll_left_btn)
        {
              m_scroll_left_btn->SetSize(size);
        }
        else
        {
              m_scroll_left_btn = new wxRibbonPageScrollButton(this, wxID_ANY, GetPosition(), size, direction);
              reposition = true;
        }
        if(!IsShown())
        {
              m_scroll_left_btn->Hide();
        }
    }
    else
    {
        if(m_scroll_left_btn != NULL)
        {
            m_scroll_left_btn->Destroy();
            m_scroll_left_btn = NULL;
            reposition = true;
        }
    }

    if(show_right)
    {
        wxMemoryDC temp_dc;
        wxSize size;
        long direction;
        if(GetMajorAxis() == wxHORIZONTAL)
        {
              direction = wxRIBBON_SCROLL_BTN_RIGHT;
              size = m_art->GetScrollButtonMinimumSize(temp_dc, GetParent(), direction);
              size.SetHeight(GetSize().GetHeight());
        }
        else
        {
              direction = wxRIBBON_SCROLL_BTN_DOWN;
              size = m_art->GetScrollButtonMinimumSize(temp_dc, GetParent(), direction);
              size.SetWidth(GetSize().GetWidth());
        }
        wxPoint initial_pos = GetPosition() + GetSize() - size;
        if (m_scroll_right_btn)
        {
              m_scroll_right_btn->SetSize(size);
        }
        else
        {
              m_scroll_right_btn = new wxRibbonPageScrollButton(this, wxID_ANY, initial_pos, size, direction);
              reposition = true;
        }
        if(!IsShown())
        {
              m_scroll_right_btn->Hide();
        }
    }
    else
    {
        if(m_scroll_right_btn != NULL)
        {
            m_scroll_right_btn->Destroy();
            m_scroll_right_btn = NULL;
            reposition = true;
        }
    }

    if(reposition)
    {
        wxDynamicCast(GetParent(), wxRibbonBar)->RepositionPage(this);
    }
}

static int GetSizeInOrientation(wxSize size, wxOrientation orientation)
{
    switch(orientation)
    {
    case wxHORIZONTAL: return size.GetWidth();
    case wxVERTICAL: return size.GetHeight();
    case wxBOTH: return size.GetWidth() * size.GetHeight();
    default: return 0;
    }
}

bool wxRibbonPage::ExpandPanels(wxOrientation direction, int maximum_amount)
{
    bool expanded_something = false;
    while(maximum_amount > 0)
    {
        int smallest_size = INT_MAX;
        wxRibbonPanel* smallest_panel = NULL;
        wxSize* smallest_panel_size = NULL;
        wxSize* panel_size = m_size_calc_array;
        for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
                  node;
                  node = node->GetNext(), ++panel_size )
        {
            wxRibbonPanel* panel = wxDynamicCast(node->GetData(), wxRibbonPanel);
            if(panel == NULL)
            {
                continue;
            }
            if (panel->GetFlags() & wxRIBBON_PANEL_FLEXIBLE)
            {
                // Don't change if it's flexible since we already calculated the
                // correct size for the panel.
            }
            else if(panel->IsSizingContinuous())
            {
                int size = GetSizeInOrientation(*panel_size, direction);
                if(size < smallest_size)
                {
                    smallest_size = size;
                    smallest_panel = panel;
                    smallest_panel_size = panel_size;
                }
            }
            else
            {
                int size = GetSizeInOrientation(*panel_size, direction);
                if(size < smallest_size)
                {
                    wxSize larger = panel->GetNextLargerSize(direction, *panel_size);
                    if(larger != (*panel_size) && GetSizeInOrientation(larger, direction) > size)
                    {
                        smallest_size = size;
                        smallest_panel = panel;
                        smallest_panel_size = panel_size;
                    }
                }
            }
        }
        if(smallest_panel != NULL)
        {
            if(smallest_panel->IsSizingContinuous())
            {
                int amount = maximum_amount;
                if(amount > 32)
                {
                    // For "large" growth, grow this panel a bit, and then re-allocate
                    // the remainder (which may come to this panel again anyway)
                    amount = 32;
                }
                if(direction & wxHORIZONTAL)
                {
                    smallest_panel_size->x += amount;
                }
                if(direction & wxVERTICAL)
                {
                    smallest_panel_size->y += amount;
                }
                maximum_amount -= amount;
                m_collapse_stack.Add(smallest_panel);
                expanded_something = true;
            }
            else
            {
                wxSize larger = smallest_panel->GetNextLargerSize(direction, *smallest_panel_size);
                wxSize delta = larger - (*smallest_panel_size);
                if(GetSizeInOrientation(delta, direction) <= maximum_amount)
                {
                    *smallest_panel_size = larger;
                    maximum_amount -= GetSizeInOrientation(delta, direction);
                    m_collapse_stack.Add(smallest_panel);
                    expanded_something = true;
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }
    return expanded_something;
}

bool wxRibbonPage::CollapsePanels(wxOrientation direction, int minimum_amount)
{
    bool collapsed_something = false;
    while(minimum_amount > 0)
    {
        int largest_size = 0;
        wxRibbonPanel* largest_panel = NULL;
        wxSize* largest_panel_size = NULL;
        wxSize* panel_size = m_size_calc_array;
        if(!m_collapse_stack.IsEmpty())
        {
            // For a more consistent panel layout, try to collapse panels which
            // were recently expanded.
            largest_panel = wxDynamicCast(m_collapse_stack.Last(), wxRibbonPanel);
            m_collapse_stack.RemoveAt(m_collapse_stack.GetCount() - 1);
            for(wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
                      node;
                      node = node->GetNext(), ++panel_size )
            {
                wxRibbonPanel* panel = wxDynamicCast(node->GetData(), wxRibbonPanel);
                if(panel == largest_panel)
                {
                    largest_panel_size = panel_size;
                    break;
                }
            }
        }
        else
        {
            for(wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
                      node;
                      node = node->GetNext(), ++panel_size )
            {
                wxRibbonPanel* panel = wxDynamicCast(node->GetData(), wxRibbonPanel);
                if(panel == NULL)
                {
                    continue;
                }
                if(panel->IsSizingContinuous())
                {
                    int size = GetSizeInOrientation(*panel_size, direction);
                    if(size > largest_size)
                    {
                        largest_size = size;
                        largest_panel = panel;
                        largest_panel_size = panel_size;
                    }
                }
                else
                {
                    int size = GetSizeInOrientation(*panel_size, direction);
                    if(size > largest_size)
                    {
                        wxSize smaller = panel->GetNextSmallerSize(direction, *panel_size);
                        if(smaller != (*panel_size) &&
                            GetSizeInOrientation(smaller, direction) < size)
                        {
                            largest_size = size;
                            largest_panel = panel;
                            largest_panel_size = panel_size;
                        }
                    }
                }
            }
        }
        if(largest_panel != NULL)
        {
            if(largest_panel->IsSizingContinuous())
            {
                int amount = minimum_amount;
                if(amount > 32)
                {
                    // For "large" contraction, reduce this panel a bit, and
                    // then re-allocate the remainder of the quota (which may
                    // come to this panel again anyway)
                    amount = 32;
                }
                if(direction & wxHORIZONTAL)
                {
                    largest_panel_size->x -= amount;
                }
                if(direction & wxVERTICAL)
                {
                    largest_panel_size->y -= amount;
                }
                minimum_amount -= amount;
                collapsed_something = true;
            }
            else
            {
                wxSize smaller = largest_panel->GetNextSmallerSize(direction, *largest_panel_size);
                wxSize delta = (*largest_panel_size) - smaller;
                *largest_panel_size = smaller;
                minimum_amount -= GetSizeInOrientation(delta, direction);
                collapsed_something = true;
            }
        }
        else
        {
            break;
        }
    }
    return collapsed_something;
}

bool wxRibbonPage::DismissExpandedPanel()
{
    for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
              node;
              node = node->GetNext() )
    {
        wxRibbonPanel* panel = wxDynamicCast(node->GetData(), wxRibbonPanel);
        if(panel == NULL)
        {
            continue;
        }
        if(panel->GetExpandedPanel() != NULL)
        {
            return panel->HideExpanded();
        }
    }
    return false;
}

wxSize wxRibbonPage::GetMinSize() const
{
    wxSize min(wxDefaultCoord, wxDefaultCoord);

    for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
          node;
          node = node->GetNext() )
    {
        wxWindow* child = node->GetData();
        wxSize child_min(child->GetMinSize());

        min.x = wxMax(min.x, child_min.x);
        min.y = wxMax(min.y, child_min.y);
    }

    if(GetMajorAxis() == wxHORIZONTAL)
    {
        min.x = wxDefaultCoord;
        if(min.y != wxDefaultCoord)
        {
            min.y += m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_TOP_SIZE) + m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_BOTTOM_SIZE);
        }
    }
    else
    {
        if(min.x != wxDefaultCoord)
        {
            min.x += m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_LEFT_SIZE) + m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_RIGHT_SIZE);
        }
        min.y = wxDefaultCoord;
    }

    return min;
}

wxSize wxRibbonPage::DoGetBestSize() const
{
    wxSize best(0, 0);
    size_t count = 0;

    if(GetMajorAxis() == wxHORIZONTAL)
    {
        best.y = wxDefaultCoord;

        for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
          node;
          node = node->GetNext() )
        {
            wxWindow* child = node->GetData();
            wxSize child_best(child->GetBestSize());

            if(child_best.x != wxDefaultCoord)
            {
                best.IncBy(child_best.x, 0);
            }
            best.y = wxMax(best.y, child_best.y);

            ++count;
        }

        if(count > 1)
        {
            best.IncBy((count - 1) * m_art->GetMetric(wxRIBBON_ART_PANEL_X_SEPARATION_SIZE), 0);
        }
    }
    else
    {
        best.x = wxDefaultCoord;

        for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
          node;
          node = node->GetNext() )
        {
            wxWindow* child = node->GetData();
            wxSize child_best(child->GetBestSize());

            best.x = wxMax(best.x, child_best.x);
            if(child_best.y != wxDefaultCoord)
            {
                best.IncBy(0, child_best.y);
            }

            ++count;
        }

        if(count > 1)
        {
            best.IncBy(0, (count - 1) * m_art->GetMetric(wxRIBBON_ART_PANEL_Y_SEPARATION_SIZE));
        }
    }

    if(best.x != wxDefaultCoord)
    {
        best.x += m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_LEFT_SIZE) + m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_RIGHT_SIZE);
    }
    if(best.y != wxDefaultCoord)
    {
        best.y += m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_TOP_SIZE) + m_art->GetMetric(wxRIBBON_ART_PAGE_BORDER_BOTTOM_SIZE);
    }
    return best;
}

void wxRibbonPage::HideIfExpanded()
{
    wxStaticCast(m_parent, wxRibbonBar)->HideIfExpanded();
}

#endif // wxUSE_RIBBON
