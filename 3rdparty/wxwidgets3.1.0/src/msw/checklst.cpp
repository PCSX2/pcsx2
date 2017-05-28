///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/checklst.cpp
// Purpose:     implementation of wxCheckListBox class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     16.11.97
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
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

#if wxUSE_CHECKLISTBOX && wxUSE_OWNER_DRAWN

#include "wx/checklst.h"

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcctl.h"
    #include "wx/object.h"
    #include "wx/colour.h"
    #include "wx/font.h"
    #include "wx/bitmap.h"
    #include "wx/window.h"
    #include "wx/listbox.h"
    #include "wx/dcmemory.h"
    #include "wx/settings.h"
    #include "wx/log.h"
#endif

#include "wx/ownerdrw.h"

#include <windowsx.h>

#include "wx/renderer.h"
#include "wx/msw/private.h"
#include "wx/msw/dc.h"

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// get item (converted to right type)
#define GetItem(n)    ((wxCheckListBoxItem *)(GetItem(n)))

namespace
{
    // space around check mark bitmap in pixels
    static const int CHECKMARK_EXTRA_SPACE = 1;

    // space between check bitmap and text label
    static const int CHECKMARK_LABEL_SPACE = 2;

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// declaration and implementation of wxCheckListBoxItem class
// ----------------------------------------------------------------------------

class wxCheckListBoxItem : public wxOwnerDrawn
{
public:
    // ctor
    wxCheckListBoxItem(wxCheckListBox *parent);

    // drawing functions
    virtual bool OnDrawItem(wxDC& dc, const wxRect& rc, wxODAction act, wxODStatus stat);

    // simple accessors and operations
    wxCheckListBox *GetParent() const
        { return m_parent; }

    int GetIndex() const
        { return m_parent->GetItemIndex(const_cast<wxCheckListBoxItem*>(this)); }

    wxString GetName() const
        { return m_parent->GetString(GetIndex()); }


    bool IsChecked() const
        { return m_checked; }

    void Check(bool bCheck)
        { m_checked = bCheck; }

    void Toggle()
        { Check(!IsChecked()); }

private:
    wxCheckListBox *m_parent;
    bool m_checked;

    wxDECLARE_NO_COPY_CLASS(wxCheckListBoxItem);
};

wxCheckListBoxItem::wxCheckListBoxItem(wxCheckListBox *parent)
{
    m_parent = parent;
    m_checked = false;

    wxSize size = wxRendererNative::Get().GetCheckBoxSize(parent);
    size.x += 2 * CHECKMARK_EXTRA_SPACE + CHECKMARK_LABEL_SPACE;

    SetMarginWidth(size.GetWidth());
    SetBackgroundColour(parent->GetBackgroundColour());
}

bool wxCheckListBoxItem::OnDrawItem(wxDC& dc, const wxRect& rc,
                                    wxODAction act, wxODStatus stat)
{
    // first draw the label
    if ( !wxOwnerDrawn::OnDrawItem(dc, rc, act, stat) )
        return false;

    // now draw the check mark part
    wxMSWDCImpl *impl = (wxMSWDCImpl*) dc.GetImpl();
    HDC hdc = GetHdcOf(*impl);

    wxSize size = wxRendererNative::Get().GetCheckBoxSize(GetParent());

    // first create bitmap in a memory DC
    MemoryHDC hdcMem(hdc);
    CompatibleBitmap hBmpCheck(hdc, size.GetWidth(), size.GetHeight());

    // then draw a check mark into it
    {
        SelectInHDC selBmp(hdcMem, hBmpCheck);

        int flags = wxCONTROL_FLAT;
        if ( IsChecked() )
            flags |= wxCONTROL_CHECKED;

        wxDCTemp dcMem(hdcMem);
        wxRendererNative::Get().DrawCheckBox(GetParent(), dcMem, wxRect(size), flags);
    } // select hBmpCheck out of hdcMem

    // finally draw bitmap to screen

    // position of check mark bitmap
    int x = rc.GetX() + CHECKMARK_EXTRA_SPACE;
    int y = rc.GetY() + (rc.GetHeight() - size.GetHeight()) / 2;

    UINT uState = stat & wxOwnerDrawn::wxODSelected ? wxDSB_SELECTED : wxDSB_NORMAL;
    wxDrawStateBitmap(hdc, hBmpCheck, x, y, uState);

    return true;
}

// ----------------------------------------------------------------------------
// implementation of wxCheckListBox class
// ----------------------------------------------------------------------------

// define event table
// ------------------
wxBEGIN_EVENT_TABLE(wxCheckListBox, wxListBox)
  EVT_KEY_DOWN(wxCheckListBox::OnKeyDown)
  EVT_LEFT_DOWN(wxCheckListBox::OnLeftClick)
wxEND_EVENT_TABLE()

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

bool wxCheckListBox::Create(wxWindow *parent, wxWindowID id,
                            const wxPoint& pos, const wxSize& size,
                            int n, const wxString choices[],
                            long style,
                            const wxValidator& validator, const wxString& name)
{
    return wxListBox::Create(parent, id, pos, size, n, choices,
                             style | wxLB_OWNERDRAW, validator, name);
}

bool wxCheckListBox::Create(wxWindow *parent, wxWindowID id,
                            const wxPoint& pos, const wxSize& size,
                            const wxArrayString& choices,
                            long style,
                            const wxValidator& validator, const wxString& name)
{
    return wxListBox::Create(parent, id, pos, size, choices,
                             style | wxLB_OWNERDRAW, validator, name);
}

// create/retrieve item
// --------------------

// create a check list box item
wxOwnerDrawn *wxCheckListBox::CreateLboxItem(size_t WXUNUSED(n))
{
    wxCheckListBoxItem *pItem = new wxCheckListBoxItem(this);
    return pItem;
}

// return item size
// ----------------
bool wxCheckListBox::MSWOnMeasure(WXMEASUREITEMSTRUCT *item)
{
    if ( wxListBox::MSWOnMeasure(item) )
    {
        MEASUREITEMSTRUCT *pStruct = (MEASUREITEMSTRUCT *)item;

        wxSize size = wxRendererNative::Get().GetCheckBoxSize(this);
        size.x += 2 * CHECKMARK_EXTRA_SPACE;
        size.y += 2 * CHECKMARK_EXTRA_SPACE;

        // add place for the check mark
        pStruct->itemWidth += size.GetWidth();

        if ( pStruct->itemHeight < static_cast<unsigned int>(size.GetHeight()) )
            pStruct->itemHeight = size.GetHeight();

        return true;
    }

    return false;
  }

// check items
// -----------

bool wxCheckListBox::IsChecked(unsigned int uiIndex) const
{
    wxCHECK_MSG( IsValid(uiIndex), false, wxT("bad wxCheckListBox index") );

    return GetItem(uiIndex)->IsChecked();
}

void wxCheckListBox::Check(unsigned int uiIndex, bool bCheck)
{
    wxCHECK_RET( IsValid(uiIndex), wxT("bad wxCheckListBox index") );

    GetItem(uiIndex)->Check(bCheck);
    RefreshItem(uiIndex);
}

void wxCheckListBox::Toggle(unsigned int uiIndex)
{
    wxCHECK_RET( IsValid(uiIndex), wxT("bad wxCheckListBox index") );

    GetItem(uiIndex)->Toggle();
    RefreshItem(uiIndex);
}

// process events
// --------------

void wxCheckListBox::OnKeyDown(wxKeyEvent& event)
{
    // what do we do?
    enum
    {
        NONE,
        TOGGLE,
        SET,
        CLEAR
    } oper;

    switch ( event.GetKeyCode() )
    {
        case WXK_SPACE:
            oper = TOGGLE;
            break;

        case WXK_NUMPAD_ADD:
        case '+':
            oper = SET;
            break;

        case WXK_NUMPAD_SUBTRACT:
        case '-':
            oper = CLEAR;
            break;

        default:
            oper = NONE;
    }

    if ( oper != NONE )
    {
        wxArrayInt selections;
        int count = 0;
        if ( HasMultipleSelection() )
        {
            count = GetSelections(selections);
        }
        else
        {
            int sel = GetSelection();
            if (sel != -1)
            {
                count = 1;
                selections.Add(sel);
            }
        }

        for ( int i = 0; i < count; i++ )
        {
            int nItem = selections[i];

            switch ( oper )
            {
                case TOGGLE:
                    Toggle(nItem);
                    break;

                case SET:
                case CLEAR:
                    Check(nItem, oper == SET);
                    break;

                default:
                    wxFAIL_MSG( wxT("what should this key do?") );
            }

            // we should send an event as this has been done by the user and
            // not by the program
            SendEvent(nItem);
        }
    }
    else // nothing to do
    {
        event.Skip();
    }
}

void wxCheckListBox::OnLeftClick(wxMouseEvent& event)
{
    // clicking on the item selects it, clicking on the checkmark toggles

    int nItem = HitTest(event.GetX(), event.GetY());

    if ( nItem != wxNOT_FOUND )
    {
        wxRect rect;
        GetItemRect(nItem, rect);

        // convert item rect to check mark rect
        wxSize size = wxRendererNative::Get().GetCheckBoxSize(this);
        rect.x += CHECKMARK_EXTRA_SPACE;
        rect.y += (rect.GetHeight() - size.GetHeight()) / 2;
        rect.SetSize(size);

        if ( rect.Contains(event.GetX(), event.GetY()) )
        {
            // people expect to get "kill focus" event for the currently
            // focused control before getting events from the other controls
            // and, equally importantly, they may prevent the focus change from
            // taking place at all (e.g. because the old control contents is
            // invalid and needs to be corrected) in which case we shouldn't
            // generate this event at all
            SetFocus();
            if ( FindFocus() == this )
            {
                Toggle(nItem);
                SendEvent(nItem);

                // scroll one item down if the item is the last one
                // and isn't visible at all
                int h;
                GetClientSize(NULL, &h);
                if ( rect.GetBottom() > h )
                    ScrollLines(1);
            }
        }
        else
        {
            // implement default behaviour: clicking on the item selects it
            event.Skip();
        }
    }
    else
    {
        // implement default behaviour on click outside of client zone
        event.Skip();
    }
}

wxSize wxCheckListBox::DoGetBestClientSize() const
{
    wxSize best = wxListBox::DoGetBestClientSize();

    // add room for the checkbox
    wxSize size = wxRendererNative::Get().GetCheckBoxSize(const_cast<wxCheckListBox*>(this));
    size.x += 2 * CHECKMARK_EXTRA_SPACE;
    size.y += 2 * CHECKMARK_EXTRA_SPACE;

    best.x += size.GetWidth();
    if ( best.y < size.GetHeight() )
        best.y = size.GetHeight();

    CacheBestSize(best);
    return best;
}

#endif // wxUSE_CHECKLISTBOX
