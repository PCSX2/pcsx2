///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/popupcmn.cpp
// Purpose:     implementation of wxPopupTransientWindow
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.01.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_POPUPWIN

#include "wx/popupwin.h"

#ifndef WX_PRECOMP
    #include "wx/combobox.h"        // wxComboCtrl
    #include "wx/app.h"             // wxPostEvent
    #include "wx/log.h"
#endif //WX_PRECOMP

#include "wx/display.h"
#include "wx/recguard.h"

#ifdef __WXUNIVERSAL__
    #include "wx/univ/renderer.h"
    #include "wx/scrolbar.h"
#endif // __WXUNIVERSAL__

#ifdef __WXGTK__
    #include <gtk/gtk.h>
    #if GTK_CHECK_VERSION(2,0,0)
        #include "wx/gtk/private/gtk2-compat.h"
    #else
        #define gtk_widget_get_window(x) x->window
    #endif
#elif defined(__WXMSW__)
    #include "wx/msw/private.h"
#elif defined(__WXX11__)
    #include "wx/x11/private.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxPopupWindow, wxWindow);
wxIMPLEMENT_DYNAMIC_CLASS(wxPopupTransientWindow, wxPopupWindow);

#if wxUSE_COMBOBOX && defined(__WXUNIVERSAL__)
wxIMPLEMENT_DYNAMIC_CLASS(wxPopupComboWindow, wxPopupTransientWindow);
#endif

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// event handlers which we use to intercept events which cause the popup to
// disappear
class wxPopupWindowHandler : public wxEvtHandler
{
public:
    wxPopupWindowHandler(wxPopupTransientWindow *popup) : m_popup(popup) {}

protected:
    // event handlers
    void OnLeftDown(wxMouseEvent& event);
    void OnCaptureLost(wxMouseCaptureLostEvent& event);

private:
    wxPopupTransientWindow *m_popup;

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(wxPopupWindowHandler);
};

class wxPopupFocusHandler : public wxEvtHandler
{
public:
    wxPopupFocusHandler(wxPopupTransientWindow *popup) : m_popup(popup) {}

protected:
    void OnKillFocus(wxFocusEvent& event);
    void OnChar(wxKeyEvent& event);

private:
    wxPopupTransientWindow *m_popup;

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(wxPopupFocusHandler);
};

// ----------------------------------------------------------------------------
// event tables
// ----------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(wxPopupWindowHandler, wxEvtHandler)
    EVT_LEFT_DOWN(wxPopupWindowHandler::OnLeftDown)
    EVT_MOUSE_CAPTURE_LOST(wxPopupWindowHandler::OnCaptureLost)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(wxPopupFocusHandler, wxEvtHandler)
    EVT_KILL_FOCUS(wxPopupFocusHandler::OnKillFocus)
    EVT_CHAR(wxPopupFocusHandler::OnChar)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(wxPopupTransientWindow, wxPopupWindow)
#if defined(__WXMSW__) || (defined(__WXMAC__) && wxOSX_USE_COCOA_OR_CARBON)
    EVT_IDLE(wxPopupTransientWindow::OnIdle)
#endif
wxEND_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxPopupWindowBase
// ----------------------------------------------------------------------------

wxPopupWindowBase::~wxPopupWindowBase()
{
    // this destructor is required for Darwin
}

bool wxPopupWindowBase::Create(wxWindow* WXUNUSED(parent), int WXUNUSED(flags))
{
    return true;
}

void wxPopupWindowBase::Position(const wxPoint& ptOrigin,
                                 const wxSize& size)
{
    // determine the position and size of the screen we clamp the popup to
    wxPoint posScreen;
    wxSize sizeScreen;

    const int displayNum = wxDisplay::GetFromPoint(ptOrigin);
    if ( displayNum != wxNOT_FOUND )
    {
        const wxRect rectScreen = wxDisplay(displayNum).GetGeometry();
        posScreen = rectScreen.GetPosition();
        sizeScreen = rectScreen.GetSize();
    }
    else // outside of any display?
    {
        // just use the primary one then
        posScreen = wxPoint(0, 0);
        sizeScreen = wxGetDisplaySize();
    }


    const wxSize sizeSelf = GetSize();

    // is there enough space to put the popup below the window (where we put it
    // by default)?
    wxCoord y = ptOrigin.y + size.y;
    if ( y + sizeSelf.y > posScreen.y + sizeScreen.y )
    {
        // check if there is enough space above
        if ( ptOrigin.y > sizeSelf.y )
        {
            // do position the control above the window
            y -= size.y + sizeSelf.y;
        }
        //else: not enough space below nor above, leave below
    }

    // now check left/right too
    wxCoord x = ptOrigin.x;

    if ( wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft )
    {
        // shift the window to the left instead of the right.
        x -= size.x;
        x -= sizeSelf.x;        // also shift it by window width.
    }
    else
        x += size.x;


    if ( x + sizeSelf.x > posScreen.x + sizeScreen.x )
    {
        // check if there is enough space to the left
        if ( ptOrigin.x > sizeSelf.x )
        {
            // do position the control to the left
            x -= size.x + sizeSelf.x;
        }
        //else: not enough space there neither, leave in default position
    }

    Move(x, y, wxSIZE_NO_ADJUSTMENTS);
}

// ----------------------------------------------------------------------------
// wxPopupTransientWindow
// ----------------------------------------------------------------------------

void wxPopupTransientWindow::Init()
{
    m_child =
    m_focus = NULL;

    m_handlerFocus = NULL;
    m_handlerPopup = NULL;
}

wxPopupTransientWindow::wxPopupTransientWindow(wxWindow *parent, int style)
{
    Init();

    (void)Create(parent, style);
}

wxPopupTransientWindow::~wxPopupTransientWindow()
{
    if (m_handlerPopup && m_handlerPopup->GetNextHandler())
        PopHandlers();

    wxASSERT(!m_handlerFocus || !m_handlerFocus->GetNextHandler());
    wxASSERT(!m_handlerPopup || !m_handlerPopup->GetNextHandler());

    delete m_handlerFocus;
    delete m_handlerPopup;
}

void wxPopupTransientWindow::PopHandlers()
{
    if ( m_child )
    {
        if ( !m_child->RemoveEventHandler(m_handlerPopup) )
        {
            // something is very wrong and someone else probably deleted our
            // handler - so don't risk deleting it second time
            m_handlerPopup = NULL;
        }
        if (m_child->HasCapture())
        {
            m_child->ReleaseMouse();
        }
        m_child = NULL;
    }

    if ( m_focus )
    {
        if ( !m_focus->RemoveEventHandler(m_handlerFocus) )
        {
            // see above
            m_handlerFocus = NULL;
        }
    }
    m_focus = NULL;
}

void wxPopupTransientWindow::Popup(wxWindow *winFocus)
{
    // If we have a single child, we suppose that it must cover the entire
    // popup window and hence we give the mouse capture to it instead of
    // keeping it for ourselves.
    //
    // Notice that this works best for combobox-like popups which have a single
    // control inside them and not so well for popups containing a single
    // wxPanel with multiple children inside it but OTOH it does no harm in
    // this case neither and we can't reliably distinguish between them.
    const wxWindowList& children = GetChildren();
    if ( children.GetCount() == 1 )
    {
        m_child = children.GetFirst()->GetData();
    }
    else
    {
        m_child = this;
    }

    Show();

    // There is a problem if these are still in use
    wxASSERT(!m_handlerFocus || !m_handlerFocus->GetNextHandler());
    wxASSERT(!m_handlerPopup || !m_handlerPopup->GetNextHandler());

    if (!m_handlerPopup)
        m_handlerPopup = new wxPopupWindowHandler(this);

    m_child->PushEventHandler(m_handlerPopup);

#if defined(__WXMSW__)
    // Focusing on child of popup window does not work on MSW unless WS_POPUP
    // style is set. We do not even want to try to set the focus, as it may
    // provoke errors on some Windows versions (Vista and later).
    if ( ::GetWindowLong(GetHwnd(), GWL_STYLE) & WS_POPUP )
#endif
    {
        m_focus = winFocus ? winFocus : this;
        m_focus->SetFocus();
    }

#if defined( __WXMSW__ ) || (defined( __WXMAC__) && wxOSX_USE_COCOA_OR_CARBON)
    // MSW doesn't allow to set focus to the popup window, but we need to
    // subclass the window which has the focus, and not winFocus passed in or
    // otherwise everything else breaks down
    m_focus = FindFocus();
#elif defined(__WXGTK__)
    // GTK+ catches the activate events from the popup
    // window, not the focus events from the child window
    m_focus = this;
#endif

    if ( m_focus )
    {
        if (!m_handlerFocus)
            m_handlerFocus = new wxPopupFocusHandler(this);

        m_focus->PushEventHandler(m_handlerFocus);
    }
}

bool wxPopupTransientWindow::Show( bool show )
{
#ifdef __WXGTK__
    if (!show)
    {
#ifdef __WXGTK3__
        GdkDisplay* display = gtk_widget_get_display(m_widget);
        GdkDeviceManager* manager = gdk_display_get_device_manager(display);
        GdkDevice* device = gdk_device_manager_get_client_pointer(manager);
        gdk_device_ungrab(device, unsigned(GDK_CURRENT_TIME));
#else
        gdk_pointer_ungrab( (guint32)GDK_CURRENT_TIME );
#endif

        gtk_grab_remove( m_widget );
    }
#endif

#ifdef __WXX11__
    if (!show)
    {
        XUngrabPointer( wxGlobalDisplay(), CurrentTime );
    }
#endif

#if defined( __WXMSW__ ) || defined( __WXMAC__)
    if (!show && m_child && m_child->HasCapture())
    {
        m_child->ReleaseMouse();
    }
#endif

    bool ret = wxPopupWindow::Show( show );

#ifdef __WXGTK__
    if (show)
    {
        gtk_grab_add( m_widget );

        const GdkEventMask mask = GdkEventMask(
            GDK_BUTTON_PRESS_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_POINTER_MOTION_HINT_MASK |
            GDK_POINTER_MOTION_MASK);
        GdkWindow* window = gtk_widget_get_window(m_widget);
#ifdef __WXGTK3__
        GdkDisplay* display = gdk_window_get_display(window);
        GdkDeviceManager* manager = gdk_display_get_device_manager(display);
        GdkDevice* device = gdk_device_manager_get_client_pointer(manager);
        gdk_device_grab(device, window,
            GDK_OWNERSHIP_NONE, true, mask, NULL, unsigned(GDK_CURRENT_TIME));
#else
        gdk_pointer_grab( window, true,
                          mask,
                          NULL,
                          NULL,
                          (guint32)GDK_CURRENT_TIME );
#endif
    }
#endif

#ifdef __WXX11__
    if (show)
    {
        Window xwindow = (Window) m_clientWindow;

        /* int res =*/ XGrabPointer(wxGlobalDisplay(), xwindow,
            True,
            ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask,
            GrabModeAsync,
            GrabModeAsync,
            None,
            None,
            CurrentTime );
    }
#endif

#if defined( __WXMSW__ ) || defined( __WXMAC__)
    if (show && m_child)
    {
        // Assume that the mouse is outside the popup to begin with
        m_child->CaptureMouse();
    }
#endif

    return ret;
}

bool wxPopupTransientWindow::Destroy()
{
    // The popup window can be deleted at any moment, even while some events
    // are still being processed for it, so delay its real destruction until
    // the next idle time when we're sure that it's safe to really destroy it.

    wxCHECK_MSG( !wxPendingDelete.Member(this), false,
                 wxS("Shouldn't destroy the popup twice.") );

    wxPendingDelete.Append(this);

    return true;
}

void wxPopupTransientWindow::Dismiss()
{
    Hide();
    PopHandlers();
}

void wxPopupTransientWindow::DismissAndNotify()
{
    Dismiss();
    OnDismiss();
}

void wxPopupTransientWindow::OnDismiss()
{
    // nothing to do here - but it may be interesting for derived class
}

bool wxPopupTransientWindow::ProcessLeftDown(wxMouseEvent& WXUNUSED(event))
{
    // no special processing here
    return false;
}

#if defined(__WXMSW__) ||(defined(__WXMAC__) && wxOSX_USE_COCOA_OR_CARBON)
void wxPopupTransientWindow::OnIdle(wxIdleEvent& event)
{
    event.Skip();

    if (IsShown() && m_child)
    {
        // Store the last mouse position to minimize the number of calls to
        // wxFindWindowAtPoint() which are quite expensive.
        static wxPoint s_posLast;
        const wxPoint pos = wxGetMousePosition();
        if ( pos != s_posLast )
        {
            s_posLast = pos;

            wxWindow* const winUnderMouse = wxFindWindowAtPoint(pos);

            // We release the mouse capture while the mouse is inside the popup
            // itself to allow using it normally with the controls inside it.
            if ( wxGetTopLevelParent(winUnderMouse) == this )
            {
                if ( m_child->HasCapture() )
                {
                    m_child->ReleaseMouse();
                }
            }
            else // And we reacquire it as soon as the mouse goes outside.
            {
                if ( !m_child->HasCapture() )
                {
                    m_child->CaptureMouse();
                }
            }
        }
    }
}
#endif // wxOSX/Carbon


#if wxUSE_COMBOBOX && defined(__WXUNIVERSAL__)

// ----------------------------------------------------------------------------
// wxPopupComboWindow
// ----------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(wxPopupComboWindow, wxPopupTransientWindow)
    EVT_KEY_DOWN(wxPopupComboWindow::OnKeyDown)
wxEND_EVENT_TABLE()

wxPopupComboWindow::wxPopupComboWindow(wxComboCtrl *parent)
                  : wxPopupTransientWindow(parent)
{
    m_combo = parent;
}

bool wxPopupComboWindow::Create(wxComboCtrl *parent)
{
    m_combo = parent;

    return wxPopupWindow::Create(parent);
}

void wxPopupComboWindow::PositionNearCombo()
{
    // the origin point must be in screen coords
    wxPoint ptOrigin = m_combo->ClientToScreen(wxPoint(0,0));

#if 0 //def __WXUNIVERSAL__
    // account for the fact that (0, 0) is not the top left corner of the
    // window: there is also the border
    wxRect rectBorders = m_combo->GetRenderer()->
                            GetBorderDimensions(m_combo->GetBorder());
    ptOrigin.x -= rectBorders.x;
    ptOrigin.y -= rectBorders.y;
#endif // __WXUNIVERSAL__

    // position below or above the combobox: the width is 0 to put it exactly
    // below us, not to the left or to the right
    Position(ptOrigin, wxSize(0, m_combo->GetSize().y));
}

void wxPopupComboWindow::OnDismiss()
{
    m_combo->OnPopupDismiss(true);
}

void wxPopupComboWindow::OnKeyDown(wxKeyEvent& event)
{
    m_combo->ProcessWindowEvent(event);
}

#endif // wxUSE_COMBOBOX && defined(__WXUNIVERSAL__)

// ----------------------------------------------------------------------------
// wxPopupWindowHandler
// ----------------------------------------------------------------------------

void wxPopupWindowHandler::OnLeftDown(wxMouseEvent& event)
{
    // let the window have it first (we're the first event handler in the chain
    // of handlers for this window)
    if ( m_popup->ProcessLeftDown(event) )
    {
        return;
    }

    wxPoint pos = event.GetPosition();

    // in non-Univ ports the system manages scrollbars for us
#if defined(__WXUNIVERSAL__) && wxUSE_SCROLLBAR
    // scrollbar on which the click occurred
    wxWindow *sbar = NULL;
#endif // __WXUNIVERSAL__ && wxUSE_SCROLLBAR

    wxWindow *win = (wxWindow *)event.GetEventObject();

    switch ( win->HitTest(pos.x, pos.y) )
    {
        case wxHT_WINDOW_OUTSIDE:
            {
                // do the coords translation now as after DismissAndNotify()
                // m_popup may be destroyed
                wxMouseEvent event2(event);

                m_popup->ClientToScreen(&event2.m_x, &event2.m_y);

                // clicking outside a popup dismisses it
                m_popup->DismissAndNotify();

                // dismissing a tooltip shouldn't waste a click, i.e. you
                // should be able to dismiss it and press the button with the
                // same click, so repost this event to the window beneath us
                wxWindow *winUnder = wxFindWindowAtPoint(event2.GetPosition());
                if ( winUnder )
                {
                    // translate the event coords to the ones of the window
                    // which is going to get the event
                    winUnder->ScreenToClient(&event2.m_x, &event2.m_y);

                    event2.SetEventObject(winUnder);
                    wxPostEvent(winUnder->GetEventHandler(), event2);
                }
            }
            break;

#if defined(__WXUNIVERSAL__) && wxUSE_SCROLLBAR
        case wxHT_WINDOW_HORZ_SCROLLBAR:
            sbar = win->GetScrollbar(wxHORIZONTAL);
            break;

        case wxHT_WINDOW_VERT_SCROLLBAR:
            sbar = win->GetScrollbar(wxVERTICAL);
            break;
#endif // __WXUNIVERSAL__ && wxUSE_SCROLLBAR

        default:
            // forgot to update the switch after adding a new hit test code?
            wxFAIL_MSG( wxT("unexpected HitTest() return value") );
            wxFALLTHROUGH;

        case wxHT_WINDOW_CORNER:
            // don't actually know if this one is good for anything, but let it
            // pass just in case

        case wxHT_WINDOW_INSIDE:
            // let the normal processing take place
            event.Skip();
            break;
    }

#if defined(__WXUNIVERSAL__) && wxUSE_SCROLLBAR
    if ( sbar )
    {
        // translate the event coordinates to the scrollbar ones
        pos = sbar->ScreenToClient(win->ClientToScreen(pos));

        // and give the event to it
        wxMouseEvent event2 = event;
        event2.m_x = pos.x;
        event2.m_y = pos.y;

        (void)sbar->GetEventHandler()->ProcessEvent(event2);
    }
#endif // __WXUNIVERSAL__ && wxUSE_SCROLLBAR
}

void
wxPopupWindowHandler::OnCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(event))
{
    m_popup->DismissAndNotify();

    // There is no need to skip the event here, normally we've already dealt
    // with the focus loss.
}

// ----------------------------------------------------------------------------
// wxPopupFocusHandler
// ----------------------------------------------------------------------------

void wxPopupFocusHandler::OnKillFocus(wxFocusEvent& event)
{
    // when we lose focus we always disappear - unless it goes to the popup (in
    // which case we don't really lose it)
    wxWindow *win = event.GetWindow();
    while ( win )
    {
        if ( win == m_popup )
            return;
        win = win->GetParent();
    }

    m_popup->DismissAndNotify();
}

void wxPopupFocusHandler::OnChar(wxKeyEvent& event)
{
    // we can be associated with the popup itself in which case we should avoid
    // infinite recursion
    static int s_inside;
    wxRecursionGuard guard(s_inside);
    if ( guard.IsInside() )
    {
        event.Skip();
        return;
    }

    // let the window have it first, it might process the keys
    if ( !m_popup->GetEventHandler()->ProcessEvent(event) )
    {
        // by default, dismiss the popup
        m_popup->DismissAndNotify();
    }
}

#endif // wxUSE_POPUPWIN
