/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/window.h
// Purpose:     wxWindow class
// Author:      Vaclav Slavik
// Created:     2006-08-10
// Copyright:   (c) 2006 REA Elektronik GmbH
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_WINDOW_H_
#define _WX_DFB_WINDOW_H_

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#include "wx/dfb/dfbptr.h"

wxDFB_DECLARE_INTERFACE(IDirectFBSurface);
struct wxDFBWindowEvent;

class WXDLLIMPEXP_FWD_CORE wxFont;
class WXDLLIMPEXP_FWD_CORE wxNonOwnedWindow;

class wxOverlayImpl;
class wxDfbOverlaysList;

// ---------------------------------------------------------------------------
// wxWindow
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowDFB : public wxWindowBase
{
public:
    wxWindowDFB() { Init(); }

    wxWindowDFB(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxPanelNameStr)
    {
        Init();
        Create(parent, id, pos, size, style, name);
    }

    virtual ~wxWindowDFB();

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxPanelNameStr);

    // implement base class (pure) virtual methods
    // -------------------------------------------

    virtual void SetLabel( const wxString &WXUNUSED(label) ) {}
    virtual wxString GetLabel() const { return wxEmptyString; }

    virtual void Raise();
    virtual void Lower();

    virtual bool Show(bool show = true);

    virtual void SetFocus();

    virtual bool Reparent(wxWindowBase *newParent);

    virtual void WarpPointer(int x, int y);

    virtual void Refresh(bool eraseBackground = true,
                         const wxRect *rect = (const wxRect *) NULL);
    virtual void Update();

    virtual bool SetCursor(const wxCursor &cursor);
    virtual bool SetFont(const wxFont &font) { m_font = font; return true; }

    virtual int GetCharHeight() const;
    virtual int GetCharWidth() const;

#if wxUSE_DRAG_AND_DROP
    virtual void SetDropTarget(wxDropTarget *dropTarget);

    // Accept files for dragging
    virtual void DragAcceptFiles(bool accept);
#endif // wxUSE_DRAG_AND_DROP

    virtual WXWidget GetHandle() const { return this; }

    // implementation from now on
    // --------------------------

    // Returns DirectFB surface used for rendering of this window
    wxIDirectFBSurfacePtr GetDfbSurface();

    // returns toplevel window the window belongs to
    wxNonOwnedWindow *GetTLW() const { return m_tlw; }

    virtual bool IsDoubleBuffered() const { return true; }

protected:
    // implement the base class pure virtuals
    virtual void DoGetTextExtent(const wxString& string,
                                 int *x, int *y,
                                 int *descent = NULL,
                                 int *externalLeading = NULL,
                                 const wxFont *theFont = NULL) const;
    virtual void DoClientToScreen(int *x, int *y) const;
    virtual void DoScreenToClient(int *x, int *y) const;
    virtual void DoGetPosition(int *x, int *y) const;
    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoGetClientSize(int *width, int *height) const;
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);
    virtual void DoSetClientSize(int width, int height);

    virtual void DoCaptureMouse();
    virtual void DoReleaseMouse();

    virtual void DoThaw();

    // move the window to the specified location and resize it: this is called
    // from both DoSetSize() and DoSetClientSize() and would usually just call
    // ::MoveWindow() except for composite controls which will want to arrange
    // themselves inside the given rectangle
    virtual void DoMoveWindow(int x, int y, int width, int height);

    // return DFB surface used to render this window (will be assigned to
    // m_surface if the window is visible)
    virtual wxIDirectFBSurfacePtr ObtainDfbSurface() const;

    // this method must be called when window's position, size or visibility
    // changes; it resets m_surface so that ObtainDfbSurface has to be called
    // next time GetDfbSurface is called
    void InvalidateDfbSurface();

    // called by parent to render (part of) the window
    void PaintWindow(const wxRect& rect);

    // paint window's overlays (if any) on top of window's surface
    void PaintOverlays(const wxRect& rect);

    // refreshes the entire window (including non-client areas)
    void DoRefreshWindow();
    // refreshes given rectangle of the window (in window, _not_ client coords)
    virtual void DoRefreshRect(const wxRect& rect);
    // refreshes given rectangle; unlike RefreshRect(), the argument is in
    // window, not client, coords and unlike DoRefreshRect() and like Refresh(),
    // does nothing if the window is hidden or frozen
    void RefreshWindowRect(const wxRect& rect);

    // add/remove overlay for this window
    void AddOverlay(wxOverlayImpl *overlay);
    void RemoveOverlay(wxOverlayImpl *overlay);

    // DirectFB events handling
    void HandleKeyEvent(const wxDFBWindowEvent& event_);

private:
    // common part of all ctors
    void Init();
    // counterpart to SetFocus
    void DFBKillFocus();

protected:
    // toplevel window (i.e. DirectFB window) this window belongs to
    wxNonOwnedWindow *m_tlw;

private:
    // subsurface of TLW's surface covered by this window
    wxIDirectFBSurfacePtr m_surface;

    // position of the window (relative to the parent, not used by wxTLW, so
    // don't access it directly)
    wxRect m_rect;

    // overlays for this window (or NULL if it doesn't have any)
    wxDfbOverlaysList *m_overlays;

    friend class wxNonOwnedWindow; // for HandleXXXEvent
    friend class wxOverlayImpl; // for Add/RemoveOverlay
    friend class wxWindowDCImpl; // for PaintOverlays

    DECLARE_DYNAMIC_CLASS(wxWindowDFB)
    wxDECLARE_NO_COPY_CLASS(wxWindowDFB);
    DECLARE_EVENT_TABLE()
};

#endif // _WX_DFB_WINDOW_H_
