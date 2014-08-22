/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/trackingrectmanager.h
// Purpose:     wxCocoaTrackingRectManager
// Notes:       Source in window.mm
// Author:      David Elliott <dfe@cox.net>
// Modified by:
// Created:     2007/05/02
// Copyright:   (c) 2007 Software 2000 Ltd.
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef __WX_COCOA_TRACKINGRECTMANAGER_H__
#define __WX_COCOA_TRACKINGRECTMANAGER_H__

#include <CoreFoundation/CFRunLoop.h>

#define wxTRACE_COCOA_TrackingRect wxT("COCOA_TrackingRect")

class wxCocoaTrackingRectManager
{
    wxDECLARE_NO_COPY_CLASS(wxCocoaTrackingRectManager);
public:
    wxCocoaTrackingRectManager(wxWindow *window);
    void ClearTrackingRect();
    void BuildTrackingRect();
    void RebuildTrackingRectIfNeeded();
    void RebuildTrackingRect();
    bool IsOwnerOfEvent(NSEvent *anEvent);
    ~wxCocoaTrackingRectManager();
    void BeginSynthesizingEvents();
    void StopSynthesizingEvents();
protected:
    wxWindow *m_window;
    bool m_isTrackingRectActive;
    NSInteger m_trackingRectTag;
    NSRect m_trackingRectInWindowCoordinates;
private:
    wxCocoaTrackingRectManager();
};

#endif // ndef __WX_COCOA_TRACKINGRECTMANAGER_H__
