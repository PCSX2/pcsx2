///////////////////////////////////////////////////////////////////////////////
// Name:        wx/custombgwin.h
// Purpose:     Class adding support for custom window backgrounds.
// Author:      Vadim Zeitlin
// Created:     2011-10-10
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CUSTOMBGWIN_H_
#define _WX_CUSTOMBGWIN_H_

// ----------------------------------------------------------------------------
// wxCustomBackgroundWindow: Adds support for custom backgrounds to any
//                           wxWindow-derived class.
// ----------------------------------------------------------------------------

class wxCustomBackgroundWindowBase
{
public:
    // Trivial default ctor.
    wxCustomBackgroundWindowBase() { }

    // Also a trivial but virtual -- to suppress g++ warnings -- dtor.
    virtual ~wxCustomBackgroundWindowBase() { }

    // Use the given bitmap to tile the background of this window. This bitmap
    // will show through any transparent children.
    //
    // Notice that you must not prevent the base class EVT_ERASE_BACKGROUND
    // handler from running (i.e. not to handle this event yourself) for this
    // to work.
    void SetBackgroundBitmap(const wxBitmap& bmp)
    {
        DoSetBackgroundBitmap(bmp);
    }

protected:
    virtual void DoSetBackgroundBitmap(const wxBitmap& bmp) = 0;

    wxDECLARE_NO_COPY_CLASS(wxCustomBackgroundWindowBase);
};

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/custombgwin.h"
#elif defined(__WXMSW__)
    #include "wx/msw/custombgwin.h"
#else
    #include "wx/generic/custombgwin.h"
#endif

#endif // _WX_CUSTOMBGWIN_H_
