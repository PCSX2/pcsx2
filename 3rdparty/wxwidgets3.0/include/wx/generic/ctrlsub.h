///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/ctrlsub.h
// Purpose:     common functionality of wxItemContainer-derived controls
// Author:      Vadim Zeitlin
// Created:     2007-07-25
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_CTRLSUB_H_
#define _WX_GENERIC_CTRLSUB_H_

#include "wx/dynarray.h"

// ----------------------------------------------------------------------------
// wxControlWithItemsGeneric: generic implementation of item client data
// ----------------------------------------------------------------------------

class wxControlWithItemsGeneric : public wxControlWithItemsBase
{
public:
    wxControlWithItemsGeneric() { }

    virtual void DoInitItemClientData()
    {
        m_itemsClientData.resize(GetCount(), NULL);
    }

    virtual void DoSetItemClientData(unsigned int n, void *clientData)
    {
        m_itemsClientData[n] = clientData;
    }

    virtual void *DoGetItemClientData(unsigned int n) const
    {
        return m_itemsClientData[n];
    }

    virtual void DoClear() { m_itemsClientData.clear(); }
    virtual void DoDeleteOneItem(unsigned int pos)
    {
        if ( HasClientData() )
            m_itemsClientData.RemoveAt(pos);
    }

protected:
    // preallocate memory for numItems new items: this should be called from
    // the derived classes DoInsertItems() to speed up appending big numbers of
    // items with client data; it is safe to call even if we don't use client
    // data at all and does nothing in this case
    void AllocClientData(unsigned int numItems)
    {
        if ( HasClientData() )
            m_itemsClientData.reserve(m_itemsClientData.size() + numItems);
    }

    // this must be called by derived classes when a new item is added to the
    // control to add storage for the corresponding client data pointer (before
    // inserting many items, call AllocClientData())
    void InsertNewItemClientData(unsigned int pos,
                                 void **clientData,
                                 unsigned int n,
                                 wxClientDataType type)
    {
        if ( InitClientDataIfNeeded(type) )
            m_itemsClientData.Insert(clientData[n], pos);
    }

    // the same as InsertNewItemClientData() but for numItems consecutive items
    // (this can only be used if the control doesn't support sorting as
    // otherwise the items positions wouldn't be consecutive any more)
    void InsertNewItemsClientData(unsigned int pos,
                                  unsigned int numItems,
                                  void **clientData,
                                  wxClientDataType type)
    {
        if ( InitClientDataIfNeeded(type) )
        {
            // it's more efficient to insert everything at once and then update
            // for big number of items to avoid moving the array contents
            // around (which would result in O(N^2) algorithm)
            m_itemsClientData.Insert(NULL, pos, numItems);

            for ( unsigned int n = 0; n < numItems; ++n, ++pos )
                m_itemsClientData[pos] = clientData[n];
        }
    }


    // vector containing the client data pointers: it is either empty (if
    // client data is not used) or has the same number of elements as the
    // control
    wxArrayPtrVoid m_itemsClientData;

private:
    // initialize client data if needed, return false if we don't have any
    // client data and true otherwise
    bool InitClientDataIfNeeded(wxClientDataType type)
    {
        if ( !HasClientData() )
        {
            if ( type == wxClientData_None )
            {
                // we didn't have the client data before and are not asked to
                // store it now neither
                return false;
            }

            // this is the first time client data is used with this control
            DoInitItemClientData();
            SetClientDataType(type);
        }
        //else: we already have client data

        return true;
    }

    wxDECLARE_NO_COPY_CLASS(wxControlWithItemsGeneric);
};

#endif // _WX_GENERIC_CTRLSUB_H_

