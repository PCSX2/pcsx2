/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xtihandler.h
// Purpose:     XTI handlers
// Author:      Stefan Csomor
// Modified by: Francesco Montorsi
// Created:     27/07/03
// Copyright:   (c) 1997 Julian Smart
//              (c) 2003 Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _XTIHANDLER_H_
#define _XTIHANDLER_H_

#include "wx/defs.h"

#if wxUSE_EXTENDED_RTTI

#include "wx/xti.h"

// copied from event.h which cannot be included at this place

class WXDLLIMPEXP_FWD_BASE wxEvent;

#ifdef __VISUALC__
#define wxMSVC_FWD_MULTIPLE_BASES __multiple_inheritance
#else
#define wxMSVC_FWD_MULTIPLE_BASES
#endif

class WXDLLIMPEXP_FWD_BASE wxMSVC_FWD_MULTIPLE_BASES wxEvtHandler;
typedef void (wxEvtHandler::*wxEventFunction)(wxEvent&);
typedef wxEventFunction wxObjectEventFunction;

// ----------------------------------------------------------------------------
// Handler Info
//
// this describes an event sink
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxHandlerInfo
{
    friend class WXDLLIMPEXP_BASE wxDynamicClassInfo;

public:
    wxHandlerInfo(wxHandlerInfo* &iter,
                  wxClassInfo* itsClass,
                  const wxString& name,
                  wxObjectEventFunction address,
                  const wxClassInfo* eventClassInfo) :
            m_eventFunction(address),
            m_name(name),
            m_eventClassInfo(eventClassInfo),
            m_itsClass(itsClass)
       {
            Insert(iter);
       }

    ~wxHandlerInfo()
        { Remove(); }

    // return the name of this handler
    const wxString& GetName() const { return m_name; }

    // return the class info of the event
    const wxClassInfo *GetEventClassInfo() const { return m_eventClassInfo; }

    // get the handler function pointer
    wxObjectEventFunction GetEventFunction() const { return m_eventFunction; }

    // returns NULL if this is the last handler of this class
    wxHandlerInfo*     GetNext() const { return m_next; }

    // return the class this property is declared in
    const wxClassInfo*   GetDeclaringClass() const { return m_itsClass; }

private:

    // inserts this handler at the end of the linked chain which begins
    // with "iter" handler.
    void Insert(wxHandlerInfo* &iter);

    // removes this handler from the linked chain of the m_itsClass handlers.
    void Remove();

    wxObjectEventFunction m_eventFunction;
    wxString              m_name;
    const wxClassInfo*    m_eventClassInfo;
    wxHandlerInfo*        m_next;
    wxClassInfo*          m_itsClass;
};

#define wxHANDLER(name,eventClassType)                                               \
    static wxHandlerInfo _handlerInfo##name( first, class_t::GetClassInfoStatic(),   \
                    wxT(#name), (wxObjectEventFunction) (wxEventFunction) &name,     \
                    wxCLASSINFO( eventClassType ) );

#define wxBEGIN_HANDLERS_TABLE(theClass)          \
    wxHandlerInfo *theClass::GetHandlersStatic()  \
    {                                             \
        typedef theClass class_t;                 \
        static wxHandlerInfo* first = NULL;

#define wxEND_HANDLERS_TABLE()                    \
    return first; }

#define wxEMPTY_HANDLERS_TABLE(theClass)          \
    wxBEGIN_HANDLERS_TABLE(theClass)              \
    wxEND_HANDLERS_TABLE()

#endif      // wxUSE_EXTENDED_RTTI
#endif      // _XTIHANDLER_H_
