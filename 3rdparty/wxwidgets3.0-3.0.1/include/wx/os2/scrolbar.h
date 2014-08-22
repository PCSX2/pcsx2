/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/scrolbar.h
// Purpose:     wxScrollBar class
// Author:      David Webster
// Modified by:
// Created:     10/15/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SCROLBAR_H_
#define _WX_SCROLBAR_H_

#include "wx/scrolbar.h"

// Scrollbar item
class WXDLLIMPEXP_CORE wxScrollBar : public wxScrollBarBase
{
public:
    inline wxScrollBar()
    {
        m_nPageSize = 0;
        m_nViewSize = 0;
        m_nObjectSize = 0;
    }
    inline wxScrollBar( wxWindow*          pParent
                       ,wxWindowID         vId
                       ,const wxPoint&     rPos = wxDefaultPosition
                       ,const wxSize&      rSize = wxDefaultSize
                       ,long               lStyle = wxSB_HORIZONTAL
#if wxUSE_VALIDATORS
                       ,const wxValidator& rValidator = wxDefaultValidator
#endif
                       ,const wxString&    rsName = wxScrollBarNameStr
                      )
    {
        Create( pParent
               ,vId
               ,rPos
               ,rSize
               ,lStyle
#if wxUSE_VALIDATORS
               ,rValidator
#endif
               ,rsName
              );
    }
    virtual ~wxScrollBar();

    bool Create( wxWindow*          pParent
                ,wxWindowID         vId
                ,const wxPoint&     rPos = wxDefaultPosition
                ,const wxSize&      rSize = wxDefaultSize
                ,long               lStyle = wxSB_HORIZONTAL
#if wxUSE_VALIDATORS
                ,const wxValidator& rValidator = wxDefaultValidator
#endif
                ,const wxString&    rsName = wxScrollBarNameStr
               );

                   int   GetThumbPosition(void) const ;
    inline         int   GetThumbSize(void) const { return m_nPageSize; }
    inline         int   GetPageSize(void) const { return m_nViewSize; }
    inline         int   GetRange(void) const { return m_nObjectSize; }

           virtual void  SetThumbPosition(int nViewStart);
           virtual void  SetScrollbar( int  nPosition
                                      ,int  nThumbSize
                                      ,int  nRange
                                      ,int  nPageSize
                                      ,bool bRefresh = TRUE
                                     );

                   void      Command(wxCommandEvent& rEvent);
            virtual WXHBRUSH OnCtlColor( WXHDC    hDC
                                        ,WXHWND   hWnd
                                        ,WXUINT   uCtlColor
                                        ,WXUINT   uMessage
                                        ,WXWPARAM wParam
                                        ,WXLPARAM lParam
                                       );
            virtual bool     OS2OnScroll( int    nOrientation
                                         ,WXWORD wParam
                                         ,WXWORD wPos
                                         ,WXHWND hControl
                                        );

protected:
    int                             m_nPageSize;
    int                             m_nViewSize;
    int                             m_nObjectSize;

private:
    DECLARE_DYNAMIC_CLASS(wxScrollBar)
}; // end of CLASS wxScrollBar

#endif
    // _WX_SCROLBAR_H_
