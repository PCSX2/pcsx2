/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/spinbutt.h
// Purpose:     wxSpinButton class
// Author:      David Webster
// Modified by:
// Created:     10/15/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SPINBUTT_H_
#define _WX_SPINBUTT_H_

#include "wx/control.h"
#include "wx/event.h"

extern MRESULT EXPENTRY wxSpinCtrlWndProc(
  HWND                              hWnd
, UINT                              uMessage
, MPARAM                            wParam
, MPARAM                            lParam
);

class WXDLLIMPEXP_CORE wxSpinButton: public wxSpinButtonBase
{
public:
    // Construction
    wxSpinButton() { }
    inline wxSpinButton( wxWindow*       pParent
                        ,wxWindowID      vId = -1
                        ,const wxPoint&  rPos = wxDefaultPosition
                        ,const wxSize&   rSize = wxDefaultSize
                        ,long            lStyle = wxSP_VERTICAL
                        ,const wxString& rsName = wxT("wxSpinButton")
                       )
    {
        Create(pParent, vId, rPos, rSize, lStyle, rsName);
    }
    virtual ~wxSpinButton();


    bool Create( wxWindow*       pParent
                ,wxWindowID      vId = -1
                ,const wxPoint&  rPos = wxDefaultPosition
                ,const wxSize&   rSize = wxDefaultSize
                ,long            lStyle = wxSP_VERTICAL
                ,const wxString& rsName = wxT("wxSpinButton")
               );

    // Accessors
    inline virtual int  GetMax(void) const { return m_max; }
    inline virtual int  GetMin(void) const { return m_min; }
           virtual int  GetValue(void) const;
    inline         bool IsVertical(void) const {return ((m_windowStyle & wxSP_VERTICAL) != 0); }
           virtual void SetValue(int nVal);
           virtual void SetRange( int nMinVal
                                 ,int nMaxVal
                                );

    //
    // Implementation
    //
    virtual bool OS2Command( WXUINT wParam
                            ,WXWORD wId
                           );
    virtual bool OS2OnScroll( int    nOrientation
                             ,WXWORD wParam
                             ,WXWORD wPos
                             ,WXHWND hControl
                            );

    inline virtual bool AcceptsFocus(void) const { return FALSE; }
protected:
    virtual wxSize DoGetBestSize() const;
private:
    DECLARE_DYNAMIC_CLASS(wxSpinButton)
}; // end of CLASS wxSpinButton

#endif // _WX_SPINBUTT_H_
