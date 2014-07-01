/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/radiobut.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_RADIOBUT_H_
#define _WX_GTK_RADIOBUT_H_

//-----------------------------------------------------------------------------
// wxRadioButton
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRadioButton: public wxControl
{
public:
    wxRadioButton() { }
    wxRadioButton( wxWindow *parent,
                   wxWindowID id,
                   const wxString& label,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = 0,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxRadioButtonNameStr )
    {
        Create( parent, id, label, pos, size, style, validator, name );
    }

    bool Create( wxWindow *parent,
                 wxWindowID id,
                 const wxString& label,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = 0,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString& name = wxRadioButtonNameStr );

    virtual void SetLabel(const wxString& label);
    virtual void SetValue(bool val);
    virtual bool GetValue() const;
    virtual bool Enable( bool enable = true );

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

protected:
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }

    virtual void DoApplyWidgetStyle(GtkRcStyle *style);
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

private:
    typedef wxControl base_type;

    DECLARE_DYNAMIC_CLASS(wxRadioButton)
};

#endif // _WX_GTK_RADIOBUT_H_
