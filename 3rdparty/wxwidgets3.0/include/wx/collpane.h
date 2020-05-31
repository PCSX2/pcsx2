/////////////////////////////////////////////////////////////////////////////
// Name:        wx/collpane.h
// Purpose:     wxCollapsiblePane
// Author:      Francesco Montorsi
// Modified by:
// Created:     8/10/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLLAPSABLE_PANE_H_BASE_
#define _WX_COLLAPSABLE_PANE_H_BASE_

#include "wx/defs.h"


#if wxUSE_COLLPANE

#include "wx/control.h"

// class name
extern WXDLLIMPEXP_DATA_CORE(const char) wxCollapsiblePaneNameStr[];

// ----------------------------------------------------------------------------
// wxCollapsiblePaneBase: interface for wxCollapsiblePane
// ----------------------------------------------------------------------------

#define wxCP_DEFAULT_STYLE          (wxTAB_TRAVERSAL | wxNO_BORDER)
#define wxCP_NO_TLW_RESIZE          (0x0002)

class WXDLLIMPEXP_CORE wxCollapsiblePaneBase : public wxControl
{
public:
    wxCollapsiblePaneBase() {}

    virtual void Collapse(bool collapse = true) = 0;
    void Expand() { Collapse(false); }

    virtual bool IsCollapsed() const = 0;
    bool IsExpanded() const { return !IsCollapsed(); }

    virtual wxWindow *GetPane() const = 0;

    virtual wxString GetLabel() const = 0;
    virtual void SetLabel(const wxString& label) = 0;
};


// ----------------------------------------------------------------------------
// event types and macros
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxCollapsiblePaneEvent;

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_COLLAPSIBLEPANE_CHANGED, wxCollapsiblePaneEvent );

class WXDLLIMPEXP_CORE wxCollapsiblePaneEvent : public wxCommandEvent
{
public:
    wxCollapsiblePaneEvent() {}
    wxCollapsiblePaneEvent(wxObject *generator, int id, bool collapsed)
        : wxCommandEvent(wxEVT_COLLAPSIBLEPANE_CHANGED, id),
        m_bCollapsed(collapsed)
    {
        SetEventObject(generator);
    }

    bool GetCollapsed() const { return m_bCollapsed; }
    void SetCollapsed(bool c) { m_bCollapsed = c; }


    // default copy ctor, assignment operator and dtor are ok
    virtual wxEvent *Clone() const { return new wxCollapsiblePaneEvent(*this); }

private:
    bool m_bCollapsed;

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxCollapsiblePaneEvent)
};

// ----------------------------------------------------------------------------
// event types and macros
// ----------------------------------------------------------------------------

typedef void (wxEvtHandler::*wxCollapsiblePaneEventFunction)(wxCollapsiblePaneEvent&);

#define wxCollapsiblePaneEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxCollapsiblePaneEventFunction, func)

#define EVT_COLLAPSIBLEPANE_CHANGED(id, fn) \
    wx__DECLARE_EVT1(wxEVT_COLLAPSIBLEPANE_CHANGED, id, wxCollapsiblePaneEventHandler(fn))


#if defined(__WXGTK20__) && !defined(__WXUNIVERSAL__)
    #include "wx/gtk/collpane.h"
#else
    #include "wx/generic/collpaneg.h"

    // use #define and not a typedef to allow forward declaring the class
    #define wxCollapsiblePane wxGenericCollapsiblePane
#endif

// old wxEVT_COMMAND_* constant
#define wxEVT_COMMAND_COLLPANE_CHANGED   wxEVT_COLLAPSIBLEPANE_CHANGED

#endif // wxUSE_COLLPANE

#endif // _WX_COLLAPSABLE_PANE_H_BASE_
