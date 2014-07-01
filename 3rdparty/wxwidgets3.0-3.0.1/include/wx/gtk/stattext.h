/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/stattext.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_STATTEXT_H_
#define _WX_GTK_STATTEXT_H_

//-----------------------------------------------------------------------------
// wxStaticText
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStaticText : public wxStaticTextBase
{
public:
    wxStaticText();
    wxStaticText(wxWindow *parent,
                 wxWindowID id,
                 const wxString &label,
                 const wxPoint &pos = wxDefaultPosition,
                 const wxSize &size = wxDefaultSize,
                 long style = 0,
                 const wxString &name = wxStaticTextNameStr );

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString &label,
                const wxPoint &pos = wxDefaultPosition,
                const wxSize &size = wxDefaultSize,
                long style = 0,
                const wxString &name = wxStaticTextNameStr );

    void SetLabel( const wxString &label );

    bool SetFont( const wxFont &font );

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation
    // --------------

protected:
    virtual bool GTKWidgetNeedsMnemonic() const;
    virtual void GTKWidgetDoSetMnemonic(GtkWidget* w);

    virtual wxSize DoGetBestSize() const;

    virtual wxString DoGetLabel() const;
    virtual void DoSetLabel(const wxString& str);
#if wxUSE_MARKUP
    virtual bool DoSetLabelMarkup(const wxString& markup);
#endif // wxUSE_MARKUP

private:
    // Common part of SetLabel() and DoSetLabelMarkup().
    typedef void (wxStaticText::*GTKLabelSetter)(GtkLabel *, const wxString&);

    void GTKDoSetLabel(GTKLabelSetter setter, const wxString& label);


    DECLARE_DYNAMIC_CLASS(wxStaticText)
};

#endif
    // _WX_GTK_STATTEXT_H_
