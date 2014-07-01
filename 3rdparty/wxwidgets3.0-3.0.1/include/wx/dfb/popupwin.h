///////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/popupwin.h
// Purpose:     wxPopupWindow class for wxDFB
// Author:      Vaclav Slavik
// Created:     2006-12-24
// Copyright:   (c) 2006 TT-Solutions
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_POPUPWIN_H_
#define _WX_DFB_POPUPWIN_H_

// ----------------------------------------------------------------------------
// wxPopupWindow
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPopupWindow : public wxPopupWindowBase
{
public:
    wxPopupWindow() {}

    wxPopupWindow(wxWindow *parent, int flags = wxBORDER_NONE)
        { Create(parent, flags); }

    bool Create(wxWindow *parent, int flags = wxBORDER_NONE)
    {
        if ( !wxPopupWindowBase::Create(parent) )
            return false;

        return wxNonOwnedWindow::Create
               (
                 parent,
                 -1,
                 // DFB windows must have valid pos & size:
                 wxPoint(0, 0), wxSize(1, 1),
                 (flags & wxBORDER_MASK) | wxPOPUP_WINDOW
               );
    }

    DECLARE_DYNAMIC_CLASS(wxPopupWindow)
};

#endif // _WX_DFB_POPUPWIN_H_

