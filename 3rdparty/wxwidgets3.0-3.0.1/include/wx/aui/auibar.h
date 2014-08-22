///////////////////////////////////////////////////////////////////////////////
// Name:        wx/aui/toolbar.h
// Purpose:     wxaui: wx advanced user interface - docking window manager
// Author:      Benjamin I. Williams
// Modified by:
// Created:     2008-08-04
// Copyright:   (C) Copyright 2005, Kirix Corporation, All Rights Reserved.
// Licence:     wxWindows Library Licence, Version 3.1
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_AUIBAR_H_
#define _WX_AUIBAR_H_

#include "wx/defs.h"

#if wxUSE_AUI

#include "wx/control.h"
#include "wx/sizer.h"
#include "wx/pen.h"

class WXDLLIMPEXP_FWD_CORE wxClientDC;
class WXDLLIMPEXP_FWD_AUI wxAuiPaneInfo;

enum wxAuiToolBarStyle
{
    wxAUI_TB_TEXT          = 1 << 0,
    wxAUI_TB_NO_TOOLTIPS   = 1 << 1,
    wxAUI_TB_NO_AUTORESIZE = 1 << 2,
    wxAUI_TB_GRIPPER       = 1 << 3,
    wxAUI_TB_OVERFLOW      = 1 << 4,
    // using this style forces the toolbar to be vertical and
    // be only dockable to the left or right sides of the window
    // whereas by default it can be horizontal or vertical and
    // be docked anywhere
    wxAUI_TB_VERTICAL      = 1 << 5,
    wxAUI_TB_HORZ_LAYOUT   = 1 << 6,
    // analogous to wxAUI_TB_VERTICAL, but forces the toolbar
    // to be horizontal
    wxAUI_TB_HORIZONTAL    = 1 << 7,
    wxAUI_TB_PLAIN_BACKGROUND = 1 << 8,
    wxAUI_TB_HORZ_TEXT     = (wxAUI_TB_HORZ_LAYOUT | wxAUI_TB_TEXT),
    wxAUI_ORIENTATION_MASK = (wxAUI_TB_VERTICAL | wxAUI_TB_HORIZONTAL),
    wxAUI_TB_DEFAULT_STYLE = 0
};

enum wxAuiToolBarArtSetting
{
    wxAUI_TBART_SEPARATOR_SIZE = 0,
    wxAUI_TBART_GRIPPER_SIZE = 1,
    wxAUI_TBART_OVERFLOW_SIZE = 2
};

enum wxAuiToolBarToolTextOrientation
{
    wxAUI_TBTOOL_TEXT_LEFT = 0,     // unused/unimplemented
    wxAUI_TBTOOL_TEXT_RIGHT = 1,
    wxAUI_TBTOOL_TEXT_TOP = 2,      // unused/unimplemented
    wxAUI_TBTOOL_TEXT_BOTTOM = 3
};


// aui toolbar event class

class WXDLLIMPEXP_AUI wxAuiToolBarEvent : public wxNotifyEvent
{
public:
    wxAuiToolBarEvent(wxEventType commandType = wxEVT_NULL,
                      int winId = 0)
          : wxNotifyEvent(commandType, winId)
    {
        m_isDropdownClicked = false;
        m_clickPt = wxPoint(-1, -1);
        m_rect = wxRect(-1,-1, 0, 0);
        m_toolId = -1;
    }
#ifndef SWIG
    wxAuiToolBarEvent(const wxAuiToolBarEvent& c) : wxNotifyEvent(c)
    {
        m_isDropdownClicked = c.m_isDropdownClicked;
        m_clickPt = c.m_clickPt;
        m_rect = c.m_rect;
        m_toolId = c.m_toolId;
    }
#endif
    wxEvent *Clone() const { return new wxAuiToolBarEvent(*this); }

    bool IsDropDownClicked() const  { return m_isDropdownClicked; }
    void SetDropDownClicked(bool c) { m_isDropdownClicked = c;    }

    wxPoint GetClickPoint() const        { return m_clickPt; }
    void SetClickPoint(const wxPoint& p) { m_clickPt = p;    }

    wxRect GetItemRect() const        { return m_rect; }
    void SetItemRect(const wxRect& r) { m_rect = r;    }

    int GetToolId() const  { return m_toolId; }
    void SetToolId(int toolId) { m_toolId = toolId; }

private:

    bool m_isDropdownClicked;
    wxPoint m_clickPt;
    wxRect m_rect;
    int m_toolId;

private:
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxAuiToolBarEvent)
};


class WXDLLIMPEXP_AUI wxAuiToolBarItem
{
    friend class wxAuiToolBar;

public:

    wxAuiToolBarItem()
    {
        m_window = NULL;
        m_sizerItem = NULL;
        m_spacerPixels = 0;
        m_toolId = 0;
        m_kind = wxITEM_NORMAL;
        m_state = 0;  // normal, enabled
        m_proportion = 0;
        m_active = true;
        m_dropDown = true;
        m_sticky = true;
        m_userData = 0;
        m_alignment = wxALIGN_CENTER;
    }

    wxAuiToolBarItem(const wxAuiToolBarItem& c)
    {
        Assign(c);
    }

    wxAuiToolBarItem& operator=(const wxAuiToolBarItem& c)
    {
        Assign(c);
        return *this;
    }

    void Assign(const wxAuiToolBarItem& c)
    {
        m_window = c.m_window;
        m_label = c.m_label;
        m_bitmap = c.m_bitmap;
        m_disabledBitmap = c.m_disabledBitmap;
        m_hoverBitmap = c.m_hoverBitmap;
        m_shortHelp = c.m_shortHelp;
        m_longHelp = c.m_longHelp;
        m_sizerItem = c.m_sizerItem;
        m_minSize = c.m_minSize;
        m_spacerPixels = c.m_spacerPixels;
        m_toolId = c.m_toolId;
        m_kind = c.m_kind;
        m_state = c.m_state;
        m_proportion = c.m_proportion;
        m_active = c.m_active;
        m_dropDown = c.m_dropDown;
        m_sticky = c.m_sticky;
        m_userData = c.m_userData;
        m_alignment = c.m_alignment;
    }


    void SetWindow(wxWindow* w) { m_window = w; }
    wxWindow* GetWindow() { return m_window; }

    void SetId(int newId) { m_toolId = newId; }
    int GetId() const { return m_toolId; }

    void SetKind(int newKind) { m_kind = newKind; }
    int GetKind() const { return m_kind; }

    void SetState(int newState) { m_state = newState; }
    int GetState() const { return m_state; }

    void SetSizerItem(wxSizerItem* s) { m_sizerItem = s; }
    wxSizerItem* GetSizerItem() const { return m_sizerItem; }

    void SetLabel(const wxString& s) { m_label = s; }
    const wxString& GetLabel() const { return m_label; }

    void SetBitmap(const wxBitmap& bmp) { m_bitmap = bmp; }
    const wxBitmap& GetBitmap() const { return m_bitmap; }

    void SetDisabledBitmap(const wxBitmap& bmp) { m_disabledBitmap = bmp; }
    const wxBitmap& GetDisabledBitmap() const { return m_disabledBitmap; }

    void SetHoverBitmap(const wxBitmap& bmp) { m_hoverBitmap = bmp; }
    const wxBitmap& GetHoverBitmap() const { return m_hoverBitmap; }

    void SetShortHelp(const wxString& s) { m_shortHelp = s; }
    const wxString& GetShortHelp() const { return m_shortHelp; }

    void SetLongHelp(const wxString& s) { m_longHelp = s; }
    const wxString& GetLongHelp() const { return m_longHelp; }

    void SetMinSize(const wxSize& s) { m_minSize = s; }
    const wxSize& GetMinSize() const { return m_minSize; }

    void SetSpacerPixels(int s) { m_spacerPixels = s; }
    int GetSpacerPixels() const { return m_spacerPixels; }

    void SetProportion(int p) { m_proportion = p; }
    int GetProportion() const { return m_proportion; }

    void SetActive(bool b) { m_active = b; }
    bool IsActive() const { return m_active; }

    void SetHasDropDown(bool b)
    {
        wxCHECK_RET( !b || m_kind == wxITEM_NORMAL,
                     wxS("Only normal tools can have drop downs") );

        m_dropDown = b;
    }

    bool HasDropDown() const { return m_dropDown; }

    void SetSticky(bool b) { m_sticky = b; }
    bool IsSticky() const { return m_sticky; }

    void SetUserData(long l) { m_userData = l; }
    long GetUserData() const { return m_userData; }

    void SetAlignment(int l) { m_alignment = l; }
    int GetAlignment() const { return m_alignment; }

private:

    wxWindow* m_window;          // item's associated window
    wxString m_label;            // label displayed on the item
    wxBitmap m_bitmap;           // item's bitmap
    wxBitmap m_disabledBitmap;  // item's disabled bitmap
    wxBitmap m_hoverBitmap;     // item's hover bitmap
    wxString m_shortHelp;       // short help (for tooltip)
    wxString m_longHelp;        // long help (for status bar)
    wxSizerItem* m_sizerItem;   // sizer item
    wxSize m_minSize;           // item's minimum size
    int m_spacerPixels;         // size of a spacer
    int m_toolId;                // item's id
    int m_kind;                  // item's kind
    int m_state;                 // state
    int m_proportion;            // proportion
    bool m_active;               // true if the item is currently active
    bool m_dropDown;             // true if the item has a dropdown button
    bool m_sticky;               // overrides button states if true (always active)
    long m_userData;            // user-specified data
    int m_alignment;             // sizer alignment flag, defaults to wxCENTER, may be wxEXPAND or any other
};

#ifndef SWIG
WX_DECLARE_USER_EXPORTED_OBJARRAY(wxAuiToolBarItem, wxAuiToolBarItemArray, WXDLLIMPEXP_AUI);
#endif




// tab art class

class WXDLLIMPEXP_AUI wxAuiToolBarArt
{
public:

    wxAuiToolBarArt() { }
    virtual ~wxAuiToolBarArt() { }

    virtual wxAuiToolBarArt* Clone() = 0;
    virtual void SetFlags(unsigned int flags) = 0;
    virtual unsigned int GetFlags() = 0;
    virtual void SetFont(const wxFont& font) = 0;
    virtual wxFont GetFont() = 0;
    virtual void SetTextOrientation(int orientation) = 0;
    virtual int GetTextOrientation() = 0;

    virtual void DrawBackground(
                         wxDC& dc,
                         wxWindow* wnd,
                         const wxRect& rect) = 0;

    virtual void DrawPlainBackground(
                                  wxDC& dc,
                                  wxWindow* wnd,
                                  const wxRect& rect) = 0;

    virtual void DrawLabel(
                         wxDC& dc,
                         wxWindow* wnd,
                         const wxAuiToolBarItem& item,
                         const wxRect& rect) = 0;

    virtual void DrawButton(
                         wxDC& dc,
                         wxWindow* wnd,
                         const wxAuiToolBarItem& item,
                         const wxRect& rect) = 0;

    virtual void DrawDropDownButton(
                         wxDC& dc,
                         wxWindow* wnd,
                         const wxAuiToolBarItem& item,
                         const wxRect& rect) = 0;

    virtual void DrawControlLabel(
                         wxDC& dc,
                         wxWindow* wnd,
                         const wxAuiToolBarItem& item,
                         const wxRect& rect) = 0;

    virtual void DrawSeparator(
                         wxDC& dc,
                         wxWindow* wnd,
                         const wxRect& rect) = 0;

    virtual void DrawGripper(
                         wxDC& dc,
                         wxWindow* wnd,
                         const wxRect& rect) = 0;

    virtual void DrawOverflowButton(
                         wxDC& dc,
                         wxWindow* wnd,
                         const wxRect& rect,
                         int state) = 0;

    virtual wxSize GetLabelSize(
                         wxDC& dc,
                         wxWindow* wnd,
                         const wxAuiToolBarItem& item) = 0;

    virtual wxSize GetToolSize(
                         wxDC& dc,
                         wxWindow* wnd,
                         const wxAuiToolBarItem& item) = 0;

    virtual int GetElementSize(int elementId) = 0;
    virtual void SetElementSize(int elementId, int size) = 0;

    virtual int ShowDropDown(
                         wxWindow* wnd,
                         const wxAuiToolBarItemArray& items) = 0;
};



class WXDLLIMPEXP_AUI wxAuiDefaultToolBarArt : public wxAuiToolBarArt
{

public:

    wxAuiDefaultToolBarArt();
    virtual ~wxAuiDefaultToolBarArt();

    virtual wxAuiToolBarArt* Clone();
    virtual void SetFlags(unsigned int flags);
    virtual unsigned int GetFlags();
    virtual void SetFont(const wxFont& font);
    virtual wxFont GetFont();
    virtual void SetTextOrientation(int orientation);
    virtual int GetTextOrientation();

    virtual void DrawBackground(
                wxDC& dc,
                wxWindow* wnd,
                const wxRect& rect);

    virtual void DrawPlainBackground(wxDC& dc,
                                  wxWindow* wnd,
                                  const wxRect& rect);

    virtual void DrawLabel(
                wxDC& dc,
                wxWindow* wnd,
                const wxAuiToolBarItem& item,
                const wxRect& rect);

    virtual void DrawButton(
                wxDC& dc,
                wxWindow* wnd,
                const wxAuiToolBarItem& item,
                const wxRect& rect);

    virtual void DrawDropDownButton(
                wxDC& dc,
                wxWindow* wnd,
                const wxAuiToolBarItem& item,
                const wxRect& rect);

    virtual void DrawControlLabel(
                wxDC& dc,
                wxWindow* wnd,
                const wxAuiToolBarItem& item,
                const wxRect& rect);

    virtual void DrawSeparator(
                wxDC& dc,
                wxWindow* wnd,
                const wxRect& rect);

    virtual void DrawGripper(
                wxDC& dc,
                wxWindow* wnd,
                const wxRect& rect);

    virtual void DrawOverflowButton(
                wxDC& dc,
                wxWindow* wnd,
                const wxRect& rect,
                int state);

    virtual wxSize GetLabelSize(
                wxDC& dc,
                wxWindow* wnd,
                const wxAuiToolBarItem& item);

    virtual wxSize GetToolSize(
                wxDC& dc,
                wxWindow* wnd,
                const wxAuiToolBarItem& item);

    virtual int GetElementSize(int element);
    virtual void SetElementSize(int elementId, int size);

    virtual int ShowDropDown(wxWindow* wnd,
                             const wxAuiToolBarItemArray& items);

protected:

    wxBitmap m_buttonDropDownBmp;
    wxBitmap m_disabledButtonDropDownBmp;
    wxBitmap m_overflowBmp;
    wxBitmap m_disabledOverflowBmp;
    wxColour m_baseColour;
    wxColour m_highlightColour;
    wxFont m_font;
    unsigned int m_flags;
    int m_textOrientation;

    wxPen m_gripperPen1;
    wxPen m_gripperPen2;
    wxPen m_gripperPen3;

    int m_separatorSize;
    int m_gripperSize;
    int m_overflowSize;
};




class WXDLLIMPEXP_AUI wxAuiToolBar : public wxControl
{
public:
    wxAuiToolBar() { Init(); }

    wxAuiToolBar(wxWindow* parent,
                 wxWindowID id = wxID_ANY,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxAUI_TB_DEFAULT_STYLE)
    {
        Init();
        Create(parent, id, pos, size, style);
    }

    virtual ~wxAuiToolBar();

    bool Create(wxWindow* parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxAUI_TB_DEFAULT_STYLE);

    virtual void SetWindowStyleFlag(long style);

    void SetArtProvider(wxAuiToolBarArt* art);
    wxAuiToolBarArt* GetArtProvider() const;

    bool SetFont(const wxFont& font);


    wxAuiToolBarItem* AddTool(int toolId,
                 const wxString& label,
                 const wxBitmap& bitmap,
                 const wxString& shortHelpString = wxEmptyString,
                 wxItemKind kind = wxITEM_NORMAL);

    wxAuiToolBarItem* AddTool(int toolId,
                 const wxString& label,
                 const wxBitmap& bitmap,
                 const wxBitmap& disabledBitmap,
                 wxItemKind kind,
                 const wxString& shortHelpString,
                 const wxString& longHelpString,
                 wxObject* clientData);

    wxAuiToolBarItem* AddTool(int toolId,
                 const wxBitmap& bitmap,
                 const wxBitmap& disabledBitmap,
                 bool toggle = false,
                 wxObject* clientData = NULL,
                 const wxString& shortHelpString = wxEmptyString,
                 const wxString& longHelpString = wxEmptyString)
    {
        return AddTool(toolId,
                wxEmptyString,
                bitmap,
                disabledBitmap,
                toggle ? wxITEM_CHECK : wxITEM_NORMAL,
                shortHelpString,
                longHelpString,
                clientData);
    }

    wxAuiToolBarItem* AddLabel(int toolId,
                  const wxString& label = wxEmptyString,
                  const int width = -1);
    wxAuiToolBarItem* AddControl(wxControl* control,
                    const wxString& label = wxEmptyString);
    wxAuiToolBarItem* AddSeparator();
    wxAuiToolBarItem* AddSpacer(int pixels);
    wxAuiToolBarItem* AddStretchSpacer(int proportion = 1);

    bool Realize();

    wxControl* FindControl(int windowId);
    wxAuiToolBarItem* FindToolByPosition(wxCoord x, wxCoord y) const;
    wxAuiToolBarItem* FindToolByIndex(int idx) const;
    wxAuiToolBarItem* FindTool(int toolId) const;

    void ClearTools() { Clear() ; }
    void Clear();
    bool DeleteTool(int toolId);
    bool DeleteByIndex(int toolId);

    size_t GetToolCount() const;
    int GetToolPos(int toolId) const { return GetToolIndex(toolId); }
    int GetToolIndex(int toolId) const;
    bool GetToolFits(int toolId) const;
    wxRect GetToolRect(int toolId) const;
    bool GetToolFitsByIndex(int toolId) const;
    bool GetToolBarFits() const;

    void SetMargins(const wxSize& size) { SetMargins(size.x, size.x, size.y, size.y); }
    void SetMargins(int x, int y) { SetMargins(x, x, y, y); }
    void SetMargins(int left, int right, int top, int bottom);

    void SetToolBitmapSize(const wxSize& size);
    wxSize GetToolBitmapSize() const;

    bool GetOverflowVisible() const;
    void SetOverflowVisible(bool visible);

    bool GetGripperVisible() const;
    void SetGripperVisible(bool visible);

    void ToggleTool(int toolId, bool state);
    bool GetToolToggled(int toolId) const;

    void EnableTool(int toolId, bool state);
    bool GetToolEnabled(int toolId) const;

    void SetToolDropDown(int toolId, bool dropdown);
    bool GetToolDropDown(int toolId) const;

    void SetToolBorderPadding(int padding);
    int  GetToolBorderPadding() const;

    void SetToolTextOrientation(int orientation);
    int  GetToolTextOrientation() const;

    void SetToolPacking(int packing);
    int  GetToolPacking() const;

    void SetToolProportion(int toolId, int proportion);
    int  GetToolProportion(int toolId) const;

    void SetToolSeparation(int separation);
    int GetToolSeparation() const;

    void SetToolSticky(int toolId, bool sticky);
    bool GetToolSticky(int toolId) const;

    wxString GetToolLabel(int toolId) const;
    void SetToolLabel(int toolId, const wxString& label);

    wxBitmap GetToolBitmap(int toolId) const;
    void SetToolBitmap(int toolId, const wxBitmap& bitmap);

    wxString GetToolShortHelp(int toolId) const;
    void SetToolShortHelp(int toolId, const wxString& helpString);

    wxString GetToolLongHelp(int toolId) const;
    void SetToolLongHelp(int toolId, const wxString& helpString);

    void SetCustomOverflowItems(const wxAuiToolBarItemArray& prepend,
                                const wxAuiToolBarItemArray& append);

    // get size of hint rectangle for a particular dock location
    wxSize GetHintSize(int dockDirection) const;
    bool IsPaneValid(const wxAuiPaneInfo& pane) const;

    // Override to call DoIdleUpdate().
    virtual void UpdateWindowUI(long flags = wxUPDATE_UI_NONE);

protected:
    void Init();

    virtual void OnCustomRender(wxDC& WXUNUSED(dc),
                                const wxAuiToolBarItem& WXUNUSED(item),
                                const wxRect& WXUNUSED(rect)) { }

protected:

    void DoIdleUpdate();
    void SetOrientation(int orientation);
    void SetHoverItem(wxAuiToolBarItem* item);
    void SetPressedItem(wxAuiToolBarItem* item);
    void RefreshOverflowState();

    int GetOverflowState() const;
    wxRect GetOverflowRect() const;
    wxSize GetLabelSize(const wxString& label);
    wxAuiToolBarItem* FindToolByPositionWithPacking(wxCoord x, wxCoord y) const;

    void DoSetSize(int x,
                   int y,
                   int width,
                   int height,
                   int sizeFlags = wxSIZE_AUTO);

protected: // handlers

    void OnSize(wxSizeEvent& evt);
    void OnIdle(wxIdleEvent& evt);
    void OnPaint(wxPaintEvent& evt);
    void OnEraseBackground(wxEraseEvent& evt);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnRightDown(wxMouseEvent& evt);
    void OnRightUp(wxMouseEvent& evt);
    void OnMiddleDown(wxMouseEvent& evt);
    void OnMiddleUp(wxMouseEvent& evt);
    void OnMotion(wxMouseEvent& evt);
    void OnLeaveWindow(wxMouseEvent& evt);
    void OnCaptureLost(wxMouseCaptureLostEvent& evt);
    void OnSetCursor(wxSetCursorEvent& evt);

protected:

    wxAuiToolBarItemArray m_items;      // array of toolbar items
    wxAuiToolBarArt* m_art;             // art provider
    wxBoxSizer* m_sizer;                // main sizer for toolbar
    wxAuiToolBarItem* m_actionItem;    // item that's being acted upon (pressed)
    wxAuiToolBarItem* m_tipItem;       // item that has its tooltip shown
    wxBitmap m_bitmap;                  // double-buffer bitmap
    wxSizerItem* m_gripperSizerItem;
    wxSizerItem* m_overflowSizerItem;
    wxSize m_absoluteMinSize;
    wxPoint m_actionPos;               // position of left-mouse down
    wxAuiToolBarItemArray m_customOverflowPrepend;
    wxAuiToolBarItemArray m_customOverflowAppend;

    int m_buttonWidth;
    int m_buttonHeight;
    int m_sizerElementCount;
    int m_leftPadding;
    int m_rightPadding;
    int m_topPadding;
    int m_bottomPadding;
    int m_toolPacking;
    int m_toolBorderPadding;
    int m_toolTextOrientation;
    int m_overflowState;
    bool m_dragging;
    bool m_gripperVisible;
    bool m_overflowVisible;

    bool RealizeHelper(wxClientDC& dc, bool horizontal);
    static bool IsPaneValid(long style, const wxAuiPaneInfo& pane);
    bool IsPaneValid(long style) const;
    void SetArtFlags() const;
    wxOrientation m_orientation;
    wxSize m_horzHintSize;
    wxSize m_vertHintSize;

private:
    // Common part of OnLeaveWindow() and OnCaptureLost().
    void DoResetMouseState();

    DECLARE_EVENT_TABLE()
    DECLARE_CLASS(wxAuiToolBar)
};




// wx event machinery

#ifndef SWIG

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_AUI, wxEVT_AUITOOLBAR_TOOL_DROPDOWN, wxAuiToolBarEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_AUI, wxEVT_AUITOOLBAR_OVERFLOW_CLICK, wxAuiToolBarEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_AUI, wxEVT_AUITOOLBAR_RIGHT_CLICK, wxAuiToolBarEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_AUI, wxEVT_AUITOOLBAR_MIDDLE_CLICK, wxAuiToolBarEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_AUI, wxEVT_AUITOOLBAR_BEGIN_DRAG, wxAuiToolBarEvent );

typedef void (wxEvtHandler::*wxAuiToolBarEventFunction)(wxAuiToolBarEvent&);

#define wxAuiToolBarEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxAuiToolBarEventFunction, func)

#define EVT_AUITOOLBAR_TOOL_DROPDOWN(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, winid, wxAuiToolBarEventHandler(fn))
#define EVT_AUITOOLBAR_OVERFLOW_CLICK(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_AUITOOLBAR_OVERFLOW_CLICK, winid, wxAuiToolBarEventHandler(fn))
#define EVT_AUITOOLBAR_RIGHT_CLICK(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_AUITOOLBAR_RIGHT_CLICK, winid, wxAuiToolBarEventHandler(fn))
#define EVT_AUITOOLBAR_MIDDLE_CLICK(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_AUITOOLBAR_MIDDLE_CLICK, winid, wxAuiToolBarEventHandler(fn))
#define EVT_AUITOOLBAR_BEGIN_DRAG(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_AUITOOLBAR_BEGIN_DRAG, winid, wxAuiToolBarEventHandler(fn))

#else

// wxpython/swig event work
%constant wxEventType wxEVT_AUITOOLBAR_TOOL_DROPDOWN;
%constant wxEventType wxEVT_AUITOOLBAR_OVERFLOW_CLICK;
%constant wxEventType wxEVT_AUITOOLBAR_RIGHT_CLICK;
%constant wxEventType wxEVT_AUITOOLBAR_MIDDLE_CLICK;
%constant wxEventType wxEVT_AUITOOLBAR_BEGIN_DRAG;

%pythoncode {
    EVT_AUITOOLBAR_TOOL_DROPDOWN = wx.PyEventBinder( wxEVT_AUITOOLBAR_TOOL_DROPDOWN, 1 )
    EVT_AUITOOLBAR_OVERFLOW_CLICK = wx.PyEventBinder( wxEVT_AUITOOLBAR_OVERFLOW_CLICK, 1 )
    EVT_AUITOOLBAR_RIGHT_CLICK = wx.PyEventBinder( wxEVT_AUITOOLBAR_RIGHT_CLICK, 1 )
    EVT_AUITOOLBAR_MIDDLE_CLICK = wx.PyEventBinder( wxEVT_AUITOOLBAR_MIDDLE_CLICK, 1 )
    EVT_AUITOOLBAR_BEGIN_DRAG = wx.PyEventBinder( wxEVT_AUITOOLBAR_BEGIN_DRAG, 1 )
}
#endif  // SWIG

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_AUITOOLBAR_TOOL_DROPDOWN    wxEVT_AUITOOLBAR_TOOL_DROPDOWN
#define wxEVT_COMMAND_AUITOOLBAR_OVERFLOW_CLICK   wxEVT_AUITOOLBAR_OVERFLOW_CLICK
#define wxEVT_COMMAND_AUITOOLBAR_RIGHT_CLICK      wxEVT_AUITOOLBAR_RIGHT_CLICK
#define wxEVT_COMMAND_AUITOOLBAR_MIDDLE_CLICK     wxEVT_AUITOOLBAR_MIDDLE_CLICK
#define wxEVT_COMMAND_AUITOOLBAR_BEGIN_DRAG       wxEVT_AUITOOLBAR_BEGIN_DRAG

#endif  // wxUSE_AUI
#endif  // _WX_AUIBAR_H_

