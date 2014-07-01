/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/cursor.h
// Purpose:     wxCursor class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CURSOR_H_
#define _WX_CURSOR_H_

#include "wx/gdiobj.h"
#include "wx/gdicmn.h"

class WXDLLIMPEXP_FWD_CORE wxImage;

// Cursor
class WXDLLIMPEXP_CORE wxCursor : public wxGDIObject
{
public:
    wxCursor();

    wxCursor(const char bits[], int width, int height,
             int hotSpotX = -1, int hotSpotY = -1,
             const char maskBits[] = NULL,
             const wxColour* fg = NULL, const wxColour* bg = NULL);

    wxCursor(const wxString& name,
             wxBitmapType type = wxCURSOR_DEFAULT_TYPE,
             int hotSpotX = 0, int hotSpotY = 0);

#if wxUSE_IMAGE
    wxCursor(const wxImage& image);
#endif

    wxCursor(wxStockCursor id) { InitFromStock(id); }
#if WXWIN_COMPATIBILITY_2_8
    wxCursor(int id) { InitFromStock((wxStockCursor)id); }
#endif

    virtual ~wxCursor();

    // Motif-specific.
    // Create/get a cursor for the current display
    WXCursor GetXCursor(WXDisplay* display) const;

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    void InitFromStock(wxStockCursor);

    void Create(const char bits[], int width, int height,
                int hotSpotX = -1, int hotSpotY = -1,
                const char maskBits[] = NULL);
    void Create(WXPixmap cursor, WXPixmap mask, int hotSpotX, int hotSpotY);

    // Make a cursor from standard id
    WXCursor MakeCursor(WXDisplay* display, wxStockCursor id) const;

    DECLARE_DYNAMIC_CLASS(wxCursor)
};

extern WXDLLIMPEXP_CORE void wxSetCursor(const wxCursor& cursor);

#endif
// _WX_CURSOR_H_
