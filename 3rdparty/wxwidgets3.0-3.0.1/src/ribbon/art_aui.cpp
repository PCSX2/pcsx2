///////////////////////////////////////////////////////////////////////////////
// Name:        src/ribbon/art_aui.cpp
// Purpose:     AUI style art provider for ribbon interface
// Author:      Peter Cawley
// Modified by:
// Created:     2009-08-04
// Copyright:   (C) Peter Cawley
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_RIBBON

#include "wx/ribbon/art.h"
#include "wx/ribbon/art_internal.h"
#include "wx/ribbon/bar.h"
#include "wx/ribbon/buttonbar.h"
#include "wx/ribbon/gallery.h"
#include "wx/ribbon/toolbar.h"

#ifndef WX_PRECOMP
#include "wx/dc.h"
#include "wx/settings.h"
#endif

#ifdef __WXMSW__
#include "wx/msw/private.h"
#elif defined(__WXMAC__)
#include "wx/osx/private.h"
#endif

wxRibbonAUIArtProvider::wxRibbonAUIArtProvider()
    : wxRibbonMSWArtProvider(false)
{
#if defined( __WXMAC__ ) && wxOSX_USE_COCOA_OR_CARBON
    wxColor base_colour = wxColour( wxMacCreateCGColorFromHITheme(kThemeBrushToolbarBackground));
#else
    wxColor base_colour = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
#endif

    SetColourScheme(base_colour,
        wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT),
        wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));

    m_tab_active_label_font = m_tab_label_font;
    m_tab_active_label_font.SetWeight(wxFONTWEIGHT_BOLD);

    m_page_border_left = 1;
    m_page_border_right = 1;
    m_page_border_top = 1;
    m_page_border_bottom = 2;
    m_tab_separation_size = 0;

    m_gallery_bitmap_padding_left_size = 3;
    m_gallery_bitmap_padding_right_size = 3;
    m_gallery_bitmap_padding_top_size = 3;
    m_gallery_bitmap_padding_bottom_size = 3;
}

wxRibbonAUIArtProvider::~wxRibbonAUIArtProvider()
{
}

wxRibbonArtProvider* wxRibbonAUIArtProvider::Clone() const
{
    wxRibbonAUIArtProvider *copy = new wxRibbonAUIArtProvider();
    CloneTo(copy);

    copy->m_tab_ctrl_background_colour = m_tab_ctrl_background_colour;
    copy->m_tab_ctrl_background_gradient_colour = m_tab_ctrl_background_gradient_colour;
    copy->m_panel_label_background_colour = m_panel_label_background_colour;
    copy->m_panel_label_background_gradient_colour = m_panel_label_background_gradient_colour;
    copy->m_panel_hover_label_background_colour = m_panel_hover_label_background_colour;
    copy->m_panel_hover_label_background_gradient_colour = m_panel_hover_label_background_gradient_colour;

    copy->m_background_brush = m_background_brush;
    copy->m_tab_active_top_background_brush = m_tab_active_top_background_brush;
    copy->m_tab_hover_background_brush = m_tab_hover_background_brush;
    copy->m_button_bar_hover_background_brush = m_button_bar_hover_background_brush;
    copy->m_button_bar_active_background_brush = m_button_bar_active_background_brush;
    copy->m_gallery_button_active_background_brush = m_gallery_button_active_background_brush;
    copy->m_gallery_button_hover_background_brush = m_gallery_button_hover_background_brush;
    copy->m_gallery_button_disabled_background_brush = m_gallery_button_disabled_background_brush;

    copy->m_toolbar_hover_borden_pen = m_toolbar_hover_borden_pen;
    copy->m_tool_hover_background_brush = m_tool_hover_background_brush;
    copy->m_tool_active_background_brush = m_tool_active_background_brush;

    return copy;
}

void wxRibbonAUIArtProvider::SetFont(int id, const wxFont& font)
{
    wxRibbonMSWArtProvider::SetFont(id, font);
    if(id == wxRIBBON_ART_TAB_LABEL_FONT)
    {
        m_tab_active_label_font = m_tab_label_font;
        m_tab_active_label_font.SetWeight(wxFONTWEIGHT_BOLD);
    }
}

wxColour wxRibbonAUIArtProvider::GetColour(int id) const
{
    switch(id)
    {
    case wxRIBBON_ART_PAGE_BACKGROUND_COLOUR:
    case wxRIBBON_ART_PAGE_BACKGROUND_GRADIENT_COLOUR:
        return m_background_brush.GetColour();
    case wxRIBBON_ART_TAB_CTRL_BACKGROUND_COLOUR:
        return m_tab_ctrl_background_colour;
    case wxRIBBON_ART_TAB_CTRL_BACKGROUND_GRADIENT_COLOUR:
        return m_tab_ctrl_background_gradient_colour;
    case wxRIBBON_ART_TAB_ACTIVE_BACKGROUND_TOP_COLOUR:
    case wxRIBBON_ART_TAB_ACTIVE_BACKGROUND_TOP_GRADIENT_COLOUR:
        return m_tab_active_top_background_brush.GetColour();
    case wxRIBBON_ART_TAB_HOVER_BACKGROUND_COLOUR:
    case wxRIBBON_ART_TAB_HOVER_BACKGROUND_GRADIENT_COLOUR:
        return m_tab_hover_background_brush.GetColour();
    case wxRIBBON_ART_PANEL_LABEL_BACKGROUND_COLOUR:
        return m_panel_label_background_colour;
    case wxRIBBON_ART_PANEL_LABEL_BACKGROUND_GRADIENT_COLOUR:
        return m_panel_label_background_gradient_colour;
    case wxRIBBON_ART_PANEL_HOVER_LABEL_BACKGROUND_COLOUR:
        return m_panel_hover_label_background_colour;
    case wxRIBBON_ART_PANEL_HOVER_LABEL_BACKGROUND_GRADIENT_COLOUR:
        return m_panel_hover_label_background_gradient_colour;
    case wxRIBBON_ART_BUTTON_BAR_HOVER_BACKGROUND_COLOUR:
    case wxRIBBON_ART_BUTTON_BAR_HOVER_BACKGROUND_GRADIENT_COLOUR:
        return m_button_bar_hover_background_brush.GetColour();
    case wxRIBBON_ART_GALLERY_BUTTON_HOVER_BACKGROUND_COLOUR:
    case wxRIBBON_ART_GALLERY_BUTTON_HOVER_BACKGROUND_GRADIENT_COLOUR:
        return m_gallery_button_hover_background_brush.GetColour();
    case wxRIBBON_ART_GALLERY_BUTTON_ACTIVE_BACKGROUND_COLOUR:
    case wxRIBBON_ART_GALLERY_BUTTON_ACTIVE_BACKGROUND_GRADIENT_COLOUR:
        return m_gallery_button_active_background_brush.GetColour();
    case wxRIBBON_ART_GALLERY_BUTTON_DISABLED_BACKGROUND_COLOUR:
    case wxRIBBON_ART_GALLERY_BUTTON_DISABLED_BACKGROUND_GRADIENT_COLOUR:
        return m_gallery_button_disabled_background_brush.GetColour();
    default:
        return wxRibbonMSWArtProvider::GetColour(id);
    }
}

void wxRibbonAUIArtProvider::SetColour(int id, const wxColor& colour)
{
    switch(id)
    {
    case wxRIBBON_ART_PAGE_BACKGROUND_COLOUR:
    case wxRIBBON_ART_PAGE_BACKGROUND_GRADIENT_COLOUR:
        m_background_brush.SetColour(colour);
        break;
    case wxRIBBON_ART_TAB_CTRL_BACKGROUND_COLOUR:
        m_tab_ctrl_background_colour = colour;
        break;
    case wxRIBBON_ART_TAB_CTRL_BACKGROUND_GRADIENT_COLOUR:
        m_tab_ctrl_background_gradient_colour = colour;
        break;
    case wxRIBBON_ART_TAB_ACTIVE_BACKGROUND_TOP_COLOUR:
    case wxRIBBON_ART_TAB_ACTIVE_BACKGROUND_TOP_GRADIENT_COLOUR:
        m_tab_active_top_background_brush.SetColour(colour);
        break;
    case wxRIBBON_ART_TAB_HOVER_BACKGROUND_COLOUR:
    case wxRIBBON_ART_TAB_HOVER_BACKGROUND_GRADIENT_COLOUR:
        m_tab_hover_background_brush.SetColour(colour);
        break;
    case wxRIBBON_ART_PANEL_LABEL_BACKGROUND_COLOUR:
        m_panel_label_background_colour = colour;
        break;
    case wxRIBBON_ART_PANEL_LABEL_BACKGROUND_GRADIENT_COLOUR:
        m_panel_label_background_gradient_colour = colour;
        break;
    case wxRIBBON_ART_BUTTON_BAR_HOVER_BACKGROUND_COLOUR:
    case wxRIBBON_ART_BUTTON_BAR_HOVER_BACKGROUND_GRADIENT_COLOUR:
        m_button_bar_hover_background_brush.SetColour(colour);
        break;
    case wxRIBBON_ART_GALLERY_BUTTON_HOVER_BACKGROUND_COLOUR:
    case wxRIBBON_ART_GALLERY_BUTTON_HOVER_BACKGROUND_GRADIENT_COLOUR:
        m_gallery_button_hover_background_brush.SetColour(colour);
        break;
    case wxRIBBON_ART_GALLERY_BUTTON_ACTIVE_BACKGROUND_COLOUR:
    case wxRIBBON_ART_GALLERY_BUTTON_ACTIVE_BACKGROUND_GRADIENT_COLOUR:
        m_gallery_button_active_background_brush.SetColour(colour);
        break;
    case wxRIBBON_ART_GALLERY_BUTTON_DISABLED_BACKGROUND_COLOUR:
    case wxRIBBON_ART_GALLERY_BUTTON_DISABLED_BACKGROUND_GRADIENT_COLOUR:
        m_gallery_button_disabled_background_brush.SetColour(colour);
        break;
    default:
        wxRibbonMSWArtProvider::SetColour(id, colour);
        break;
    }
}

void wxRibbonAUIArtProvider::SetColourScheme(
                         const wxColour& primary,
                         const wxColour& secondary,
                         const wxColour& tertiary)
{
    wxRibbonHSLColour primary_hsl(primary);
    wxRibbonHSLColour secondary_hsl(secondary);
    wxRibbonHSLColour tertiary_hsl(tertiary);

    // Map primary & secondary luminance from [0, 1] to [0.15, 0.85]
    primary_hsl.luminance = cos(primary_hsl.luminance * M_PI) * -0.35 + 0.5;
    secondary_hsl.luminance = cos(secondary_hsl.luminance * M_PI) * -0.35 + 0.5;

    // TODO: Remove next line once this provider stops piggybacking MSW
    wxRibbonMSWArtProvider::SetColourScheme(primary, secondary, tertiary);

#define LikePrimary(luminance) \
    wxRibbonShiftLuminance(primary_hsl, luminance ## f).ToRGB()
#define LikeSecondary(luminance) \
    wxRibbonShiftLuminance(secondary_hsl, luminance ## f).ToRGB()

    m_tab_ctrl_background_colour = LikePrimary(0.9);
    m_tab_ctrl_background_gradient_colour = LikePrimary(1.7);
    m_tab_border_pen = LikePrimary(0.75);
    m_tab_label_colour = LikePrimary(0.1);
    m_tab_hover_background_top_colour =  primary_hsl.ToRGB();
    m_tab_hover_background_top_gradient_colour = LikePrimary(1.6);
    m_tab_hover_background_brush = m_tab_hover_background_top_colour;
    m_tab_active_background_colour = m_tab_ctrl_background_gradient_colour;
    m_tab_active_background_gradient_colour = primary_hsl.ToRGB();
    m_tab_active_top_background_brush = m_tab_active_background_colour;
    m_panel_label_colour = m_tab_label_colour;
    m_panel_minimised_label_colour = m_panel_label_colour;
    m_panel_hover_label_colour = tertiary_hsl.ToRGB();
    m_page_border_pen = m_tab_border_pen;
    m_panel_border_pen = m_tab_border_pen;
    m_background_brush = primary_hsl.ToRGB();
    m_page_hover_background_colour = LikePrimary(1.5);
    m_page_hover_background_gradient_colour = LikePrimary(0.9);
    m_panel_label_background_colour = LikePrimary(0.85);
    m_panel_label_background_gradient_colour = LikePrimary(0.97);
    m_panel_hover_label_background_gradient_colour = secondary_hsl.ToRGB();
    m_panel_hover_label_background_colour = secondary_hsl.Lighter(0.2f).ToRGB();
    m_button_bar_hover_border_pen = secondary_hsl.ToRGB();
    m_button_bar_hover_background_brush = LikeSecondary(1.7);
    m_button_bar_active_background_brush = LikeSecondary(1.4);
    m_button_bar_label_colour = m_tab_label_colour;
    m_button_bar_label_disabled_colour = m_tab_label_colour;
    m_gallery_border_pen = m_tab_border_pen;
    m_gallery_item_border_pen = m_button_bar_hover_border_pen;
    m_gallery_hover_background_brush = LikePrimary(1.2);
    m_gallery_button_background_colour = m_page_hover_background_colour;
    m_gallery_button_background_gradient_colour = m_page_hover_background_gradient_colour;
    m_gallery_button_hover_background_brush = m_button_bar_hover_background_brush;
    m_gallery_button_active_background_brush = m_button_bar_active_background_brush;
    m_gallery_button_disabled_background_brush = primary_hsl.Desaturated(0.15f).ToRGB();
    SetColour(wxRIBBON_ART_GALLERY_BUTTON_FACE_COLOUR, LikePrimary(0.1));
    SetColour(wxRIBBON_ART_GALLERY_BUTTON_DISABLED_FACE_COLOUR, wxColour(128, 128, 128));
    SetColour(wxRIBBON_ART_GALLERY_BUTTON_ACTIVE_FACE_COLOUR, LikeSecondary(0.1));
    SetColour(wxRIBBON_ART_GALLERY_BUTTON_HOVER_FACE_COLOUR, LikeSecondary(0.1));
    m_toolbar_border_pen = m_tab_border_pen;
    SetColour(wxRIBBON_ART_TOOLBAR_FACE_COLOUR, LikePrimary(0.1));
    m_tool_background_colour = m_page_hover_background_colour;
    m_tool_background_gradient_colour = m_page_hover_background_gradient_colour;
    m_toolbar_hover_borden_pen = m_button_bar_hover_border_pen;
    m_tool_hover_background_brush = m_button_bar_hover_background_brush;
    m_tool_active_background_brush = m_button_bar_active_background_brush;

#undef LikeSecondary
#undef LikePrimary
}

void wxRibbonAUIArtProvider::DrawTabCtrlBackground(
                        wxDC& dc,
                        wxWindow* WXUNUSED(wnd),
                        const wxRect& rect)
{
    wxRect gradient_rect(rect);
    gradient_rect.height--;
    dc.GradientFillLinear(gradient_rect, m_tab_ctrl_background_colour,
        m_tab_ctrl_background_gradient_colour, wxSOUTH);
    dc.SetPen(m_tab_border_pen);
    dc.DrawLine(rect.x, rect.GetBottom(), rect.GetRight()+1, rect.GetBottom());
}

int wxRibbonAUIArtProvider::GetTabCtrlHeight(
                        wxDC& dc,
                        wxWindow* WXUNUSED(wnd),
                        const wxRibbonPageTabInfoArray& pages)
{
    int text_height = 0;
    int icon_height = 0;

    if(pages.GetCount() <= 1 && (m_flags & wxRIBBON_BAR_ALWAYS_SHOW_TABS) == 0)
    {
        // To preserve space, a single tab need not be displayed. We still need
        // one pixel of border though.
        return 1;
    }

    if(m_flags & wxRIBBON_BAR_SHOW_PAGE_LABELS)
    {
        dc.SetFont(m_tab_active_label_font);
        text_height = dc.GetTextExtent(wxT("ABCDEFXj")).GetHeight();
    }
    if(m_flags & wxRIBBON_BAR_SHOW_PAGE_ICONS)
    {
        size_t numpages = pages.GetCount();
        for(size_t i = 0; i < numpages; ++i)
        {
            const wxRibbonPageTabInfo& info = pages.Item(i);
            if(info.page->GetIcon().IsOk())
            {
                icon_height = wxMax(icon_height, info.page->GetIcon().GetHeight());
            }
        }
    }

    return wxMax(text_height, icon_height) + 10;
}

void wxRibbonAUIArtProvider::DrawTab(wxDC& dc,
                 wxWindow* WXUNUSED(wnd),
                 const wxRibbonPageTabInfo& tab)
{
    if(tab.rect.height <= 1)
        return;

    dc.SetFont(m_tab_label_font);
    dc.SetPen(*wxTRANSPARENT_PEN);
    if(tab.active || tab.hovered || tab.highlight)
    {
        if(tab.active)
        {
            dc.SetFont(m_tab_active_label_font);
            dc.SetBrush(m_background_brush);
            dc.DrawRectangle(tab.rect.x, tab.rect.y + tab.rect.height - 1,
                tab.rect.width - 1, 1);
        }
        wxRect grad_rect(tab.rect);
        grad_rect.height -= 4;
        grad_rect.width -= 1;
        grad_rect.height /= 2;
        grad_rect.y = grad_rect.y + tab.rect.height - grad_rect.height - 1;
        dc.SetBrush(m_tab_active_top_background_brush);
        dc.DrawRectangle(tab.rect.x, tab.rect.y + 3, tab.rect.width - 1,
            grad_rect.y - tab.rect.y - 3);
        if(tab.highlight)
        {
            wxColour top_colour((m_tab_active_background_colour.Red()   + m_tab_hover_background_top_colour.Red())/2,
                                (m_tab_active_background_colour.Green() + m_tab_hover_background_top_colour.Green())/2,
                                (m_tab_active_background_colour.Blue()  + m_tab_hover_background_top_colour.Blue())/2);

            wxColour bottom_colour((m_tab_active_background_gradient_colour.Red()   + m_tab_hover_background_top_gradient_colour.Red())/2,
                                   (m_tab_active_background_gradient_colour.Green() + m_tab_hover_background_top_gradient_colour.Green())/2,
                                   (m_tab_active_background_gradient_colour.Blue()  + m_tab_hover_background_top_gradient_colour.Blue())/2);

            dc.GradientFillLinear(grad_rect, top_colour,
                bottom_colour, wxSOUTH);
        }
        else
        {
            dc.GradientFillLinear(grad_rect, m_tab_active_background_colour,
                m_tab_active_background_gradient_colour, wxSOUTH);
        }
    }
    else
    {
        wxRect btm_rect(tab.rect);
        btm_rect.height -= 4;
        btm_rect.width -= 1;
        btm_rect.height /= 2;
        btm_rect.y = btm_rect.y + tab.rect.height - btm_rect.height - 1;
        dc.SetBrush(m_tab_hover_background_brush);
        dc.DrawRectangle(btm_rect.x, btm_rect.y, btm_rect.width,
            btm_rect.height);
        wxRect grad_rect(tab.rect);
        grad_rect.width -= 1;
        grad_rect.y += 3;
        grad_rect.height = btm_rect.y - grad_rect.y;
        dc.GradientFillLinear(grad_rect, m_tab_hover_background_top_colour,
            m_tab_hover_background_top_gradient_colour, wxSOUTH);
    }

    wxPoint border_points[5];
    border_points[0] = wxPoint(0, 3);
    border_points[1] = wxPoint(1, 2);
    border_points[2] = wxPoint(tab.rect.width - 3, 2);
    border_points[3] = wxPoint(tab.rect.width - 1, 4);
    border_points[4] = wxPoint(tab.rect.width - 1, tab.rect.height - 1);

    dc.SetPen(m_tab_border_pen);
    dc.DrawLines(sizeof(border_points)/sizeof(wxPoint), border_points, tab.rect.x, tab.rect.y);

    wxRect old_clip;
    dc.GetClippingBox(old_clip);
    bool is_first_tab = false;
    wxRibbonBar* bar = wxDynamicCast(tab.page->GetParent(), wxRibbonBar);
    if(bar && bar->GetPage(0) == tab.page)
        is_first_tab = true;

    wxBitmap icon;
    if(m_flags & wxRIBBON_BAR_SHOW_PAGE_ICONS)
    {
        icon = tab.page->GetIcon();
        if((m_flags & wxRIBBON_BAR_SHOW_PAGE_LABELS) == 0)
        {
            if(icon.IsOk())
            {
            int x = tab.rect.x + (tab.rect.width - icon.GetWidth()) / 2;
            dc.DrawBitmap(icon, x, tab.rect.y + 1 + (tab.rect.height - 1 -
                icon.GetHeight()) / 2, true);
            }
        }
    }
    if(m_flags & wxRIBBON_BAR_SHOW_PAGE_LABELS)
    {
        wxString label = tab.page->GetLabel();
        if(!label.IsEmpty())
        {
            dc.SetTextForeground(m_tab_label_colour);
            dc.SetBackgroundMode(wxTRANSPARENT);

            int offset = 0;
            if(icon.IsOk())
                offset += icon.GetWidth() + 2;
            int text_height;
            int text_width;
            dc.GetTextExtent(label, &text_width, &text_height);
            int x = (tab.rect.width - 2 - text_width - offset) / 2;
            if(x > 8)
                x = 8;
            else if(x < 1)
                x = 1;
            int width = tab.rect.width - x - 2;
            x += tab.rect.x + offset;
            int y = tab.rect.y + (tab.rect.height - text_height) / 2;
            if(icon.IsOk())
            {
                dc.DrawBitmap(icon, x - offset, tab.rect.y + (tab.rect.height -
                    icon.GetHeight()) / 2, true);
            }
            dc.SetClippingRegion(x, tab.rect.y, width, tab.rect.height);
            dc.DrawText(label, x, y);
        }
    }

    // Draw the left hand edge of the tab only for the first tab (subsequent
    // tabs use the right edge of the prior tab as their left edge). As this is
    // outside the rectangle for the tab, only draw it if the leftmost part of
    // the tab is within the clip rectangle (the clip region has to be cleared
    // to draw outside the tab).
    if(is_first_tab && old_clip.x <= tab.rect.x
        && tab.rect.x < old_clip.x + old_clip.width)
    {
        dc.DestroyClippingRegion();
        dc.DrawLine(tab.rect.x - 1, tab.rect.y + 4, tab.rect.x - 1,
            tab.rect.y + tab.rect.height - 1);
    }
}

void wxRibbonAUIArtProvider::GetBarTabWidth(
                        wxDC& dc,
                        wxWindow* WXUNUSED(wnd),
                        const wxString& label,
                        const wxBitmap& bitmap,
                        int* ideal,
                        int* small_begin_need_separator,
                        int* small_must_have_separator,
                        int* minimum)
{
    int width = 0;
    int min = 0;
    if((m_flags & wxRIBBON_BAR_SHOW_PAGE_LABELS) && !label.IsEmpty())
    {
        dc.SetFont(m_tab_active_label_font);
        width += dc.GetTextExtent(label).GetWidth();
        min += wxMin(30, width); // enough for a few chars
        if(bitmap.IsOk())
        {
            // gap between label and bitmap
            width += 4;
            min += 2;
        }
    }
    if((m_flags & wxRIBBON_BAR_SHOW_PAGE_ICONS) && bitmap.IsOk())
    {
        width += bitmap.GetWidth();
        min += bitmap.GetWidth();
    }

    if(ideal != NULL)
    {
        *ideal = width + 16;
    }
    if(small_begin_need_separator != NULL)
    {
        *small_begin_need_separator = min;
    }
    if(small_must_have_separator != NULL)
    {
        *small_must_have_separator = min;
    }
    if(minimum != NULL)
    {
        *minimum = min;
    }
}

void wxRibbonAUIArtProvider::DrawTabSeparator(
                    wxDC& WXUNUSED(dc),
                    wxWindow* WXUNUSED(wnd),
                    const wxRect& WXUNUSED(rect),
                    double WXUNUSED(visibility))
{
    // No explicit separators between tabs
}

void wxRibbonAUIArtProvider::DrawPageBackground(
                        wxDC& dc,
                        wxWindow* WXUNUSED(wnd),
                        const wxRect& rect)
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(m_background_brush);
    dc.DrawRectangle(rect.x + 1, rect.y, rect.width - 2, rect.height - 1);

    dc.SetPen(m_page_border_pen);
    dc.DrawLine(rect.x, rect.y, rect.x, rect.y + rect.height);
    dc.DrawLine(rect.GetRight(), rect.y, rect.GetRight(), rect.y +rect.height);
    dc.DrawLine(rect.x, rect.GetBottom(), rect.GetRight()+1, rect.GetBottom());
}

wxSize wxRibbonAUIArtProvider::GetScrollButtonMinimumSize(
                        wxDC& WXUNUSED(dc),
                        wxWindow* WXUNUSED(wnd),
                        long WXUNUSED(style))
{
    return wxSize(11, 11);
}

void wxRibbonAUIArtProvider::DrawScrollButton(
                        wxDC& dc,
                        wxWindow* WXUNUSED(wnd),
                        const wxRect& rect,
                        long style)
{
    wxRect true_rect(rect);
    wxPoint arrow_points[3];

    if((style & wxRIBBON_SCROLL_BTN_FOR_MASK) == wxRIBBON_SCROLL_BTN_FOR_TABS)
    {
        true_rect.y += 2;
        true_rect.height -= 2;
        dc.SetPen(m_tab_border_pen);
    }
    else
    {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(m_background_brush);
        dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
        dc.SetPen(m_page_border_pen);
    }

    switch(style & wxRIBBON_SCROLL_BTN_DIRECTION_MASK)
    {
    case wxRIBBON_SCROLL_BTN_LEFT:
        dc.DrawLine(true_rect.GetRight(), true_rect.y, true_rect.GetRight(),
            true_rect.y + true_rect.height);
        arrow_points[0] = wxPoint(rect.width / 2 - 2, rect.height / 2);
        arrow_points[1] = arrow_points[0] + wxPoint(5, -5);
        arrow_points[2] = arrow_points[0] + wxPoint(5,  5);
        break;
    case wxRIBBON_SCROLL_BTN_RIGHT:
        dc.DrawLine(true_rect.x, true_rect.y, true_rect.x,
            true_rect.y + true_rect.height);
        arrow_points[0] = wxPoint(rect.width / 2 + 3, rect.height / 2);
        arrow_points[1] = arrow_points[0] - wxPoint(5, -5);
        arrow_points[2] = arrow_points[0] - wxPoint(5,  5);
        break;
    case wxRIBBON_SCROLL_BTN_DOWN:
        dc.DrawLine(true_rect.x, true_rect.y, true_rect.x + true_rect.width,
            true_rect.y);
        arrow_points[0] = wxPoint(rect.width / 2, rect.height / 2 + 3);
        arrow_points[1] = arrow_points[0] - wxPoint( 5, 5);
        arrow_points[2] = arrow_points[0] - wxPoint(-5, 5);
        break;
    case wxRIBBON_SCROLL_BTN_UP:
        dc.DrawLine(true_rect.x, true_rect.GetBottom(),
            true_rect.x + true_rect.width, true_rect.GetBottom());
        arrow_points[0] = wxPoint(rect.width / 2, rect.height / 2 - 2);
        arrow_points[1] = arrow_points[0] + wxPoint( 5, 5);
        arrow_points[2] = arrow_points[0] + wxPoint(-5, 5);
        break;
    default:
        return;
    }

    int x = rect.x;
    int y = rect.y;
    if(style & wxRIBBON_SCROLL_BTN_ACTIVE)
    {
        ++x;
        ++y;
    }

    dc.SetPen(*wxTRANSPARENT_PEN);
    wxBrush B(m_tab_label_colour);
    dc.SetBrush(B);
    dc.DrawPolygon(sizeof(arrow_points)/sizeof(wxPoint), arrow_points, x, y);
}

wxSize wxRibbonAUIArtProvider::GetPanelSize(
                        wxDC& dc,
                        const wxRibbonPanel* wnd,
                        wxSize client_size,
                        wxPoint* client_offset)
{
    dc.SetFont(m_panel_label_font);
    wxSize label_size = dc.GetTextExtent(wnd->GetLabel());
    int label_height = label_size.GetHeight() + 5;
    if(m_flags & wxRIBBON_BAR_FLOW_VERTICAL)
    {
        client_size.IncBy(4, label_height + 6);
        if(client_offset)
            *client_offset = wxPoint(2, label_height + 3);
    }
    else
    {
        client_size.IncBy(6, label_height + 4);
        if(client_offset)
            *client_offset = wxPoint(3, label_height + 2);
    }
    return client_size;
}

wxSize wxRibbonAUIArtProvider::GetPanelClientSize(
                        wxDC& dc,
                        const wxRibbonPanel* wnd,
                        wxSize size,
                        wxPoint* client_offset)
{
    dc.SetFont(m_panel_label_font);
    wxSize label_size = dc.GetTextExtent(wnd->GetLabel());
    int label_height = label_size.GetHeight() + 5;
    if(m_flags & wxRIBBON_BAR_FLOW_VERTICAL)
    {
        size.DecBy(4, label_height + 6);
        if(client_offset)
            *client_offset = wxPoint(2, label_height + 3);
    }
    else
    {
        size.DecBy(6, label_height + 4);
        if(client_offset)
            *client_offset = wxPoint(3, label_height + 2);
    }
    if (size.x < 0) size.x = 0;
    if (size.y < 0) size.y = 0;
    return size;
}

wxRect wxRibbonAUIArtProvider::GetPanelExtButtonArea(wxDC& dc,
                        const wxRibbonPanel* wnd,
                        wxRect rect)
{
    wxRect true_rect(rect);
    RemovePanelPadding(&true_rect);

    true_rect.x++;
    true_rect.width -= 2;
    true_rect.y++;

    dc.SetFont(m_panel_label_font);
    wxSize label_size = dc.GetTextExtent(wnd->GetLabel());
    int label_height = label_size.GetHeight() + 5;
    wxRect label_rect(true_rect);
    label_rect.height = label_height - 1;

    rect = wxRect(label_rect.GetRight()-13, label_rect.GetBottom()-13, 13, 13);
    return rect;
}

void wxRibbonAUIArtProvider::DrawPanelBackground(
                        wxDC& dc,
                        wxRibbonPanel* wnd,
                        const wxRect& rect)
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(m_background_brush);
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);

    wxRect true_rect(rect);
    RemovePanelPadding(&true_rect);

    dc.SetPen(m_panel_border_pen);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(true_rect.x, true_rect.y, true_rect.width, true_rect.height);

    true_rect.x++;
    true_rect.width -= 2;
    true_rect.y++;

    dc.SetFont(m_panel_label_font);
    wxSize label_size = dc.GetTextExtent(wnd->GetLabel());
    int label_height = label_size.GetHeight() + 5;
    wxRect label_rect(true_rect);
    label_rect.height = label_height - 1;
    dc.DrawLine(label_rect.x, label_rect.y + label_rect.height,
        label_rect.x + label_rect.width, label_rect.y + label_rect.height);

    wxColour label_bg_colour = m_panel_label_background_colour;
    wxColour label_bg_grad_colour = m_panel_label_background_gradient_colour;
    if(wnd->IsHovered())
    {
        label_bg_colour = m_panel_hover_label_background_colour;
        label_bg_grad_colour = m_panel_hover_label_background_gradient_colour;
        dc.SetTextForeground(m_panel_hover_label_colour);
    }
    else
    {
        dc.SetTextForeground(m_panel_label_colour);
    }
    dc.GradientFillLinear(label_rect,
#ifdef __WXMAC__
        label_bg_grad_colour, label_bg_colour, wxSOUTH);
#else
        label_bg_colour, label_bg_grad_colour, wxSOUTH);
#endif
    dc.SetFont(m_panel_label_font);
    dc.DrawText(wnd->GetLabel(), label_rect.x + 3, label_rect.y + 2);

    if(wnd->IsHovered())
    {
        wxRect gradient_rect(true_rect);
        gradient_rect.y += label_rect.height + 1;
        gradient_rect.height = true_rect.height - label_rect.height - 3;
#ifdef __WXMAC__
        wxColour colour = m_page_hover_background_gradient_colour;
        wxColour gradient = m_page_hover_background_colour;
#else
        wxColour colour = m_page_hover_background_colour;
        wxColour gradient = m_page_hover_background_gradient_colour;
#endif
        dc.GradientFillLinear(gradient_rect, colour, gradient, wxSOUTH);
    }

    if(wnd->HasExtButton())
    {
        if(wnd->IsExtButtonHovered())
        {
            dc.SetPen(m_panel_hover_button_border_pen);
            dc.SetBrush(m_panel_hover_button_background_brush);
            dc.DrawRoundedRectangle(label_rect.GetRight() - 13, label_rect.GetBottom() - 13, 13, 13, 1.0);
            dc.DrawBitmap(m_panel_extension_bitmap[1], label_rect.GetRight() - 10, label_rect.GetBottom() - 10, true);
        }
        else
            dc.DrawBitmap(m_panel_extension_bitmap[0], label_rect.GetRight() - 10, label_rect.GetBottom() - 10, true);
    }
}

void wxRibbonAUIArtProvider::DrawMinimisedPanel(
                        wxDC& dc,
                        wxRibbonPanel* wnd,
                        const wxRect& rect,
                        wxBitmap& bitmap)
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(m_background_brush);
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);

    wxRect true_rect(rect);
    RemovePanelPadding(&true_rect);

    dc.SetPen(m_panel_border_pen);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(true_rect.x, true_rect.y, true_rect.width, true_rect.height);
    true_rect.Deflate(1);

    if(wnd->IsHovered() || wnd->GetExpandedPanel())
    {
        wxColour colour = m_page_hover_background_colour;
        wxColour gradient = m_page_hover_background_gradient_colour;
#ifdef __WXMAC__
        if(!wnd->GetExpandedPanel())
#else
        if(wnd->GetExpandedPanel())
#endif
        {
            wxColour temp = colour;
            colour = gradient;
            gradient = temp;
        }
        dc.GradientFillLinear(true_rect, colour, gradient, wxSOUTH);
    }

    wxRect preview;
    DrawMinimisedPanelCommon(dc, wnd, true_rect, &preview);

    dc.SetPen(m_panel_border_pen);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(preview.x, preview.y, preview.width, preview.height);
    preview.Deflate(1);
    wxRect preview_caption_rect(preview);
    preview_caption_rect.height = 7;
    preview.y += preview_caption_rect.height;
    preview.height -= preview_caption_rect.height;
#ifdef __WXMAC__
    dc.GradientFillLinear(preview_caption_rect,
        m_panel_hover_label_background_gradient_colour,
        m_panel_hover_label_background_colour, wxSOUTH);
    dc.GradientFillLinear(preview,
        m_page_hover_background_gradient_colour,
        m_page_hover_background_colour, wxSOUTH);
#else
    dc.GradientFillLinear(preview_caption_rect,
        m_panel_hover_label_background_colour,
        m_panel_hover_label_background_gradient_colour, wxSOUTH);
    dc.GradientFillLinear(preview,
        m_page_hover_background_colour,
        m_page_hover_background_gradient_colour, wxSOUTH);
#endif

    if(bitmap.IsOk())
    {
        dc.DrawBitmap(bitmap, preview.x + (preview.width - bitmap.GetWidth()) / 2,
            preview.y + (preview.height - bitmap.GetHeight()) / 2, true);
    }
}

void wxRibbonAUIArtProvider::DrawPartialPanelBackground(wxDC& dc,
        wxWindow* wnd, const wxRect& rect)
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(m_background_brush);
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);

    wxPoint offset(wnd->GetPosition());
    wxWindow* parent = wnd->GetParent();
    wxRibbonPanel* panel = NULL;

    for(; parent; parent = parent->GetParent())
    {
        panel = wxDynamicCast(parent, wxRibbonPanel);
        if(panel != NULL)
        {
            if(!panel->IsHovered())
                return;
            break;
        }
        offset += parent->GetPosition();
    }
    if(panel == NULL)
        return;

    wxRect background(panel->GetSize());
    RemovePanelPadding(&background);
    background.x++;
    background.width -= 2;
    dc.SetFont(m_panel_label_font);
    int caption_height = dc.GetTextExtent(panel->GetLabel()).GetHeight() + 7;
    background.y += caption_height - 1;
    background.height -= caption_height;

    wxRect paint_rect(rect);
    paint_rect.x += offset.x;
    paint_rect.y += offset.y;

    wxColour bg_clr, bg_grad_clr;
#ifdef __WXMAC__
    bg_grad_clr = m_page_hover_background_colour;
    bg_clr = m_page_hover_background_gradient_colour;
#else
    bg_clr = m_page_hover_background_colour;
    bg_grad_clr = m_page_hover_background_gradient_colour;
#endif

    paint_rect.Intersect(background);
    if(!paint_rect.IsEmpty())
    {
        wxColour starting_colour(wxRibbonInterpolateColour(bg_clr, bg_grad_clr,
            paint_rect.y, background.y, background.y + background.height));
        wxColour ending_colour(wxRibbonInterpolateColour(bg_clr, bg_grad_clr,
            paint_rect.y + paint_rect.height, background.y,
            background.y + background.height));
        paint_rect.x -= offset.x;
        paint_rect.y -= offset.y;
        dc.GradientFillLinear(paint_rect, starting_colour, ending_colour
            , wxSOUTH);
    }
}

void wxRibbonAUIArtProvider::DrawGalleryBackground(
                        wxDC& dc,
                        wxRibbonGallery* wnd,
                        const wxRect& rect)
{
    DrawPartialPanelBackground(dc, wnd, rect);

    if(wnd->IsHovered())
    {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(m_gallery_hover_background_brush);
        if(m_flags & wxRIBBON_BAR_FLOW_VERTICAL)
        {
            dc.DrawRectangle(rect.x + 1, rect.y + 1, rect.width - 2,
                rect.height - 16);
        }
        else
        {
            dc.DrawRectangle(rect.x + 1, rect.y + 1, rect.width - 16,
                rect.height - 2);
        }
    }

    dc.SetPen(m_gallery_border_pen);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);

    DrawGalleryBackgroundCommon(dc, wnd, rect);
}

void wxRibbonAUIArtProvider::DrawGalleryButton(wxDC& dc, wxRect rect,
        wxRibbonGalleryButtonState state, wxBitmap* bitmaps)
{
    int extra_height = 0;
    int extra_width = 0;
    wxRect reduced_rect(rect);
    reduced_rect.Deflate(1);
    if(m_flags & wxRIBBON_BAR_FLOW_VERTICAL)
    {
        reduced_rect.width++;
        extra_width = 1;
    }
    else
    {
        reduced_rect.height++;
        extra_height = 1;
    }

    wxBitmap btn_bitmap;
    switch(state)
    {
    case wxRIBBON_GALLERY_BUTTON_NORMAL:
        dc.GradientFillLinear(reduced_rect,
            m_gallery_button_background_colour,
            m_gallery_button_background_gradient_colour, wxSOUTH);
        btn_bitmap = bitmaps[0];
        break;
    case wxRIBBON_GALLERY_BUTTON_HOVERED:
        dc.SetPen(m_gallery_item_border_pen);
        dc.SetBrush(m_gallery_button_hover_background_brush);
        dc.DrawRectangle(rect.x, rect.y, rect.width + extra_width,
            rect.height + extra_height);
        btn_bitmap = bitmaps[1];
        break;
    case wxRIBBON_GALLERY_BUTTON_ACTIVE:
        dc.SetPen(m_gallery_item_border_pen);
        dc.SetBrush(m_gallery_button_active_background_brush);
        dc.DrawRectangle(rect.x, rect.y, rect.width + extra_width,
            rect.height + extra_height);
        btn_bitmap = bitmaps[2];
        break;
    case wxRIBBON_GALLERY_BUTTON_DISABLED:
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(m_gallery_button_disabled_background_brush);
        dc.DrawRectangle(reduced_rect.x, reduced_rect.y, reduced_rect.width,
            reduced_rect.height);
        btn_bitmap = bitmaps[3];
        break;
    }

    dc.DrawBitmap(btn_bitmap, reduced_rect.x + reduced_rect.width / 2 - 2,
        (rect.y + rect.height / 2) - 2, true);
}

void wxRibbonAUIArtProvider::DrawGalleryItemBackground(
                        wxDC& dc,
                        wxRibbonGallery* wnd,
                        const wxRect& rect,
                        wxRibbonGalleryItem* item)
{
    if(wnd->GetHoveredItem() != item && wnd->GetActiveItem() != item &&
        wnd->GetSelection() != item)
        return;

    dc.SetPen(m_gallery_item_border_pen);
    if(wnd->GetActiveItem() == item || wnd->GetSelection() == item)
        dc.SetBrush(m_gallery_button_active_background_brush);
    else
        dc.SetBrush(m_gallery_button_hover_background_brush);

    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
}

void wxRibbonAUIArtProvider::DrawButtonBarBackground(
                        wxDC& dc,
                        wxWindow* wnd,
                        const wxRect& rect)
{
    DrawPartialPanelBackground(dc, wnd, rect);
}

void wxRibbonAUIArtProvider::DrawButtonBarButton(
                        wxDC& dc,
                        wxWindow* WXUNUSED(wnd),
                        const wxRect& rect,
                        wxRibbonButtonKind kind,
                        long state,
                        const wxString& label,
                        const wxBitmap& bitmap_large,
                        const wxBitmap& bitmap_small)
{
    if(kind == wxRIBBON_BUTTON_TOGGLE)
    {
        kind = wxRIBBON_BUTTON_NORMAL;
        if(state & wxRIBBON_BUTTONBAR_BUTTON_TOGGLED)
            state ^= wxRIBBON_BUTTONBAR_BUTTON_ACTIVE_MASK;
    }

    if(state & (wxRIBBON_BUTTONBAR_BUTTON_HOVER_MASK
        | wxRIBBON_BUTTONBAR_BUTTON_ACTIVE_MASK))
    {
        dc.SetPen(m_button_bar_hover_border_pen);

        wxRect bg_rect(rect);
        bg_rect.Deflate(1);

        if(kind == wxRIBBON_BUTTON_HYBRID)
        {
            switch(state & wxRIBBON_BUTTONBAR_BUTTON_SIZE_MASK)
            {
            case wxRIBBON_BUTTONBAR_BUTTON_LARGE:
                {
                    int iYBorder = rect.y + bitmap_large.GetHeight() + 4;
                    wxRect partial_bg(rect);
                    if(state & wxRIBBON_BUTTONBAR_BUTTON_NORMAL_HOVERED)
                    {
                        partial_bg.SetBottom(iYBorder - 1);
                    }
                    else
                    {
                        partial_bg.height -= (iYBorder - partial_bg.y + 1);
                        partial_bg.y = iYBorder + 1;
                    }
                    dc.DrawLine(rect.x, iYBorder, rect.x + rect.width, iYBorder);
                    bg_rect.Intersect(partial_bg);
                }
                break;
            case wxRIBBON_BUTTONBAR_BUTTON_MEDIUM:
                {
                    int iArrowWidth = 9;
                    if(state & wxRIBBON_BUTTONBAR_BUTTON_NORMAL_HOVERED)
                    {
                        bg_rect.width -= iArrowWidth;
                        dc.DrawLine(bg_rect.x + bg_rect.width,
                            rect.y, bg_rect.x + bg_rect.width,
                            rect.y + rect.height);
                    }
                    else
                    {
                        --iArrowWidth;
                        bg_rect.x += bg_rect.width - iArrowWidth;
                        bg_rect.width = iArrowWidth;
                        dc.DrawLine(bg_rect.x - 1, rect.y,
                            bg_rect.x - 1, rect.y + rect.height);
                    }
                }
                break;
            case wxRIBBON_BUTTONBAR_BUTTON_SMALL:
                break;
            }
        }

        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);

        dc.SetPen(*wxTRANSPARENT_PEN);
        if(state & wxRIBBON_BUTTONBAR_BUTTON_ACTIVE_MASK)
            dc.SetBrush(m_button_bar_active_background_brush);
        else
            dc.SetBrush(m_button_bar_hover_background_brush);
        dc.DrawRectangle(bg_rect.x, bg_rect.y, bg_rect.width, bg_rect.height);
    }

    dc.SetFont(m_button_bar_label_font);
    dc.SetTextForeground(state & wxRIBBON_BUTTONBAR_BUTTON_DISABLED
                            ? m_button_bar_label_disabled_colour
                            : m_button_bar_label_colour);
    DrawButtonBarButtonForeground(dc, rect, kind, state, label, bitmap_large,
        bitmap_small);
}

void wxRibbonAUIArtProvider::DrawToolBarBackground(
                        wxDC& dc,
                        wxWindow* wnd,
                        const wxRect& rect)
{
    DrawPartialPanelBackground(dc, wnd, rect);
}

void wxRibbonAUIArtProvider::DrawToolGroupBackground(
                    wxDC& dc,
                    wxWindow* WXUNUSED(wnd),
                    const wxRect& rect)
{
    dc.SetPen(m_toolbar_border_pen);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
    wxRect bg_rect(rect);
    bg_rect.Deflate(1);
    dc.GradientFillLinear(bg_rect, m_tool_background_colour,
        m_tool_background_gradient_colour, wxSOUTH);
}

void wxRibbonAUIArtProvider::DrawTool(
            wxDC& dc,
            wxWindow* WXUNUSED(wnd),
            const wxRect& rect,
            const wxBitmap& bitmap,
            wxRibbonButtonKind kind,
            long state)
{
    if(kind == wxRIBBON_BUTTON_TOGGLE)
    {
        if(state & wxRIBBON_TOOLBAR_TOOL_TOGGLED)
            state ^= wxRIBBON_TOOLBAR_TOOL_ACTIVE_MASK;
    }

    wxRect bg_rect(rect);
    bg_rect.Deflate(1);
    if((state & wxRIBBON_TOOLBAR_TOOL_LAST) == 0)
        bg_rect.width++;
    bool is_custom_bg = (state & (wxRIBBON_TOOLBAR_TOOL_HOVER_MASK |
        wxRIBBON_TOOLBAR_TOOL_ACTIVE_MASK)) != 0;
    bool is_split_hybrid = kind == wxRIBBON_BUTTON_HYBRID && is_custom_bg;

    // Background
    if(is_custom_bg)
    {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(m_tool_hover_background_brush);
        dc.DrawRectangle(bg_rect.x, bg_rect.y, bg_rect.width, bg_rect.height);
        if(state & wxRIBBON_TOOLBAR_TOOL_ACTIVE_MASK)
        {
            wxRect active_rect(bg_rect);
            if(kind == wxRIBBON_BUTTON_HYBRID)
            {
                active_rect.width -= 8;
                if(state & wxRIBBON_TOOLBAR_TOOL_DROPDOWN_ACTIVE)
                {
                    active_rect.x += active_rect.width;
                    active_rect.width = 8;
                }
            }
            dc.SetBrush(m_tool_active_background_brush);
            dc.DrawRectangle(active_rect.x, active_rect.y, active_rect.width,
                active_rect.height);
        }
    }

    // Border
    if(is_custom_bg)
        dc.SetPen(m_toolbar_hover_borden_pen);
    else
        dc.SetPen(m_toolbar_border_pen);
    if((state & wxRIBBON_TOOLBAR_TOOL_FIRST) == 0)
    {
        wxColour existing;
        if(!dc.GetPixel(rect.x, rect.y + 1, &existing) ||
            existing != m_toolbar_hover_borden_pen.GetColour())
        {
            dc.DrawLine(rect.x, rect.y + 1, rect.x, rect.y + rect.height - 1);
        }
    }
    if(is_custom_bg)
    {
        wxRect border_rect(bg_rect);
        border_rect.Inflate(1);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(border_rect.x, border_rect.y, border_rect.width,
            border_rect.height);
    }

    // Foreground
    int avail_width = bg_rect.GetWidth();
    if(kind & wxRIBBON_BUTTON_DROPDOWN)
    {
        avail_width -= 8;
        if(is_split_hybrid)
        {
            dc.DrawLine(rect.x + avail_width + 1, rect.y,
                rect.x + avail_width + 1, rect.y + rect.height);
        }
        dc.DrawBitmap(m_toolbar_drop_bitmap, bg_rect.x + avail_width + 2,
            bg_rect.y + (bg_rect.height / 2) - 2, true);
    }
    dc.DrawBitmap(bitmap, bg_rect.x + (avail_width - bitmap.GetWidth()) / 2,
        bg_rect.y + (bg_rect.height - bitmap.GetHeight()) / 2, true);
}

#endif // wxUSE_RIBBON
