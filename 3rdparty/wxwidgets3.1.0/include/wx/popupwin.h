///////////////////////////////////////////////////////////////////////////////
// Name:        wx/popupwin.h
// Purpose:     wxPopupWindow interface declaration
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.01.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_POPUPWIN_H_BASE_
#define _WX_POPUPWIN_H_BASE_

#include "wx/defs.h"

#if wxUSE_POPUPWIN

#include "wx/nonownedwnd.h"

// ----------------------------------------------------------------------------
// wxPopupWindow: a special kind of top level window used for popup menus,
// combobox popups and such.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPopupWindowBase : public wxNonOwnedWindow
{
public:
    wxPopupWindowBase() { }
    virtual ~wxPopupWindowBase();

    // create the popup window
    //
    // style may only contain border flags
    bool Create(wxWindow *parent, int style = wxBORDER_NONE);

    // move the popup window to the right position, i.e. such that it is
    // entirely visible
    //
    // the popup is positioned at ptOrigin + size if it opens below and to the
    // right (default), at ptOrigin - sizePopup if it opens above and to the
    // left &c
    //
    // the point must be given in screen coordinates!
    virtual void Position(const wxPoint& ptOrigin,
                          const wxSize& size);

    virtual bool IsTopLevel() const wxOVERRIDE { return true; }

    wxDECLARE_NO_COPY_CLASS(wxPopupWindowBase);
};


// include the real class declaration
#if defined(__WXMSW__)
    #include "wx/msw/popupwin.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/popupwin.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/popupwin.h"
#elif defined(__WXX11__)
    #include "wx/x11/popupwin.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/popupwin.h"
#elif defined(__WXDFB__)
    #include "wx/dfb/popupwin.h"
#elif defined(__WXMAC__)
    #include "wx/osx/popupwin.h"
#elif defined(__WXQT__)
    #include "wx/qt/popupwin.h"
#else
    #error "wxPopupWindow is not supported under this platform."
#endif

// ----------------------------------------------------------------------------
// wxPopupTransientWindow: a wxPopupWindow which disappears automatically
// when the user clicks mouse outside it or if it loses focus in any other way
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxPopupWindowHandler;
class WXDLLIMPEXP_FWD_CORE wxPopupFocusHandler;

class WXDLLIMPEXP_CORE wxPopupTransientWindow : public wxPopupWindow
{
public:
    // ctors
    wxPopupTransientWindow() { Init(); }
    wxPopupTransientWindow(wxWindow *parent, int style = wxBORDER_NONE);

    virtual ~wxPopupTransientWindow();

    // popup the window (this will show it too) and keep focus at winFocus
    // (or itself if it's NULL), dismiss the popup if we lose focus
    virtual void Popup(wxWindow *focus = NULL);

    // hide the window
    virtual void Dismiss();

    // can the window be dismissed now?
    //
    // VZ: where is this used??
    virtual bool CanDismiss()
        { return true; }

    // called when a mouse is pressed while the popup is shown: return true
    // from here to prevent its normal processing by the popup (which consists
    // in dismissing it if the mouse is clicked outside it)
    virtual bool ProcessLeftDown(wxMouseEvent& event);

    // Overridden to grab the input on some plaforms
    virtual bool Show( bool show = true ) wxOVERRIDE;

    // Override to implement delayed destruction of this window.
    virtual bool Destroy() wxOVERRIDE;

protected:
    // common part of all ctors
    void Init();

    // this is called when the popup is disappeared because of anything
    // else but direct call to Dismiss()
    virtual void OnDismiss();

    // dismiss and notify the derived class
    void DismissAndNotify();

    // remove our event handlers
    void PopHandlers();

    // get alerted when child gets deleted from under us
    void OnDestroy(wxWindowDestroyEvent& event);

#if defined(__WXMSW__) ||(defined(__WXMAC__) && wxOSX_USE_COCOA_OR_CARBON)
    // Check if the mouse needs to be captured or released: we must release
    // when it's inside our window if we want the embedded controls to work.
    void OnIdle(wxIdleEvent& event);
#endif

    // the child of this popup if any
    wxWindow *m_child;

    // the window which has the focus while we're shown
    wxWindow *m_focus;

    // these classes may call our DismissAndNotify()
    friend class wxPopupWindowHandler;
    friend class wxPopupFocusHandler;

    // the handlers we created, may be NULL (if not, must be deleted)
    wxPopupWindowHandler *m_handlerPopup;
    wxPopupFocusHandler  *m_handlerFocus;

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxPopupTransientWindow);
    wxDECLARE_NO_COPY_CLASS(wxPopupTransientWindow);
};

#if wxUSE_COMBOBOX && defined(__WXUNIVERSAL__)

// ----------------------------------------------------------------------------
// wxPopupComboWindow: wxPopupTransientWindow used by wxComboBox
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxComboBox;
class WXDLLIMPEXP_FWD_CORE wxComboCtrl;

class WXDLLIMPEXP_CORE wxPopupComboWindow : public wxPopupTransientWindow
{
public:
    wxPopupComboWindow() { m_combo = NULL; }
    wxPopupComboWindow(wxComboCtrl *parent);

    bool Create(wxComboCtrl *parent);

    // position the window correctly relatively to the combo
    void PositionNearCombo();

protected:
    // notify the combo here
    virtual void OnDismiss();

    // forward the key presses to the combobox
    void OnKeyDown(wxKeyEvent& event);

    // the parent combobox
    wxComboCtrl *m_combo;

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxPopupComboWindow);
};

#endif // wxUSE_COMBOBOX && defined(__WXUNIVERSAL__)

#endif // wxUSE_POPUPWIN

#endif // _WX_POPUPWIN_H_BASE_
