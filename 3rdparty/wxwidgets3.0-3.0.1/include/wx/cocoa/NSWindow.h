///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/NSWindow.h
// Purpose:     wxCocoaNSWindow class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/16
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_NSWINDOW_H__
#define __WX_COCOA_NSWINDOW_H__

#include "wx/hashmap.h"
#include "wx/cocoa/ObjcAssociate.h"

WX_DECLARE_OBJC_HASHMAP(NSWindow);

class WXDLLIMPEXP_FWD_CORE wxMenuBar;
class WXDLLIMPEXP_FWD_CORE wxTopLevelWindowCocoa;

DECLARE_WXCOCOA_OBJC_CLASS(NSMenuItem);
DECLARE_WXCOCOA_OBJC_CLASS(wxNSWindowDelegate);

class WXDLLIMPEXP_CORE wxCocoaNSWindow
{
/* NSWindow is a rather special case and requires some extra attention */
    WX_DECLARE_OBJC_INTERFACE_HASHMAP(NSWindow)
public:
    void AssociateNSWindow(WX_NSWindow cocoaNSWindow);
    void DisassociateNSWindow(WX_NSWindow cocoaNSWindow);
    virtual bool Cocoa_canBecomeKeyWindow(bool &WXUNUSED(canBecome))
    {   return false; }
    virtual bool Cocoa_canBecomeMainWindow(bool &WXUNUSED(canBecome))
    {   return false; }
    virtual bool CocoaDelegate_windowShouldClose(void) = 0;
    virtual void CocoaDelegate_windowWillClose(void) = 0;
    virtual void CocoaDelegate_windowDidBecomeKey(void) { }
    virtual void CocoaDelegate_windowDidResignKey(void) { }
    virtual void CocoaDelegate_windowDidBecomeMain(void) { }
    virtual void CocoaDelegate_windowDidResignMain(void) { }
    virtual void CocoaDelegate_wxMenuItemAction(WX_NSMenuItem menuItem) = 0;
    virtual bool CocoaDelegate_validateMenuItem(WX_NSMenuItem menuItem) = 0;
    virtual wxMenuBar* GetAppMenuBar(wxCocoaNSWindow *win);
    inline wxTopLevelWindowCocoa* GetWxTopLevelWindowCocoa()
    {   return m_wxTopLevelWindowCocoa; }
protected:
    wxCocoaNSWindow(wxTopLevelWindowCocoa *tlw = NULL);
    virtual ~wxCocoaNSWindow();
    WX_wxNSWindowDelegate m_cocoaDelegate;
    wxTopLevelWindowCocoa *m_wxTopLevelWindowCocoa;
};

#endif // _WX_COCOA_NSWINDOW_H_
