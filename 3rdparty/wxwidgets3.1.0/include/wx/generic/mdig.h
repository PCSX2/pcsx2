/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/mdig.h
// Purpose:     Generic MDI (Multiple Document Interface) classes
// Author:      Hans Van Leemputten
// Modified by: 2008-10-31 Vadim Zeitlin: derive from the base classes
// Created:     29/07/2002
// Copyright:   (c) 2002 Hans Van Leemputten
//              (c) 2008 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_MDIG_H_
#define _WX_GENERIC_MDIG_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/panel.h"

class WXDLLIMPEXP_FWD_CORE wxBookCtrlBase;
class WXDLLIMPEXP_FWD_CORE wxBookCtrlEvent;
class WXDLLIMPEXP_FWD_CORE wxIcon;
class WXDLLIMPEXP_FWD_CORE wxIconBundle;
class WXDLLIMPEXP_FWD_CORE wxNotebook;

#if wxUSE_GENERIC_MDI_AS_NATIVE
    #define wxGenericMDIParentFrame wxMDIParentFrame
    #define wxGenericMDIChildFrame wxMDIChildFrame
    #define wxGenericMDIClientWindow wxMDIClientWindow
#else // !wxUSE_GENERIC_MDI_AS_NATIVE
    class WXDLLIMPEXP_FWD_CORE wxGenericMDIParentFrame;
    class WXDLLIMPEXP_FWD_CORE wxGenericMDIChildFrame;
    class WXDLLIMPEXP_FWD_CORE wxGenericMDIClientWindow;
#endif // wxUSE_GENERIC_MDI_AS_NATIVE/!wxUSE_GENERIC_MDI_AS_NATIVE

// ----------------------------------------------------------------------------
// wxGenericMDIParentFrame
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericMDIParentFrame : public wxMDIParentFrameBase
{
public:
    wxGenericMDIParentFrame() { Init(); }
    wxGenericMDIParentFrame(wxWindow *parent,
                     wxWindowID winid,
                     const wxString& title,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     long style = wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL,
                     const wxString& name = wxFrameNameStr)
    {
        Init();

        Create(parent, winid, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID winid,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL,
                const wxString& name = wxFrameNameStr);

    virtual ~wxGenericMDIParentFrame();

    // implement base class pure virtuals
    static bool IsTDI() { return true; }

    virtual void ActivateNext() { AdvanceActive(true); }
    virtual void ActivatePrevious() { AdvanceActive(false); }

#if wxUSE_MENUS
    virtual void SetWindowMenu(wxMenu* pMenu);

    virtual void SetMenuBar(wxMenuBar *pMenuBar);
#endif // wxUSE_MENUS

    virtual wxGenericMDIClientWindow *OnCreateGenericClient();


    // implementation only from now on
    void WXSetChildMenuBar(wxGenericMDIChildFrame *child);
    void WXUpdateChildTitle(wxGenericMDIChildFrame *child);
    void WXActivateChild(wxGenericMDIChildFrame *child);
    void WXRemoveChild(wxGenericMDIChildFrame *child);
    bool WXIsActiveChild(wxGenericMDIChildFrame *child) const;
    bool WXIsInsideChildHandler(wxGenericMDIChildFrame *child) const;

    // return the book control used by the client window to manage the pages
    wxBookCtrlBase *GetBookCtrl() const;

protected:
#if wxUSE_MENUS
    wxMenuBar *m_pMyMenuBar;
#endif // wxUSE_MENUS

    // advance the activation forward or backwards
    void AdvanceActive(bool forward);

private:
    void Init();

#if wxUSE_MENUS
    void RemoveWindowMenu(wxMenuBar *pMenuBar);
    void AddWindowMenu(wxMenuBar *pMenuBar);

    void OnWindowMenu(wxCommandEvent& event);
#endif // wxUSE_MENUS

    virtual bool ProcessEvent(wxEvent& event);

    void OnClose(wxCloseEvent& event);

    // return the client window, may be NULL if we hadn't been created yet
    wxGenericMDIClientWindow *GetGenericClientWindow() const;

    // close all children, return false if any of them vetoed it
    bool CloseAll();


    // this pointer is non-NULL if we're currently inside our ProcessEvent()
    // and we forwarded the event to this child (as we do with menu events)
    wxMDIChildFrameBase *m_childHandler;

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxGenericMDIParentFrame);
};

// ----------------------------------------------------------------------------
// wxGenericMDIChildFrame
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericMDIChildFrame : public wxTDIChildFrame
{
public:
    wxGenericMDIChildFrame() { Init(); }
    wxGenericMDIChildFrame(wxGenericMDIParentFrame *parent,
                           wxWindowID winid,
                           const wxString& title,
                           const wxPoint& pos = wxDefaultPosition,
                           const wxSize& size = wxDefaultSize,
                           long style = wxDEFAULT_FRAME_STYLE,
                           const wxString& name = wxFrameNameStr)
    {
        Init();

        Create(parent, winid, title, pos, size, style, name);
    }

    bool Create(wxGenericMDIParentFrame *parent,
                wxWindowID winid,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    virtual ~wxGenericMDIChildFrame();

    // implement MDI operations
    virtual void Activate();


#if wxUSE_MENUS
    virtual void SetMenuBar( wxMenuBar *menu_bar );
    virtual wxMenuBar *GetMenuBar() const;
#endif // wxUSE_MENUS

    virtual wxString GetTitle() const { return m_title; }
    virtual void SetTitle(const wxString& title);

    virtual bool TryAfter(wxEvent& event);

    // implementation only from now on

    wxGenericMDIParentFrame* GetGenericMDIParent() const
    {
#if wxUSE_GENERIC_MDI_AS_NATIVE
        return GetMDIParent();
#else // generic != native
        return m_mdiParentGeneric;
#endif
    }

protected:
    wxString m_title;

#if wxUSE_MENUS
    wxMenuBar        *m_pMenuBar;
#endif // wxUSE_MENUS

#if !wxUSE_GENERIC_MDI_AS_NATIVE
    wxGenericMDIParentFrame *m_mdiParentGeneric;
#endif

protected:
    void Init();

private:
    void OnMenuHighlight(wxMenuEvent& event);
    void OnClose(wxCloseEvent& event);

    wxDECLARE_DYNAMIC_CLASS(wxGenericMDIChildFrame);
    wxDECLARE_EVENT_TABLE();

    friend class wxGenericMDIClientWindow;
};

// ----------------------------------------------------------------------------
// wxGenericMDIClientWindow
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericMDIClientWindow : public wxMDIClientWindowBase
{
public:
    wxGenericMDIClientWindow() { }

    // unfortunately we need to provide our own version of CreateClient()
    // because of the difference in the type of the first parameter and
    // implement the base class pure virtual method in terms of it
    // (CreateGenericClient() is virtual itself to allow customizing the client
    // window creation by overriding it in the derived classes)
    virtual bool CreateGenericClient(wxWindow *parent);
    virtual bool CreateClient(wxMDIParentFrame *parent,
                              long WXUNUSED(style) = wxVSCROLL | wxHSCROLL)
    {
        return CreateGenericClient(parent);
    }

    // implementation only
    wxBookCtrlBase *GetBookCtrl() const;
    wxGenericMDIChildFrame *GetChild(size_t pos) const;
    int FindChild(wxGenericMDIChildFrame *child) const;

private:
    void PageChanged(int OldSelection, int newSelection);

    void OnPageChanged(wxBookCtrlEvent& event);
    void OnSize(wxSizeEvent& event);

    // the notebook containing all MDI children as its pages
    wxNotebook *m_notebook;

    wxDECLARE_DYNAMIC_CLASS(wxGenericMDIClientWindow);
};

// ----------------------------------------------------------------------------
// inline functions implementation
// ----------------------------------------------------------------------------

inline bool
wxGenericMDIParentFrame::
WXIsInsideChildHandler(wxGenericMDIChildFrame *child) const
{
    return child == m_childHandler;
}

#endif // _WX_GENERIC_MDIG_H_
