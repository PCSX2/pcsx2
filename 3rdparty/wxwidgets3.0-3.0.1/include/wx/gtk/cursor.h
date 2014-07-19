/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/cursor.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_CURSOR_H_
#define _WX_GTK_CURSOR_H_

#include "wx/gdiobj.h"
#include "wx/gdicmn.h"

class WXDLLIMPEXP_FWD_CORE wxColour;
class WXDLLIMPEXP_FWD_CORE wxImage;

//-----------------------------------------------------------------------------
// wxCursor
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxCursor : public wxGDIObject
{
public:
    wxCursor();
    wxCursor(wxStockCursor id) { InitFromStock(id); }
#if WXWIN_COMPATIBILITY_2_8
    wxCursor(int id) { InitFromStock((wxStockCursor)id); }
#endif
#if wxUSE_IMAGE
    wxCursor( const wxImage & image );
    wxCursor(const wxString& name,
             wxBitmapType type = wxCURSOR_DEFAULT_TYPE,
             int hotSpotX = 0, int hotSpotY = 0);
#endif
    wxCursor( const char bits[], int width, int height,
              int hotSpotX = -1, int hotSpotY = -1,
              const char maskBits[] = NULL,
              const wxColour* fg = NULL, const wxColour* bg = NULL);
    virtual ~wxCursor();

    // implementation

    GdkCursor *GetCursor() const;

protected:
    void InitFromStock(wxStockCursor);
#if wxUSE_IMAGE
    void InitFromImage(const wxImage& image);
#endif

    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    DECLARE_DYNAMIC_CLASS(wxCursor)
};

#endif // _WX_GTK_CURSOR_H_
