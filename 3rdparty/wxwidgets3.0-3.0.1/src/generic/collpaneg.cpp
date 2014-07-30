/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/collpaneg.cpp
// Purpose:     wxGenericCollapsiblePane
// Author:      Francesco Montorsi
// Modified By:
// Created:     8/10/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/defs.h"

#if wxUSE_COLLPANE && wxUSE_BUTTON && wxUSE_STATLINE

#include "wx/collpane.h"

#ifndef WX_PRECOMP
    #include "wx/toplevel.h"
    #include "wx/button.h"
    #include "wx/sizer.h"
    #include "wx/panel.h"
#endif // !WX_PRECOMP

#include "wx/statline.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// ============================================================================
// implementation
// ============================================================================

const char wxCollapsiblePaneNameStr[] = "collapsiblePane";

//-----------------------------------------------------------------------------
// wxGenericCollapsiblePane
//-----------------------------------------------------------------------------

wxDEFINE_EVENT( wxEVT_COLLAPSIBLEPANE_CHANGED, wxCollapsiblePaneEvent );
IMPLEMENT_DYNAMIC_CLASS(wxGenericCollapsiblePane, wxControl)
IMPLEMENT_DYNAMIC_CLASS(wxCollapsiblePaneEvent, wxCommandEvent)

BEGIN_EVENT_TABLE(wxGenericCollapsiblePane, wxControl)
    EVT_BUTTON(wxID_ANY, wxGenericCollapsiblePane::OnButton)
    EVT_SIZE(wxGenericCollapsiblePane::OnSize)
END_EVENT_TABLE()

void wxGenericCollapsiblePane::Init()
{
    m_pButton = NULL;
    m_pPane = NULL;
    m_pStaticLine = NULL;
    m_sz = NULL;
}

bool wxGenericCollapsiblePane::Create(wxWindow *parent,
                                      wxWindowID id,
                                      const wxString& label,
                                      const wxPoint& pos,
                                      const wxSize& size,
                                      long style,
                                      const wxValidator& val,
                                      const wxString& name)
{
    if ( !wxControl::Create(parent, id, pos, size, style, val, name) )
        return false;

    m_strLabel = label;

    // sizer containing the expand button and possibly a static line
    m_sz = new wxBoxSizer(wxHORIZONTAL);

#if defined( __WXMAC__ ) && !defined(__WXUNIVERSAL__)
    // on Mac we use the special disclosure triangle button
    m_pStaticLine = NULL;
    m_pButton = new wxDisclosureTriangle(this, wxID_ANY, GetBtnLabel());
    m_sz->Add(m_pButton);
#else
    // create children and lay them out using a wxBoxSizer
    // (so that we automatically get RTL features)
    m_pButton = new wxButton(this, wxID_ANY, GetBtnLabel(), wxPoint(0, 0),
                             wxDefaultSize, wxBU_EXACTFIT);
    m_pStaticLine = new wxStaticLine(this, wxID_ANY);

    // on other platforms we put the static line and the button horizontally
    m_sz->Add(m_pButton, 0, wxLEFT|wxTOP|wxBOTTOM, GetBorder());
    m_sz->Add(m_pStaticLine, 1, wxALIGN_CENTER|wxLEFT|wxRIGHT, GetBorder());
#endif

    // FIXME: at least under wxCE and wxGTK1 the background is black if we don't do
    //        this, no idea why...
#if defined(__WXWINCE__) || defined(__WXGTK__)
    SetBackgroundColour(parent->GetBackgroundColour());
#endif

    // do not set sz as our sizers since we handle the pane window without using sizers
    m_pPane = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                          wxTAB_TRAVERSAL|wxNO_BORDER, wxT("wxCollapsiblePanePane") );

    // start as collapsed:
    m_pPane->Hide();

    return true;
}

wxGenericCollapsiblePane::~wxGenericCollapsiblePane()
{
    if (m_pButton)
        m_pButton->SetContainingSizer(NULL);

    if (m_pStaticLine)
        m_pStaticLine->SetContainingSizer(NULL);

    // our sizer is not deleted automatically since we didn't use SetSizer()!
    wxDELETE(m_sz);
}

wxSize wxGenericCollapsiblePane::DoGetBestSize() const
{
    // NB: do not use GetSize() but rather GetMinSize()
    wxSize sz = m_sz->GetMinSize();

    // when expanded, we need more vertical space
    if ( IsExpanded() )
    {
        sz.SetWidth(wxMax( sz.GetWidth(), m_pPane->GetBestSize().x ));
        sz.SetHeight(sz.y + GetBorder() + m_pPane->GetBestSize().y);
    }

    return sz;
}

wxString wxGenericCollapsiblePane::GetBtnLabel() const
{
    // on mac the triangle indicates the state, no string change
#ifdef __WXMAC__
    return m_strLabel;
#else
    return m_strLabel + (IsCollapsed() ? wxT(" >>") : wxT(" <<"));
#endif
}

void wxGenericCollapsiblePane::OnStateChange(const wxSize& sz)
{
    // minimal size has priority over the best size so set here our min size
//    SetMinSize(sz);
    SetSize(sz);

    if (this->HasFlag(wxCP_NO_TLW_RESIZE))
    {
        // the user asked to explicitly handle the resizing itself...
        return;
    }


    wxTopLevelWindow *top =
        wxDynamicCast(wxGetTopLevelParent(this), wxTopLevelWindow);
    if ( !top )
        return;

    wxSizer *sizer = top->GetSizer();
    if ( !sizer )
        return;

    const wxSize newBestSize = sizer->ComputeFittingClientSize(top);
    top->SetMinClientSize(newBestSize);

    // we shouldn't attempt to resize a maximized window, whatever happens
    if ( !top->IsMaximized() )
        top->SetClientSize(newBestSize);
}

void wxGenericCollapsiblePane::Collapse(bool collapse)
{
    // optimization
    if ( IsCollapsed() == collapse )
        return;

    InvalidateBestSize();

    // update our state
    m_pPane->Show(!collapse);

    // update button label
#if defined( __WXMAC__ ) && !defined(__WXUNIVERSAL__)
    m_pButton->SetOpen( !collapse );
#else
    // NB: this must be done after updating our "state"
    m_pButton->SetLabel(GetBtnLabel());
#endif


    OnStateChange(GetBestSize());
}

void wxGenericCollapsiblePane::SetLabel(const wxString &label)
{
    m_strLabel = label;
#ifdef __WXMAC__
    m_pButton->SetLabel(GetBtnLabel());
#else
    m_pButton->SetLabel(GetBtnLabel());
    m_pButton->SetInitialSize();
#endif

    Layout();
}

bool wxGenericCollapsiblePane::Layout()
{
#ifdef __WXMAC__
    if (!m_pButton || !m_pPane || !m_sz)
        return false;     // we need to complete the creation first!
#else
    if (!m_pButton || !m_pStaticLine || !m_pPane || !m_sz)
        return false;     // we need to complete the creation first!
#endif

    wxSize oursz(GetSize());

    // move & resize the button and the static line
    m_sz->SetDimension(0, 0, oursz.GetWidth(), m_sz->GetMinSize().GetHeight());
    m_sz->Layout();

    if ( IsExpanded() )
    {
        // move & resize the container window
        int yoffset = m_sz->GetSize().GetHeight() + GetBorder();
        m_pPane->SetSize(0, yoffset,
                        oursz.x, oursz.y - yoffset);

        // this is very important to make the pane window layout show correctly
        m_pPane->Layout();
    }

    return true;
}

int wxGenericCollapsiblePane::GetBorder() const
{
#if defined( __WXMAC__ )
    return 6;
#elif defined(__WXMSW__)
    wxASSERT(m_pButton);
    return m_pButton->ConvertDialogToPixels(wxSize(2, 0)).x;
#else
    return 5;
#endif
}



//-----------------------------------------------------------------------------
// wxGenericCollapsiblePane - event handlers
//-----------------------------------------------------------------------------

void wxGenericCollapsiblePane::OnButton(wxCommandEvent& event)
{
    if ( event.GetEventObject() != m_pButton )
    {
        event.Skip();
        return;
    }

    Collapse(!IsCollapsed());

    // this change was generated by the user - send the event
    wxCollapsiblePaneEvent ev(this, GetId(), IsCollapsed());
    GetEventHandler()->ProcessEvent(ev);
}

void wxGenericCollapsiblePane::OnSize(wxSizeEvent& WXUNUSED(event))
{
#if 0       // for debug only
    wxClientDC dc(this);
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(wxPoint(0,0), GetSize());
    dc.SetPen(*wxRED_PEN);
    dc.DrawRectangle(wxPoint(0,0), GetBestSize());
#endif

    Layout();
}

#endif // wxUSE_COLLPANE && wxUSE_BUTTON && wxUSE_STATLINE
