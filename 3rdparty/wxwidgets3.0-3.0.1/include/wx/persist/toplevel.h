///////////////////////////////////////////////////////////////////////////////
// Name:        wx/persist/toplevel.h
// Purpose:     persistence support for wxTLW
// Author:      Vadim Zeitlin
// Created:     2009-01-19
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PERSIST_TOPLEVEL_H_
#define _WX_PERSIST_TOPLEVEL_H_

#include "wx/persist/window.h"

#include "wx/toplevel.h"
#include "wx/display.h"

// ----------------------------------------------------------------------------
// string constants used by wxPersistentTLW
// ----------------------------------------------------------------------------

// we use just "Window" to keep configuration files and such short, there
// should be no confusion with wxWindow itself as we don't have persistent
// windows, just persistent controls which have their own specific kind strings
#define wxPERSIST_TLW_KIND "Window"

// names for various persistent options
#define wxPERSIST_TLW_X "x"
#define wxPERSIST_TLW_Y "y"
#define wxPERSIST_TLW_W "w"
#define wxPERSIST_TLW_H "h"

#define wxPERSIST_TLW_MAXIMIZED "Maximized"
#define wxPERSIST_TLW_ICONIZED "Iconized"

// ----------------------------------------------------------------------------
// wxPersistentTLW: supports saving/restoring window position and size as well
//                  as maximized/iconized/restore state
// ----------------------------------------------------------------------------

class wxPersistentTLW : public wxPersistentWindow<wxTopLevelWindow>
{
public:
    wxPersistentTLW(wxTopLevelWindow *tlw)
        : wxPersistentWindow<wxTopLevelWindow>(tlw)
    {
    }

    virtual void Save() const
    {
        const wxTopLevelWindow * const tlw = Get();

        const wxPoint pos = tlw->GetScreenPosition();
        SaveValue(wxPERSIST_TLW_X, pos.x);
        SaveValue(wxPERSIST_TLW_Y, pos.y);

        // notice that we use GetSize() here and not GetClientSize() because
        // the latter doesn't return correct results for the minimized windows
        // (at least not under Windows)
        //
        // of course, it shouldn't matter anyhow usually, the client size
        // should be preserved as well unless the size of the decorations
        // changed between the runs
        const wxSize size = tlw->GetSize();
        SaveValue(wxPERSIST_TLW_W, size.x);
        SaveValue(wxPERSIST_TLW_H, size.y);

        SaveValue(wxPERSIST_TLW_MAXIMIZED, tlw->IsMaximized());
        SaveValue(wxPERSIST_TLW_ICONIZED, tlw->IsIconized());
    }

    virtual bool Restore()
    {
        wxTopLevelWindow * const tlw = Get();

        long x wxDUMMY_INITIALIZE(-1),
             y wxDUMMY_INITIALIZE(-1),
             w wxDUMMY_INITIALIZE(-1),
             h wxDUMMY_INITIALIZE(-1);
        const bool hasPos = RestoreValue(wxPERSIST_TLW_X, &x) &&
                            RestoreValue(wxPERSIST_TLW_Y, &y);
        const bool hasSize = RestoreValue(wxPERSIST_TLW_W, &w) &&
                             RestoreValue(wxPERSIST_TLW_H, &h);

        if ( hasPos )
        {
            // to avoid making the window completely invisible if it had been
            // shown on a monitor which was disconnected since the last run
            // (this is pretty common for notebook with external displays)
            //
            // NB: we should allow window position to be (slightly) off screen,
            //     it's not uncommon to position the window so that its upper
            //     left corner has slightly negative coordinate
            if ( wxDisplay::GetFromPoint(wxPoint(x, y)) != wxNOT_FOUND ||
                 (hasSize && wxDisplay::GetFromPoint(
                                    wxPoint(x + w, y + h)) != wxNOT_FOUND) )
            {
                tlw->Move(x, y, wxSIZE_ALLOW_MINUS_ONE);
            }
            //else: should we try to adjust position/size somehow?
        }

        if ( hasSize )
            tlw->SetSize(w, h);

        // note that the window can be both maximized and iconized
        bool maximized;
        if ( RestoreValue(wxPERSIST_TLW_MAXIMIZED, &maximized) && maximized )
            tlw->Maximize();

        bool iconized;
        if ( RestoreValue(wxPERSIST_TLW_ICONIZED, &iconized) && iconized )
            tlw->Iconize();

        // the most important property of the window that we restore is its
        // size, so disregard the value of hasPos here
        return hasSize;
    }

    virtual wxString GetKind() const { return wxPERSIST_TLW_KIND; }
};

inline wxPersistentObject *wxCreatePersistentObject(wxTopLevelWindow *tlw)
{
    return new wxPersistentTLW(tlw);
}

#endif // _WX_PERSIST_TOPLEVEL_H_
