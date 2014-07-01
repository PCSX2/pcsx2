/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/checkbox.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKCHECKBOX_H_
#define _WX_GTKCHECKBOX_H_

// ----------------------------------------------------------------------------
// wxCheckBox
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxCheckBox : public wxCheckBoxBase
{
public:
    wxCheckBox();
    ~wxCheckBox();
    wxCheckBox( wxWindow *parent, wxWindowID id, const wxString& label,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize, long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxCheckBoxNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxCheckBoxNameStr );

    void SetValue( bool state );
    bool GetValue() const;

    virtual void SetLabel( const wxString& label );
    virtual bool Enable( bool enable = true );

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation
    void GTKDisableEvents();
    void GTKEnableEvents();

protected:
    virtual void DoApplyWidgetStyle(GtkRcStyle *style);
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    void DoSet3StateValue(wxCheckBoxState state);
    wxCheckBoxState DoGet3StateValue() const;

private:
    typedef wxCheckBoxBase base_type;

    GtkWidget *m_widgetCheckbox;
    GtkWidget *m_widgetLabel;

    DECLARE_DYNAMIC_CLASS(wxCheckBox)
};

#endif // _WX_GTKCHECKBOX_H_
