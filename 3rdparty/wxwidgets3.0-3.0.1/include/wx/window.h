///////////////////////////////////////////////////////////////////////////////
// Name:        wx/window.h
// Purpose:     wxWindowBase class - the interface of wxWindow
// Author:      Vadim Zeitlin
// Modified by: Ron Lee
// Created:     01/02/97
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WINDOW_H_BASE_
#define _WX_WINDOW_H_BASE_

// ----------------------------------------------------------------------------
// headers which we must include here
// ----------------------------------------------------------------------------

#include "wx/event.h"           // the base class

#include "wx/list.h"            // defines wxWindowList

#include "wx/cursor.h"          // we have member variables of these classes
#include "wx/font.h"            // so we can't do without them
#include "wx/colour.h"
#include "wx/region.h"
#include "wx/utils.h"
#include "wx/intl.h"

#include "wx/validate.h"        // for wxDefaultValidator (always include it)

#if wxUSE_PALETTE
    #include "wx/palette.h"
#endif // wxUSE_PALETTE

#if wxUSE_ACCEL
    #include "wx/accel.h"
#endif // wxUSE_ACCEL

#if wxUSE_ACCESSIBILITY
#include "wx/access.h"
#endif

// when building wxUniv/Foo we don't want the code for native menu use to be
// compiled in - it should only be used when building real wxFoo
#ifdef __WXUNIVERSAL__
    #define wxUSE_MENUS_NATIVE 0
#else // !__WXUNIVERSAL__
    #define wxUSE_MENUS_NATIVE wxUSE_MENUS
#endif // __WXUNIVERSAL__/!__WXUNIVERSAL__


// Define this macro if the corresponding operating system handles the state
// of children windows automatically when the parent is enabled/disabled.
// Otherwise wx itself must ensure that when the parent is disabled its
// children are disabled too, and their initial state is restored when the
// parent is enabled back.
#if defined(__WXMSW__) || defined(__WXPM__)
    // must do everything ourselves
    #undef wxHAS_NATIVE_ENABLED_MANAGEMENT
#elif defined(__WXOSX__)
    #if wxOSX_USE_CARBON
        #define wxHAS_NATIVE_ENABLED_MANAGEMENT
    #else
        // must do everything ourselves
        #undef wxHAS_NATIVE_ENABLED_MANAGEMENT
    #endif
#else
    #define wxHAS_NATIVE_ENABLED_MANAGEMENT
#endif

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxCaret;
class WXDLLIMPEXP_FWD_CORE wxControl;
class WXDLLIMPEXP_FWD_CORE wxCursor;
class WXDLLIMPEXP_FWD_CORE wxDC;
class WXDLLIMPEXP_FWD_CORE wxDropTarget;
class WXDLLIMPEXP_FWD_CORE wxLayoutConstraints;
class WXDLLIMPEXP_FWD_CORE wxSizer;
class WXDLLIMPEXP_FWD_CORE wxToolTip;
class WXDLLIMPEXP_FWD_CORE wxWindowBase;
class WXDLLIMPEXP_FWD_CORE wxWindow;
class WXDLLIMPEXP_FWD_CORE wxScrollHelper;

#if wxUSE_ACCESSIBILITY
class WXDLLIMPEXP_FWD_CORE wxAccessible;
#endif

// ----------------------------------------------------------------------------
// helper stuff used by wxWindow
// ----------------------------------------------------------------------------

// struct containing all the visual attributes of a control
struct WXDLLIMPEXP_CORE wxVisualAttributes
{
    // the font used for control label/text inside it
    wxFont font;

    // the foreground colour
    wxColour colFg;

    // the background colour, may be wxNullColour if the controls background
    // colour is not solid
    wxColour colBg;
};

// different window variants, on platforms like eg mac uses different
// rendering sizes
enum wxWindowVariant
{
    wxWINDOW_VARIANT_NORMAL,  // Normal size
    wxWINDOW_VARIANT_SMALL,   // Smaller size (about 25 % smaller than normal)
    wxWINDOW_VARIANT_MINI,    // Mini size (about 33 % smaller than normal)
    wxWINDOW_VARIANT_LARGE,   // Large size (about 25 % larger than normal)
    wxWINDOW_VARIANT_MAX
};

#if wxUSE_SYSTEM_OPTIONS
    #define wxWINDOW_DEFAULT_VARIANT wxT("window-default-variant")
#endif

// valid values for Show/HideWithEffect()
enum wxShowEffect
{
    wxSHOW_EFFECT_NONE,
    wxSHOW_EFFECT_ROLL_TO_LEFT,
    wxSHOW_EFFECT_ROLL_TO_RIGHT,
    wxSHOW_EFFECT_ROLL_TO_TOP,
    wxSHOW_EFFECT_ROLL_TO_BOTTOM,
    wxSHOW_EFFECT_SLIDE_TO_LEFT,
    wxSHOW_EFFECT_SLIDE_TO_RIGHT,
    wxSHOW_EFFECT_SLIDE_TO_TOP,
    wxSHOW_EFFECT_SLIDE_TO_BOTTOM,
    wxSHOW_EFFECT_BLEND,
    wxSHOW_EFFECT_EXPAND,
    wxSHOW_EFFECT_MAX
};

// flags for SendSizeEvent()
enum
{
    wxSEND_EVENT_POST = 1
};

// ----------------------------------------------------------------------------
// (pseudo)template list classes
// ----------------------------------------------------------------------------

WX_DECLARE_LIST_3(wxWindow, wxWindowBase, wxWindowList, wxWindowListNode, class WXDLLIMPEXP_CORE);

// ----------------------------------------------------------------------------
// global variables
// ----------------------------------------------------------------------------

extern WXDLLIMPEXP_DATA_CORE(wxWindowList) wxTopLevelWindows;

// declared here for compatibility only, main declaration is in wx/app.h
extern WXDLLIMPEXP_DATA_BASE(wxList) wxPendingDelete;

// ----------------------------------------------------------------------------
// wxWindowBase is the base class for all GUI controls/widgets, this is the public
// interface of this class.
//
// Event handler: windows have themselves as their event handlers by default,
// but their event handlers could be set to another object entirely. This
// separation can reduce the amount of derivation required, and allow
// alteration of a window's functionality (e.g. by a resource editor that
// temporarily switches event handlers).
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowBase : public wxEvtHandler
{
public:
    // creating the window
    // -------------------

        // default ctor, initializes everything which can be initialized before
        // Create()
    wxWindowBase() ;

    virtual ~wxWindowBase();

    // deleting the window
    // -------------------

        // ask the window to close itself, return true if the event handler
        // honoured our request
    bool Close( bool force = false );

        // the following functions delete the C++ objects (the window itself
        // or its children) as well as the GUI windows and normally should
        // never be used directly

        // delete window unconditionally (dangerous!), returns true if ok
    virtual bool Destroy();
        // delete all children of this window, returns true if ok
    bool DestroyChildren();

        // is the window being deleted?
    bool IsBeingDeleted() const;

    // window attributes
    // -----------------

        // label is just the same as the title (but for, e.g., buttons it
        // makes more sense to speak about labels), title access
        // is available from wxTLW classes only (frames, dialogs)
    virtual void SetLabel(const wxString& label) = 0;
    virtual wxString GetLabel() const = 0;

        // the window name is used for ressource setting in X, it is not the
        // same as the window title/label
    virtual void SetName( const wxString &name ) { m_windowName = name; }
    virtual wxString GetName() const { return m_windowName; }

        // sets the window variant, calls internally DoSetVariant if variant
        // has changed
    void SetWindowVariant(wxWindowVariant variant);
    wxWindowVariant GetWindowVariant() const { return m_windowVariant; }


        // get or change the layout direction (LTR or RTL) for this window,
        // wxLayout_Default is returned if layout direction is not supported
    virtual wxLayoutDirection GetLayoutDirection() const
        { return wxLayout_Default; }
    virtual void SetLayoutDirection(wxLayoutDirection WXUNUSED(dir))
        { }

        // mirror coordinates for RTL layout if this window uses it and if the
        // mirroring is not done automatically like Win32
    virtual wxCoord AdjustForLayoutDirection(wxCoord x,
                                             wxCoord width,
                                             wxCoord widthTotal) const;


        // window id uniquely identifies the window among its siblings unless
        // it is wxID_ANY which means "don't care"
    void SetId( wxWindowID winid ) { m_windowId = winid; }
    wxWindowID GetId() const { return m_windowId; }

        // generate a unique id (or count of them consecutively), returns a
        // valid id in the auto-id range or wxID_NONE if failed.  If using
        // autoid management, it will mark the id as reserved until it is
        // used (by assigning it to a wxWindowIDRef) or unreserved.
    static wxWindowID NewControlId(int count = 1)
    {
        return wxIdManager::ReserveId(count);
    }

        // If an ID generated from NewControlId is not assigned to a wxWindowIDRef,
        // it must be unreserved
    static void UnreserveControlId(wxWindowID id, int count = 1)
    {
        wxIdManager::UnreserveId(id, count);
    }


    // moving/resizing
    // ---------------

        // set the window size and/or position
    void SetSize( int x, int y, int width, int height,
                  int sizeFlags = wxSIZE_AUTO )
        {  DoSetSize(x, y, width, height, sizeFlags); }

    void SetSize( int width, int height )
        { DoSetSize( wxDefaultCoord, wxDefaultCoord, width, height, wxSIZE_USE_EXISTING ); }

    void SetSize( const wxSize& size )
        { SetSize( size.x, size.y); }

    void SetSize(const wxRect& rect, int sizeFlags = wxSIZE_AUTO)
        { DoSetSize(rect.x, rect.y, rect.width, rect.height, sizeFlags); }

    void Move(int x, int y, int flags = wxSIZE_USE_EXISTING)
        { DoSetSize(x, y, wxDefaultCoord, wxDefaultCoord, flags); }

    void Move(const wxPoint& pt, int flags = wxSIZE_USE_EXISTING)
        { Move(pt.x, pt.y, flags); }

    void SetPosition(const wxPoint& pt) { Move(pt); }

        // Z-order
    virtual void Raise() = 0;
    virtual void Lower() = 0;

        // client size is the size of area available for subwindows
    void SetClientSize( int width, int height )
        { DoSetClientSize(width, height); }

    void SetClientSize( const wxSize& size )
        { DoSetClientSize(size.x, size.y); }

    void SetClientSize(const wxRect& rect)
        { SetClientSize( rect.width, rect.height ); }

        // get the window position (pointers may be NULL): notice that it is in
        // client coordinates for child windows and screen coordinates for the
        // top level ones, use GetScreenPosition() if you need screen
        // coordinates for all kinds of windows
    void GetPosition( int *x, int *y ) const { DoGetPosition(x, y); }
    wxPoint GetPosition() const
    {
        int x, y;
        DoGetPosition(&x, &y);

        return wxPoint(x, y);
    }

        // get the window position in screen coordinates
    void GetScreenPosition(int *x, int *y) const { DoGetScreenPosition(x, y); }
    wxPoint GetScreenPosition() const
    {
        int x, y;
        DoGetScreenPosition(&x, &y);

        return wxPoint(x, y);
    }

        // get the window size (pointers may be NULL)
    void GetSize( int *w, int *h ) const { DoGetSize(w, h); }
    wxSize GetSize() const
    {
        int w, h;
        DoGetSize(& w, & h);
        return wxSize(w, h);
    }

    void GetClientSize( int *w, int *h ) const { DoGetClientSize(w, h); }
    wxSize GetClientSize() const
    {
        int w, h;
        DoGetClientSize(&w, &h);

        return wxSize(w, h);
    }

        // get the position and size at once
    wxRect GetRect() const
    {
        int x, y, w, h;
        GetPosition(&x, &y);
        GetSize(&w, &h);

        return wxRect(x, y, w, h);
    }

    wxRect GetScreenRect() const
    {
        int x, y, w, h;
        GetScreenPosition(&x, &y);
        GetSize(&w, &h);

        return wxRect(x, y, w, h);
    }

        // get the origin of the client area of the window relative to the
        // window top left corner (the client area may be shifted because of
        // the borders, scrollbars, other decorations...)
    virtual wxPoint GetClientAreaOrigin() const;

        // get the client rectangle in window (i.e. client) coordinates
    wxRect GetClientRect() const
    {
        return wxRect(GetClientAreaOrigin(), GetClientSize());
    }

    // client<->window size conversion
    virtual wxSize ClientToWindowSize(const wxSize& size) const;
    virtual wxSize WindowToClientSize(const wxSize& size) const;

        // get the size best suited for the window (in fact, minimal
        // acceptable size using which it will still look "nice" in
        // most situations)
    wxSize GetBestSize() const;

    void GetBestSize(int *w, int *h) const
    {
        wxSize s = GetBestSize();
        if ( w )
            *w = s.x;
        if ( h )
            *h = s.y;
    }

        // Determine the best size in the other direction if one of them is
        // fixed. This is used with windows that can wrap their contents and
        // returns input-independent best size for the others.
    int GetBestHeight(int width) const;
    int GetBestWidth(int height) const;


    void SetScrollHelper( wxScrollHelper *sh )   { m_scrollHelper = sh; }
    wxScrollHelper *GetScrollHelper()            { return m_scrollHelper; }

        // reset the cached best size value so it will be recalculated the
        // next time it is needed.
    void InvalidateBestSize();
    void CacheBestSize(const wxSize& size) const
        { wxConstCast(this, wxWindowBase)->m_bestSizeCache = size; }


        // This function will merge the window's best size into the window's
        // minimum size, giving priority to the min size components, and
        // returns the results.
    virtual wxSize GetEffectiveMinSize() const;

    wxDEPRECATED_MSG("use GetEffectiveMinSize() instead")
    wxSize GetBestFittingSize() const;
    wxDEPRECATED_MSG("use GetEffectiveMinSize() instead")
    wxSize GetAdjustedMinSize() const;

        // A 'Smart' SetSize that will fill in default size values with 'best'
        // size.  Sets the minsize to what was passed in.
    void SetInitialSize(const wxSize& size=wxDefaultSize);

    wxDEPRECATED_MSG("use SetInitialSize() instead")
    void SetBestFittingSize(const wxSize& size=wxDefaultSize);


        // the generic centre function - centers the window on parent by`
        // default or on screen if it doesn't have parent or
        // wxCENTER_ON_SCREEN flag is given
    void Centre(int dir = wxBOTH) { DoCentre(dir); }
    void Center(int dir = wxBOTH) { DoCentre(dir); }

        // centre with respect to the parent window
    void CentreOnParent(int dir = wxBOTH) { DoCentre(dir); }
    void CenterOnParent(int dir = wxBOTH) { CentreOnParent(dir); }

        // set window size to wrap around its children
    virtual void Fit();

        // set virtual size to satisfy children
    virtual void FitInside();


        // SetSizeHints is actually for setting the size hints
        // for the wxTLW for a Window Manager - hence the name -
        // and it is therefore overridden in wxTLW to do that.
        // In wxWindow(Base), it has (unfortunately) been abused
        // to mean the same as SetMinSize() and SetMaxSize().

    virtual void SetSizeHints( int minW, int minH,
                               int maxW = wxDefaultCoord, int maxH = wxDefaultCoord,
                               int incW = wxDefaultCoord, int incH = wxDefaultCoord )
    { DoSetSizeHints(minW, minH, maxW, maxH, incW, incH); }

    void SetSizeHints( const wxSize& minSize,
                       const wxSize& maxSize=wxDefaultSize,
                       const wxSize& incSize=wxDefaultSize)
    { DoSetSizeHints(minSize.x, minSize.y, maxSize.x, maxSize.y, incSize.x, incSize.y); }


#if WXWIN_COMPATIBILITY_2_8
    // these are useless and do nothing since wxWidgets 2.9
    wxDEPRECATED( virtual void SetVirtualSizeHints( int minW, int minH,
                                      int maxW = wxDefaultCoord, int maxH = wxDefaultCoord ) );
    wxDEPRECATED( void SetVirtualSizeHints( const wxSize& minSize,
                                            const wxSize& maxSize=wxDefaultSize) );
#endif // WXWIN_COMPATIBILITY_2_8


        // Call these to override what GetBestSize() returns. This
        // method is only virtual because it is overridden in wxTLW
        // as a different API for SetSizeHints().
    virtual void SetMinSize(const wxSize& minSize);
    virtual void SetMaxSize(const wxSize& maxSize);

        // Like Set*Size, but for client, not window, size
    virtual void SetMinClientSize(const wxSize& size)
        { SetMinSize(ClientToWindowSize(size)); }
    virtual void SetMaxClientSize(const wxSize& size)
        { SetMaxSize(ClientToWindowSize(size)); }

        // Override these methods to impose restrictions on min/max size.
        // The easier way is to call SetMinSize() and SetMaxSize() which
        // will have the same effect. Doing both is non-sense.
    virtual wxSize GetMinSize() const { return wxSize(m_minWidth, m_minHeight); }
    virtual wxSize GetMaxSize() const { return wxSize(m_maxWidth, m_maxHeight); }

        // Like Get*Size, but for client, not window, size
    virtual wxSize GetMinClientSize() const
        { return WindowToClientSize(GetMinSize()); }
    virtual wxSize GetMaxClientSize() const
        { return WindowToClientSize(GetMaxSize()); }

        // Get the min and max values one by one
    int GetMinWidth() const { return GetMinSize().x; }
    int GetMinHeight() const { return GetMinSize().y; }
    int GetMaxWidth() const { return GetMaxSize().x; }
    int GetMaxHeight() const { return GetMaxSize().y; }


        // Methods for accessing the virtual size of a window.  For most
        // windows this is just the client area of the window, but for
        // some like scrolled windows it is more or less independent of
        // the screen window size.  You may override the DoXXXVirtual
        // methods below for classes where that is the case.

    void SetVirtualSize( const wxSize &size ) { DoSetVirtualSize( size.x, size.y ); }
    void SetVirtualSize( int x, int y ) { DoSetVirtualSize( x, y ); }

    wxSize GetVirtualSize() const { return DoGetVirtualSize(); }
    void GetVirtualSize( int *x, int *y ) const
    {
        wxSize s( DoGetVirtualSize() );

        if( x )
            *x = s.GetWidth();
        if( y )
            *y = s.GetHeight();
    }

        // Override these methods for windows that have a virtual size
        // independent of their client size.  eg. the virtual area of a
        // wxScrolledWindow.

    virtual void DoSetVirtualSize( int x, int y );
    virtual wxSize DoGetVirtualSize() const;

        // Return the largest of ClientSize and BestSize (as determined
        // by a sizer, interior children, or other means)

    virtual wxSize GetBestVirtualSize() const
    {
        wxSize  client( GetClientSize() );
        wxSize  best( GetBestSize() );

        return wxSize( wxMax( client.x, best.x ), wxMax( client.y, best.y ) );
    }

    // returns the magnification of the content of this window
    // eg 2.0 for a window on a retina screen
    virtual double GetContentScaleFactor() const
    { return 1.0; }
    
    // return the size of the left/right and top/bottom borders in x and y
    // components of the result respectively
    virtual wxSize GetWindowBorderSize() const;

    // wxSizer and friends use this to give a chance to a component to recalc
    // its min size once one of the final size components is known. Override
    // this function when that is useful (such as for wxStaticText which can
    // stretch over several lines). Parameter availableOtherDir
    // tells the item how much more space there is available in the opposite
    // direction (-1 if unknown).
    virtual bool
    InformFirstDirection(int direction, int size, int availableOtherDir);

    // sends a size event to the window using its current size -- this has an
    // effect of refreshing the window layout
    //
    // by default the event is sent, i.e. processed immediately, but if flags
    // value includes wxSEND_EVENT_POST then it's posted, i.e. only schedule
    // for later processing
    virtual void SendSizeEvent(int flags = 0);

    // this is a safe wrapper for GetParent()->SendSizeEvent(): it checks that
    // we have a parent window and it's not in process of being deleted
    //
    // this is used by controls such as tool/status bars changes to which must
    // also result in parent re-layout
    void SendSizeEventToParent(int flags = 0);

    // this is a more readable synonym for SendSizeEvent(wxSEND_EVENT_POST)
    void PostSizeEvent() { SendSizeEvent(wxSEND_EVENT_POST); }

    // this is the same as SendSizeEventToParent() but using PostSizeEvent()
    void PostSizeEventToParent() { SendSizeEventToParent(wxSEND_EVENT_POST); }

    // These functions should be used before repositioning the children of
    // this window to reduce flicker or, in MSW case, even avoid display
    // corruption in some situations (so they're more than just optimization).
    //
    // EndRepositioningChildren() should be called if and only if
    // BeginRepositioningChildren() returns true. To ensure that this is always
    // done automatically, use ChildrenRepositioningGuard class below.
    virtual bool BeginRepositioningChildren() { return false; }
    virtual void EndRepositioningChildren() { }

    // A simple helper which ensures that EndRepositioningChildren() is called
    // from its dtor if and only if calling BeginRepositioningChildren() from
    // the ctor returned true.
    class ChildrenRepositioningGuard
    {
    public:
        // Notice that window can be NULL here, for convenience. In this case
        // this class simply doesn't do anything.
        wxEXPLICIT ChildrenRepositioningGuard(wxWindowBase* win)
            : m_win(win),
              m_callEnd(win && win->BeginRepositioningChildren())
        {
        }

        ~ChildrenRepositioningGuard()
        {
            if ( m_callEnd )
                m_win->EndRepositioningChildren();
        }

    private:
        wxWindowBase* const m_win;
        const bool m_callEnd;

        wxDECLARE_NO_COPY_CLASS(ChildrenRepositioningGuard);
    };


    // window state
    // ------------

        // returns true if window was shown/hidden, false if the nothing was
        // done (window was already shown/hidden)
    virtual bool Show( bool show = true );
    bool Hide() { return Show(false); }

        // show or hide the window with a special effect, not implemented on
        // most platforms (where it is the same as Show()/Hide() respectively)
        //
        // timeout specifies how long the animation should take, in ms, the
        // default value of 0 means to use the default (system-dependent) value
    virtual bool ShowWithEffect(wxShowEffect WXUNUSED(effect),
                                unsigned WXUNUSED(timeout) = 0)
    {
        return Show();
    }

    virtual bool HideWithEffect(wxShowEffect WXUNUSED(effect),
                                unsigned WXUNUSED(timeout) = 0)
    {
        return Hide();
    }

        // returns true if window was enabled/disabled, false if nothing done
    virtual bool Enable( bool enable = true );
    bool Disable() { return Enable(false); }

    virtual bool IsShown() const { return m_isShown; }
        // returns true if the window is really enabled and false otherwise,
        // whether because it had been explicitly disabled itself or because
        // its parent is currently disabled -- then this method returns false
        // whatever is the intrinsic state of this window, use IsThisEnabled(0
        // to retrieve it. In other words, this relation always holds:
        //
        //   IsEnabled() == IsThisEnabled() && parent.IsEnabled()
        //
    bool IsEnabled() const;

        // returns the internal window state independently of the parent(s)
        // state, i.e. the state in which the window would be if all its
        // parents were enabled (use IsEnabled() above to get the effective
        // window state)
    bool IsThisEnabled() const { return m_isEnabled; }

    // returns true if the window is visible, i.e. IsShown() returns true
    // if called on it and all its parents up to the first TLW
    virtual bool IsShownOnScreen() const;

        // get/set window style (setting style won't update the window and so
        // is only useful for internal usage)
    virtual void SetWindowStyleFlag( long style ) { m_windowStyle = style; }
    virtual long GetWindowStyleFlag() const { return m_windowStyle; }

        // just some (somewhat shorter) synonyms
    void SetWindowStyle( long style ) { SetWindowStyleFlag(style); }
    long GetWindowStyle() const { return GetWindowStyleFlag(); }

        // check if the flag is set
    bool HasFlag(int flag) const { return (m_windowStyle & flag) != 0; }
    virtual bool IsRetained() const { return HasFlag(wxRETAINED); }

        // turn the flag on if it had been turned off before and vice versa,
        // return true if the flag is currently turned on
    bool ToggleWindowStyle(int flag);

        // extra style: the less often used style bits which can't be set with
        // SetWindowStyleFlag()
    virtual void SetExtraStyle(long exStyle) { m_exStyle = exStyle; }
    long GetExtraStyle() const { return m_exStyle; }

    bool HasExtraStyle(int exFlag) const { return (m_exStyle & exFlag) != 0; }

#if WXWIN_COMPATIBILITY_2_8
        // make the window modal (all other windows unresponsive)
    wxDEPRECATED( virtual void MakeModal(bool modal = true) );
#endif

    // (primitive) theming support
    // ---------------------------

    virtual void SetThemeEnabled(bool enableTheme) { m_themeEnabled = enableTheme; }
    virtual bool GetThemeEnabled() const { return m_themeEnabled; }


    // focus and keyboard handling
    // ---------------------------

        // set focus to this window
    virtual void SetFocus() = 0;

        // set focus to this window as the result of a keyboard action
    virtual void SetFocusFromKbd() { SetFocus(); }

        // return the window which currently has the focus or NULL
    static wxWindow *FindFocus();

    static wxWindow *DoFindFocus() /* = 0: implement in derived classes */;

        // return true if the window has focus (handles composite windows
        // correctly - returns true if GetMainWindowOfCompositeControl()
        // has focus)
    virtual bool HasFocus() const;

        // can this window have focus in principle?
        //
        // the difference between AcceptsFocus[FromKeyboard]() and CanAcceptFocus
        // [FromKeyboard]() is that the former functions are meant to be
        // overridden in the derived classes to simply return false if the
        // control can't have focus, while the latter are meant to be used by
        // this class clients and take into account the current window state
    virtual bool AcceptsFocus() const { return true; }

        // can this window or one of its children accept focus?
        //
        // usually it's the same as AcceptsFocus() but is overridden for
        // container windows
    virtual bool AcceptsFocusRecursively() const { return AcceptsFocus(); }

        // can this window be given focus by keyboard navigation? if not, the
        // only way to give it focus (provided it accepts it at all) is to
        // click it
    virtual bool AcceptsFocusFromKeyboard() const { return AcceptsFocus(); }


        // Can this window be focused right now, in its current state? This
        // shouldn't be called at all if AcceptsFocus() returns false.
        //
        // It is a convenient helper for the various functions using it below
        // but also a hook allowing to override the default logic for some rare
        // cases (currently just wxRadioBox in wxMSW) when it's inappropriate.
    virtual bool CanBeFocused() const { return IsShown() && IsEnabled(); }

        // can this window itself have focus?
    bool IsFocusable() const { return AcceptsFocus() && CanBeFocused(); }

        // can this window have focus right now?
        //
        // if this method returns true, it means that calling SetFocus() will
        // put focus either to this window or one of its children, if you need
        // to know whether this window accepts focus itself, use IsFocusable()
    bool CanAcceptFocus() const
        { return AcceptsFocusRecursively() && CanBeFocused(); }

        // can this window be assigned focus from keyboard right now?
    bool CanAcceptFocusFromKeyboard() const
        { return AcceptsFocusFromKeyboard() && CanBeFocused(); }

        // call this when the return value of AcceptsFocus() changes
    virtual void SetCanFocus(bool WXUNUSED(canFocus)) { }

        // navigates inside this window
    bool NavigateIn(int flags = wxNavigationKeyEvent::IsForward)
        { return DoNavigateIn(flags); }

        // navigates in the specified direction from this window, this is
        // equivalent to GetParent()->NavigateIn()
    bool Navigate(int flags = wxNavigationKeyEvent::IsForward)
        { return m_parent && ((wxWindowBase *)m_parent)->DoNavigateIn(flags); }

    // this function will generate the appropriate call to Navigate() if the
    // key event is one normally used for keyboard navigation and return true
    // in this case
    bool HandleAsNavigationKey(const wxKeyEvent& event);

        // move this window just before/after the specified one in tab order
        // (the other window must be our sibling!)
    void MoveBeforeInTabOrder(wxWindow *win)
        { DoMoveInTabOrder(win, OrderBefore); }
    void MoveAfterInTabOrder(wxWindow *win)
        { DoMoveInTabOrder(win, OrderAfter); }


    // parent/children relations
    // -------------------------

        // get the list of children
    const wxWindowList& GetChildren() const { return m_children; }
    wxWindowList& GetChildren() { return m_children; }

    // needed just for extended runtime
    const wxWindowList& GetWindowChildren() const { return GetChildren() ; }

        // get the window before/after this one in the parents children list,
        // returns NULL if this is the first/last window
    wxWindow *GetPrevSibling() const { return DoGetSibling(OrderBefore); }
    wxWindow *GetNextSibling() const { return DoGetSibling(OrderAfter); }

        // get the parent or the parent of the parent
    wxWindow *GetParent() const { return m_parent; }
    inline wxWindow *GetGrandParent() const;

        // is this window a top level one?
    virtual bool IsTopLevel() const;

        // is this window a child or grand child of this one (inside the same
        // TLW)?
    bool IsDescendant(wxWindowBase* win) const;

        // it doesn't really change parent, use Reparent() instead
    void SetParent( wxWindowBase *parent );
        // change the real parent of this window, return true if the parent
        // was changed, false otherwise (error or newParent == oldParent)
    virtual bool Reparent( wxWindowBase *newParent );

        // implementation mostly
    virtual void AddChild( wxWindowBase *child );
    virtual void RemoveChild( wxWindowBase *child );

    // returns true if the child is in the client area of the window, i.e. is
    // not scrollbar, toolbar etc.
    virtual bool IsClientAreaChild(const wxWindow *WXUNUSED(child)) const
        { return true; }

    // looking for windows
    // -------------------

        // find window among the descendants of this one either by id or by
        // name (return NULL if not found)
    wxWindow *FindWindow(long winid) const;
    wxWindow *FindWindow(const wxString& name) const;

        // Find a window among any window (all return NULL if not found)
    static wxWindow *FindWindowById( long winid, const wxWindow *parent = NULL );
    static wxWindow *FindWindowByName( const wxString& name,
                                       const wxWindow *parent = NULL );
    static wxWindow *FindWindowByLabel( const wxString& label,
                                        const wxWindow *parent = NULL );

    // event handler stuff
    // -------------------

        // get the current event handler
    wxEvtHandler *GetEventHandler() const { return m_eventHandler; }

        // replace the event handler (allows to completely subclass the
        // window)
    void SetEventHandler( wxEvtHandler *handler );

        // push/pop event handler: allows to chain a custom event handler to
        // alreasy existing ones
    void PushEventHandler( wxEvtHandler *handler );
    wxEvtHandler *PopEventHandler( bool deleteHandler = false );

        // find the given handler in the event handler chain and remove (but
        // not delete) it from the event handler chain, return true if it was
        // found and false otherwise (this also results in an assert failure so
        // this function should only be called when the handler is supposed to
        // be there)
    bool RemoveEventHandler(wxEvtHandler *handler);

        // Process an event by calling GetEventHandler()->ProcessEvent(): this
        // is a straightforward replacement for ProcessEvent() itself which
        // shouldn't be used directly with windows as it doesn't take into
        // account any event handlers associated with the window
    bool ProcessWindowEvent(wxEvent& event)
        { return GetEventHandler()->ProcessEvent(event); }

        // Call GetEventHandler()->ProcessEventLocally(): this should be used
        // instead of calling ProcessEventLocally() directly on the window
        // itself as this wouldn't take any pushed event handlers into account
        // correctly
    bool ProcessWindowEventLocally(wxEvent& event)
        { return GetEventHandler()->ProcessEventLocally(event); }

        // Process an event by calling GetEventHandler()->ProcessEvent() and
        // handling any exceptions thrown by event handlers. It's mostly useful
        // when processing wx events when called from C code (e.g. in GTK+
        // callback) when the exception wouldn't correctly propagate to
        // wxEventLoop.
    bool HandleWindowEvent(wxEvent& event) const;

        // disable wxEvtHandler double-linked list mechanism:
    virtual void SetNextHandler(wxEvtHandler *handler);
    virtual void SetPreviousHandler(wxEvtHandler *handler);


    // Watcom doesn't allow reducing access with using access declaration, see
    // #10749
#ifndef __WATCOMC__
protected:

    // NOTE: we change the access specifier of the following wxEvtHandler functions
    //       so that the user won't be able to call them directly.
    //       Calling wxWindow::ProcessEvent in fact only works when there are NO
    //       event handlers pushed on the window.
    //       To ensure correct operation, instead of wxWindow::ProcessEvent
    //       you must always call wxWindow::GetEventHandler()->ProcessEvent()
    //       or HandleWindowEvent().
    //       The same holds for all other wxEvtHandler functions.

    using wxEvtHandler::ProcessEvent;
    using wxEvtHandler::ProcessEventLocally;
#if wxUSE_THREADS
    using wxEvtHandler::ProcessThreadEvent;
#endif
    using wxEvtHandler::SafelyProcessEvent;
    using wxEvtHandler::ProcessPendingEvents;
    using wxEvtHandler::AddPendingEvent;
    using wxEvtHandler::QueueEvent;
#endif // __WATCOMC__

public:

    // validators
    // ----------

#if wxUSE_VALIDATORS
        // a window may have an associated validator which is used to control
        // user input
    virtual void SetValidator( const wxValidator &validator );
    virtual wxValidator *GetValidator() { return m_windowValidator; }
#endif // wxUSE_VALIDATORS


    // dialog oriented functions
    // -------------------------

        // validate the correctness of input, return true if ok
    virtual bool Validate();

        // transfer data between internal and GUI representations
    virtual bool TransferDataToWindow();
    virtual bool TransferDataFromWindow();

    virtual void InitDialog();

#if wxUSE_ACCEL
    // accelerators
    // ------------
    virtual void SetAcceleratorTable( const wxAcceleratorTable& accel )
        { m_acceleratorTable = accel; }
    wxAcceleratorTable *GetAcceleratorTable()
        { return &m_acceleratorTable; }

#endif // wxUSE_ACCEL

#if wxUSE_HOTKEY
    // hot keys (system wide accelerators)
    // -----------------------------------

    virtual bool RegisterHotKey(int hotkeyId, int modifiers, int keycode);
    virtual bool UnregisterHotKey(int hotkeyId);
#endif // wxUSE_HOTKEY


    // dialog units translations
    // -------------------------

    wxPoint ConvertPixelsToDialog( const wxPoint& pt ) const;
    wxPoint ConvertDialogToPixels( const wxPoint& pt ) const;
    wxSize ConvertPixelsToDialog( const wxSize& sz ) const
    {
        wxPoint pt(ConvertPixelsToDialog(wxPoint(sz.x, sz.y)));

        return wxSize(pt.x, pt.y);
    }

    wxSize ConvertDialogToPixels( const wxSize& sz ) const
    {
        wxPoint pt(ConvertDialogToPixels(wxPoint(sz.x, sz.y)));

        return wxSize(pt.x, pt.y);
    }

    // mouse functions
    // ---------------

        // move the mouse to the specified position
    virtual void WarpPointer(int x, int y) = 0;

        // start or end mouse capture, these functions maintain the stack of
        // windows having captured the mouse and after calling ReleaseMouse()
        // the mouse is not released but returns to the window which had had
        // captured it previously (if any)
    void CaptureMouse();
    void ReleaseMouse();

        // get the window which currently captures the mouse or NULL
    static wxWindow *GetCapture();

        // does this window have the capture?
    virtual bool HasCapture() const
        { return (wxWindow *)this == GetCapture(); }

    // painting the window
    // -------------------

        // mark the specified rectangle (or the whole window) as "dirty" so it
        // will be repainted
    virtual void Refresh( bool eraseBackground = true,
                          const wxRect *rect = (const wxRect *) NULL ) = 0;

        // a less awkward wrapper for Refresh
    void RefreshRect(const wxRect& rect, bool eraseBackground = true)
    {
        Refresh(eraseBackground, &rect);
    }

        // repaint all invalid areas of the window immediately
    virtual void Update() { }

        // clear the window background
    virtual void ClearBackground();

        // freeze the window: don't redraw it until it is thawed
    void Freeze();

        // thaw the window: redraw it after it had been frozen
    void Thaw();

        // return true if window had been frozen and not unthawed yet
    bool IsFrozen() const { return m_freezeCount != 0; }

        // adjust DC for drawing on this window
    virtual void PrepareDC( wxDC & WXUNUSED(dc) ) { }

        // return true if the window contents is double buffered by the system
    virtual bool IsDoubleBuffered() const { return false; }

        // the update region of the window contains the areas which must be
        // repainted by the program
    const wxRegion& GetUpdateRegion() const { return m_updateRegion; }
    wxRegion& GetUpdateRegion() { return m_updateRegion; }

        // get the update rectangleregion bounding box in client coords
    wxRect GetUpdateClientRect() const;

        // these functions verify whether the given point/rectangle belongs to
        // (or at least intersects with) the update region
    virtual bool DoIsExposed( int x, int y ) const;
    virtual bool DoIsExposed( int x, int y, int w, int h ) const;

    bool IsExposed( int x, int y ) const
        { return DoIsExposed(x, y); }
    bool IsExposed( int x, int y, int w, int h ) const
    { return DoIsExposed(x, y, w, h); }
    bool IsExposed( const wxPoint& pt ) const
        { return DoIsExposed(pt.x, pt.y); }
    bool IsExposed( const wxRect& rect ) const
        { return DoIsExposed(rect.x, rect.y, rect.width, rect.height); }

    // colours, fonts and cursors
    // --------------------------

        // get the default attributes for the controls of this class: we
        // provide a virtual function which can be used to query the default
        // attributes of an existing control and a static function which can
        // be used even when no existing object of the given class is
        // available, but which won't return any styles specific to this
        // particular control, of course (e.g. "Ok" button might have
        // different -- bold for example -- font)
    virtual wxVisualAttributes GetDefaultAttributes() const
    {
        return GetClassDefaultAttributes(GetWindowVariant());
    }

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

        // set/retrieve the window colours (system defaults are used by
        // default): SetXXX() functions return true if colour was changed,
        // SetDefaultXXX() reset the "m_inheritXXX" flag after setting the
        // value to prevent it from being inherited by our children
    virtual bool SetBackgroundColour(const wxColour& colour);
    void SetOwnBackgroundColour(const wxColour& colour)
    {
        if ( SetBackgroundColour(colour) )
            m_inheritBgCol = false;
    }
    wxColour GetBackgroundColour() const;
    bool InheritsBackgroundColour() const
    {
        return m_inheritBgCol;
    }
    bool UseBgCol() const
    {
        return m_hasBgCol;
    }

    virtual bool SetForegroundColour(const wxColour& colour);
    void SetOwnForegroundColour(const wxColour& colour)
    {
        if ( SetForegroundColour(colour) )
            m_inheritFgCol = false;
    }
    wxColour GetForegroundColour() const;

        // Set/get the background style.
    virtual bool SetBackgroundStyle(wxBackgroundStyle style);
    wxBackgroundStyle GetBackgroundStyle() const
        { return m_backgroundStyle; }

        // returns true if the control has "transparent" areas such as a
        // wxStaticText and wxCheckBox and the background should be adapted
        // from a parent window
    virtual bool HasTransparentBackground() { return false; }

        // Returns true if background transparency is supported for this
        // window, i.e. if calling SetBackgroundStyle(wxBG_STYLE_TRANSPARENT)
        // has a chance of succeeding. If reason argument is non-NULL, returns a
        // user-readable explanation of why it isn't supported if the return
        // value is false.
    virtual bool IsTransparentBackgroundSupported(wxString* reason = NULL) const;

        // set/retrieve the font for the window (SetFont() returns true if the
        // font really changed)
    virtual bool SetFont(const wxFont& font) = 0;
    void SetOwnFont(const wxFont& font)
    {
        if ( SetFont(font) )
            m_inheritFont = false;
    }
    wxFont GetFont() const;

        // set/retrieve the cursor for this window (SetCursor() returns true
        // if the cursor was really changed)
    virtual bool SetCursor( const wxCursor &cursor );
    const wxCursor& GetCursor() const { return m_cursor; }

#if wxUSE_CARET
        // associate a caret with the window
    void SetCaret(wxCaret *caret);
        // get the current caret (may be NULL)
    wxCaret *GetCaret() const { return m_caret; }
#endif // wxUSE_CARET

        // get the (average) character size for the current font
    virtual int GetCharHeight() const = 0;
    virtual int GetCharWidth() const = 0;

        // get the width/height/... of the text using current or specified
        // font
    void GetTextExtent(const wxString& string,
                       int *x, int *y,
                       int *descent = NULL,
                       int *externalLeading = NULL,
                       const wxFont *font = NULL) const
    {
        DoGetTextExtent(string, x, y, descent, externalLeading, font);
    }

    wxSize GetTextExtent(const wxString& string) const
    {
        wxCoord w, h;
        GetTextExtent(string, &w, &h);
        return wxSize(w, h);
    }

    // client <-> screen coords
    // ------------------------

        // translate to/from screen/client coordinates (pointers may be NULL)
    void ClientToScreen( int *x, int *y ) const
        { DoClientToScreen(x, y); }
    void ScreenToClient( int *x, int *y ) const
        { DoScreenToClient(x, y); }

        // wxPoint interface to do the same thing
    wxPoint ClientToScreen(const wxPoint& pt) const
    {
        int x = pt.x, y = pt.y;
        DoClientToScreen(&x, &y);

        return wxPoint(x, y);
    }

    wxPoint ScreenToClient(const wxPoint& pt) const
    {
        int x = pt.x, y = pt.y;
        DoScreenToClient(&x, &y);

        return wxPoint(x, y);
    }

        // test where the given (in client coords) point lies
    wxHitTest HitTest(wxCoord x, wxCoord y) const
        { return DoHitTest(x, y); }

    wxHitTest HitTest(const wxPoint& pt) const
        { return DoHitTest(pt.x, pt.y); }

    // misc
    // ----

    // get the window border style from the given flags: this is different from
    // simply doing flags & wxBORDER_MASK because it uses GetDefaultBorder() to
    // translate wxBORDER_DEFAULT to something reasonable
    wxBorder GetBorder(long flags) const;

    // get border for the flags of this window
    wxBorder GetBorder() const { return GetBorder(GetWindowStyleFlag()); }

    // send wxUpdateUIEvents to this window, and children if recurse is true
    virtual void UpdateWindowUI(long flags = wxUPDATE_UI_NONE);

    // do the window-specific processing after processing the update event
    virtual void DoUpdateWindowUI(wxUpdateUIEvent& event) ;

#if wxUSE_MENUS
    // show popup menu at the given position, generate events for the items
    // selected in it
    bool PopupMenu(wxMenu *menu, const wxPoint& pos = wxDefaultPosition)
        { return PopupMenu(menu, pos.x, pos.y); }
    bool PopupMenu(wxMenu *menu, int x, int y);

    // simply return the id of the selected item or wxID_NONE without
    // generating any events
    int GetPopupMenuSelectionFromUser(wxMenu& menu,
                                      const wxPoint& pos = wxDefaultPosition)
        { return DoGetPopupMenuSelectionFromUser(menu, pos.x, pos.y); }
    int GetPopupMenuSelectionFromUser(wxMenu& menu, int x, int y)
        { return DoGetPopupMenuSelectionFromUser(menu, x, y); }
#endif // wxUSE_MENUS

    // override this method to return true for controls having multiple pages
    virtual bool HasMultiplePages() const { return false; }


    // scrollbars
    // ----------

        // can the window have the scrollbar in this orientation?
    virtual bool CanScroll(int orient) const;

        // does the window have the scrollbar in this orientation?
    bool HasScrollbar(int orient) const;

        // configure the window scrollbars
    virtual void SetScrollbar( int orient,
                               int pos,
                               int thumbvisible,
                               int range,
                               bool refresh = true ) = 0;
    virtual void SetScrollPos( int orient, int pos, bool refresh = true ) = 0;
    virtual int GetScrollPos( int orient ) const = 0;
    virtual int GetScrollThumb( int orient ) const = 0;
    virtual int GetScrollRange( int orient ) const = 0;

        // scroll window to the specified position
    virtual void ScrollWindow( int dx, int dy,
                               const wxRect* rect = NULL ) = 0;

        // scrolls window by line/page: note that not all controls support this
        //
        // return true if the position changed, false otherwise
    virtual bool ScrollLines(int WXUNUSED(lines)) { return false; }
    virtual bool ScrollPages(int WXUNUSED(pages)) { return false; }

        // convenient wrappers for ScrollLines/Pages
    bool LineUp() { return ScrollLines(-1); }
    bool LineDown() { return ScrollLines(1); }
    bool PageUp() { return ScrollPages(-1); }
    bool PageDown() { return ScrollPages(1); }

        // call this to always show one or both scrollbars, even if the window
        // is big enough to not require them
    virtual void AlwaysShowScrollbars(bool WXUNUSED(horz) = true,
                                      bool WXUNUSED(vert) = true)
    {
    }

        // return true if AlwaysShowScrollbars() had been called before for the
        // corresponding orientation
    virtual bool IsScrollbarAlwaysShown(int WXUNUSED(orient)) const
    {
        return false;
    }

    // context-sensitive help
    // ----------------------

    // these are the convenience functions wrapping wxHelpProvider methods

#if wxUSE_HELP
        // associate this help text with this window
    void SetHelpText(const wxString& text);

#if WXWIN_COMPATIBILITY_2_8
    // Associate this help text with all windows with the same id as this one.
    // Don't use this, do wxHelpProvider::Get()->AddHelp(id, text);
    wxDEPRECATED( void SetHelpTextForId(const wxString& text) );
#endif // WXWIN_COMPATIBILITY_2_8

        // get the help string associated with the given position in this window
        //
        // notice that pt may be invalid if event origin is keyboard or unknown
        // and this method should return the global window help text then
    virtual wxString GetHelpTextAtPoint(const wxPoint& pt,
                                        wxHelpEvent::Origin origin) const;
        // returns the position-independent help text
    wxString GetHelpText() const
    {
        return GetHelpTextAtPoint(wxDefaultPosition, wxHelpEvent::Origin_Unknown);
    }

#else // !wxUSE_HELP
    // silently ignore SetHelpText() calls
    void SetHelpText(const wxString& WXUNUSED(text)) { }
    void SetHelpTextForId(const wxString& WXUNUSED(text)) { }
#endif // wxUSE_HELP

    // tooltips
    // --------

#if wxUSE_TOOLTIPS
        // the easiest way to set a tooltip for a window is to use this method
    void SetToolTip( const wxString &tip );
        // attach a tooltip to the window, pointer can be NULL to remove
        // existing tooltip
    void SetToolTip( wxToolTip *tip ) { DoSetToolTip(tip); }
        // more readable synonym for SetToolTip(NULL)
    void UnsetToolTip() { SetToolTip(NULL); }
        // get the associated tooltip or NULL if none
    wxToolTip* GetToolTip() const { return m_tooltip; }
    wxString GetToolTipText() const;

    // Use the same tool tip as the given one (which can be NULL to indicate
    // that no tooltip should be used) for this window. This is currently only
    // used by wxCompositeWindow::DoSetToolTip() implementation and is not part
    // of the public wx API.
    //
    // Returns true if tip was valid and we copied it or false if it was NULL
    // and we reset our own tooltip too.
    bool CopyToolTip(wxToolTip *tip);
#else // !wxUSE_TOOLTIPS
        // make it much easier to compile apps in an environment
        // that doesn't support tooltips, such as PocketPC
    void SetToolTip(const wxString & WXUNUSED(tip)) { }
    void UnsetToolTip() { }
#endif // wxUSE_TOOLTIPS/!wxUSE_TOOLTIPS

    // drag and drop
    // -------------
#if wxUSE_DRAG_AND_DROP
        // set/retrieve the drop target associated with this window (may be
        // NULL; it's owned by the window and will be deleted by it)
    virtual void SetDropTarget( wxDropTarget *dropTarget ) = 0;
    virtual wxDropTarget *GetDropTarget() const { return m_dropTarget; }

    // Accept files for dragging
    virtual void DragAcceptFiles(bool accept)
#ifdef __WXMSW__
    // it does have common implementation but not for MSW which has its own
    // native version of it
    = 0
#endif // __WXMSW__
    ;

#endif // wxUSE_DRAG_AND_DROP

    // constraints and sizers
    // ----------------------
#if wxUSE_CONSTRAINTS
        // set the constraints for this window or retrieve them (may be NULL)
    void SetConstraints( wxLayoutConstraints *constraints );
    wxLayoutConstraints *GetConstraints() const { return m_constraints; }

        // implementation only
    void UnsetConstraints(wxLayoutConstraints *c);
    wxWindowList *GetConstraintsInvolvedIn() const
        { return m_constraintsInvolvedIn; }
    void AddConstraintReference(wxWindowBase *otherWin);
    void RemoveConstraintReference(wxWindowBase *otherWin);
    void DeleteRelatedConstraints();
    void ResetConstraints();

        // these methods may be overridden for special layout algorithms
    virtual void SetConstraintSizes(bool recurse = true);
    virtual bool LayoutPhase1(int *noChanges);
    virtual bool LayoutPhase2(int *noChanges);
    virtual bool DoPhase(int phase);

        // these methods are virtual but normally won't be overridden
    virtual void SetSizeConstraint(int x, int y, int w, int h);
    virtual void MoveConstraint(int x, int y);
    virtual void GetSizeConstraint(int *w, int *h) const ;
    virtual void GetClientSizeConstraint(int *w, int *h) const ;
    virtual void GetPositionConstraint(int *x, int *y) const ;

#endif // wxUSE_CONSTRAINTS

        // when using constraints or sizers, it makes sense to update
        // children positions automatically whenever the window is resized
        // - this is done if autoLayout is on
    void SetAutoLayout( bool autoLayout ) { m_autoLayout = autoLayout; }
    bool GetAutoLayout() const { return m_autoLayout; }

        // lay out the window and its children
    virtual bool Layout();

        // sizers
    void SetSizer(wxSizer *sizer, bool deleteOld = true );
    void SetSizerAndFit( wxSizer *sizer, bool deleteOld = true );

    wxSizer *GetSizer() const { return m_windowSizer; }

    // Track if this window is a member of a sizer
    void SetContainingSizer(wxSizer* sizer);
    wxSizer *GetContainingSizer() const { return m_containingSizer; }

    // accessibility
    // ----------------------
#if wxUSE_ACCESSIBILITY
    // Override to create a specific accessible object.
    virtual wxAccessible* CreateAccessible();

    // Sets the accessible object.
    void SetAccessible(wxAccessible* accessible) ;

    // Returns the accessible object.
    wxAccessible* GetAccessible() { return m_accessible; }

    // Returns the accessible object, creating if necessary.
    wxAccessible* GetOrCreateAccessible() ;
#endif


    // Set window transparency if the platform supports it
    virtual bool SetTransparent(wxByte WXUNUSED(alpha)) { return false; }
    virtual bool CanSetTransparent() { return false; }


    // implementation
    // --------------

        // event handlers
    void OnSysColourChanged( wxSysColourChangedEvent& event );
    void OnInitDialog( wxInitDialogEvent &event );
    void OnMiddleClick( wxMouseEvent& event );
#if wxUSE_HELP
    void OnHelp(wxHelpEvent& event);
#endif // wxUSE_HELP

        // virtual function for implementing internal idle
        // behaviour
        virtual void OnInternalIdle();

    // Send idle event to window and all subwindows
    // Returns true if more idle time is requested.
    virtual bool SendIdleEvents(wxIdleEvent& event);

        // get the handle of the window for the underlying window system: this
        // is only used for wxWin itself or for user code which wants to call
        // platform-specific APIs
    virtual WXWidget GetHandle() const = 0;
        // associate the window with a new native handle
    virtual void AssociateHandle(WXWidget WXUNUSED(handle)) { }
        // dissociate the current native handle from the window
    virtual void DissociateHandle() { }

#if wxUSE_PALETTE
        // Store the palette used by DCs in wxWindow so that the dcs can share
        // a palette. And we can respond to palette messages.
    wxPalette GetPalette() const { return m_palette; }

        // When palette is changed tell the DC to set the system palette to the
        // new one.
    void SetPalette(const wxPalette& pal);

        // return true if we have a specific palette
    bool HasCustomPalette() const { return m_hasCustomPalette; }

        // return the first parent window with a custom palette or NULL
    wxWindow *GetAncestorWithCustomPalette() const;
#endif // wxUSE_PALETTE

    // inherit the parents visual attributes if they had been explicitly set
    // by the user (i.e. we don't inherit default attributes) and if we don't
    // have our own explicitly set
    virtual void InheritAttributes();

    // returns false from here if this window doesn't want to inherit the
    // parents colours even if InheritAttributes() would normally do it
    //
    // this just provides a simple way to customize InheritAttributes()
    // behaviour in the most common case
    virtual bool ShouldInheritColours() const { return false; }

    // returns true if the window can be positioned outside of parent's client
    // area (normal windows can't, but e.g. menubar or statusbar can):
    virtual bool CanBeOutsideClientArea() const { return false; }

    // returns true if the platform should explicitly apply a theme border. Currently
    // used only by Windows
    virtual bool CanApplyThemeBorder() const { return true; }

    // returns the main window of composite control; this is the window
    // that FindFocus returns if the focus is in one of composite control's
    // windows
    virtual wxWindow *GetMainWindowOfCompositeControl()
        { return (wxWindow*)this; }

    // If this function returns true, keyboard navigation events shouldn't
    // escape from it. A typical example of such "navigation domain" is a top
    // level window because pressing TAB in one of them must not transfer focus
    // to a different top level window. But it's not limited to them, e.g. MDI
    // children frames are not top level windows (and their IsTopLevel()
    // returns false) but still are self-contained navigation domains as well.
    virtual bool IsTopNavigationDomain() const { return false; }


protected:
    // helper for the derived class Create() methods: the first overload, with
    // validator parameter, should be used for child windows while the second
    // one is used for top level ones
    bool CreateBase(wxWindowBase *parent,
                    wxWindowID winid,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = 0,
                    const wxValidator& validator = wxDefaultValidator,
                    const wxString& name = wxPanelNameStr);

    bool CreateBase(wxWindowBase *parent,
                    wxWindowID winid,
                    const wxPoint& pos,
                    const wxSize& size,
                    long style,
                    const wxString& name);

    // event handling specific to wxWindow
    virtual bool TryBefore(wxEvent& event);
    virtual bool TryAfter(wxEvent& event);

    enum WindowOrder
    {
        OrderBefore,     // insert before the given window
        OrderAfter       // insert after the given window
    };

    // common part of GetPrev/NextSibling()
    wxWindow *DoGetSibling(WindowOrder order) const;

    // common part of MoveBefore/AfterInTabOrder()
    virtual void DoMoveInTabOrder(wxWindow *win, WindowOrder move);

    // implementation of Navigate() and NavigateIn()
    virtual bool DoNavigateIn(int flags);

#if wxUSE_CONSTRAINTS
    // satisfy the constraints for the windows but don't set the window sizes
    void SatisfyConstraints();
#endif // wxUSE_CONSTRAINTS

    // Send the wxWindowDestroyEvent if not done yet and sets m_isBeingDeleted
    // to true
    void SendDestroyEvent();

    // this method should be implemented to use operating system specific code
    // to really enable/disable the widget, it will only be called when we
    // really need to enable/disable window and so no additional checks on the
    // widgets state are necessary
    virtual void DoEnable(bool WXUNUSED(enable)) { }


    // the window id - a number which uniquely identifies a window among
    // its siblings unless it is wxID_ANY
    wxWindowIDRef        m_windowId;

    // the parent window of this window (or NULL) and the list of the children
    // of this window
    wxWindow            *m_parent;
    wxWindowList         m_children;

    // the minimal allowed size for the window (no minimal size if variable(s)
    // contain(s) wxDefaultCoord)
    int                  m_minWidth,
                         m_minHeight,
                         m_maxWidth,
                         m_maxHeight;

    // event handler for this window: usually is just 'this' but may be
    // changed with SetEventHandler()
    wxEvtHandler        *m_eventHandler;

#if wxUSE_VALIDATORS
    // associated validator or NULL if none
    wxValidator         *m_windowValidator;
#endif // wxUSE_VALIDATORS

#if wxUSE_DRAG_AND_DROP
    wxDropTarget        *m_dropTarget;
#endif // wxUSE_DRAG_AND_DROP

    // visual window attributes
    wxCursor             m_cursor;
    wxFont               m_font;                // see m_hasFont
    wxColour             m_backgroundColour,    //     m_hasBgCol
                         m_foregroundColour;    //     m_hasFgCol

#if wxUSE_CARET
    wxCaret             *m_caret;
#endif // wxUSE_CARET

    // the region which should be repainted in response to paint event
    wxRegion             m_updateRegion;

#if wxUSE_ACCEL
    // the accelerator table for the window which translates key strokes into
    // command events
    wxAcceleratorTable   m_acceleratorTable;
#endif // wxUSE_ACCEL

    // the tooltip for this window (may be NULL)
#if wxUSE_TOOLTIPS
    wxToolTip           *m_tooltip;
#endif // wxUSE_TOOLTIPS

    // constraints and sizers
#if wxUSE_CONSTRAINTS
    // the constraints for this window or NULL
    wxLayoutConstraints *m_constraints;

    // constraints this window is involved in
    wxWindowList        *m_constraintsInvolvedIn;
#endif // wxUSE_CONSTRAINTS

    // this window's sizer
    wxSizer             *m_windowSizer;

    // The sizer this window is a member of, if any
    wxSizer             *m_containingSizer;

    // Layout() window automatically when its size changes?
    bool                 m_autoLayout:1;

    // window state
    bool                 m_isShown:1;
    bool                 m_isEnabled:1;
    bool                 m_isBeingDeleted:1;

    // was the window colours/font explicitly changed by user?
    bool                 m_hasBgCol:1;
    bool                 m_hasFgCol:1;
    bool                 m_hasFont:1;

    // and should it be inherited by children?
    bool                 m_inheritBgCol:1;
    bool                 m_inheritFgCol:1;
    bool                 m_inheritFont:1;

    // window attributes
    long                 m_windowStyle,
                         m_exStyle;
    wxString             m_windowName;
    bool                 m_themeEnabled;
    wxBackgroundStyle    m_backgroundStyle;
#if wxUSE_PALETTE
    wxPalette            m_palette;
    bool                 m_hasCustomPalette;
#endif // wxUSE_PALETTE

#if wxUSE_ACCESSIBILITY
    wxAccessible*       m_accessible;
#endif

    // Virtual size (scrolling)
    wxSize                m_virtualSize;

    wxScrollHelper       *m_scrollHelper;

    wxWindowVariant       m_windowVariant ;

    // override this to change the default (i.e. used when no style is
    // specified) border for the window class
    virtual wxBorder GetDefaultBorder() const;

    // this allows you to implement standard control borders without
    // repeating the code in different classes that are not derived from
    // wxControl
    virtual wxBorder GetDefaultBorderForControl() const { return wxBORDER_THEME; }

    // Get the default size for the new window if no explicit size given. TLWs
    // have their own default size so this is just for non top-level windows.
    static int WidthDefault(int w) { return w == wxDefaultCoord ? 20 : w; }
    static int HeightDefault(int h) { return h == wxDefaultCoord ? 20 : h; }


    // Used to save the results of DoGetBestSize so it doesn't need to be
    // recalculated each time the value is needed.
    wxSize m_bestSizeCache;

    wxDEPRECATED_MSG("use SetInitialSize() instead.")
    void SetBestSize(const wxSize& size);
    wxDEPRECATED_MSG("use SetInitialSize() instead.")
    virtual void SetInitialBestSize(const wxSize& size);



    // more pure virtual functions
    // ---------------------------

    // NB: we must have DoSomething() function when Something() is an overloaded
    //     method: indeed, we can't just have "virtual Something()" in case when
    //     the function is overloaded because then we'd have to make virtual all
    //     the variants (otherwise only the virtual function may be called on a
    //     pointer to derived class according to C++ rules) which is, in
    //     general, absolutely not needed. So instead we implement all
    //     overloaded Something()s in terms of DoSomething() which will be the
    //     only one to be virtual.

    // text extent
    virtual void DoGetTextExtent(const wxString& string,
                                 int *x, int *y,
                                 int *descent = NULL,
                                 int *externalLeading = NULL,
                                 const wxFont *font = NULL) const = 0;

    // coordinates translation
    virtual void DoClientToScreen( int *x, int *y ) const = 0;
    virtual void DoScreenToClient( int *x, int *y ) const = 0;

    virtual wxHitTest DoHitTest(wxCoord x, wxCoord y) const;

    // capture/release the mouse, used by Capture/ReleaseMouse()
    virtual void DoCaptureMouse() = 0;
    virtual void DoReleaseMouse() = 0;

    // retrieve the position/size of the window
    virtual void DoGetPosition(int *x, int *y) const = 0;
    virtual void DoGetScreenPosition(int *x, int *y) const;
    virtual void DoGetSize(int *width, int *height) const = 0;
    virtual void DoGetClientSize(int *width, int *height) const = 0;

    // get the size which best suits the window: for a control, it would be
    // the minimal size which doesn't truncate the control, for a panel - the
    // same size as it would have after a call to Fit()
    virtual wxSize DoGetBestSize() const;

    // this method can be overridden instead of DoGetBestSize() if it computes
    // the best size of the client area of the window only, excluding borders
    // (GetBorderSize() will be used to add them)
    virtual wxSize DoGetBestClientSize() const { return wxDefaultSize; }

    // These two methods can be overridden to implement intelligent
    // width-for-height and/or height-for-width best size determination for the
    // window. By default the fixed best size is used.
    virtual int DoGetBestClientHeight(int WXUNUSED(width)) const
        { return wxDefaultCoord; }
    virtual int DoGetBestClientWidth(int WXUNUSED(height)) const
        { return wxDefaultCoord; }

    // this is the virtual function to be overridden in any derived class which
    // wants to change how SetSize() or Move() works - it is called by all
    // versions of these functions in the base class
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO) = 0;

    // same as DoSetSize() for the client size
    virtual void DoSetClientSize(int width, int height) = 0;

    virtual void DoSetSizeHints( int minW, int minH,
                                 int maxW, int maxH,
                                 int incW, int incH );

    // return the total size of the window borders, i.e. the sum of the widths
    // of the left and the right border in the x component of the returned size
    // and the sum of the heights of the top and bottom borders in the y one
    //
    // NB: this is currently only implemented properly for wxMSW, wxGTK and
    //     wxUniv and doesn't behave correctly in the presence of scrollbars in
    //     the other ports
    virtual wxSize DoGetBorderSize() const;

    // move the window to the specified location and resize it: this is called
    // from both DoSetSize() and DoSetClientSize() and would usually just
    // reposition this window except for composite controls which will want to
    // arrange themselves inside the given rectangle
    //
    // Important note: the coordinates passed to this method are in parent's
    // *window* coordinates and not parent's client coordinates (as the values
    // passed to DoSetSize and returned by DoGetPosition are)!
    virtual void DoMoveWindow(int x, int y, int width, int height) = 0;

    // centre the window in the specified direction on parent, note that
    // wxCENTRE_ON_SCREEN shouldn't be specified here, it only makes sense for
    // TLWs
    virtual void DoCentre(int dir);

#if wxUSE_TOOLTIPS
    virtual void DoSetToolTip( wxToolTip *tip );
#endif // wxUSE_TOOLTIPS

#if wxUSE_MENUS
    virtual bool DoPopupMenu(wxMenu *menu, int x, int y) = 0;
#endif // wxUSE_MENUS

    // Makes an adjustment to the window position to make it relative to the
    // parents client area, e.g. if the parent is a frame with a toolbar, its
    // (0, 0) is just below the toolbar
    virtual void AdjustForParentClientOrigin(int& x, int& y,
                                             int sizeFlags = 0) const;

    // implements the window variants
    virtual void DoSetWindowVariant( wxWindowVariant variant ) ;


    // really freeze/thaw the window (should have port-specific implementation)
    virtual void DoFreeze() { }
    virtual void DoThaw() { }


    // Must be called when mouse capture is lost to send
    // wxMouseCaptureLostEvent to windows on capture stack.
    static void NotifyCaptureLost();

private:
    // recursively call our own and our children DoEnable() when the
    // enabled/disabled status changed because a parent window had been
    // enabled/disabled
    void NotifyWindowOnEnableChange(bool enabled);

#if wxUSE_MENUS
    // temporary event handlers used by GetPopupMenuSelectionFromUser()
    void InternalOnPopupMenu(wxCommandEvent& event);
    void InternalOnPopupMenuUpdate(wxUpdateUIEvent& event);

    // implementation of the public GetPopupMenuSelectionFromUser() method
    int DoGetPopupMenuSelectionFromUser(wxMenu& menu, int x, int y);
#endif // wxUSE_MENUS

    // layout the window children when its size changes unless this was
    // explicitly disabled with SetAutoLayout(false)
    void InternalOnSize(wxSizeEvent& event);

    // base for dialog unit conversion, i.e. average character size
    wxSize GetDlgUnitBase() const;


    // number of Freeze() calls minus the number of Thaw() calls: we're frozen
    // (i.e. not being updated) if it is positive
    unsigned int m_freezeCount;


    DECLARE_ABSTRACT_CLASS(wxWindowBase)
    wxDECLARE_NO_COPY_CLASS(wxWindowBase);
    DECLARE_EVENT_TABLE()
};



// Inlines for some deprecated methods
inline wxSize wxWindowBase::GetBestFittingSize() const
{
    return GetEffectiveMinSize();
}

inline void wxWindowBase::SetBestFittingSize(const wxSize& size)
{
    SetInitialSize(size);
}

inline void wxWindowBase::SetBestSize(const wxSize& size)
{
    SetInitialSize(size);
}

inline void wxWindowBase::SetInitialBestSize(const wxSize& size)
{
    SetInitialSize(size);
}


// ----------------------------------------------------------------------------
// now include the declaration of wxWindow class
// ----------------------------------------------------------------------------

// include the declaration of the platform-specific class
#if defined(__WXMSW__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowMSW
    #else // !wxUniv
        #define wxWindowMSW wxWindow
    #endif // wxUniv/!wxUniv
    #include "wx/msw/window.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/window.h"
#elif defined(__WXGTK20__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowGTK
    #else // !wxUniv
        #define wxWindowGTK wxWindow
    #endif // wxUniv
    #include "wx/gtk/window.h"
#elif defined(__WXGTK__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowGTK
    #else // !wxUniv
        #define wxWindowGTK wxWindow
    #endif // wxUniv
    #include "wx/gtk1/window.h"
#elif defined(__WXX11__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowX11
    #else // !wxUniv
        #define wxWindowX11 wxWindow
    #endif // wxUniv
    #include "wx/x11/window.h"
#elif defined(__WXDFB__)
    #define wxWindowNative wxWindowDFB
    #include "wx/dfb/window.h"
#elif defined(__WXMAC__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowMac
    #else // !wxUniv
        #define wxWindowMac wxWindow
    #endif // wxUniv
    #include "wx/osx/window.h"
#elif defined(__WXCOCOA__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowCocoa
    #else // !wxUniv
        #define wxWindowCocoa wxWindow
    #endif // wxUniv
    #include "wx/cocoa/window.h"
#elif defined(__WXPM__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowOS2
    #else // !wxUniv
        #define wxWindowOS2 wxWindow
    #endif // wxUniv/!wxUniv
    #include "wx/os2/window.h"
#endif

// for wxUniversal, we now derive the real wxWindow from wxWindow<platform>,
// for the native ports we already have defined it above
#if defined(__WXUNIVERSAL__)
    #ifndef wxWindowNative
        #error "wxWindowNative must be defined above!"
    #endif

    #include "wx/univ/window.h"
#endif // wxUniv

// ----------------------------------------------------------------------------
// inline functions which couldn't be declared in the class body because of
// forward dependencies
// ----------------------------------------------------------------------------

inline wxWindow *wxWindowBase::GetGrandParent() const
{
    return m_parent ? m_parent->GetParent() : NULL;
}

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

// Find the wxWindow at the current mouse position, also returning the mouse
// position.
extern WXDLLIMPEXP_CORE wxWindow* wxFindWindowAtPointer(wxPoint& pt);

// Get the current mouse position.
extern WXDLLIMPEXP_CORE wxPoint wxGetMousePosition();

// get the currently active window of this application or NULL
extern WXDLLIMPEXP_CORE wxWindow *wxGetActiveWindow();

// get the (first) top level parent window
WXDLLIMPEXP_CORE wxWindow* wxGetTopLevelParent(wxWindow *win);

#if WXWIN_COMPATIBILITY_2_6
    wxDEPRECATED_MSG("use wxWindow::NewControlId() instead")
    inline wxWindowID NewControlId() { return wxWindowBase::NewControlId(); }
#endif // WXWIN_COMPATIBILITY_2_6

#if wxUSE_ACCESSIBILITY
// ----------------------------------------------------------------------------
// accessible object for windows
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowAccessible: public wxAccessible
{
public:
    wxWindowAccessible(wxWindow* win): wxAccessible(win) { if (win) win->SetAccessible(this); }
    virtual ~wxWindowAccessible() {}

// Overridables

        // Can return either a child object, or an integer
        // representing the child element, starting from 1.
    virtual wxAccStatus HitTest(const wxPoint& pt, int* childId, wxAccessible** childObject);

        // Returns the rectangle for this object (id = 0) or a child element (id > 0).
    virtual wxAccStatus GetLocation(wxRect& rect, int elementId);

        // Navigates from fromId to toId/toObject.
    virtual wxAccStatus Navigate(wxNavDir navDir, int fromId,
                int* toId, wxAccessible** toObject);

        // Gets the name of the specified object.
    virtual wxAccStatus GetName(int childId, wxString* name);

        // Gets the number of children.
    virtual wxAccStatus GetChildCount(int* childCount);

        // Gets the specified child (starting from 1).
        // If *child is NULL and return value is wxACC_OK,
        // this means that the child is a simple element and
        // not an accessible object.
    virtual wxAccStatus GetChild(int childId, wxAccessible** child);

        // Gets the parent, or NULL.
    virtual wxAccStatus GetParent(wxAccessible** parent);

        // Performs the default action. childId is 0 (the action for this object)
        // or > 0 (the action for a child).
        // Return wxACC_NOT_SUPPORTED if there is no default action for this
        // window (e.g. an edit control).
    virtual wxAccStatus DoDefaultAction(int childId);

        // Gets the default action for this object (0) or > 0 (the action for a child).
        // Return wxACC_OK even if there is no action. actionName is the action, or the empty
        // string if there is no action.
        // The retrieved string describes the action that is performed on an object,
        // not what the object does as a result. For example, a toolbar button that prints
        // a document has a default action of "Press" rather than "Prints the current document."
    virtual wxAccStatus GetDefaultAction(int childId, wxString* actionName);

        // Returns the description for this object or a child.
    virtual wxAccStatus GetDescription(int childId, wxString* description);

        // Returns help text for this object or a child, similar to tooltip text.
    virtual wxAccStatus GetHelpText(int childId, wxString* helpText);

        // Returns the keyboard shortcut for this object or child.
        // Return e.g. ALT+K
    virtual wxAccStatus GetKeyboardShortcut(int childId, wxString* shortcut);

        // Returns a role constant.
    virtual wxAccStatus GetRole(int childId, wxAccRole* role);

        // Returns a state constant.
    virtual wxAccStatus GetState(int childId, long* state);

        // Returns a localized string representing the value for the object
        // or child.
    virtual wxAccStatus GetValue(int childId, wxString* strValue);

        // Selects the object or child.
    virtual wxAccStatus Select(int childId, wxAccSelectionFlags selectFlags);

        // Gets the window with the keyboard focus.
        // If childId is 0 and child is NULL, no object in
        // this subhierarchy has the focus.
        // If this object has the focus, child should be 'this'.
    virtual wxAccStatus GetFocus(int* childId, wxAccessible** child);

#if wxUSE_VARIANT
        // Gets a variant representing the selected children
        // of this object.
        // Acceptable values:
        // - a null variant (IsNull() returns true)
        // - a list variant (GetType() == wxT("list")
        // - an integer representing the selected child element,
        //   or 0 if this object is selected (GetType() == wxT("long")
        // - a "void*" pointer to a wxAccessible child object
    virtual wxAccStatus GetSelections(wxVariant* selections);
#endif // wxUSE_VARIANT
};

#endif // wxUSE_ACCESSIBILITY


#endif // _WX_WINDOW_H_BASE_
