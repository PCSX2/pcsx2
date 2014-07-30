/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/window.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKWINDOWH__
#define __GTKWINDOWH__

typedef struct _GtkTooltips GtkTooltips;
#ifdef HAVE_XIM
typedef struct _GdkIC GdkIC;
typedef struct _GdkICAttr GdkICAttr;
#endif

//-----------------------------------------------------------------------------
// callback definition for inserting a window (internal)
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxWindowGTK;
typedef void (*wxInsertChildFunction)( wxWindowGTK*, wxWindowGTK* );

//-----------------------------------------------------------------------------
// wxWindowGTK
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowGTK : public wxWindowBase
{
public:
    // creating the window
    // -------------------
    wxWindowGTK();
    wxWindowGTK(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxPanelNameStr);
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxPanelNameStr);
    virtual ~wxWindowGTK();

    // implement base class (pure) virtual methods
    // -------------------------------------------

    virtual void SetLabel(const wxString& WXUNUSED(label)) { }
    virtual wxString GetLabel() const { return wxEmptyString; }

    virtual bool Destroy();

    virtual void Raise();
    virtual void Lower();

    virtual bool Show( bool show = true );
    virtual void DoEnable( bool enable );

    virtual void SetWindowStyleFlag( long style );

    virtual bool IsRetained() const;

    virtual void SetFocus();
    virtual bool AcceptsFocus() const;

    virtual bool Reparent( wxWindowBase *newParent );

    virtual void WarpPointer(int x, int y);

    virtual void Refresh( bool eraseBackground = true,
                          const wxRect *rect = (const wxRect *) NULL );
    virtual void Update();
    virtual void ClearBackground();

    virtual bool SetBackgroundColour( const wxColour &colour );
    virtual bool SetForegroundColour( const wxColour &colour );
    virtual bool SetCursor( const wxCursor &cursor );
    virtual bool SetFont( const wxFont &font );

    virtual bool SetBackgroundStyle(wxBackgroundStyle style) ;

    virtual int GetCharHeight() const;
    virtual int GetCharWidth() const;

    virtual void SetScrollbar( int orient, int pos, int thumbVisible,
                               int range, bool refresh = true );
    virtual void SetScrollPos( int orient, int pos, bool refresh = true );
    virtual int GetScrollPos( int orient ) const;
    virtual int GetScrollThumb( int orient ) const;
    virtual int GetScrollRange( int orient ) const;
    virtual void ScrollWindow( int dx, int dy,
                               const wxRect* rect = NULL );

#if wxUSE_DRAG_AND_DROP
    virtual void SetDropTarget( wxDropTarget *dropTarget );
#endif // wxUSE_DRAG_AND_DROP

    virtual bool IsDoubleBuffered() const { return false; }

    GdkWindow* GTKGetDrawingWindow() const;

    // implementation
    // --------------

    virtual WXWidget GetHandle() const { return m_widget; }

    // I don't want users to override what's done in idle so everything that
    // has to be done in idle time in order for wxGTK to work is done in
    // OnInternalIdle
    virtual void OnInternalIdle();

    // Internal representation of Update()
    void GtkUpdate();

    // For compatibility across platforms (not in event table)
    void OnIdle(wxIdleEvent& WXUNUSED(event)) {}

    // Used by all window classes in the widget creation process.
    bool PreCreation( wxWindowGTK *parent, const wxPoint &pos, const wxSize &size );
    void PostCreation();

    // Internal addition of child windows. differs from class
    // to class not by using virtual functions but by using
    // the m_insertCallback.
    void DoAddChild(wxWindowGTK *child);

    // This methods sends wxPaintEvents to the window. It reads the
    // update region, breaks it up into rects and sends an event
    // for each rect. It is also responsible for background erase
    // events and NC paint events. It is called from "draw" and
    // "expose" handlers as well as from ::Update()
    void GtkSendPaintEvents();

    // The methods below are required because many native widgets
    // are composed of several subwidgets and setting a style for
    // the widget means setting it for all subwidgets as well.
    // also, it is not clear which native widget is the top
    // widget where (most of) the input goes. even tooltips have
    // to be applied to all subwidgets.
    virtual GtkWidget* GetConnectWidget();
    virtual bool IsOwnGtkWindow( GdkWindow *window );
    void ConnectWidget( GtkWidget *widget );

#if wxUSE_TOOLTIPS
    virtual void ApplyToolTip( GtkTooltips *tips, const wxChar *tip );
#endif // wxUSE_TOOLTIPS

    // Call after modifing the value of m_hAdjust or m_vAdjust to bring the
    // scrolbar in sync (this does not generate any wx events)
    void GtkUpdateScrollbar(int orient);

    // Called from GTK signal handlers. it indicates that
    // the layout functions have to be called later on
    // (i.e. in idle time, implemented in OnInternalIdle() ).
    void GtkUpdateSize() { m_sizeSet = false; }

    // fix up the mouse event coords, used by wxListBox only so far
    virtual void FixUpMouseEvent(GtkWidget * WXUNUSED(widget),
                                 wxCoord& WXUNUSED(x),
                                 wxCoord& WXUNUSED(y)) { }

    // is this window transparent for the mouse events (as wxStaticBox is)?
    virtual bool IsTransparentForMouse() const { return false; }

    // is this a radiobutton (used by radiobutton code itself only)?
    virtual bool IsRadioButton() const { return false; }

    // position and size of the window
    int                  m_x, m_y;
    int                  m_width, m_height;
    int                  m_oldClientWidth,m_oldClientHeight;

    // see the docs in src/gtk/window.cpp
    GtkWidget           *m_widget;          // mostly the widget seen by the rest of GTK
    GtkWidget           *m_wxwindow;        // mostly the client area as per wxWidgets

    // this widget will be queried for GTK's focus events
    GtkWidget           *m_focusWidget;

#ifdef HAVE_XIM
    // XIM support for wxWidgets
    GdkIC               *m_ic;
    GdkICAttr           *m_icattr;
#endif // HAVE_XIM

    // The area to be cleared (and not just refreshed)
    // We cannot make this distinction under GTK 2.0.
    wxRegion             m_clearRegion;

    // scrolling stuff
    GtkAdjustment       *m_hAdjust,*m_vAdjust;
    float                m_oldHorizontalPos;
    float                m_oldVerticalPos;

    // extra (wxGTK-specific) flags
    bool                 m_needParent:1;        // ! wxFrame, wxDialog, wxNotebookPage ?
    bool                 m_noExpose:1;          // wxGLCanvas has its own redrawing
    bool                 m_nativeSizeEvent:1;   // wxGLCanvas sends wxSizeEvent upon "alloc_size"
    bool                 m_hasScrolling:1;
    bool                 m_hasVMT:1;
    bool                 m_sizeSet:1;
    bool                 m_resizing:1;
    bool                 m_acceptsFocus:1;      // true if not static
    bool                 m_hasFocus:1;          // true if == FindFocus()
    bool                 m_isScrolling:1;       // dragging scrollbar thumb?
    bool                 m_clipPaintRegion:1;   // true after ScrollWindow()
    bool                 m_needsStyleChange:1;  // May not be able to change
                                                // background style until OnIdle

    // C++ has no virtual methods in the constrcutor of any class but we need
    // different methods of inserting a child window into a wxFrame,
    // wxMDIFrame, wxNotebook etc. this is the callback that will get used.
    wxInsertChildFunction  m_insertCallback;

    // implement the base class pure virtuals
    virtual void DoGetTextExtent(const wxString& string,
                               int *x, int *y,
                               int *descent = NULL,
                               int *externalLeading = NULL,
                               const wxFont *theFont = NULL) const;

#if wxUSE_MENUS_NATIVE
    virtual bool DoPopupMenu( wxMenu *menu, int x, int y );
#endif // wxUSE_MENUS_NATIVE

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

    virtual void DoCaptureMouse();
    virtual void DoReleaseMouse();

#if wxUSE_TOOLTIPS
    virtual void DoSetToolTip( wxToolTip *tip );
#endif // wxUSE_TOOLTIPS

protected:
    // common part of all ctors (not virtual because called from ctor)
    void Init();

    // Called by ApplyWidgetStyle (which is called by SetFont() and
    // SetXXXColour etc to apply style changed to native widgets) to create
    // modified GTK style with non-standard attributes. If forceStyle=true,
    // creates empty GtkRcStyle if there are no modifications, otherwise
    // returns NULL in such case.
    GtkRcStyle *CreateWidgetStyle(bool forceStyle = false);

    // Overridden in many GTK widgets who have to handle subwidgets
    virtual void ApplyWidgetStyle(bool forceStyle = false);

    // helper function to ease native widgets wrapping, called by
    // ApplyWidgetStyle -- override this, not ApplyWidgetStyle
    virtual void DoApplyWidgetStyle(GtkRcStyle *style);

private:
    DECLARE_DYNAMIC_CLASS(wxWindowGTK)
    wxDECLARE_NO_COPY_CLASS(wxWindowGTK);
};

extern WXDLLIMPEXP_CORE wxWindow *wxFindFocusedChild(wxWindowGTK *win);

#endif // __GTKWINDOWH__
