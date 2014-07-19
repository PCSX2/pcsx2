///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/objc/NSView.h
// Purpose:     WXNSView class
// Author:      David Elliott
// Modified by:
// Created:     2007/04/20 (move from NSView.mm)
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_OBJC_NSVIEW_H__
#define __WX_COCOA_OBJC_NSVIEW_H__

#include "wx/cocoa/objc/objc_uniquifying.h"

#import <AppKit/NSView.h>

// ============================================================================
// @class WXNSView
// ============================================================================
@interface WXNSView : NSView
{
}

- (void)drawRect: (NSRect)rect;
- (void)mouseDown:(NSEvent *)theEvent;
- (void)mouseDragged:(NSEvent *)theEvent;
- (void)mouseUp:(NSEvent *)theEvent;
- (void)mouseMoved:(NSEvent *)theEvent;
- (void)mouseEntered:(NSEvent *)theEvent;
- (void)mouseExited:(NSEvent *)theEvent;
- (void)rightMouseDown:(NSEvent *)theEvent;
- (void)rightMouseDragged:(NSEvent *)theEvent;
- (void)rightMouseUp:(NSEvent *)theEvent;
- (void)otherMouseDown:(NSEvent *)theEvent;
- (void)otherMouseDragged:(NSEvent *)theEvent;
- (void)otherMouseUp:(NSEvent *)theEvent;
- (void)resetCursorRects;
- (void)viewDidMoveToWindow;
- (void)viewWillMoveToWindow:(NSWindow *)newWindow;
@end // WXNSView
WX_DECLARE_GET_OBJC_CLASS(WXNSView,NSView)

#endif //ndef __WX_COCOA_OBJC_NSVIEW_H__
