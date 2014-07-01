/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/gauge.h
// Purpose:     wxGauge class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GAUGE_H_
#define _WX_GAUGE_H_

// Group box
class WXDLLIMPEXP_CORE wxGauge : public wxGaugeBase
{
    DECLARE_DYNAMIC_CLASS(wxGauge)

public:
    inline wxGauge() { m_rangeMax = 0; m_gaugePos = 0; }

    inline wxGauge(wxWindow *parent, wxWindowID id,
        int range,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxGA_HORIZONTAL,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxGaugeNameStr)
    {
        Create(parent, id, range, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
        int range,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxGA_HORIZONTAL,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxGaugeNameStr);

    void SetShadowWidth(int w);
    void SetRange(int r);
    void SetValue(int pos);

    int GetShadowWidth() const ;
    int GetRange() const ;
    int GetValue() const ;

    virtual void Command(wxCommandEvent& WXUNUSED(event)) {} ;

private:
    virtual wxSize DoGetBestSize() const;
    virtual void DoMoveWindow(int x, int y, int width, int height);
};

#endif
// _WX_GAUGE_H_
