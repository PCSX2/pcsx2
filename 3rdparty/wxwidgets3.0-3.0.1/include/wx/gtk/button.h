/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/button.h
// Purpose:     wxGTK wxButton class declaration
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_BUTTON_H_
#define _WX_GTK_BUTTON_H_

//-----------------------------------------------------------------------------
// wxButton
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxButton : public wxButtonBase
{
public:
    wxButton() {}
    wxButton(wxWindow *parent, wxWindowID id,
           const wxString& label = wxEmptyString,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxButtonNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
           const wxString& label = wxEmptyString,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxButtonNameStr);

    virtual wxWindow *SetDefault();
    virtual void SetLabel( const wxString &label );

    // implementation
    // --------------

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // helper to allow access to protected member from GTK callback
    void MoveWindow(int x, int y, int width, int height) { DoMoveWindow(x, y, width, height); }

    // called from GTK callbacks: they update the button state and call
    // GTKUpdateBitmap()
    void GTKMouseEnters();
    void GTKMouseLeaves();
    void GTKPressed();
    void GTKReleased();

protected:
    virtual wxSize DoGetBestSize() const;
    virtual void DoApplyWidgetStyle(GtkRcStyle *style);

#if wxUSE_MARKUP
    virtual bool DoSetLabelMarkup(const wxString& markup);
#endif // wxUSE_MARKUP

private:
    typedef wxButtonBase base_type;

    // Return the GtkLabel used by this button.
    GtkLabel *GTKGetLabel() const;

    DECLARE_DYNAMIC_CLASS(wxButton)
};

#endif // _WX_GTK_BUTTON_H_
