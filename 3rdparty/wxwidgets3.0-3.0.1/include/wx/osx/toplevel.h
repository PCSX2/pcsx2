///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/toplevel.h
// Purpose:     wxTopLevelWindowMac is the Mac implementation of wxTLW
// Author:      Stefan Csomor
// Modified by:
// Created:     20.09.01
// Copyright:   (c) 2001 Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_TOPLEVEL_H_
#define _WX_MSW_TOPLEVEL_H_

// ----------------------------------------------------------------------------
// wxTopLevelWindowMac
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTopLevelWindowMac : public wxTopLevelWindowBase
{
public:
    // constructors and such
    wxTopLevelWindowMac() { Init(); }

    wxTopLevelWindowMac(wxWindow *parent,
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

    virtual ~wxTopLevelWindowMac();

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    bool Create(wxWindow *parent, WXWindow nativeWindow);

    virtual bool Destroy();

    virtual wxPoint GetClientAreaOrigin() const;

    // Attracts the users attention to this window if the application is
    // inactive (should be called when a background event occurs)
    virtual void RequestUserAttention(int flags = wxUSER_ATTENTION_INFO);

    // implement base class pure virtuals
    virtual void Maximize(bool maximize = true);
    virtual bool IsMaximized() const;
    virtual void Iconize(bool iconize = true);
    virtual bool IsIconized() const;
    virtual void Restore();

    virtual bool IsActive();

    virtual void ShowWithoutActivating();
    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL) ;
    virtual bool IsFullScreen() const ;

    // implementation from now on
    // --------------------------

    virtual void SetTitle( const wxString& title);
    virtual wxString GetTitle() const;

    virtual void SetLabel(const wxString& label) { SetTitle( label ); }
    virtual wxString GetLabel() const            { return GetTitle(); }
    
    virtual void OSXSetModified(bool modified);
    virtual bool OSXIsModified() const;

    virtual void SetRepresentedFilename(const wxString& filename);

protected:
    // common part of all ctors
    void Init();

    // is the frame currently iconized?
    bool m_iconized;

    // should the frame be maximized when it will be shown? set by Maximize()
    // when it is called while the frame is hidden
    bool m_maximizeOnShow;
private :
    DECLARE_EVENT_TABLE()
};

#endif // _WX_MSW_TOPLEVEL_H_
