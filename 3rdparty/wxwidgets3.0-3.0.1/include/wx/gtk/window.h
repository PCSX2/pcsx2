/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/window.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_WINDOW_H_
#define _WX_GTK_WINDOW_H_

#include "wx/dynarray.h"

#ifdef __WXGTK3__
    typedef struct _cairo cairo_t;
    typedef struct _GtkStyleProvider GtkStyleProvider;
    #define WXUNUSED_IN_GTK3(x)
#else
    #define WXUNUSED_IN_GTK3(x) x
#endif

typedef struct _GdkEventKey GdkEventKey;
typedef struct _GtkIMContext GtkIMContext;

WX_DEFINE_EXPORTED_ARRAY_PTR(GdkWindow *, wxArrayGdkWindows);

extern "C"
{

typedef void (*wxGTKCallback)();

}

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

    virtual void Raise();
    virtual void Lower();

    virtual bool Show( bool show = true );

    virtual bool IsRetained() const;

    virtual void SetFocus();

    // hint from wx to native GTK+ tab traversal code
    virtual void SetCanFocus(bool canFocus);

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
    virtual bool IsTransparentBackgroundSupported(wxString* reason = NULL) const;

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
    virtual bool ScrollLines(int lines);
    virtual bool ScrollPages(int pages);

#if wxUSE_DRAG_AND_DROP
    virtual void SetDropTarget( wxDropTarget *dropTarget );
#endif // wxUSE_DRAG_AND_DROP

    virtual void AddChild( wxWindowBase *child );
    virtual void RemoveChild( wxWindowBase *child );

    virtual void SetLayoutDirection(wxLayoutDirection dir);
    virtual wxLayoutDirection GetLayoutDirection() const;
    virtual wxCoord AdjustForLayoutDirection(wxCoord x,
                                             wxCoord width,
                                             wxCoord widthTotal) const;

    virtual bool DoIsExposed( int x, int y ) const;
    virtual bool DoIsExposed( int x, int y, int w, int h ) const;

    // currently wxGTK2-only
    void SetDoubleBuffered(bool on);
    virtual bool IsDoubleBuffered() const;

    // SetLabel(), which does nothing in wxWindow
    virtual void SetLabel(const wxString& label) { m_gtkLabel = label; }
    virtual wxString GetLabel() const            { return m_gtkLabel; }

    // implementation
    // --------------

    virtual WXWidget GetHandle() const { return m_widget; }

    // many important things are done here, this function must be called
    // regularly
    virtual void OnInternalIdle();

    // For compatibility across platforms (not in event table)
    void OnIdle(wxIdleEvent& WXUNUSED(event)) {}

    // Used by all window classes in the widget creation process.
    bool PreCreation( wxWindowGTK *parent, const wxPoint &pos, const wxSize &size );
    void PostCreation();

    // Internal addition of child windows
    void DoAddChild(wxWindowGTK *child);

    // This method sends wxPaintEvents to the window.
    // It is also responsible for background erase events.
#ifdef __WXGTK3__
    void GTKSendPaintEvents(cairo_t* cr);
#else
    void GTKSendPaintEvents(const GdkRegion* region);
#endif

    // The methods below are required because many native widgets
    // are composed of several subwidgets and setting a style for
    // the widget means setting it for all subwidgets as well.
    // also, it is not clear which native widget is the top
    // widget where (most of) the input goes. even tooltips have
    // to be applied to all subwidgets.
    virtual GtkWidget* GetConnectWidget();
    void ConnectWidget( GtkWidget *widget );

    // Called from several event handlers, if it returns true or false, the
    // same value should be immediately returned by the handler without doing
    // anything else. If it returns -1, the handler should continue as usual
    int GTKCallbackCommonPrologue(struct _GdkEventAny *event) const;

    // Simplified form of GTKCallbackCommonPrologue() which can be used from
    // GTK callbacks without return value to check if the event should be
    // ignored: if this returns true, the event shouldn't be handled
    bool GTKShouldIgnoreEvent() const;


    // override this if some events should never be consumed by wxWidgets but
    // but have to be left for the native control
    //
    // base version just calls HandleWindowEvent()
    virtual bool GTKProcessEvent(wxEvent& event) const;

    // Map GTK widget direction of the given widget to/from wxLayoutDirection
    static wxLayoutDirection GTKGetLayout(GtkWidget *widget);
    static void GTKSetLayout(GtkWidget *widget, wxLayoutDirection dir);

    // This is called when capture is taken from the window. It will
    // fire off capture lost events.
    void GTKReleaseMouseAndNotify();

    GdkWindow* GTKGetDrawingWindow() const;

    bool GTKHandleFocusIn();
    bool GTKHandleFocusOut();
    void GTKHandleFocusOutNoDeferring();
    static void GTKHandleDeferredFocusOut();

    // Called when m_widget becomes realized. Derived classes must call the
    // base class method if they override it.
    virtual void GTKHandleRealized();
    void GTKHandleUnrealize();

protected:
    // for controls composed of multiple GTK widgets, return true to eliminate
    // spurious focus events if the focus changes between GTK+ children within
    // the same wxWindow
    virtual bool GTKNeedsToFilterSameWindowFocus() const { return false; }

    // Override GTKWidgetNeedsMnemonic and return true if your
    // needs to set its mnemonic widget, such as for a
    // GtkLabel for wxStaticText, then do the actual
    // setting of the widget inside GTKWidgetDoSetMnemonic
    virtual bool GTKWidgetNeedsMnemonic() const;
    virtual void GTKWidgetDoSetMnemonic(GtkWidget* w);

    // Get the GdkWindows making part of this window: usually there will be
    // only one of them in which case it should be returned directly by this
    // function. If there is more than one GdkWindow (can be the case for
    // composite widgets), return NULL and fill in the provided array
    //
    // This is not pure virtual for backwards compatibility but almost
    // certainly must be overridden in any wxControl-derived class!
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    // Check if the given window makes part of this widget
    bool GTKIsOwnWindow(GdkWindow *window) const;

public:
    // Returns the default context which usually is anti-aliased
    PangoContext   *GTKGetPangoDefaultContext();

#if wxUSE_TOOLTIPS
    // applies tooltip to the widget (tip must be UTF-8 encoded)
    virtual void GTKApplyToolTip(const char* tip);
#endif // wxUSE_TOOLTIPS

    // Called when a window should delay showing itself
    // until idle time used in Reparent().
    void GTKShowOnIdle() { m_showOnIdle = true; }

    // This is called from the various OnInternalIdle methods
    bool GTKShowFromOnIdle();

    // is this window transparent for the mouse events (as wxStaticBox is)?
    virtual bool GTKIsTransparentForMouse() const { return false; }

    // Common scroll event handling code for wxWindow and wxScrollBar
    wxEventType GTKGetScrollEventType(GtkRange* range);

    // position and size of the window
    int                  m_x, m_y;
    int                  m_width, m_height;
    int m_clientWidth, m_clientHeight;
    // Whether the client size variables above are known to be correct
    // (because they have been validated by a size-allocate) and should
    // be used to report client size
    bool m_useCachedClientSize;

    // see the docs in src/gtk/window.cpp
    GtkWidget           *m_widget;          // mostly the widget seen by the rest of GTK
    GtkWidget           *m_wxwindow;        // mostly the client area as per wxWidgets

    // label for use with GetLabelSetLabel
    wxString             m_gtkLabel;

    // return true if the window is of a standard (i.e. not wxWidgets') class
    bool IsOfStandardClass() const { return m_wxwindow == NULL; }

    // this widget will be queried for GTK's focus events
    GtkWidget           *m_focusWidget;

    void GTKDisableFocusOutEvent();
    void GTKEnableFocusOutEvent();


    // Input method support

    // The IM context used for generic, i.e. non-native, windows.
    //
    // It might be a good idea to avoid allocating it unless key events from
    // this window are really needed but currently we do it unconditionally.
    //
    // For native widgets (i.e. those for which IsOfStandardClass() returns
    // true) it is NULL.
    GtkIMContext* m_imContext;

    // Pointer to the event being currently processed by the IME or NULL if not
    // inside key handling.
    GdkEventKey* m_imKeyEvent;

    // This method generalizes gtk_im_context_filter_keypress(): for the
    // generic windows it does just that but it's overridden by the classes
    // wrapping native widgets that use IM themselves and so provide specific
    // methods for accessing it such gtk_entry_im_context_filter_keypress().
    virtual int GTKIMFilterKeypress(GdkEventKey* event) const;

    // This method must be called from the derived classes "insert-text" signal
    // handlers to check if the text is not being inserted by the IM and, if
    // this is the case, generate appropriate wxEVT_CHAR events for it.
    //
    // Returns true if we did generate and process events corresponding to this
    // text or false if we didn't handle it.
    bool GTKOnInsertText(const char* text);

    // This is just a helper of GTKOnInsertText() which is also used by GTK+
    // "commit" signal handler.
    bool GTKDoInsertTextFromIM(const char* text);


    // indices for the arrays below
    enum ScrollDir { ScrollDir_Horz, ScrollDir_Vert, ScrollDir_Max };

    // horizontal/vertical scroll bar
    GtkRange* m_scrollBar[ScrollDir_Max];

    // horizontal/vertical scroll position
    double m_scrollPos[ScrollDir_Max];

    // return the scroll direction index corresponding to the given orientation
    // (which is wxVERTICAL or wxHORIZONTAL)
    static ScrollDir ScrollDirFromOrient(int orient)
    {
        return orient == wxVERTICAL ? ScrollDir_Vert : ScrollDir_Horz;
    }

    // return the orientation for the given scrolling direction
    static int OrientFromScrollDir(ScrollDir dir)
    {
        return dir == ScrollDir_Horz ? wxHORIZONTAL : wxVERTICAL;
    }

    // find the direction of the given scrollbar (must be one of ours)
    ScrollDir ScrollDirFromRange(GtkRange *range) const;

    void GTKUpdateCursor(bool isBusyOrGlobalCursor = false, bool isRealize = false);

    // extra (wxGTK-specific) flags
    bool                 m_noExpose:1;          // wxGLCanvas has its own redrawing
    bool                 m_nativeSizeEvent:1;   // wxGLCanvas sends wxSizeEvent upon "alloc_size"
    bool                 m_isScrolling:1;       // dragging scrollbar thumb?
    bool                 m_clipPaintRegion:1;   // true after ScrollWindow()
    wxRegion             m_nativeUpdateRegion;  // not transformed for RTL
    bool                 m_dirtyTabOrder:1;     // tab order changed, GTK focus
                                                // chain needs update
    bool                 m_mouseButtonDown:1;

    bool                 m_showOnIdle:1;        // postpone showing the window until idle

protected:
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
    virtual wxSize DoGetBorderSize() const;
    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual void DoEnable(bool enable);

#if wxUSE_MENUS_NATIVE
    virtual bool DoPopupMenu( wxMenu *menu, int x, int y );
#endif // wxUSE_MENUS_NATIVE

    virtual void DoCaptureMouse();
    virtual void DoReleaseMouse();

    virtual void DoFreeze();
    virtual void DoThaw();

    void GTKFreezeWidget(GtkWidget *w);
    void GTKThawWidget(GtkWidget *w);
    void GTKDisconnect(void* instance);

#if wxUSE_TOOLTIPS
    virtual void DoSetToolTip( wxToolTip *tip );
#endif // wxUSE_TOOLTIPS

    // Create a GtkScrolledWindow containing the given widget (usually
    // m_wxwindow but not necessarily) and assigns it to m_widget. Also shows
    // the widget passed to it.
    //
    // Can be only called if we have either wxHSCROLL or wxVSCROLL in our
    // style.
    void GTKCreateScrolledWindowWith(GtkWidget* view);

    virtual void DoMoveInTabOrder(wxWindow *win, WindowOrder move);
    virtual bool DoNavigateIn(int flags);


    // Copies m_children tab order to GTK focus chain:
    void RealizeTabOrder();

#ifndef __WXGTK3__
    // Called by ApplyWidgetStyle (which is called by SetFont() and
    // SetXXXColour etc to apply style changed to native widgets) to create
    // modified GTK style with non-standard attributes.
    GtkRcStyle* GTKCreateWidgetStyle();
#endif

    void GTKApplyWidgetStyle(bool forceStyle = false);

    // helper function to ease native widgets wrapping, called by
    // ApplyWidgetStyle -- override this, not ApplyWidgetStyle
    virtual void DoApplyWidgetStyle(GtkRcStyle *style);

    void GTKApplyStyle(GtkWidget* widget, GtkRcStyle* style);

    // sets the border of a given GtkScrolledWindow from a wx style
    static void GTKScrolledWindowSetBorder(GtkWidget* w, int style);

    // Connect the given function to the specified signal on m_widget.
    //
    // This is just a wrapper for g_signal_connect() and returns the handler id
    // just as it does.
    unsigned long GTKConnectWidget(const char *signal, wxGTKCallback callback);

    void ConstrainSize();

private:
    void Init();

    // return true if this window must have a non-NULL parent, false if it can
    // be created without parent (normally only top level windows but in wxGTK
    // there is also the exception of wxMenuBar)
    virtual bool GTKNeedsParent() const { return !IsTopLevel(); }

    enum ScrollUnit { ScrollUnit_Line, ScrollUnit_Page, ScrollUnit_Max };

    // common part of ScrollLines() and ScrollPages() and could be used, in the
    // future, for horizontal scrolling as well
    //
    // return true if we scrolled, false otherwise (on error or simply if we
    // are already at the end)
    bool DoScrollByUnits(ScrollDir dir, ScrollUnit unit, int units);
    virtual void AddChildGTK(wxWindowGTK* child);

#ifdef __WXGTK3__
    // paint context is stashed here so wxPaintDC can use it
    cairo_t* m_paintContext;
    // style provider for "background-image"
    GtkStyleProvider* m_styleProvider;

public:
    cairo_t* GTKPaintContext() const
    {
        return m_paintContext;
    }
#endif

    DECLARE_DYNAMIC_CLASS(wxWindowGTK)
    wxDECLARE_NO_COPY_CLASS(wxWindowGTK);
};

#endif // _WX_GTK_WINDOW_H_
