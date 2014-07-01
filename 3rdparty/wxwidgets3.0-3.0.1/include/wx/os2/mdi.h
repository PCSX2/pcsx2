/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/mdi.h
// Purpose:     MDI (Multiple Document Interface) classes.
//              This doesn't have to be implemented just like Windows,
//              it could be a tabbed design as in wxGTK.
// Author:      David Webster
// Modified by:
// Created:     10/10/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MDI_H_
#define _WX_MDI_H_

#include "wx/frame.h"

class WXDLLIMPEXP_FWD_CORE wxMDIClientWindow;
class WXDLLIMPEXP_FWD_CORE wxMDIChildFrame;

class WXDLLIMPEXP_CORE wxMDIParentFrame: public wxFrame
{
DECLARE_DYNAMIC_CLASS(wxMDIParentFrame)

  friend class WXDLLIMPEXP_FWD_CORE wxMDIChildFrame;
public:

  wxMDIParentFrame();
  inline wxMDIParentFrame(wxWindow *parent,
           wxWindowID id,
           const wxString& title,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL,  // Scrolling refers to client window
           const wxString& name = wxFrameNameStr)
  {
      Create(parent, id, title, pos, size, style, name);
  }

  virtual ~wxMDIParentFrame();

  bool Create(wxWindow *parent,
           wxWindowID id,
           const wxString& title,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL,
           const wxString& name = wxFrameNameStr);

    // accessors
    // ---------

    // Get the active MDI child window (Windows only)
    wxMDIChildFrame *GetActiveChild() const;

    // Get the client window
    wxMDIClientWindow *GetClientWindow() const { return m_clientWindow; }

    // Create the client window class (don't Create the window,
    // just return a new class)
    virtual wxMDIClientWindow *OnCreateClient(void);

    wxMenu* GetWindowMenu() const { return m_windowMenu; }
//    void    SetWindowMenu(wxMwnu* pMenu);

    // MDI operations
    // --------------
    virtual void Cascade();
    virtual void Tile();
    virtual void ArrangeIcons();
    virtual void ActivateNext();
    virtual void ActivatePrevious();

    // handlers
    // --------

    // Responds to colour changes
    void OnSysColourChanged(wxSysColourChangedEvent& event);

    void OnSize(wxSizeEvent& event);

    bool HandleActivate(int state, bool minimized, WXHWND activate);
    bool HandleCommand(WXWORD id, WXWORD cmd, WXHWND control);

    // override window proc for MDI-specific message processing
    virtual MRESULT OS2WindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);

    virtual MRESULT OS2DefWindowProc(WXUINT, WXWPARAM, WXLPARAM);
    virtual bool OS2TranslateMessage(WXMSG* msg);

protected:
    virtual void InternalSetMenuBar();

    wxMDIClientWindow *             m_clientWindow;
    wxMDIChildFrame *               m_currentChild;
    wxMenu*                         m_windowMenu;

    // TRUE if MDI Frame is intercepting commands, not child
    bool m_parentFrameActive;

private:
    DECLARE_EVENT_TABLE()
};

class WXDLLIMPEXP_CORE wxMDIChildFrame: public wxFrame
{
DECLARE_DYNAMIC_CLASS(wxMDIChildFrame)
public:

  wxMDIChildFrame();
  inline wxMDIChildFrame(wxMDIParentFrame *parent,
           wxWindowID id,
           const wxString& title,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = wxDEFAULT_FRAME_STYLE,
           const wxString& name = wxFrameNameStr)
  {
      Create(parent, id, title, pos, size, style, name);
  }

  virtual ~wxMDIChildFrame();

  bool Create(wxMDIParentFrame *parent,
           wxWindowID id,
           const wxString& title,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = wxDEFAULT_FRAME_STYLE,
           const wxString& name = wxFrameNameStr);

    // MDI operations
    virtual void Maximize(bool maximize = TRUE);
    virtual void Restore();
    virtual void Activate();

    // Handlers

    bool HandleMDIActivate(long bActivate, WXHWND, WXHWND);
    bool HandleSize(int x, int y, WXUINT);
    bool HandleWindowPosChanging(void *lpPos);
    bool HandleCommand(WXWORD id, WXWORD cmd, WXHWND control);

    virtual MRESULT OS2WindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam);
    virtual MRESULT OS2DefWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam);
    virtual bool OS2TranslateMessage(WXMSG *msg);

    virtual void OS2DestroyWindow();

    // Implementation
    bool ResetWindowStyle(void *vrect);

protected:
    virtual void DoGetPosition(int *x, int *y) const;
    virtual void DoSetClientSize(int width, int height);
    virtual void InternalSetMenuBar();
};

/* The client window is a child of the parent MDI frame, and itself
 * contains the child MDI frames.
 * However, you create the MDI children as children of the MDI parent:
 * only in the implementation does the client window become the parent
 * of the children. Phew! So the children are sort of 'adopted'...
 */

class WXDLLIMPEXP_CORE wxMDIClientWindow: public wxWindow
{
  DECLARE_DYNAMIC_CLASS(wxMDIClientWindow)

 public:

    wxMDIClientWindow() { Init(); }
    wxMDIClientWindow(wxMDIParentFrame *parent, long style = 0)
    {
        Init();

        CreateClient(parent, style);
    }

    // Note: this is virtual, to allow overridden behaviour.
    virtual bool CreateClient(wxMDIParentFrame *parent,
                              long style = wxVSCROLL | wxHSCROLL);

    // Explicitly call default scroll behaviour
    void OnScroll(wxScrollEvent& event);

protected:
    void Init() { m_scrollX = m_scrollY = 0; }

    int m_scrollX, m_scrollY;

private:
    DECLARE_EVENT_TABLE()
};

#endif
    // _WX_MDI_H_
