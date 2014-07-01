/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/stattext.h
// Purpose:     wxStaticText class
// Author:      David Webster
// Modified by:
// Created:     10/17/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STATTEXT_H_
#define _WX_STATTEXT_H_

#include "wx/control.h"

class WXDLLIMPEXP_CORE wxStaticText : public wxStaticTextBase
{
public:
    inline wxStaticText() { }
    inline wxStaticText( wxWindow*       pParent
                        ,wxWindowID      vId
                        ,const wxString& rsLabel
                        ,const wxPoint&  rPos = wxDefaultPosition
                        ,const wxSize&   rSize = wxDefaultSize
                        ,long            lStyle = 0L
                        ,const wxString& rsName = wxStaticTextNameStr
                       )
    {
        Create(pParent, vId, rsLabel, rPos, rSize, lStyle, rsName);
    }

    bool Create( wxWindow*       pParent
                ,wxWindowID      vId
                ,const wxString& rsLabel
                ,const wxPoint&  rPos = wxDefaultPosition
                ,const wxSize&   rSize = wxDefaultSize
                ,long            lStyle = 0L
                ,const wxString& rsName = wxStaticTextNameStr
               );

    //
    // Accessors
    //
    virtual void SetLabel(const wxString& rsLabel);
    virtual bool SetFont(const wxFont &rFont);

    //
    // Overridden base class virtuals
    //
    virtual bool AcceptsFocus() const { return FALSE; }

    //
    // Callbacks
    //
    virtual MRESULT OS2WindowProc( WXUINT   uMsg
                                  ,WXWPARAM wParam
                                  ,WXLPARAM lParam
                                 );

protected:
    virtual void   DoSetSize( int nX
                             ,int nY
                             ,int nWidth
                             ,int nHeight
                             ,int nSizeFlags = wxSIZE_AUTO
                            );
    virtual wxSize DoGetBestSize(void) const;

    virtual void DoSetLabel(const wxString& str);
    virtual wxString DoGetLabel() const;

private:
    DECLARE_DYNAMIC_CLASS(wxStaticText)
}; // end of CLASS wxStaticText

#endif
    // _WX_STATTEXT_H_
