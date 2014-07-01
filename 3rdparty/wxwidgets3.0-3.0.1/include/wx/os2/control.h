/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/control.h
// Purpose:     wxControl class
// Author:      David Webster
// Modified by:
// Created:     09/17/99
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CONTROL_H_
#define _WX_CONTROL_H_

#include "wx/dynarray.h"

// General item class
class WXDLLIMPEXP_CORE wxControl : public wxControlBase
{
    DECLARE_ABSTRACT_CLASS(wxControl)

public:
   wxControl();
   wxControl( wxWindow*          pParent
             ,wxWindowID         vId
             ,const wxPoint&     rPos = wxDefaultPosition
             ,const wxSize&      rSize = wxDefaultSize
             ,long               lStyle = 0
             ,const wxValidator& rValidator = wxDefaultValidator
             ,const wxString&    rsName = wxControlNameStr
            )
    {
        Create( pParent, vId, rPos, rSize, lStyle, rValidator, rsName );
    }

    bool Create( wxWindow*          pParent
                ,wxWindowID         vId
                ,const wxPoint&     rPos = wxDefaultPosition
                ,const wxSize&      rSize = wxDefaultSize
                ,long               lStyle = 0
                ,const wxValidator& rValidator = wxDefaultValidator
                ,const wxString&    rsName = wxControlNameStr
               );

    virtual void SetLabel(const wxString& rsLabel);
    virtual wxString GetLabel() const { return m_label; }

    //
    // Simulates an event
    //
    virtual void Command(wxCommandEvent& rEvent) { ProcessCommand(rEvent); }

    //
    // Implementation from now on
    // --------------------------
    //

    //
    // Calls the callback and appropriate event handlers
    //
    bool ProcessCommand(wxCommandEvent& rEvent);

    //
    // For ownerdraw items
    //
    virtual bool OS2OnDraw(WXDRAWITEMSTRUCT* WXUNUSED(pItem)) { return false; }
    virtual long OS2OnMeasure(WXMEASUREITEMSTRUCT* WXUNUSED(pItem)) { return 0L; }

    wxArrayLong&     GetSubcontrols() { return m_aSubControls; }
    void             OnEraseBackground(wxEraseEvent& rEvent);
    virtual WXHBRUSH OnCtlColor( WXHDC    hDC
                                ,WXHWND   pWnd
                                ,WXUINT   nCtlColor
                                ,WXUINT   uMessage
                                ,WXWPARAM wParam
                                ,WXLPARAM lParam
                               );

public:
    //
    // For controls like radiobuttons which are really composite
    //
    wxArrayLong m_aSubControls;

    virtual wxSize DoGetBestSize(void) const;

    //
    // Create the control of the given PM class
    //
    bool OS2CreateControl( const wxChar*   zClassname
                          ,const wxString& rsLabel
                          ,const wxPoint&  rPos
                          ,const wxSize&   rSize
                          ,long            lStyle
                         );
    //
    // Create the control of the given class with the given style, returns false
    // if creation failed.
    //
    bool OS2CreateControl( const wxChar*   zClassname
                          ,WXDWORD         dwStyle
                          ,const wxPoint&  rPos = wxDefaultPosition
                          ,const wxSize&   rSize = wxDefaultSize
                          ,const wxString& rsLabel = wxEmptyString
                          ,WXDWORD         dwExstyle = (WXDWORD)-1
                         );

    //
    // Default style for the control include WS_TABSTOP if it AcceptsFocus()
    //
    virtual WXDWORD OS2GetStyle( long     lStyle
                                ,WXDWORD* pdwExstyle
                               ) const;

    inline int  GetXComp(void) const {return m_nXComp;}
    inline int  GetYComp(void) const {return m_nYComp;}
    inline void SetXComp(const int nXComp) {m_nXComp = nXComp;}
    inline void SetYComp(const int nYComp) {m_nYComp = nYComp;}

private:
    int m_nXComp;
    int m_nYComp;

    wxString m_label;
    WXDWORD  m_dwStyle;

    DECLARE_EVENT_TABLE()
}; // end of wxControl

#endif // _WX_CONTROL_H_
