/////////////////////////////////////////////////////////////////////////////
// Name:        wx/statline.h
// Purpose:     wxStaticLine class interface
// Author:      Vadim Zeitlin
// Created:     28.06.99
// Copyright:   (c) 1999 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STATLINE_H_BASE_
#define _WX_STATLINE_H_BASE_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// this defines wxUSE_STATLINE
#include "wx/defs.h"

#if wxUSE_STATLINE

// the base class declaration
#include "wx/control.h"

// ----------------------------------------------------------------------------
// global variables
// ----------------------------------------------------------------------------

// the default name for objects of class wxStaticLine
extern WXDLLIMPEXP_DATA_CORE(const char) wxStaticLineNameStr[];

// ----------------------------------------------------------------------------
// wxStaticLine - a line in a dialog
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStaticLineBase : public wxControl
{
public:
    // constructor
    wxStaticLineBase() { }

    // is the line vertical?
    bool IsVertical() const { return (GetWindowStyle() & wxLI_VERTICAL) != 0; }

    // get the default size for the "lesser" dimension of the static line
    static int GetDefaultSize() { return 2; }

    // overridden base class virtuals
    virtual bool AcceptsFocus() const { return false; }

protected:
    // choose the default border for this window
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }

    // set the right size for the right dimension
    wxSize AdjustSize(const wxSize& size) const
    {
        wxSize sizeReal(size);
        if ( IsVertical() )
        {
            if ( size.x == wxDefaultCoord )
                sizeReal.x = GetDefaultSize();
        }
        else
        {
            if ( size.y == wxDefaultCoord )
                sizeReal.y = GetDefaultSize();
        }

        return sizeReal;
    }

    virtual wxSize DoGetBestSize() const
    {
        return AdjustSize(wxDefaultSize);
    }

    wxDECLARE_NO_COPY_CLASS(wxStaticLineBase);
};

// ----------------------------------------------------------------------------
// now include the actual class declaration
// ----------------------------------------------------------------------------

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/statline.h"
#elif defined(__WXMSW__)
    #include "wx/msw/statline.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/statline.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/statline.h"
#elif defined(__WXPM__)
    #include "wx/os2/statline.h"
#elif defined(__WXMAC__)
    #include "wx/osx/statline.h"
#elif defined(__WXCOCOA__)
    #include "wx/cocoa/statline.h"
#else // use generic implementation for all other platforms
    #include "wx/generic/statline.h"
#endif

#endif // wxUSE_STATLINE

#endif // _WX_STATLINE_H_BASE_
