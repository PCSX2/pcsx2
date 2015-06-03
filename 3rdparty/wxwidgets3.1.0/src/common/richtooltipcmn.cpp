///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/richtooltipcmn.cpp
// Purpose:     wxRichToolTip implementation common to all platforms.
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
    #include "wx/icon.h"
#endif // WX_PRECOMP

#include "wx/private/richtooltip.h"

// ============================================================================
// implementation
// ============================================================================

wxRichToolTip::wxRichToolTip(const wxString& title,
                             const wxString& message) :
    m_impl(wxRichToolTipImpl::Create(title, message))
{
}

void
wxRichToolTip::SetBackgroundColour(const wxColour& col, const wxColour& colEnd)
{
    m_impl->SetBackgroundColour(col, colEnd);
}

void wxRichToolTip::SetIcon(int icon)
{
    m_impl->SetStandardIcon(icon);
}

void wxRichToolTip::SetIcon(const wxIcon& icon)
{
    m_impl->SetCustomIcon(icon);
}

void wxRichToolTip::SetTimeout(unsigned milliseconds,
                               unsigned millisecondsDelay)
{
    m_impl->SetTimeout(milliseconds, millisecondsDelay);
}

void wxRichToolTip::SetTipKind(wxTipKind tipKind)
{
    m_impl->SetTipKind(tipKind);
}

void wxRichToolTip::SetTitleFont(const wxFont& font)
{
    m_impl->SetTitleFont(font);
}

void wxRichToolTip::ShowFor(wxWindow* win, const wxRect* rect)
{
    wxCHECK_RET( win, wxS("Must have a valid window") );

    m_impl->ShowFor(win, rect);
}

wxRichToolTip::~wxRichToolTip()
{
    delete m_impl;
}

#endif // wxUSE_RICHTOOLTIP
