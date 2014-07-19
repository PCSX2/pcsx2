/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/cursor.h
// Purpose:     wxCursor class
// Author:      David Elliott <dfe@cox.net>
// Modified by:
// Created:     2002/11/27
// Copyright:   (c) David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_CURSOR_H_
#define _WX_COCOA_CURSOR_H_

#include "wx/bitmap.h"

class WXDLLIMPEXP_CORE wxCursorRefData : public wxGDIRefData
{
public:
    wxCursorRefData();
    virtual ~wxCursorRefData();

protected:
    int m_width, m_height;
    WX_NSCursor m_hCursor;

    friend class WXDLLIMPEXP_FWD_CORE wxBitmap;
    friend class WXDLLIMPEXP_FWD_CORE wxCursor;

    wxDECLARE_NO_COPY_CLASS(wxCursorRefData);
};

#define M_CURSORDATA ((wxCursorRefData *)m_refData)
#define M_CURSORHANDLERDATA ((wxCursorRefData *)bitmap->m_refData)

// Cursor
class WXDLLIMPEXP_CORE wxCursor: public wxBitmap
{
public:
    wxCursor();

    wxCursor(const wxString& name, wxBitmapType type = wxCURSOR_DEFAULT_TYPE,
             int hotSpotX = 0, int hotSpotY = 0);

    wxCursor(wxStockCursor id) { InitFromStock(id); }
#if WXWIN_COMPATIBILITY_2_8
    wxCursor(int id) { InitFromStock((wxStockCursor)id); }
#endif
    virtual ~wxCursor();

    // FIXME: operator==() is wrong!
    bool operator==(const wxCursor& cursor) const { return m_refData == cursor.m_refData; }
    bool operator!=(const wxCursor& cursor) const { return !(*this == cursor); }

    WX_NSCursor GetNSCursor() const { return M_CURSORDATA ? M_CURSORDATA->m_hCursor : 0; }

private:
    void InitFromStock(wxStockCursor);
    DECLARE_DYNAMIC_CLASS(wxCursor)
};

extern WXDLLIMPEXP_CORE void wxSetCursor(const wxCursor& cursor);

#endif
    // _WX_COCOA_CURSOR_H_
