/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/collpaneg.h
// Purpose:     wxGenericCollapsiblePane
// Author:      Francesco Montorsi
// Modified by:
// Created:     8/10/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLLAPSABLE_PANE_H_GENERIC_
#define _WX_COLLAPSABLE_PANE_H_GENERIC_

// forward declared
class WXDLLIMPEXP_FWD_CORE wxButton;
class WXDLLIMPEXP_FWD_CORE wxStaticLine;
#if defined( __WXMAC__ ) && !defined(__WXUNIVERSAL__)
class WXDLLIMPEXP_FWD_CORE wxDisclosureTriangle;
#endif

#include "wx/containr.h"

// ----------------------------------------------------------------------------
// wxGenericCollapsiblePane
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericCollapsiblePane :
    public wxNavigationEnabled<wxCollapsiblePaneBase>
{
public:
    wxGenericCollapsiblePane() { Init(); }

    wxGenericCollapsiblePane(wxWindow *parent,
                        wxWindowID winid,
                        const wxString& label,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = wxCP_DEFAULT_STYLE,
                        const wxValidator& val = wxDefaultValidator,
                        const wxString& name = wxCollapsiblePaneNameStr)
    {
        Init();

        Create(parent, winid, label, pos, size, style, val, name);
    }

    virtual ~wxGenericCollapsiblePane();

    bool Create(wxWindow *parent,
                wxWindowID winid,
                const wxString& label,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxCP_DEFAULT_STYLE,
                const wxValidator& val = wxDefaultValidator,
                const wxString& name = wxCollapsiblePaneNameStr);

    // public wxCollapsiblePane API
    virtual void Collapse(bool collapse = true);
    virtual void SetLabel(const wxString &label);

    virtual bool IsCollapsed() const
        { return m_pPane==NULL || !m_pPane->IsShown(); }
    virtual wxWindow *GetPane() const
        { return m_pPane; }
    virtual wxString GetLabel() const
        { return m_strLabel; }

    virtual bool Layout();


    // for the generic collapsible pane only:
    wxControl* GetControlWidget() const
        { return (wxControl*)m_pButton; }

    // implementation only, don't use
    void OnStateChange(const wxSize& sizeNew);

protected:
    // overridden methods
    virtual wxSize DoGetBestSize() const;

    wxString GetBtnLabel() const;
    int GetBorder() const;

    // child controls
#if defined( __WXMAC__ ) && !defined(__WXUNIVERSAL__)
    wxDisclosureTriangle *m_pButton;
#else
    wxButton *m_pButton;
#endif
    wxStaticLine *m_pStaticLine;
    wxWindow *m_pPane;
    wxSizer *m_sz;

    // the button label without ">>" or "<<"
    wxString m_strLabel;

private:
    void Init();

    // event handlers
    void OnButton(wxCommandEvent &ev);
    void OnSize(wxSizeEvent &ev);

    DECLARE_DYNAMIC_CLASS(wxGenericCollapsiblePane)
    DECLARE_EVENT_TABLE()
};

#endif // _WX_COLLAPSABLE_PANE_H_GENERIC_
