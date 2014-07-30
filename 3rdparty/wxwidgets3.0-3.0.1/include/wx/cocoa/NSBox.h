///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/NSBox.h
// Purpose:     wxCocoaNSBox class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/19
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_NSBOX_H__
#define __WX_COCOA_NSBOX_H__

#include "wx/hashmap.h"
#include "wx/cocoa/ObjcAssociate.h"

WX_DECLARE_OBJC_HASHMAP(NSBox);
class wxCocoaNSBox
{
    WX_DECLARE_OBJC_INTERFACE(NSBox)
protected:
//    virtual void Cocoa_didChangeText(void) = 0;
};

#endif // _WX_COCOA_NSBOX_H_
