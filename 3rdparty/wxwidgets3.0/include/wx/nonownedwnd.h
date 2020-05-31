///////////////////////////////////////////////////////////////////////////////
// Name:        wx/nonownedwnd.h
// Purpose:     declares wxNonTopLevelWindow class
// Author:      Vaclav Slavik
// Modified by:
// Created:     2006-12-24
// Copyright:   (c) 2006 TT-Solutions
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_NONOWNEDWND_H_
#define _WX_NONOWNEDWND_H_

#include "wx/window.h"

// Styles that can be used with any wxNonOwnedWindow:
#define wxFRAME_SHAPED          0x0010  // Create a window that is able to be shaped

class WXDLLIMPEXP_FWD_CORE wxGraphicsPath;

// ----------------------------------------------------------------------------
// wxNonOwnedWindow: a window that is not a child window of another one.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxNonOwnedWindowBase : public wxWindow
{
public:
    // Set the shape of the window to the given region.
    // Returns true if the platform supports this feature (and the
    // operation is successful.)
    bool SetShape(const wxRegion& region)
    {
        // This style is in fact only needed by wxOSX/Carbon so once we don't
        // use this port any more, we could get rid of this requirement, but
        // for now you must specify wxFRAME_SHAPED for SetShape() to work on
        // all platforms.
        wxCHECK_MSG
        (
            HasFlag(wxFRAME_SHAPED), false,
            wxS("Shaped windows must be created with the wxFRAME_SHAPED style.")
        );

        return region.IsEmpty() ? DoClearShape() : DoSetRegionShape(region);
    }

#if wxUSE_GRAPHICS_CONTEXT
    // Set the shape using the specified path.
    bool SetShape(const wxGraphicsPath& path)
    {
        wxCHECK_MSG
        (
            HasFlag(wxFRAME_SHAPED), false,
            wxS("Shaped windows must be created with the wxFRAME_SHAPED style.")
        );

        return DoSetPathShape(path);
    }
#endif // wxUSE_GRAPHICS_CONTEXT


    // Overridden base class methods.
    // ------------------------------

    virtual void AdjustForParentClientOrigin(int& WXUNUSED(x), int& WXUNUSED(y),
                                             int WXUNUSED(sizeFlags) = 0) const
    {
        // Non owned windows positions don't need to be adjusted for parent
        // client area origin so simply do nothing here.
    }

    virtual void InheritAttributes()
    {
        // Non owned windows don't inherit attributes from their parent window
        // (if the parent frame is red, it doesn't mean that all dialogs shown
        // by it should be red as well), so don't do anything here neither.
    }

protected:
    virtual bool DoClearShape()
    {
        return false;
    }

    virtual bool DoSetRegionShape(const wxRegion& WXUNUSED(region))
    {
        return false;
    }

#if wxUSE_GRAPHICS_CONTEXT
    virtual bool DoSetPathShape(const wxGraphicsPath& WXUNUSED(path))
    {
        return false;
    }
#endif // wxUSE_GRAPHICS_CONTEXT
};

#if defined(__WXDFB__)
    #include "wx/dfb/nonownedwnd.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/nonownedwnd.h"
#elif defined(__WXMAC__)
    #include "wx/osx/nonownedwnd.h"
#elif defined(__WXMSW__) && !defined(__WXWINCE__)
    #include "wx/msw/nonownedwnd.h"
#else
    // No special class needed in other ports, they can derive both wxTLW and
    // wxPopupWindow directly from wxWindow and don't implement SetShape().
    class wxNonOwnedWindow : public wxNonOwnedWindowBase
    {
    };
#endif

#endif // _WX_NONOWNEDWND_H_
