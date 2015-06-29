///////////////////////////////////////////////////////////////////////////////
// Name:        wx/simplebook.h
// Purpose:     wxBookCtrlBase-derived class without any controller.
// Author:      Vadim Zeitlin
// Created:     2012-08-21
// Copyright:   (c) 2012 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SIMPLEBOOK_H_
#define _WX_SIMPLEBOOK_H_

#include "wx/bookctrl.h"

#if wxUSE_BOOKCTRL

#include "wx/vector.h"

// ----------------------------------------------------------------------------
// wxSimplebook: a book control without any user-actionable controller.
// ----------------------------------------------------------------------------

// NB: This class doesn't use DLL export declaration as it's fully inline.

class wxSimplebook : public wxBookCtrlBase
{
public:
    wxSimplebook()
    {
        Init();
    }

    wxSimplebook(wxWindow *parent,
                 wxWindowID winid = wxID_ANY,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = 0,
                 const wxString& name = wxEmptyString)
        : wxBookCtrlBase(parent, winid, pos, size, style | wxBK_TOP, name)
    {
        Init();
    }

    bool Create(wxWindow *parent,
                wxWindowID winid = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxEmptyString)
    {
        return wxBookCtrlBase::Create(parent, winid, pos, size, style | wxBK_TOP, name);
    }


    // Methods specific to this class.

    // A method allowing to add a new page without any label (which is unused
    // by this control) and show it immediately.
    bool ShowNewPage(wxWindow* page)
    {
        return AddPage(page, wxString(), true /* select it */);
    }


    // Set effect to use for showing/hiding pages.
    void SetEffects(wxShowEffect showEffect, wxShowEffect hideEffect)
    {
        m_showEffect = showEffect;
        m_hideEffect = hideEffect;
    }

    // Or the same effect for both of them.
    void SetEffect(wxShowEffect effect)
    {
        SetEffects(effect, effect);
    }

    // And the same for time outs.
    void SetEffectsTimeouts(unsigned showTimeout, unsigned hideTimeout)
    {
        m_showTimeout = showTimeout;
        m_hideTimeout = hideTimeout;
    }

    void SetEffectTimeout(unsigned timeout)
    {
        SetEffectsTimeouts(timeout, timeout);
    }


    // Implement base class pure virtual methods.

    // Page management
    virtual bool InsertPage(size_t n,
                            wxWindow *page,
                            const wxString& text,
                            bool bSelect = false,
                            int imageId = NO_IMAGE)
    {
        if ( !wxBookCtrlBase::InsertPage(n, page, text, bSelect, imageId) )
            return false;

        m_pageTexts.insert(m_pageTexts.begin() + n, text);

        if ( !DoSetSelectionAfterInsertion(n, bSelect) )
            page->Hide();

        return true;
    }

    virtual int SetSelection(size_t n)
    {
        return DoSetSelection(n, SetSelection_SendEvent);
    }

    virtual int ChangeSelection(size_t n)
    {
        return DoSetSelection(n);
    }

    // Neither labels nor images are supported but we still store the labels
    // just in case the user code attaches some importance to them.
    virtual bool SetPageText(size_t n, const wxString& strText)
    {
        wxCHECK_MSG( n < GetPageCount(), false, wxS("Invalid page") );

        m_pageTexts[n] = strText;

        return true;
    }

    virtual wxString GetPageText(size_t n) const
    {
        wxCHECK_MSG( n < GetPageCount(), wxString(), wxS("Invalid page") );

        return m_pageTexts[n];
    }

    virtual bool SetPageImage(size_t WXUNUSED(n), int WXUNUSED(imageId))
    {
        return false;
    }

    virtual int GetPageImage(size_t WXUNUSED(n)) const
    {
        return NO_IMAGE;
    }

protected:
    virtual void UpdateSelectedPage(size_t newsel)
    {
        m_selection = (int)newsel;
    }

    virtual wxBookCtrlEvent* CreatePageChangingEvent() const
    {
        return new wxBookCtrlEvent(wxEVT_BOOKCTRL_PAGE_CHANGING,
                                   GetId());
    }

    virtual void MakeChangedEvent(wxBookCtrlEvent& event)
    {
        event.SetEventType(wxEVT_BOOKCTRL_PAGE_CHANGED);
    }

    virtual wxWindow *DoRemovePage(size_t page)
    {
        wxWindow* const win = wxBookCtrlBase::DoRemovePage(page);
        if ( win )
        {
            m_pageTexts.erase(m_pageTexts.begin() + page);

            DoSetSelectionAfterRemoval(page);
        }

        return win;
    }

    virtual void DoSize()
    {
        wxWindow* const page = GetCurrentPage();
        if ( page )
            page->SetSize(GetPageRect());
    }

    virtual void DoShowPage(wxWindow* page, bool show)
    {
        if ( show )
            page->ShowWithEffect(m_showEffect, m_showTimeout);
        else
            page->HideWithEffect(m_hideEffect, m_hideTimeout);
    }

private:
    void Init()
    {
        // We don't need any border as we don't have anything to separate the
        // page contents from.
        SetInternalBorder(0);

        // No effects by default.
        m_showEffect =
        m_hideEffect = wxSHOW_EFFECT_NONE;

        m_showTimeout =
        m_hideTimeout = 0;
    }

    wxVector<wxString> m_pageTexts;

    wxShowEffect m_showEffect,
                 m_hideEffect;

    unsigned m_showTimeout,
             m_hideTimeout;

    wxDECLARE_NO_COPY_CLASS(wxSimplebook);
};

#endif // wxUSE_BOOKCTRL

#endif // _WX_SIMPLEBOOK_H_
