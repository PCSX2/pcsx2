/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/frame.h
// Purpose:     wxFrame class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MOTIF_FRAME_H_
#define _WX_MOTIF_FRAME_H_

class WXDLLIMPEXP_CORE wxFrame : public wxFrameBase
{
public:
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

    virtual bool Show(bool show = true);

    // Set menu bar
    void SetMenuBar(wxMenuBar *menu_bar);

    // Set title
    void SetTitle(const wxString& title);

    // Set icon
    virtual void SetIcons(const wxIconBundle& icons);

#if wxUSE_STATUSBAR
    virtual void PositionStatusBar();
#endif // wxUSE_STATUSBAR

    // Create toolbar
#if wxUSE_TOOLBAR
    virtual wxToolBar* CreateToolBar(long style = -1,
                                     wxWindowID id = wxID_ANY,
                                     const wxString& name = wxToolBarNameStr);
    virtual void SetToolBar(wxToolBar *toolbar);
    virtual void PositionToolBar();
#endif // wxUSE_TOOLBAR

    // Implementation only from now on
    // -------------------------------

    void OnSysColourChanged(wxSysColourChangedEvent& event);
    void OnActivate(wxActivateEvent& event);

    virtual void ChangeFont(bool keepOriginalSize = true);
    virtual void ChangeBackgroundColour();
    virtual void ChangeForegroundColour();
    WXWidget GetMenuBarWidget() const;
    WXWidget GetShellWidget() const { return m_frameShell; }
    WXWidget GetWorkAreaWidget() const { return m_workArea; }
    WXWidget GetClientAreaWidget() const { return m_clientArea; }
    WXWidget GetTopWidget() const { return m_frameShell; }

    virtual WXWidget GetMainWidget() const { return m_mainWidget; }

    // The widget that can have children on it
    WXWidget GetClientWidget() const;
    bool GetVisibleStatus() const { return m_visibleStatus; }
    void SetVisibleStatus( bool status ) { m_visibleStatus = status; }

    bool PreResize();

    // for generic/mdig.h
    virtual void DoGetClientSize(int *width, int *height) const;

private:
    // common part of all ctors
    void Init();

    // set a single icon for the frame
    void DoSetIcon( const wxIcon& icon );

    //// Motif-specific
    WXWidget              m_frameShell;
    WXWidget              m_workArea;
    WXWidget              m_clientArea;
    bool                  m_visibleStatus;
    bool                  m_iconized;

    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoSetSize(int x, int y,
        int width, int height,
        int sizeFlags = wxSIZE_AUTO);
    virtual void DoSetClientSize(int width, int height);

private:
    virtual bool XmDoCreateTLW(wxWindow* parent,
                               wxWindowID id,
                               const wxString& title,
                               const wxPoint& pos,
                               const wxSize& size,
                               long style,
                               const wxString& name);



    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxFrame)
};

#endif // _WX_MOTIF_FRAME_H_

