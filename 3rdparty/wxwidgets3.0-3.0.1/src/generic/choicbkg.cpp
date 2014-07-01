///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/choicbkg.cpp
// Purpose:     generic implementation of wxChoicebook
// Author:      Vadim Zeitlin
// Modified by: Wlodzimierz ABX Skiba from generic/listbkg.cpp
// Created:     15.09.04
// Copyright:   (c) Vadim Zeitlin, Wlodzimierz Skiba
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

#if wxUSE_CHOICEBOOK

#include "wx/choicebk.h"

#ifndef WX_PRECOMP
    #include "wx/settings.h"
    #include "wx/choice.h"
    #include "wx/sizer.h"
#endif

#include "wx/imaglist.h"

// ----------------------------------------------------------------------------
// various wxWidgets macros
// ----------------------------------------------------------------------------

// check that the page index is valid
#define IS_VALID_PAGE(nPage) ((nPage) < GetPageCount())

// ----------------------------------------------------------------------------
// event table
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxChoicebook, wxBookCtrlBase)

wxDEFINE_EVENT( wxEVT_CHOICEBOOK_PAGE_CHANGING, wxBookCtrlEvent );
wxDEFINE_EVENT( wxEVT_CHOICEBOOK_PAGE_CHANGED,  wxBookCtrlEvent );

BEGIN_EVENT_TABLE(wxChoicebook, wxBookCtrlBase)
    EVT_CHOICE(wxID_ANY, wxChoicebook::OnChoiceSelected)
END_EVENT_TABLE()

// ============================================================================
// wxChoicebook implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxChoicebook creation
// ----------------------------------------------------------------------------

bool
wxChoicebook::Create(wxWindow *parent,
                     wxWindowID id,
                     const wxPoint& pos,
                     const wxSize& size,
                     long style,
                     const wxString& name)
{
    if ( (style & wxBK_ALIGN_MASK) == wxBK_DEFAULT )
    {
        style |= wxBK_TOP;
    }

    // no border for this control, it doesn't look nice together with
    // wxChoice border
    style &= ~wxBORDER_MASK;
    style |= wxBORDER_NONE;

    if ( !wxControl::Create(parent, id, pos, size, style,
                            wxDefaultValidator, name) )
        return false;

    m_bookctrl = new wxChoice
                 (
                    this,
                    wxID_ANY,
                    wxDefaultPosition,
                    wxDefaultSize
                 );

    wxSizer* mainSizer = new wxBoxSizer(IsVertical() ? wxVERTICAL : wxHORIZONTAL);

    if (style & wxBK_RIGHT || style & wxBK_BOTTOM)
        mainSizer->Add(0, 0, 1, wxEXPAND, 0);

    m_controlSizer = new wxBoxSizer(IsVertical() ? wxHORIZONTAL : wxVERTICAL);
    m_controlSizer->Add(m_bookctrl, 1, (IsVertical() ? wxALIGN_CENTRE_VERTICAL : wxALIGN_CENTRE) |wxGROW, 0);
    mainSizer->Add(m_controlSizer, 0, (IsVertical() ? (int) wxGROW : (int) wxALIGN_CENTRE_VERTICAL)|wxALL, m_controlMargin);
    SetSizer(mainSizer);
    return true;
}

// ----------------------------------------------------------------------------
// accessing the pages
// ----------------------------------------------------------------------------

bool wxChoicebook::SetPageText(size_t n, const wxString& strText)
{
    GetChoiceCtrl()->SetString(n, strText);

    return true;
}

wxString wxChoicebook::GetPageText(size_t n) const
{
    return GetChoiceCtrl()->GetString(n);
}

int wxChoicebook::GetPageImage(size_t WXUNUSED(n)) const
{
    return wxNOT_FOUND;
}

bool wxChoicebook::SetPageImage(size_t WXUNUSED(n), int WXUNUSED(imageId))
{
    // fail silently, the code may be written to use one of several book
    // classes and call SetPageImage() unconditionally, it's better to just
    // ignore it (which is the best we can do short of rewriting this class to
    // use wxBitmapComboBox anyhow) than complain loudly about a rather
    // harmless problem

    return false;
}

// ----------------------------------------------------------------------------
// miscellaneous other stuff
// ----------------------------------------------------------------------------

void wxChoicebook::DoSetWindowVariant(wxWindowVariant variant)
{
    wxBookCtrlBase::DoSetWindowVariant(variant);
    if (m_bookctrl)
        m_bookctrl->SetWindowVariant(variant);
}

void wxChoicebook::SetImageList(wxImageList *imageList)
{
    // TODO: can be implemented in form of static bitmap near choice control

    wxBookCtrlBase::SetImageList(imageList);
}

// ----------------------------------------------------------------------------
// selection
// ----------------------------------------------------------------------------

wxBookCtrlEvent* wxChoicebook::CreatePageChangingEvent() const
{
    return new wxBookCtrlEvent(wxEVT_CHOICEBOOK_PAGE_CHANGING, m_windowId);
}

void wxChoicebook::MakeChangedEvent(wxBookCtrlEvent &event)
{
    event.SetEventType(wxEVT_CHOICEBOOK_PAGE_CHANGED);
}

// ----------------------------------------------------------------------------
// adding/removing the pages
// ----------------------------------------------------------------------------

bool
wxChoicebook::InsertPage(size_t n,
                         wxWindow *page,
                         const wxString& text,
                         bool bSelect,
                         int imageId)
{
    if ( !wxBookCtrlBase::InsertPage(n, page, text, bSelect, imageId) )
        return false;

    GetChoiceCtrl()->Insert(text, n);

    // if the inserted page is before the selected one, we must update the
    // index of the selected page
    if ( int(n) <= m_selection )
    {
        // one extra page added
        m_selection++;
        GetChoiceCtrl()->Select(m_selection);
    }

    if ( !DoSetSelectionAfterInsertion(n, bSelect) )
        page->Hide();

    return true;
}

wxWindow *wxChoicebook::DoRemovePage(size_t page)
{
    wxWindow *win = wxBookCtrlBase::DoRemovePage(page);

    if ( win )
    {
        GetChoiceCtrl()->Delete(page);

        DoSetSelectionAfterRemoval(page);
    }

    return win;
}


bool wxChoicebook::DeleteAllPages()
{
    GetChoiceCtrl()->Clear();
    return wxBookCtrlBase::DeleteAllPages();
}

// ----------------------------------------------------------------------------
// wxChoicebook events
// ----------------------------------------------------------------------------

void wxChoicebook::OnChoiceSelected(wxCommandEvent& eventChoice)
{
    if ( eventChoice.GetEventObject() != m_bookctrl )
    {
        eventChoice.Skip();
        return;
    }

    const int selNew = eventChoice.GetSelection();

    if ( selNew == m_selection )
    {
        // this event can only come from our own Select(m_selection) below
        // which we call when the page change is vetoed, so we should simply
        // ignore it
        return;
    }

    SetSelection(selNew);

    // change wasn't allowed, return to previous state
    if (m_selection != selNew)
        GetChoiceCtrl()->Select(m_selection);
}

#endif // wxUSE_CHOICEBOOK
