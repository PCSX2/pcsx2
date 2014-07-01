///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/NSMenu.h
// Purpose:     wxCocoaNSMenu class
// Author:      David Elliott
// Modified by:
// Created:     2002/12/09
// Copyright:   (c) 2002 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_NSMENU_H__
#define __WX_COCOA_NSMENU_H__

#include "wx/hashmap.h"
#include "wx/cocoa/ObjcAssociate.h"

WX_DECLARE_OBJC_HASHMAP(NSMenu);

// ========================================================================
// wxCocoaNSMenu
// ========================================================================

class wxCocoaNSMenu
{
    WX_DECLARE_OBJC_INTERFACE_HASHMAP(NSMenu)
public:
    void AssociateNSMenu(WX_NSMenu cocoaNSMenu, unsigned int flags = 0);
    void DisassociateNSMenu(WX_NSMenu cocoaNSMenu);
    enum
    {   OBSERVE_DidAddItem          = 0x01
    ,   OBSERVE_DidChangeItem       = 0x02
    ,   OBSERVE_DidRemoveItem       = 0x04
    ,   OBSERVE_DidSendAction       = 0x08
    ,   OBSERVE_WillSendAction      = 0x10
    };
    virtual void Cocoa_dealloc() {}
    virtual void CocoaNotification_menuDidAddItem(WX_NSNotification WXUNUSED(notification)) {}
    virtual void CocoaNotification_menuDidChangeItem(WX_NSNotification WXUNUSED(notification)) {}
    virtual void CocoaNotification_menuDidRemoveItem(WX_NSNotification WXUNUSED(notification)) {}
    virtual void CocoaNotification_menuDidSendAction(WX_NSNotification WXUNUSED(notification)) {}
    virtual void CocoaNotification_menuWillSendAction(WX_NSNotification WXUNUSED(notification)) {}
    virtual ~wxCocoaNSMenu() { }

protected:
    static struct objc_object *sm_cocoaObserver;
};

#endif // _WX_COCOA_NSMENU_H_
