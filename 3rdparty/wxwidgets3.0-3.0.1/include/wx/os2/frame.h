/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/frame.h
// Purpose:     wxFrame class
// Author:      David Webster
// Modified by:
// Created:     10/27/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FRAME_H_
#define _WX_FRAME_H_

//
// Get the default resource ID's for frames
//
#include "wx/os2/wxrsc.h"

class WXDLLIMPEXP_CORE wxFrame : public wxFrameBase
{
public:
    // construction
    wxFrame() { Init(); }
    wxFrame( wxWindow*       pParent
               ,wxWindowID      vId
               ,const wxString& rsTitle
               ,const wxPoint&  rPos = wxDefaultPosition
               ,const wxSize&   rSize = wxDefaultSize
               ,long            lStyle = wxDEFAULT_FRAME_STYLE
               ,const wxString& rsName = wxFrameNameStr
              )
    {
        Init();

        Create(pParent, vId, rsTitle, rPos, rSize, lStyle, rsName);
    }

    bool Create( wxWindow*       pParent
                ,wxWindowID      vId
                ,const wxString& rsTitle
                ,const wxPoint&  rPos = wxDefaultPosition
                ,const wxSize&   rSize = wxDefaultSize
                ,long            lStyle = wxDEFAULT_FRAME_STYLE
                ,const wxString& rsName = wxFrameNameStr
               );

    virtual ~wxFrame();

    // implement base class pure virtuals
#if wxUSE_MENUS_NATIVE
    virtual void SetMenuBar(wxMenuBar* pMenubar);
#endif
    virtual bool ShowFullScreen( bool bShow
                                ,long lStyle = wxFULLSCREEN_ALL
                               );


    // implementation only from now on
    // -------------------------------

    virtual void Raise(void);

    // event handlers
    void OnSysColourChanged(wxSysColourChangedEvent& rEvent);

    // Toolbar
#if wxUSE_TOOLBAR
    virtual wxToolBar* CreateToolBar( long            lStyle = -1
                                     ,wxWindowID      vId = -1
                                     ,const wxString& rsName = wxToolBarNameStr
                                    );

    virtual wxToolBar* OnCreateToolBar( long            lStyle
                                       ,wxWindowID      vId
                                       ,const wxString& rsName
                                      );
    virtual void       PositionToolBar(void);
#endif // wxUSE_TOOLBAR

    // Status bar
#if wxUSE_STATUSBAR
    virtual wxStatusBar* OnCreateStatusBar( int             nNumber = 1
                                           ,long            lStyle = wxSTB_DEFAULT_STYLE
                                           ,wxWindowID      vId = 0
                                           ,const wxString& rsName = wxStatusLineNameStr
                                          );
    virtual void PositionStatusBar(void);

    // Hint to tell framework which status bar to use: the default is to use
    // native one for the platforms which support it (Win32), the generic one
    // otherwise

    // TODO: should this go into a wxFrameworkSettings class perhaps?
    static void UseNativeStatusBar(bool bUseNative)
        { m_bUseNativeStatusBar = bUseNative; }
    static bool UsesNativeStatusBar()
        { return m_bUseNativeStatusBar; }
#endif // wxUSE_STATUSBAR

    WXHMENU GetWinMenu() const { return m_hMenu; }

    // Returns the origin of client area (may be different from (0,0) if the
    // frame has a toolbar)
    virtual wxPoint GetClientAreaOrigin() const;

    // event handlers
    bool HandlePaint(void);
    bool HandleSize( int    nX
                    ,int    nY
                    ,WXUINT uFlag
                   );
    bool HandleCommand( WXWORD wId
                       ,WXWORD wCmd
                       ,WXHWND wControl
                      );
    bool HandleMenuSelect( WXWORD  wItem
                          ,WXWORD  wFlags
                          ,WXHMENU hMenu
                         );

    // tooltip management
#if wxUSE_TOOLTIPS
    WXHWND GetToolTipCtrl(void) const { return m_hWndToolTip; }
    void   SetToolTipCtrl(WXHWND hHwndTT) { m_hWndToolTip = hHwndTT; }
#endif // tooltips

    void      SetClient(WXHWND    c_Hwnd);
    void      SetClient(wxWindow* c_Window);
    wxWindow *GetClient();

 friend MRESULT EXPENTRY wxFrameWndProc(HWND  hWnd,ULONG ulMsg, MPARAM wParam, MPARAM lParam);
 friend MRESULT EXPENTRY wxFrameMainWndProc(HWND  hWnd,ULONG ulMsg, MPARAM wParam, MPARAM lParam);

protected:
    // common part of all ctors
    void         Init(void);

    virtual WXHICON GetDefaultIcon(void) const;
    // override base class virtuals
    virtual void DoGetClientSize( int* pWidth
                                 ,int* pHeight
                                ) const;
    virtual void DoSetClientSize( int nWidth
                                 ,int nWeight
                                );
    inline virtual bool IsMDIChild(void) const { return FALSE; }

#if wxUSE_MENUS_NATIVE
    // helper
    void         DetachMenuBar(void);
    // perform MSW-specific action when menubar is changed
    virtual void AttachMenuBar(wxMenuBar* pMenubar);
    // a plug in for MDI frame classes which need to do something special when
    // the menubar is set
    virtual void InternalSetMenuBar(void);
#endif
    // propagate our state change to all child frames
    void IconizeChildFrames(bool bIconize);

    // we add menu bar accel processing
    bool OS2TranslateMessage(WXMSG* pMsg);

    // window proc for the frames
    MRESULT OS2WindowProc( WXUINT   uMessage
                          ,WXWPARAM wParam
                          ,WXLPARAM lParam
                         );

    bool                            m_bIconized;
    WXHICON                         m_hDefaultIcon;

#if wxUSE_STATUSBAR
    static bool                     m_bUseNativeStatusBar;
#endif // wxUSE_STATUSBAR

    // Data to save/restore when calling ShowFullScreen
    long                            m_lFsStyle;           // Passed to ShowFullScreen
    wxRect                          m_vFsOldSize;
    long                            m_lFsOldWindowStyle;
    int                             m_nFsStatusBarFields; // 0 for no status bar
    int                             m_nFsStatusBarHeight;
    int                             m_nFsToolBarHeight;
    bool                            m_bFsIsMaximized;
    bool                            m_bFsIsShowing;
    bool                            m_bWasMinimized;
    bool                            m_bIsShown;

private:
#if wxUSE_TOOLTIPS
    WXHWND                          m_hWndToolTip;
#endif // tooltips

    //
    // Handles to child windows of the Frame, and the frame itself,
    // that we don't have child objects for (m_hWnd in wxWindow is the
    // handle of the Frame's client window!
    //
    WXHWND                          m_hTitleBar;
    WXHWND                          m_hHScroll;
    WXHWND                          m_hVScroll;

    //
    // Swp structures for various client data
    // DW: Better off in attached RefData?
    //
    SWP                             m_vSwpTitleBar;
    SWP                             m_vSwpMenuBar;
    SWP                             m_vSwpHScroll;
    SWP                             m_vSwpVScroll;
    SWP                             m_vSwpStatusBar;
    SWP                             m_vSwpToolBar;

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxFrame)
};

MRESULT EXPENTRY wxFrameWndProc(HWND  hWnd,ULONG ulMsg, MPARAM wParam, MPARAM lParam);
MRESULT EXPENTRY wxFrameMainWndProc(HWND  hWnd,ULONG ulMsg, MPARAM wParam, MPARAM lParam);
#endif
    // _WX_FRAME_H_

