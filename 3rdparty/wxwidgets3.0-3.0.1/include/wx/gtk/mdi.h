/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/mdi.h
// Purpose:     TDI-based MDI implementation for wxGTK
// Author:      Robert Roebling
// Modified by: 2008-10-31 Vadim Zeitlin: derive from the base classes
// Copyright:   (c) 1998 Robert Roebling
//              (c) 2008 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_MDI_H_
#define _WX_GTK_MDI_H_

#include "wx/frame.h"

class WXDLLIMPEXP_FWD_CORE wxMDIChildFrame;
class WXDLLIMPEXP_FWD_CORE wxMDIClientWindow;

typedef struct _GtkNotebook GtkNotebook;

//-----------------------------------------------------------------------------
// wxMDIParentFrame
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMDIParentFrame : public wxMDIParentFrameBase
{
public:
    wxMDIParentFrame() { Init(); }
    wxMDIParentFrame(wxWindow *parent,
                     wxWindowID id,
                     const wxString& title,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     long style = wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL,
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
                long style = wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL,
                const wxString& name = wxFrameNameStr);

    // we don't store the active child in m_currentChild unlike the base class
    // version so override this method to find it dynamically
    virtual wxMDIChildFrame *GetActiveChild() const;

    // implement base class pure virtuals
    // ----------------------------------

    virtual void ActivateNext();
    virtual void ActivatePrevious();

    static bool IsTDI() { return true; }

    // implementation

    bool                m_justInserted;

    virtual void OnInternalIdle();

protected:
    virtual void DoGetClientSize(int* width, int* height) const;

private:
    friend class wxMDIChildFrame;
    void Init();

    DECLARE_DYNAMIC_CLASS(wxMDIParentFrame)
};

//-----------------------------------------------------------------------------
// wxMDIChildFrame
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMDIChildFrame : public wxTDIChildFrame
{
public:
    wxMDIChildFrame() { Init(); }
    wxMDIChildFrame(wxMDIParentFrame *parent,
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

    bool Create(wxMDIParentFrame *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    virtual ~wxMDIChildFrame();

    virtual void SetMenuBar( wxMenuBar *menu_bar );
    virtual wxMenuBar *GetMenuBar() const;

    virtual void Activate();

    virtual void SetTitle(const wxString& title);

    // implementation

    void OnActivate( wxActivateEvent& event );
    void OnMenuHighlight( wxMenuEvent& event );
    virtual void GTKHandleRealized();

    wxMenuBar         *m_menuBar;
    bool               m_justInserted;

private:
    void Init();

    GtkNotebook *GTKGetNotebook() const;

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxMDIChildFrame)
};

//-----------------------------------------------------------------------------
// wxMDIClientWindow
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMDIClientWindow : public wxMDIClientWindowBase
{
public:
    wxMDIClientWindow() { }
    ~wxMDIClientWindow();

    virtual bool CreateClient(wxMDIParentFrame *parent,
                              long style = wxVSCROLL | wxHSCROLL);

private:
    virtual void AddChildGTK(wxWindowGTK* child);

    DECLARE_DYNAMIC_CLASS(wxMDIClientWindow)
};

#endif // _WX_GTK_MDI_H_
