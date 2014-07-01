/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/slider.h
// Purpose:     wxSlider class
// Author:      David Webster
// Modified by:
// Created:     10/15/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SLIDER_H_
#define _WX_SLIDER_H_

#include "wx/control.h"

// Slider
class WXDLLIMPEXP_CORE wxSlider: public wxSliderBase
{
public:
  wxSlider();
  inline wxSlider( wxWindow*          pParent
                  ,wxWindowID         vId
                  ,int                nValue
                  ,int                nMinValue
                  ,int                nMaxValue
                  ,const wxPoint&     rPos = wxDefaultPosition
                  ,const wxSize&      rSize = wxDefaultSize
                  ,long               lStyle = wxSL_HORIZONTAL
                  ,const wxValidator& rValidator = wxDefaultValidator
                  ,const wxString&    rsName = wxSliderNameStr
                 )
    {
        Create( pParent
               ,vId
               ,nValue
               ,nMinValue
               ,nMaxValue
               ,rPos
               ,rSize
               ,lStyle
               ,rValidator
               ,rsName
              );
    }
    virtual ~wxSlider();

    bool Create( wxWindow*          pParent
                ,wxWindowID         vId
                ,int                nValue
                ,int                nMinValue
                ,int                nMaxValue
                ,const wxPoint&     rPos = wxDefaultPosition
                ,const wxSize&      rSize = wxDefaultSize
                ,long               lStyle = wxSL_HORIZONTAL
                ,const wxValidator& rValidator = wxDefaultValidator
                ,const wxString&    rsName = wxSliderNameStr
               );

         virtual int  GetValue(void) const ;
         virtual void SetValue(int);

                 void GetSize( int* pnX
                              ,int* pnY
                             ) const;
                 void GetPosition( int* pnX
                                  ,int* pnY
                                 ) const ;
                 bool Show(bool bShow = TRUE);
                 void SetRange( int nMinValue
                               ,int nMaxValue
                              );

    inline       int  GetMin(void) const { return m_nRangeMin; }
    inline       int  GetMax(void) const { return m_nRangeMax; }

    //
    // For trackbars only
    //
                 void ClearSel(void);
                 void ClearTicks(void);

                 int  GetLineSize(void) const;
                 int  GetPageSize(void) const ;
                 int  GetSelEnd(void) const;
                 int  GetSelStart(void) const;
    inline       int  GetTickFreq(void) const { return m_nTickFreq; }
                 int  GetThumbLength(void) const ;

                 void SetLineSize(int nLineSize);
                 void SetPageSize(int nPageSize);
                 void SetSelection( int nMinPos
                                   ,int nMaxPos
                                  );
                 void SetThumbLength(int nLen) ;
                 void SetTick(int ntickPos) ;

    //
    // IMPLEMENTATION
    //
    inline         WXHWND   GetStaticMin(void) const { return m_hStaticMin; }
    inline         WXHWND   GetStaticMax(void) const { return m_hStaticMax; }
    inline         WXHWND   GetEditValue(void) const { return m_hStaticValue; }
           virtual bool     ContainsHWND(WXHWND hWnd) const;
                   void     AdjustSubControls( int  nX
                                              ,int  nY
                                              ,int  nWidth
                                              ,int  nHeight
                                              ,int  nSizeFlags
                                             );
    inline         int      GetSizeFlags(void) { return m_nSizeFlags; }
                   void     Command(wxCommandEvent& rEvent);
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
    WXHWND                          m_hStaticMin;
    WXHWND                          m_hStaticMax;
    WXHWND                          m_hStaticValue;
    int                             m_nRangeMin;
    int                             m_nRangeMax;
    int                             m_nPageSize;
    int                             m_nLineSize;
    int                             m_nTickFreq;
    double                          m_dPixelToRange;
    int                             m_nThumbLength;
    int                             m_nSizeFlags;

    virtual void DoGetSize( int* pnWidth
                           ,int* pnHeight
                          ) const;
    virtual void DoSetSize( int  nX
                           ,int  nY
                           ,int  nWidth
                           ,int  nHeight
                           ,int  nSizeFlags = wxSIZE_AUTO
                          );

    // Platform-specific implementation of SetTickFreq
    virtual void DoSetTickFreq(int freq);

private:
    DECLARE_DYNAMIC_CLASS(wxSlider)
}; // end of CLASS wxSlider

#endif
    // _WX_SLIDER_H_
