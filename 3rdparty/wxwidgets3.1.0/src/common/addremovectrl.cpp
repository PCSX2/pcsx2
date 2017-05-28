///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/addremovectrl.cpp
// Purpose:     wxAddRemoveCtrl implementation.
// Author:      Vadim Zeitlin
// Created:     2015-01-29
// Copyright:   (c) 2015 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_ADDREMOVECTRL

#ifndef WX_PRECOMP
#endif // WX_PRECOMP

#include "wx/addremovectrl.h"

#include "wx/private/addremovectrl.h"

// ============================================================================
// wxAddRemoveCtrl implementation
// ============================================================================

// ----------------------------------------------------------------------------
// common part
// ----------------------------------------------------------------------------

extern
WXDLLIMPEXP_DATA_ADV(const char) wxAddRemoveCtrlNameStr[] = "wxAddRemoveCtrl";

bool
wxAddRemoveCtrl::Create(wxWindow* parent,
                        wxWindowID winid,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxString& name)
{
    if ( !wxPanel::Create(parent, winid, pos, size, style, name) )
        return false;

    // We don't do anything here, the buttons are created when we're given the
    // adaptor to use them with in SetAdaptor().
    return true;
}

wxAddRemoveCtrl::~wxAddRemoveCtrl()
{
    delete m_impl;
}

void wxAddRemoveCtrl::SetAdaptor(wxAddRemoveAdaptor* adaptor)
{
    wxCHECK_RET( !m_impl, wxS("should be only called once") );

    wxCHECK_RET( adaptor, wxS("should have a valid adaptor") );

    wxWindow* const ctrlItems = adaptor->GetItemsCtrl();
    wxCHECK_RET( ctrlItems, wxS("should have a valid items control") );

    m_impl = new wxAddRemoveImpl(adaptor, this, ctrlItems);
}

void
wxAddRemoveCtrl::SetButtonsToolTips(const wxString& addtip,
                                    const wxString& removetip)
{
    wxCHECK_RET( m_impl, wxS("can only be called after SetAdaptor()") );

    m_impl->SetButtonsToolTips(addtip, removetip);
}

wxSize wxAddRemoveCtrl::DoGetBestClientSize() const
{
    return m_impl ? m_impl->GetBestClientSize() : wxDefaultSize;
}

#endif // wxUSE_ADDREMOVECTRL
