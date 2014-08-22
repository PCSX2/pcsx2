/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/gauge.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKGAUGEH__
#define __GTKGAUGEH__

//-----------------------------------------------------------------------------
// wxGaugeBox
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGauge: public wxGaugeBase
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
    void SetRange( int r );
    void SetValue( int pos );
    int GetShadowWidth() const { return 0; }
    int GetBezelFace() const { return 0; }
    int GetRange() const;
    int GetValue() const;

    bool IsVertical() const { return HasFlag(wxGA_VERTICAL); }

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation
    // -------------

    // the max and current gauge values
    int m_rangeMax,
        m_gaugePos;

protected:
    // common part of all ctors
    void Init() { m_rangeMax = m_gaugePos = 0; }

    // set the gauge value to the value of m_gaugePos
    void DoSetGauge();

    virtual wxSize DoGetBestSize() const;

    virtual wxVisualAttributes GetDefaultAttributes() const;

private:
    DECLARE_DYNAMIC_CLASS(wxGauge)
};

#endif
    // __GTKGAUGEH__
