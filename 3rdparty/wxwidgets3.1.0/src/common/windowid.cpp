///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/windowid.cpp
// Purpose:     wxWindowID class - a class for managing window ids
// Author:      Brian Vanderburg II
// Created:     2007-09-21
// Copyright:   (c) 2007 Brian Vanderburg II
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// Needed headers
// ----------------------------------------------------------------------------
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/intl.h"
#endif //WX_PRECOMP

#include "wx/hashmap.h"

// Not needed, included in defs.h
// #include "wx/windowid.h"

#define wxTRACE_WINDOWID wxT("windowid")

namespace
{

#if wxUSE_AUTOID_MANAGEMENT


// initially no ids are in use and we allocate them consecutively, but after we
// exhaust the entire range, we wrap around and reuse the ids freed in the
// meanwhile
static const wxUint8 ID_FREE = 0;
static const wxUint8 ID_STARTCOUNT = 1;
static const wxUint8 ID_COUNTTOOLARGE = 254;
static const wxUint8 ID_RESERVED = 255;

// we use a two level count, most IDs will be used less than ID_COUNTTOOLARGE-1
// thus we store their count directly in this array, however when the same ID
// is reused a great number of times (more than or equal to ID_COUNTTOOLARGE),
// the hash map stores the actual count
wxUint8 gs_autoIdsRefCount[wxID_AUTO_HIGHEST - wxID_AUTO_LOWEST + 1] = { 0 };

// NB: this variable is allocated (again) only when an ID gets at least
// ID_COUNTTOOLARGE refs, and is freed when the latest entry in the map gets
// freed. The cell storing the count for an ID is freed only when its count
// gets to zero (not when it goes below ID_COUNTTOOLARGE, so as to avoid
// degenerate cases)
wxLongToLongHashMap *gs_autoIdsLargeRefCount = NULL;

// this is an optimization used until we wrap around wxID_AUTO_HIGHEST: if this
// value is < wxID_AUTO_HIGHEST we know that we haven't wrapped yet and so can
// allocate the ids simply by incrementing it
wxWindowID gs_nextAutoId = wxID_AUTO_LOWEST;

// Reserve an ID
void ReserveIdRefCount(wxWindowID winid)
{
    wxCHECK_RET(winid >= wxID_AUTO_LOWEST && winid <= wxID_AUTO_HIGHEST,
            wxT("invalid id range"));

    winid -= wxID_AUTO_LOWEST;

    wxCHECK_RET(gs_autoIdsRefCount[winid] == ID_FREE,
            wxT("id already in use or already reserved"));
    gs_autoIdsRefCount[winid] = ID_RESERVED;
}

// Unreserve and id
void UnreserveIdRefCount(wxWindowID winid)
{
    wxCHECK_RET(winid >= wxID_AUTO_LOWEST && winid <= wxID_AUTO_HIGHEST,
            wxT("invalid id range"));

    winid -= wxID_AUTO_LOWEST;

    wxCHECK_RET(gs_autoIdsRefCount[winid] == ID_RESERVED,
            wxT("id already in use or not reserved"));
    gs_autoIdsRefCount[winid] = ID_FREE;
}

// Get the usage count of an id
int GetIdRefCount(wxWindowID winid)
{
    wxCHECK_MSG(winid >= wxID_AUTO_LOWEST && winid <= wxID_AUTO_HIGHEST, 0,
            wxT("invalid id range"));

    winid -= wxID_AUTO_LOWEST;
    int refCount = gs_autoIdsRefCount[winid];
    if (refCount == ID_COUNTTOOLARGE)
        refCount = (*gs_autoIdsLargeRefCount)[winid];
    return refCount;
}

// Increase the count for an id
void IncIdRefCount(wxWindowID winid)
{
    wxCHECK_RET(winid >= wxID_AUTO_LOWEST && winid <= wxID_AUTO_HIGHEST,
            wxT("invalid id range"));

    winid -= wxID_AUTO_LOWEST;

    wxCHECK_RET(gs_autoIdsRefCount[winid] != ID_FREE, wxT("id should first be reserved"));

    if(gs_autoIdsRefCount[winid] == ID_RESERVED)
    {
        gs_autoIdsRefCount[winid] = ID_STARTCOUNT;
    }
    else if (gs_autoIdsRefCount[winid] >= ID_COUNTTOOLARGE-1)
    {
        if (gs_autoIdsRefCount[winid] == ID_COUNTTOOLARGE-1)
        {
            // we need to allocate a cell, and maybe the hash map itself
            if (!gs_autoIdsLargeRefCount)
                gs_autoIdsLargeRefCount = new wxLongToLongHashMap;
            (*gs_autoIdsLargeRefCount)[winid] = ID_COUNTTOOLARGE-1;

            gs_autoIdsRefCount[winid] = ID_COUNTTOOLARGE;
        }
        ++(*gs_autoIdsLargeRefCount)[winid];
    }
    else
    {
        gs_autoIdsRefCount[winid]++;
    }

    wxLogTrace(wxTRACE_WINDOWID, wxT("Increasing ref count of ID %d to %d"),
            winid + wxID_AUTO_LOWEST, GetIdRefCount(winid + wxID_AUTO_LOWEST));
}

// Decrease the count for an id
void DecIdRefCount(wxWindowID winid)
{
    wxCHECK_RET(winid >= wxID_AUTO_LOWEST && winid <= wxID_AUTO_HIGHEST,
            wxT("invalid id range"));

    winid -= wxID_AUTO_LOWEST;

    wxCHECK_RET(gs_autoIdsRefCount[winid] != ID_FREE, wxT("id count already 0"));

    // DecIdRefCount is only called on an ID that has been IncIdRefCount'ed'
    // so it should never be reserved, but test anyway
    if(gs_autoIdsRefCount[winid] == ID_RESERVED)
    {
        wxFAIL_MSG(wxT("reserve id being decreased"));
        gs_autoIdsRefCount[winid] = ID_FREE;
    }
    else if(gs_autoIdsRefCount[winid] == ID_COUNTTOOLARGE)
    {
        long &largeCount = (*gs_autoIdsLargeRefCount)[winid];
        --largeCount;
        if (largeCount == 0)
        {
            gs_autoIdsLargeRefCount->erase (winid);
            gs_autoIdsRefCount[winid] = ID_FREE;

            if (gs_autoIdsLargeRefCount->empty())
                wxDELETE (gs_autoIdsLargeRefCount);
        }
    }
    else
        gs_autoIdsRefCount[winid]--;

    wxLogTrace(wxTRACE_WINDOWID, wxT("Decreasing ref count of ID %d to %d"),
            winid + wxID_AUTO_LOWEST, GetIdRefCount(winid + wxID_AUTO_LOWEST));
}

#else // wxUSE_AUTOID_MANAGEMENT

static wxWindowID gs_nextAutoId = wxID_AUTO_HIGHEST;

#endif

} // anonymous namespace


#if wxUSE_AUTOID_MANAGEMENT

void wxWindowIDRef::Assign(wxWindowID winid)
{
    if ( winid != m_id )
    {
        // decrease count if it is in the managed range
        if ( m_id >= wxID_AUTO_LOWEST && m_id <= wxID_AUTO_HIGHEST )
            DecIdRefCount(m_id);

        m_id = winid;

        // increase count if it is in the managed range
        if ( m_id >= wxID_AUTO_LOWEST && m_id <= wxID_AUTO_HIGHEST )
            IncIdRefCount(m_id);
    }
}

#endif // wxUSE_AUTOID_MANAGEMENT



wxWindowID wxIdManager::ReserveId(int count)
{
    wxASSERT_MSG(count > 0, wxT("can't allocate less than 1 id"));


#if wxUSE_AUTOID_MANAGEMENT
    if ( gs_nextAutoId + count - 1 <= wxID_AUTO_HIGHEST )
    {
        wxWindowID winid = gs_nextAutoId;

        while(count--)
        {
            ReserveIdRefCount(gs_nextAutoId++);
        }

        return winid;
    }
    else
    {
        int found = 0;

        for(wxWindowID winid = wxID_AUTO_LOWEST; winid <= wxID_AUTO_HIGHEST; winid++)
        {
            if(GetIdRefCount(winid) == 0)
            {
                found++;
                if(found == count)
                {
                    // Imagine this:  100 free IDs left.  Then NewId(50) takes 50
                    // so 50 left.  Then, the 25 before that last 50 are freed, but
                    // gs_nextAutoId does not decrement and stays where it is at
                    // with 50 free. Then NewId(75) gets called, and since there
                    // are only 50 left according to gs_nextAutoId, it does a
                    // search and finds the 75 at the end.  Then NewId(10) gets
                    // called, and accorind to gs_nextAutoId, their are still
                    // 50 at the end so it returns them without testing the ref
                    // To fix this, the next ID is also updated here as needed
                    if(winid >= gs_nextAutoId)
                        gs_nextAutoId = winid + 1;

                    while(count--)
                        ReserveIdRefCount(winid--);

                    return winid + 1;
                }
            }
            else
            {
                found = 0;
            }
        }
    }

    wxLogError(_("Out of window IDs.  Recommend shutting down application."));
    return wxID_NONE;
#else // !wxUSE_AUTOID_MANAGEMENT
    // Make sure enough in the range
    wxWindowID winid;

    winid = gs_nextAutoId - count + 1;

    if ( winid >= wxID_AUTO_LOWEST && winid <= wxID_AUTO_HIGHEST )
    {
        // There is enough, but it may be time to wrap
        if(winid == wxID_AUTO_LOWEST)
            gs_nextAutoId = wxID_AUTO_HIGHEST;
        else
            gs_nextAutoId = winid - 1;

        return winid;
    }
    else
    {
        // There is not enough at the low end of the range or
        // count was big enough to wrap around to the positive
        // Surely 'count' is not so big to take up much of the range
        gs_nextAutoId = wxID_AUTO_HIGHEST - count;
        return gs_nextAutoId + 1;
    }
#endif // wxUSE_AUTOID_MANAGEMENT/!wxUSE_AUTOID_MANAGEMENT
}

void wxIdManager::UnreserveId(wxWindowID winid, int count)
{
    wxASSERT_MSG(count > 0, wxT("can't unreserve less than 1 id"));

#if wxUSE_AUTOID_MANAGEMENT
    while (count--)
        UnreserveIdRefCount(winid++);
#else
    wxUnusedVar(winid);
    wxUnusedVar(count);
#endif
}

