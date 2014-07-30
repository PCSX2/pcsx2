///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/NSView.h
// Purpose:     wxCocoaNSView class
// Author:      David Elliott
// Modified by:
// Created:     2003/02/15
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_NSVIEW_H__
#define __WX_COCOA_NSVIEW_H__

#include "wx/hashmap.h"
#include "wx/cocoa/ObjcAssociate.h"

#if defined(__LP64__) || defined(NS_BUILD_32_LIKE_64)
typedef struct CGRect NSRect;
#else
typedef struct _NSRect NSRect;
#endif

struct objc_object;

class wxWindow;

WX_DECLARE_OBJC_HASHMAP(NSView);
class wxCocoaNSView
{
/* NSView is a rather special case and requires some extra attention */
    WX_DECLARE_OBJC_INTERFACE_HASHMAP(NSView)
public:
    void AssociateNSView(WX_NSView cocoaNSView);
    void DisassociateNSView(WX_NSView cocoaNSView);
protected:
    static struct objc_object *sm_cocoaObserver;
public:
    virtual wxWindow* GetWxWindow() const
    {   return NULL; }
    virtual void Cocoa_FrameChanged(void) = 0;
    virtual void Cocoa_synthesizeMouseMoved(void) = 0;
    virtual bool Cocoa_acceptsFirstMouse(bool &WXUNUSED(acceptsFirstMouse), WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_drawRect(const NSRect &WXUNUSED(rect))
    {   return false; }
    virtual bool Cocoa_mouseDown(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_mouseDragged(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_mouseUp(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_mouseMoved(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_mouseEntered(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_mouseExited(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_rightMouseDown(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_rightMouseDragged(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_rightMouseUp(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_otherMouseDown(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_otherMouseDragged(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_otherMouseUp(WX_NSEvent WXUNUSED(theEvent))
    {   return false; }
    virtual bool Cocoa_resetCursorRects()
    {   return false; }
    virtual bool Cocoa_viewDidMoveToWindow()
    {   return false; }
    virtual bool Cocoa_viewWillMoveToWindow(WX_NSWindow WXUNUSED(newWindow))
    {   return false; }
    virtual ~wxCocoaNSView() { }
};

#endif
    // __WX_COCOA_NSVIEW_H__
