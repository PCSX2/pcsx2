///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/NSTableView.h
// Purpose:     wxCocoaNSTableView class
// Author:      David Elliott
// Modified by:
// Created:     2003/08/05
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_NSTABLEVIEW_H__
#define __WX_COCOA_NSTABLEVIEW_H__

#include "wx/hashmap.h"
#include "wx/cocoa/ObjcAssociate.h"

WX_DECLARE_OBJC_HASHMAP(NSTableView);

class wxCocoaNSTableView
{
    WX_DECLARE_OBJC_INTERFACE(NSTableView)
public:
    virtual int CocoaDataSource_numberOfRows() = 0;
    virtual struct objc_object* CocoaDataSource_objectForTableColumn(
        WX_NSTableColumn tableColumn, int rowIndex) = 0;
    virtual ~wxCocoaNSTableView() { }
};

#endif // _WX_COCOA_NSTABLEVIEW_H_
