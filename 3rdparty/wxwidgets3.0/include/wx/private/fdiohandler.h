///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/fdiohandler.h
// Purpose:     declares wxFDIOHandler class
// Author:      Vadim Zeitlin
// Created:     2009-08-17
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_FDIOHANDLER_H_
#define _WX_PRIVATE_FDIOHANDLER_H_

// ----------------------------------------------------------------------------
// wxFDIOHandler: interface used to process events on file descriptors
// ----------------------------------------------------------------------------

class wxFDIOHandler
{
public:
    wxFDIOHandler() { m_regmask = 0; }

    // called when descriptor is available for non-blocking read
    virtual void OnReadWaiting() = 0;

    // called when descriptor is available  for non-blocking write
    virtual void OnWriteWaiting() = 0;

    // called when there is exception on descriptor
    virtual void OnExceptionWaiting() = 0;

    // called to check if the handler is still valid, only used by
    // wxSocketImplUnix currently
    virtual bool IsOk() const { return true; }


    // get/set the mask of events for which we're currently registered for:
    // it's a combination of wxFDIO_{INPUT,OUTPUT,EXCEPTION}
    int GetRegisteredEvents() const { return m_regmask; }
    void SetRegisteredEvent(int flag) { m_regmask |= flag; }
    void ClearRegisteredEvent(int flag) { m_regmask &= ~flag; }


    // virtual dtor for the base class
    virtual ~wxFDIOHandler() { }

private:
    int m_regmask;

    wxDECLARE_NO_COPY_CLASS(wxFDIOHandler);
};

#endif // _WX_PRIVATE_FDIOHANDLER_H_

