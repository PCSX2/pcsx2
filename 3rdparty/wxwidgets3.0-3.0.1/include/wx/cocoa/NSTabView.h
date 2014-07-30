///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/NSTabView.h
// Purpose:     wxCocoaNSTabView class
// Author:      David Elliott
// Modified by:
// Created:     2004/04/08
// Copyright:   (c) 2004 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_NSTABVIEW_H__
#define _WX_COCOA_NSTABVIEW_H__

#include "wx/hashmap.h"
#include "wx/cocoa/ObjcAssociate.h"
#include "wx/cocoa/ObjcRef.h"

DECLARE_WXCOCOA_OBJC_CLASS(NSTabView);
DECLARE_WXCOCOA_OBJC_CLASS(NSTabViewItem);
WX_DECLARE_OBJC_HASHMAP(NSTabView);
class wxCocoaNSTabView
{
    WX_DECLARE_OBJC_INTERFACE_HASHMAP(NSTabView)
public:
    void AssociateNSTabView(WX_NSTabView cocoaNSTabView);
    void DisassociateNSTabView(WX_NSTabView ocoaNSTabView);
    virtual void CocoaDelegate_tabView_didSelectTabViewItem(WX_NSTabViewItem tabviewItem) = 0;
    virtual bool CocoaDelegate_tabView_shouldSelectTabViewItem(WX_NSTabViewItem tabviewItem) = 0;
    virtual ~wxCocoaNSTabView() { }

protected:
    static wxObjcAutoRefFromAlloc<struct objc_object*> sm_cocoaDelegate;
};

#endif // _WX_COCOA_NSTABVIEW_H__
