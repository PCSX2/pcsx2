///////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/toplevel.h
// Purpose:     wxTopLevelWindowOS2 is the OS2 implementation of wxTLW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.09.01
// Copyright:   (c) 2001 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_TOPLEVEL_H_
#define _WX_MSW_TOPLEVEL_H_

enum ETemplateID
{
    kResizeableDialog = 130,
    kCaptionDialog,
    kNoCaptionDialog
};

// ----------------------------------------------------------------------------
// wxTopLevelWindowOS2
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTopLevelWindowOS2 : public wxTopLevelWindowBase
{
public:
    // constructors and such
    wxTopLevelWindowOS2() { Init(); }

    wxTopLevelWindowOS2( wxWindow*       pParent
                        ,wxWindowID      vId
                        ,const wxString& rsTitle
                        ,const wxPoint&  rPos = wxDefaultPosition
                        ,const wxSize&   rSize = wxDefaultSize
                        ,long            lStyle = wxDEFAULT_FRAME_STYLE
                        ,const wxString& rsName = wxFrameNameStr
                       )
    {
        Init();

        (void)Create(pParent, vId, rsTitle, rPos, rSize, lStyle, rsName);
    }

    bool Create( wxWindow*       pParent
                ,wxWindowID      vId
                ,const wxString& rsTitle
                ,const wxPoint&  rPos = wxDefaultPosition
                ,const wxSize&   rSize = wxDefaultSize
                ,long            lStyle = wxDEFAULT_FRAME_STYLE
                ,const wxString& rsName = wxFrameNameStr
               );

    virtual ~wxTopLevelWindowOS2();

    //
    // Implement base class pure virtuals
    //
    virtual void SetTitle( const wxString& title);
    virtual wxString GetTitle() const;

    virtual void Iconize(bool bIconize = true);
    virtual bool IsFullScreen(void) const { return m_bFsIsShowing; }
    virtual bool IsIconized(void) const;
    virtual bool IsMaximized(void) const;
    virtual void Maximize(bool bMaximize = true);
    virtual void Restore(void);
    virtual void SendSizeEvent(int flags = 0);
    virtual void SetIcons(const wxIconBundle& rIcons);

    virtual bool Show(bool bShow = true);
    virtual bool ShowFullScreen( bool bShow,
                                 long lStyle = wxFULLSCREEN_ALL );

    //
    // EnableCloseButton(false) may be used to remove the "Close"
    // button from the title bar
    //
    bool EnableCloseButton(bool bEnable = true);
    HWND GetFrame(void) const { return m_hFrame; }

    //
    // Implementation from now on
    // --------------------------
    //
    PSWP         GetSwpClient(void) { return &m_vSwpClient; }

    void         OnActivate(wxActivateEvent& rEvent);

    void         SetLastFocus(wxWindow *pWin) { m_pWinLastFocused = pWin; }
    wxWindow*    GetLastFocus(void) const { return m_pWinLastFocused; }

protected:

    //
    // Common part of all ctors
    //
    void Init(void);

    //
    // Create a new frame, return false if it couldn't be created
    //
    bool CreateFrame( const wxString& rsTitle
                     ,const wxPoint&  rPos
                     ,const wxSize&   rSize
                    );

    //
    // Create a new dialog using the given dialog template from resources,
    // return false if it couldn't be created
    //
    bool CreateDialog( ULONG           ulDlgTemplate
                      ,const wxString& rsTitle
                      ,const wxPoint&  rPos
                      ,const wxSize&   rSize
                     );

    //
    // Common part of Iconize(), Maximize() and Restore()
    //
    void DoShowWindow(int nShowCmd);

    //
    // Implement the geometry-related methods for a top level window
    //
    virtual void DoSetClientSize( int nWidth
                                 ,int nHeight
                                );
    virtual void DoGetClientSize( int* pnWidth
                                 ,int* pnHeight
                                ) const;

    //
    // Translate wxWidgets flags into OS flags
    //
    virtual WXDWORD OS2GetStyle( long     lFlag
                                ,WXDWORD* pdwExstyle
                               ) const;

    //
    // Choose the right parent to use with CreateWindow()
    //
    virtual WXHWND  OS2GetParent(void) const;

    //
    // Is the frame currently iconized?
    //
    bool m_bIconized;

    //
    // Should the frame be maximized when it will be shown? set by Maximize()
    // when it is called while the frame is hidden
    //
    bool   m_bMaximizeOnShow;

    //
    // Data to save/restore when calling ShowFullScreen
    //
    long   m_lFsStyle; // Passed to ShowFullScreen
    wxRect m_vFsOldSize;
    long   m_lFsOldWindowStyle;
    bool   m_bFsIsMaximized;
    bool   m_bFsIsShowing;

    wxWindow* m_pWinLastFocused;

    WXHWND m_hFrame;
    SWP    m_vSwp;
    SWP    m_vSwpClient;
    static bool m_sbInitialized;
    static wxWindow* m_spHiddenParent;

    DECLARE_EVENT_TABLE()
}; // end of CLASS wxTopLevelWindowOS2

#endif // _WX_MSW_TOPLEVEL_H_
