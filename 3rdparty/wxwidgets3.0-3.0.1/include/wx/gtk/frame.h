/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/frame.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling, Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_FRAME_H_
#define _WX_GTK_FRAME_H_

//-----------------------------------------------------------------------------
// wxFrame
//-----------------------------------------------------------------------------

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

#if wxUSE_STATUSBAR
    void SetStatusBar(wxStatusBar *statbar);
#endif // wxUSE_STATUSBAR

#if wxUSE_TOOLBAR
    void SetToolBar(wxToolBar *toolbar);
#endif // wxUSE_TOOLBAR

    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL);
    wxPoint GetClientAreaOrigin() const { return wxPoint(0, 0); }

#if wxUSE_LIBHILDON || wxUSE_LIBHILDON2
    // in Hildon environment all frames are always shown maximized
    virtual bool IsMaximized() const { return true; }
#endif // wxUSE_LIBHILDON || wxUSE_LIBHILDON2

    // implementation from now on
    // --------------------------

    virtual bool SendIdleEvents(wxIdleEvent& event);

protected:
    // override wxWindow methods to take into account tool/menu/statusbars
    virtual void DoGetClientSize( int *width, int *height ) const;

#if wxUSE_MENUS_NATIVE
    virtual void DetachMenuBar();
    virtual void AttachMenuBar(wxMenuBar *menubar);
#endif // wxUSE_MENUS_NATIVE

private:
    void Init();

    long m_fsSaveFlag;

    DECLARE_DYNAMIC_CLASS(wxFrame)
};

#endif // _WX_GTK_FRAME_H_
