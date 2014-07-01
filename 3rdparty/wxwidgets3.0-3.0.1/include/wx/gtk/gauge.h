/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/gauge.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_GAUGE_H_
#define _WX_GTK_GAUGE_H_

//-----------------------------------------------------------------------------
// wxGauge
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGauge: public wxControl
{
public:
    wxGauge() { Init(); }

    wxGauge( wxWindow *parent,
             wxWindowID id,
             int range,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxGA_HORIZONTAL,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxGaugeNameStr )
    {
        Init();

        Create(parent, id, range, pos, size, style, validator, name);
    }

    bool Create( wxWindow *parent,
                 wxWindowID id, int range,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxGA_HORIZONTAL,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString& name = wxGaugeNameStr );

    void SetShadowWidth( int WXUNUSED(w) ) { }
    void SetBezelFace( int WXUNUSED(w) ) { }
    int GetShadowWidth() const { return 0; }
    int GetBezelFace() const { return 0; }

    // determinate mode API
    void SetRange( int r );
    void SetValue( int pos );

    int GetRange() const;
    int GetValue() const;

    // indeterminate mode API
    virtual void Pulse();

    bool IsVertical() const { return HasFlag(wxGA_VERTICAL); }

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    virtual wxVisualAttributes GetDefaultAttributes() const;

    // implementation
    // -------------

    // the max and current gauge values
    int m_rangeMax,
        m_gaugePos;

protected:
    // set the gauge value to the value of m_gaugePos
    void DoSetGauge();

    virtual wxSize DoGetBestSize() const;

private:
    void Init() { m_rangeMax = m_gaugePos = 0; }

    DECLARE_DYNAMIC_CLASS(wxGauge)
};

#endif
    // _WX_GTK_GAUGE_H_
