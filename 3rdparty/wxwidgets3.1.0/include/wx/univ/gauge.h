///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/gauge.h
// Purpose:     wxUniversal wxGauge declaration
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.02.01
// Copyright:   (c) 2001 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_GAUGE_H_
#define _WX_UNIV_GAUGE_H_

// ----------------------------------------------------------------------------
// wxGauge: a progress bar
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGauge : public wxGaugeBase
{
public:
    wxGauge() { Init(); }

    wxGauge(wxWindow *parent,
            wxWindowID id,
            int range,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxGA_HORIZONTAL,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxGaugeNameStr)
    {
        Init();

        (void)Create(parent, id, range, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                int range,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxGA_HORIZONTAL,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxGaugeNameStr);

    // implement base class virtuals
    virtual void SetRange(int range) wxOVERRIDE;
    virtual void SetValue(int pos) wxOVERRIDE;

    // wxUniv-specific methods

    // is it a smooth progress bar or a discrete one?
    bool IsSmooth() const { return (GetWindowStyle() & wxGA_SMOOTH) != 0; }

    // is it a vertica; progress bar or a horizontal one?
    bool IsVertical() const { return (GetWindowStyle() & wxGA_VERTICAL) != 0; }

protected:
    // common part of all ctors
    void Init();

    // return the def border for a progress bar
    virtual wxBorder GetDefaultBorder() const wxOVERRIDE;

    // return the default size
    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;

    // draw the control
    virtual void DoDraw(wxControlRenderer *renderer) wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxGauge);
};

#endif // _WX_UNIV_GAUGE_H_
