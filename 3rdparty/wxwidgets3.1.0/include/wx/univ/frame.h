///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/frame.h
// Purpose:     wxFrame class for wxUniversal
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.05.01
// Copyright:   (c) 2001 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_FRAME_H_
#define _WX_UNIV_FRAME_H_

// ----------------------------------------------------------------------------
// wxFrame
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFrame : public wxFrameBase
{
public:
    wxFrame() {}
    wxFrame(wxWindow *parent,
            wxWindowID id,
            const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxDEFAULT_FRAME_STYLE,
            const wxString& name = wxFrameNameStr)
    {
        Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    virtual wxPoint GetClientAreaOrigin() const wxOVERRIDE;
    virtual bool Enable(bool enable = true) wxOVERRIDE;

#if wxUSE_STATUSBAR
    virtual wxStatusBar* CreateStatusBar(int number = 1,
                                         long style = wxSTB_DEFAULT_STYLE,
                                         wxWindowID id = 0,
                                         const wxString& name = wxStatusLineNameStr) wxOVERRIDE;
#endif // wxUSE_STATUSBAR

#if wxUSE_TOOLBAR
    // create main toolbar bycalling OnCreateToolBar()
    virtual wxToolBar* CreateToolBar(long style = -1,
                                     wxWindowID id = wxID_ANY,
                                     const wxString& name = wxToolBarNameStr) wxOVERRIDE;
#endif // wxUSE_TOOLBAR

    virtual wxSize GetMinSize() const wxOVERRIDE;

protected:
    void OnSize(wxSizeEvent& event);
    void OnSysColourChanged(wxSysColourChangedEvent& event);

    virtual void DoGetClientSize(int *width, int *height) const wxOVERRIDE;
    virtual void DoSetClientSize(int width, int height) wxOVERRIDE;

#if wxUSE_MENUS
    // override to update menu bar position when the frame size changes
    virtual void PositionMenuBar() wxOVERRIDE;
    virtual void DetachMenuBar() wxOVERRIDE;
    virtual void AttachMenuBar(wxMenuBar *menubar) wxOVERRIDE;
#endif // wxUSE_MENUS

#if wxUSE_STATUSBAR
    // override to update statusbar position when the frame size changes
    virtual void PositionStatusBar() wxOVERRIDE;
#endif // wxUSE_MENUS

protected:
#if wxUSE_TOOLBAR
    virtual void PositionToolBar() wxOVERRIDE;
#endif // wxUSE_TOOLBAR

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxFrame);
};

#endif // _WX_UNIV_FRAME_H_
