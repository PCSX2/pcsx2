///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/popupwin.h
// Purpose:     wxPopupWindow class for wxMSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.01.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_POPUPWIN_H_
#define _WX_MSW_POPUPWIN_H_

// ----------------------------------------------------------------------------
// wxPopupWindow
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPopupWindow : public wxPopupWindowBase
{
public:
    wxPopupWindow() { }

    wxPopupWindow(wxWindow *parent, int flags = wxBORDER_NONE)
        { (void)Create(parent, flags); }

    bool Create(wxWindow *parent, int flags = wxBORDER_NONE);

    virtual void SetFocus();
    virtual bool Show(bool show = true);

    // return the style to be used for the popup windows
    virtual WXDWORD MSWGetStyle(long flags, WXDWORD *exstyle) const;

    // get the HWND to be used as parent of this window with CreateWindow()
    virtual WXHWND MSWGetParent() const;

protected:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxPopupWindow);
};

#endif // _WX_MSW_POPUPWIN_H_

