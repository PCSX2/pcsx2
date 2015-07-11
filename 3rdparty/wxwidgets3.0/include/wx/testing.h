/////////////////////////////////////////////////////////////////////////////
// Name:        wx/testing.h
// Purpose:     helpers for GUI testing
// Author:      Vaclav Slavik
// Created:     2012-08-28
// Copyright:   (c) 2012 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TESTING_H_
#define _WX_TESTING_H_

#include "wx/debug.h"
#include "wx/string.h"
#include "wx/modalhook.h"

class WXDLLIMPEXP_FWD_CORE wxMessageDialogBase;
class WXDLLIMPEXP_FWD_CORE wxFileDialogBase;

// ----------------------------------------------------------------------------
// testing API
// ----------------------------------------------------------------------------

// Don't include this code when building the library itself
#ifndef WXBUILDING

#include "wx/beforestd.h"
#include <algorithm>
#include <iterator>
#include <queue>
#include "wx/afterstd.h"
#include "wx/cpp.h"
#include "wx/dialog.h"
#include "wx/msgdlg.h"
#include "wx/filedlg.h"

class wxTestingModalHook;

// Non-template base class for wxExpectModal<T> (via wxExpectModalBase).
// Only used internally.
class wxModalExpectation
{
public:
    wxModalExpectation() : m_isOptional(false) {}
    virtual ~wxModalExpectation() {}

    bool IsOptional() const { return m_isOptional; }

    virtual int Invoke(wxDialog *dlg) const = 0;

    virtual wxString GetDescription() const = 0;

protected:
    // Is this dialog optional, i.e. not required to be shown?
    bool m_isOptional;
};


// This must be specialized for each type. The specialization MUST be derived
// from wxExpectModalBase<T>.
template<class T> class wxExpectModal {};


/**
    Base class for wxExpectModal<T> specializations.

    Every such specialization must be derived from wxExpectModalBase; there's
    no other use for this class than to serve as wxExpectModal<T>'s base class.

    T must be a class derived from wxDialog.
 */
template<class T>
class wxExpectModalBase : public wxModalExpectation
{
public:
    typedef T DialogType;
    typedef wxExpectModal<DialogType> ExpectationType;

    /**
        Returns a copy of the expectation where the expected dialog is marked
        as optional.

        Optional dialogs aren't required to appear, it's not an error if they
        don't.
     */
    ExpectationType Optional() const
    {
        ExpectationType e(*static_cast<const ExpectationType*>(this));
        e.m_isOptional = true;
        return e;
    }

protected:
    virtual int Invoke(wxDialog *dlg) const
    {
        DialogType *t = dynamic_cast<DialogType*>(dlg);
        if ( t )
            return OnInvoked(t);
        else
            return wxID_NONE; // not handled
    }

    /// Returns description of the expected dialog (by default, its class).
    virtual wxString GetDescription() const
    {
        return wxCLASSINFO(T)->GetClassName();
    }

    /**
        This method is called when ShowModal() was invoked on a dialog of type T.

        @return Return value is used as ShowModal()'s return value.
     */
    virtual int OnInvoked(DialogType *dlg) const = 0;
};


// wxExpectModal<T> specializations for common dialogs:

template<>
class wxExpectModal<wxMessageDialog> : public wxExpectModalBase<wxMessageDialog>
{
public:
    wxExpectModal(int id)
    {
        switch ( id )
        {
            case wxYES:
                m_id = wxID_YES;
                break;
            case wxNO:
                m_id = wxID_NO;
                break;
            case wxCANCEL:
                m_id = wxID_CANCEL;
                break;
            case wxOK:
                m_id = wxID_OK;
                break;
            case wxHELP:
                m_id = wxID_HELP;
                break;
            default:
                m_id = id;
                break;
        }
    }

protected:
    virtual int OnInvoked(wxMessageDialog *WXUNUSED(dlg)) const
    {
        return m_id;
    }

    int m_id;
};

#if wxUSE_FILEDLG

template<>
class wxExpectModal<wxFileDialog> : public wxExpectModalBase<wxFileDialog>
{
public:
    wxExpectModal(const wxString& path, int id = wxID_OK)
        : m_path(path), m_id(id)
    {
    }

protected:
    virtual int OnInvoked(wxFileDialog *dlg) const
    {
        dlg->SetPath(m_path);
        return m_id;
    }

    wxString m_path;
    int m_id;
};

#endif

// Implementation of wxModalDialogHook for use in testing, with
// wxExpectModal<T> and the wxTEST_DIALOG() macro. It is not intended for
// direct use, use the macro instead.
class wxTestingModalHook : public wxModalDialogHook
{
public:
    wxTestingModalHook()
    {
        Register();
    }

    // Called to verify that all expectations were met. This cannot be done in
    // the destructor, because ReportFailure() may throw (either because it's
    // overriden or because wx's assertions handling is, globally). And
    // throwing from the destructor would introduce all sort of problems,
    // including messing up the order of errors in some cases.
    void CheckUnmetExpectations()
    {
        while ( !m_expectations.empty() )
        {
            const wxModalExpectation *expect = m_expectations.front();
            m_expectations.pop();
            if ( expect->IsOptional() )
                continue;

            ReportFailure
            (
                wxString::Format
                (
                    "Expected %s dialog was not shown.",
                    expect->GetDescription()
                )
            );
            break;
        }
    }

    void AddExpectation(const wxModalExpectation& e)
    {
        m_expectations.push(&e);
    }

protected:
    virtual int Enter(wxDialog *dlg)
    {
        while ( !m_expectations.empty() )
        {
            const wxModalExpectation *expect = m_expectations.front();
            m_expectations.pop();

            int ret = expect->Invoke(dlg);
            if ( ret != wxID_NONE )
                return ret; // dialog shown as expected

            // not showing an optional dialog is OK, but showing an unexpected
            // one definitely isn't:
            if ( !expect->IsOptional() )
            {
                ReportFailure
                (
                    wxString::Format
                    (
                        "A %s dialog was shown unexpectedly, expected %s.",
                        dlg->GetClassInfo()->GetClassName(),
                        expect->GetDescription()
                    )
                );
                return wxID_NONE;
            }
            // else: try the next expectation in the chain
        }

        ReportFailure
        (
            wxString::Format
            (
                "A dialog (%s) was shown unexpectedly.",
                dlg->GetClassInfo()->GetClassName()
            )
        );
        return wxID_NONE;
    }

protected:
    virtual void ReportFailure(const wxString& msg)
    {
        wxFAIL_MSG( msg );
    }

private:
    std::queue<const wxModalExpectation*> m_expectations;

    wxDECLARE_NO_COPY_CLASS(wxTestingModalHook);
};


// Redefining this value makes it possible to customize the hook class,
// including e.g. its error reporting.
#define wxTEST_DIALOG_HOOK_CLASS wxTestingModalHook

#define WX_TEST_IMPL_ADD_EXPECTATION(pos, expect)                              \
    const wxModalExpectation& wx_exp##pos = expect;                            \
    wx_hook.AddExpectation(wx_exp##pos);

/**
    Runs given code with all modal dialogs redirected to wxExpectModal<T>
    hooks, instead of being shown to the user.

    The first argument is any valid expression, typically a function call. The
    remaining arguments are wxExpectModal<T> instances defining the dialogs
    that are expected to be shown, in order of appearance.

    Some typical examples:

    @code
    wxTEST_DIALOG
    (
        rc = dlg.ShowModal(),
        wxExpectModal<wxFileDialog>(wxGetCwd() + "/test.txt")
    );
    @endcode

    Sometimes, the code may show more than one dialog:

    @code
    wxTEST_DIALOG
    (
        RunSomeFunction(),
        wxExpectModal<wxMessageDialog>(wxNO),
        wxExpectModal<MyConfirmationDialog>(wxYES),
        wxExpectModal<wxFileDialog>(wxGetCwd() + "/test.txt")
    );
    @endcode

    Notice that wxExpectModal<T> has some convenience methods for further
    tweaking the expectations. For example, it's possible to mark an expected
    dialog as @em optional for situations when a dialog may be shown, but isn't
    required to, by calling the Optional() method:

    @code
    wxTEST_DIALOG
    (
        RunSomeFunction(),
        wxExpectModal<wxMessageDialog>(wxNO),
        wxExpectModal<wxFileDialog>(wxGetCwd() + "/test.txt").Optional()
    );
    @endcode

    @note By default, errors are reported with wxFAIL_MSG(). You may customize this by
          implementing a class derived from wxTestingModalHook, overriding its
          ReportFailure() method and redefining the wxTEST_DIALOG_HOOK_CLASS
          macro to be the name of this class.

    @note Custom dialogs are supported too. All you have to do is to specialize
          wxExpectModal<> for your dialog type and implement its OnInvoked()
          method.
 */
#ifdef HAVE_VARIADIC_MACROS
#define wxTEST_DIALOG(codeToRun, ...)                                          \
    {                                                                          \
        wxTEST_DIALOG_HOOK_CLASS wx_hook;                                      \
        wxCALL_FOR_EACH(WX_TEST_IMPL_ADD_EXPECTATION, __VA_ARGS__)             \
        codeToRun;                                                             \
        wx_hook.CheckUnmetExpectations();                                      \
    }
#endif /* HAVE_VARIADIC_MACROS */

#endif // !WXBUILDING

#endif // _WX_TESTING_H_
