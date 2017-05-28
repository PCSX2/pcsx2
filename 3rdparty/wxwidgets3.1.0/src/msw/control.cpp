/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/control.cpp
// Purpose:     wxControl class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
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

#if wxUSE_CONTROLS

#include "wx/control.h"

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/event.h"
    #include "wx/app.h"
    #include "wx/dcclient.h"
    #include "wx/log.h"
    #include "wx/settings.h"
    #include "wx/ctrlsub.h"
#endif

#if wxUSE_LISTCTRL
    #include "wx/listctrl.h"
#endif // wxUSE_LISTCTRL

#if wxUSE_TREECTRL
    #include "wx/treectrl.h"
#endif // wxUSE_TREECTRL

#include "wx/renderer.h"
#include "wx/msw/private.h"
#include "wx/msw/uxtheme.h"
#include "wx/msw/dc.h"          // for wxDCTemp
#include "wx/msw/ownerdrawnbutton.h"

// Missing from MinGW 4.8 SDK headers.
#ifndef BS_TYPEMASK
#define BS_TYPEMASK 0xf
#endif

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxControl, wxWindow);

// ============================================================================
// wxControl implementation
// ============================================================================

// ----------------------------------------------------------------------------
// control window creation
// ----------------------------------------------------------------------------

bool wxControl::Create(wxWindow *parent,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxValidator& wxVALIDATOR_PARAM(validator),
                       const wxString& name)
{
    if ( !wxWindow::Create(parent, id, pos, size, style, name) )
        return false;

#if wxUSE_VALIDATORS
    SetValidator(validator);
#endif

    return true;
}

bool wxControl::MSWCreateControl(const wxChar *classname,
                                 const wxString& label,
                                 const wxPoint& pos,
                                 const wxSize& size)
{
    WXDWORD exstyle;
    WXDWORD msStyle = MSWGetStyle(GetWindowStyle(), &exstyle);

    return MSWCreateControl(classname, msStyle, pos, size, label, exstyle);
}

bool wxControl::MSWCreateControl(const wxChar *classname,
                                 WXDWORD style,
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 const wxString& label,
                                 WXDWORD exstyle)
{
    // if no extended style given, determine it ourselves
    if ( exstyle == (WXDWORD)-1 )
    {
        exstyle = 0;
        (void) MSWGetStyle(GetWindowStyle(), &exstyle);
    }

    // all controls should have this style
    style |= WS_CHILD;

    // create the control visible if it's currently shown for wxWidgets
    if ( m_isShown )
    {
        style |= WS_VISIBLE;
    }

    // choose the position for the control: we have a problem with default size
    // here as we can't calculate the best size before the control exists
    // (DoGetBestSize() may need to use m_hWnd), so just choose the minimal
    // possible but non 0 size because 0 window width/height result in problems
    // elsewhere
    int x = pos.x == wxDefaultCoord ? 0 : pos.x,
        y = pos.y == wxDefaultCoord ? 0 : pos.y,
        w = size.x == wxDefaultCoord ? 1 : size.x,
        h = size.y == wxDefaultCoord ? 1 : size.y;

    // ... and adjust it to account for a possible parent frames toolbar
    AdjustForParentClientOrigin(x, y);

    m_hWnd = (WXHWND)::CreateWindowEx
                       (
                        exstyle,            // extended style
                        classname,          // the kind of control to create
                        label.t_str(),      // the window name
                        style,              // the window style
                        x, y, w, h,         // the window position and size
                        GetHwndOf(GetParent()),         // parent
                        (HMENU)wxUIntToPtr(GetId()),    // child id
                        wxGetInstance(),    // app instance
                        NULL                // creation parameters
                       );

    if ( !m_hWnd )
    {
        wxLogLastError(wxString::Format
                       (
                        wxT("CreateWindowEx(\"%s\", flags=%08lx, ex=%08lx)"),
                        classname, style, exstyle
                       ));

        return false;
    }

#if !wxUSE_UNICODE
    // Text labels starting with the character 0xff (which is a valid character
    // in many code pages) don't appear correctly as CreateWindowEx() has some
    // special treatment for this case, apparently the strings starting with -1
    // are not really strings but something called "ordinals". There is no
    // documentation about it but the fact is that the label gets mangled or
    // not displayed at all if we don't do this, see #9572.
    //
    // Notice that 0xffff is not a valid Unicode character so the problem
    // doesn't arise in Unicode build.
    if ( !label.empty() && label[0] == -1 )
        ::SetWindowText(GetHwnd(), label.t_str());
#endif // !wxUSE_UNICODE

    // saving the label in m_labelOrig to return it verbatim
    // later in GetLabel()
    m_labelOrig = label;

    // install wxWidgets window proc for this window
    SubclassWin(m_hWnd);

    // set up fonts and colours
    InheritAttributes();
    if ( !m_hasFont )
    {
        bool setFont = true;

        wxFont font = GetDefaultAttributes().font;

        // if we set a font for {list,tree}ctrls and the font size is changed in
        // the display properties then the font size for these controls doesn't
        // automatically adjust when they receive WM_SETTINGCHANGE

        // FIXME: replace the dynamic casts with virtual function calls!!
#if wxUSE_LISTCTRL || wxUSE_TREECTRL
        bool testFont = false;
#if wxUSE_LISTCTRL
        if ( wxDynamicCastThis(wxListCtrl) )
            testFont = true;
#endif // wxUSE_LISTCTRL
#if wxUSE_TREECTRL
        if ( wxDynamicCastThis(wxTreeCtrl) )
            testFont = true;
#endif // wxUSE_TREECTRL

        if ( testFont )
        {
            // not sure if we need to explicitly set the font here for Win95/NT4
            // but we definitely can't do it for any newer version
            // see wxGetCCDefaultFont() in src/msw/settings.cpp for explanation
            // of why this test works

            // TODO: test Win95/NT4 to see if this is needed or breaks the
            // font resizing as it does on newer versions
            if ( font != wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT) )
            {
                setFont = false;
            }
        }
#endif // wxUSE_LISTCTRL || wxUSE_TREECTRL

        if ( setFont )
        {
            SetFont(GetDefaultAttributes().font);
        }
    }

    // set the size now if no initial size specified
    SetInitialSize(size);

    return true;
}

// ----------------------------------------------------------------------------
// various accessors
// ----------------------------------------------------------------------------

WXDWORD wxControl::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    long msStyle = wxWindow::MSWGetStyle(style, exstyle);

    if ( AcceptsFocusFromKeyboard() )
    {
        msStyle |= WS_TABSTOP;
    }

    return msStyle;
}

wxSize wxControl::DoGetBestSize() const
{
    if (m_windowSizer)
       return wxControlBase::DoGetBestSize();

    return wxSize(DEFAULT_ITEM_WIDTH, DEFAULT_ITEM_HEIGHT);
}

wxBorder wxControl::GetDefaultBorder() const
{
    return wxControlBase::GetDefaultBorder();
}

// This is a helper for all wxControls made with UPDOWN native control.
// In wxMSW it was only wxSpinCtrl derived from wxSpinButton but in
// WinCE of Smartphones this happens also for native wxTextCtrl,
// wxChoice and others.
wxSize wxControl::GetBestSpinnerSize(const bool is_vertical) const
{
    // take size according to layout
    wxSize bestSize(
#if defined(__SMARTPHONE__) && defined(__WXWINCE__)
                    0,GetCharHeight()
#else
                    ::GetSystemMetrics(is_vertical ? SM_CXVSCROLL : SM_CXHSCROLL),
                    ::GetSystemMetrics(is_vertical ? SM_CYVSCROLL : SM_CYHSCROLL)
#endif
    );

    // correct size as for undocumented MSW variants cases (WinCE and perhaps others)
    if (bestSize.x==0)
        bestSize.x = bestSize.y;
    if (bestSize.y==0)
        bestSize.y = bestSize.x;

    // double size according to layout
    if (is_vertical)
        bestSize.y *= 2;
    else
        bestSize.x *= 2;

    return bestSize;
}

/* static */ wxVisualAttributes
wxControl::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    wxVisualAttributes attrs;

    // old school (i.e. not "common") controls use the standard dialog font
    // by default
    attrs.font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);

    // most, or at least many, of the controls use the same colours as the
    // buttons -- others will have to override this (and possibly simply call
    // GetCompositeControlsDefaultAttributes() from their versions)
    attrs.colFg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT);
    attrs.colBg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);

    return attrs;
}

// ----------------------------------------------------------------------------
// message handling
// ----------------------------------------------------------------------------

bool wxControl::ProcessCommand(wxCommandEvent& event)
{
    return HandleWindowEvent(event);
}

bool wxControl::MSWOnNotify(int idCtrl,
                            WXLPARAM lParam,
                            WXLPARAM* result)
{
    wxEventType eventType wxDUMMY_INITIALIZE(wxEVT_NULL);

    NMHDR *hdr = (NMHDR*) lParam;
    switch ( hdr->code )
    {
        case NM_CLICK:
            eventType = wxEVT_COMMAND_LEFT_CLICK;
            break;

        case NM_DBLCLK:
            eventType = wxEVT_COMMAND_LEFT_DCLICK;
            break;

        case NM_RCLICK:
            eventType = wxEVT_COMMAND_RIGHT_CLICK;
            break;

        case NM_RDBLCLK:
            eventType = wxEVT_COMMAND_RIGHT_DCLICK;
            break;

        case NM_SETFOCUS:
            eventType = wxEVT_COMMAND_SET_FOCUS;
            break;

        case NM_KILLFOCUS:
            eventType = wxEVT_COMMAND_KILL_FOCUS;
            break;

        case NM_RETURN:
            eventType = wxEVT_COMMAND_ENTER;
            break;

        default:
            return wxWindow::MSWOnNotify(idCtrl, lParam, result);
    }

    wxCommandEvent event(wxEVT_NULL, m_windowId);
    event.SetEventType(eventType);
    event.SetEventObject(this);

    return HandleWindowEvent(event);
}

WXHBRUSH wxControl::DoMSWControlColor(WXHDC pDC, wxColour colBg, WXHWND hWnd)
{
    HDC hdc = (HDC)pDC;

    WXHBRUSH hbr = 0;
    if ( !colBg.IsOk() )
    {
        wxWindow *win = wxFindWinFromHandle( hWnd );
        if ( !win )
        {
            // If this HWND doesn't correspond to a wxWindow, it still might be
            // one of its children for which we need to set the background
            // brush, e.g. this is the case for the EDIT control that is part
            // of wxComboBox but also e.g. of wxSlider label HWNDs which are
            // logically part of it, but are siblings of the main control at
            // Windows level.
            //
            // So check whether it's a sibling of this window which is part of
            // the same wx object.
            if ( ContainsHWND(hWnd) )
            {
                win = this;
            }
            else // Or maybe a child sub-window of this one.
            {
                HWND parent = ::GetParent(hWnd);
                if ( parent )
                {
                    wxWindow *winParent = wxFindWinFromHandle( parent );
                    if( winParent && winParent->ContainsHWND( hWnd ) )
                        win = winParent;
                 }
            }
        }

        if ( win )
            hbr = win->MSWGetBgBrush(pDC);

        // if the control doesn't have any bg colour, foreground colour will be
        // ignored as the return value would be 0 -- so forcefully give it a
        // non default background brush in this case
        if ( !hbr && m_hasFgCol )
            colBg = GetBackgroundColour();
    }

    // use the background colour override if a valid colour is given: this is
    // used when the control is disabled to grey it out and also if colBg was
    // set just above
    if ( colBg.IsOk() )
    {
        wxBrush *brush = wxTheBrushList->FindOrCreateBrush(colBg);
        hbr = (WXHBRUSH)brush->GetResourceHandle();
    }

    // always set the foreground colour if we changed the background, whether
    // m_hasFgCol is true or not: if it true, we must do it, of course, but
    // even if it isn't, we must set the default foreground explicitly as by
    // default just the simple black is used
    if ( hbr )
    {
        ::SetTextColor(hdc, wxColourToRGB(GetForegroundColour()));
    }

    // finally also set the background colour for text drawing: without this,
    // the text in an edit control is drawn using the default background even
    // if we return a valid brush
    if ( colBg.IsOk() || m_hasBgCol )
    {
        if ( !colBg.IsOk() )
            colBg = GetBackgroundColour();

        ::SetBkColor(hdc, wxColourToRGB(colBg));
    }

    return hbr;
}

WXHBRUSH wxControl::MSWControlColor(WXHDC pDC, WXHWND hWnd)
{
    if ( HasTransparentBackground() )
        ::SetBkMode((HDC)pDC, TRANSPARENT);

    // don't pass any background colour to DoMSWControlColor(), our own
    // background colour will be used by it only if it is set, otherwise the
    // defaults will be used
    return DoMSWControlColor(pDC, wxColour(), hWnd);
}

WXHBRUSH wxControl::MSWControlColorDisabled(WXHDC pDC)
{
    return DoMSWControlColor(pDC,
                             wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE),
                             GetHWND());
}

wxWindow* wxControl::MSWFindItem(long id, WXHWND hWnd) const
{
    // is it us or one of our "internal" children?
    if ( id == GetId() || (GetSubcontrols().Index(id) != wxNOT_FOUND) )
        return const_cast<wxControl *>(this);

    return wxControlBase::MSWFindItem(id, hWnd);
}

// ----------------------------------------------------------------------------
// Owner drawn buttons support.
// ----------------------------------------------------------------------------

void
wxMSWOwnerDrawnButtonBase::MSWMakeOwnerDrawnIfNecessary(const wxColour& colFg)
{
    // The only way to change the checkbox foreground colour when using
    // themes is to owner draw it.
    if ( wxUxThemeEngine::GetIfActive() )
        MSWMakeOwnerDrawn(colFg.IsOk());
}

bool wxMSWOwnerDrawnButtonBase::MSWIsOwnerDrawn() const
{
    return
        (::GetWindowLong(GetHwndOf(m_win), GWL_STYLE) & BS_OWNERDRAW) == BS_OWNERDRAW;
}

void wxMSWOwnerDrawnButtonBase::MSWMakeOwnerDrawn(bool ownerDrawn)
{
    long style = ::GetWindowLong(GetHwndOf(m_win), GWL_STYLE);

    // note that BS_CHECKBOX & BS_OWNERDRAW != 0 so we can't operate on
    // them as on independent style bits
    if ( ownerDrawn )
    {
        style &= ~BS_TYPEMASK;
        style |= BS_OWNERDRAW;

        m_win->Bind(wxEVT_ENTER_WINDOW,
                    &wxMSWOwnerDrawnButtonBase::OnMouseEnterOrLeave, this);
        m_win->Bind(wxEVT_LEAVE_WINDOW,
                    &wxMSWOwnerDrawnButtonBase::OnMouseEnterOrLeave, this);

        m_win->Bind(wxEVT_LEFT_DOWN,
                    &wxMSWOwnerDrawnButtonBase::OnMouseLeft, this);
        m_win->Bind(wxEVT_LEFT_UP,
                    &wxMSWOwnerDrawnButtonBase::OnMouseLeft, this);

        m_win->Bind(wxEVT_SET_FOCUS,
                    &wxMSWOwnerDrawnButtonBase::OnFocus, this);

        m_win->Bind(wxEVT_KILL_FOCUS,
                    &wxMSWOwnerDrawnButtonBase::OnFocus, this);
    }
    else // reset to default colour
    {
        style &= ~BS_OWNERDRAW;
        style |= MSWGetButtonStyle();

        m_win->Unbind(wxEVT_ENTER_WINDOW,
                      &wxMSWOwnerDrawnButtonBase::OnMouseEnterOrLeave, this);
        m_win->Unbind(wxEVT_LEAVE_WINDOW,
                      &wxMSWOwnerDrawnButtonBase::OnMouseEnterOrLeave, this);

        m_win->Unbind(wxEVT_LEFT_DOWN,
                      &wxMSWOwnerDrawnButtonBase::OnMouseLeft, this);
        m_win->Unbind(wxEVT_LEFT_UP,
                      &wxMSWOwnerDrawnButtonBase::OnMouseLeft, this);

        m_win->Unbind(wxEVT_SET_FOCUS,
                      &wxMSWOwnerDrawnButtonBase::OnFocus, this);
        m_win->Unbind(wxEVT_KILL_FOCUS,
                      &wxMSWOwnerDrawnButtonBase::OnFocus, this);
    }

    ::SetWindowLong(GetHwndOf(m_win), GWL_STYLE, style);

    if ( !ownerDrawn )
        MSWOnButtonResetOwnerDrawn();
}

void wxMSWOwnerDrawnButtonBase::OnMouseEnterOrLeave(wxMouseEvent& event)
{
    if ( event.GetEventType() == wxEVT_LEAVE_WINDOW )
        m_isPressed = false;

    m_win->Refresh();

    event.Skip();
}

void wxMSWOwnerDrawnButtonBase::OnMouseLeft(wxMouseEvent& event)
{
    // TODO: we should capture the mouse here to be notified about left up
    //       event but this interferes with BN_CLICKED generation so if we
    //       want to do this we'd need to generate them ourselves
    m_isPressed = event.GetEventType() == wxEVT_LEFT_DOWN;
    m_win->Refresh();

    event.Skip();
}

void wxMSWOwnerDrawnButtonBase::OnFocus(wxFocusEvent& event)
{
    m_win->Refresh();

    event.Skip();
}

bool wxMSWOwnerDrawnButtonBase::MSWDrawButton(WXDRAWITEMSTRUCT *item)
{
    DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)item;

    if ( !MSWIsOwnerDrawn() || dis->CtlType != ODT_BUTTON )
        return false;

    // shall we draw a focus rect?
    const bool isFocused = m_isPressed || m_win->HasFocus();

    int flags = MSWGetButtonCheckedFlag();

    if ( dis->itemState & ODS_SELECTED )
        flags |= wxCONTROL_SELECTED | wxCONTROL_PRESSED;

    if ( !m_win->IsEnabled() )
        flags |= wxCONTROL_DISABLED;

    if ( m_isPressed )
        flags |= wxCONTROL_PRESSED;

    if ( wxFindWindowAtPoint(wxGetMousePosition()) == m_win )
        flags |= wxCONTROL_CURRENT;


    // calculate the rectangles for the button itself and the label
    HDC hdc = dis->hDC;
    const RECT& rect = dis->rcItem;

    // calculate the rectangles for the button itself and the label
    const wxSize bestSize = m_win->GetBestSize();
    RECT rectButton,
         rectLabel;
    rectLabel.top = rect.top + (rect.bottom - rect.top - bestSize.y) / 2;
    rectLabel.bottom = rectLabel.top + bestSize.y;

    // choose the values consistent with those used for native, non
    // owner-drawn, buttons
    static const int MARGIN = 3;
    int CXMENUCHECK = ::GetSystemMetrics(SM_CXMENUCHECK) + 1;

    // the buttons were even bigger under Windows XP
    if ( wxGetWinVersion() < wxWinVersion_6 )
        CXMENUCHECK += 2;

    // The space between the button and the label
    // is included in the button bitmap.
    const int buttonSize = wxMin(CXMENUCHECK - MARGIN, m_win->GetSize().y);
    rectButton.top = rect.top + (rect.bottom - rect.top - buttonSize) / 2;
    rectButton.bottom = rectButton.top + buttonSize;

    const bool isRightAligned = m_win->HasFlag(wxALIGN_RIGHT);
    if ( isRightAligned )
    {
        rectLabel.right = rect.right - CXMENUCHECK;
        rectLabel.left = rect.left;

        rectButton.left = rectLabel.right + ( CXMENUCHECK + MARGIN - buttonSize ) / 2;
        rectButton.right = rectButton.left + buttonSize;
    }
    else // normal, left-aligned button
    {
        rectButton.left = rect.left + ( CXMENUCHECK - MARGIN - buttonSize ) / 2;
        rectButton.right = rectButton.left + buttonSize;

        rectLabel.left = rect.left + CXMENUCHECK;
        rectLabel.right = rect.right;
    }

    // Erase the background.
    ::FillRect(hdc, &rect, m_win->MSWGetBgBrush(hdc));

    // draw the button itself
    wxDCTemp dc(hdc);

    MSWDrawButtonBitmap(dc, wxRectFromRECT(rectButton), flags);

    // draw the text
    const wxString& label = m_win->GetLabel();

    // first we need to measure it
    UINT fmt = DT_NOCLIP;

    // drawing underlying doesn't look well with focus rect (and the native
    // control doesn't do it)
    if ( isFocused )
        fmt |= DT_HIDEPREFIX;
    if ( isRightAligned )
        fmt |= DT_RIGHT;
    // TODO: also use DT_HIDEPREFIX if the system is configured so

    // we need to get the label real size first if we have to draw a focus rect
    // around it
    if ( isFocused )
    {
        RECT oldLabelRect = rectLabel; // needed if right aligned

        if ( !::DrawText(hdc, label.t_str(), label.length(), &rectLabel,
                         fmt | DT_CALCRECT) )
        {
            wxLogLastError(wxT("DrawText(DT_CALCRECT)"));
        }

        if ( isRightAligned )
        {
            // move the label rect to the right
            const int labelWidth = rectLabel.right - rectLabel.left;
            rectLabel.right = oldLabelRect.right;
            rectLabel.left = rectLabel.right - labelWidth;
        }
    }

    if ( flags & wxCONTROL_DISABLED )
    {
        ::SetTextColor(hdc, ::GetSysColor(COLOR_GRAYTEXT));
    }

    if ( !::DrawText(hdc, label.t_str(), label.length(), &rectLabel, fmt) )
    {
        wxLogLastError(wxT("DrawText()"));
    }

    // finally draw the focus
    if ( isFocused )
    {
        rectLabel.left--;
        rectLabel.right++;
        if ( !::DrawFocusRect(hdc, &rectLabel) )
        {
            wxLogLastError(wxT("DrawFocusRect()"));
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// wxControlWithItems
// ----------------------------------------------------------------------------

void wxControlWithItems::MSWAllocStorage(const wxArrayStringsAdapter& items,
                                         unsigned wm)
{
    const unsigned numItems = items.GetCount();
    unsigned long totalTextLength = numItems; // for trailing '\0' characters
    for ( unsigned i = 0; i < numItems; ++i )
    {
        totalTextLength += items[i].length();
    }

    if ( SendMessage((HWND)MSWGetItemsHWND(), wm, numItems,
                     (LPARAM)totalTextLength*sizeof(wxChar)) == LB_ERRSPACE )
    {
        wxLogLastError(wxT("SendMessage(XX_INITSTORAGE)"));
    }
}

int wxControlWithItems::MSWInsertOrAppendItem(unsigned pos,
                                              const wxString& item,
                                              unsigned wm)
{
    LRESULT n = SendMessage((HWND)MSWGetItemsHWND(), wm, pos,
                            wxMSW_CONV_LPARAM(item));
    if ( n == CB_ERR || n == CB_ERRSPACE )
    {
        wxLogLastError(wxT("SendMessage(XX_ADD/INSERTSTRING)"));
        return wxNOT_FOUND;
    }

    return n;
}

// ---------------------------------------------------------------------------
// global functions
// ---------------------------------------------------------------------------

// this is used in radiobox.cpp and slider95.cpp and should be removed as soon
// as it is not needed there any more!
//
// Call this repeatedly for several wnds to find the overall size
// of the widget.
// Call it initially with wxDefaultCoord for all values in rect.
// Keep calling for other widgets, and rect will be modified
// to calculate largest bounding rectangle.
void wxFindMaxSize(WXHWND wnd, RECT *rect)
{
    int left = rect->left;
    int right = rect->right;
    int top = rect->top;
    int bottom = rect->bottom;

    GetWindowRect((HWND) wnd, rect);

    if (left < 0)
        return;

    if (left < rect->left)
        rect->left = left;

    if (right > rect->right)
        rect->right = right;

    if (top < rect->top)
        rect->top = top;

    if (bottom > rect->bottom)
        rect->bottom = bottom;
}

#endif // wxUSE_CONTROLS
