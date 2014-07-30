/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/minifram.h
// Purpose:     wxMiniFrame class. A small frame for e.g. floating toolbars.
//              If there is no equivalent on your platform, just make it a
//              normal frame.
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MINIFRAM_H_
#define _WX_MINIFRAM_H_

#include "wx/frame.h"

class WXDLLIMPEXP_CORE wxMiniFrame: public wxFrame {

    DECLARE_DYNAMIC_CLASS(wxMiniFrame)

public:
    inline wxMiniFrame() {}
    inline wxMiniFrame(wxWindow *parent,
        wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_FRAME_STYLE|wxTINY_CAPTION,
        const wxString& name = wxFrameNameStr)
    {
        // Use wxFrame constructor in absence of more specific code.
        Create(parent, id, title, pos, size, style, name);
    }

    virtual ~wxMiniFrame() {}
protected:
};

#endif
// _WX_MINIFRAM_H_
