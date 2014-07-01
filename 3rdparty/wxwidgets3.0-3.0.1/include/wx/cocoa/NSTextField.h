///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/NSTextField.h
// Purpose:     wxCocoaNSTextField class
// Author:      David Elliott
// Modified by:
// Created:     2002/12/09
// Copyright:   (c) 2002 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_NSTEXTFIELD_H__
#define __WX_COCOA_NSTEXTFIELD_H__

#include "wx/hashmap.h"
#include "wx/cocoa/ObjcAssociate.h"

WX_DECLARE_OBJC_HASHMAP(NSTextField);
class wxCocoaNSTextField
{
    WX_DECLARE_OBJC_INTERFACE(NSTextField)
protected:
    virtual void Cocoa_didChangeText(void) = 0;
    virtual ~wxCocoaNSTextField() { }
};

#endif // _WX_COCOA_NSTEXTFIELD_H_
