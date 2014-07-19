/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/statbox.h
// Purpose:     wxStaticBox class
// Author:      David Webster
// Modified by:
// Created:     10/15/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STATBOX_H_
#define _WX_STATBOX_H_

#include "wx/control.h"

// Group box
class WXDLLIMPEXP_CORE wxStaticBox : public wxStaticBoxBase
{
public:
    inline wxStaticBox() {}
    inline wxStaticBox( wxWindow*       pParent
                       ,wxWindowID      vId
                       ,const wxString& rsLabel
                       ,const wxPoint&  rPos = wxDefaultPosition
                       ,const wxSize&   rSize = wxDefaultSize
                       ,long            lStyle = 0
                       ,const wxString& rsName = wxStaticBoxNameStr
                      )
    {
        Create(pParent, vId, rsLabel, rPos, rSize, lStyle, rsName);
    }

    bool Create( wxWindow*       pParent
                ,wxWindowID      vId
                ,const wxString& rsLabel
                ,const wxPoint&  rPos = wxDefaultPosition
                ,const wxSize&   rSize = wxDefaultSize
                ,long            lStyle = 0
                ,const wxString& rsName = wxStaticBoxNameStr
               );

    //
    // implementation from now on
    // --------------------------
    //
    virtual MRESULT OS2WindowProc( WXUINT   uMsg
                                  ,WXWPARAM wParam
                                  ,WXLPARAM lParam
                                 );

    //
    // overridden base class virtuals
    //
    inline virtual bool AcceptsFocus(void) const { return FALSE; }

protected:
    virtual wxSize DoGetBestSize(void) const;

private:
    DECLARE_DYNAMIC_CLASS(wxStaticBox)
}; // end of CLASS wxStaticBox

#endif
    // _WX_STATBOX_H_

