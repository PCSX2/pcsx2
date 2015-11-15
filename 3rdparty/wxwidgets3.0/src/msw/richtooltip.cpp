///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/richtooltip.cpp
// Purpose:     Native MSW implementation of wxRichToolTip.
// Author:      Vadim Zeitlin
// Created:     2011-10-18
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
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

#if wxUSE_RICHTOOLTIP

#ifndef WX_PRECOMP
    #include "wx/treectrl.h"
#endif // WX_PRECOMP

#include "wx/private/richtooltip.h"
#include "wx/generic/private/richtooltip.h"
#include "wx/msw/private.h"
#include "wx/msw/uxtheme.h"

// Provide definitions missing from some compilers SDK headers.

#ifndef TTI_NONE
enum
{
    TTI_NONE,
    TTI_INFO,
    TTI_WARNING,
    TTI_ERROR
};
#endif // !defined(TTI_XXX)

#ifndef Edit_ShowBalloonTip
struct EDITBALLOONTIP
{
    DWORD cbStruct;
    LPCWSTR pszTitle;
    LPCWSTR pszText;
    int ttiIcon;
};

#define Edit_ShowBalloonTip(hwnd, pebt) \
    (BOOL)::SendMessage((hwnd), 0x1503 /* EM_SHOWBALLOONTIP */, 0, (LPARAM)(pebt))

#endif // !defined(Edit_ShowBalloonTip)

// ============================================================================
// wxRichToolTipMSWImpl: the real implementation.
// ============================================================================

class wxRichToolTipMSWImpl : public wxRichToolTipGenericImpl
{
public:
    wxRichToolTipMSWImpl(const wxString& title, const wxString& message) :
        wxRichToolTipGenericImpl(title, message)
    {
        // So far so good...
        m_canUseNative = true;

        m_ttiIcon = TTI_NONE;
    }

    virtual void SetBackgroundColour(const wxColour& col,
                                     const wxColour& colEnd)
    {
        // Setting background colour is not supported neither.
        m_canUseNative = false;

        wxRichToolTipGenericImpl::SetBackgroundColour(col, colEnd);
    }

    virtual void SetCustomIcon(const wxIcon& icon)
    {
        // Custom icons are not supported by EM_SHOWBALLOONTIP.
        m_canUseNative = false;

        wxRichToolTipGenericImpl::SetCustomIcon(icon);
    }

    virtual void SetStandardIcon(int icon)
    {
        wxRichToolTipGenericImpl::SetStandardIcon(icon);
        if ( !m_canUseNative )
            return;

        switch ( icon & wxICON_MASK )
        {
            case wxICON_WARNING:
                m_ttiIcon = TTI_WARNING;
                break;

            case wxICON_ERROR:
                m_ttiIcon = TTI_ERROR;
                break;

            case wxICON_INFORMATION:
                m_ttiIcon = TTI_INFO;
                break;

            case wxICON_QUESTION:
                wxFAIL_MSG("Question icon doesn't make sense for a tooltip");
                break;

            case wxICON_NONE:
                m_ttiIcon = TTI_NONE;
                break;
        }
    }

    virtual void SetTimeout(unsigned millisecondsTimeout,
                            unsigned millisecondsDelay)
    {
        // We don't support changing the timeout or the delay
        // (maybe TTM_SETDELAYTIME could be used for this?).
        m_canUseNative = false;

        wxRichToolTipGenericImpl::SetTimeout(millisecondsTimeout,
                                             millisecondsDelay);
    }

    virtual void SetTipKind(wxTipKind tipKind)
    {
        // Setting non-default tip is not supported.
        if ( tipKind != wxTipKind_Auto )
            m_canUseNative = false;

        wxRichToolTipGenericImpl::SetTipKind(tipKind);
    }

    virtual void SetTitleFont(const wxFont& font)
    {
        // Setting non-default font is not supported.
        m_canUseNative = false;

        wxRichToolTipGenericImpl::SetTitleFont(font);
    }

    virtual void ShowFor(wxWindow* win, const wxRect* rect)
    {
        // TODO: We could use native tooltip control to show native balloon
        //       tooltips for any window but right now we use the simple
        //       EM_SHOWBALLOONTIP API which can only be used with text
        //       controls.
        if ( m_canUseNative && !rect )
        {
            wxTextCtrl* const text = wxDynamicCast(win, wxTextCtrl);
            if ( text )
            {
                EDITBALLOONTIP ebt;
                ebt.cbStruct = sizeof(EDITBALLOONTIP);
                ebt.pszTitle = m_title.wc_str();
                ebt.pszText = m_message.wc_str();
                ebt.ttiIcon = m_ttiIcon;
                if ( Edit_ShowBalloonTip(GetHwndOf(text), &ebt) )
                    return;
            }
        }

        // Don't set m_canUseNative to false here, we could be able to use the
        // native tooltips if we're called for a different window the next
        // time.
        wxRichToolTipGenericImpl::ShowFor(win, rect);
    }

private:
    // If this is false, we've been requested to do something that the native
    // version doesn't support and so need to fall back to the generic one.
    bool m_canUseNative;

    // One of TTI_NONE, TTI_INFO, TTI_WARNING or TTI_ERROR.
    int m_ttiIcon;
};

/* static */
wxRichToolTipImpl*
wxRichToolTipImpl::Create(const wxString& title, const wxString& message)
{
    // EM_SHOWBALLOONTIP is only implemented by comctl32.dll v6 so don't even
    // bother using the native implementation if we're not using themes.
    if ( wxUxThemeEngine::GetIfActive() )
        return new wxRichToolTipMSWImpl(title, message);

    return new wxRichToolTipGenericImpl(title, message);
}

#endif // wxUSE_RICHTOOLTIP
