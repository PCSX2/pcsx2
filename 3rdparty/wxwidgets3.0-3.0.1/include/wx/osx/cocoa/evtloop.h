///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/cocoa/evtloop.h
// Purpose:     declaration of wxGUIEventLoop for wxOSX/Cocoa
// Author:      Vadim Zeitlin
// Created:     2008-12-28
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_COCOA_EVTLOOP_H_
#define _WX_OSX_COCOA_EVTLOOP_H_

class WXDLLIMPEXP_BASE wxGUIEventLoop : public wxCFEventLoop
{
public:
    wxGUIEventLoop();
    ~wxGUIEventLoop();
    
    void BeginModalSession( wxWindow* modalWindow );
    
    void EndModalSession();

    virtual void WakeUp();

    void OSXUseLowLevelWakeup(bool useIt)
        { m_osxLowLevelWakeUp = useIt ; }
    
protected:
    virtual int DoDispatchTimeout(unsigned long timeout);

    virtual void OSXDoRun();
    virtual void OSXDoStop();

    virtual CFRunLoopRef CFGetCurrentRunLoop() const;
    
    void* m_modalSession;
    
    wxWindow* m_modalWindow;
    
    WXWindow m_dummyWindow;
    
    int m_modalNestedLevel;
    
    bool m_osxLowLevelWakeUp;
};

#endif // _WX_OSX_COCOA_EVTLOOP_H_

