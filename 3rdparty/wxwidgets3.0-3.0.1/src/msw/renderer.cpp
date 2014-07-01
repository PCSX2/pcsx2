///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/renderer.cpp
// Purpose:     implementation of wxRendererNative for Windows
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.07.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/window.h"
    #include "wx/dc.h"
    #include "wx/settings.h"
#endif //WX_PRECOMP

#include "wx/dcgraph.h"
#include "wx/scopeguard.h"
#include "wx/splitter.h"
#include "wx/renderer.h"
#include "wx/msw/private.h"
#include "wx/msw/uxtheme.h"

// tmschema.h is in Win32 Platform SDK and might not be available with earlier
// compilers
#ifndef CP_DROPDOWNBUTTON
    #define BP_PUSHBUTTON      1
    #define BP_RADIOBUTTON     2
    #define BP_CHECKBOX        3
    #define RBS_UNCHECKEDNORMAL 1
    #define RBS_CHECKEDNORMAL   (RBS_UNCHECKEDNORMAL + 4)
    #define RBS_MIXEDNORMAL     (RBS_CHECKEDNORMAL + 4)
    #define CBS_UNCHECKEDNORMAL 1
    #define CBS_CHECKEDNORMAL   (CBS_UNCHECKEDNORMAL + 4)
    #define CBS_MIXEDNORMAL     (CBS_CHECKEDNORMAL + 4)

    #define PBS_NORMAL          1
    #define PBS_HOT             2
    #define PBS_PRESSED         3
    #define PBS_DISABLED        4
    #define PBS_DEFAULTED       5

    #define CP_DROPDOWNBUTTON  1

    #define CBXS_NORMAL        1
    #define CBXS_HOT           2
    #define CBXS_PRESSED       3
    #define CBXS_DISABLED      4

    #define TVP_GLYPH           2

    #define GLPS_CLOSED         1
    #define GLPS_OPENED         2

    #define HP_HEADERITEM       1

    #define HIS_NORMAL          1
    #define HIS_HOT             2
    #define HIS_PRESSED         3

    #define TMT_HEIGHT          2417

    #define HP_HEADERSORTARROW  4
    #define HSAS_SORTEDUP       1
    #define HSAS_SORTEDDOWN     2

    #define EP_EDITTEXT         1
    #define ETS_NORMAL          1
    #define ETS_HOT             2
    #define ETS_SELECTED        3
    #define ETS_DISABLED        4
    #define ETS_FOCUSED         5
    #define ETS_READONLY        6
    #define ETS_ASSIST          7
    #define TMT_FILLCOLOR       3802
    #define TMT_TEXTCOLOR       3803
    #define TMT_BORDERCOLOR     3801
    #define TMT_EDGEFILLCOLOR   3808

    #define WP_MINBUTTON 15
    #define WP_MAXBUTTON 17
    #define WP_CLOSEBUTTON 18
    #define WP_RESTOREBUTTON 21
    #define WP_HELPBUTTON 23
#endif

#if defined(__WXWINCE__)
    #ifndef DFCS_FLAT
        #define DFCS_FLAT 0
    #endif
    #ifndef DFCS_MONO
        #define DFCS_MONO 0
    #endif
#endif

#ifndef DFCS_HOT
    #define DFCS_HOT 0x1000
#endif

// ----------------------------------------------------------------------------
// methods common to wxRendererMSW and wxRendererXP
// ----------------------------------------------------------------------------

class wxRendererMSWBase : public wxDelegateRendererNative
{
public:
    wxRendererMSWBase() { }
    wxRendererMSWBase(wxRendererNative& rendererNative)
        : wxDelegateRendererNative(rendererNative) { }

    void DrawFocusRect(wxWindow * win,
                        wxDC& dc,
                        const wxRect& rect,
                        int flags = 0);

    void DrawItemSelectionRect(wxWindow *win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags = 0);

    void DrawChoice(wxWindow* win,
                     wxDC& dc,
                     const wxRect& rect,
                     int flags = 0);

    void DrawComboBox(wxWindow* win,
                       wxDC& dc,
                       const wxRect& rect,
                       int flags = 0);

    virtual void DrawComboBoxDropButton(wxWindow *win,
                                         wxDC& dc,
                                         const wxRect& rect,
                                         int flags = 0) = 0;

    virtual void DrawTextCtrl(wxWindow* win,
                               wxDC& dc,
                               const wxRect& rect,
                               int flags = 0) = 0;
};

// ----------------------------------------------------------------------------
// wxRendererMSW: wxRendererNative implementation for "old" Win32 systems
// ----------------------------------------------------------------------------

class wxRendererMSW : public wxRendererMSWBase
{
public:
    wxRendererMSW() { }

    static wxRendererNative& Get();

    virtual void DrawComboBoxDropButton(wxWindow *win,
                                        wxDC& dc,
                                        const wxRect& rect,
                                        int flags = 0);

    virtual void DrawCheckBox(wxWindow *win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0)
    {
        DoDrawButton(DFCS_BUTTONCHECK, win, dc, rect, flags);
    }

    virtual void DrawPushButton(wxWindow *win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags = 0);

    virtual void DrawTextCtrl(wxWindow* win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0);

    virtual void DrawRadioBitmap(wxWindow* win,
                                 wxDC& dc,
                                 const wxRect& rect,
                                 int flags = 0)
    {
        DoDrawButton(DFCS_BUTTONRADIO, win, dc, rect, flags);
    }

    virtual void DrawTitleBarBitmap(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    wxTitleBarButton button,
                                    int flags = 0);

    virtual wxSize GetCheckBoxSize(wxWindow *win);

    virtual int GetHeaderButtonHeight(wxWindow *win);

    virtual int GetHeaderButtonMargin(wxWindow *win);

private:
    // wrapper of DrawFrameControl()
    void DoDrawFrameControl(UINT type,
                            UINT kind,
                            wxWindow *win,
                            wxDC& dc,
                            const wxRect& rect,
                            int flags);

    // common part of Draw{PushButton,CheckBox,RadioBitmap}(): wraps
    // DrawFrameControl(DFC_BUTTON)
    void DoDrawButton(UINT kind,
                      wxWindow *win,
                      wxDC& dc,
                      const wxRect& rect,
                      int flags)
    {
        DoDrawFrameControl(DFC_BUTTON, kind, win, dc, rect, flags);
    }

    wxDECLARE_NO_COPY_CLASS(wxRendererMSW);
};

// ----------------------------------------------------------------------------
// wxRendererXP: wxRendererNative implementation for Windows XP and later
// ----------------------------------------------------------------------------

#if wxUSE_UXTHEME

class wxRendererXP : public wxRendererMSWBase
{
public:
    wxRendererXP() : wxRendererMSWBase(wxRendererMSW::Get()) { }

    static wxRendererNative& Get();

    virtual int DrawHeaderButton(wxWindow *win,
                                  wxDC& dc,
                                  const wxRect& rect,
                                  int flags = 0,
                                  wxHeaderSortIconType sortArrow = wxHDR_SORT_ICON_NONE,
                                  wxHeaderButtonParams* params = NULL);

    virtual void DrawTreeItemButton(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0);
    virtual void DrawSplitterBorder(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0);
    virtual void DrawSplitterSash(wxWindow *win,
                                  wxDC& dc,
                                  const wxSize& size,
                                  wxCoord position,
                                  wxOrientation orient,
                                  int flags = 0);
    virtual void DrawComboBoxDropButton(wxWindow *win,
                                        wxDC& dc,
                                        const wxRect& rect,
                                        int flags = 0);
    virtual void DrawCheckBox(wxWindow *win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0)
    {
        if ( !DoDrawXPButton(BP_CHECKBOX, win, dc, rect, flags) )
            m_rendererNative.DrawCheckBox(win, dc, rect, flags);
    }

    virtual void DrawPushButton(wxWindow *win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags = 0)
    {
        if ( !DoDrawXPButton(BP_PUSHBUTTON, win, dc, rect, flags) )
            m_rendererNative.DrawPushButton(win, dc, rect, flags);
    }

    virtual void DrawTextCtrl(wxWindow* win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0);

    virtual void DrawRadioBitmap(wxWindow *win,
                                 wxDC& dc,
                                 const wxRect& rect,
                                 int flags = 0)
    {
        if ( !DoDrawXPButton(BP_RADIOBUTTON, win, dc, rect, flags) )
            m_rendererNative.DrawRadioBitmap(win, dc, rect, flags);
    }

    virtual void DrawTitleBarBitmap(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    wxTitleBarButton button,
                                    int flags = 0);

    virtual wxSplitterRenderParams GetSplitterParams(const wxWindow *win);

private:
    // wrapper around DrawThemeBackground() translating flags to NORMAL/HOT/
    // PUSHED/DISABLED states (and so suitable for drawing anything
    // button-like)
    void DoDrawButtonLike(HTHEME htheme,
                          int part,
                          wxDC& dc,
                          const wxRect& rect,
                          int flags);

    // common part of DrawCheckBox(), DrawPushButton() and DrawRadioBitmap()
    bool DoDrawXPButton(int kind,
                        wxWindow *win,
                        wxDC& dc,
                        const wxRect& rect,
                        int flags);

    wxDECLARE_NO_COPY_CLASS(wxRendererXP);
};

#endif // wxUSE_UXTHEME


// ============================================================================
// wxRendererMSWBase implementation
// ============================================================================

void wxRendererMSWBase::DrawFocusRect(wxWindow * WXUNUSED(win),
                                      wxDC& dc,
                                      const wxRect& rect,
                                      int WXUNUSED(flags))
{
    RECT rc;
    wxCopyRectToRECT(rect, rc);

    ::DrawFocusRect(GetHdcOf(dc.GetTempHDC()), &rc);
}

void wxRendererMSWBase::DrawItemSelectionRect(wxWindow *win,
                                              wxDC& dc,
                                              const wxRect& rect,
                                              int flags)
{
    wxBrush brush;
    if ( flags & wxCONTROL_SELECTED )
    {
        if ( flags & wxCONTROL_FOCUSED )
        {
            brush = wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
        }
        else // !focused
        {
            brush = wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
        }
    }
    else // !selected
    {
        brush = *wxTRANSPARENT_BRUSH;
    }

    dc.SetBrush(brush);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle( rect );

    if ((flags & wxCONTROL_FOCUSED) && (flags & wxCONTROL_CURRENT))
        DrawFocusRect( win, dc, rect, flags );
}

void wxRendererMSWBase::DrawChoice(wxWindow* win,
                                   wxDC& dc,
                                   const wxRect& rect,
                                   int flags)
{
    DrawComboBox(win, dc, rect, flags);
}

void wxRendererMSWBase::DrawComboBox(wxWindow* win,
                                     wxDC& dc,
                                     const wxRect& rect,
                                     int flags)
{
    // Draw the main part of the control same as TextCtrl
    DrawTextCtrl(win, dc, rect, flags);

    // Draw the button inside the border, on the right side
    wxRect br(rect);
    br.height -= 2;
    br.x += br.width - br.height - 1;
    br.width = br.height;
    br.y += 1;

    DrawComboBoxDropButton(win, dc, br, flags);
}

// ============================================================================
// wxRendererNative and wxRendererMSW implementation
// ============================================================================

/* static */
wxRendererNative& wxRendererNative::GetDefault()
{
#if wxUSE_UXTHEME
    wxUxThemeEngine *themeEngine = wxUxThemeEngine::Get();
    if ( themeEngine && themeEngine->IsAppThemed() )
        return wxRendererXP::Get();
#endif // wxUSE_UXTHEME

    return wxRendererMSW::Get();
}

/* static */
wxRendererNative& wxRendererMSW::Get()
{
    static wxRendererMSW s_rendererMSW;

    return s_rendererMSW;
}

void
wxRendererMSW::DrawComboBoxDropButton(wxWindow * WXUNUSED(win),
                                      wxDC& dc,
                                      const wxRect& rect,
                                      int flags)
{
    wxCHECK_RET( dc.GetImpl(), wxT("Invalid wxDC") );

    wxRect adjustedRect = dc.GetImpl()->MSWApplyGDIPlusTransform(rect);
    
    RECT r;
    wxCopyRectToRECT(adjustedRect, r);

    int style = DFCS_SCROLLCOMBOBOX;
    if ( flags & wxCONTROL_DISABLED )
        style |= DFCS_INACTIVE;
    if ( flags & wxCONTROL_PRESSED )
        style |= DFCS_PUSHED | DFCS_FLAT;

    ::DrawFrameControl(GetHdcOf(dc.GetTempHDC()), &r, DFC_SCROLL, style);
}

void
wxRendererMSW::DoDrawFrameControl(UINT type,
                                  UINT kind,
                                  wxWindow * WXUNUSED(win),
                                  wxDC& dc,
                                  const wxRect& rect,
                                  int flags)
{
    wxCHECK_RET( dc.GetImpl(), wxT("Invalid wxDC") );

    wxRect adjustedRect = dc.GetImpl()->MSWApplyGDIPlusTransform(rect);

    RECT r;
    wxCopyRectToRECT(adjustedRect, r);

    int style = kind;
    if ( flags & wxCONTROL_CHECKED )
        style |= DFCS_CHECKED;
    if ( flags & wxCONTROL_DISABLED )
        style |= DFCS_INACTIVE;
    if ( flags & wxCONTROL_FLAT )
        style |= DFCS_MONO;
    if ( flags & wxCONTROL_PRESSED )
        style |= DFCS_PUSHED;
    if ( flags & wxCONTROL_CURRENT )
        style |= DFCS_HOT;
    if ( flags & wxCONTROL_UNDETERMINED )
        // Using DFCS_BUTTON3STATE here doesn't work (as might be expected),
        // use the following two styles to get the same look of a check box
        // in the undetermined state.
        style |= DFCS_INACTIVE | DFCS_CHECKED;

    ::DrawFrameControl(GetHdcOf(dc.GetTempHDC()), &r, type, style);
}

void
wxRendererMSW::DrawPushButton(wxWindow *win,
                              wxDC& dc,
                              const wxRect& rectOrig,
                              int flags)
{
    wxRect rect(rectOrig);
    if ( flags & wxCONTROL_ISDEFAULT )
    {
        // DrawFrameControl() doesn't seem to support default buttons so we
        // have to draw the border ourselves
        wxDCPenChanger pen(dc, *wxBLACK_PEN);
        wxDCBrushChanger brush(dc, *wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(rect);
        rect.Deflate(1);
    }

    DoDrawButton(DFCS_BUTTONPUSH, win, dc, rect, flags);
}

void
wxRendererMSW::DrawTitleBarBitmap(wxWindow *win,
                                  wxDC& dc,
                                  const wxRect& rect,
                                  wxTitleBarButton button,
                                  int flags)
{
    UINT kind;
    switch ( button )
    {
        case wxTITLEBAR_BUTTON_CLOSE:
            kind = DFCS_CAPTIONCLOSE;
            break;

        case wxTITLEBAR_BUTTON_MAXIMIZE:
            kind = DFCS_CAPTIONMAX;
            break;

        case wxTITLEBAR_BUTTON_ICONIZE:
            kind = DFCS_CAPTIONMIN;
            break;

        case wxTITLEBAR_BUTTON_RESTORE:
            kind = DFCS_CAPTIONRESTORE;
            break;

        case wxTITLEBAR_BUTTON_HELP:
            kind = DFCS_CAPTIONHELP;
            break;

        default:
            wxFAIL_MSG( "unsupported title bar button" );
            return;
    }

    DoDrawFrameControl(DFC_CAPTION, kind, win, dc, rect, flags);
}

wxSize wxRendererMSW::GetCheckBoxSize(wxWindow * WXUNUSED(win))
{
    return wxSize(::GetSystemMetrics(SM_CXMENUCHECK),
                  ::GetSystemMetrics(SM_CYMENUCHECK));
}

int wxRendererMSW::GetHeaderButtonHeight(wxWindow * WXUNUSED(win))
{
    // some "reasonable" value returned in case of error, it doesn't really
    // correspond to anything but it's better than returning 0
    static const int DEFAULT_HEIGHT = 20;


    // create a temporary header window just to get its geometry
    HWND hwndHeader = ::CreateWindow(WC_HEADER, NULL, 0,
                                     0, 0, 0, 0, NULL, NULL, NULL, NULL);
    if ( !hwndHeader )
        return DEFAULT_HEIGHT;

    wxON_BLOCK_EXIT1( ::DestroyWindow, hwndHeader );

    // initialize the struct filled with the values by Header_Layout()
    RECT parentRect = { 0, 0, 100, 100 };
    WINDOWPOS wp = { 0, 0, 0, 0, 0, 0, 0 };
    HDLAYOUT hdl = { &parentRect, &wp };

    return Header_Layout(hwndHeader, &hdl) ? wp.cy : DEFAULT_HEIGHT;
}

int wxRendererMSW::GetHeaderButtonMargin(wxWindow *WXUNUSED(win))
{
    return 10;
}

// Uses the theme to draw the border and fill for something like a wxTextCtrl
void wxRendererMSW::DrawTextCtrl(wxWindow* WXUNUSED(win),
                                 wxDC& dc,
                                 const wxRect& rect,
                                 int WXUNUSED(flags))
{
    wxColour fill;
    wxColour bdr;
    {
        fill = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        bdr = *wxBLACK;
    }

    dc.SetPen(bdr);
    dc.SetBrush(fill);
    dc.DrawRectangle(rect);
}


// ============================================================================
// wxRendererXP implementation
// ============================================================================

#if wxUSE_UXTHEME

/* static */
wxRendererNative& wxRendererXP::Get()
{
    static wxRendererXP s_rendererXP;

    return s_rendererXP;
}

// NOTE: There is no guarantee that the button drawn fills the entire rect (XP
// default theme, for example), so the caller should have cleared button's
// background before this call. This is quite likely a wxMSW-specific thing.
void
wxRendererXP::DrawComboBoxDropButton(wxWindow * win,
                                      wxDC& dc,
                                      const wxRect& rect,
                                      int flags)
{
    wxUxThemeHandle hTheme(win, L"COMBOBOX");
    if ( !hTheme )
    {
        m_rendererNative.DrawComboBoxDropButton(win, dc, rect, flags);
        return;
    }

    wxCHECK_RET( dc.GetImpl(), wxT("Invalid wxDC") );

    wxRect adjustedRect = dc.GetImpl()->MSWApplyGDIPlusTransform(rect);

    RECT r;
    wxCopyRectToRECT(adjustedRect, r);

    int state;
    if ( flags & wxCONTROL_PRESSED )
        state = CBXS_PRESSED;
    else if ( flags & wxCONTROL_CURRENT )
        state = CBXS_HOT;
    else if ( flags & wxCONTROL_DISABLED )
        state = CBXS_DISABLED;
    else
        state = CBXS_NORMAL;

    wxUxThemeEngine::Get()->DrawThemeBackground
                            (
                                hTheme,
                                GetHdcOf(dc.GetTempHDC()),
                                CP_DROPDOWNBUTTON,
                                state,
                                &r,
                                NULL
                            );

}

int
wxRendererXP::DrawHeaderButton(wxWindow *win,
                               wxDC& dc,
                               const wxRect& rect,
                               int flags,
                               wxHeaderSortIconType sortArrow,
                               wxHeaderButtonParams* params)
{
    wxUxThemeHandle hTheme(win, L"HEADER");
    if ( !hTheme )
    {
        return m_rendererNative.DrawHeaderButton(win, dc, rect, flags, sortArrow, params);
    }

    wxCHECK_MSG( dc.GetImpl(), -1, wxT("Invalid wxDC") );

    wxRect adjustedRect = dc.GetImpl()->MSWApplyGDIPlusTransform(rect);

    RECT r;
    wxCopyRectToRECT(adjustedRect, r);

    int state;
    if ( flags & wxCONTROL_PRESSED )
        state = HIS_PRESSED;
    else if ( flags & wxCONTROL_CURRENT )
        state = HIS_HOT;
    else
        state = HIS_NORMAL;
    wxUxThemeEngine::Get()->DrawThemeBackground
                            (
                                hTheme,
                                GetHdcOf(dc.GetTempHDC()),
                                HP_HEADERITEM,
                                state,
                                &r,
                                NULL
                            );

    // NOTE: Using the theme to draw HP_HEADERSORTARROW doesn't do anything.
    // Why?  If this can be fixed then draw the sort arrows using the theme
    // and then clear those flags before calling DrawHeaderButtonContents.

    // Add any extras that are specified in flags and params
    return DrawHeaderButtonContents(win, dc, rect, flags, sortArrow, params);
}


void
wxRendererXP::DrawTreeItemButton(wxWindow *win,
                                 wxDC& dc,
                                 const wxRect& rect,
                                 int flags)
{
    wxUxThemeHandle hTheme(win, L"TREEVIEW");
    if ( !hTheme )
    {
        m_rendererNative.DrawTreeItemButton(win, dc, rect, flags);
        return;
    }

    wxCHECK_RET( dc.GetImpl(), wxT("Invalid wxDC") );

    wxRect adjustedRect = dc.GetImpl()->MSWApplyGDIPlusTransform(rect);

    RECT r;
    wxCopyRectToRECT(adjustedRect, r);

    int state = flags & wxCONTROL_EXPANDED ? GLPS_OPENED : GLPS_CLOSED;
    wxUxThemeEngine::Get()->DrawThemeBackground
                            (
                                hTheme,
                                GetHdcOf(dc.GetTempHDC()),
                                TVP_GLYPH,
                                state,
                                &r,
                                NULL
                            );
}

bool
wxRendererXP::DoDrawXPButton(int kind,
                             wxWindow *win,
                             wxDC& dc,
                             const wxRect& rect,
                             int flags)
{
    wxUxThemeHandle hTheme(win, L"BUTTON");
    if ( !hTheme )
        return false;

    DoDrawButtonLike(hTheme, kind, dc, rect, flags);

    return true;
}

void
wxRendererXP::DoDrawButtonLike(HTHEME htheme,
                               int part,
                               wxDC& dc,
                               const wxRect& rect,
                               int flags)
{
    wxCHECK_RET( dc.GetImpl(), wxT("Invalid wxDC") );

    wxRect adjustedRect = dc.GetImpl()->MSWApplyGDIPlusTransform(rect);

    RECT r;
    wxCopyRectToRECT(adjustedRect, r);

    // the base state is always 1, whether it is PBS_NORMAL,
    // {CBS,RBS}_UNCHECKEDNORMAL or CBS_NORMAL
    int state = 1;

    // XBS_XXX is followed by XBX_XXXHOT, then XBS_XXXPRESSED and DISABLED
    enum
    {
        NORMAL_OFFSET,
        HOT_OFFSET,
        PRESSED_OFFSET,
        DISABLED_OFFSET,
        STATES_COUNT
    };

    // in both RBS_ and CBS_ enums CHECKED elements are offset by 4 from base
    // (UNCHECKED) ones and MIXED are offset by 4 again as there are all states
    // from the above enum in between them
    if ( flags & wxCONTROL_CHECKED )
        state += STATES_COUNT;
    else if ( flags & wxCONTROL_UNDETERMINED )
        state += 2*STATES_COUNT;

    if ( flags & wxCONTROL_DISABLED )
        state += DISABLED_OFFSET;
    else if ( flags & wxCONTROL_PRESSED )
        state += PRESSED_OFFSET;
    else if ( flags & wxCONTROL_CURRENT )
        state += HOT_OFFSET;
    // wxCONTROL_ISDEFAULT flag is only valid for push buttons
    else if ( part == BP_PUSHBUTTON && (flags & wxCONTROL_ISDEFAULT) )
        state = PBS_DEFAULTED;

    wxUxThemeEngine::Get()->DrawThemeBackground
                            (
                                htheme,
                                GetHdcOf(dc.GetTempHDC()),
                                part,
                                state,
                                &r,
                                NULL
                            );
}

void
wxRendererXP::DrawTitleBarBitmap(wxWindow *win,
                                 wxDC& dc,
                                 const wxRect& rect,
                                 wxTitleBarButton button,
                                 int flags)
{
    wxUxThemeHandle hTheme(win, L"WINDOW");
    if ( !hTheme )
    {
        m_rendererNative.DrawTitleBarBitmap(win, dc, rect, button, flags);
        return;
    }

    int part;
    switch ( button )
    {
        case wxTITLEBAR_BUTTON_CLOSE:
            part = WP_CLOSEBUTTON;
            break;

        case wxTITLEBAR_BUTTON_MAXIMIZE:
            part = WP_MAXBUTTON;
            break;

        case wxTITLEBAR_BUTTON_ICONIZE:
            part = WP_MINBUTTON;
            break;

        case wxTITLEBAR_BUTTON_RESTORE:
            part = WP_RESTOREBUTTON;
            break;

        case wxTITLEBAR_BUTTON_HELP:
            part = WP_HELPBUTTON;
            break;

        default:
            wxFAIL_MSG( "unsupported title bar button" );
            return;
    }

    DoDrawButtonLike(hTheme, part, dc, rect, flags);
}

// Uses the theme to draw the border and fill for something like a wxTextCtrl
void wxRendererXP::DrawTextCtrl(wxWindow* win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags)
{
    wxUxThemeHandle hTheme(win, L"EDIT");
    if ( !hTheme )
    {
        m_rendererNative.DrawTextCtrl(win,dc,rect,flags);
        return;
    }

    wxColour fill;
    wxColour bdr;
    COLORREF cref;

    wxUxThemeEngine::Get()->GetThemeColor(hTheme, EP_EDITTEXT,
                                          ETS_NORMAL, TMT_FILLCOLOR, &cref);
    fill = wxRGBToColour(cref);

    int etsState;
    if ( flags & wxCONTROL_DISABLED )
        etsState = ETS_DISABLED;
    else
        etsState = ETS_NORMAL;

    wxUxThemeEngine::Get()->GetThemeColor(hTheme, EP_EDITTEXT,
                                              etsState, TMT_BORDERCOLOR, &cref);
    bdr = wxRGBToColour(cref);

    dc.SetPen( bdr );
    dc.SetBrush( fill );
    dc.DrawRectangle(rect);
}

// ----------------------------------------------------------------------------
// splitter drawing
// ----------------------------------------------------------------------------

// the width of the sash: this is the same as used by Explorer...
static const wxCoord SASH_WIDTH = 4;

wxSplitterRenderParams
wxRendererXP::GetSplitterParams(const wxWindow * win)
{
    if ( win->HasFlag(wxSP_NO_XP_THEME) )
        return m_rendererNative.GetSplitterParams(win);
    else
        return wxSplitterRenderParams(SASH_WIDTH, 0, false);
}

void
wxRendererXP::DrawSplitterBorder(wxWindow * win,
                                 wxDC& dc,
                                 const wxRect& rect,
                                 int flags)
{
    if ( win->HasFlag(wxSP_NO_XP_THEME) )
    {
        m_rendererNative.DrawSplitterBorder(win, dc, rect, flags);
    }
}

void
wxRendererXP::DrawSplitterSash(wxWindow *win,
                               wxDC& dc,
                               const wxSize& size,
                               wxCoord position,
                               wxOrientation orient,
                               int flags)
{
    if ( !win->HasFlag(wxSP_NO_XP_THEME) )
    {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE)));
        if ( orient == wxVERTICAL )
        {
            dc.DrawRectangle(position, 0, SASH_WIDTH, size.y);
        }
        else // wxHORIZONTAL
        {
            dc.DrawRectangle(0, position, size.x, SASH_WIDTH);
        }

        return;
    }

    m_rendererNative.DrawSplitterSash(win, dc, size, position, orient, flags);
}

#endif // wxUSE_UXTHEME
