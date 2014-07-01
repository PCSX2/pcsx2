///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/NSApplication.h
// Purpose:     wxNSApplicationDelegate definition
// Author:      David Elliott
// Modified by:
// Created:     2004/01/26
// Copyright:   (c) 2003,2004 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_NSAPPLICATION_H__
#define _WX_COCOA_NSAPPLICATION_H__

#include "wx/cocoa/objc/objc_uniquifying.h"

// ========================================================================
// wxNSApplicationDelegate
// ========================================================================
/*!
    @class wxNSApplicationDelegate
    @discussion Implements an NSApplication delegate which can respond to messages sent by Cocoa to change Cocoa's behaviour.

    wxCocoa will set a singleton instance of this class as the NSApplication delegate upon startup unless wxWidgets is running
    in a "plugin" manner in which case it would not be appropriate to do this.

    Although Cocoa will send notifications to the delegate it is also possible to register a different object to listen for
    them.  Because we want to support the plugin case, we use a separate notification observer object when we can.
*/
@interface wxNSApplicationDelegate : NSObject
{
}

// Delegate methods
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication;
@end // interface wxNSApplicationDelegate : NSObject
WX_DECLARE_GET_OBJC_CLASS(wxNSApplicationDelegate,NSObject)

// ========================================================================
// wxNSApplicationObserver
// ========================================================================
/*!
    @class wxNSApplicationObserver
    @discussion Observes most notifications sent by the NSApplication singleton.

    wxCocoa will create a singleton instance of this class upon startup and register it with the default notification center to
    listen for several events sent by the NSApplication singleton.

    Because there can be any number of notification observers, this method allows wxCocoa to function properly even when it is
    running as a plugin of some other (most likely not wxWidgets) application.
*/
@interface wxNSApplicationObserver : NSObject
{
}

// Methods defined as (but not used here) as NSApplication delegate methods.
- (void)applicationWillBecomeActive:(NSNotification *)notification;
- (void)applicationDidBecomeActive:(NSNotification *)notification;
- (void)applicationWillResignActive:(NSNotification *)notification;
- (void)applicationDidResignActive:(NSNotification *)notification;
- (void)applicationWillUpdate:(NSNotification *)notification;

// Other notifications
- (void)controlTintChanged:(NSNotification *)notification;
@end // interface wxNSApplicationObserver : NSObject
WX_DECLARE_GET_OBJC_CLASS(wxNSApplicationObserver,NSObject)

#endif //ndef _WX_COCOA_NSAPPLICATION_H__
