///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msdos/apptrait.h
// Author:      Michael Wetherell
// Copyright:   (c) 2006 Michael Wetherell
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSDOS_APPTRAIT_H_
#define _WX_MSDOS_APPTRAIT_H_

class wxConsoleAppTraits : public wxConsoleAppTraitsBase
{
public:
    virtual wxEventLoopBase *CreateEventLoop() { return NULL; }
#if wxUSE_TIMER
    virtual wxTimerImpl *CreateTimerImpl(wxTimer *) { return NULL; }
#endif // wxUSE_TIMER
};

#if wxUSE_GUI

class wxGUIAppTraits : public wxGUIAppTraitsBase
{
public:
    virtual wxEventLoopBase *CreateEventLoop();
    virtual wxPortId GetToolkitVersion(int *majVer = NULL, int *minVer = NULL) const;

#if wxUSE_TIMER
    virtual wxTimerImpl *CreateTimerImpl(wxTimer *timer);
#endif // wxUSE_TIMER
};

#endif // wxUSE_GUI

#endif // _WX_MSDOS_APPTRAIT_H_
