/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/checkbox.cpp
// Purpose:     wxCheckBox
// Author:      Julian Smart
// Modified by:
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

#if wxUSE_CHECKBOX

#include "wx/checkbox.h"

#ifndef WX_PRECOMP
    #include "wx/brush.h"
    #include "wx/dcclient.h"
    #include "wx/dcscreen.h"
    #include "wx/settings.h"
#endif

#include "wx/msw/dc.h"          // for wxDCTemp
#include "wx/renderer.h"
#include "wx/msw/uxtheme.h"
#include "wx/msw/private/button.h"
#include "wx/msw/missing.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#ifndef BP_CHECKBOX
    #define BP_CHECKBOX 3
#endif

// these values are defined in tmschema.h (except the first one)
enum
{
    CBS_INVALID,
    CBS_UNCHECKEDNORMAL,
    CBS_UNCHECKEDHOT,
    CBS_UNCHECKEDPRESSED,
    CBS_UNCHECKEDDISABLED,
    CBS_CHECKEDNORMAL,
    CBS_CHECKEDHOT,
    CBS_CHECKEDPRESSED,
    CBS_CHECKEDDISABLED,
    CBS_MIXEDNORMAL,
    CBS_MIXEDHOT,
    CBS_MIXEDPRESSED,
    CBS_MIXEDDISABLED
};

// these are our own
enum
{
    CBS_HOT_OFFSET = 1,
    CBS_PRESSED_OFFSET = 2,
    CBS_DISABLED_OFFSET = 3
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxCheckBox creation
// ----------------------------------------------------------------------------

void wxCheckBox::Init()
{
    m_state = wxCHK_UNCHECKED;
    m_isPressed =
    m_isHot = false;
}

bool wxCheckBox::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxString& label,
                        const wxPoint& pos,
                        const wxSize& size, long style,
                        const wxValidator& validator,
                        const wxString& name)
{
    Init();

    WXValidateStyle(&style);
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    WXDWORD exstyle;
    WXDWORD msStyle = MSWGetStyle(style, &exstyle);

    msStyle |= wxMSWButton::GetMultilineStyle(label);

    return MSWCreateControl(wxT("BUTTON"), msStyle, pos, size, label, exstyle);
}

WXDWORD wxCheckBox::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    // buttons never have an external border, they draw their own one
    WXDWORD msStyle = wxControl::MSWGetStyle(style, exstyle);

    if ( style & wxCHK_3STATE )
        msStyle |= BS_3STATE;
    else
        msStyle |= BS_CHECKBOX;

    if ( style & wxALIGN_RIGHT )
    {
        msStyle |= BS_LEFTTEXT | BS_RIGHT;
    }

    return msStyle;
}

// ----------------------------------------------------------------------------
// wxCheckBox geometry
// ----------------------------------------------------------------------------

wxSize wxCheckBox::DoGetBestClientSize() const
{
    static int s_checkSize = 0;

    if ( !s_checkSize )
    {
        wxScreenDC dc;
        dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));

        s_checkSize = dc.GetCharHeight();
    }

    wxString str = wxGetWindowText(GetHWND());

    int wCheckbox, hCheckbox;
    if ( !str.empty() )
    {
        wxClientDC dc(const_cast<wxCheckBox *>(this));
        dc.SetFont(GetFont());
        dc.GetMultiLineTextExtent(GetLabelText(str), &wCheckbox, &hCheckbox);
        wCheckbox += s_checkSize + GetCharWidth();

        if ( ::GetWindowLong(GetHwnd(), GWL_STYLE) & BS_MULTILINE )
        {
            // We need to make the checkbox even wider in this case because
            // otherwise it wraps lines automatically and not only on "\n"s as
            // we need and this makes the size computed here wrong resulting in
            // checkbox contents being truncated when it's actually displayed.
            // Without this hack simple checkbox with "Some thing\n and more"
            // label appears on 3 lines, not 2, under Windows 2003 using
            // classic look and feel (although it works fine under Windows 7,
            // with or without themes).
            wCheckbox += s_checkSize;
        }

        if ( hCheckbox < s_checkSize )
            hCheckbox = s_checkSize;
    }
    else
    {
        wCheckbox = s_checkSize;
        hCheckbox = s_checkSize;
    }
#ifdef __WXWINCE__
    hCheckbox += 1;
#endif

    wxSize best(wCheckbox, hCheckbox);
    CacheBestSize(best);
    return best;
}

// ----------------------------------------------------------------------------
// wxCheckBox operations
// ----------------------------------------------------------------------------

void wxCheckBox::SetLabel(const wxString& label)
{
    wxMSWButton::UpdateMultilineStyle(GetHwnd(), label);

    wxCheckBoxBase::SetLabel(label);
}

void wxCheckBox::SetValue(bool val)
{
    Set3StateValue(val ? wxCHK_CHECKED : wxCHK_UNCHECKED);
}

bool wxCheckBox::GetValue() const
{
    return Get3StateValue() != wxCHK_UNCHECKED;
}

void wxCheckBox::Command(wxCommandEvent& event)
{
    int state = event.GetInt();
    wxCHECK_RET( (state == wxCHK_UNCHECKED) || (state == wxCHK_CHECKED)
        || (state == wxCHK_UNDETERMINED),
        wxT("event.GetInt() returned an invalid checkbox state") );

    Set3StateValue((wxCheckBoxState) state);
    ProcessCommand(event);
}

wxCOMPILE_TIME_ASSERT(wxCHK_UNCHECKED == BST_UNCHECKED
    && wxCHK_CHECKED == BST_CHECKED
    && wxCHK_UNDETERMINED == BST_INDETERMINATE, EnumValuesIncorrect);

void wxCheckBox::DoSet3StateValue(wxCheckBoxState state)
{
    m_state = state;
    if ( !IsOwnerDrawn() )
        ::SendMessage(GetHwnd(), BM_SETCHECK, (WPARAM) state, 0);
    else // owner drawn buttons don't react to this message
        Refresh();
}

wxCheckBoxState wxCheckBox::DoGet3StateValue() const
{
    return m_state;
}

bool wxCheckBox::MSWCommand(WXUINT cmd, WXWORD WXUNUSED(id))
{
    if ( cmd != BN_CLICKED && cmd != BN_DBLCLK )
        return false;

    // first update the value so that user event handler gets the new checkbox
    // value

    // ownerdrawn buttons don't manage their state themselves unlike usual
    // auto checkboxes so do it ourselves in any case
    wxCheckBoxState state;
    if ( Is3rdStateAllowedForUser() )
    {
        state = (wxCheckBoxState)((m_state + 1) % 3);
    }
    else // 2 state checkbox (at least from users point of view)
    {
        // note that wxCHK_UNDETERMINED also becomes unchecked when clicked
        state = m_state == wxCHK_UNCHECKED ? wxCHK_CHECKED : wxCHK_UNCHECKED;
    }

    DoSet3StateValue(state);


    // generate the event
    wxCommandEvent event(wxEVT_CHECKBOX, m_windowId);

    event.SetInt(state);
    event.SetEventObject(this);
    ProcessCommand(event);

    return true;
}

// ----------------------------------------------------------------------------
// owner drawn checkboxes stuff
// ----------------------------------------------------------------------------

bool wxCheckBox::SetForegroundColour(const wxColour& colour)
{
    if ( !wxCheckBoxBase::SetForegroundColour(colour) )
        return false;

    // the only way to change the checkbox foreground colour under Windows XP
    // is to owner draw it
    if ( wxUxThemeEngine::GetIfActive() )
        MSWMakeOwnerDrawn(colour.IsOk());

    return true;
}

bool wxCheckBox::IsOwnerDrawn() const
{
    return
        (::GetWindowLong(GetHwnd(), GWL_STYLE) & BS_OWNERDRAW) == BS_OWNERDRAW;
}

void wxCheckBox::MSWMakeOwnerDrawn(bool ownerDrawn)
{
    long style = ::GetWindowLong(GetHwnd(), GWL_STYLE);

    // note that BS_CHECKBOX & BS_OWNERDRAW != 0 so we can't operate on
    // them as on independent style bits
    if ( ownerDrawn )
    {
        style &= ~(BS_CHECKBOX | BS_3STATE);
        style |= BS_OWNERDRAW;

        Connect(wxEVT_ENTER_WINDOW,
                wxMouseEventHandler(wxCheckBox::OnMouseEnterOrLeave));
        Connect(wxEVT_LEAVE_WINDOW,
                wxMouseEventHandler(wxCheckBox::OnMouseEnterOrLeave));
        Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(wxCheckBox::OnMouseLeft));
        Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxCheckBox::OnMouseLeft));
        Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(wxCheckBox::OnFocus));
        Connect(wxEVT_KILL_FOCUS, wxFocusEventHandler(wxCheckBox::OnFocus));
    }
    else // reset to default colour
    {
        style &= ~BS_OWNERDRAW;
        style |= HasFlag(wxCHK_3STATE) ? BS_3STATE : BS_CHECKBOX;

        Disconnect(wxEVT_ENTER_WINDOW,
                   wxMouseEventHandler(wxCheckBox::OnMouseEnterOrLeave));
        Disconnect(wxEVT_LEAVE_WINDOW,
                   wxMouseEventHandler(wxCheckBox::OnMouseEnterOrLeave));
        Disconnect(wxEVT_LEFT_DOWN, wxMouseEventHandler(wxCheckBox::OnMouseLeft));
        Disconnect(wxEVT_LEFT_UP, wxMouseEventHandler(wxCheckBox::OnMouseLeft));
        Disconnect(wxEVT_SET_FOCUS, wxFocusEventHandler(wxCheckBox::OnFocus));
        Disconnect(wxEVT_KILL_FOCUS, wxFocusEventHandler(wxCheckBox::OnFocus));
    }

    ::SetWindowLong(GetHwnd(), GWL_STYLE, style);

    if ( !ownerDrawn )
    {
        // ensure that controls state is consistent with internal state
        DoSet3StateValue(m_state);
    }
}

void wxCheckBox::OnMouseEnterOrLeave(wxMouseEvent& event)
{
    m_isHot = event.GetEventType() == wxEVT_ENTER_WINDOW;
    if ( !m_isHot )
        m_isPressed = false;

    Refresh();

    event.Skip();
}

void wxCheckBox::OnMouseLeft(wxMouseEvent& event)
{
    // TODO: we should capture the mouse here to be notified about left up
    //       event but this interferes with BN_CLICKED generation so if we
    //       want to do this we'd need to generate them ourselves
    m_isPressed = event.GetEventType() == wxEVT_LEFT_DOWN;
    Refresh();

    event.Skip();
}

void wxCheckBox::OnFocus(wxFocusEvent& event)
{
    Refresh();

    event.Skip();
}

bool wxCheckBox::MSWOnDraw(WXDRAWITEMSTRUCT *item)
{
    DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)item;

    if ( !IsOwnerDrawn() || dis->CtlType != ODT_BUTTON )
        return wxCheckBoxBase::MSWOnDraw(item);

    // calculate the rectangles for the check mark itself and the label
    HDC hdc = dis->hDC;
    RECT& rect = dis->rcItem;
    RECT rectCheck,
         rectLabel;
    rectLabel.top = rect.top + (rect.bottom - rect.top - GetBestSize().y) / 2;
    rectLabel.bottom = rectLabel.top + GetBestSize().y;
    const int MARGIN = 3;
    const int CXMENUCHECK = ::GetSystemMetrics(SM_CXMENUCHECK);
    // the space between the checkbox and the label is included in the
    // check-mark bitmap
    const int checkSize = wxMin(CXMENUCHECK - MARGIN, GetSize().y);
    rectCheck.top = rect.top + (rect.bottom - rect.top - checkSize) / 2;
    rectCheck.bottom = rectCheck.top + checkSize;

    const bool isRightAligned = HasFlag(wxALIGN_RIGHT);
    if ( isRightAligned )
    {
        rectLabel.right = rect.right - CXMENUCHECK;
        rectLabel.left = rect.left;

        rectCheck.left = rectLabel.right + ( CXMENUCHECK + MARGIN - checkSize ) / 2;
        rectCheck.right = rectCheck.left + checkSize;
    }
    else // normal, left-aligned checkbox
    {
        rectCheck.left = rect.left + ( CXMENUCHECK - MARGIN - checkSize ) / 2;
        rectCheck.right = rectCheck.left + checkSize;

        rectLabel.left = rect.left + CXMENUCHECK;
        rectLabel.right = rect.right;
    }

    // shall we draw a focus rect?
    const bool isFocused = m_isPressed || FindFocus() == this;


    // draw the checkbox itself
    wxDCTemp dc(hdc);

    int flags = 0;
    if ( !IsEnabled() )
        flags |= wxCONTROL_DISABLED;
    switch ( Get3StateValue() )
    {
        case wxCHK_CHECKED:
            flags |= wxCONTROL_CHECKED;
            break;

        case wxCHK_UNDETERMINED:
            flags |= wxCONTROL_PRESSED;
            break;

        default:
            wxFAIL_MSG( wxT("unexpected Get3StateValue() return value") );
            // fall through

        case wxCHK_UNCHECKED:
            // no extra styles needed
            break;
    }

    if ( wxFindWindowAtPoint(wxGetMousePosition()) == this )
        flags |= wxCONTROL_CURRENT;

    wxRendererNative::Get().
        DrawCheckBox(this, dc, wxRectFromRECT(rectCheck), flags);

    // draw the text
    const wxString& label = GetLabel();

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

    if ( !IsEnabled() )
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

#endif // wxUSE_CHECKBOX
