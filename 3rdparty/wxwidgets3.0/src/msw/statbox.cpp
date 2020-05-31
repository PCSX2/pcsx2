/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/statbox.cpp
// Purpose:     wxStaticBox
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

#if wxUSE_STATBOX

#include "wx/statbox.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
    #include "wx/image.h"
    #include "wx/sizer.h"
#endif

#include "wx/notebook.h"
#include "wx/sysopt.h"

#include "wx/msw/uxtheme.h"
#include "wx/msw/private.h"
#include "wx/msw/missing.h"
#include "wx/msw/dc.h"

// the values coincide with those in tmschema.h
#define BP_GROUPBOX 4

#define GBS_NORMAL 1

#define TMT_FONT 210

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxStaticBox
// ----------------------------------------------------------------------------

bool wxStaticBox::Create(wxWindow *parent,
                         wxWindowID id,
                         const wxString& label,
                         const wxPoint& pos,
                         const wxSize& size,
                         long style,
                         const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;

    if ( !MSWCreateControl(wxT("BUTTON"), label, pos, size) )
        return false;

#ifndef __WXWINCE__
    if (!wxSystemOptions::IsFalse(wxT("msw.staticbox.optimized-paint")))
    {
        Connect(wxEVT_PAINT, wxPaintEventHandler(wxStaticBox::OnPaint));

        // Our OnPaint() completely erases our background, so don't do it in
        // WM_ERASEBKGND too to avoid flicker.
        SetBackgroundStyle(wxBG_STYLE_PAINT);
    }
#endif // !__WXWINCE__

    return true;
}

WXDWORD wxStaticBox::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    long styleWin = wxStaticBoxBase::MSWGetStyle(style, exstyle);

    // no need for it anymore, must be removed for wxRadioBox child
    // buttons to be able to repaint themselves
    styleWin &= ~WS_CLIPCHILDREN;

    if ( exstyle )
    {
#ifndef __WXWINCE__
        // We may have children inside this static box, so use this style for
        // TAB navigation to work if we ever use IsDialogMessage() to implement
        // it (currently we don't because it's too buggy and implement TAB
        // navigation ourselves, but this could change in the future).
        *exstyle |= WS_EX_CONTROLPARENT;

        if (wxSystemOptions::IsFalse(wxT("msw.staticbox.optimized-paint")))
            *exstyle |= WS_EX_TRANSPARENT;
#endif
    }

    styleWin |= BS_GROUPBOX;

    return styleWin;
}

wxSize wxStaticBox::DoGetBestSize() const
{
    wxSize best;

    // Calculate the size needed by the label
    int cx, cy;
    wxGetCharSize(GetHWND(), &cx, &cy, GetFont());

    int wBox;
    GetTextExtent(GetLabelText(wxGetWindowText(m_hWnd)), &wBox, &cy);

    wBox += 3*cx;
    int hBox = EDIT_HEIGHT_FROM_CHAR_HEIGHT(cy);

    // If there is a sizer then the base best size is the sizer's minimum
    if (GetSizer() != NULL)
    {
        wxSize cm(GetSizer()->CalcMin());
        best = ClientToWindowSize(cm);
        // adjust for a long label if needed
        best.x = wxMax(best.x, wBox);
    }
    // otherwise the best size falls back to the label size
    else
    {
        best = wxSize(wBox, hBox);
    }
    return best;
}

void wxStaticBox::GetBordersForSizer(int *borderTop, int *borderOther) const
{
    wxStaticBoxBase::GetBordersForSizer(borderTop, borderOther);

    // need extra space, don't know how much but this seems to be enough
    *borderTop += GetCharHeight()/3;
}

// all the hacks below are not necessary for WinCE
#ifndef __WXWINCE__

WXLRESULT wxStaticBox::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    if ( nMsg == WM_NCHITTEST )
    {
        // This code breaks some other processing such as enter/leave tracking
        // so it's off by default.

        static int s_useHTClient = -1;
        if (s_useHTClient == -1)
            s_useHTClient = wxSystemOptions::GetOptionInt(wxT("msw.staticbox.htclient"));
        if (s_useHTClient == 1)
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            ScreenToClient(&xPos, &yPos);

            // Make sure you can drag by the top of the groupbox, but let
            // other (enclosed) controls get mouse events also
            if ( yPos < 10 )
                return (long)HTCLIENT;
        }
    }

    if ( nMsg == WM_PRINTCLIENT )
    {
        // we have to process WM_PRINTCLIENT ourselves as otherwise child
        // windows' background (eg buttons in radio box) would never be drawn
        // unless we have a parent with non default background

        // so check first if we have one
        if ( !HandlePrintClient((WXHDC)wParam) )
        {
            // no, we don't, erase the background ourselves
            // (don't use our own) - see PaintBackground for explanation
            wxBrush brush(GetParent()->GetBackgroundColour());
            wxFillRect(GetHwnd(), (HDC)wParam, GetHbrushOf(brush));
        }

        return 0;
    }

    if ( nMsg == WM_UPDATEUISTATE )
    {
        // DefWindowProc() redraws just the static box text when it gets this
        // message and it does it using the standard (blue in standard theme)
        // colour and not our own label colour that we use in PaintForeground()
        // resulting in the label mysteriously changing the colour when e.g.
        // "Alt" is pressed anywhere in the window, see #12497.
        //
        // To avoid this we simply refresh the window forcing our own code
        // redrawing the label in the correct colour to be called. This is
        // inefficient but there doesn't seem to be anything else we can do.
        //
        // Notice that the problem is XP-specific and doesn't arise under later
        // systems.
        if ( m_hasFgCol && wxGetWinVersion() == wxWinVersion_XP )
            Refresh();
    }

    return wxControl::MSWWindowProc(nMsg, wParam, lParam);
}

// ----------------------------------------------------------------------------
// static box drawing
// ----------------------------------------------------------------------------

/*
   We draw the static box ourselves because it's the only way to prevent it
   from flickering horribly on resize (because everything inside the box is
   erased twice: once when the box itself is repainted and second time when
   the control inside it is repainted) without using WS_EX_TRANSPARENT style as
   we used to do and which resulted in other problems.
 */

// MSWGetRegionWithoutSelf helper: removes the given rectangle from region
static inline void
SubtractRectFromRgn(HRGN hrgn, int left, int top, int right, int bottom)
{
    AutoHRGN hrgnRect(::CreateRectRgn(left, top, right, bottom));
    if ( !hrgnRect )
    {
        wxLogLastError(wxT("CreateRectRgn()"));
        return;
    }

    ::CombineRgn(hrgn, hrgn, hrgnRect, RGN_DIFF);
}

void wxStaticBox::MSWGetRegionWithoutSelf(WXHRGN hRgn, int w, int h)
{
    HRGN hrgn = (HRGN)hRgn;

    // remove the area occupied by the static box borders from the region
    int borderTop, border;
    GetBordersForSizer(&borderTop, &border);

    // top
    SubtractRectFromRgn(hrgn, 0, 0, w, borderTop);

    // bottom
    SubtractRectFromRgn(hrgn, 0, h - border, w, h);

    // left
    SubtractRectFromRgn(hrgn, 0, 0, border, h);

    // right
    SubtractRectFromRgn(hrgn, w - border, 0, w, h);
}

namespace {
RECT AdjustRectForRtl(wxLayoutDirection dir, RECT const& childRect, RECT const& boxRect) {
    RECT ret = childRect;
    if( dir == wxLayout_RightToLeft ) {
        // The clipping region too is mirrored in RTL layout.
        // We need to mirror screen coordinates relative to static box window priot to
        // intersecting with region.
        ret.right = boxRect.right - childRect.left - boxRect.left;
        ret.left = boxRect.right - childRect.right - boxRect.left;
    }

    return ret;
}
}

WXHRGN wxStaticBox::MSWGetRegionWithoutChildren()
{
    RECT boxRc;
    ::GetWindowRect(GetHwnd(), &boxRc);
    HRGN hrgn = ::CreateRectRgn(boxRc.left, boxRc.top, boxRc.right + 1, boxRc.bottom + 1);
    bool foundThis = false;

    // Iterate over all sibling windows as in the old wxWidgets API the
    // controls appearing inside the static box were created as its siblings
    // and not children. This is now deprecated but should still work.
    //
    // Also notice that we must iterate over all windows, not just all
    // wxWindows, as there may be composite windows etc.
    HWND child;
    for ( child = ::GetWindow(GetHwndOf(GetParent()), GW_CHILD);
          child;
          child = ::GetWindow(child, GW_HWNDNEXT) )
    {
        if ( ! ::IsWindowVisible(child) )
        {
            // if the window isn't visible then it doesn't need clipped
            continue;
        }

        LONG style = ::GetWindowLong(child, GWL_STYLE);
        wxString str(wxGetWindowClass(child));
        str.UpperCase();
        if ( str == wxT("BUTTON") && (style & BS_GROUPBOX) == BS_GROUPBOX )
        {
            if ( child == GetHwnd() )
                foundThis = true;

            // Any static boxes below this one in the Z-order can't be clipped
            // since if we have the case where a static box with a low Z-order
            // is nested inside another static box with a high Z-order then the
            // nested static box would be painted over. Doing it this way
            // unfortunately results in flicker if the Z-order of nested static
            // boxes is not inside (lowest) to outside (highest) but at least
            // they are still shown.
            if ( foundThis )
                continue;
        }

        RECT rc;
        ::GetWindowRect(child, &rc);
        rc = AdjustRectForRtl(GetLayoutDirection(), rc, boxRc );
        if ( ::RectInRegion(hrgn, &rc) )
        {
            // need to remove WS_CLIPSIBLINGS from all sibling windows
            // that are within this staticbox if set
            if ( style & WS_CLIPSIBLINGS )
            {
                style &= ~WS_CLIPSIBLINGS;
                ::SetWindowLong(child, GWL_STYLE, style);

                // MSDN: "If you have changed certain window data using
                // SetWindowLong, you must call SetWindowPos to have the
                // changes take effect."
                ::SetWindowPos(child, NULL, 0, 0, 0, 0,
                               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                               SWP_FRAMECHANGED);
            }

            AutoHRGN hrgnChild(::CreateRectRgnIndirect(&rc));
            ::CombineRgn(hrgn, hrgn, hrgnChild, RGN_DIFF);
        }
    }

    // Also iterate over all children of the static box, we need to clip them
    // out as well.
    for ( child = ::GetWindow(GetHwnd(), GW_CHILD);
          child;
          child = ::GetWindow(child, GW_HWNDNEXT) )
    {
        if ( !::IsWindowVisible(child) )
        {
            // if the window isn't visible then it doesn't need clipped
            continue;
        }

        RECT rc;
        ::GetWindowRect(child, &rc);
        rc = AdjustRectForRtl(GetLayoutDirection(), rc, boxRc );
        AutoHRGN hrgnChild(::CreateRectRgnIndirect(&rc));
        ::CombineRgn(hrgn, hrgn, hrgnChild, RGN_DIFF);
    }

    return (WXHRGN)hrgn;
}

// helper for OnPaint(): really erase the background, i.e. do it even if we
// don't have any non default brush for doing it (DoEraseBackground() doesn't
// do anything in such case)
void wxStaticBox::PaintBackground(wxDC& dc, const RECT& rc)
{
    // note that we do not use the box background colour here, it shouldn't
    // apply to its interior for several reasons:
    //  1. wxGTK doesn't do it
    //  2. controls inside the box don't get correct bg colour because they
    //     are not our children so we'd have some really ugly colour mix if
    //     we did it
    //  3. this is backwards compatible behaviour and some people rely on it,
    //     see http://groups.google.com/groups?selm=4252E932.3080801%40able.es
    wxMSWDCImpl *impl = (wxMSWDCImpl*) dc.GetImpl();
    HBRUSH hbr = MSWGetBgBrush(impl->GetHDC());

    // if there is no special brush for painting this control, just use the
    // solid background colour
    wxBrush brush;
    if ( !hbr )
    {
        brush = wxBrush(GetParent()->GetBackgroundColour());
        hbr = GetHbrushOf(brush);
    }

    ::FillRect(GetHdcOf(*impl), &rc, hbr);
}

void wxStaticBox::PaintForeground(wxDC& dc, const RECT&)
{
    wxMSWDCImpl *impl = (wxMSWDCImpl*) dc.GetImpl();
    MSWDefWindowProc(WM_PAINT, (WPARAM)GetHdcOf(*impl), 0);

#if wxUSE_UXTHEME
    // when using XP themes, neither setting the text colour nor transparent
    // background mode doesn't change anything: the static box def window proc
    // still draws the label in its own colours, so we need to redraw the text
    // ourselves if we have a non default fg colour
    if ( m_hasFgCol && wxUxThemeEngine::GetIfActive() )
    {
        // draw over the text in default colour in our colour
        HDC hdc = GetHdcOf(*impl);
        ::SetTextColor(hdc, GetForegroundColour().GetPixel());

        // Get dimensions of the label
        const wxString label = GetLabel();

        // choose the correct font
        AutoHFONT font;
        SelectInHDC selFont;
        if ( m_hasFont )
        {
            selFont.Init(hdc, GetHfontOf(GetFont()));
        }
        else // no font set, use the one set by the theme
        {
            wxUxThemeHandle hTheme(this, L"BUTTON");
            if ( hTheme )
            {
                wxUxThemeFont themeFont;
                if ( wxUxThemeEngine::Get()->GetThemeFont
                                             (
                                                hTheme,
                                                hdc,
                                                BP_GROUPBOX,
                                                GBS_NORMAL,
                                                TMT_FONT,
                                                themeFont.GetPtr()
                                             ) == S_OK )
                {
                    font.Init(themeFont.GetLOGFONT());
                    if ( font )
                        selFont.Init(hdc, font);
                }
            }
        }

        // Get the font extent
        int width, height;
        dc.GetTextExtent(wxStripMenuCodes(label, wxStrip_Mnemonics),
                         &width, &height);

        int x;
        int y = height;

        // first we need to correctly paint the background of the label
        // as Windows ignores the brush offset when doing it
        //
        // FIXME: value of x is hardcoded as this is what it is on my system,
        //        no idea if it's true everywhere
        RECT dimensions = {0, 0, 0, y};
        x = 9;
        dimensions.left = x;
        dimensions.right = x + width;

        // need to adjust the rectangle to cover all the label background
        dimensions.left -= 2;
        dimensions.right += 2;
        dimensions.bottom += 2;

        if ( UseBgCol() )
        {
            // our own background colour should be used for the background of
            // the label: this is consistent with the behaviour under pre-XP
            // systems (i.e. without visual themes) and generally makes sense
            wxBrush brush = wxBrush(GetBackgroundColour());
            ::FillRect(hdc, &dimensions, GetHbrushOf(brush));
        }
        else // paint parent background
        {
            PaintBackground(dc, dimensions);
        }

        UINT drawTextFlags = DT_SINGLELINE | DT_VCENTER;

        // determine the state of UI queues to draw the text correctly under XP
        // and later systems
        static const bool isXPorLater = wxGetWinVersion() >= wxWinVersion_XP;
        if ( isXPorLater )
        {
            if ( ::SendMessage(GetHwnd(), WM_QUERYUISTATE, 0, 0) &
                    UISF_HIDEACCEL )
            {
                drawTextFlags |= DT_HIDEPREFIX;
            }
        }

        // now draw the text
        RECT rc2 = { x, 0, x + width, y };
        ::DrawText(hdc, label.t_str(), label.length(), &rc2,
                   drawTextFlags);
    }
#endif // wxUSE_UXTHEME
}

void wxStaticBox::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    RECT rc;
    ::GetClientRect(GetHwnd(), &rc);
    wxPaintDC dc(this);

    // draw the entire box in a memory DC
    wxMemoryDC memdc(&dc);
    wxBitmap bitmap(rc.right, rc.bottom);
    memdc.SelectObject(bitmap);

    PaintBackground(memdc, rc);
    PaintForeground(memdc, rc);

    // now only blit the static box border itself, not the interior, to avoid
    // flicker when background is drawn below
    //
    // note that it seems to be faster to do 4 small blits here and then paint
    // directly into wxPaintDC than painting background in wxMemoryDC and then
    // blitting everything at once to wxPaintDC, this is why we do it like this
    int borderTop, border;
    GetBordersForSizer(&borderTop, &border);

    // top
    dc.Blit(border, 0, rc.right - border, borderTop,
            &memdc, border, 0);
    // bottom
    dc.Blit(border, rc.bottom - border, rc.right - border, border,
            &memdc, border, rc.bottom - border);
    // left
    dc.Blit(0, 0, border, rc.bottom,
            &memdc, 0, 0);
    // right (note that upper and bottom right corners were already part of the
    // first two blits so we shouldn't overwrite them here to avoi flicker)
    dc.Blit(rc.right - border, borderTop,
            border, rc.bottom - borderTop - border,
            &memdc, rc.right - border, borderTop);


    // create the region excluding box children
    AutoHRGN hrgn((HRGN)MSWGetRegionWithoutChildren());
    RECT rcWin;
    ::GetWindowRect(GetHwnd(), &rcWin);
    ::OffsetRgn(hrgn, -rcWin.left, -rcWin.top);

    // and also the box itself
    MSWGetRegionWithoutSelf((WXHRGN) hrgn, rc.right, rc.bottom);
    wxMSWDCImpl *impl = (wxMSWDCImpl*) dc.GetImpl();
    HDCClipper clipToBg(GetHdcOf(*impl), hrgn);

    // paint the inside of the box (excluding box itself and child controls)
    PaintBackground(dc, rc);
}

#endif // !__WXWINCE__

#endif // wxUSE_STATBOX
