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

#include <typeinfo>

class wxTestingModalHook;

// This helper is used to construct the best possible name for the dialog of
// the given type using wxRTTI for this type, if any, and the C++ RTTI for
// either the type T statically or the dynamic type of "dlg" if it's non-null.
template <class T>
wxString wxGetDialogClassDescription(const wxClassInfo *ci, T* dlg = NULL)
{
    // We prefer to use the name from wxRTTI as it's guaranteed to be readable,
    // unlike the name returned by type_info::name() which may need to be
    // demangled, but if wxRTTI macros were not used for this object, it's
    // better to return a not-very-readable-but-informative mangled name rather
    // than a readable but useless "wxDialog".
    if ( ci == wxCLASSINFO(wxDialog) )
    {
        return wxString::Format("dialog of type \"%s\"",
                                (dlg ? typeid(*dlg) : typeid(T)).name());
    }

    // We consider that an unmangled name is clear enough to be used on its own.
    return ci->GetClassName();
}

// Non-template base class for wxExpectModal<T> (via wxExpectModalBase).
// Only used internally.
class wxModalExpectation
{
public:
    wxModalExpectation() : m_isOptional(false) {}
    virtual ~wxModalExpectation() {}

    wxString GetDescription() const
    {
        return m_description.empty() ? GetDefaultDescription() : m_description;
    }

    bool IsOptional() const { return m_isOptional; }

    virtual int Invoke(wxDialog *dlg) const = 0;

protected:
    // Override to return the default description of the expected dialog used
    // if no specific description for this particular expectation is given.
    virtual wxString GetDefaultDescription() const = 0;

    // User-provided description of the dialog, may be empty.
    wxString m_description;

    // Is this dialog optional, i.e. not required to be shown?
    bool m_isOptional;
};


// This template is specialized for some of the standard dialog classes and can
// also be specialized outside of the library for the custom dialogs.
//
// All specializations must derive from wxExpectModalBase<T>.
template<class T> class wxExpectModal;


/**
    Base class for the expectation of a dialog of the given type T.

    Test code can derive ad hoc classes from this class directly and implement
    its OnInvoked() to perform the necessary actions or derive wxExpectModal<T>
    and implement it once if the implementation of OnInvoked() is always the
    same, i.e. depends just on the type T.

    T must be a class derived from wxDialog and E is the derived class type,
    i.e. this is an example of using CRTP. The default value of E is fine in
    case you're using this class as a base for your wxExpectModal<>
    specialization anyhow but also if you don't use neither Optional() nor
    Describe() methods, as the derived class type is only needed for them.
 */
template<class T, class E = wxExpectModal<T> >
class wxExpectModalBase : public wxModalExpectation
{
public:
    typedef T DialogType;
    typedef E ExpectationType;


    // A note about these "modifier" methods: they return copies of this object
    // and not a reference to the object itself (after modifying it) because
    // this object is likely to be temporary and will be destroyed soon, while
    // the new temporary created by these objects is bound to a const reference
    // inside WX_TEST_IMPL_ADD_EXPECTATION() macro ensuring that its lifetime
    // is prolonged until we can check if the expectations were met.
    //
    // This is also the reason these methods must be in this class and use
    // CRTP: a copy of this object can't be created in the base class, which is
    // abstract, and the copy must have the same type as the derived object to
    // avoid slicing.
    //
    // Make sure you understand this comment in its entirety before considering
    // modifying this code.


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

    /**
        Sets a description shown in the error message if the expectation fails.

        Using this method with unique descriptions for the different dialogs is
        recommended to make it easier to find out which one of the expected
        dialogs exactly was not shown.
     */
    ExpectationType Describe(const wxString& description) const
    {
        ExpectationType e(*static_cast<const ExpectationType*>(this));
        e.m_description = description;
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
    virtual wxString GetDefaultDescription() const
    {
        return wxGetDialogClassDescription<T>(wxCLASSINFO(T));
    }

    /**
        This method is called when ShowModal() was invoked on a dialog of type T.

        @return Return value is used as ShowModal()'s return value.
     */
    virtual int OnInvoked(DialogType *dlg) const = 0;
};


// wxExpectModal<T> specializations for common dialogs:

template<class T>
class wxExpectDismissableModal
    : public wxExpectModalBase<T, wxExpectDismissableModal<T> >
{
public:
    explicit wxExpectDismissableModal(int id)
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
    virtual int OnInvoked(T *WXUNUSED(dlg)) const
    {
        return m_id;
    }

    int m_id;
};

template<>
class wxExpectModal<wxMessageDialog>
    : public wxExpectDismissableModal<wxMessageDialog>
{
public:
    explicit wxExpectModal(int id)
        : wxExpectDismissableModal<wxMessageDialog>(id)
    {
    }

protected:
    virtual wxString GetDefaultDescription() const
    {
        // It can be useful to show which buttons the expected message box was
        // supposed to have, in case there could have been several of them.
        wxString details;
        switch ( m_id )
        {
            case wxID_YES:
            case wxID_NO:
                details = "wxYES_NO style";
                break;

            case wxID_CANCEL:
                details = "wxCANCEL style";
                break;

            case wxID_OK:
                details = "wxOK style";
                break;

            default:
                details.Printf("a button with ID=%d", m_id);
                break;
        }

        return "wxMessageDialog with " + details;
    }
};

class wxExpectAny : public wxExpectDismissableModal<wxDialog>
{
public:
    explicit wxExpectAny(int id)
        : wxExpectDismissableModal<wxDialog>(id)
    {
    }
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
    // This object is created with the location of the macro containing it by
    // wxTEST_DIALOG macro, otherwise it falls back to the location of this
    // line itself, which is not very useful, so normally you should provide
    // your own values.
    wxTestingModalHook(const char* file = NULL,
                       int line = 0,
                       const char* func = NULL)
        : m_file(file), m_line(line), m_func(func)
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
                    "Expected %s was not shown.",
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
                        "%s was shown unexpectedly, expected %s.",
                        DescribeUnexpectedDialog(dlg),
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
                "%s was shown unexpectedly.",
                DescribeUnexpectedDialog(dlg)
            )
        );
        return wxID_NONE;
    }

protected:
    // This method may be overridden to provide a better description of
    // (unexpected) dialogs, e.g. add knowledge of custom dialogs used by the
    // program here.
    virtual wxString DescribeUnexpectedDialog(wxDialog* dlg) const
    {
        // Message boxes are handled specially here just because they are so
        // ubiquitous.
        if ( wxMessageDialog *msgdlg = dynamic_cast<wxMessageDialog*>(dlg) )
        {
            return wxString::Format
                   (
                        "A message box \"%s\"",
                        msgdlg->GetMessage()
                   );
        }

        return wxString::Format
               (
                    "A %s with title \"%s\"",
                    wxGetDialogClassDescription(dlg->GetClassInfo(), dlg),
                    dlg->GetTitle()
               );
    }

    // This method may be overridden to change the way test failures are
    // handled. By default they result in an assertion failure which, of
    // course, can itself be customized.
    virtual void ReportFailure(const wxString& msg)
    {
        wxFAIL_MSG_AT( msg,
                       m_file ? m_file : __FILE__,
                       m_line ? m_line : __LINE__,
                       m_func ? m_func : __WXFUNCTION__ );
    }

private:
    const char* const m_file;
    const int m_line;
    const char* const m_func;

    std::queue<const wxModalExpectation*> m_expectations;

    wxDECLARE_NO_COPY_CLASS(wxTestingModalHook);
};


// Redefining this value makes it possible to customize the hook class,
// including e.g. its error reporting.
#ifndef wxTEST_DIALOG_HOOK_CLASS
    #define wxTEST_DIALOG_HOOK_CLASS wxTestingModalHook
#endif

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

// See wx/cpp.h for the explanations of this hack.
#if defined(__GNUC__) && __GNUC__ == 3
    #pragma GCC system_header
#endif /* gcc-3.x */

#define wxTEST_DIALOG(codeToRun, ...)                                          \
    {                                                                          \
        wxTEST_DIALOG_HOOK_CLASS wx_hook(__FILE__, __LINE__, __WXFUNCTION__);  \
        wxCALL_FOR_EACH(WX_TEST_IMPL_ADD_EXPECTATION, __VA_ARGS__)             \
        codeToRun;                                                             \
        wx_hook.CheckUnmetExpectations();                                      \
    }
#endif /* HAVE_VARIADIC_MACROS */

#endif // !WXBUILDING

#endif // _WX_TESTING_H_
