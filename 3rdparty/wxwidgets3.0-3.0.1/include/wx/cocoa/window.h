/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/window.h
// Purpose:     wxWindowCocoa
// Author:      David Elliott
// Modified by:
// Created:     2002/12/26
// Copyright:   (c) 2002 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_WINDOW_H__
#define __WX_COCOA_WINDOW_H__

#include "wx/cocoa/NSView.h"

#ifdef __OBJC__
    #import <Foundation/NSGeometry.h>
#endif //def __OBJC__

// We can only import Foundation/NSGeometry.h from Objective-C code but it's
// nice to be able to use NSPoint and NSRect in the declarations of helper
// methods so we must define them as opaque structs identically to the way
// they are defined by the real header.
// NOTE: We specifically use these regardless of C++ or Objective-C++ mode so
// the compiler will complain if we got the definitions wrong.  In regular
// C++ mode there is no way to know if we got the definitons right so
// we depend on at least one Objective-C++ file including this header.
#if defined(__LP64__) || defined(NS_BUILD_32_LIKE_64)
typedef struct CGPoint NSPoint;
typedef struct CGRect NSRect;
#else
typedef struct _NSPoint NSPoint;
typedef struct _NSRect NSRect;
#endif

DECLARE_WXCOCOA_OBJC_CLASS(NSAffineTransform);

class wxWindowCocoaHider;
class wxWindowCocoaScrollView;
class wxCocoaTrackingRectManager;

// ========================================================================
// wxWindowCocoa
// ========================================================================
class WXDLLIMPEXP_CORE wxWindowCocoa: public wxWindowBase, protected wxCocoaNSView
{
    DECLARE_DYNAMIC_CLASS(wxWindowCocoa)
    wxDECLARE_NO_COPY_CLASS(wxWindowCocoa);
    DECLARE_EVENT_TABLE()
    friend wxWindow *wxWindowBase::GetCapture();
    friend class wxWindowCocoaScrollView;
    friend class wxCocoaTrackingRectManager;
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxWindowCocoa() { Init(); }
    inline wxWindowCocoa(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = 0,
            const wxString& name = wxPanelNameStr)
    {
        Init();
        Create(parent, winid, pos, size, style, name);
    }

    virtual ~wxWindowCocoa();

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = 0,
            const wxString& name = wxPanelNameStr);
protected:
    void Init();
// ------------------------------------------------------------------------
// Cocoa specifics
// ------------------------------------------------------------------------
public:
    // Returns the content NSView (where children are added, drawing performed)
    inline WX_NSView GetNSView() { return m_cocoaNSView; }
    // Returns the NSView suitable for use as a subview
    WX_NSView GetNSViewForSuperview() const;
    // Returns the NSView that may be hidden/is being hidden
    WX_NSView GetNSViewForHiding() const;
    // Returns the NSView for non-client drawing
    virtual WX_NSView GetNonClientNSView() { return GetNSViewForSuperview(); }
    // Add/remove children
    void CocoaAddChild(wxWindowCocoa *child);
    void CocoaRemoveFromParent(void);
#ifdef __OBJC__
    // Returns an autoreleased NSAffineTransform which can be applied
    // to a graphics context currently using the view's coordinate system
    // (such as the one locked when drawRect is called or after a call
    // to [NSView lockFocus]) such that further drawing is done using
    // the wxWidgets coordinate system.
    WX_NSAffineTransform CocoaGetWxToBoundsTransform();
#endif //def __OBJC__
protected:
    // actually enable/disable the cocoa control, overridden by subclasses
    virtual void CocoaSetEnabled(bool WXUNUSED(enable)) { }

    void CocoaCreateNSScrollView();
    void InitMouseEvent(wxMouseEvent &event, WX_NSEvent cocoaEvent);
    virtual wxWindow* GetWxWindow() const;
    virtual void Cocoa_FrameChanged(void);
    virtual void Cocoa_synthesizeMouseMoved(void);
    virtual bool Cocoa_drawRect(const NSRect &rect);
    virtual bool Cocoa_mouseDown(WX_NSEvent theEvent);
    virtual bool Cocoa_mouseDragged(WX_NSEvent theEvent);
    virtual bool Cocoa_mouseUp(WX_NSEvent theEvent);
    virtual bool Cocoa_mouseMoved(WX_NSEvent theEvent);
    virtual bool Cocoa_mouseEntered(WX_NSEvent theEvent);
    virtual bool Cocoa_mouseExited(WX_NSEvent theEvent);
    virtual bool Cocoa_rightMouseDown(WX_NSEvent theEvent);
    virtual bool Cocoa_rightMouseDragged(WX_NSEvent theEvent);
    virtual bool Cocoa_rightMouseUp(WX_NSEvent theEvent);
    virtual bool Cocoa_otherMouseDown(WX_NSEvent theEvent);
    virtual bool Cocoa_otherMouseDragged(WX_NSEvent theEvent);
    virtual bool Cocoa_otherMouseUp(WX_NSEvent theEvent);
    virtual bool Cocoa_resetCursorRects();
    virtual bool Cocoa_viewDidMoveToWindow();
    virtual bool Cocoa_viewWillMoveToWindow(WX_NSWindow newWindow);
    void SetNSView(WX_NSView cocoaNSView);
    WX_NSView m_cocoaNSView;
    wxWindowCocoaHider *m_cocoaHider;
    wxWindowCocoaScrollView *m_wxCocoaScrollView;
    bool m_isInPaint;
    wxCocoaTrackingRectManager *m_visibleTrackingRectManager;
    static wxWindow *sm_capturedWindow;
    virtual void CocoaReplaceView(WX_NSView oldView, WX_NSView newView);
    void SetInitialFrameRect(const wxPoint& pos, const wxSize& size);
#ifdef __OBJC__
    inline NSRect MakeDefaultNSRect(const wxSize& size)
    {
        // NOTE: position is 10,10 to make it "obvious" that it's out of place
        return NSMakeRect(10.0,10.0,WidthDefault(size.x),HeightDefault(size.y));
    }
    // These functions translate NSPoint or NSRect between the coordinate
    // system of Cocoa's boudns rect and wx's coordinate system.
    NSPoint CocoaTransformBoundsToWx(NSPoint pointBounds);
    NSRect CocoaTransformBoundsToWx(NSRect rectBounds);
    NSPoint CocoaTransformWxToBounds(NSPoint pointWx);
    NSRect CocoaTransformWxToBounds(NSRect rectWx);
#endif //def __OBJC__
    static wxPoint OriginInWxDisplayCoordinatesForRectInCocoaScreenCoordinates(NSRect windowFrame);
    static NSPoint OriginInCocoaScreenCoordinatesForRectInWxDisplayCoordinates(wxCoord x, wxCoord y, wxCoord width, wxCoord height, bool keepOriginVisible);
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    /* Pure Virtuals */
    // Raise the window to the top of the Z order
    virtual void Raise();
    // Lower the window to the bottom of the Z order
    virtual void Lower();
    // Set the focus to this window
    virtual void SetFocus();
    // Warp the pointer the given position
    virtual void WarpPointer(int x_pos, int y_pos) ;
    // Change the window's cursor
    virtual bool SetCursor( const wxCursor &cursor );
    // Send the window a refresh event
    virtual void Refresh(bool eraseBack = true, const wxRect *rect = NULL);
    // Set/get the window's font
    virtual bool SetFont(const wxFont& f);
//    inline virtual wxFont& GetFont() const;
    virtual void SetLabel(const wxString& label);
    virtual wxString GetLabel() const;
    // label handling
    // Get character size
    virtual int GetCharHeight() const;
    virtual int GetCharWidth() const;
    virtual void DoGetTextExtent(const wxString& string, int *x, int *y,
                             int *descent = NULL,
                             int *externalLeading = NULL,
                             const wxFont *theFont = NULL) const;
    // Scroll stuff
    virtual void SetScrollbar(int orient, int pos, int thumbVisible,
      int range, bool refresh = true);
    virtual void SetScrollPos(int orient, int pos, bool refresh = true);
    virtual int GetScrollPos(int orient) const;
    virtual int GetScrollThumb(int orient) const;
    virtual int GetScrollRange(int orient) const;
    virtual void ScrollWindow(int dx, int dy, const wxRect *rect = NULL);
    virtual void DoSetVirtualSize(int x, int y);
    // Get the private handle (platform-dependent)
    virtual WXWidget GetHandle() const;
    // Convert client to screen coordinates
    virtual void DoClientToScreen(int *x, int *y) const;
    // Convert screen to client coordinates
    virtual void DoScreenToClient(int *x, int *y) const;
    // Capture/release mouse
    virtual void DoCaptureMouse();
    virtual void DoReleaseMouse();
    // Get window position, relative to parent (or screen if no parent)
    virtual void DoGetPosition(int *x, int *y) const;
    // Get overall window size
    virtual void DoGetSize(int *width, int *height) const;
    // Get/set client (application-useable) size
    virtual void DoGetClientSize(int *width, int *height) const;
    virtual void DoSetClientSize(int width, int size);
    // Set this window's tooltip
    virtual void DoSetToolTip( wxToolTip *tip );
    // Set the size of the wxWindow (the contentView of an NSWindow)
    // wxTopLevelWindow will override this and set the NSWindow size
    // such that the contentView will be this size
    virtual void CocoaSetWxWindowSize(int width, int height);
    // Set overall size and position
    virtual void DoSetSize(int x, int y, int width, int height, int sizeFlags = wxSIZE_AUTO);
    virtual void DoMoveWindow(int x, int y, int width, int height);
    // Popup a menu
    virtual bool DoPopupMenu(wxMenu *menu, int x, int y);

    /* Other implementation */

    // NOTE: typically Close() is not virtual, but we want this for Cocoa
    virtual bool Close( bool force = false );
    virtual bool Show( bool show = true );
    virtual void DoEnable( bool enable );

    virtual bool IsDoubleBuffered() const { return true; }
};

#endif // __WX_COCOA_WINDOW_H__
