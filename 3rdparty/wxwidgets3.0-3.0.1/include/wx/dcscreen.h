/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dcscreen.h
// Purpose:     wxScreenDC base header
// Author:      Julian Smart
// Modified by:
// Created:
// Copyright:   (c) Julian Smart
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCSCREEN_H_BASE_
#define _WX_DCSCREEN_H_BASE_

#include "wx/defs.h"
#include "wx/dc.h"

class WXDLLIMPEXP_CORE wxScreenDC : public wxDC
{
public:
    wxScreenDC();

    static bool StartDrawingOnTop(wxWindow * WXUNUSED(window))
        { return true; }
    static bool StartDrawingOnTop(wxRect * WXUNUSED(rect) =  NULL)
        { return true; }
    static bool EndDrawingOnTop()
        { return true; }

private:
    DECLARE_DYNAMIC_CLASS(wxScreenDC)
};


#endif
    // _WX_DCSCREEN_H_BASE_
