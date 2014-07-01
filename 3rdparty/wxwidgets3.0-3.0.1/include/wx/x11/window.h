/////////////////////////////////////////////////////////////////////////////
// Name:        wx/x11/window.h
// Purpose:     wxWindow class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WINDOW_H_
#define _WX_WINDOW_H_

#include "wx/region.h"

// ----------------------------------------------------------------------------
// wxWindow class for Motif - see also wxWindowBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowX11 : public wxWindowBase
{
    friend class WXDLLIMPEXP_FWD_CORE wxDC;
    friend class WXDLLIMPEXP_FWD_CORE wxWindowDC;

public:
    wxWindowX11() { Init(); }

    wxWindowX11(wxWindow *parent,
        wxWindowID id,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxString& name = wxPanelNameStr)
    {
        Init();
        Create(parent, id, pos, size, style, name);
    }

    virtual ~wxWindowX11();

    bool Create(wxWindow *parent,
        wxWindowID id,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxString& name = wxPanelNameStr);

    virtual void Raise();
    virtual void Lower();

    virtual void SetLabel(const wxString& label);
    virtual wxString GetLabel() const;

    virtual bool Show( bool show = true );
    virtual bool Enable( bool enable = true );

    virtual void SetFocus();

    virtual void WarpPointer(int x, int y);

    virtual void Refresh( bool eraseBackground = true,
                          const wxRect *rect = (const wxRect *) NULL );
    virtual void Update();

    virtual bool SetBackgroundColour( const wxColour &colour );
    virtual bool SetForegroundColour( const wxColour &colour );

    virtual bool SetCursor( const wxCursor &cursor );
    virtual bool SetFont( const wxFont &font );

    virtual int GetCharHeight() const;
    virtual int GetCharWidth() const;

    virtual void ScrollWindow( int dx, int dy, const wxRect* rect = NULL );

#if wxUSE_DRAG_AND_DROP
    virtual void SetDropTarget( wxDropTarget *dropTarget );
#endif // wxUSE_DRAG_AND_DROP

    // Accept files for dragging
    virtual void DragAcceptFiles(bool accept);

    // Get the unique identifier of a window
    virtual WXWindow GetHandle() const { return X11GetMainWindow(); }

    // implementation from now on
    // --------------------------

    // accessors
    // ---------

    // Get main X11 window
    virtual WXWindow X11GetMainWindow() const;

    // Get X11 window representing the client area
    virtual WXWindow GetClientAreaWindow() const;

    void SetLastClick(int button, long timestamp)
        { m_lastButton = button; m_lastTS = timestamp; }

    int GetLastClickedButton() const { return m_lastButton; }
    long GetLastClickTime() const { return m_lastTS; }

    // Gives window a chance to do something in response to a size message, e.g.
    // arrange status bar, toolbar etc.
    virtual bool PreResize();

    // Generates paint events from m_updateRegion
    void SendPaintEvents();

    // Generates paint events from flag
    void SendNcPaintEvents();

    // Generates erase events from m_clearRegion
    void SendEraseEvents();

    // Clip to paint region?
    bool GetClipPaintRegion() { return m_clipPaintRegion; }

    // Return clear region
    wxRegion &GetClearRegion() { return m_clearRegion; }

    void NeedUpdateNcAreaInIdle( bool update = true ) { m_updateNcArea = update; }

    // Inserting into main window instead of client
    // window. This is mostly for a wxWindow's own
    // scrollbars.
    void SetInsertIntoMain( bool insert = true ) { m_insertIntoMain = insert; }
    bool GetInsertIntoMain() { return m_insertIntoMain; }

    // sets the fore/background colour for the given widget
    static void DoChangeForegroundColour(WXWindow widget, wxColour& foregroundColour);
    static void DoChangeBackgroundColour(WXWindow widget, wxColour& backgroundColour, bool changeArmColour = false);

    // I don't want users to override what's done in idle so everything that
    // has to be done in idle time in order for wxX11 to work is done in
    // OnInternalIdle
    virtual void OnInternalIdle();

protected:
    // Responds to colour changes: passes event on to children.
    void OnSysColourChanged(wxSysColourChangedEvent& event);

    // For double-click detection
    long   m_lastTS;         // last timestamp
    int    m_lastButton;     // last pressed button

protected:
    WXWindow              m_mainWindow;
    WXWindow              m_clientWindow;
    bool                  m_insertIntoMain;

    bool                  m_winCaptured;
    wxRegion              m_clearRegion;
    bool                  m_clipPaintRegion;
    bool                  m_updateNcArea;
    bool                  m_needsInputFocus; // Input focus set in OnIdle

    // implement the base class pure virtuals
    virtual void DoGetTextExtent(const wxString& string,
                                 int *x, int *y,
                                 int *descent = NULL,
                                 int *externalLeading = NULL,
                                 const wxFont *font = NULL) const;
    virtual void DoClientToScreen( int *x, int *y ) const;
    virtual void DoScreenToClient( int *x, int *y ) const;
    virtual void DoGetPosition( int *x, int *y ) const;
    virtual void DoGetSize( int *width, int *height ) const;
    virtual void DoGetClientSize( int *width, int *height ) const;
    virtual void DoSetSize(int x, int y,
        int width, int height,
        int sizeFlags = wxSIZE_AUTO);
    virtual void DoSetClientSize(int width, int height);
    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual void DoSetSizeHints(int minW, int minH,
        int maxW, int maxH,
        int incW, int incH);
    virtual void DoCaptureMouse();
    virtual void DoReleaseMouse();

#if wxUSE_TOOLTIPS
    virtual void DoSetToolTip( wxToolTip *tip );
#endif // wxUSE_TOOLTIPS

private:
    // common part of all ctors
    void Init();

    DECLARE_DYNAMIC_CLASS(wxWindowX11)
    wxDECLARE_NO_COPY_CLASS(wxWindowX11);
    DECLARE_EVENT_TABLE()
};

// ----------------------------------------------------------------------------
// A little class to switch off `size optimization' while an instance of the
// object exists: this may be useful to temporarily disable the optimisation
// which consists to do nothing when the new size is equal to the old size -
// although quite useful usually to avoid flicker, sometimes it leads to
// undesired effects.
//
// Usage: create an instance of this class on the stack to disable the size
// optimisation, it will be reenabled as soon as the object goes out from scope.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxNoOptimize
{
public:
    wxNoOptimize() { ms_count++; }
    ~wxNoOptimize() { ms_count--; }

    static bool CanOptimize() { return ms_count == 0; }

protected:
    static int ms_count;
};

#endif // _WX_WINDOW_H_
