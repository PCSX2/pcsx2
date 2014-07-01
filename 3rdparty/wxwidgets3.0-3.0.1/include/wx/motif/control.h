/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/control.h
// Purpose:     wxControl class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CONTROL_H_
#define _WX_CONTROL_H_

#include "wx/window.h"
#include "wx/list.h"
#include "wx/validate.h"

// General item class
class WXDLLIMPEXP_CORE wxControl: public wxControlBase
{
    DECLARE_ABSTRACT_CLASS(wxControl)

public:
    wxControl();
    wxControl( wxWindow *parent,
        wxWindowID id,
        const wxPoint &pos = wxDefaultPosition,
        const wxSize &size = wxDefaultSize,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString &name = wxControlNameStr )
    {
        Create(parent, id, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxControlNameStr);

    // simulates the event, returns true if the event was processed
    virtual void Command(wxCommandEvent& WXUNUSED(event)) { }

    // calls the callback and appropriate event handlers, returns true if
    // event was processed
    virtual bool ProcessCommand(wxCommandEvent& event);

    virtual void SetLabel(const wxString& label);
    virtual wxString GetLabel() const ;

    bool InSetValue() const { return m_inSetValue; }

protected:
    // calls wxControlBase::CreateControl, also sets foreground, background and
    // font to parent's values
    bool CreateControl(wxWindow *parent,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxValidator& validator,
                       const wxString& name);

    // native implementation using XtQueryGeometry
    virtual wxSize DoGetBestSize() const;

    // Motif: prevent callbacks being called while in SetValue
    bool m_inSetValue;

    DECLARE_EVENT_TABLE()
};

#endif // _WX_CONTROL_H_
