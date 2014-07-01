/////////////////////////////////////////////////////////////////////////////
// Name:        wx/tracker.h
// Purpose:     Support class for object lifetime tracking (wxWeakRef<T>)
// Author:      Arne Steinarson
// Created:     28 Dec 07
// Copyright:   (c) 2007 Arne Steinarson
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TRACKER_H_
#define _WX_TRACKER_H_

#include "wx/defs.h"

class wxEventConnectionRef;

// This class represents an object tracker and is stored in a linked list
// in the tracked object. It is only used in one of its derived forms.
class WXDLLIMPEXP_BASE wxTrackerNode
{
public:
    wxTrackerNode() : m_nxt(NULL) { }
    virtual ~wxTrackerNode() { }

    virtual void OnObjectDestroy() = 0;

    virtual wxEventConnectionRef *ToEventConnection() { return NULL; }

private:
    wxTrackerNode *m_nxt;

    friend class wxTrackable;        // For list access
    friend class wxEvtHandler;       // For list access
};

// Add-on base class for a trackable object.
class WXDLLIMPEXP_BASE wxTrackable
{
public:
    void AddNode(wxTrackerNode *prn)
    {
        prn->m_nxt = m_first;
        m_first = prn;
    }

    void RemoveNode(wxTrackerNode *prn)
    {
        for ( wxTrackerNode **pprn = &m_first; *pprn; pprn = &(*pprn)->m_nxt )
        {
            if ( *pprn == prn )
            {
                *pprn = prn->m_nxt;
                return;
            }
        }

        wxFAIL_MSG( "removing invalid tracker node" );
    }

    wxTrackerNode *GetFirst() const { return m_first; }

protected:
    // this class is only supposed to be used as a base class but never be
    // created nor destroyed directly so all ctors and dtor are protected

    wxTrackable() : m_first(NULL) { }

    // copy ctor and assignment operator intentionally do not copy m_first: the
    // objects which track the original trackable shouldn't track the new copy
    wxTrackable(const wxTrackable& WXUNUSED(other)) : m_first(NULL) { }
    wxTrackable& operator=(const wxTrackable& WXUNUSED(other)) { return *this; }

    // dtor is not virtual: this class is not supposed to be used
    // polymorphically and adding a virtual table to it would add unwanted
    // overhead
    ~wxTrackable()
    {
        // Notify all registered refs
        while ( m_first )
        {
            wxTrackerNode * const first = m_first;
            m_first = first->m_nxt;
            first->OnObjectDestroy();
        }
    }

    wxTrackerNode *m_first;
};

#endif // _WX_TRACKER_H_

