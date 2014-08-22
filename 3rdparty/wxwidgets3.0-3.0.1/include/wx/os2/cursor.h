/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/cursor.h
// Purpose:     wxCursor class
// Author:      David Webster
// Modified by:
// Created:     10/13/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CURSOR_H_
#define _WX_CURSOR_H_

#include "wx/bitmap.h"

class WXDLLIMPEXP_CORE wxCursorRefData: public wxGDIImageRefData
{
public:
    wxCursorRefData();
    virtual ~wxCursorRefData() { Free(); }
    virtual void Free(void);
    bool                            m_bDestroyCursor;
}; // end of CLASS wxCursorRefData

#define M_CURSORDATA ((wxCursorRefData *)m_refData)
#define M_CURSORHANDLERDATA ((wxCursorRefData *)bitmap->m_refData)

// Cursor
class WXDLLIMPEXP_CORE wxCursor: public wxBitmap
{
public:
    wxCursor();

    wxCursor(const wxImage& rImage);

    wxCursor( const wxString& rsName
             ,wxBitmapType    lType = wxCURSOR_DEFAULT_TYPE
             ,int             nHotSpotX = 0
             ,int             nHotSpotY = 0
            );
    wxCursor(wxStockCursor id) { InitFromStock(id); }
#if WXWIN_COMPATIBILITY_2_8
    wxCursor(int id) { InitFromStock((wxStockCursor)id); }
#endif
    inline ~wxCursor() { }

    inline WXHCURSOR GetHCURSOR(void) const { return (M_CURSORDATA ? M_CURSORDATA->m_hCursor : 0); }
    inline void      SetHCURSOR(WXHCURSOR hCursor) { SetHandle((WXHANDLE)hCursor); }

protected:
    void InitFromStock(wxStockCursor);
    inline virtual wxGDIImageRefData* CreateData(void) const { return (new wxCursorRefData); }

private:
    DECLARE_DYNAMIC_CLASS(wxCursor)
}; // end of CLASS wxCursor

#endif
    // _WX_CURSOR_H_
