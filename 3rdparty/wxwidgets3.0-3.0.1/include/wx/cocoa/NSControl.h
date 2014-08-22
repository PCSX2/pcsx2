///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/NSControl.h
// Purpose:     wxCocoaNSControl class
// Author:      David Elliott
// Modified by:
// Created:     2003/02/15
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_NSCONTROL_H__
#define __WX_COCOA_NSCONTROL_H__

#include "wx/hashmap.h"
#include "wx/cocoa/ObjcAssociate.h"

WX_DECLARE_OBJC_HASHMAP(NSControl);
class wxCocoaNSControl
{
    WX_DECLARE_OBJC_INTERFACE(NSControl)
public:
    virtual void CocoaTarget_action() {}
//    virtual void Cocoa_didChangeText(void) = 0;

    virtual ~wxCocoaNSControl() { }

protected:
    static struct objc_object *sm_cocoaTarget;
};

#endif // _WX_COCOA_NSCONTROL_H_
