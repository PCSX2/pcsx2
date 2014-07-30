/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/dlgcmn.cpp
// Purpose:     common (to all ports) wxDialog functions
// Author:      Vadim Zeitlin
// Modified by:
// Created:     28.06.99
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

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

#include "wx/dialog.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/button.h"
    #include "wx/dcclient.h"
    #include "wx/intl.h"
    #include "wx/settings.h"
    #include "wx/stattext.h"
    #include "wx/sizer.h"
    #include "wx/containr.h"
#endif

#include "wx/statline.h"
#include "wx/sysopt.h"
#include "wx/module.h"
#include "wx/bookctrl.h"
#include "wx/scrolwin.h"
#include "wx/textwrapper.h"
#include "wx/modalhook.h"

#if wxUSE_DISPLAY
#include "wx/display.h"
#endif

extern WXDLLEXPORT_DATA(const char) wxDialogNameStr[] = "dialog";

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxDialogStyle )
wxBEGIN_FLAGS( wxDialogStyle )
// new style border flags, we put them first to
// use them for streaming out
wxFLAGS_MEMBER(wxBORDER_SIMPLE)
wxFLAGS_MEMBER(wxBORDER_SUNKEN)
wxFLAGS_MEMBER(wxBORDER_DOUBLE)
wxFLAGS_MEMBER(wxBORDER_RAISED)
wxFLAGS_MEMBER(wxBORDER_STATIC)
wxFLAGS_MEMBER(wxBORDER_NONE)

// old style border flags
wxFLAGS_MEMBER(wxSIMPLE_BORDER)
wxFLAGS_MEMBER(wxSUNKEN_BORDER)
wxFLAGS_MEMBER(wxDOUBLE_BORDER)
wxFLAGS_MEMBER(wxRAISED_BORDER)
wxFLAGS_MEMBER(wxSTATIC_BORDER)
wxFLAGS_MEMBER(wxNO_BORDER)

// standard window styles
wxFLAGS_MEMBER(wxTAB_TRAVERSAL)
wxFLAGS_MEMBER(wxCLIP_CHILDREN)

// dialog styles
wxFLAGS_MEMBER(wxWS_EX_VALIDATE_RECURSIVELY)
wxFLAGS_MEMBER(wxSTAY_ON_TOP)
wxFLAGS_MEMBER(wxCAPTION)
#if WXWIN_COMPATIBILITY_2_6
wxFLAGS_MEMBER(wxTHICK_FRAME)
#endif // WXWIN_COMPATIBILITY_2_6
wxFLAGS_MEMBER(wxSYSTEM_MENU)
wxFLAGS_MEMBER(wxRESIZE_BORDER)
#if WXWIN_COMPATIBILITY_2_6
wxFLAGS_MEMBER(wxRESIZE_BOX)
#endif // WXWIN_COMPATIBILITY_2_6
wxFLAGS_MEMBER(wxCLOSE_BOX)
wxFLAGS_MEMBER(wxMAXIMIZE_BOX)
wxFLAGS_MEMBER(wxMINIMIZE_BOX)
wxEND_FLAGS( wxDialogStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxDialog, wxTopLevelWindow, "wx/dialog.h")

wxBEGIN_PROPERTIES_TABLE(wxDialog)
wxPROPERTY( Title, wxString, SetTitle, GetTitle, wxString(), \
           0 /*flags*/, wxT("Helpstring"), wxT("group"))

wxPROPERTY_FLAGS( WindowStyle, wxDialogStyle, long, SetWindowStyleFlag, \
                 GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                 wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxDialog)

wxCONSTRUCTOR_6( wxDialog, wxWindow*, Parent, wxWindowID, Id, \
                wxString, Title, wxPoint, Position, wxSize, Size, long, WindowStyle)

// ----------------------------------------------------------------------------
// wxDialogBase
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxDialogBase, wxTopLevelWindow)
    EVT_BUTTON(wxID_ANY, wxDialogBase::OnButton)

    EVT_CLOSE(wxDialogBase::OnCloseWindow)

    EVT_CHAR_HOOK(wxDialogBase::OnCharHook)
END_EVENT_TABLE()

wxDialogLayoutAdapter* wxDialogBase::sm_layoutAdapter = NULL;
bool wxDialogBase::sm_layoutAdaptation = false;

wxDialogBase::wxDialogBase()
{
    m_returnCode = 0;
    m_affirmativeId = wxID_OK;
    m_escapeId = wxID_ANY;
    m_layoutAdaptationLevel = 3;
    m_layoutAdaptationDone = FALSE;
    m_layoutAdaptationMode = wxDIALOG_ADAPTATION_MODE_DEFAULT;

    // the dialogs have this flag on by default to prevent the events from the
    // dialog controls from reaching the parent frame which is usually
    // undesirable and can lead to unexpected and hard to find bugs
    SetExtraStyle(GetExtraStyle() | wxWS_EX_BLOCK_EVENTS);
}

wxWindow *wxDialogBase::CheckIfCanBeUsedAsParent(wxWindow *parent) const
{
    if ( !parent )
        return NULL;

    extern WXDLLIMPEXP_DATA_BASE(wxList) wxPendingDelete;
    if ( wxPendingDelete.Member(parent) || parent->IsBeingDeleted() )
    {
        // this window is being deleted and we shouldn't create any children
        // under it
        return NULL;
    }

    if ( parent->HasExtraStyle(wxWS_EX_TRANSIENT) )
    {
        // this window is not being deleted yet but it's going to disappear
        // soon so still don't parent this window under it
        return NULL;
    }

    if ( !parent->IsShownOnScreen() )
    {
        // using hidden parent won't work correctly neither
        return NULL;
    }

    // FIXME-VC6: this compiler requires an explicit const cast or it fails
    //            with error C2446
    if ( const_cast<const wxWindow *>(parent) == this )
    {
        // not sure if this can really happen but it doesn't hurt to guard
        // against this clearly invalid situation
        return NULL;
    }

    return parent;
}

wxWindow *
wxDialogBase::GetParentForModalDialog(wxWindow *parent, long style) const
{
    // creating a parent-less modal dialog will result (under e.g. wxGTK2)
    // in an unfocused dialog, so try to find a valid parent for it unless we
    // were explicitly asked not to
    if ( style & wxDIALOG_NO_PARENT )
        return NULL;

    // first try the given parent
    if ( parent )
        parent = CheckIfCanBeUsedAsParent(wxGetTopLevelParent(parent));

    // then the currently active window
    if ( !parent )
        parent = CheckIfCanBeUsedAsParent(
                    wxGetTopLevelParent(wxGetActiveWindow()));

    // and finally the application main window
    if ( !parent )
        parent = CheckIfCanBeUsedAsParent(wxTheApp->GetTopWindow());

    return parent;
}

#if wxUSE_STATTEXT

wxSizer *wxDialogBase::CreateTextSizer(const wxString& message)
{
    wxTextSizerWrapper wrapper(this);

    return CreateTextSizer(message, wrapper);
}

wxSizer *wxDialogBase::CreateTextSizer(const wxString& message,
                                       wxTextSizerWrapper& wrapper)
{
    // I admit that this is complete bogus, but it makes
    // message boxes work for pda screens temporarily..
    int widthMax = -1;
    const bool is_pda = wxSystemSettings::GetScreenType() <= wxSYS_SCREEN_PDA;
    if (is_pda)
    {
        widthMax = wxSystemSettings::GetMetric( wxSYS_SCREEN_X ) - 25;
    }

    return wrapper.CreateSizer(message, widthMax);
}

#endif // wxUSE_STATTEXT

wxSizer *wxDialogBase::CreateButtonSizer(long flags)
{
#ifdef __SMARTPHONE__
    wxDialog* dialog = (wxDialog*) this;
    if ( flags & wxOK )
        dialog->SetLeftMenu(wxID_OK);

    if ( flags & wxCANCEL )
        dialog->SetRightMenu(wxID_CANCEL);

    if ( flags & wxYES )
        dialog->SetLeftMenu(wxID_YES);

    if ( flags & wxNO )
        dialog->SetRightMenu(wxID_NO);

    return NULL;
#else // !__SMARTPHONE__

#if wxUSE_BUTTON

#ifdef __POCKETPC__
    // PocketPC guidelines recommend for Ok/Cancel dialogs to use OK button
    // located inside caption bar and implement Cancel functionality through
    // Undo outside dialog. As native behaviour this will be default here but
    // can be replaced with real wxButtons by setting the option below to 1
    if ( (flags & ~(wxCANCEL|wxNO_DEFAULT)) != wxOK ||
            wxSystemOptions::GetOptionInt(wxT("wince.dialog.real-ok-cancel")) )
#endif // __POCKETPC__
    {
        return CreateStdDialogButtonSizer(flags);
    }
#ifdef __POCKETPC__
    return NULL;
#endif // __POCKETPC__

#else // !wxUSE_BUTTON
    wxUnusedVar(flags);

    return NULL;
#endif // wxUSE_BUTTON/!wxUSE_BUTTON

#endif // __SMARTPHONE__/!__SMARTPHONE__
}

wxSizer *wxDialogBase::CreateSeparatedSizer(wxSizer *sizer)
{
    // Mac Human Interface Guidelines recommend not to use static lines as
    // grouping elements
#if wxUSE_STATLINE && !defined(__WXMAC__)
    wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
    topsizer->Add(new wxStaticLine(this),
                   wxSizerFlags().Expand().DoubleBorder(wxBOTTOM));
    topsizer->Add(sizer, wxSizerFlags().Expand());
    sizer = topsizer;
#endif // wxUSE_STATLINE

    return sizer;
}

wxSizer *wxDialogBase::CreateSeparatedButtonSizer(long flags)
{
    wxSizer *sizer = CreateButtonSizer(flags);
    if ( !sizer )
        return NULL;

    return CreateSeparatedSizer(sizer);
}

#if wxUSE_BUTTON

wxStdDialogButtonSizer *wxDialogBase::CreateStdDialogButtonSizer( long flags )
{
    wxStdDialogButtonSizer *sizer = new wxStdDialogButtonSizer();

    wxButton *ok = NULL;
    wxButton *yes = NULL;
    wxButton *no = NULL;

    if (flags & wxOK)
    {
        ok = new wxButton(this, wxID_OK);
        sizer->AddButton(ok);
    }

    if (flags & wxCANCEL)
    {
        wxButton *cancel = new wxButton(this, wxID_CANCEL);
        sizer->AddButton(cancel);
    }

    if (flags & wxYES)
    {
        yes = new wxButton(this, wxID_YES);
        sizer->AddButton(yes);
    }

    if (flags & wxNO)
    {
        no = new wxButton(this, wxID_NO);
        sizer->AddButton(no);
    }

    if (flags & wxAPPLY)
    {
        wxButton *apply = new wxButton(this, wxID_APPLY);
        sizer->AddButton(apply);
    }

    if (flags & wxCLOSE)
    {
        wxButton *close = new wxButton(this, wxID_CLOSE);
        sizer->AddButton(close);
    }

    if (flags & wxHELP)
    {
        wxButton *help = new wxButton(this, wxID_HELP);
        sizer->AddButton(help);
    }

    if (flags & wxNO_DEFAULT)
    {
        if (no)
        {
            no->SetDefault();
            no->SetFocus();
        }
    }
    else
    {
        if (ok)
        {
            ok->SetDefault();
            ok->SetFocus();
        }
        else if (yes)
        {
            yes->SetDefault();
            yes->SetFocus();
        }
    }

    if (flags & wxOK)
        SetAffirmativeId(wxID_OK);
    else if (flags & wxYES)
        SetAffirmativeId(wxID_YES);
    else if (flags & wxCLOSE)
        SetAffirmativeId(wxID_CLOSE);

    sizer->Realize();

    return sizer;
}

#endif // wxUSE_BUTTON

// ----------------------------------------------------------------------------
// standard buttons handling
// ----------------------------------------------------------------------------

void wxDialogBase::EndDialog(int rc)
{
    if ( IsModal() )
        EndModal(rc);
    else
        Hide();
}

void wxDialogBase::AcceptAndClose()
{
    if ( Validate() && TransferDataFromWindow() )
    {
        EndDialog(m_affirmativeId);
    }
}

void wxDialogBase::SetAffirmativeId(int affirmativeId)
{
    m_affirmativeId = affirmativeId;
}

void wxDialogBase::SetEscapeId(int escapeId)
{
    m_escapeId = escapeId;
}

bool wxDialogBase::EmulateButtonClickIfPresent(int id)
{
#if wxUSE_BUTTON
    wxButton *btn = wxDynamicCast(FindWindow(id), wxButton);

    if ( !btn || !btn->IsEnabled() || !btn->IsShown() )
        return false;

    wxCommandEvent event(wxEVT_BUTTON, id);
    event.SetEventObject(btn);
    btn->GetEventHandler()->ProcessEvent(event);

    return true;
#else // !wxUSE_BUTTON
    wxUnusedVar(id);
    return false;
#endif // wxUSE_BUTTON/!wxUSE_BUTTON
}

bool wxDialogBase::SendCloseButtonClickEvent()
{
    int idCancel = GetEscapeId();
    switch ( idCancel )
    {
        case wxID_NONE:
            // The user doesn't want this dialog to close "implicitly".
            break;

        case wxID_ANY:
            // this value is special: it means translate Esc to wxID_CANCEL
            // but if there is no such button, then fall back to wxID_OK
            if ( EmulateButtonClickIfPresent(wxID_CANCEL) )
                return true;
            idCancel = GetAffirmativeId();
            // fall through

        default:
            // translate Esc to button press for the button with given id
            if ( EmulateButtonClickIfPresent(idCancel) )
                return true;
    }

    return false;
}

bool wxDialogBase::IsEscapeKey(const wxKeyEvent& event)
{
    // For most platforms, Esc key is used to close the dialogs.
    //
    // Notice that we intentionally don't check for modifiers here, Shift-Esc,
    // Alt-Esc and so on still close the dialog, typically.
    return event.GetKeyCode() == WXK_ESCAPE;
}

void wxDialogBase::OnCharHook(wxKeyEvent& event)
{
    if ( IsEscapeKey(event) )
    {
        if ( SendCloseButtonClickEvent() )
        {
            // Skip the call to event.Skip() below, we did handle this key.
            return;
        }
    }

    event.Skip();
}

void wxDialogBase::OnButton(wxCommandEvent& event)
{
    const int id = event.GetId();
    if ( id == GetAffirmativeId() )
    {
        AcceptAndClose();
    }
    else if ( id == wxID_APPLY )
    {
        if ( Validate() )
            TransferDataFromWindow();

        // TODO: disable the Apply button until things change again
    }
    else if ( id == GetEscapeId() ||
                (id == wxID_CANCEL && GetEscapeId() == wxID_ANY) )
    {
        EndDialog(wxID_CANCEL);
    }
    else // not a standard button
    {
        event.Skip();
    }
}

// ----------------------------------------------------------------------------
// compatibility methods for supporting the modality API
// ----------------------------------------------------------------------------

wxDEFINE_EVENT( wxEVT_WINDOW_MODAL_DIALOG_CLOSED , wxWindowModalDialogEvent  );

IMPLEMENT_DYNAMIC_CLASS(wxWindowModalDialogEvent, wxCommandEvent)

void wxDialogBase::ShowWindowModal ()
{
    int retval = ShowModal();
    // wxWindowModalDialogEvent relies on GetReturnCode() returning correct
    // code. Rather than doing it manually in all ShowModal() overrides for
    // native dialogs (and getting accidentally broken again), set it here.
    // The worst that can happen is that it will be set twice to the same
    // value.
    SetReturnCode(retval);
    SendWindowModalDialogEvent ( wxEVT_WINDOW_MODAL_DIALOG_CLOSED  );
}

void wxDialogBase::SendWindowModalDialogEvent ( wxEventType type )
{
    wxWindowModalDialogEvent event ( type, GetId());
    event.SetEventObject(this);

    if ( !GetEventHandler()->ProcessEvent(event) )
    {
        // the event is not propagated upwards to the parent automatically
        // because the dialog is a top level window, so do it manually as
        // in 9 cases of 10 the message must be processed by the dialog
        // owner and not the dialog itself
        (void)GetParent()->GetEventHandler()->ProcessEvent(event);
    }
}


wxDialogModality wxDialogBase::GetModality() const
{
    return IsModal() ? wxDIALOG_MODALITY_APP_MODAL : wxDIALOG_MODALITY_NONE;
}

// ----------------------------------------------------------------------------
// other event handlers
// ----------------------------------------------------------------------------

void wxDialogBase::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
    // We'll send a Cancel message by default, which may close the dialog.

    // Check for looping if the Cancel event handler calls Close().
    //
    // VZ: this is horrible and MT-unsafe. Can't we reuse some of these global
    //     lists here? don't dare to change it now, but should be done later!
    static wxList closing;

    if ( closing.Member(this) )
        return;

    closing.Append(this);

    if ( !SendCloseButtonClickEvent() )
    {
        // If the handler didn't close the dialog (e.g. because there is no
        // button with matching id) we still want to close it when the user
        // clicks the "x" button in the title bar, otherwise we shouldn't even
        // have put it there.
        //
        // Notice that using wxID_CLOSE might have been a better choice but we
        // use wxID_CANCEL for compatibility reasons.
        EndDialog(wxID_CANCEL);
    }

    closing.DeleteObject(this);
}

void wxDialogBase::OnSysColourChanged(wxSysColourChangedEvent& event)
{
#ifndef __WXGTK__
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));
    Refresh();
#endif

    event.Skip();
}

/// Do the adaptation
bool wxDialogBase::DoLayoutAdaptation()
{
    if (GetLayoutAdapter())
    {
        wxWindow* focusWindow = wxFindFocusDescendant(this); // from event.h
        if (GetLayoutAdapter()->DoLayoutAdaptation((wxDialog*) this))
        {
            if (focusWindow)
                focusWindow->SetFocus();
            return true;
        }
        else
            return false;
    }
    else
        return false;
}

/// Can we do the adaptation?
bool wxDialogBase::CanDoLayoutAdaptation()
{
    // Check if local setting overrides the global setting
    bool layoutEnabled = (GetLayoutAdaptationMode() == wxDIALOG_ADAPTATION_MODE_ENABLED) || (IsLayoutAdaptationEnabled() && (GetLayoutAdaptationMode() != wxDIALOG_ADAPTATION_MODE_DISABLED));

    return (layoutEnabled && !m_layoutAdaptationDone && GetLayoutAdaptationLevel() != 0 && GetLayoutAdapter() != NULL && GetLayoutAdapter()->CanDoLayoutAdaptation((wxDialog*) this));
}

/// Set scrolling adapter class, returning old adapter
wxDialogLayoutAdapter* wxDialogBase::SetLayoutAdapter(wxDialogLayoutAdapter* adapter)
{
    wxDialogLayoutAdapter* oldLayoutAdapter = sm_layoutAdapter;
    sm_layoutAdapter = adapter;
    return oldLayoutAdapter;
}

/*!
 * Standard adapter
 */

IMPLEMENT_CLASS(wxDialogLayoutAdapter, wxObject)

IMPLEMENT_CLASS(wxStandardDialogLayoutAdapter, wxDialogLayoutAdapter)

// Allow for caption size on wxWidgets < 2.9
#if defined(__WXGTK__) && !wxCHECK_VERSION(2,9,0)
#define wxEXTRA_DIALOG_HEIGHT 30
#else
#define wxEXTRA_DIALOG_HEIGHT 0
#endif

/// Indicate that adaptation should be done
bool wxStandardDialogLayoutAdapter::CanDoLayoutAdaptation(wxDialog* dialog)
{
    if (dialog->GetSizer())
    {
        wxSize windowSize, displaySize;
        return MustScroll(dialog, windowSize, displaySize) != 0;
    }
    else
        return false;
}

bool wxStandardDialogLayoutAdapter::DoLayoutAdaptation(wxDialog* dialog)
{
    if (dialog->GetSizer())
    {
#if wxUSE_BOOKCTRL
        wxBookCtrlBase* bookContentWindow = wxDynamicCast(dialog->GetContentWindow(), wxBookCtrlBase);

        if (bookContentWindow)
        {
            // If we have a book control, make all the pages (that use sizers) scrollable
            wxWindowList windows;
            for (size_t i = 0; i < bookContentWindow->GetPageCount(); i++)
            {
                wxWindow* page = bookContentWindow->GetPage(i);

                wxScrolledWindow* scrolledWindow = wxDynamicCast(page, wxScrolledWindow);
                if (scrolledWindow)
                    windows.Append(scrolledWindow);
                else if (!scrolledWindow && page->GetSizer())
                {
                    // Create a scrolled window and reparent
                    scrolledWindow = CreateScrolledWindow(page);
                    wxSizer* oldSizer = page->GetSizer();

                    wxSizer* newSizer = new wxBoxSizer(wxVERTICAL);
                    newSizer->Add(scrolledWindow,1, wxEXPAND, 0);

                    page->SetSizer(newSizer, false /* don't delete the old sizer */);

                    scrolledWindow->SetSizer(oldSizer);

                    ReparentControls(page, scrolledWindow);

                    windows.Append(scrolledWindow);
                }
            }

            FitWithScrolling(dialog, windows);
        }
        else
#endif // wxUSE_BOOKCTRL
        {
#if wxUSE_BUTTON
            // If we have an arbitrary dialog, create a scrolling area for the main content, and a button sizer
            // for the main buttons.
            wxScrolledWindow* scrolledWindow = CreateScrolledWindow(dialog);

            int buttonSizerBorder = 0;

            // First try to find a wxStdDialogButtonSizer
            wxSizer* buttonSizer = FindButtonSizer(true /* find std button sizer */, dialog, dialog->GetSizer(), buttonSizerBorder);

            // Next try to find a wxBoxSizer containing the controls
            if (!buttonSizer && dialog->GetLayoutAdaptationLevel() > wxDIALOG_ADAPTATION_STANDARD_SIZER)
                buttonSizer = FindButtonSizer(false /* find ordinary sizer */, dialog, dialog->GetSizer(), buttonSizerBorder);

            // If we still don't have a button sizer, collect any 'loose' buttons in the layout
            if (!buttonSizer && dialog->GetLayoutAdaptationLevel() > wxDIALOG_ADAPTATION_ANY_SIZER)
            {
                int count = 0;
                wxStdDialogButtonSizer* stdButtonSizer = new wxStdDialogButtonSizer;
                buttonSizer = stdButtonSizer;

                FindLooseButtons(dialog, stdButtonSizer, dialog->GetSizer(), count);
                if (count > 0)
                    stdButtonSizer->Realize();
                else
                {
                    wxDELETE(buttonSizer);
                }
            }

            if (buttonSizerBorder == 0)
                buttonSizerBorder = 5;

            ReparentControls(dialog, scrolledWindow, buttonSizer);

            wxBoxSizer* newTopSizer = new wxBoxSizer(wxVERTICAL);
            wxSizer* oldSizer = dialog->GetSizer();

            dialog->SetSizer(newTopSizer, false /* don't delete old sizer */);

            newTopSizer->Add(scrolledWindow, 1, wxEXPAND|wxALL, 0);
            if (buttonSizer)
                newTopSizer->Add(buttonSizer, 0, wxEXPAND|wxALL, buttonSizerBorder);

            scrolledWindow->SetSizer(oldSizer);

            FitWithScrolling(dialog, scrolledWindow);
#endif // wxUSE_BUTTON
        }
    }

    dialog->SetLayoutAdaptationDone(true);
    return true;
}

// Create the scrolled window
wxScrolledWindow* wxStandardDialogLayoutAdapter::CreateScrolledWindow(wxWindow* parent)
{
    wxScrolledWindow* scrolledWindow = new wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL|wxVSCROLL|wxHSCROLL|wxBORDER_NONE);
    return scrolledWindow;
}

#if wxUSE_BUTTON

/// Find and remove the button sizer, if any
wxSizer* wxStandardDialogLayoutAdapter::FindButtonSizer(bool stdButtonSizer, wxDialog* dialog, wxSizer* sizer, int& retBorder, int accumlatedBorder)
{
    for ( wxSizerItemList::compatibility_iterator node = sizer->GetChildren().GetFirst();
          node; node = node->GetNext() )
    {
        wxSizerItem *item = node->GetData();
        wxSizer *childSizer = item->GetSizer();

        if ( childSizer )
        {
            int newBorder = accumlatedBorder;
            if (item->GetFlag() & wxALL)
                newBorder += item->GetBorder();

            if (stdButtonSizer) // find wxStdDialogButtonSizer
            {
                wxStdDialogButtonSizer* buttonSizer = wxDynamicCast(childSizer, wxStdDialogButtonSizer);
                if (buttonSizer)
                {
                    sizer->Detach(childSizer);
                    retBorder = newBorder;
                    return buttonSizer;
                }
            }
            else // find a horizontal box sizer containing standard buttons
            {
                wxBoxSizer* buttonSizer = wxDynamicCast(childSizer, wxBoxSizer);
                if (buttonSizer && IsOrdinaryButtonSizer(dialog, buttonSizer))
                {
                    sizer->Detach(childSizer);
                    retBorder = newBorder;
                    return buttonSizer;
                }
            }

            wxSizer* s = FindButtonSizer(stdButtonSizer, dialog, childSizer, retBorder, newBorder);
            if (s)
                return s;
        }
    }
    return NULL;
}

/// Check if this sizer contains standard buttons, and so can be repositioned in the dialog
bool wxStandardDialogLayoutAdapter::IsOrdinaryButtonSizer(wxDialog* dialog, wxBoxSizer* sizer)
{
    if (sizer->GetOrientation() != wxHORIZONTAL)
        return false;

    for ( wxSizerItemList::compatibility_iterator node = sizer->GetChildren().GetFirst();
          node; node = node->GetNext() )
    {
        wxSizerItem *item = node->GetData();
        wxButton *childButton = wxDynamicCast(item->GetWindow(), wxButton);

        if (childButton && IsStandardButton(dialog, childButton))
            return true;
    }
    return false;
}

/// Check if this is a standard button
bool wxStandardDialogLayoutAdapter::IsStandardButton(wxDialog* dialog, wxButton* button)
{
    wxWindowID id = button->GetId();

    return (id == wxID_OK || id == wxID_CANCEL || id == wxID_YES || id == wxID_NO || id == wxID_SAVE ||
            id == wxID_APPLY || id == wxID_HELP || id == wxID_CONTEXT_HELP || dialog->IsMainButtonId(id));
}

/// Find 'loose' main buttons in the existing layout and add them to the standard dialog sizer
bool wxStandardDialogLayoutAdapter::FindLooseButtons(wxDialog* dialog, wxStdDialogButtonSizer* buttonSizer, wxSizer* sizer, int& count)
{
    wxSizerItemList::compatibility_iterator node = sizer->GetChildren().GetFirst();
    while (node)
    {
        wxSizerItemList::compatibility_iterator next = node->GetNext();
        wxSizerItem *item = node->GetData();
        wxSizer *childSizer = item->GetSizer();
        wxButton *childButton = wxDynamicCast(item->GetWindow(), wxButton);

        if (childButton && IsStandardButton(dialog, childButton))
        {
            sizer->Detach(childButton);
            buttonSizer->AddButton(childButton);
            count ++;
        }

        if (childSizer)
            FindLooseButtons(dialog, buttonSizer, childSizer, count);

        node = next;
    }
    return true;
}

#endif // wxUSE_BUTTON

/// Reparent the controls to the scrolled window
void wxStandardDialogLayoutAdapter::ReparentControls(wxWindow* parent, wxWindow* reparentTo, wxSizer* buttonSizer)
{
    DoReparentControls(parent, reparentTo, buttonSizer);
}

void wxStandardDialogLayoutAdapter::DoReparentControls(wxWindow* parent, wxWindow* reparentTo, wxSizer* buttonSizer)
{
    wxWindowList::compatibility_iterator node = parent->GetChildren().GetFirst();
    while (node)
    {
        wxWindowList::compatibility_iterator next = node->GetNext();

        wxWindow *win = node->GetData();

        // Don't reparent the scrolled window or buttons in the button sizer
        if (win != reparentTo && (!buttonSizer || !buttonSizer->GetItem(win)))
        {
            win->Reparent(reparentTo);
#ifdef __WXMSW__
            // Restore correct tab order
            ::SetWindowPos((HWND) win->GetHWND(), HWND_BOTTOM, -1, -1, -1, -1, SWP_NOMOVE|SWP_NOSIZE);
#endif
        }

        node = next;
    }
}

/// Find whether scrolling will be necessary for the dialog, returning wxVERTICAL, wxHORIZONTAL or both
int wxStandardDialogLayoutAdapter::MustScroll(wxDialog* dialog, wxSize& windowSize, wxSize& displaySize)
{
    return DoMustScroll(dialog, windowSize, displaySize);
}

/// Find whether scrolling will be necessary for the dialog, returning wxVERTICAL, wxHORIZONTAL or both
int wxStandardDialogLayoutAdapter::DoMustScroll(wxDialog* dialog, wxSize& windowSize, wxSize& displaySize)
{
    wxSize minWindowSize = dialog->GetSizer()->GetMinSize();
    windowSize = dialog->GetSize();
    windowSize = wxSize(wxMax(windowSize.x, minWindowSize.x), wxMax(windowSize.y, minWindowSize.y));
#if wxUSE_DISPLAY
    displaySize = wxDisplay(wxDisplay::GetFromWindow(dialog)).GetClientArea().GetSize();
#else
    displaySize = wxGetClientDisplayRect().GetSize();
#endif

    int flags = 0;

    if (windowSize.y >= (displaySize.y - wxEXTRA_DIALOG_HEIGHT))
        flags |= wxVERTICAL;
    if (windowSize.x >= displaySize.x)
        flags |= wxHORIZONTAL;

    return flags;
}

// A function to fit the dialog around its contents, and then adjust for screen size.
// If scrolled windows are passed, scrolling is enabled in the required orientation(s).
bool wxStandardDialogLayoutAdapter::FitWithScrolling(wxDialog* dialog, wxWindowList& windows)
{
    return DoFitWithScrolling(dialog, windows);
}

// A function to fit the dialog around its contents, and then adjust for screen size.
// If a scrolled window is passed, scrolling is enabled in the required orientation(s).
bool wxStandardDialogLayoutAdapter::FitWithScrolling(wxDialog* dialog, wxScrolledWindow* scrolledWindow)
{
    return DoFitWithScrolling(dialog, scrolledWindow);
}

// A function to fit the dialog around its contents, and then adjust for screen size.
// If a scrolled window is passed, scrolling is enabled in the required orientation(s).
bool wxStandardDialogLayoutAdapter::DoFitWithScrolling(wxDialog* dialog, wxScrolledWindow* scrolledWindow)
{
    wxWindowList windows;
    windows.Append(scrolledWindow);
    return DoFitWithScrolling(dialog, windows);
}

bool wxStandardDialogLayoutAdapter::DoFitWithScrolling(wxDialog* dialog, wxWindowList& windows)
{
    wxSizer* sizer = dialog->GetSizer();
    if (!sizer)
        return false;

    sizer->SetSizeHints(dialog);

    wxSize windowSize, displaySize;
    int scrollFlags = DoMustScroll(dialog, windowSize, displaySize);
    int scrollBarSize = 20;

    if (scrollFlags)
    {
        int scrollBarExtraX = 0, scrollBarExtraY = 0;
        bool resizeHorizontally = (scrollFlags & wxHORIZONTAL) != 0;
        bool resizeVertically = (scrollFlags & wxVERTICAL) != 0;

        if (windows.GetCount() != 0)
        {
            // Allow extra for a scrollbar, assuming we resizing in one direction only.
            if ((resizeVertically && !resizeHorizontally) && (windowSize.x < (displaySize.x - scrollBarSize)))
                scrollBarExtraX = scrollBarSize;
            if ((resizeHorizontally && !resizeVertically) && (windowSize.y < (displaySize.y - scrollBarSize)))
                scrollBarExtraY = scrollBarSize;
        }

        wxWindowList::compatibility_iterator node = windows.GetFirst();
        while (node)
        {
            wxWindow *win = node->GetData();
            wxScrolledWindow* scrolledWindow = wxDynamicCast(win, wxScrolledWindow);
            if (scrolledWindow)
            {
                scrolledWindow->SetScrollRate(resizeHorizontally ? 10 : 0, resizeVertically ? 10 : 0);

                if (scrolledWindow->GetSizer())
                    scrolledWindow->GetSizer()->Fit(scrolledWindow);
            }

            node = node->GetNext();
        }

        wxSize limitTo = windowSize + wxSize(scrollBarExtraX, scrollBarExtraY);
        if (resizeVertically)
            limitTo.y = displaySize.y - wxEXTRA_DIALOG_HEIGHT;
        if (resizeHorizontally)
            limitTo.x = displaySize.x;

        dialog->SetMinSize(limitTo);
        dialog->SetSize(limitTo);

        dialog->SetSizeHints( limitTo.x, limitTo.y, dialog->GetMaxWidth(), dialog->GetMaxHeight() );
    }

    return true;
}

/*!
 * Module to initialise standard adapter
 */

class wxDialogLayoutAdapterModule: public wxModule
{
    DECLARE_DYNAMIC_CLASS(wxDialogLayoutAdapterModule)
public:
    wxDialogLayoutAdapterModule() {}
    virtual void OnExit() { delete wxDialogBase::SetLayoutAdapter(NULL); }
    virtual bool OnInit() { wxDialogBase::SetLayoutAdapter(new wxStandardDialogLayoutAdapter); return true; }
};

IMPLEMENT_DYNAMIC_CLASS(wxDialogLayoutAdapterModule, wxModule)
