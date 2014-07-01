///////////////////////////////////////////////////////////////////////////////
// Name:        src/ribbon/buttonbar.cpp
// Purpose:     Ribbon control similar to a tool bar
// Author:      Peter Cawley
// Modified by:
// Created:     2009-07-01
// Copyright:   (C) Peter Cawley
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_RIBBON

#include "wx/ribbon/panel.h"
#include "wx/ribbon/buttonbar.h"
#include "wx/ribbon/art.h"
#include "wx/dcbuffer.h"

#ifndef WX_PRECOMP
#endif

#ifdef __WXMSW__
#include "wx/msw/private.h"
#endif

wxDEFINE_EVENT(wxEVT_RIBBONBUTTONBAR_CLICKED, wxRibbonButtonBarEvent);
wxDEFINE_EVENT(wxEVT_RIBBONBUTTONBAR_DROPDOWN_CLICKED, wxRibbonButtonBarEvent);

IMPLEMENT_DYNAMIC_CLASS(wxRibbonButtonBarEvent, wxCommandEvent)
IMPLEMENT_CLASS(wxRibbonButtonBar, wxRibbonControl)

BEGIN_EVENT_TABLE(wxRibbonButtonBar, wxRibbonControl)
    EVT_ERASE_BACKGROUND(wxRibbonButtonBar::OnEraseBackground)
    EVT_ENTER_WINDOW(wxRibbonButtonBar::OnMouseEnter)
    EVT_LEAVE_WINDOW(wxRibbonButtonBar::OnMouseLeave)
    EVT_MOTION(wxRibbonButtonBar::OnMouseMove)
    EVT_PAINT(wxRibbonButtonBar::OnPaint)
    EVT_SIZE(wxRibbonButtonBar::OnSize)
    EVT_LEFT_DOWN(wxRibbonButtonBar::OnMouseDown)
    EVT_LEFT_UP(wxRibbonButtonBar::OnMouseUp)
END_EVENT_TABLE()

class wxRibbonButtonBarButtonSizeInfo
{
public:
    bool is_supported;
    wxSize size;
    wxRect normal_region;
    wxRect dropdown_region;
};

class wxRibbonButtonBarButtonInstance
{
public:
    wxPoint position;
    wxRibbonButtonBarButtonBase* base;
    wxRibbonButtonBarButtonState size;
};

class wxRibbonButtonBarButtonBase
{
public:
    wxRibbonButtonBarButtonInstance NewInstance()
    {
        wxRibbonButtonBarButtonInstance i;
        i.base = this;
        return i;
    }

    wxRibbonButtonBarButtonState GetLargestSize()
    {
        if(sizes[wxRIBBON_BUTTONBAR_BUTTON_LARGE].is_supported)
            return wxRIBBON_BUTTONBAR_BUTTON_LARGE;
        if(sizes[wxRIBBON_BUTTONBAR_BUTTON_MEDIUM].is_supported)
            return wxRIBBON_BUTTONBAR_BUTTON_MEDIUM;
        wxASSERT(sizes[wxRIBBON_BUTTONBAR_BUTTON_SMALL].is_supported);
        return wxRIBBON_BUTTONBAR_BUTTON_SMALL;
    }

    bool GetSmallerSize(
        wxRibbonButtonBarButtonState* size, int n = 1)
    {
        for(; n > 0; --n)
        {
            switch(*size)
            {
            case wxRIBBON_BUTTONBAR_BUTTON_LARGE:
                if(sizes[wxRIBBON_BUTTONBAR_BUTTON_MEDIUM].is_supported)
                {
                    *size = wxRIBBON_BUTTONBAR_BUTTON_MEDIUM;
                    break;
                }
            case wxRIBBON_BUTTONBAR_BUTTON_MEDIUM:
                if(sizes[wxRIBBON_BUTTONBAR_BUTTON_SMALL].is_supported)
                {
                    *size = wxRIBBON_BUTTONBAR_BUTTON_SMALL;
                    break;
                }
            case wxRIBBON_BUTTONBAR_BUTTON_SMALL:
            default:
                return false;
            }
        }
        return true;
    }

    wxString label;
    wxString help_string;
    wxBitmap bitmap_large;
    wxBitmap bitmap_large_disabled;
    wxBitmap bitmap_small;
    wxBitmap bitmap_small_disabled;
    wxRibbonButtonBarButtonSizeInfo sizes[3];
    wxClientDataContainer client_data;
    int id;
    wxRibbonButtonKind kind;
    long state;
};

WX_DECLARE_OBJARRAY(wxRibbonButtonBarButtonInstance, wxArrayRibbonButtonBarButtonInstance);
#include "wx/arrimpl.cpp"
WX_DEFINE_OBJARRAY(wxArrayRibbonButtonBarButtonInstance)

class wxRibbonButtonBarLayout
{
public:
    wxSize overall_size;
    wxArrayRibbonButtonBarButtonInstance buttons;

    void CalculateOverallSize()
    {
        overall_size = wxSize(0, 0);
        size_t btn_count = buttons.Count();
        size_t btn_i;
        for(btn_i = 0; btn_i < btn_count; ++btn_i)
        {
            wxRibbonButtonBarButtonInstance& instance = buttons.Item(btn_i);
            wxSize size = instance.base->sizes[instance.size].size;
            int right = instance.position.x + size.GetWidth();
            int bottom = instance.position.y + size.GetHeight();
            if(right > overall_size.GetWidth())
            {
                overall_size.SetWidth(right);
            }
            if(bottom > overall_size.GetHeight())
            {
                overall_size.SetHeight(bottom);
            }
        }
    }

    wxRibbonButtonBarButtonInstance* FindSimilarInstance(
        wxRibbonButtonBarButtonInstance* inst)
    {
        if(inst == NULL)
        {
            return NULL;
        }
        size_t btn_count = buttons.Count();
        size_t btn_i;
        for(btn_i = 0; btn_i < btn_count; ++btn_i)
        {
            wxRibbonButtonBarButtonInstance& instance = buttons.Item(btn_i);
            if(instance.base == inst->base)
            {
                return &instance;
            }
        }
        return NULL;
    }
};

wxRibbonButtonBar::wxRibbonButtonBar()
{
    m_layouts_valid = false;
    CommonInit (0);
}

wxRibbonButtonBar::wxRibbonButtonBar(wxWindow* parent,
                  wxWindowID id,
                  const wxPoint& pos,
                  const wxSize& size,
                  long style)
    : wxRibbonControl(parent, id, pos, size, wxBORDER_NONE)
{
    m_layouts_valid = false;

    CommonInit(style);
}

wxRibbonButtonBar::~wxRibbonButtonBar()
{
    size_t count = m_buttons.GetCount();
    size_t i;
    for(i = 0; i < count; ++i)
    {
        wxRibbonButtonBarButtonBase* button = m_buttons.Item(i);
        delete button;
    }
    m_buttons.Clear();

    count = m_layouts.GetCount();
    for(i = 0; i < count; ++i)
    {
        wxRibbonButtonBarLayout* layout = m_layouts.Item(i);
        delete layout;
    }
    m_layouts.Clear();
}

bool wxRibbonButtonBar::Create(wxWindow* parent,
                wxWindowID id,
                const wxPoint& pos,
                const wxSize& size,
                long style)
{
    if(!wxRibbonControl::Create(parent, id, pos, size, wxBORDER_NONE))
    {
        return false;
    }

    CommonInit(style);
    return true;
}

wxRibbonButtonBarButtonBase* wxRibbonButtonBar::AddButton(
                int button_id,
                const wxString& label,
                const wxBitmap& bitmap,
                const wxString& help_string,
                wxRibbonButtonKind kind)
{
    return AddButton(button_id, label, bitmap, wxNullBitmap, wxNullBitmap,
        wxNullBitmap, kind, help_string);
}

wxRibbonButtonBarButtonBase* wxRibbonButtonBar::AddDropdownButton(
                int button_id,
                const wxString& label,
                const wxBitmap& bitmap,
                const wxString& help_string)
{
    return AddButton(button_id, label, bitmap, help_string,
        wxRIBBON_BUTTON_DROPDOWN);
}

wxRibbonButtonBarButtonBase* wxRibbonButtonBar::AddToggleButton(
                int button_id,
                const wxString& label,
                const wxBitmap& bitmap,
                const wxString& help_string)
{
    return AddButton(button_id, label, bitmap, help_string,
        wxRIBBON_BUTTON_TOGGLE);
}

wxRibbonButtonBarButtonBase* wxRibbonButtonBar::AddHybridButton(
                int button_id,
                const wxString& label,
                const wxBitmap& bitmap,
                const wxString& help_string)
{
    return AddButton(button_id, label, bitmap, help_string,
        wxRIBBON_BUTTON_HYBRID);
}

wxRibbonButtonBarButtonBase* wxRibbonButtonBar::AddButton(
                int button_id,
                const wxString& label,
                const wxBitmap& bitmap,
                const wxBitmap& bitmap_small,
                const wxBitmap& bitmap_disabled,
                const wxBitmap& bitmap_small_disabled,
                wxRibbonButtonKind kind,
                const wxString& help_string)
{
    return InsertButton(GetButtonCount(), button_id, label, bitmap,
        bitmap_small, bitmap_disabled,bitmap_small_disabled, kind, help_string);
}

wxRibbonButtonBarButtonBase* wxRibbonButtonBar::InsertButton(
                size_t pos,
                int button_id,
                const wxString& label,
                const wxBitmap& bitmap,
                const wxBitmap& bitmap_small,
                const wxBitmap& bitmap_disabled,
                const wxBitmap& bitmap_small_disabled,
                wxRibbonButtonKind kind,
                const wxString& help_string)
{
    wxASSERT(bitmap.IsOk() || bitmap_small.IsOk());
    if(m_buttons.IsEmpty())
    {
        if(bitmap.IsOk())
        {
            m_bitmap_size_large = bitmap.GetSize();
            if(!bitmap_small.IsOk())
            {
                m_bitmap_size_small = m_bitmap_size_large;
                m_bitmap_size_small *= 0.5;
            }
        }
        if(bitmap_small.IsOk())
        {
            m_bitmap_size_small = bitmap_small.GetSize();
            if(!bitmap.IsOk())
            {
                m_bitmap_size_large = m_bitmap_size_small;
                m_bitmap_size_large *= 2.0;
            }
        }
    }

    wxRibbonButtonBarButtonBase* base = new wxRibbonButtonBarButtonBase;
    base->id = button_id;
    base->label = label;
    base->bitmap_large = bitmap;
    if(!base->bitmap_large.IsOk())
    {
        base->bitmap_large = MakeResizedBitmap(base->bitmap_small,
            m_bitmap_size_large);
    }
    else if(base->bitmap_large.GetSize() != m_bitmap_size_large)
    {
        base->bitmap_large = MakeResizedBitmap(base->bitmap_large,
            m_bitmap_size_large);
    }
    base->bitmap_small = bitmap_small;
    if(!base->bitmap_small.IsOk())
    {
        base->bitmap_small = MakeResizedBitmap(base->bitmap_large,
            m_bitmap_size_small);
    }
    else if(base->bitmap_small.GetSize() != m_bitmap_size_small)
    {
        base->bitmap_small = MakeResizedBitmap(base->bitmap_small,
            m_bitmap_size_small);
    }
    base->bitmap_large_disabled = bitmap_disabled;
    if(!base->bitmap_large_disabled.IsOk())
    {
        base->bitmap_large_disabled = MakeDisabledBitmap(base->bitmap_large);
    }
    base->bitmap_small_disabled = bitmap_small_disabled;
    if(!base->bitmap_small_disabled.IsOk())
    {
        base->bitmap_small_disabled = MakeDisabledBitmap(base->bitmap_small);
    }
    base->kind = kind;
    base->help_string = help_string;
    base->state = 0;

    wxClientDC temp_dc(this);
    FetchButtonSizeInfo(base, wxRIBBON_BUTTONBAR_BUTTON_SMALL, temp_dc);
    FetchButtonSizeInfo(base, wxRIBBON_BUTTONBAR_BUTTON_MEDIUM, temp_dc);
    FetchButtonSizeInfo(base, wxRIBBON_BUTTONBAR_BUTTON_LARGE, temp_dc);

    m_buttons.Insert(base, pos);
    m_layouts_valid = false;
    return base;
}


void
wxRibbonButtonBar::SetItemClientObject(wxRibbonButtonBarButtonBase* item,
                                       wxClientData* data)
{
    wxCHECK_RET( item, "Can't associate client object with an invalid item" );

    item->client_data.SetClientObject(data);
}

wxClientData*
wxRibbonButtonBar::GetItemClientObject(const wxRibbonButtonBarButtonBase* item) const
{
    wxCHECK_MSG( item, NULL, "Can't get client object for an invalid item" );

    return item->client_data.GetClientObject();
}

void
wxRibbonButtonBar::SetItemClientData(wxRibbonButtonBarButtonBase* item,
                                     void* data)
{
    wxCHECK_RET( item, "Can't associate client data with an invalid item" );

    item->client_data.SetClientData(data);
}

void*
wxRibbonButtonBar::GetItemClientData(const wxRibbonButtonBarButtonBase* item) const
{
    wxCHECK_MSG( item, NULL, "Can't get client data for an invalid item" );

    return item->client_data.GetClientData();
}


wxRibbonButtonBarButtonBase* wxRibbonButtonBar::InsertButton(
                size_t pos,
                int button_id,
                const wxString& label,
                const wxBitmap& bitmap,
                const wxString& help_string,
                wxRibbonButtonKind kind)
{
    return InsertButton(pos, button_id, label, bitmap, wxNullBitmap,
        wxNullBitmap, wxNullBitmap, kind, help_string);
}

wxRibbonButtonBarButtonBase* wxRibbonButtonBar::InsertDropdownButton(
                size_t pos,
                int button_id,
                const wxString& label,
                const wxBitmap& bitmap,
                const wxString& help_string)
{
    return InsertButton(pos, button_id, label, bitmap, help_string,
        wxRIBBON_BUTTON_DROPDOWN);
}

wxRibbonButtonBarButtonBase* wxRibbonButtonBar::InsertToggleButton(
                size_t pos,
                int button_id,
                const wxString& label,
                const wxBitmap& bitmap,
                const wxString& help_string)
{
    return InsertButton(pos, button_id, label, bitmap, help_string,
        wxRIBBON_BUTTON_TOGGLE);
}

wxRibbonButtonBarButtonBase* wxRibbonButtonBar::InsertHybridButton(
                size_t pos,
                int button_id,
                const wxString& label,
                const wxBitmap& bitmap,
                const wxString& help_string)
{
    return InsertButton(pos, button_id, label, bitmap, help_string,
        wxRIBBON_BUTTON_HYBRID);
}

void wxRibbonButtonBar::FetchButtonSizeInfo(wxRibbonButtonBarButtonBase* button,
        wxRibbonButtonBarButtonState size, wxDC& dc)
{
    wxRibbonButtonBarButtonSizeInfo& info = button->sizes[size];
    if(m_art)
    {
        info.is_supported = m_art->GetButtonBarButtonSize(dc, this,
            button->kind, size, button->label, m_bitmap_size_large,
            m_bitmap_size_small, &info.size, &info.normal_region,
            &info.dropdown_region);
    }
    else
        info.is_supported = false;
}

wxBitmap wxRibbonButtonBar::MakeResizedBitmap(const wxBitmap& original, wxSize size)
{
    wxImage img(original.ConvertToImage());
    img.Rescale(size.GetWidth(), size.GetHeight(), wxIMAGE_QUALITY_HIGH);
    return wxBitmap(img);
}

wxBitmap wxRibbonButtonBar::MakeDisabledBitmap(const wxBitmap& original)
{
    wxImage img(original.ConvertToImage());
    return wxBitmap(img.ConvertToGreyscale());
}

size_t wxRibbonButtonBar::GetButtonCount() const
{
    return m_buttons.GetCount();
}

bool wxRibbonButtonBar::Realize()
{
    if(!m_layouts_valid)
    {
        MakeLayouts();
        m_layouts_valid = true;
    }
    return true;
}

void wxRibbonButtonBar::ClearButtons()
{
    m_layouts_valid = false;
    size_t count = m_buttons.GetCount();
    size_t i;
    for(i = 0; i < count; ++i)
    {
        wxRibbonButtonBarButtonBase* button = m_buttons.Item(i);
        delete button;
    }
    m_buttons.Clear();
    Realize();
}

bool wxRibbonButtonBar::DeleteButton(int button_id)
{
    size_t count = m_buttons.GetCount();
    size_t i;
    for(i = 0; i < count; ++i)
    {
        wxRibbonButtonBarButtonBase* button = m_buttons.Item(i);
        if(button->id == button_id)
        {
            m_layouts_valid = false;
            m_buttons.RemoveAt(i);
            Realize();
            Refresh();
            return true;
        }
    }
    return false;
}

void wxRibbonButtonBar::EnableButton(int button_id, bool enable)
{
    size_t count = m_buttons.GetCount();
    size_t i;
    for(i = 0; i < count; ++i)
    {
        wxRibbonButtonBarButtonBase* button = m_buttons.Item(i);
        if(button->id == button_id)
        {
            if(enable)
            {
                if(button->state & wxRIBBON_BUTTONBAR_BUTTON_DISABLED)
                {
                    button->state &= ~wxRIBBON_BUTTONBAR_BUTTON_DISABLED;
                    Refresh();
                }
            }
            else
            {
                if((button->state & wxRIBBON_BUTTONBAR_BUTTON_DISABLED) == 0)
                {
                    button->state |= wxRIBBON_BUTTONBAR_BUTTON_DISABLED;
                    Refresh();
                }
            }
            return;
        }
    }
}

void wxRibbonButtonBar::ToggleButton(int button_id, bool checked)
{
    size_t count = m_buttons.GetCount();
    size_t i;
    for(i = 0; i < count; ++i)
    {
        wxRibbonButtonBarButtonBase* button = m_buttons.Item(i);
        if(button->id == button_id)
        {
            if(checked)
            {
                if((button->state & wxRIBBON_BUTTONBAR_BUTTON_TOGGLED) == 0)
                {
                    button->state |= wxRIBBON_BUTTONBAR_BUTTON_TOGGLED;
                    Refresh();
                }
            }
            else
            {
                if(button->state & wxRIBBON_BUTTONBAR_BUTTON_TOGGLED)
                {
                    button->state &= ~wxRIBBON_BUTTONBAR_BUTTON_TOGGLED;
                    Refresh();
                }
            }
            return;
        }
    }
}

void wxRibbonButtonBar::SetArtProvider(wxRibbonArtProvider* art)
{
    if(art == m_art)
    {
        return;
    }

    wxRibbonControl::SetArtProvider(art);

    wxClientDC temp_dc(this);
    size_t btn_count = m_buttons.Count();
    size_t btn_i;
    for(btn_i = 0; btn_i < btn_count; ++btn_i)
    {
        wxRibbonButtonBarButtonBase* base = m_buttons.Item(btn_i);

        FetchButtonSizeInfo(base, wxRIBBON_BUTTONBAR_BUTTON_SMALL, temp_dc);
        FetchButtonSizeInfo(base, wxRIBBON_BUTTONBAR_BUTTON_MEDIUM, temp_dc);
        FetchButtonSizeInfo(base, wxRIBBON_BUTTONBAR_BUTTON_LARGE, temp_dc);
    }

    m_layouts_valid = false;
    Realize();
}

bool wxRibbonButtonBar::IsSizingContinuous() const
{
    return false;
}

wxSize wxRibbonButtonBar::DoGetNextSmallerSize(wxOrientation direction,
                                             wxSize result) const
{
    size_t nlayouts = m_layouts.GetCount();
    size_t i;
    for(i = 0; i < nlayouts; ++i)
    {
        wxRibbonButtonBarLayout* layout = m_layouts.Item(i);
        wxSize size = layout->overall_size;
        switch(direction)
        {
        case wxHORIZONTAL:
            if(size.x < result.x && size.y <= result.y)
            {
                result.x = size.x;
                break;
            }
            else
                continue;
        case wxVERTICAL:
            if(size.x <= result.x && size.y < result.y)
            {
                result.y = size.y;
                break;
            }
            else
                continue;
        case wxBOTH:
            if(size.x < result.x && size.y < result.y)
            {
                result = size;
                break;
            }
            else
                continue;
        }
        break;
    }
    return result;
}

wxSize wxRibbonButtonBar::DoGetNextLargerSize(wxOrientation direction,
                                            wxSize result) const
{
    size_t nlayouts = m_layouts.GetCount();
    size_t i = nlayouts;
    while(i > 0)
    {
        --i;
        wxRibbonButtonBarLayout* layout = m_layouts.Item(i);
        wxSize size = layout->overall_size;
        switch(direction)
        {
        case wxHORIZONTAL:
            if(size.x > result.x && size.y <= result.y)
            {
                result.x = size.x;
                break;
            }
            else
                continue;
        case wxVERTICAL:
            if(size.x <= result.x && size.y > result.y)
            {
                result.y = size.y;
                break;
            }
            else
                continue;
        case wxBOTH:
            if(size.x > result.x && size.y > result.y)
            {
                result = size;
                break;
            }
            else
                continue;
        }
        break;
    }
    return result;
}

void wxRibbonButtonBar::UpdateWindowUI(long flags)
{
    wxWindowBase::UpdateWindowUI(flags);

    // don't waste time updating state of tools in a hidden toolbar
    if ( !IsShown() )
        return;

    size_t btn_count = m_buttons.size();
    bool rerealize = false;
    for ( size_t btn_i = 0; btn_i < btn_count; ++btn_i )
    {
        wxRibbonButtonBarButtonBase& btn = *m_buttons.Item(btn_i);
        int id = btn.id;

        wxUpdateUIEvent event(id);
        event.SetEventObject(this);

        if ( ProcessWindowEvent(event) )
        {
            if ( event.GetSetEnabled() )
                EnableButton(id, event.GetEnabled());
            if ( event.GetSetChecked() )
                ToggleButton(id, event.GetChecked());
            if ( event.GetSetText() )
            {
                btn.label = event.GetText();
                rerealize = true;
            }
        }
    }

    if ( rerealize )
        Realize();
}

void wxRibbonButtonBar::OnEraseBackground(wxEraseEvent& WXUNUSED(evt))
{
    // All painting done in main paint handler to minimise flicker
}

void wxRibbonButtonBar::OnPaint(wxPaintEvent& WXUNUSED(evt))
{
    wxAutoBufferedPaintDC dc(this);
    m_art->DrawButtonBarBackground(dc, this, GetSize());

    wxRibbonButtonBarLayout* layout = m_layouts.Item(m_current_layout);

    size_t btn_count = layout->buttons.Count();
    size_t btn_i;
    for(btn_i = 0; btn_i < btn_count; ++btn_i)
    {
        wxRibbonButtonBarButtonInstance& button = layout->buttons.Item(btn_i);
        wxRibbonButtonBarButtonBase* base = button.base;

        wxBitmap* bitmap = &base->bitmap_large;
        wxBitmap* bitmap_small = &base->bitmap_small;
        if(base->state & wxRIBBON_BUTTONBAR_BUTTON_DISABLED)
        {
            bitmap = &base->bitmap_large_disabled;
            bitmap_small = &base->bitmap_small_disabled;
        }
        wxRect rect(button.position + m_layout_offset, base->sizes[button.size].size);

        m_art->DrawButtonBarButton(dc, this, rect, base->kind,
            base->state | button.size, base->label, *bitmap, *bitmap_small);
    }
}

void wxRibbonButtonBar::OnSize(wxSizeEvent& evt)
{
    wxSize new_size = evt.GetSize();
    size_t layout_count = m_layouts.GetCount();
    size_t layout_i;
    m_current_layout = layout_count - 1;
    for(layout_i = 0; layout_i < layout_count; ++layout_i)
    {
        wxSize layout_size = m_layouts.Item(layout_i)->overall_size;
        if(layout_size.x <= new_size.x && layout_size.y <= new_size.y)
        {
            m_layout_offset.x = (new_size.x - layout_size.x) / 2;
            m_layout_offset.y = (new_size.y - layout_size.y) / 2;
            m_current_layout = layout_i;
            break;
        }
    }
    m_hovered_button = m_layouts.Item(m_current_layout)->FindSimilarInstance(m_hovered_button);
    Refresh();
}

void wxRibbonButtonBar::CommonInit(long WXUNUSED(style))
{
    m_bitmap_size_large = wxSize(32, 32);
    m_bitmap_size_small = wxSize(16, 16);

    wxRibbonButtonBarLayout* placeholder_layout = new wxRibbonButtonBarLayout;
    placeholder_layout->overall_size = wxSize(20, 20);
    m_layouts.Add(placeholder_layout);
    m_current_layout = 0;
    m_layout_offset = wxPoint(0, 0);
    m_hovered_button = NULL;
    m_active_button = NULL;
    m_lock_active_state = false;
    m_show_tooltips_for_disabled = false;

    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
}

void wxRibbonButtonBar::SetShowToolTipsForDisabled(bool show)
{
    m_show_tooltips_for_disabled = show;
}

bool wxRibbonButtonBar::GetShowToolTipsForDisabled() const
{
    return m_show_tooltips_for_disabled;
}

wxSize wxRibbonButtonBar::GetMinSize() const
{
    return m_layouts.Last()->overall_size;
}

wxSize wxRibbonButtonBar::DoGetBestSize() const
{
    return m_layouts.Item(0)->overall_size;
}

void wxRibbonButtonBar::MakeLayouts()
{
    if(m_layouts_valid || m_art == NULL)
    {
        return;
    }
    {
        // Clear existing layouts
        if(m_hovered_button)
        {
            m_hovered_button->base->state &= ~wxRIBBON_BUTTONBAR_BUTTON_HOVER_MASK;
            m_hovered_button = NULL;
        }
        if(m_active_button)
        {
            m_active_button->base->state &= ~wxRIBBON_BUTTONBAR_BUTTON_ACTIVE_MASK;
            m_active_button = NULL;
        }
        size_t count = m_layouts.GetCount();
        size_t i;
        for(i = 0; i < count; ++i)
        {
            wxRibbonButtonBarLayout* layout = m_layouts.Item(i);
            delete layout;
        }
        m_layouts.Clear();
    }
    size_t btn_count = m_buttons.Count();
    size_t btn_i;
    {
        // Best layout : all buttons large, stacking horizontally
        wxRibbonButtonBarLayout* layout = new wxRibbonButtonBarLayout;
        wxPoint cursor(0, 0);
        layout->overall_size.SetHeight(0);
        for(btn_i = 0; btn_i < btn_count; ++btn_i)
        {
            wxRibbonButtonBarButtonBase* button = m_buttons.Item(btn_i);
            wxRibbonButtonBarButtonInstance instance = button->NewInstance();
            instance.position = cursor;
            instance.size = button->GetLargestSize();
            wxSize& size = button->sizes[instance.size].size;
            cursor.x += size.GetWidth();
            layout->overall_size.SetHeight(wxMax(layout->overall_size.GetHeight(),
                size.GetHeight()));
            layout->buttons.Add(instance);
        }
        layout->overall_size.SetWidth(cursor.x);
        m_layouts.Add(layout);
    }
    if(btn_count >= 2)
    {
        // Collapse the rightmost buttons and stack them vertically
        size_t iLast = btn_count - 1;
        while(TryCollapseLayout(m_layouts.Last(), iLast, &iLast) && iLast > 0)
        {
            --iLast;
        }
    }
}

bool wxRibbonButtonBar::TryCollapseLayout(wxRibbonButtonBarLayout* original,
                                          size_t first_btn, size_t* last_button)
{
    size_t btn_count = m_buttons.Count();
    size_t btn_i;
    int used_height = 0;
    int used_width = 0;
    int available_width = 0;
    int available_height = 0;

    for(btn_i = first_btn + 1; btn_i > 0; /* decrement is inside loop */)
    {
        --btn_i;
        wxRibbonButtonBarButtonBase* button = m_buttons.Item(btn_i);
        wxRibbonButtonBarButtonState large_size_class = button->GetLargestSize();
        wxSize large_size = button->sizes[large_size_class].size;
        int t_available_height = wxMax(available_height,
            large_size.GetHeight());
        int t_available_width = available_width + large_size.GetWidth();
        wxRibbonButtonBarButtonState small_size_class = large_size_class;
        if(!button->GetSmallerSize(&small_size_class))
        {
            return false;
        }
        wxSize small_size = button->sizes[small_size_class].size;
        int t_used_height = used_height + small_size.GetHeight();
        int t_used_width = wxMax(used_width, small_size.GetWidth());

        if(t_used_height > t_available_height)
        {
            ++btn_i;
            break;
        }
        else
        {
            used_height = t_used_height;
            used_width = t_used_width;
            available_width = t_available_width;
            available_height = t_available_height;
        }
    }

    if(btn_i >= first_btn || used_width >= available_width)
    {
        return false;
    }
    if(last_button != NULL)
    {
        *last_button = btn_i;
    }

    wxRibbonButtonBarLayout* layout = new wxRibbonButtonBarLayout;
    WX_APPEND_ARRAY(layout->buttons, original->buttons);
    wxPoint cursor(layout->buttons.Item(btn_i).position);
    bool preserve_height = false;
    if(btn_i == 0)
    {
        // If height isn't preserved (i.e. it is reduced), then the minimum
        // size for the button bar will decrease, preventing the original
        // layout from being used (in some cases).
        // It may be a good idea to always preserve the height, but for now
        // it is only done when the first button is involved in a collapse.
        preserve_height = true;
    }

    for(; btn_i <= first_btn; ++btn_i)
    {
        wxRibbonButtonBarButtonInstance& instance = layout->buttons.Item(btn_i);
        instance.base->GetSmallerSize(&instance.size);
        instance.position = cursor;
        cursor.y += instance.base->sizes[instance.size].size.GetHeight();
    }

    int x_adjust = available_width - used_width;

    for(; btn_i < btn_count; ++btn_i)
    {
        wxRibbonButtonBarButtonInstance& instance = layout->buttons.Item(btn_i);
        instance.position.x -= x_adjust;
    }

    layout->CalculateOverallSize();

    // Sanity check
    if(layout->overall_size.GetWidth() >= original->overall_size.GetWidth() ||
        layout->overall_size.GetHeight() > original->overall_size.GetHeight())
    {
        delete layout;
        wxFAIL_MSG("Layout collapse resulted in increased size");
        return false;
    }

    if(preserve_height)
    {
        layout->overall_size.SetHeight(original->overall_size.GetHeight());
    }

    m_layouts.Add(layout);
    return true;
}

void wxRibbonButtonBar::OnMouseMove(wxMouseEvent& evt)
{
    wxPoint cursor(evt.GetPosition());
    wxRibbonButtonBarButtonInstance* new_hovered = NULL;
    wxRibbonButtonBarButtonInstance* tooltipButton = NULL;
    long new_hovered_state = 0;

    wxRibbonButtonBarLayout* layout = m_layouts.Item(m_current_layout);
    size_t btn_count = layout->buttons.Count();
    size_t btn_i;
    for(btn_i = 0; btn_i < btn_count; ++btn_i)
    {
        wxRibbonButtonBarButtonInstance& instance = layout->buttons.Item(btn_i);
        wxRibbonButtonBarButtonSizeInfo& size = instance.base->sizes[instance.size];
        wxRect btn_rect;
        btn_rect.SetTopLeft(m_layout_offset + instance.position);
        btn_rect.SetSize(size.size);
        if(btn_rect.Contains(cursor))
        {
            if((instance.base->state & wxRIBBON_BUTTONBAR_BUTTON_DISABLED) == 0)
            {
                tooltipButton = &instance;
                new_hovered = &instance;
                new_hovered_state = instance.base->state;
                new_hovered_state &= ~wxRIBBON_BUTTONBAR_BUTTON_HOVER_MASK;
                wxPoint offset(cursor);
                offset -= btn_rect.GetTopLeft();
                if(size.normal_region.Contains(offset))
                {
                    new_hovered_state |= wxRIBBON_BUTTONBAR_BUTTON_NORMAL_HOVERED;
                }
                if(size.dropdown_region.Contains(offset))
                {
                    new_hovered_state |= wxRIBBON_BUTTONBAR_BUTTON_DROPDOWN_HOVERED;
                }
                break;
            }
            else if (m_show_tooltips_for_disabled)
            {
                tooltipButton = &instance;
            }
        }
    }

#if wxUSE_TOOLTIPS
    if(tooltipButton == NULL && GetToolTip())
    {
        UnsetToolTip();
    }
    if(tooltipButton)
    {
        SetToolTip(tooltipButton->base->help_string);
    }
#endif

    if(new_hovered != m_hovered_button || (m_hovered_button != NULL &&
        new_hovered_state != m_hovered_button->base->state))
    {
        if(m_hovered_button != NULL)
        {
            m_hovered_button->base->state &= ~wxRIBBON_BUTTONBAR_BUTTON_HOVER_MASK;
        }
        m_hovered_button = new_hovered;
        if(m_hovered_button != NULL)
        {
            m_hovered_button->base->state = new_hovered_state;
        }
        Refresh(false);
    }

    if(m_active_button && !m_lock_active_state)
    {
        long new_active_state = m_active_button->base->state;
        new_active_state &= ~wxRIBBON_BUTTONBAR_BUTTON_ACTIVE_MASK;
        wxRibbonButtonBarButtonSizeInfo& size =
            m_active_button->base->sizes[m_active_button->size];
        wxRect btn_rect;
        btn_rect.SetTopLeft(m_layout_offset + m_active_button->position);
        btn_rect.SetSize(size.size);
        if(btn_rect.Contains(cursor))
        {
            wxPoint offset(cursor);
            offset -= btn_rect.GetTopLeft();
            if(size.normal_region.Contains(offset))
            {
                new_active_state |= wxRIBBON_BUTTONBAR_BUTTON_NORMAL_ACTIVE;
            }
            if(size.dropdown_region.Contains(offset))
            {
                new_active_state |= wxRIBBON_BUTTONBAR_BUTTON_DROPDOWN_ACTIVE;
            }
        }
        if(new_active_state != m_active_button->base->state)
        {
            m_active_button->base->state = new_active_state;
            Refresh(false);
        }
    }
}

void wxRibbonButtonBar::OnMouseDown(wxMouseEvent& evt)
{
    wxPoint cursor(evt.GetPosition());
    m_active_button = NULL;

    wxRibbonButtonBarLayout* layout = m_layouts.Item(m_current_layout);
    size_t btn_count = layout->buttons.Count();
    size_t btn_i;
    for(btn_i = 0; btn_i < btn_count; ++btn_i)
    {
        wxRibbonButtonBarButtonInstance& instance = layout->buttons.Item(btn_i);
        wxRibbonButtonBarButtonSizeInfo& size = instance.base->sizes[instance.size];
        wxRect btn_rect;
        btn_rect.SetTopLeft(m_layout_offset + instance.position);
        btn_rect.SetSize(size.size);
        if(btn_rect.Contains(cursor))
        {
            if((instance.base->state & wxRIBBON_BUTTONBAR_BUTTON_DISABLED) == 0)
            {
                m_active_button = &instance;
                cursor -= btn_rect.GetTopLeft();
                long state = 0;
                if(size.normal_region.Contains(cursor))
                    state = wxRIBBON_BUTTONBAR_BUTTON_NORMAL_ACTIVE;
                else if(size.dropdown_region.Contains(cursor))
                    state = wxRIBBON_BUTTONBAR_BUTTON_DROPDOWN_ACTIVE;
                instance.base->state |= state;
                Refresh(false);
                break;
            }
        }
    }
}

void wxRibbonButtonBar::OnMouseUp(wxMouseEvent& evt)
{
    wxPoint cursor(evt.GetPosition());

    if(m_active_button)
    {
        wxRibbonButtonBarButtonSizeInfo& size =
            m_active_button->base->sizes[m_active_button->size];
        wxRect btn_rect;
        btn_rect.SetTopLeft(m_layout_offset + m_active_button->position);
        btn_rect.SetSize(size.size);
        if(btn_rect.Contains(cursor))
        {
            int id = m_active_button->base->id;
            cursor -= btn_rect.GetTopLeft();
            wxEventType event_type;
            do
            {
                if(size.normal_region.Contains(cursor))
                    event_type = wxEVT_RIBBONBUTTONBAR_CLICKED;
                else if(size.dropdown_region.Contains(cursor))
                    event_type = wxEVT_RIBBONBUTTONBAR_DROPDOWN_CLICKED;
                else
                    break;
                wxRibbonButtonBarEvent notification(event_type, id);
                if(m_active_button->base->kind == wxRIBBON_BUTTON_TOGGLE)
                {
                    m_active_button->base->state ^=
                        wxRIBBON_BUTTONBAR_BUTTON_TOGGLED;
                    notification.SetInt(m_active_button->base->state &
                        wxRIBBON_BUTTONBAR_BUTTON_TOGGLED);
                }
                notification.SetEventObject(this);
                notification.SetBar(this);
                notification.SetButton(m_active_button->base);
                m_lock_active_state = true;
                ProcessWindowEvent(notification);
                m_lock_active_state = false;

                wxStaticCast(m_parent, wxRibbonPanel)->HideIfExpanded();
            } while(false);
            if(m_active_button) // may have been NULLed by event handler
            {
                m_active_button->base->state &= ~wxRIBBON_BUTTONBAR_BUTTON_ACTIVE_MASK;
                m_active_button = NULL;
            }
            Refresh(false);
        }
    }
}

void wxRibbonButtonBar::OnMouseEnter(wxMouseEvent& evt)
{
    if(m_active_button && !evt.LeftIsDown())
    {
        m_active_button = NULL;
    }
}

void wxRibbonButtonBar::OnMouseLeave(wxMouseEvent& WXUNUSED(evt))
{
    bool repaint = false;
    if(m_hovered_button != NULL)
    {
        m_hovered_button->base->state &= ~wxRIBBON_BUTTONBAR_BUTTON_HOVER_MASK;
        m_hovered_button = NULL;
        repaint = true;
    }
    if(m_active_button != NULL && !m_lock_active_state)
    {
        m_active_button->base->state &= ~wxRIBBON_BUTTONBAR_BUTTON_ACTIVE_MASK;
        repaint = true;
    }
    if(repaint)
        Refresh(false);
}

wxRibbonButtonBarButtonBase *wxRibbonButtonBar::GetActiveItem() const
{
    return m_active_button == NULL ? NULL : m_active_button->base;
}


wxRibbonButtonBarButtonBase *wxRibbonButtonBar::GetHoveredItem() const
{
    return m_hovered_button == NULL ? NULL : m_hovered_button->base;
}


wxRibbonButtonBarButtonBase *wxRibbonButtonBar::GetItem(size_t n) const
{
    wxCHECK_MSG(n < m_buttons.GetCount(), NULL, "wxRibbonButtonBar item's index is out of bound");
    return m_buttons.Item(n);
}

wxRibbonButtonBarButtonBase *wxRibbonButtonBar::GetItemById(int button_id) const
{
    size_t count = m_buttons.GetCount();
    for ( size_t i = 0; i < count; ++i )
    {
        wxRibbonButtonBarButtonBase* button = m_buttons.Item(i);
        if ( button->id == button_id )
            return button;
    }

    return NULL;

}

int wxRibbonButtonBar::GetItemId(wxRibbonButtonBarButtonBase *item) const
{
    wxCHECK_MSG(item != NULL, wxNOT_FOUND, "wxRibbonButtonBar item should not be NULL");
    return item->id;
}


bool wxRibbonButtonBarEvent::PopupMenu(wxMenu* menu)
{
    wxPoint pos = wxDefaultPosition;
    if(m_bar->m_active_button)
    {
        wxRibbonButtonBarButtonSizeInfo& size =
            m_bar->m_active_button->base->sizes[m_bar->m_active_button->size];
        wxRect btn_rect;
        btn_rect.SetTopLeft(m_bar->m_layout_offset +
            m_bar->m_active_button->position);
        btn_rect.SetSize(size.size);
        pos = btn_rect.GetBottomLeft();
        pos.y++;
    }
    return m_bar->PopupMenu(menu, pos);
}

#endif // wxUSE_RIBBON
