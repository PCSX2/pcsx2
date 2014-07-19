/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/radiobox.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_RADIOBOX_H_
#define _WX_GTK_RADIOBOX_H_

#include "wx/bitmap.h"

//-----------------------------------------------------------------------------
// wxRadioBox
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRadioBox : public wxControl,
                                    public wxRadioBoxBase
{
public:
    // ctors and dtor
    wxRadioBox() { Init(); }
    wxRadioBox(wxWindow *parent,
               wxWindowID id,
               const wxString& title,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               int n = 0,
               const wxString choices[] = (const wxString *) NULL,
               int majorDim = 1,
               long style = wxRA_SPECIFY_COLS,
               const wxValidator& val = wxDefaultValidator,
               const wxString& name = wxRadioBoxNameStr)
    {
        Init();

        Create( parent, id, title, pos, size, n, choices, majorDim, style, val, name );
    }

    wxRadioBox(wxWindow *parent,
               wxWindowID id,
               const wxString& title,
               const wxPoint& pos,
               const wxSize& size,
               const wxArrayString& choices,
               int majorDim = 1,
               long style = wxRA_SPECIFY_COLS,
               const wxValidator& val = wxDefaultValidator,
               const wxString& name = wxRadioBoxNameStr)
    {
        Init();

        Create( parent, id, title, pos, size, choices, majorDim, style, val, name );
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0,
                const wxString choices[] = (const wxString *) NULL,
                int majorDim = 0,
                long style = wxRA_SPECIFY_COLS,
                const wxValidator& val = wxDefaultValidator,
                const wxString& name = wxRadioBoxNameStr);
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                int majorDim = 0,
                long style = wxRA_SPECIFY_COLS,
                const wxValidator& val = wxDefaultValidator,
                const wxString& name = wxRadioBoxNameStr);

    virtual ~wxRadioBox();


    // implement wxItemContainerImmutable methods
    virtual unsigned int GetCount() const;

    virtual wxString GetString(unsigned int n) const;
    virtual void SetString(unsigned int n, const wxString& s);

    virtual void SetSelection(int n);
    virtual int GetSelection() const;


    // implement wxRadioBoxBase methods
    virtual bool Show(unsigned int n, bool show = true);
    virtual bool Enable(unsigned int n, bool enable = true);

    virtual bool IsItemEnabled(unsigned int n) const;
    virtual bool IsItemShown(unsigned int n) const;


    // override some base class methods to operate on radiobox itself too
    virtual bool Show( bool show = true );
    virtual bool Enable( bool enable = true );

    virtual void SetLabel( const wxString& label );

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation
    // --------------

    void SetFocus();
    void GtkDisableEvents();
    void GtkEnableEvents();
    bool IsOwnGtkWindow( GdkWindow *window );
    void DoApplyWidgetStyle(GtkRcStyle *style);
#if wxUSE_TOOLTIPS
    void ApplyToolTip( GtkTooltips *tips, const wxChar *tip );
#endif // wxUSE_TOOLTIPS

    virtual void OnInternalIdle();

    bool             m_hasFocus,
                     m_lostFocus;
    wxList           m_boxes;

protected:
    // common part of all ctors
    void Init();

private:
    DECLARE_DYNAMIC_CLASS(wxRadioBox)
};

#endif // _WX_GTK_RADIOBOX_H_
