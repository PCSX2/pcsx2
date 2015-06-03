///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/ctrlsub.cpp
// Purpose:     wxItemContainer implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     22.10.99
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_CONTROLS

#ifndef WX_PRECOMP
    #include "wx/ctrlsub.h"
    #include "wx/arrstr.h"
#endif

wxIMPLEMENT_ABSTRACT_CLASS(wxControlWithItems, wxControl);

// ============================================================================
// wxItemContainerImmutable implementation
// ============================================================================

wxItemContainerImmutable::~wxItemContainerImmutable()
{
    // this destructor is required for Darwin
}

// ----------------------------------------------------------------------------
// selection
// ----------------------------------------------------------------------------

wxString wxItemContainerImmutable::GetStringSelection() const
{
    wxString s;

    int sel = GetSelection();
    if ( sel != wxNOT_FOUND )
        s = GetString((unsigned int)sel);

    return s;
}

bool wxItemContainerImmutable::SetStringSelection(const wxString& s)
{
    const int sel = FindString(s);
    if ( sel == wxNOT_FOUND )
        return false;

    SetSelection(sel);

    return true;
}

wxArrayString wxItemContainerImmutable::GetStrings() const
{
    wxArrayString result;

    const unsigned int count = GetCount();
    result.Alloc(count);
    for ( unsigned int n = 0; n < count; n++ )
        result.Add(GetString(n));

    return result;
}

// ============================================================================
// wxItemContainer implementation
// ============================================================================

wxItemContainer::~wxItemContainer()
{
    // this destructor is required for Darwin
}

// ----------------------------------------------------------------------------
// deleting items
// ----------------------------------------------------------------------------

void wxItemContainer::Clear()
{
    if ( HasClientObjectData() )
    {
        const unsigned count = GetCount();
        for ( unsigned i = 0; i < count; ++i )
            ResetItemClientObject(i);
    }

    SetClientDataType(wxClientData_None);

    DoClear();
}

void wxItemContainer::Delete(unsigned int pos)
{
    wxCHECK_RET( pos < GetCount(), wxT("invalid index") );

    if ( HasClientObjectData() )
        ResetItemClientObject(pos);

    DoDeleteOneItem(pos);

    if ( IsEmpty() )
    {
        SetClientDataType(wxClientData_None);
    }
}

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

int wxItemContainer::DoInsertItemsInLoop(const wxArrayStringsAdapter& items,
                                         unsigned int pos,
                                         void **clientData,
                                         wxClientDataType type)
{
    int n = wxNOT_FOUND;

    const unsigned int count = items.GetCount();
    for ( unsigned int i = 0; i < count; ++i )
    {
        n = DoInsertOneItem(items[i], pos++);
        if ( n == wxNOT_FOUND )
            break;

        AssignNewItemClientData(n, clientData, i, type);
    }

    return n;
}

int
wxItemContainer::DoInsertOneItem(const wxString& WXUNUSED(item),
                                 unsigned int WXUNUSED(pos))
{
    wxFAIL_MSG( wxT("Must be overridden if DoInsertItemsInLoop() is used") );

    return wxNOT_FOUND;
}


// ----------------------------------------------------------------------------
// client data
// ----------------------------------------------------------------------------

void wxItemContainer::SetClientObject(unsigned int n, wxClientData *data)
{
    wxASSERT_MSG( !HasClientUntypedData(),
                  wxT("can't have both object and void client data") );

    wxCHECK_RET( IsValid(n), "Invalid index passed to SetClientObject()" );

    if ( HasClientObjectData() )
    {
        wxClientData * clientDataOld
            = static_cast<wxClientData *>(DoGetItemClientData(n));
        if ( clientDataOld )
            delete clientDataOld;
    }
    else // didn't have any client data so far
    {
        // now we have object client data
        DoInitItemClientData();

        SetClientDataType(wxClientData_Object);
    }

    DoSetItemClientData(n, data);
}

wxClientData *wxItemContainer::GetClientObject(unsigned int n) const
{
    wxCHECK_MSG( HasClientObjectData(), NULL,
                  wxT("this window doesn't have object client data") );

    wxCHECK_MSG( IsValid(n), NULL,
                 "Invalid index passed to GetClientObject()" );

    return static_cast<wxClientData *>(DoGetItemClientData(n));
}

wxClientData *wxItemContainer::DetachClientObject(unsigned int n)
{
    wxClientData * const data = GetClientObject(n);
    if ( data )
    {
        // reset the pointer as we don't own it any more
        DoSetItemClientData(n, NULL);
    }

    return data;
}

void wxItemContainer::SetClientData(unsigned int n, void *data)
{
    if ( !HasClientData() )
    {
        DoInitItemClientData();
        SetClientDataType(wxClientData_Void);
    }

    wxASSERT_MSG( HasClientUntypedData(),
                  wxT("can't have both object and void client data") );

    wxCHECK_RET( IsValid(n), "Invalid index passed to SetClientData()" );

    DoSetItemClientData(n, data);
}

void *wxItemContainer::GetClientData(unsigned int n) const
{
    wxCHECK_MSG( HasClientUntypedData(), NULL,
                  wxT("this window doesn't have void client data") );

    wxCHECK_MSG( IsValid(n), NULL,
                 "Invalid index passed to GetClientData()" );

    return DoGetItemClientData(n);
}

void wxItemContainer::AssignNewItemClientData(unsigned int pos,
                                              void **clientData,
                                              unsigned int n,
                                              wxClientDataType type)
{
    switch ( type )
    {
        case wxClientData_Object:
            SetClientObject
            (
                pos,
                (reinterpret_cast<wxClientData **>(clientData))[n]
            );
            break;

        case wxClientData_Void:
            SetClientData(pos, clientData[n]);
            break;

        default:
            wxFAIL_MSG( wxT("unknown client data type") );
            wxFALLTHROUGH;

        case wxClientData_None:
            // nothing to do
            break;
    }
}

void wxItemContainer::ResetItemClientObject(unsigned int n)
{
    wxClientData * const data = GetClientObject(n);
    if ( data )
    {
        delete data;
        DoSetItemClientData(n, NULL);
    }
}

// ============================================================================
// wxControlWithItems implementation
// ============================================================================

void
wxControlWithItemsBase::InitCommandEventWithItems(wxCommandEvent& event, int n)
{
    InitCommandEvent(event);

    if ( n != wxNOT_FOUND )
    {
        if ( HasClientObjectData() )
            event.SetClientObject(GetClientObject(n));
        else if ( HasClientUntypedData() )
            event.SetClientData(GetClientData(n));
    }
}

void wxControlWithItemsBase::SendSelectionChangedEvent(wxEventType eventType)
{
    const int n = GetSelection();
    if ( n == wxNOT_FOUND )
        return;

    wxCommandEvent event(eventType, m_windowId);
    event.SetInt(n);
    event.SetEventObject(this);
    event.SetString(GetStringSelection());
    InitCommandEventWithItems(event, n);

    HandleWindowEvent(event);
}

#endif // wxUSE_CONTROLS
