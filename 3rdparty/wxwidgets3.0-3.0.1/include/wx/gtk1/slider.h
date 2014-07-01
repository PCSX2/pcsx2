/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/slider.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKSLIDERH__
#define __GTKSLIDERH__

// ----------------------------------------------------------------------------
// wxSlider
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSlider : public wxSliderBase
{
public:
    wxSlider() { }
    wxSlider(wxWindow *parent,
             wxWindowID id,
             int value, int minValue, int maxValue,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxSL_HORIZONTAL,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxSliderNameStr)
    {
        Create( parent, id, value, minValue, maxValue,
                pos, size, style, validator, name );
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                int value, int minValue, int maxValue,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSL_HORIZONTAL,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxSliderNameStr);

    // implement the base class pure virtuals
    virtual int GetValue() const;
    virtual void SetValue(int value);

    virtual void SetRange(int minValue, int maxValue);
    virtual int GetMin() const;
    virtual int GetMax() const;

    virtual void SetLineSize(int lineSize);
    virtual void SetPageSize(int pageSize);
    virtual int GetLineSize() const;
    virtual int GetPageSize() const;

    virtual void SetThumbLength(int lenPixels);
    virtual int GetThumbLength() const;

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation
    bool IsOwnGtkWindow( GdkWindow *window );
    void GtkDisableEvents();
    void GtkEnableEvents();

    GtkAdjustment  *m_adjust;
    float           m_oldPos;

private:
    DECLARE_DYNAMIC_CLASS(wxSlider)
};

#endif // __GTKSLIDERH__
