///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/addremovectrl.h
// Purpose:     wxAddRemoveImpl helper class declaration
// Author:      Vadim Zeitlin
// Created:     2015-02-04
// Copyright:   (c) 2015 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_ADDREMOVECTRL_H_
#define _WX_PRIVATE_ADDREMOVECTRL_H_

#include "wx/button.h"
#include "wx/sizer.h"

// ----------------------------------------------------------------------------
// wxAddRemoveImplBase: implementation-only part of wxAddRemoveCtrl, base part
// ----------------------------------------------------------------------------

class wxAddRemoveImplBase
{
public:
    // Base class ctor just initializes the associated adaptor, the derived
    // class is supposed to create the buttons and layout everything.
    //
    // Takes ownership of the adaptor pointer.
    wxEXPLICIT wxAddRemoveImplBase(wxAddRemoveAdaptor* adaptor,
                                   wxAddRemoveCtrl* WXUNUSED(parent),
                                   wxWindow* ctrlItems)
        : m_adaptor(adaptor)
    {
        ctrlItems->Bind(wxEVT_CHAR, &wxAddRemoveImplBase::OnChar, this);
    }

    // wxOSX implementation needs to override this as it doesn't use sizers,
    // for the others it is not necessary.
    virtual wxSize GetBestClientSize() const { return wxDefaultSize; }

    virtual void SetButtonsToolTips(const wxString& addtip,
                                    const wxString& removetip) = 0;

    virtual ~wxAddRemoveImplBase()
    {
        delete m_adaptor;
    }

    // Event handlers which must be connected to the appropriate sources by the
    // derived classes.

    void OnUpdateUIAdd(wxUpdateUIEvent& event)
    {
        event.Enable( m_adaptor->CanAdd() );
    }

    void OnUpdateUIRemove(wxUpdateUIEvent& event)
    {
        event.Enable( m_adaptor->CanRemove() );
    }

    void OnAdd(wxCommandEvent& WXUNUSED(event))
    {
        m_adaptor->OnAdd();
    }

    void OnRemove(wxCommandEvent& WXUNUSED(event))
    {
        m_adaptor->OnRemove();
    }

private:
    // This event handler is connected by this class itself and doesn't need to
    // be accessible to the derived classes.

    void OnChar(wxKeyEvent& event)
    {
        switch ( event.GetKeyCode() )
        {
            case '+':
            case WXK_INSERT:
            case WXK_NUMPAD_INSERT:
                if ( m_adaptor->CanAdd() )
                    m_adaptor->OnAdd();
                return;

            case '-':
            case WXK_DELETE:
            case WXK_NUMPAD_DELETE:
                if ( m_adaptor->CanRemove() )
                    m_adaptor->OnRemove();
                return;
        }

        event.Skip();
    }

    wxAddRemoveAdaptor* const m_adaptor;

    wxDECLARE_NO_COPY_CLASS(wxAddRemoveImplBase);
};

// GTK+ uses a wxToolBar-based implementation and so doesn't need this class.
#ifndef __WXGTK__

// Base class for the ports using actual wxButtons for the "+"/"-" buttons.
class wxAddRemoveImplWithButtons : public wxAddRemoveImplBase
{
public:
    wxEXPLICIT wxAddRemoveImplWithButtons(wxAddRemoveAdaptor* adaptor,
                                          wxAddRemoveCtrl* parent,
                                          wxWindow* ctrlItems)
        : wxAddRemoveImplBase(adaptor, parent, ctrlItems)
    {
        m_btnAdd =
        m_btnRemove = NULL;
    }

    virtual void SetButtonsToolTips(const wxString& addtip,
                                    const wxString& removetip) wxOVERRIDE
    {
        m_btnAdd->SetToolTip(addtip);
        m_btnRemove->SetToolTip(removetip);
    }

protected:
    // Must be called by the derived class ctor after creating the buttons to
    // set up the event handlers.
    void SetUpEvents()
    {
        m_btnAdd->Bind(wxEVT_UPDATE_UI,
                       &wxAddRemoveImplBase::OnUpdateUIAdd, this);
        m_btnRemove->Bind(wxEVT_UPDATE_UI,
                         &wxAddRemoveImplBase::OnUpdateUIRemove, this);

        m_btnAdd->Bind(wxEVT_BUTTON, &wxAddRemoveImplBase::OnAdd, this);
        m_btnRemove->Bind(wxEVT_BUTTON, &wxAddRemoveImplBase::OnRemove, this);
    }

    wxButton *m_btnAdd,
             *m_btnRemove;

    wxDECLARE_NO_COPY_CLASS(wxAddRemoveImplWithButtons);
};

#endif // !wxGTK

#ifdef __WXOSX__
    #include "wx/osx/private/addremovectrl.h"
#elif defined(__WXGTK__)
    #include "wx/gtk/private/addremovectrl.h"
#else
    #include "wx/generic/private/addremovectrl.h"
#endif

#endif // _WX_PRIVATE_ADDREMOVECTRL_H_
