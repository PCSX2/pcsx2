/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/mdig.cpp
// Purpose:     Generic MDI (Multiple Document Interface) classes
// Author:      Hans Van Leemputten
// Modified by: 2008-10-31 Vadim Zeitlin: derive from the base classes
// Created:     29/07/2002
// Copyright:   (c) 2002 Hans Van Leemputten
//              (c) 2008 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MDI

#ifndef WX_PRECOMP
    #include "wx/menu.h"
    #include "wx/intl.h"
    #include "wx/log.h"
#endif //WX_PRECOMP

#include "wx/mdi.h"
#include "wx/generic/mdig.h"
#include "wx/notebook.h"
#include "wx/scopeguard.h"

#include "wx/stockitem.h"

enum MDI_MENU_ID
{
    wxWINDOWCLOSE = 4001,
    wxWINDOWCLOSEALL,
    wxWINDOWNEXT,
    wxWINDOWPREV
};

//-----------------------------------------------------------------------------
// wxGenericMDIParentFrame
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxGenericMDIParentFrame, wxFrame)

BEGIN_EVENT_TABLE(wxGenericMDIParentFrame, wxFrame)
    EVT_CLOSE(wxGenericMDIParentFrame::OnClose)
#if wxUSE_MENUS
    EVT_MENU(wxID_ANY, wxGenericMDIParentFrame::OnWindowMenu)
#endif
END_EVENT_TABLE()

void wxGenericMDIParentFrame::Init()
{
#if wxUSE_MENUS
    m_pMyMenuBar = NULL;
#endif // wxUSE_MENUS
}

wxGenericMDIParentFrame::~wxGenericMDIParentFrame()
{
    // Make sure the client window is destructed before the menu bars are!
    wxDELETE(m_clientWindow);

#if wxUSE_MENUS
    wxDELETE(m_pMyMenuBar);

    RemoveWindowMenu(GetMenuBar());
#endif // wxUSE_MENUS
}

bool wxGenericMDIParentFrame::Create(wxWindow *parent,
                              wxWindowID id,
                              const wxString& title,
                              const wxPoint& pos,
                              const wxSize& size,
                              long style,
                              const wxString& name)
{
    // this style can be used to prevent a window from having the standard MDI
    // "Window" menu
    if ( !(style & wxFRAME_NO_WINDOW_MENU) )
    {
#if wxUSE_MENUS
        m_windowMenu = new wxMenu;

        m_windowMenu->Append(wxWINDOWCLOSE,    _("Cl&ose"));
        m_windowMenu->Append(wxWINDOWCLOSEALL, _("Close All"));
        m_windowMenu->AppendSeparator();
        m_windowMenu->Append(wxWINDOWNEXT,     _("&Next"));
        m_windowMenu->Append(wxWINDOWPREV,     _("&Previous"));
#endif // wxUSE_MENUS
    }

    // the scrolling styles don't make sense neither for us nor for our client
    // window (to which they're supposed to apply)
    style &= ~(wxHSCROLL | wxVSCROLL);

    if ( !wxFrame::Create( parent, id, title, pos, size, style, name ) )
        return false;

    wxGenericMDIClientWindow * const client = OnCreateGenericClient();
    if ( !client->CreateGenericClient(this) )
        return false;

    m_clientWindow = client;

    return true;
}

wxGenericMDIClientWindow *wxGenericMDIParentFrame::OnCreateGenericClient()
{
    return new wxGenericMDIClientWindow;
}

bool wxGenericMDIParentFrame::CloseAll()
{
    wxGenericMDIClientWindow * const client = GetGenericClientWindow();
    if ( !client )
        return true; // none of the windows left

    wxBookCtrlBase * const book = client->GetBookCtrl();
    while ( book->GetPageCount() )
    {
        wxGenericMDIChildFrame * const child = client->GetChild(0);
        if ( !child->Close() )
        {
            // it refused to close, don't close the remaining ones neither
            return false;
        }
    }

    return true;
}

#if wxUSE_MENUS
void wxGenericMDIParentFrame::SetWindowMenu(wxMenu* pMenu)
{
    // Replace the window menu from the currently loaded menu bar.
    wxMenuBar *pMenuBar = GetMenuBar();

    if (m_windowMenu)
    {
        RemoveWindowMenu(pMenuBar);

        wxDELETE(m_windowMenu);
    }

    if (pMenu)
    {
        m_windowMenu = pMenu;

        AddWindowMenu(pMenuBar);
    }
}

void wxGenericMDIParentFrame::SetMenuBar(wxMenuBar *pMenuBar)
{
    // Remove the Window menu from the old menu bar
    RemoveWindowMenu(GetMenuBar());
    // Add the Window menu to the new menu bar.
    AddWindowMenu(pMenuBar);

    wxFrame::SetMenuBar(pMenuBar);
}
#endif // wxUSE_MENUS

void wxGenericMDIParentFrame::WXSetChildMenuBar(wxGenericMDIChildFrame *pChild)
{
#if wxUSE_MENUS
    if (pChild  == NULL)
    {
        // No Child, set Our menu bar back.
        SetMenuBar(m_pMyMenuBar);

        // Make sure we know our menu bar is in use
        m_pMyMenuBar = NULL;
    }
    else
    {
        if (pChild->GetMenuBar() == NULL)
            return;

        // Do we need to save the current bar?
        if (m_pMyMenuBar == NULL)
            m_pMyMenuBar = GetMenuBar();

        SetMenuBar(pChild->GetMenuBar());
    }
#endif // wxUSE_MENUS
}

wxGenericMDIClientWindow *
wxGenericMDIParentFrame::GetGenericClientWindow() const
{
    return static_cast<wxGenericMDIClientWindow *>(m_clientWindow);
}

wxBookCtrlBase *wxGenericMDIParentFrame::GetBookCtrl() const
{
    wxGenericMDIClientWindow * const client = GetGenericClientWindow();
    return client ? client->GetBookCtrl() : NULL;
}

void wxGenericMDIParentFrame::AdvanceActive(bool forward)
{
    wxBookCtrlBase * const book = GetBookCtrl();
    if ( book )
        book->AdvanceSelection(forward);
}

void wxGenericMDIParentFrame::WXUpdateChildTitle(wxGenericMDIChildFrame *child)
{
    wxGenericMDIClientWindow * const client = GetGenericClientWindow();

    const int pos = client->FindChild(child);
    if ( pos == wxNOT_FOUND )
        return;

    client->GetBookCtrl()->SetPageText(pos, child->GetTitle());
}

void wxGenericMDIParentFrame::WXActivateChild(wxGenericMDIChildFrame *child)
{
    wxGenericMDIClientWindow * const client = GetGenericClientWindow();

    const int pos = client->FindChild(child);
    if ( pos == wxNOT_FOUND )
        return;

    client->GetBookCtrl()->SetSelection(pos);
}

void wxGenericMDIParentFrame::WXRemoveChild(wxGenericMDIChildFrame *child)
{
    const bool removingActive = WXIsActiveChild(child);
    if ( removingActive )
    {
        SetActiveChild(NULL);
        WXSetChildMenuBar(NULL);
    }

    wxGenericMDIClientWindow * const client = GetGenericClientWindow();
    wxCHECK_RET( client, "should have client window" );

    wxBookCtrlBase * const book = client->GetBookCtrl();

    // Remove page if still there
    int pos = client->FindChild(child);
    if ( pos != wxNOT_FOUND )
    {
        if ( book->RemovePage(pos) )
            book->Refresh();
    }

    if ( removingActive )
    {
        // Set the new selection to a remaining page
        const size_t count = book->GetPageCount();
        if ( count > (size_t)pos )
        {
            book->SetSelection(pos);
        }
        else
        {
            if ( count > 0 )
                book->SetSelection(count - 1);
        }
    }
}

bool
wxGenericMDIParentFrame::WXIsActiveChild(wxGenericMDIChildFrame *child) const
{
    return static_cast<wxMDIChildFrameBase *>(GetActiveChild()) == child;
}

#if wxUSE_MENUS
void wxGenericMDIParentFrame::RemoveWindowMenu(wxMenuBar *pMenuBar)
{
    if (pMenuBar && m_windowMenu)
    {
        // Remove old window menu
        int pos = pMenuBar->FindMenu(_("&Window"));
        if (pos != wxNOT_FOUND)
        {
            wxASSERT(m_windowMenu == pMenuBar->GetMenu(pos)); // DBG:: We're going to delete the wrong menu!!!
            pMenuBar->Remove(pos);
        }
    }
}

void wxGenericMDIParentFrame::AddWindowMenu(wxMenuBar *pMenuBar)
{
    if (pMenuBar && m_windowMenu)
    {
        int pos = pMenuBar->FindMenu(wxGetStockLabel(wxID_HELP,false));
        if (pos == wxNOT_FOUND)
        {
            pMenuBar->Append(m_windowMenu, _("&Window"));
        }
        else
        {
            pMenuBar->Insert(pos, m_windowMenu, _("&Window"));
        }
    }
}

void wxGenericMDIParentFrame::OnWindowMenu(wxCommandEvent &event)
{
    switch ( event.GetId() )
    {
        case wxWINDOWCLOSE:
            if ( m_currentChild )
                m_currentChild->Close();
            break;

        case wxWINDOWCLOSEALL:
            CloseAll();
            break;

        case wxWINDOWNEXT:
            ActivateNext();
            break;

        case wxWINDOWPREV:
            ActivatePrevious();
            break;

        default:
            event.Skip();
    }
}
#endif // wxUSE_MENUS

void wxGenericMDIParentFrame::OnClose(wxCloseEvent& event)
{
    if ( !CloseAll() )
        event.Veto();
    else
        event.Skip();
}

bool wxGenericMDIParentFrame::ProcessEvent(wxEvent& event)
{
    if ( m_currentChild )
    {
        // the menu events should be given to the child as we show its menu bar
        // as our own
        const wxEventType eventType = event.GetEventType();
        if ( eventType == wxEVT_MENU ||
             eventType == wxEVT_UPDATE_UI )
        {
            // set the flag indicating that this event was forwarded to the
            // child from the parent and so shouldn't be propagated upwards if
            // not processed to avoid infinite loop
            m_childHandler = m_currentChild;
            wxON_BLOCK_EXIT_NULL(m_childHandler);

            if ( m_currentChild->ProcessWindowEvent(event) )
                return true;
        }
    }

    return wxMDIParentFrameBase::ProcessEvent(event);
}

// ----------------------------------------------------------------------------
// wxGenericMDIChildFrame
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxGenericMDIChildFrame, wxFrame)

BEGIN_EVENT_TABLE(wxGenericMDIChildFrame, wxFrame)
    EVT_MENU_HIGHLIGHT_ALL(wxGenericMDIChildFrame::OnMenuHighlight)

    EVT_CLOSE(wxGenericMDIChildFrame::OnClose)
END_EVENT_TABLE()

void wxGenericMDIChildFrame::Init()
{
#if wxUSE_MENUS
    m_pMenuBar = NULL;
#endif // wxUSE_MENUS

#if !wxUSE_GENERIC_MDI_AS_NATIVE
    m_mdiParentGeneric = NULL;
#endif
}

wxGenericMDIChildFrame::~wxGenericMDIChildFrame()
{
    wxGenericMDIParentFrame * const parent = GetGenericMDIParent();

    // it could happen that we don't have a valid parent if we hadn't been ever
    // really created -- but in this case there is nothing else to do neither
    if ( parent )
        parent->WXRemoveChild(this);

#if wxUSE_MENUS
    delete m_pMenuBar;
#endif // wxUSE_MENUS
}

bool wxGenericMDIChildFrame::Create(wxGenericMDIParentFrame *parent,
                                    wxWindowID id,
                                    const wxString& title,
                                    const wxPoint& WXUNUSED(pos),
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    const wxString& name)
{
    // unfortunately we can't use the base class m_mdiParent field unless
    // wxGenericMDIParentFrame is wxMDIParentFrame
#if wxUSE_GENERIC_MDI_AS_NATIVE
    m_mdiParent = parent;
#else // generic != native
    // leave m_mdiParent NULL, we don't have it
    m_mdiParentGeneric = parent;
#endif

    wxBookCtrlBase * const book = parent->GetBookCtrl();

    wxASSERT_MSG( book, "Missing MDI client window." );

    // note that we ignore the styles, none of the usual TLW styles apply to
    // this (child) window
    if ( !wxWindow::Create(book, id, wxDefaultPosition, size, 0, name) )
        return false;

    m_title = title;
    book->AddPage(this, title, true);

    return true;
}

#if wxUSE_MENUS
void wxGenericMDIChildFrame::SetMenuBar( wxMenuBar *menu_bar )
{
    wxMenuBar *pOldMenuBar = m_pMenuBar;
    m_pMenuBar = menu_bar;

    if (m_pMenuBar)
    {
        wxGenericMDIParentFrame *parent = GetGenericMDIParent();

        if ( parent )
        {
            m_pMenuBar->SetParent(parent);

            if ( parent->WXIsActiveChild(this) )
            {
                // Replace current menu bars
                if (pOldMenuBar)
                    parent->WXSetChildMenuBar(NULL);
                parent->WXSetChildMenuBar(this);
            }
        }
    }
}

wxMenuBar *wxGenericMDIChildFrame::GetMenuBar() const
{
    return m_pMenuBar;
}
#endif // wxUSE_MENUS

void wxGenericMDIChildFrame::SetTitle(const wxString& title)
{
    m_title = title;

    wxGenericMDIParentFrame * const parent = GetGenericMDIParent();
    if ( parent )
        parent->WXUpdateChildTitle(this);
    //else: it's ok, we might be not created yet
}

void wxGenericMDIChildFrame::Activate()
{
    wxGenericMDIParentFrame * const parent = GetGenericMDIParent();

    wxCHECK_RET( parent, "can't activate MDI child without parent" );
    parent->WXActivateChild(this);
}

void wxGenericMDIChildFrame::OnMenuHighlight(wxMenuEvent& event)
{
    wxGenericMDIParentFrame * const parent = GetGenericMDIParent();
    if ( parent)
    {
        // we don't have any help text for this item,
        // but may be the MDI frame does?
        parent->OnMenuHighlight(event);
    }
}

void wxGenericMDIChildFrame::OnClose(wxCloseEvent& WXUNUSED(event))
{
    // we're not a TLW so don't delay the destruction of this window
    delete this;
}

bool wxGenericMDIChildFrame::TryAfter(wxEvent& event)
{
    // we shouldn't propagate the event to the parent if we received it from it
    // in the first place
    wxGenericMDIParentFrame * const parent = GetGenericMDIParent();
    if ( parent && parent->WXIsInsideChildHandler(this) )
        return false;

    return wxTDIChildFrame::TryAfter(event);
}

// ----------------------------------------------------------------------------
// wxGenericMDIClientWindow
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxGenericMDIClientWindow, wxWindow)

bool
wxGenericMDIClientWindow::CreateGenericClient(wxWindow *parent)
{
    if ( !wxWindow::Create(parent, wxID_ANY) )
        return false;

    m_notebook = new wxNotebook(this, wxID_ANY);
    m_notebook->Connect
                (
                    wxEVT_NOTEBOOK_PAGE_CHANGED,
                    wxNotebookEventHandler(
                        wxGenericMDIClientWindow::OnPageChanged),
                    NULL,
                    this
                );

    // now that we have a notebook to resize, hook up OnSize() too
    Connect(wxEVT_SIZE, wxSizeEventHandler(wxGenericMDIClientWindow::OnSize));

    return true;
}

wxBookCtrlBase *wxGenericMDIClientWindow::GetBookCtrl() const
{
    return m_notebook;
}

wxGenericMDIChildFrame *wxGenericMDIClientWindow::GetChild(size_t pos) const
{
    return static_cast<wxGenericMDIChildFrame *>(GetBookCtrl()->GetPage(pos));
}

int wxGenericMDIClientWindow::FindChild(wxGenericMDIChildFrame *child) const
{
    wxBookCtrlBase * const book = GetBookCtrl();
    const size_t count = book->GetPageCount();
    for ( size_t pos = 0; pos < count; pos++ )
    {
        if ( book->GetPage(pos) == child )
            return pos;
    }

    return wxNOT_FOUND;
}

void wxGenericMDIClientWindow::PageChanged(int oldSelection, int newSelection)
{
    // Don't do anything if nothing changed
    if (oldSelection == newSelection)
        return;

    // Again check if we really need to do this...
    if (newSelection != -1)
    {
        wxGenericMDIChildFrame * const child = GetChild(newSelection);

        if ( child->GetGenericMDIParent()->WXIsActiveChild(child) )
            return;
    }

    // Notify old active child that it has been deactivated
    if (oldSelection != -1)
    {
        wxGenericMDIChildFrame * const oldChild = GetChild(oldSelection);
        if (oldChild)
        {
            wxActivateEvent event(wxEVT_ACTIVATE, false, oldChild->GetId());
            event.SetEventObject( oldChild );
            oldChild->GetEventHandler()->ProcessEvent(event);
        }
    }

    // Notify new active child that it has been activated
    if (newSelection != -1)
    {
        wxGenericMDIChildFrame * const activeChild = GetChild(newSelection);
        if ( activeChild )
        {
            wxActivateEvent event(wxEVT_ACTIVATE, true, activeChild->GetId());
            event.SetEventObject( activeChild );
            activeChild->GetEventHandler()->ProcessEvent(event);

            wxGenericMDIParentFrame * const
                parent = activeChild->GetGenericMDIParent();

            if ( parent )
            {
                // this is a dirty hack as activeChild is not really a
                // wxMDIChildFrame at all but we still want to store it in the
                // base class m_currentChild field and this will work as long
                // as we only use as wxMDIChildFrameBase pointer (which it is)
                parent->SetActiveChild(
                    reinterpret_cast<wxMDIChildFrame *>(activeChild));
                parent->WXSetChildMenuBar(activeChild);
            }
        }
    }
}

void wxGenericMDIClientWindow::OnPageChanged(wxBookCtrlEvent& event)
{
    PageChanged(event.GetOldSelection(), event.GetSelection());

    event.Skip();
}

void wxGenericMDIClientWindow::OnSize(wxSizeEvent& WXUNUSED(event))
{
    m_notebook->SetSize(GetClientSize());
}

#endif // wxUSE_MDI

