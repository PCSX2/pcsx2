///////////////////////////////////////////////////////////////////////////////
// Name:        wx/compositewin.h
// Purpose:     wxCompositeWindow<> declaration
// Author:      Vadim Zeitlin
// Created:     2011-01-02
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COMPOSITEWIN_H_
#define _WX_COMPOSITEWIN_H_

#include "wx/window.h"
#include "wx/containr.h"

class WXDLLIMPEXP_FWD_CORE wxToolTip;

// NB: This is an experimental and, as for now, undocumented class used only by
//     wxWidgets itself internally. Don't use it in your code until its API is
//     officially stabilized unless you are ready to change it with the next
//     wxWidgets release.

// ----------------------------------------------------------------------------
// wxCompositeWindow is a helper for implementing composite windows: to define
// a class using subwindows, simply inherit from it specialized with the real
// base class name and implement GetCompositeWindowParts() pure virtual method.
// ----------------------------------------------------------------------------

// The template parameter W must be a wxWindow-derived class.
template <class W>
class wxCompositeWindow : public W
{
public:
    typedef W BaseWindowClass;

    // Default ctor doesn't do anything.
    wxCompositeWindow()
    {
        this->Connect
              (
                  wxEVT_CREATE,
                  wxWindowCreateEventHandler(wxCompositeWindow::OnWindowCreate)
              );

    }

#ifndef __VISUALC6__
    // FIXME-VC6: This compiler can't compile DoSetForAllParts() template function,
    // it can't determine whether the deduced type should be "T" or "const T&". And
    // without this function wxCompositeWindow is pretty useless so simply disable
    // this code for it, this does mean that setting colours/fonts/... for
    // composite controls won't work in the library compiled with it but so far
    // this only affects the generic wxDatePickerCtrl which is not used by default
    // under MSW anyhow so it doesn't seem to be worth it to spend time and uglify
    // the code to fix it.

    // Override all wxWindow methods which must be forwarded to the composite
    // window parts.

    // Attribute setters group.
    //
    // NB: Unfortunately we can't factor out the call for the setter itself
    //     into DoSetForAllParts() because we can't call the function passed to
    //     it non-virtually and we need to do this to avoid infinite recursion,
    //     so we work around this by calling the method of this object itself
    //     manually in each function.
    virtual bool SetForegroundColour(const wxColour& colour)
    {
        if ( !BaseWindowClass::SetForegroundColour(colour) )
            return false;

        SetForAllParts(&wxWindowBase::SetForegroundColour, colour);

        return true;
    }

    virtual bool SetBackgroundColour(const wxColour& colour)
    {
        if ( !BaseWindowClass::SetBackgroundColour(colour) )
            return false;

        SetForAllParts(&wxWindowBase::SetBackgroundColour, colour);

        return true;
    }

    virtual bool SetFont(const wxFont& font)
    {
        if ( !BaseWindowClass::SetFont(font) )
            return false;

        SetForAllParts(&wxWindowBase::SetFont, font);

        return true;
    }

    virtual bool SetCursor(const wxCursor& cursor)
    {
        if ( !BaseWindowClass::SetCursor(cursor) )
            return false;

        SetForAllParts(&wxWindowBase::SetCursor, cursor);

        return true;
    }

#if wxUSE_TOOLTIPS
    virtual void DoSetToolTip(wxToolTip *tip)
    {
        BaseWindowClass::DoSetToolTip(tip);

        SetForAllParts(&wxWindowBase::CopyToolTip, tip);
    }
#endif // wxUSE_TOOLTIPS

#endif // !__VISUALC6__

    virtual void SetFocus()
    {
        wxSetFocusToChild(this, NULL);
    }

private:
    // Must be implemented by the derived class to return all children to which
    // the public methods we override should forward to.
    virtual wxWindowList GetCompositeWindowParts() const = 0;

    void OnWindowCreate(wxWindowCreateEvent& event)
    {
        event.Skip();

        // Attach a few event handlers to all parts of the composite window.
        // This makes the composite window behave more like a simple control
        // and allows other code (such as wxDataViewCtrl's inline editing
        // support) to hook into its event processing.

        wxWindow *child = event.GetWindow();
        if ( child == this )
            return; // not a child, we don't want to Connect() to ourselves

        // Always capture wxEVT_KILL_FOCUS:
        child->Connect(wxEVT_KILL_FOCUS,
                       wxFocusEventHandler(wxCompositeWindow::OnKillFocus),
                       NULL, this);

        // Some events should be only handled for non-toplevel children. For
        // example, we want to close the control in wxDataViewCtrl when Enter
        // is pressed in the inline editor, but not when it's pressed in a
        // popup dialog it opens.
        wxWindow *win = child;
        while ( win && win != this )
        {
            if ( win->IsTopLevel() )
                return;
            win = win->GetParent();
        }

        child->Connect(wxEVT_CHAR,
                       wxKeyEventHandler(wxCompositeWindow::OnChar),
                       NULL, this);
    }

    void OnChar(wxKeyEvent& event)
    {
        if ( !this->ProcessWindowEvent(event) )
            event.Skip();
    }

    void OnKillFocus(wxFocusEvent& event)
    {
        // Ignore focus changes within the composite control:
        wxWindow *win = event.GetWindow();
        while ( win )
        {
            if ( win == this )
            {
                event.Skip();
                return;
            }

            // Note that we don't use IsTopLevel() check here, because we do
            // want to ignore focus changes going to toplevel window that have
            // the composite control as its parent; these would typically be
            // some kind of control's popup window.
            win = win->GetParent();
        }

        // The event shouldn't be ignored, forward it to the main control:
        if ( !this->ProcessWindowEvent(event) )
            event.Skip();
    }

#ifndef __VISUALC6__
    template <class T>
    void SetForAllParts(bool (wxWindowBase::*func)(const T&), const T& arg)
    {
        DoSetForAllParts<const T&>(func, arg);
    }

    template <class T>
    void SetForAllParts(bool (wxWindowBase::*func)(T*), T* arg)
    {
        DoSetForAllParts<T*>(func, arg);
    }

    template <class T>
    void DoSetForAllParts(bool (wxWindowBase::*func)(T), T arg)
    {
        // Simply call the setters for all parts of this composite window.
        const wxWindowList parts = GetCompositeWindowParts();
        for ( wxWindowList::const_iterator i = parts.begin();
              i != parts.end();
              ++i )
        {
            wxWindow * const child = *i;

            // Allow NULL elements in the list, this makes the code of derived
            // composite controls which may have optionally shown children
            // simpler and it doesn't cost us much here.
            if ( child )
                (child->*func)(arg);
        }
    }
#endif // !__VISUALC6__

    wxDECLARE_NO_COPY_TEMPLATE_CLASS(wxCompositeWindow, W);
};

#endif // _WX_COMPOSITEWIN_H_
