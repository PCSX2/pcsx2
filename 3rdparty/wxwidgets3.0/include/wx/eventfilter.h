///////////////////////////////////////////////////////////////////////////////
// Name:        wx/eventfilter.h
// Purpose:     wxEventFilter class declaration.
// Author:      Vadim Zeitlin
// Created:     2011-11-21
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_EVENTFILTER_H_
#define _WX_EVENTFILTER_H_

#include "wx/defs.h"

class WXDLLIMPEXP_FWD_BASE wxEvent;
class WXDLLIMPEXP_FWD_BASE wxEvtHandler;

// ----------------------------------------------------------------------------
// wxEventFilter is used with wxEvtHandler::AddFilter() and ProcessEvent().
// ----------------------------------------------------------------------------

class wxEventFilter
{
public:
    // Possible return values for FilterEvent().
    //
    // Notice that the values of these enum elements are fixed due to backwards
    // compatibility constraints.
    enum
    {
        // Process event as usual.
        Event_Skip = -1,

        // Don't process the event normally at all.
        Event_Ignore = 0,

        // Event was already handled, don't process it normally.
        Event_Processed = 1
    };

    wxEventFilter()
    {
        m_next = NULL;
    }

    virtual ~wxEventFilter()
    {
        wxASSERT_MSG( !m_next, "Forgot to call wxEvtHandler::RemoveFilter()?" );
    }

    // This method allows to filter all the events processed by the program, so
    // you should try to return quickly from it to avoid slowing down the
    // program to a crawl.
    //
    // Return value should be -1 to continue with the normal event processing,
    // or true or false to stop further processing and pretend that the event
    // had been already processed or won't be processed at all, respectively.
    virtual int FilterEvent(wxEvent& event) = 0;

private:
    // Objects of this class are made to be stored in a linked list in
    // wxEvtHandler so put the next node ponter directly in the class itself.
    wxEventFilter* m_next;

    // And provide access to it for wxEvtHandler [only].
    friend class wxEvtHandler;

    wxDECLARE_NO_COPY_CLASS(wxEventFilter);
};

#endif // _WX_EVENTFILTER_H_
