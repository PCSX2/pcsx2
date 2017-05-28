///////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtooltip.h
// Purpose:     Declaration of wxRichToolTip class.
// Author:      Vadim Zeitlin
// Created:     2011-10-07
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RICHTOOLTIP_H_
#define _WX_RICHTOOLTIP_H_

#include "wx/defs.h"

#if wxUSE_RICHTOOLTIP

#include "wx/colour.h"

class WXDLLIMPEXP_FWD_CORE wxFont;
class WXDLLIMPEXP_FWD_CORE wxIcon;
class WXDLLIMPEXP_FWD_CORE wxWindow;

class wxRichToolTipImpl;

// This enum describes the kind of the tip shown which combines both the tip
// position and appearance because the two are related (when the tip is
// positioned asymmetrically, a right handed triangle is used but an
// equilateral one when it's in the middle of a side).
//
// Automatic selects the tip appearance best suited for the current platform
// and the position best suited for the window the tooltip is shown for, i.e.
// chosen in such a way that the tooltip is always fully on screen.
//
// Other values describe the position of the tooltip itself, not the window it
// relates to. E.g. wxTipKind_Top places the tip on the top of the tooltip and
// so the tooltip itself is located beneath its associated window.
enum wxTipKind
{
    wxTipKind_None,
    wxTipKind_TopLeft,
    wxTipKind_Top,
    wxTipKind_TopRight,
    wxTipKind_BottomLeft,
    wxTipKind_Bottom,
    wxTipKind_BottomRight,
    wxTipKind_Auto
};

// ----------------------------------------------------------------------------
// wxRichToolTip: a customizable but not necessarily native tooltip.
// ----------------------------------------------------------------------------

// Notice that this class does not inherit from wxWindow.
class WXDLLIMPEXP_ADV wxRichToolTip
{
public:
    // Ctor must specify the tooltip title and main message, additional
    // attributes can be set later.
    wxRichToolTip(const wxString& title, const wxString& message);

    // Set the background colour: if two colours are specified, the background
    // is drawn using a gradient from top to bottom, otherwise a single solid
    // colour is used.
    void SetBackgroundColour(const wxColour& col,
                             const wxColour& colEnd = wxColour());

    // Set the small icon to show: either one of the standard information/
    // warning/error ones (the question icon doesn't make sense for a tooltip)
    // or a custom icon.
    void SetIcon(int icon = wxICON_INFORMATION);
    void SetIcon(const wxIcon& icon);

    // Set timeout after which the tooltip should disappear, in milliseconds.
    // By default the tooltip is hidden after system-dependent interval of time
    // elapses but this method can be used to change this or also disable
    // hiding the tooltip automatically entirely by passing 0 in this parameter
    // (but doing this can result in native version not being used).
    // Optionally specify a show delay.
    void SetTimeout(unsigned milliseconds, unsigned millisecondsShowdelay = 0);

    // Choose the tip kind, possibly none. By default the tip is positioned
    // automatically, as if wxTipKind_Auto was used.
    void SetTipKind(wxTipKind tipKind);

    // Set the title text font. By default it's emphasized using the font style
    // or colour appropriate for the current platform.
    void SetTitleFont(const wxFont& font);

    // Show the tooltip for the given window and optionally a specified area.
    void ShowFor(wxWindow* win, const wxRect* rect = NULL);

    // Non-virtual dtor as this class is not supposed to be derived from.
    ~wxRichToolTip();

private:
    wxRichToolTipImpl* const m_impl;

    wxDECLARE_NO_COPY_CLASS(wxRichToolTip);
};

#endif // wxUSE_RICHTOOLTIP

#endif // _WX_RICHTOOLTIP_H_
