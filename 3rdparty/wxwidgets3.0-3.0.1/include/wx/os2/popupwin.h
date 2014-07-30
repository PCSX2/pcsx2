///////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/popupwin.h
// Purpose:     wxPopupWindow class for wxPM
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.01.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PM_POPUPWIN_H_
#define _WX_PM_POPUPWIN_H_

// ----------------------------------------------------------------------------
// wxPopupWindow
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPopupWindow : public wxPopupWindowBase
{
public:
    wxPopupWindow() { }

    wxPopupWindow( wxWindow* pParent
                  ,int       nFlags
                 )
    { (void)Create(pParent, nFlags); }

    bool Create( wxWindow* pParent
                ,int       nFlags = wxBORDER_NONE
               );
    //
    // Implementation only from now on
    // -------------------------------
    //
protected:

    virtual void DoGetPosition( int* pnX
                               ,int* pny
                              ) const;

    virtual WXDWORD OS2GetStyle( long     lFlags
                                ,WXDWORD* dwExstyle
                               ) const;
    //
    // Get the HWND to be used as parent of this window with CreateWindow()
    //
    virtual WXHWND OS2GetParent(void) const;

    //
    // The list of all currently shown popup windows used by FindPopupFor()
    //
    static wxWindowList             m_svShownPopups;

    DECLARE_DYNAMIC_CLASS(wxPopupWindow)
}; // end of CLASS wxPopupWindow

#endif // _WX_PM_POPUPWIN_H_
