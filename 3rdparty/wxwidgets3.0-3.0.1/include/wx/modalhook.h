///////////////////////////////////////////////////////////////////////////////
// Name:        wx/modalhook.h
// Purpose:     Allows to hook into showing modal dialogs.
// Author:      Vadim Zeitlin
// Created:     2013-05-19
// Copyright:   (c) 2013 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MODALHOOK_H_
#define _WX_MODALHOOK_H_

#include "wx/vector.h"

class WXDLLIMPEXP_FWD_CORE wxDialog;

// ----------------------------------------------------------------------------
// Class allowing to be notified about any modal dialog calls.
// ----------------------------------------------------------------------------

// To be notified about entering and exiting modal dialogs and possibly to
// replace them with something else (e.g. just return a predefined value for
// testing), define an object of this class, override its Enter() and
// possibly Exit() methods and call Register() on it.
class WXDLLIMPEXP_CORE wxModalDialogHook
{
public:
    // Default ctor doesn't do anything, call Register() to activate the hook.
    wxModalDialogHook() { }

    // Dtor unregisters the hook if it had been registered.
    virtual ~wxModalDialogHook() { DoUnregister(); }

    // Register this hook as being active, i.e. its Enter() and Exit() methods
    // will be called.
    //
    // Notice that the order of registration matters: the last hook registered
    // is called first, and if its Enter() returns something != wxID_NONE, the
    // subsequent hooks are skipped.
    void Register();

    // Unregister this hook. Notice that is done automatically from the dtor.
    void Unregister();

    // Called from wxWidgets code before showing any modal dialogs and calls
    // Enter() for every registered hook.
    static int CallEnter(wxDialog* dialog);

    // Called from wxWidgets code after dismissing the dialog and calls Exit()
    // for every registered hook.
    static void CallExit(wxDialog* dialog);

protected:
    // Called by wxWidgets before showing any modal dialogs, override this to
    // be notified about this and return anything but wxID_NONE to skip showing
    // the modal dialog entirely and just return the specified result.
    virtual int Enter(wxDialog* dialog) = 0;

    // Called by wxWidgets after dismissing the modal dialog. Notice that it
    // won't be called if Enter() hadn't been.
    virtual void Exit(wxDialog* WXUNUSED(dialog)) { }

private:
    // Unregister the given hook, return true if it was done or false if the
    // hook wasn't found.
    bool DoUnregister();

    // All the hooks in reverse registration order (i.e. in call order).
    typedef wxVector<wxModalDialogHook*> Hooks;
    static Hooks ms_hooks;

    wxDECLARE_NO_COPY_CLASS(wxModalDialogHook);
};

// Helper object used by WX_MODAL_DIALOG_HOOK below to ensure that CallExit()
// is called on scope exit.
class wxModalDialogHookExitGuard
{
public:
    wxEXPLICIT wxModalDialogHookExitGuard(wxDialog* dialog)
        : m_dialog(dialog)
    {
    }

    ~wxModalDialogHookExitGuard()
    {
        wxModalDialogHook::CallExit(m_dialog);
    }

private:
    wxDialog* const m_dialog;

    wxDECLARE_NO_COPY_CLASS(wxModalDialogHookExitGuard);
};

// This macro needs to be used at the top of every implementation of
// ShowModal() in order for wxModalDialogHook to work.
#define WX_HOOK_MODAL_DIALOG()                                                \
    const int modalDialogHookRC = wxModalDialogHook::CallEnter(this);         \
    if ( modalDialogHookRC != wxID_NONE )                                     \
        return modalDialogHookRC;                                             \
    wxModalDialogHookExitGuard modalDialogHookExit(this)

#endif // _WX_MODALHOOK_H_
