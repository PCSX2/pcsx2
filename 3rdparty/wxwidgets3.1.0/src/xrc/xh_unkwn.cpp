/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_unkwn.cpp
// Purpose:     XRC resource for unknown widget
// Author:      Vaclav Slavik
// Created:     2000/09/09
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC

#include "wx/xrc/xh_unkwn.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/window.h"
    #include "wx/panel.h"
#endif


class wxUnknownControlContainer : public wxPanel
{
public:
    wxUnknownControlContainer(wxWindow *parent,
                              const wxString& controlName,
                              wxWindowID id = wxID_ANY,
                              const wxPoint& pos = wxDefaultPosition,
                              const wxSize& size = wxDefaultSize,
                              long style = 0)
        // Always add the wxTAB_TRAVERSAL and wxNO_BORDER styles to what comes
        // from the XRC if anything.
        : wxPanel(parent, id, pos, size, style | wxTAB_TRAVERSAL | wxNO_BORDER,
                  controlName + wxT("_container")),
          m_controlName(controlName),
          m_control(NULL)
    {
        m_bg = GetBackgroundColour();
        SetBackgroundColour(wxColour(255, 0, 255));
    }

    virtual void AddChild(wxWindowBase *child) wxOVERRIDE;
    virtual void RemoveChild(wxWindowBase *child) wxOVERRIDE;


    // Ensure that setting the min or max size both for this window itself (as
    // happens when the XRC contains the corresponding elements) or for the
    // control contained in it works as expected, i.e. the larger/smaller of
    // the sizes is used to satisfy both windows invariants.

    virtual wxSize GetMinSize() const wxOVERRIDE
    {
        wxSize size = wxPanel::GetMinSize();
        if ( m_control )
            size.IncTo(m_control->GetMinSize());

        return size;
    }

    virtual wxSize GetMaxSize() const wxOVERRIDE
    {
        wxSize size = wxPanel::GetMaxSize();
        if ( m_control )
            size.DecToIfSpecified(m_control->GetMaxSize());

        return size;
    }

protected:
    virtual wxSize DoGetBestClientSize() const wxOVERRIDE
    {
        // We don't have any natural best size when we're empty, so just return
        // the minimal valid size in this case.
        return m_control ? m_control->GetBestSize() : wxSize(1, 1);
    }

private:
    void OnSize(wxSizeEvent& event)
    {
        if ( m_control )
            m_control->SetSize(wxRect(event.GetSize()));
    }

    wxString m_controlName;
    wxWindowBase *m_control;
    wxColour m_bg;

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(wxUnknownControlContainer, wxPanel)
    EVT_SIZE(wxUnknownControlContainer::OnSize)
wxEND_EVENT_TABLE()

void wxUnknownControlContainer::AddChild(wxWindowBase *child)
{
    wxASSERT_MSG( !m_control, wxT("Couldn't add two unknown controls to the same container!") );

    wxPanel::AddChild(child);

    SetBackgroundColour(m_bg);
    child->SetName(m_controlName);
    child->SetId(wxXmlResource::GetXRCID(m_controlName));
    m_control = child;

    InvalidateBestSize();
    child->SetSize(wxRect(GetClientSize()));
}

void wxUnknownControlContainer::RemoveChild(wxWindowBase *child)
{
    wxPanel::RemoveChild(child);
    m_control = NULL;

    InvalidateBestSize();
}


wxIMPLEMENT_DYNAMIC_CLASS(wxUnknownWidgetXmlHandler, wxXmlResourceHandler);

wxUnknownWidgetXmlHandler::wxUnknownWidgetXmlHandler()
: wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxNO_FULL_REPAINT_ON_RESIZE);
}

wxObject *wxUnknownWidgetXmlHandler::DoCreateResource()
{
    wxASSERT_MSG( m_instance == NULL,
                  wxT("'unknown' controls can't be subclassed, use wxXmlResource::AttachUnknownControl") );

    wxPanel *panel =
        new wxUnknownControlContainer(m_parentAsWindow,
                                      GetName(), wxID_ANY,
                                      GetPosition(), GetSize(),
                                      GetStyle(wxT("style")));
    SetupWindow(panel);
    return panel;
}

bool wxUnknownWidgetXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("unknown"));
}

#endif // wxUSE_XRC
