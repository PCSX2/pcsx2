///////////////////////////////////////////////////////////////////////////////
// Name:        wx/activityindicator.h
// Purpose:     wxActivityIndicator declaration.
// Author:      Vadim Zeitlin
// Created:     2015-03-05
// Copyright:   (c) 2015 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ACTIVITYINDICATOR_H_
#define _WX_ACTIVITYINDICATOR_H_

#include "wx/defs.h"

#if wxUSE_ACTIVITYINDICATOR

#include "wx/control.h"

#define wxActivityIndicatorNameStr wxS("activityindicator")

// ----------------------------------------------------------------------------
// wxActivityIndicator: small animated indicator of some application activity.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxActivityIndicatorBase : public wxControl
{
public:
    // Start or stop the activity animation (it is stopped initially).
    virtual void Start() = 0;
    virtual void Stop() = 0;

    // Return true if the control is currently showing activity.
    virtual bool IsRunning() const = 0;

    // Override some base class virtual methods.
    virtual bool AcceptsFocus() const wxOVERRIDE { return false; }
    virtual bool HasTransparentBackground() wxOVERRIDE { return true; }

protected:
    // choose the default border for this window
    virtual wxBorder GetDefaultBorder() const wxOVERRIDE { return wxBORDER_NONE; }
};

#ifndef __WXUNIVERSAL__
#if defined(__WXGTK220__)
    #define wxHAS_NATIVE_ACTIVITYINDICATOR
    #include "wx/gtk/activityindicator.h"
#elif defined(__WXOSX_COCOA__)
    #define wxHAS_NATIVE_ACTIVITYINDICATOR
    #include "wx/osx/activityindicator.h"
#endif
#endif // !__WXUNIVERSAL__

#ifndef wxHAS_NATIVE_ACTIVITYINDICATOR
    #include "wx/generic/activityindicator.h"
#endif

#endif // wxUSE_ACTIVITYINDICATOR

#endif // _WX_ACTIVITYINDICATOR_H_
