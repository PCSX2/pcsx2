/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/spinctrl.h
// Purpose:     wxSpinCtrl class
// Author:      Robert Roebling
// Modified by:
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_SPINCTRL_H_
#define _WX_GTK_SPINCTRL_H_

//-----------------------------------------------------------------------------
// wxSpinCtrlGTKBase - Base class for GTK versions of the wxSpinCtrl[Double]
//
// This class manages a double valued GTK spinctrl through the DoGet/SetXXX
// functions that are made public as Get/SetXXX functions for int or double
// for the wxSpinCtrl and wxSpinCtrlDouble classes respectively to avoid
// function ambiguity.
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSpinCtrlGTKBase : public wxSpinCtrlBase
{
public:
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& value,
                const wxPoint& pos,
                const wxSize& size,
                long style,
                double min, double max, double initial,
                double inc,
                const wxString& name);

    // wxSpinCtrl(Double) methods call DoXXX functions of the same name

    // accessors
    // T GetValue() const
    // T GetMin() const
    // T GetMax() const
    // T GetIncrement() const
    virtual bool GetSnapToTicks() const;

    // operations
    virtual void SetValue(const wxString& value);
    // void SetValue(T val)
    // void SetRange(T minVal, T maxVal)
    // void SetIncrement(T inc)
    void SetSnapToTicks( bool snap_to_ticks );

    // Select text in the textctrl
    void SetSelection(long from, long to);

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation
    void OnChar( wxKeyEvent &event );

protected:
    double DoGetValue() const;
    double DoGetMin() const;
    double DoGetMax() const;
    double DoGetIncrement() const;

    void DoSetValue(double val);
    void DoSetValue(const wxString& strValue);
    void DoSetRange(double min_val, double max_val);
    void DoSetIncrement(double inc);

    void GtkDisableEvents() const;
    void GtkEnableEvents() const;

    virtual wxSize DoGetBestSize() const;
    virtual wxSize DoGetSizeFromTextSize(int xlen, int ylen = -1) const;
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    // Widgets that use the style->base colour for the BG colour should
    // override this and return true.
    virtual bool UseGTKStyleBase() const { return true; }

    friend class wxSpinCtrlEventDisabler;

    DECLARE_EVENT_TABLE()
};

//-----------------------------------------------------------------------------
// wxSpinCtrl - An integer valued spin control
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSpinCtrl : public wxSpinCtrlGTKBase
{
public:
    wxSpinCtrl() { Init(); }
    wxSpinCtrl(wxWindow *parent,
               wxWindowID id = wxID_ANY,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxSP_ARROW_KEYS | wxALIGN_RIGHT,
               int min = 0, int max = 100, int initial = 0,
               const wxString& name = wxS("wxSpinCtrl"))
    {
        Init();

        Create(parent, id, value, pos, size, style, min, max, initial, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSP_ARROW_KEYS | wxALIGN_RIGHT,
                int min = 0, int max = 100, int initial = 0,
                const wxString& name = wxS("wxSpinCtrl"))
    {
        return wxSpinCtrlGTKBase::Create(parent, id, value, pos, size,
                                         style, min, max, initial, 1, name);
    }

    // accessors
    int GetValue() const { return int(DoGetValue()); }
    int GetMin() const { return int(DoGetMin()); }
    int GetMax() const { return int(DoGetMax()); }
    int GetIncrement() const { return int(DoGetIncrement()); }

    // operations
    void SetValue(const wxString& value)    { wxSpinCtrlGTKBase::SetValue(value); } // visibility problem w/ gcc
    void SetValue( int value )              { DoSetValue(value); }
    void SetRange( int minVal, int maxVal ) { DoSetRange(minVal, maxVal); }
    void SetIncrement(int inc) { DoSetIncrement(inc); }

    virtual int GetBase() const { return m_base; }
    virtual bool SetBase(int base);

private:
    // Common part of all ctors.
    void Init()
    {
        m_base = 10;
    }

    int m_base;

    DECLARE_DYNAMIC_CLASS(wxSpinCtrl)
};

//-----------------------------------------------------------------------------
// wxSpinCtrlDouble - a double valued spin control
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSpinCtrlDouble : public wxSpinCtrlGTKBase
{
public:
    wxSpinCtrlDouble() {}
    wxSpinCtrlDouble(wxWindow *parent,
                     wxWindowID id = wxID_ANY,
                     const wxString& value = wxEmptyString,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     long style = wxSP_ARROW_KEYS | wxALIGN_RIGHT,
                     double min = 0, double max = 100, double initial = 0,
                     double inc = 1,
                     const wxString& name = wxS("wxSpinCtrlDouble"))
    {
        Create(parent, id, value, pos, size, style,
               min, max, initial, inc, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSP_ARROW_KEYS | wxALIGN_RIGHT,
                double min = 0, double max = 100, double initial = 0,
                double inc = 1,
                const wxString& name = wxS("wxSpinCtrlDouble"))
    {
        return wxSpinCtrlGTKBase::Create(parent, id, value, pos, size,
                                         style, min, max, initial, inc, name);
    }

    // accessors
    double GetValue() const     { return DoGetValue(); }
    double GetMin() const       { return DoGetMin(); }
    double GetMax() const       { return DoGetMax(); }
    double GetIncrement() const { return DoGetIncrement(); }
    unsigned GetDigits() const;

    // operations
    void SetValue(const wxString& value)        { wxSpinCtrlGTKBase::SetValue(value); } // visibility problem w/ gcc
    void SetValue(double value)                 { DoSetValue(value); }
    void SetRange(double minVal, double maxVal) { DoSetRange(minVal, maxVal); }
    void SetIncrement(double inc)               { DoSetIncrement(inc); }
    void SetDigits(unsigned digits);

    virtual int GetBase() const { return 10; }
    virtual bool SetBase(int WXUNUSED(base)) { return false; }

    DECLARE_DYNAMIC_CLASS(wxSpinCtrlDouble)
};

#endif // _WX_GTK_SPINCTRL_H_
