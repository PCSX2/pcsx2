///////////////////////////////////////////////////////////////////////////////
// Name:        wx/containr.h
// Purpose:     wxControlContainer and wxNavigationEnabled declarations
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.08.01
// Copyright:   (c) 2001, 2011 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CONTAINR_H_
#define _WX_CONTAINR_H_

#include "wx/defs.h"

#ifndef wxHAS_NATIVE_TAB_TRAVERSAL
    // We need wxEVT_XXX declarations in this case.
    #include "wx/event.h"
#endif

class WXDLLIMPEXP_FWD_CORE wxWindow;
class WXDLLIMPEXP_FWD_CORE wxWindowBase;

/*
    This header declares wxControlContainer class however it's not a real
    container of controls but rather just a helper used to implement TAB
    navigation among the window children. You should rarely need to use it
    directly, derive from the documented public wxNavigationEnabled<> class to
    implement TAB navigation in a custom composite window.
 */

// ----------------------------------------------------------------------------
// wxControlContainerBase: common part used in both native and generic cases
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxControlContainerBase
{
public:
    // default ctor, SetContainerWindow() must be called later
    wxControlContainerBase()
    {
        m_winParent = NULL;

        // By default, we accept focus ourselves.
        m_acceptsFocusSelf = true;

        // But we don't have any children accepting it yet.
        m_acceptsFocusChildren = false;

        m_inSetFocus = false;
        m_winLastFocused = NULL;
    }
    virtual ~wxControlContainerBase() {}

    void SetContainerWindow(wxWindow *winParent)
    {
        wxASSERT_MSG( !m_winParent, wxT("shouldn't be called twice") );

        m_winParent = winParent;
    }

    // This can be called by the window to indicate that it never wants to have
    // the focus for itself.
    void DisableSelfFocus()
        { m_acceptsFocusSelf = false; UpdateParentCanFocus(); }

    // This can be called to undo the effect of a previous DisableSelfFocus()
    // (otherwise calling it is not necessary as the window does accept focus
    // by default).
    void EnableSelfFocus()
        { m_acceptsFocusSelf = true; UpdateParentCanFocus(); }

    // should be called from SetFocus(), returns false if we did nothing with
    // the focus and the default processing should take place
    bool DoSetFocus();

    // returns whether we should accept focus ourselves or not
    bool AcceptsFocus() const;

    // Returns whether we or one of our children accepts focus.
    bool AcceptsFocusRecursively() const
        { return AcceptsFocus() ||
            (m_acceptsFocusChildren && HasAnyChildrenAcceptingFocus()); }

    // We accept focus from keyboard if we accept it at all.
    bool AcceptsFocusFromKeyboard() const { return AcceptsFocusRecursively(); }

    // Call this when the number of children of the window changes.
    //
    // Returns true if we have any focusable children, false otherwise.
    bool UpdateCanFocusChildren();

protected:
    // set the focus to the child which had it the last time
    virtual bool SetFocusToChild();

    // return true if we have any children accepting focus
    bool HasAnyFocusableChildren() const;

    // return true if we have any children that do accept focus right now
    bool HasAnyChildrenAcceptingFocus() const;


    // the parent window we manage the children for
    wxWindow *m_winParent;

    // the child which had the focus last time this panel was activated
    wxWindow *m_winLastFocused;

private:
    // Update the window status to reflect whether it is getting focus or not.
    void UpdateParentCanFocus();

    // Indicates whether the associated window can ever have focus itself.
    //
    // Usually this is the case, e.g. a wxPanel can be used either as a
    // container for its children or just as a normal window which can be
    // focused. But sometimes, e.g. for wxStaticBox, we can never have focus
    // ourselves and can only get it if we have any focusable children.
    bool m_acceptsFocusSelf;

    // Cached value remembering whether we have any children accepting focus.
    bool m_acceptsFocusChildren;

    // a guard against infinite recursion
    bool m_inSetFocus;
};

#ifdef wxHAS_NATIVE_TAB_TRAVERSAL

// ----------------------------------------------------------------------------
// wxControlContainer for native TAB navigation
// ----------------------------------------------------------------------------

// this must be a real class as we forward-declare it elsewhere
class WXDLLIMPEXP_CORE wxControlContainer : public wxControlContainerBase
{
protected:
    // set the focus to the child which had it the last time
    virtual bool SetFocusToChild() wxOVERRIDE;
};

#else // !wxHAS_NATIVE_TAB_TRAVERSAL

// ----------------------------------------------------------------------------
// wxControlContainer for TAB navigation implemented in wx itself
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxControlContainer : public wxControlContainerBase
{
public:
    // default ctor, SetContainerWindow() must be called later
    wxControlContainer();

    // the methods to be called from the window event handlers
    void HandleOnNavigationKey(wxNavigationKeyEvent& event);
    void HandleOnFocus(wxFocusEvent& event);
    void HandleOnWindowDestroy(wxWindowBase *child);

    // called from OnChildFocus() handler, i.e. when one of our (grand)
    // children gets the focus
    void SetLastFocus(wxWindow *win);

protected:

    wxDECLARE_NO_COPY_CLASS(wxControlContainer);
};

#endif // wxHAS_NATIVE_TAB_TRAVERSAL/!wxHAS_NATIVE_TAB_TRAVERSAL

// this function is for wxWidgets internal use only
extern WXDLLIMPEXP_CORE bool wxSetFocusToChild(wxWindow *win, wxWindow **child);

// ----------------------------------------------------------------------------
// wxNavigationEnabled: Derive from this class to support keyboard navigation
// among window children in a wxWindow-derived class. The details of this class
// don't matter, you just need to derive from it to make navigation work.
// ----------------------------------------------------------------------------

// The template parameter W must be a wxWindow-derived class.
template <class W>
class wxNavigationEnabled : public W
{
public:
    typedef W BaseWindowClass;

    wxNavigationEnabled()
    {
        m_container.SetContainerWindow(this);

#ifndef wxHAS_NATIVE_TAB_TRAVERSAL
        BaseWindowClass::Connect(wxEVT_NAVIGATION_KEY,
                wxNavigationKeyEventHandler(wxNavigationEnabled::OnNavigationKey));

        BaseWindowClass::Connect(wxEVT_SET_FOCUS,
                wxFocusEventHandler(wxNavigationEnabled::OnFocus));

        BaseWindowClass::Connect(wxEVT_CHILD_FOCUS,
                wxChildFocusEventHandler(wxNavigationEnabled::OnChildFocus));
#endif // !wxHAS_NATIVE_TAB_TRAVERSAL
    }

    WXDLLIMPEXP_INLINE_CORE virtual bool AcceptsFocus() const
    {
        return m_container.AcceptsFocus();
    }

    WXDLLIMPEXP_INLINE_CORE virtual bool AcceptsFocusRecursively() const
    {
        return m_container.AcceptsFocusRecursively();
    }

    WXDLLIMPEXP_INLINE_CORE virtual bool AcceptsFocusFromKeyboard() const
    {
        return m_container.AcceptsFocusFromKeyboard();
    }

    WXDLLIMPEXP_INLINE_CORE virtual void AddChild(wxWindowBase *child)
    {
        BaseWindowClass::AddChild(child);

        if ( m_container.UpdateCanFocusChildren() )
        {
            // Under MSW we must have wxTAB_TRAVERSAL style for TAB navigation
            // to work.
            if ( !BaseWindowClass::HasFlag(wxTAB_TRAVERSAL) )
                BaseWindowClass::ToggleWindowStyle(wxTAB_TRAVERSAL);
        }
    }

    WXDLLIMPEXP_INLINE_CORE virtual void RemoveChild(wxWindowBase *child)
    {
#ifndef wxHAS_NATIVE_TAB_TRAVERSAL
        m_container.HandleOnWindowDestroy(child);
#endif // !wxHAS_NATIVE_TAB_TRAVERSAL

        BaseWindowClass::RemoveChild(child);

        // We could reset wxTAB_TRAVERSAL here but it doesn't seem to do any
        // harm to keep it.
        m_container.UpdateCanFocusChildren();
    }

    WXDLLIMPEXP_INLINE_CORE virtual void SetFocus()
    {
        if ( !m_container.DoSetFocus() )
            BaseWindowClass::SetFocus();
    }

    void SetFocusIgnoringChildren()
    {
        BaseWindowClass::SetFocus();
    }

protected:
#ifndef wxHAS_NATIVE_TAB_TRAVERSAL
    void OnNavigationKey(wxNavigationKeyEvent& event)
    {
        m_container.HandleOnNavigationKey(event);
    }

    void OnFocus(wxFocusEvent& event)
    {
        m_container.HandleOnFocus(event);
    }

    void OnChildFocus(wxChildFocusEvent& event)
    {
        m_container.SetLastFocus(event.GetWindow());
        event.Skip();
    }
#endif // !wxHAS_NATIVE_TAB_TRAVERSAL

    wxControlContainer m_container;


    wxDECLARE_NO_COPY_TEMPLATE_CLASS(wxNavigationEnabled, W);
};

// ----------------------------------------------------------------------------
// Compatibility macros from now on, do NOT use them and preferably do not even
// look at them.
// ----------------------------------------------------------------------------

#if WXWIN_COMPATIBILITY_2_8

// common part of WX_DECLARE_CONTROL_CONTAINER in the native and generic cases,
// it should be used in the wxWindow-derived class declaration
#define WX_DECLARE_CONTROL_CONTAINER_BASE()                                   \
public:                                                                       \
    virtual bool AcceptsFocus() const;                                        \
    virtual bool AcceptsFocusRecursively() const;                             \
    virtual bool AcceptsFocusFromKeyboard() const;                            \
    virtual void AddChild(wxWindowBase *child);                               \
    virtual void RemoveChild(wxWindowBase *child);                            \
    virtual void SetFocus();                                                  \
    void SetFocusIgnoringChildren();                                          \
                                                                              \
protected:                                                                    \
    wxControlContainer m_container

// this macro must be used in the derived class ctor
#define WX_INIT_CONTROL_CONTAINER() \
    m_container.SetContainerWindow(this)

// common part of WX_DELEGATE_TO_CONTROL_CONTAINER in the native and generic
// cases, must be used in the wxWindow-derived class implementation
#define WX_DELEGATE_TO_CONTROL_CONTAINER_BASE(classname, basename)            \
    void classname::AddChild(wxWindowBase *child)                             \
    {                                                                         \
        basename::AddChild(child);                                            \
                                                                              \
        m_container.UpdateCanFocusChildren();                                 \
    }                                                                         \
                                                                              \
    bool classname::AcceptsFocusRecursively() const                           \
    {                                                                         \
        return m_container.AcceptsFocusRecursively();                         \
    }                                                                         \
                                                                              \
    void classname::SetFocus()                                                \
    {                                                                         \
        if ( !m_container.DoSetFocus() )                                      \
            basename::SetFocus();                                             \
    }                                                                         \
                                                                              \
    bool classname::AcceptsFocus() const                                      \
    {                                                                         \
        return m_container.AcceptsFocus();                                    \
    }                                                                         \
                                                                              \
    bool classname::AcceptsFocusFromKeyboard() const                          \
    {                                                                         \
        return m_container.AcceptsFocusFromKeyboard();                        \
    }


#ifdef wxHAS_NATIVE_TAB_TRAVERSAL

#define WX_EVENT_TABLE_CONTROL_CONTAINER(classname)

#define WX_DECLARE_CONTROL_CONTAINER WX_DECLARE_CONTROL_CONTAINER_BASE

#define WX_DELEGATE_TO_CONTROL_CONTAINER(classname, basename)                 \
    WX_DELEGATE_TO_CONTROL_CONTAINER_BASE(classname, basename)                \
                                                                              \
    void classname::RemoveChild(wxWindowBase *child)                          \
    {                                                                         \
        basename::RemoveChild(child);                                         \
                                                                              \
        m_container.UpdateCanFocusChildren();                                 \
    }                                                                         \
                                                                              \
    void classname::SetFocusIgnoringChildren()                                \
    {                                                                         \
        basename::SetFocus();                                                 \
    }

#else // !wxHAS_NATIVE_TAB_TRAVERSAL

// declare the methods to be forwarded
#define WX_DECLARE_CONTROL_CONTAINER()                                        \
    WX_DECLARE_CONTROL_CONTAINER_BASE();                                      \
                                                                              \
public:                                                                       \
    void OnNavigationKey(wxNavigationKeyEvent& event);                        \
    void OnFocus(wxFocusEvent& event);                                        \
    virtual void OnChildFocus(wxChildFocusEvent& event)

// implement the event table entries for wxControlContainer
#define WX_EVENT_TABLE_CONTROL_CONTAINER(classname) \
    EVT_SET_FOCUS(classname::OnFocus) \
    EVT_CHILD_FOCUS(classname::OnChildFocus) \
    EVT_NAVIGATION_KEY(classname::OnNavigationKey)

// implement the methods forwarding to the wxControlContainer
#define WX_DELEGATE_TO_CONTROL_CONTAINER(classname, basename)                 \
    WX_DELEGATE_TO_CONTROL_CONTAINER_BASE(classname, basename)                \
                                                                              \
    void classname::RemoveChild(wxWindowBase *child)                          \
    {                                                                         \
        m_container.HandleOnWindowDestroy(child);                             \
                                                                              \
        basename::RemoveChild(child);                                         \
                                                                              \
        m_container.UpdateCanFocusChildren();                                 \
    }                                                                         \
                                                                              \
    void classname::OnNavigationKey( wxNavigationKeyEvent& event )            \
    {                                                                         \
        m_container.HandleOnNavigationKey(event);                             \
    }                                                                         \
                                                                              \
    void classname::SetFocusIgnoringChildren()                                \
    {                                                                         \
        basename::SetFocus();                                                 \
    }                                                                         \
                                                                              \
    void classname::OnChildFocus(wxChildFocusEvent& event)                    \
    {                                                                         \
        m_container.SetLastFocus(event.GetWindow());                          \
        event.Skip();                                                         \
    }                                                                         \
                                                                              \
    void classname::OnFocus(wxFocusEvent& event)                              \
    {                                                                         \
        m_container.HandleOnFocus(event);                                     \
    }

#endif // wxHAS_NATIVE_TAB_TRAVERSAL/!wxHAS_NATIVE_TAB_TRAVERSAL

#endif // WXWIN_COMPATIBILITY_2_8

#endif // _WX_CONTAINR_H_
