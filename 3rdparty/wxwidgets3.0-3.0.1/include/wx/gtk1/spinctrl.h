/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/spinctrl.h
// Purpose:     wxSpinCtrl class
// Author:      Robert Roebling
// Modified by:
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKSPINCTRLH__
#define __GTKSPINCTRLH__

#include "wx/defs.h"

#if wxUSE_SPINCTRL

#include "wx/control.h"

//-----------------------------------------------------------------------------
// wxSpinCtrl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSpinCtrl : public wxControl
{
public:
    wxSpinCtrl() {}
    wxSpinCtrl(wxWindow *parent,
               wxWindowID id = -1,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxSP_ARROW_KEYS,
               int min = 0, int max = 100, int initial = 0,
               const wxString& name = wxT("wxSpinCtrl"))
    {
        Create(parent, id, value, pos, size, style, min, max, initial, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id = -1,
                const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSP_ARROW_KEYS,
                int min = 0, int max = 100, int initial = 0,
                const wxString& name = wxT("wxSpinCtrl"));

    void SetValue(const wxString& text);
    void SetSelection(long from, long to);

    virtual int GetValue() const;
    virtual void SetValue( int value );
    virtual void SetRange( int minVal, int maxVal );
    virtual int GetMin() const;
    virtual int GetMax() const;

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation
    void OnChar( wxKeyEvent &event );

    bool IsOwnGtkWindow( GdkWindow *window );
    void GtkDisableEvents();
    void GtkEnableEvents();

    GtkAdjustment  *m_adjust;
    float           m_oldPos;

    virtual int GetBase() const { return m_base; }
    virtual bool SetBase(int base);

 protected:
    virtual wxSize DoGetBestSize() const;

    // Widgets that use the style->base colour for the BG colour should
    // override this and return true.
    virtual bool UseGTKStyleBase() const { return true; }

private:
    // Common part of all ctors.
    void Init()
    {
        m_base = 10;
    }

    int m_base;

    DECLARE_DYNAMIC_CLASS(wxSpinCtrl)
    DECLARE_EVENT_TABLE()
};

#endif
    // wxUSE_SPINCTRL

#endif
    // __GTKSPINCTRLH__
