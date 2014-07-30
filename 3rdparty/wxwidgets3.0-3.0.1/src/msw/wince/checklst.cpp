///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/wince/checklst.cpp
// Purpose:     implementation of wxCheckListBox class
// Author:      Wlodzimierz ABX Skiba
// Modified by:
// Created:     30.10.2005
// Copyright:   (c) Wlodzimierz Skiba
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

#if wxUSE_CHECKLISTBOX

#include "wx/checklst.h"

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
#endif

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// implementation of wxCheckListBox class
// ----------------------------------------------------------------------------

// define event table
// ------------------
BEGIN_EVENT_TABLE(wxCheckListBox, wxControl)
    EVT_SIZE(wxCheckListBox::OnSize)
END_EVENT_TABLE()

// control creation
// ----------------

// def ctor: use Create() to really create the control
wxCheckListBox::wxCheckListBox()
{
}

// ctor which creates the associated control
wxCheckListBox::wxCheckListBox(wxWindow *parent, wxWindowID id,
                               const wxPoint& pos, const wxSize& size,
                               int nStrings, const wxString choices[],
                               long style, const wxValidator& val,
                               const wxString& name)
{
    Create(parent, id, pos, size, nStrings, choices, style, val, name);
}

wxCheckListBox::wxCheckListBox(wxWindow *parent, wxWindowID id,
                               const wxPoint& pos, const wxSize& size,
                               const wxArrayString& choices,
                               long style, const wxValidator& val,
                               const wxString& name)
{
    Create(parent, id, pos, size, choices, style, val, name);
}

wxCheckListBox::~wxCheckListBox()
{
    m_itemsClientData.Clear();
}

bool wxCheckListBox::Create(wxWindow *parent, wxWindowID id,
                            const wxPoint& pos, const wxSize& size,
                            int n, const wxString choices[],
                            long style,
                            const wxValidator& validator, const wxString& name)
{
    // initialize base class fields
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    // create the native control
    if ( !MSWCreateControl(WC_LISTVIEW, wxEmptyString, pos, size) )
    {
        // control creation failed
        return false;
    }

    ::SendMessage(GetHwnd(), LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                  LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT );

    // insert single column with checkboxes and labels
    LV_COLUMN col;
    wxZeroMemory(col);
    ListView_InsertColumn(GetHwnd(), 0, &col );

    ListView_SetItemCount( GetHwnd(), n );

    // initialize the contents
    for ( int i = 0; i < n; i++ )
    {
        Append(choices[i]);
    }

    m_itemsClientData.SetCount(n);

    // now we can compute our best size correctly, so do it if necessary
    SetInitialSize(size);

    return true;
}

bool wxCheckListBox::Create(wxWindow *parent, wxWindowID id,
                            const wxPoint& pos, const wxSize& size,
                            const wxArrayString& choices,
                            long style,
                            const wxValidator& validator, const wxString& name)
{
    wxCArrayString chs(choices);
    return Create(parent, id, pos, size, chs.GetCount(), chs.GetStrings(),
                  style, validator, name);
}

WXDWORD wxCheckListBox::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    WXDWORD wstyle = wxControl::MSWGetStyle(style, exstyle);

    wstyle |= LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_NOSORTHEADER;

    return wstyle;
}

void wxCheckListBox::OnSize(wxSizeEvent& event)
{
    // set width of the column we use to the width of list client area
    event.Skip();
    int w = GetClientSize().x;
    ListView_SetColumnWidth( GetHwnd(), 0, w );
}

// misc overloaded methods
// -----------------------

void wxCheckListBox::DoDeleteOneItem(unsigned int n)
{
    wxCHECK_RET( IsValid( n ), wxT("invalid index in wxCheckListBox::Delete") );

    if ( !ListView_DeleteItem(GetHwnd(), n) )
    {
        wxLogLastError(wxT("ListView_DeleteItem"));
    }
    m_itemsClientData.RemoveAt(n);
}

// check items
// -----------

bool wxCheckListBox::IsChecked(unsigned int uiIndex) const
{
    wxCHECK_MSG( IsValid( uiIndex ), false,
                 wxT("invalid index in wxCheckListBox::IsChecked") );

    return (ListView_GetCheckState(((HWND)GetHWND()), uiIndex) != 0);
}

void wxCheckListBox::Check(unsigned int uiIndex, bool bCheck)
{
    wxCHECK_RET( IsValid( uiIndex ),
                 wxT("invalid index in wxCheckListBox::Check") );

    ListView_SetCheckState(((HWND)GetHWND()), uiIndex, bCheck)
}

// interface derived from wxListBox and lower classes
// --------------------------------------------------

void wxCheckListBox::DoClear()
{
    unsigned int n = GetCount();

    while ( n > 0 )
    {
        n--;
        DoDeleteOneItem(n);
    }

    wxASSERT_MSG( IsEmpty(), wxT("logic error in DoClear()") );
}

unsigned int wxCheckListBox::GetCount() const
{
    return (unsigned int)ListView_GetItemCount( (HWND)GetHWND() );
}

int wxCheckListBox::GetSelection() const
{
    int i;
    for (i = 0; (unsigned int)i < GetCount(); i++)
    {
        int selState = ListView_GetItemState(GetHwnd(), i, LVIS_SELECTED);
        if (selState == LVIS_SELECTED)
            return i;
    }

    return wxNOT_FOUND;
}

int wxCheckListBox::GetSelections(wxArrayInt& aSelections) const
{
    int i;
    for (i = 0; (unsigned int)i < GetCount(); i++)
    {
        int selState = ListView_GetItemState(GetHwnd(), i, LVIS_SELECTED);
        if (selState == LVIS_SELECTED)
            aSelections.Add(i);
    }

    return aSelections.GetCount();
}

wxString wxCheckListBox::GetString(unsigned int n) const
{
    const int bufSize = 513;
    wxChar buf[bufSize];
    ListView_GetItemText( (HWND)GetHWND(), n, 0, buf, bufSize - 1 );
    buf[bufSize-1] = wxT('\0');
    wxString str(buf);
    return str;
}

bool wxCheckListBox::IsSelected(int n) const
{
    int selState = ListView_GetItemState(GetHwnd(), n, LVIS_SELECTED);
    return (selState == LVIS_SELECTED);
}

void wxCheckListBox::SetString(unsigned int n, const wxString& s)
{
    wxCHECK_RET( IsValid( n ),
                 wxT("invalid index in wxCheckListBox::SetString") );
    wxChar *buf = new wxChar[s.length()+1];
    wxStrcpy(buf, s.c_str());
    ListView_SetItemText( (HWND)GetHWND(), n, 0, buf );
    delete [] buf;
}

void* wxCheckListBox::DoGetItemClientData(unsigned int n) const
{
    return m_itemsClientData.Item(n);
}

int wxCheckListBox::DoInsertItems(const wxArrayStringsAdapter & items,
                                  unsigned int pos,
                                  void **clientData, wxClientDataType type)
{
    const unsigned int count = items.GetCount();

    ListView_SetItemCount( GetHwnd(), GetCount() + count );

    int n = wxNOT_FOUND;

    for( unsigned int i = 0; i < count; i++ )
    {
        LVITEM newItem;
        wxZeroMemory(newItem);
        newItem.iItem = pos + i;
        n = ListView_InsertItem( (HWND)GetHWND(), & newItem );
        wxCHECK_MSG( n != -1, -1, wxT("Item not added") );
        SetString( n, items[i] );
        m_itemsClientData.Insert(NULL, n);

        AssignNewItemClientData(n, clientData, i, type);
    }

    return n;
}

void wxCheckListBox::DoSetFirstItem(int n)
{
    int pos = ListView_GetTopIndex( (HWND)GetHWND() );
    if(pos == n) return;
    POINT ppt;
    BOOL ret = ListView_GetItemPosition( (HWND)GetHWND(), n, &ppt );
    wxCHECK_RET( ret == TRUE, wxT("Broken DoSetFirstItem") );
    ListView_Scroll( (HWND)GetHWND(), 0, 0 );
    ListView_Scroll( (HWND)GetHWND(), 0, ppt.y );
}

void wxCheckListBox::DoSetItemClientData(unsigned int n, void* clientData)
{
    m_itemsClientData.Item(n) = clientData;
}

void wxCheckListBox::DoSetSelection(int n, bool select)
{
    ListView_SetItemState(GetHwnd(), n, select ? LVIS_SELECTED : 0, LVIS_SELECTED);
}

bool wxCheckListBox::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result)
{
    // prepare the event
    // -----------------

    wxCommandEvent event(wxEVT_NULL, m_windowId);
    event.SetEventObject(this);

    wxEventType eventType = wxEVT_NULL;

    NMHDR *nmhdr = (NMHDR *)lParam;

    if ( nmhdr->hwndFrom == GetHwnd() )
    {
        // almost all messages use NM_LISTVIEW
        NM_LISTVIEW *nmLV = (NM_LISTVIEW *)nmhdr;

        const int iItem = nmLV->iItem;

        bool processed = true;
        switch ( nmhdr->code )
        {
            case LVN_ITEMCHANGED:
                // we translate this catch all message into more interesting
                // (and more easy to process) wxWidgets events

                // first of all, we deal with the state change events only and
                // only for valid items (item == -1 for the virtual list
                // control)
                if ( nmLV->uChanged & LVIF_STATE && iItem != -1 )
                {
                    // temp vars for readability
                    const UINT stOld = nmLV->uOldState;
                    const UINT stNew = nmLV->uNewState;

                    // Check image changed
                    if ((stOld & LVIS_STATEIMAGEMASK) != (stNew & LVIS_STATEIMAGEMASK))
                    {
                        event.SetEventType(wxEVT_CHECKLISTBOX);
                        event.SetInt(IsChecked(iItem));
                        (void) GetEventHandler()->ProcessEvent(event);
                    }

                    if ( (stNew & LVIS_SELECTED) != (stOld & LVIS_SELECTED) )
                    {
                        eventType = wxEVT_LISTBOX;

                        event.SetExtraLong( (stNew & LVIS_SELECTED) != 0 ); // is a selection
                        event.SetInt(iItem);
                    }
                }

                if ( eventType == wxEVT_NULL )
                {
                    // not an interesting event for us
                    return false;
                }

                break;

            default:
                processed = false;
        }

        if ( !processed )
            return wxControl::MSWOnNotify(idCtrl, lParam, result);
    }
    else
    {
        // where did this one come from?
        return false;
    }

    // process the event
    // -----------------

    event.SetEventType(eventType);

    bool processed = GetEventHandler()->ProcessEvent(event);
    if ( processed )
        *result = 0;

    return processed;
}


#endif
