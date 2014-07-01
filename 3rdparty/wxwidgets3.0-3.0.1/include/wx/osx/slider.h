/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/slider.h
// Purpose:     wxSlider class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SLIDER_H_
#define _WX_SLIDER_H_

#include "wx/control.h"
#include "wx/slider.h"
#include "wx/stattext.h"

// Slider
class WXDLLIMPEXP_CORE wxSlider: public wxSliderBase
{
    DECLARE_DYNAMIC_CLASS(wxSlider)

public:
    wxSlider();

    inline wxSlider(wxWindow *parent, wxWindowID id,
                    int value, int minValue, int maxValue,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = wxSL_HORIZONTAL,
                    const wxValidator& validator = wxDefaultValidator,
                    const wxString& name = wxSliderNameStr)
    {
        Create(parent, id, value, minValue, maxValue, pos, size, style, validator, name);
    }

    virtual ~wxSlider();

    bool Create(wxWindow *parent, wxWindowID id,
                int value, int minValue, int maxValue,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSL_HORIZONTAL,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxSliderNameStr);

    virtual int GetValue() const ;
    virtual void SetValue(int);

    void SetRange(int minValue, int maxValue);

    inline int GetMin() const { return m_rangeMin; }
    inline int GetMax() const { return m_rangeMax; }

    void SetMin(int minValue) { SetRange(minValue, m_rangeMax); }
    void SetMax(int maxValue) { SetRange(m_rangeMin, maxValue); }

    // For trackbars only
    inline int GetTickFreq() const { return m_tickFreq; }
    void SetPageSize(int pageSize);
    int GetPageSize() const ;
    void ClearSel() ;
    void ClearTicks() ;
    void SetLineSize(int lineSize);
    int GetLineSize() const ;
    int GetSelEnd() const ;
    int GetSelStart() const ;
    void SetSelection(int minPos, int maxPos);
    void SetThumbLength(int len) ;
    int GetThumbLength() const ;
    void SetTick(int tickPos) ;

    void Command(wxCommandEvent& event);
    // osx specific event handling common for all osx-ports

    virtual bool OSXHandleClicked( double timestampsec );
    virtual void TriggerScrollEvent( wxEventType scrollEvent ) ;

protected:
    // Platform-specific implementation of SetTickFreq
    virtual void DoSetTickFreq(int freq);

    virtual wxSize DoGetBestSize() const;
    virtual void   DoSetSize(int x, int y, int w, int h, int sizeFlags);
    virtual void   DoMoveWindow(int x, int y, int w, int h);

    // set min/max size of the slider
    virtual void DoSetSizeHints( int minW, int minH,
                                 int maxW, int maxH,
                                 int incW, int incH);

    // Common processing to invert slider values based on wxSL_INVERSE
    virtual int ValueInvertOrNot(int value) const;

    wxStaticText*    m_macMinimumStatic ;
    wxStaticText*    m_macMaximumStatic ;
    wxStaticText*    m_macValueStatic ;

    int           m_rangeMin;
    int           m_rangeMax;
    int           m_pageSize;
    int           m_lineSize;
    int           m_tickFreq;
private :
DECLARE_EVENT_TABLE()
};

#endif
    // _WX_SLIDER_H_
