/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/toplevel.h
// Purpose:     Top level window, abstraction of wxFrame and wxDialog
// Author:      Vaclav Slavik
// Created:     2006-08-10
// Copyright:   (c) 2006 REA Elektronik GmbH
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_TOPLEVEL_H_
#define _WX_DFB_TOPLEVEL_H_

//-----------------------------------------------------------------------------
// wxTopLevelWindowDFB
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTopLevelWindowDFB : public wxTopLevelWindowBase
{
public:
    // construction
    wxTopLevelWindowDFB() { Init(); }
    wxTopLevelWindowDFB(wxWindow *parent,
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

    // implement base class pure virtuals
    virtual void Maximize(bool maximize = true);
    virtual bool IsMaximized() const;
    virtual void Iconize(bool iconize = true);
    virtual bool IsIconized() const;
    virtual void Restore();

    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL);
    virtual bool IsFullScreen() const { return m_fsIsShowing; }

    virtual bool CanSetTransparent() { return true; }
    virtual bool SetTransparent(wxByte alpha);

    virtual void SetTitle(const wxString &title) { m_title = title; }
    virtual wxString GetTitle() const { return m_title; }

protected:
    // common part of all ctors
    void Init();

    virtual void HandleFocusEvent(const wxDFBWindowEvent& event_);

protected:
    wxString      m_title;

    bool          m_fsIsShowing:1;         /* full screen */
    long          m_fsSaveStyle;
    long          m_fsSaveFlag;
    wxRect        m_fsSaveFrame;

    // is the frame currently maximized?
    bool          m_isMaximized:1;
    wxRect        m_savedFrame;
};

#endif // _WX_DFB_TOPLEVEL_H_
