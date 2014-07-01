///////////////////////////////////////////////////////////////////////////////
// Name:        wx/ribbon/bar.h
// Purpose:     Top-level component of the ribbon-bar-style interface
// Author:      Peter Cawley
// Modified by:
// Created:     2009-05-23
// Copyright:   (C) Peter Cawley
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RIBBON_BAR_H_
#define _WX_RIBBON_BAR_H_

#include "wx/defs.h"

#if wxUSE_RIBBON

#include "wx/ribbon/control.h"
#include "wx/ribbon/page.h"

enum wxRibbonBarOption
{
    wxRIBBON_BAR_SHOW_PAGE_LABELS    = 1 << 0,
    wxRIBBON_BAR_SHOW_PAGE_ICONS    = 1 << 1,
    wxRIBBON_BAR_FLOW_HORIZONTAL    = 0,
    wxRIBBON_BAR_FLOW_VERTICAL        = 1 << 2,
    wxRIBBON_BAR_SHOW_PANEL_EXT_BUTTONS = 1 << 3,
    wxRIBBON_BAR_SHOW_PANEL_MINIMISE_BUTTONS = 1 << 4,
    wxRIBBON_BAR_ALWAYS_SHOW_TABS = 1 << 5,
    wxRIBBON_BAR_SHOW_TOGGLE_BUTTON = 1 << 6,
    wxRIBBON_BAR_SHOW_HELP_BUTTON = 1 << 7,

    wxRIBBON_BAR_DEFAULT_STYLE =  wxRIBBON_BAR_FLOW_HORIZONTAL
                                | wxRIBBON_BAR_SHOW_PAGE_LABELS
                                | wxRIBBON_BAR_SHOW_PANEL_EXT_BUTTONS
                                | wxRIBBON_BAR_SHOW_TOGGLE_BUTTON
                                | wxRIBBON_BAR_SHOW_HELP_BUTTON,

    wxRIBBON_BAR_FOLDBAR_STYLE =  wxRIBBON_BAR_FLOW_VERTICAL
                                | wxRIBBON_BAR_SHOW_PAGE_ICONS
                                | wxRIBBON_BAR_SHOW_PANEL_EXT_BUTTONS
                                | wxRIBBON_BAR_SHOW_PANEL_MINIMISE_BUTTONS
};

enum wxRibbonDisplayMode
{
    wxRIBBON_BAR_PINNED,
    wxRIBBON_BAR_MINIMIZED,
    wxRIBBON_BAR_EXPANDED
};

class WXDLLIMPEXP_RIBBON wxRibbonBarEvent : public wxNotifyEvent
{
public:
    wxRibbonBarEvent(wxEventType command_type = wxEVT_NULL,
                       int win_id = 0,
                       wxRibbonPage* page = NULL)
        : wxNotifyEvent(command_type, win_id)
        , m_page(page)
    {
    }
#ifndef SWIG
    wxRibbonBarEvent(const wxRibbonBarEvent& c) : wxNotifyEvent(c)
    {
        m_page = c.m_page;
    }
#endif
    wxEvent *Clone() const { return new wxRibbonBarEvent(*this); }

    wxRibbonPage* GetPage() {return m_page;}
    void SetPage(wxRibbonPage* page) {m_page = page;}

protected:
    wxRibbonPage* m_page;

#ifndef SWIG
private:
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxRibbonBarEvent)
#endif
};

class WXDLLIMPEXP_RIBBON wxRibbonPageTabInfo
{
public:
    wxRect rect;
    wxRibbonPage *page;
    int ideal_width;
    int small_begin_need_separator_width;
    int small_must_have_separator_width;
    int minimum_width;
    bool active;
    bool hovered;
    bool highlight;
    bool shown;
};

#ifndef SWIG
WX_DECLARE_USER_EXPORTED_OBJARRAY(wxRibbonPageTabInfo, wxRibbonPageTabInfoArray, WXDLLIMPEXP_RIBBON);
#endif

class WXDLLIMPEXP_RIBBON wxRibbonBar : public wxRibbonControl
{
public:
    wxRibbonBar();

    wxRibbonBar(wxWindow* parent,
                  wxWindowID id = wxID_ANY,
                  const wxPoint& pos = wxDefaultPosition,
                  const wxSize& size = wxDefaultSize,
                  long style = wxRIBBON_BAR_DEFAULT_STYLE);

    virtual ~wxRibbonBar();

    bool Create(wxWindow* parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxRIBBON_BAR_DEFAULT_STYLE);

    void SetTabCtrlMargins(int left, int right);

    void SetArtProvider(wxRibbonArtProvider* art);

    bool SetActivePage(size_t page);
    bool SetActivePage(wxRibbonPage* page);
    int GetActivePage() const;
    wxRibbonPage* GetPage(int n);
    size_t GetPageCount() const;
    bool DismissExpandedPanel();
    int GetPageNumber(wxRibbonPage* page) const;

    void DeletePage(size_t n);
    void ClearPages();

    bool IsPageShown(size_t page) const;
    void ShowPage(size_t page, bool show = true);
    void HidePage(size_t page) { ShowPage(page, false); }

    bool IsPageHighlighted(size_t page) const;
    void AddPageHighlight(size_t page, bool highlight = true);
    void RemovePageHighlight(size_t page) { AddPageHighlight(page, false); }

    void ShowPanels(bool show = true);
    void HidePanels() { ShowPanels(false); }
    bool ArePanelsShown() const { return m_arePanelsShown; }

    virtual bool HasMultiplePages() const { return true; }

    void SetWindowStyleFlag(long style);
    long GetWindowStyleFlag() const;
    virtual bool Realize();

    // Implementation only.
    bool IsToggleButtonHovered() const { return m_toggle_button_hovered; }
    bool IsHelpButtonHovered() const { return m_help_button_hovered; }

    void HideIfExpanded();

protected:
    friend class wxRibbonPage;

    virtual wxSize DoGetBestSize() const;
    wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }
    wxRibbonPageTabInfo* HitTestTabs(wxPoint position, int* index = NULL);
    void HitTestRibbonButton(const wxRect& rect, const wxPoint& position, bool &hover_flag);

    void CommonInit(long style);
    void AddPage(wxRibbonPage *page);
    void RecalculateTabSizes();
    void RecalculateMinSize();
    void ScrollTabBar(int npixels);
    void RefreshTabBar();
    void RepositionPage(wxRibbonPage *page);

    void OnPaint(wxPaintEvent& evt);
    void OnEraseBackground(wxEraseEvent& evt);
    void DoEraseBackground(wxDC& dc);
    void OnSize(wxSizeEvent& evt);
    void OnMouseLeftDown(wxMouseEvent& evt);
    void OnMouseLeftUp(wxMouseEvent& evt);
    void OnMouseMiddleDown(wxMouseEvent& evt);
    void OnMouseMiddleUp(wxMouseEvent& evt);
    void OnMouseRightDown(wxMouseEvent& evt);
    void OnMouseRightUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnMouseLeave(wxMouseEvent& evt);
    void OnMouseDoubleClick(wxMouseEvent& evt);
    void DoMouseButtonCommon(wxMouseEvent& evt, wxEventType tab_event_type);
    void OnKillFocus(wxFocusEvent& evt);

    wxRibbonPageTabInfoArray m_pages;
    wxRect m_tab_scroll_left_button_rect;
    wxRect m_tab_scroll_right_button_rect;
    wxRect m_toggle_button_rect;
    wxRect m_help_button_rect;
    long m_flags;
    int m_tabs_total_width_ideal;
    int m_tabs_total_width_minimum;
    int m_tab_margin_left;
    int m_tab_margin_right;
    int m_tab_height;
    int m_tab_scroll_amount;
    int m_current_page;
    int m_current_hovered_page;
    int m_tab_scroll_left_button_state;
    int m_tab_scroll_right_button_state;
    bool m_tab_scroll_buttons_shown;
    bool m_arePanelsShown;
    bool m_bar_hovered;
    bool m_toggle_button_hovered;
    bool m_help_button_hovered;

    wxRibbonDisplayMode m_ribbon_state;

#ifndef SWIG
    DECLARE_CLASS(wxRibbonBar)
    DECLARE_EVENT_TABLE()
#endif
};

#ifndef SWIG

wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONBAR_PAGE_CHANGED, wxRibbonBarEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONBAR_PAGE_CHANGING, wxRibbonBarEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONBAR_TAB_MIDDLE_DOWN, wxRibbonBarEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONBAR_TAB_MIDDLE_UP, wxRibbonBarEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONBAR_TAB_RIGHT_DOWN, wxRibbonBarEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONBAR_TAB_RIGHT_UP, wxRibbonBarEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONBAR_TAB_LEFT_DCLICK, wxRibbonBarEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONBAR_TOGGLED, wxRibbonBarEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONBAR_HELP_CLICK, wxRibbonBarEvent);

typedef void (wxEvtHandler::*wxRibbonBarEventFunction)(wxRibbonBarEvent&);

#define wxRibbonBarEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxRibbonBarEventFunction, func)

#define EVT_RIBBONBAR_PAGE_CHANGED(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONBAR_PAGE_CHANGED, winid, wxRibbonBarEventHandler(fn))
#define EVT_RIBBONBAR_PAGE_CHANGING(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONBAR_PAGE_CHANGING, winid, wxRibbonBarEventHandler(fn))
#define EVT_RIBBONBAR_TAB_MIDDLE_DOWN(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONBAR_TAB_MIDDLE_DOWN, winid, wxRibbonBarEventHandler(fn))
#define EVT_RIBBONBAR_TAB_MIDDLE_UP(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONBAR_TAB_MIDDLE_UP, winid, wxRibbonBarEventHandler(fn))
#define EVT_RIBBONBAR_TAB_RIGHT_DOWN(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONBAR_TAB_RIGHT_DOWN, winid, wxRibbonBarEventHandler(fn))
#define EVT_RIBBONBAR_TAB_RIGHT_UP(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONBAR_TAB_RIGHT_UP, winid, wxRibbonBarEventHandler(fn))
#define EVT_RIBBONBAR_TAB_LEFT_DCLICK(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONBAR_TAB_LEFT_DCLICK, winid, wxRibbonBarEventHandler(fn))
#define EVT_RIBBONBAR_TOGGLED(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONBAR_TOGGLED, winid, wxRibbonBarEventHandler(fn))
#define EVT_RIBBONBAR_HELP_CLICK(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONBAR_HELP_CLICK, winid, wxRibbonBarEventHandler(fn))
#else

// wxpython/swig event work
%constant wxEventType wxEVT_RIBBONBAR_PAGE_CHANGED;
%constant wxEventType wxEVT_RIBBONBAR_PAGE_CHANGING;
%constant wxEventType wxEVT_RIBBONBAR_TAB_MIDDLE_DOWN;
%constant wxEventType wxEVT_RIBBONBAR_TAB_MIDDLE_UP;
%constant wxEventType wxEVT_RIBBONBAR_TAB_RIGHT_DOWN;
%constant wxEventType wxEVT_RIBBONBAR_TAB_RIGHT_UP;
%constant wxEventType wxEVT_RIBBONBAR_TAB_LEFT_DCLICK;
%constant wxEventType wxEVT_RIBBONBAR_TOGGLED;
%constant wxEventType wxEVT_RIBBONBAR_HELP_CLICK;

%pythoncode {
    EVT_RIBBONBAR_PAGE_CHANGED = wx.PyEventBinder( wxEVT_RIBBONBAR_PAGE_CHANGED, 1 )
    EVT_RIBBONBAR_PAGE_CHANGING = wx.PyEventBinder( wxEVT_RIBBONBAR_PAGE_CHANGING, 1 )
    EVT_RIBBONBAR_TAB_MIDDLE_DOWN = wx.PyEventBinder( wxEVT_RIBBONBAR_TAB_MIDDLE_DOWN, 1 )
    EVT_RIBBONBAR_TAB_MIDDLE_UP = wx.PyEventBinder( wxEVT_RIBBONBAR_TAB_MIDDLE_UP, 1 )
    EVT_RIBBONBAR_TAB_RIGHT_DOWN = wx.PyEventBinder( wxEVT_RIBBONBAR_TAB_RIGHT_DOWN, 1 )
    EVT_RIBBONBAR_TAB_RIGHT_UP = wx.PyEventBinder( wxEVT_RIBBONBAR_TAB_RIGHT_UP, 1 )
    EVT_RIBBONBAR_TAB_LEFT_DCLICK = wx.PyEventBinder( wxEVT_RIBBONBAR_TAB_LEFT_DCLICK, 1 )
    EVT_RIBBONBAR_TOGGLED = wx.PyEventBinder( wxEVT_RIBBONBAR_TOGGLED, 1 )
    EVT_RIBBONBAR_HELP_CLICK = wx.PyEventBinder( wxEVT_RIBBONBAR_HELP_CLICK, 1 )
}
#endif

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_RIBBONBAR_PAGE_CHANGED      wxEVT_RIBBONBAR_PAGE_CHANGED
#define wxEVT_COMMAND_RIBBONBAR_PAGE_CHANGING     wxEVT_RIBBONBAR_PAGE_CHANGING
#define wxEVT_COMMAND_RIBBONBAR_TAB_MIDDLE_DOWN   wxEVT_RIBBONBAR_TAB_MIDDLE_DOWN
#define wxEVT_COMMAND_RIBBONBAR_TAB_MIDDLE_UP     wxEVT_RIBBONBAR_TAB_MIDDLE_UP
#define wxEVT_COMMAND_RIBBONBAR_TAB_RIGHT_DOWN    wxEVT_RIBBONBAR_TAB_RIGHT_DOWN
#define wxEVT_COMMAND_RIBBONBAR_TAB_RIGHT_UP      wxEVT_RIBBONBAR_TAB_RIGHT_UP
#define wxEVT_COMMAND_RIBBONBAR_TAB_LEFT_DCLICK   wxEVT_RIBBONBAR_TAB_LEFT_DCLICK
#define wxEVT_COMMAND_RIBBONBAR_TOGGLED           wxEVT_RIBBONBAR_TOGGLED
#define wxEVT_COMMAND_RIBBONBAR_HELP_CLICKED      wxEVT_RIBBONBAR_HELP_CLICK

#endif // wxUSE_RIBBON

#endif // _WX_RIBBON_BAR_H_

