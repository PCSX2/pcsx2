/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/frame.h
// Purpose:     wxFrame class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FRAME_H_
#define _WX_FRAME_H_

class WXDLLIMPEXP_CORE wxFrame : public wxFrameBase
{
public:
    // construction
    wxFrame() { Init(); }
    wxFrame(wxWindow *parent,
            wxWindowID id,
            const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxDEFAULT_FRAME_STYLE,
            const wxString& name = wxFrameNameStr)
    {
        Init();

        Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    virtual ~wxFrame();

    // implement base class pure virtuals
    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL);

    // implementation only from now on
    // -------------------------------

    // event handlers
    void OnSysColourChanged(wxSysColourChangedEvent& event);

    // Toolbar
#if wxUSE_TOOLBAR
    virtual wxToolBar* CreateToolBar(long style = -1,
                                     wxWindowID id = wxID_ANY,
                                     const wxString& name = wxToolBarNameStr);
#endif // wxUSE_TOOLBAR

    // Status bar
#if wxUSE_STATUSBAR
    virtual wxStatusBar* OnCreateStatusBar(int number = 1,
                                           long style = wxSTB_DEFAULT_STYLE,
                                           wxWindowID id = 0,
                                           const wxString& name = wxStatusLineNameStr);

    // Hint to tell framework which status bar to use: the default is to use
    // native one for the platforms which support it (Win32), the generic one
    // otherwise

    // TODO: should this go into a wxFrameworkSettings class perhaps?
    static void UseNativeStatusBar(bool useNative)
        { m_useNativeStatusBar = useNative; }
    static bool UsesNativeStatusBar()
        { return m_useNativeStatusBar; }
#endif // wxUSE_STATUSBAR

    // event handlers
    bool HandleSize(int x, int y, WXUINT flag);
    bool HandleCommand(WXWORD id, WXWORD cmd, WXHWND control);

    // tooltip management
#if wxUSE_TOOLTIPS
    WXHWND GetToolTipCtrl() const { return m_hwndToolTip; }
    void SetToolTipCtrl(WXHWND hwndTT) { m_hwndToolTip = hwndTT; }
#endif // tooltips

    // override the base class function to handle iconized/maximized frames
    virtual void SendSizeEvent(int flags = 0);

    virtual wxPoint GetClientAreaOrigin() const;

    // override base class version to add menu bar accel processing
    virtual bool MSWTranslateMessage(WXMSG *msg)
    {
        return MSWDoTranslateMessage(this, msg);
    }

    // window proc for the frames
    virtual WXLRESULT MSWWindowProc(WXUINT message,
                                    WXWPARAM wParam,
                                    WXLPARAM lParam);

#if wxUSE_MENUS
    // get the currently active menu: this is the same as the frame menu for
    // normal frames but is overridden by wxMDIParentFrame
    virtual WXHMENU MSWGetActiveMenu() const { return m_hMenu; }

    // Look up the menu in the menu bar.
    virtual wxMenu* MSWFindMenuFromHMENU(WXHMENU hMenu);
#endif // wxUSE_MENUS

protected:
    // common part of all ctors
    void Init();

    // override base class virtuals
    virtual void DoGetClientSize(int *width, int *height) const;
    virtual void DoSetClientSize(int width, int height);

#if wxUSE_MENUS_NATIVE
    // perform MSW-specific action when menubar is changed
    virtual void AttachMenuBar(wxMenuBar *menubar);

    // a plug in for MDI frame classes which need to do something special when
    // the menubar is set
    virtual void InternalSetMenuBar();
#endif // wxUSE_MENUS_NATIVE

    // propagate our state change to all child frames
    void IconizeChildFrames(bool bIconize);

    // the real implementation of MSWTranslateMessage(), also used by
    // wxMDIChildFrame
    bool MSWDoTranslateMessage(wxFrame *frame, WXMSG *msg);

    virtual bool IsMDIChild() const { return false; }

    // get default (wxWidgets) icon for the frame
    virtual WXHICON GetDefaultIcon() const;

#if wxUSE_TOOLBAR
    virtual void PositionToolBar();
#endif // wxUSE_TOOLBAR

#if wxUSE_STATUSBAR
    virtual void PositionStatusBar();

    static bool           m_useNativeStatusBar;
#endif // wxUSE_STATUSBAR

#if wxUSE_MENUS
    // frame menu, NULL if none
    WXHMENU m_hMenu;
#endif // wxUSE_MENUS

private:
#if wxUSE_TOOLTIPS
    WXHWND                m_hwndToolTip;
#endif // tooltips

    // used by IconizeChildFrames(), see comments there
    bool m_wasMinimized;

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxFrame)
};

#endif
    // _WX_FRAME_H_
