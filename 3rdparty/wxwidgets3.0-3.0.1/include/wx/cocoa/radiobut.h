/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/radiobut.h
// Purpose:     wxRadioButton class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/18
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_RADIOBUT_H__
#define __WX_COCOA_RADIOBUT_H__

#include "wx/cocoa/NSButton.h"

class WXDLLIMPEXP_FWD_CORE wxRadioButton;

WX_DECLARE_EXPORTED_LIST(wxRadioButton, wxRadioButtonList);

// ========================================================================
// wxRadioButton
// ========================================================================
class WXDLLIMPEXP_CORE wxRadioButton: public wxControl, protected wxCocoaNSButton
{
    DECLARE_DYNAMIC_CLASS(wxRadioButton)
    DECLARE_EVENT_TABLE()
    WX_DECLARE_COCOA_OWNER(NSButton,NSControl,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxRadioButton() { m_radioMaster = NULL; }
    wxRadioButton(wxWindow *parent, wxWindowID winid,
            const wxString& label,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxRadioButtonNameStr)
    {
        Create(parent, winid, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxString& label,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxRadioButtonNameStr);
    virtual ~wxRadioButton();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
    virtual void Cocoa_wxNSButtonAction(void);
private:
    void Cocoa_DeselectOtherButtonsInTheGroup(void);
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    virtual void SetValue(bool);
    virtual bool GetValue() const;
    virtual void SetLabel(const wxString& label);
    virtual wxString GetLabel() const;
protected:
    wxRadioButtonList m_radioSlaves;
    wxRadioButton *m_radioMaster;
};

#endif // __WX_COCOA_RADIOBUT_H__
