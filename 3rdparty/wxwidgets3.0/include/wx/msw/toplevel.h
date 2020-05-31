///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/toplevel.h
// Purpose:     wxTopLevelWindowMSW is the MSW implementation of wxTLW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.09.01
// Copyright:   (c) 2001 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_TOPLEVEL_H_
#define _WX_MSW_TOPLEVEL_H_

// ----------------------------------------------------------------------------
// wxTopLevelWindowMSW
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTopLevelWindowMSW : public wxTopLevelWindowBase
{
public:
    // constructors and such
    wxTopLevelWindowMSW() { Init(); }

    wxTopLevelWindowMSW(wxWindow *parent,
                        wxWindowID id,
                        const wxString& title,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = wxDEFAULT_FRAME_STYLE,
                        const wxString& name = wxFrameNameStr)
    {
        Init();

        (void)Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    virtual ~wxTopLevelWindowMSW();

    // implement base class pure virtuals
    virtual void SetTitle( const wxString& title);
    virtual wxString GetTitle() const;
    virtual void Maximize(bool maximize = true);
    virtual bool IsMaximized() const;
    virtual void Iconize(bool iconize = true);
    virtual bool IsIconized() const;
    virtual void SetIcons(const wxIconBundle& icons );
    virtual void Restore();

    virtual void SetLayoutDirection(wxLayoutDirection dir);

    virtual void RequestUserAttention(int flags = wxUSER_ATTENTION_INFO);

    virtual bool Show(bool show = true);
    virtual void Raise();

    virtual void ShowWithoutActivating();
    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL);
    virtual bool IsFullScreen() const { return m_fsIsShowing; }

    // wxMSW only: EnableCloseButton(false) may be used to remove the "Close"
    // button from the title bar
    virtual bool EnableCloseButton(bool enable = true);

    // Set window transparency if the platform supports it
    virtual bool SetTransparent(wxByte alpha);
    virtual bool CanSetTransparent();


    // MSW-specific methods
    // --------------------

    // Return the menu representing the "system" menu of the window. You can
    // call wxMenu::AppendWhatever() methods on it but removing items from it
    // is in general not a good idea.
    //
    // The pointer returned by this method belongs to the window and will be
    // deleted when the window itself is, do not delete it yourself. May return
    // NULL if getting the system menu failed.
    wxMenu *MSWGetSystemMenu() const;


    // implementation from now on
    // --------------------------

    // event handlers
    void OnActivate(wxActivateEvent& event);

    // called by wxWindow whenever it gets focus
    void SetLastFocus(wxWindow *win) { m_winLastFocused = win; }
    wxWindow *GetLastFocus() const { return m_winLastFocused; }

#if defined(__SMARTPHONE__) && defined(__WXWINCE__)
    virtual void SetLeftMenu(int id = wxID_ANY, const wxString& label = wxEmptyString, wxMenu *subMenu = NULL);
    virtual void SetRightMenu(int id = wxID_ANY, const wxString& label = wxEmptyString, wxMenu *subMenu = NULL);
    bool HandleCommand(WXWORD id, WXWORD cmd, WXHWND control);
    virtual bool MSWShouldPreProcessMessage(WXMSG* pMsg);
#endif // __SMARTPHONE__ && __WXWINCE__

#if defined(__SMARTPHONE__) || defined(__POCKETPC__)
    // Soft Input Panel (SIP) change notification
    virtual bool HandleSettingChange(WXWPARAM wParam, WXLPARAM lParam);
#endif

    // translate wxWidgets flags to Windows ones
    virtual WXDWORD MSWGetStyle(long flags, WXDWORD *exstyle) const;

    // choose the right parent to use with CreateWindow()
    virtual WXHWND MSWGetParent() const;

    // window proc for the frames
    WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam);

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

#if wxUSE_MENUS
    bool HandleMenuSelect(WXWORD nItem, WXWORD nFlags, WXHMENU hMenu);

    // handle WM_EXITMENULOOP message for Win95 only
    bool HandleExitMenuLoop(WXWORD isPopup);

    // handle WM_(UN)INITMENUPOPUP message to generate wxEVT_MENU_OPEN/CLOSE
    bool HandleMenuPopup(wxEventType evtType, WXHMENU hMenu);

    // Command part of HandleMenuPopup() and HandleExitMenuLoop().
    bool DoSendMenuOpenCloseEvent(wxEventType evtType, wxMenu* menu, bool popup);

    // Find the menu corresponding to the given handle.
    virtual wxMenu* MSWFindMenuFromHMENU(WXHMENU hMenu);
#endif // wxUSE_MENUS

protected:
    // common part of all ctors
    void Init();

    // create a new frame, return false if it couldn't be created
    bool CreateFrame(const wxString& title,
                     const wxPoint& pos,
                     const wxSize& size);

    // create a new dialog using the given dialog template from resources,
    // return false if it couldn't be created
    bool CreateDialog(const void *dlgTemplate,
                      const wxString& title,
                      const wxPoint& pos,
                      const wxSize& size);

    // common part of Iconize(), Maximize() and Restore()
    void DoShowWindow(int nShowCmd);

    // override those to return the normal window coordinates even when the
    // window is minimized
#ifndef __WXWINCE__
    virtual void DoGetPosition(int *x, int *y) const;
    virtual void DoGetSize(int *width, int *height) const;
#endif // __WXWINCE__

    // Top level windows have different freeze semantics on Windows
    virtual void DoFreeze();
    virtual void DoThaw();

    // helper of SetIcons(): calls gets the icon with the size specified by the
    // given system metrics (SM_C{X|Y}[SM]ICON) from the bundle and sets it
    // using WM_SETICON with the specified wParam (ICOM_SMALL or ICON_BIG);
    // returns true if the icon was set
    bool DoSelectAndSetIcon(const wxIconBundle& icons, int smX, int smY, int i);

    // override wxWindow virtual method to use CW_USEDEFAULT if necessary
    virtual void MSWGetCreateWindowCoords(const wxPoint& pos,
                                          const wxSize& size,
                                          int& x, int& y,
                                          int& w, int& h) const;


    // is the window currently iconized?
    bool m_iconized;

    // should the frame be maximized when it will be shown? set by Maximize()
    // when it is called while the frame is hidden
    bool m_maximizeOnShow;

    // Data to save/restore when calling ShowFullScreen
    long                  m_fsStyle; // Passed to ShowFullScreen
    wxRect                m_fsOldSize;
    long                  m_fsOldWindowStyle;
    bool                  m_fsIsMaximized;
    bool                  m_fsIsShowing;

    // Save the current focus to m_winLastFocused if we're not iconized (the
    // focus is always NULL when we're iconized).
    void DoSaveLastFocus();

    // Restore focus to m_winLastFocused if possible and needed.
    void DoRestoreLastFocus();

    // The last focused child: we remember it when we're deactivated and
    // restore focus to it when we're activated (this is done here) or restored
    // from iconic state (done by wxFrame).
    wxWindow             *m_winLastFocused;

#if defined(__SMARTPHONE__) && defined(__WXWINCE__)
    class ButtonMenu
    {
    public:
        ButtonMenu();
        ~ButtonMenu();

        void SetButton(int id = wxID_ANY,
                       const wxString& label  = wxEmptyString,
                       wxMenu *subMenu = NULL);

        bool IsAssigned() const {return m_assigned;}
        bool IsMenu() const {return m_menu!=NULL;}

        int GetId() const {return m_id;}
        wxMenu* GetMenu() const {return m_menu;}
        wxString GetLabel() {return m_label;}

        static wxMenu *DuplicateMenu(wxMenu *menu);

    protected:
        int      m_id;
        wxString m_label;
        wxMenu  *m_menu;
        bool     m_assigned;
    };

    ButtonMenu               m_LeftButton;
    ButtonMenu               m_RightButton;
    HWND                     m_MenuBarHWND;

    void ReloadButton(ButtonMenu& button, UINT menuID);
    void ReloadAllButtons();
#endif // __SMARTPHONE__ && __WXWINCE__

private:

#if defined(__SMARTPHONE__) || defined(__POCKETPC__)
    void* m_activateInfo;
#endif

    // The system menu: initially NULL but can be set (once) by
    // MSWGetSystemMenu(). Owned by this window.
    wxMenu *m_menuSystem;

    // The number of currently opened menus: 0 initially, 1 when a top level
    // menu is opened, 2 when its submenu is opened and so on.
    int m_menuDepth;

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxTopLevelWindowMSW);
};

#endif // _WX_MSW_TOPLEVEL_H_
