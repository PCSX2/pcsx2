///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/richtooltip.h
// Purpose:     wxRichToolTipImpl declaration.
// Author:      Vadim Zeitlin
// Created:     2011-10-18
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_RICHTOOLTIP_H_
#define _WX_PRIVATE_RICHTOOLTIP_H_

#include "wx/richtooltip.h"

// ----------------------------------------------------------------------------
// wxRichToolTipImpl: defines wxRichToolTip implementation.
// ----------------------------------------------------------------------------

class wxRichToolTipImpl
{
public:
    // This is implemented in a platform-specific way.
    static wxRichToolTipImpl* Create(const wxString& title,
                                     const wxString& message);

    // These methods simply mirror the public wxRichToolTip ones.
    virtual void SetBackgroundColour(const wxColour& col,
                                     const wxColour& colEnd) = 0;
    virtual void SetCustomIcon(const wxIcon& icon) = 0;
    virtual void SetStandardIcon(int icon) = 0;
    virtual void SetTimeout(unsigned milliseconds,
                            unsigned millisecondsShowdelay = 0) = 0;
    virtual void SetTipKind(wxTipKind tipKind) = 0;
    virtual void SetTitleFont(const wxFont& font) = 0;

    virtual void ShowFor(wxWindow* win, const wxRect* rect = NULL) = 0;

    virtual ~wxRichToolTipImpl() { }

protected:
    wxRichToolTipImpl() { }
};

#endif // _WX_PRIVATE_RICHTOOLTIP_H_
