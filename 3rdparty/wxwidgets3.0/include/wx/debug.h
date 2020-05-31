/////////////////////////////////////////////////////////////////////////////
// Name:        wx/debug.h
// Purpose:     Misc debug functions and macros
// Author:      Vadim Zeitlin
// Created:     29/01/98
// Copyright:   (c) 1998-2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DEBUG_H_
#define _WX_DEBUG_H_

#if !defined(__WXWINCE__)
    #include  <assert.h>
#endif // systems without assert.h

#include <limits.h>          // for CHAR_BIT used below

#include "wx/chartype.h"     // for __TFILE__ and wxChar
#include "wx/cpp.h"          // for __WXFUNCTION__
#include "wx/dlimpexp.h"     // for WXDLLIMPEXP_FWD_BASE

class WXDLLIMPEXP_FWD_BASE wxString;
class WXDLLIMPEXP_FWD_BASE wxCStrData;

// ----------------------------------------------------------------------------
// Defines controlling the debugging macros
// ----------------------------------------------------------------------------

/*
    wxWidgets can be built with several different levels of debug support
    specified by the value of wxDEBUG_LEVEL constant:

    0:  No assertion macros at all, this should only be used when optimizing
        for resource-constrained systems (typically embedded ones).
    1:  Default level, most of the assertions are enabled.
    2:  Maximal (at least for now): asserts which are "expensive"
        (performance-wise) or only make sense for finding errors in wxWidgets
        itself, as opposed to bugs in applications using it, are also enabled.
 */

// unless wxDEBUG_LEVEL is predefined (by configure or via wx/setup.h under
// Windows), use the default
#if !defined(wxDEBUG_LEVEL)
    #define wxDEBUG_LEVEL 1
#endif // !defined(wxDEBUG_LEVEL)

/*
    __WXDEBUG__ is defined when wxDEBUG_LEVEL != 0. This is done mostly for
    compatibility but it also provides a simpler way to check if asserts and
    debug logging is enabled at all.
 */
#if wxDEBUG_LEVEL > 0
    #ifndef __WXDEBUG__
        #define __WXDEBUG__
    #endif
#else
    #undef __WXDEBUG__
#endif

// Finally there is also a very old WXDEBUG macro not used anywhere at all, it
// is only defined for compatibility.
#ifdef __WXDEBUG__
    #if !defined(WXDEBUG) || !WXDEBUG
        #undef WXDEBUG
        #define WXDEBUG 1
    #endif // !WXDEBUG
#endif // __WXDEBUG__

// ----------------------------------------------------------------------------
// Handling assertion failures
// ----------------------------------------------------------------------------

/*
    Type for the function called in case of assert failure, see
    wxSetAssertHandler().
 */
typedef void (*wxAssertHandler_t)(const wxString& file,
                                  int line,
                                  const wxString& func,
                                  const wxString& cond,
                                  const wxString& msg);

#if wxDEBUG_LEVEL

// the global assert handler function, if it is NULL asserts don't check their
// conditions
extern WXDLLIMPEXP_DATA_BASE(wxAssertHandler_t) wxTheAssertHandler;

/*
    Sets the function to be called in case of assertion failure.

    The default assert handler forwards to wxApp::OnAssertFailure() whose
    default behaviour is, in turn, to show the standard assertion failure
    dialog if a wxApp object exists or shows the same dialog itself directly
    otherwise.

    While usually it is enough -- and more convenient -- to just override
    OnAssertFailure(), to handle all assertion failures, including those
    occurring even before wxApp object creation or after its destruction you
    need to provide your assertion handler function.

    This function also provides a simple way to disable all asserts: simply
    pass NULL pointer to it. Doing this will result in not even evaluating
    assert conditions at all, avoiding almost all run-time cost of asserts.

    Notice that this function is not MT-safe, so you should call it before
    starting any other threads.

    The return value of this function is the previous assertion handler. It can
    be called after any pre-processing by your handler and can also be restored
    later if you uninstall your handler.
 */
inline wxAssertHandler_t wxSetAssertHandler(wxAssertHandler_t handler)
{
    const wxAssertHandler_t old = wxTheAssertHandler;
    wxTheAssertHandler = handler;
    return old;
}

/*
    Reset the default assert handler.

    This may be used to enable asserts, which are disabled by default in this
    case, for programs built in release build (NDEBUG defined).
 */
extern void WXDLLIMPEXP_BASE wxSetDefaultAssertHandler();

#else // !wxDEBUG_LEVEL

// provide empty stubs in case assertions are completely disabled
//
// NB: can't use WXUNUSED() here as we're included from wx/defs.h before it is
//     defined
inline wxAssertHandler_t wxSetAssertHandler(wxAssertHandler_t /* handler */)
{
    return NULL;
}

inline void wxSetDefaultAssertHandler() { }

#endif // wxDEBUG_LEVEL/!wxDEBUG_LEVEL

// simply a synonym for wxSetAssertHandler(NULL)
inline void wxDisableAsserts() { wxSetAssertHandler(NULL); }

/*
    A macro which disables asserts for applications compiled in release build.

    By default, wxIMPLEMENT_APP (or rather wxIMPLEMENT_WXWIN_MAIN) disable the
    asserts in the applications compiled in the release build by calling this.
    It does nothing if NDEBUG is not defined.
 */
#ifdef NDEBUG
    #define wxDISABLE_ASSERTS_IN_RELEASE_BUILD() wxDisableAsserts()
#else
    #define wxDISABLE_ASSERTS_IN_RELEASE_BUILD()
#endif

#if wxDEBUG_LEVEL

/*
    wxOnAssert() is used by the debugging macros defined below. Different
    overloads are needed because these macros can be used with or without wxT().

    All of them are implemented in src/common/appcmn.cpp and unconditionally
    call wxTheAssertHandler so the caller must check that it is non-NULL
    (assert macros do it).
 */

#if wxUSE_UNICODE

// these overloads are the ones typically used by debugging macros: we have to
// provide wxChar* msg version because it's common to use wxT() in the macros
// and finally, we can't use const wx(char)* msg = NULL, because that would
// be ambiguous
//
// also notice that these functions can't be inline as wxString is not defined
// yet (and can't be as wxString code itself may use assertions)
extern WXDLLIMPEXP_BASE void wxOnAssert(const char *file,
                                        int line,
                                        const char *func,
                                        const char *cond);

extern WXDLLIMPEXP_BASE void wxOnAssert(const char *file,
                                        int line,
                                        const char *func,
                                        const char *cond,
                                        const char *msg);

extern WXDLLIMPEXP_BASE void wxOnAssert(const char *file,
                                        int line,
                                        const char *func,
                                        const char *cond,
                                        const wxChar *msg) ;
#endif /* wxUSE_UNICODE */

// this version is for compatibility with wx 2.8 Unicode build only, we don't
// use it ourselves any more except in ANSI-only build in which case it is all
// we need
extern WXDLLIMPEXP_BASE void wxOnAssert(const wxChar *file,
                                        int line,
                                        const char *func,
                                        const wxChar *cond,
                                        const wxChar *msg = NULL);

// these overloads work when msg passed to debug macro is a string and we
// also have to provide wxCStrData overload to resolve ambiguity which would
// otherwise arise from wxASSERT( s.c_str() )
extern WXDLLIMPEXP_BASE void wxOnAssert(const wxString& file,
                                        int line,
                                        const wxString& func,
                                        const wxString& cond,
                                        const wxString& msg);

extern WXDLLIMPEXP_BASE void wxOnAssert(const wxString& file,
                                        int line,
                                        const wxString& func,
                                        const wxString& cond);

extern WXDLLIMPEXP_BASE void wxOnAssert(const char *file,
                                        int line,
                                        const char *func,
                                        const char *cond,
                                        const wxCStrData& msg);

extern WXDLLIMPEXP_BASE void wxOnAssert(const char *file,
                                        int line,
                                        const char *func,
                                        const char *cond,
                                        const wxString& msg);

#endif // wxDEBUG_LEVEL


// ----------------------------------------------------------------------------
// Debugging macros
// ----------------------------------------------------------------------------

/*
    Assertion macros: check if the condition is true and call assert handler
    (which will by default notify the user about failure) if it isn't.

    wxASSERT and wxFAIL macros as well as wxTrap() function do nothing at all
    if wxDEBUG_LEVEL is 0 however they do check their conditions at default
    debug level 1, unlike the previous wxWidgets versions.

    wxASSERT_LEVEL_2 is meant to be used for "expensive" asserts which should
    normally be disabled because they have a big impact on performance and so
    this macro only does anything if wxDEBUG_LEVEL >= 2.
 */
#if wxDEBUG_LEVEL
    // wxTrap() can be used to break into the debugger unconditionally
    // (assuming the program is running under debugger, of course).
    //
    // If possible, we prefer to define it as a macro rather than as a function
    // to open the debugger at the position where we trapped and not inside the
    // trap function itself which is not very useful.
    #if wxCHECK_VISUALC_VERSION(7)
        #define wxTrap() __debugbreak()
    #else
        extern WXDLLIMPEXP_BASE void wxTrap();
    #endif // Win VisualC

    // Global flag used to indicate that assert macros should call wxTrap(): it
    // is set by the default assert handler if the user answers yes to the
    // question of whether to trap.
    extern WXDLLIMPEXP_DATA_BASE(bool) wxTrapInAssert;

    // This macro checks if the condition is true and calls the assert handler
    // with the provided message if it isn't and finally traps if the special
    // flag indicating that it should do it was set by the handler.
    //
    // Notice that we don't use the handler return value for compatibility
    // reasons (if we changed its return type, we'd need to change wxApp::
    // OnAssertFailure() too which would break user code overriding it), hence
    // the need for the ugly global flag.
    #define wxASSERT_MSG(cond, msg)                                           \
        wxSTATEMENT_MACRO_BEGIN                                               \
            if ( wxTheAssertHandler && !(cond) &&                             \
                    (wxOnAssert(__FILE__, __LINE__, __WXFUNCTION__,           \
                                #cond, msg), wxTrapInAssert) )                \
            {                                                                 \
                wxTrapInAssert = false;                                       \
                wxTrap();                                                     \
            }                                                                 \
        wxSTATEMENT_MACRO_END

    // a version without any additional message, don't use unless condition
    // itself is fully self-explanatory
    #define wxASSERT(cond) wxASSERT_MSG(cond, (const char*)NULL)

    // wxFAIL is a special form of assert: it always triggers (and so is
    // usually used in normally unreachable code)
    #define wxFAIL_COND_MSG(cond, msg)                                        \
        wxSTATEMENT_MACRO_BEGIN                                               \
            if ( wxTheAssertHandler &&                                        \
                    (wxOnAssert(__FILE__, __LINE__, __WXFUNCTION__,           \
                                cond, msg), wxTrapInAssert) )                \
            {                                                                 \
                wxTrapInAssert = false;                                       \
                wxTrap();                                                     \
            }                                                                 \
        wxSTATEMENT_MACRO_END

    #define wxFAIL_MSG(msg) wxFAIL_COND_MSG("Assert failure", msg)
    #define wxFAIL wxFAIL_MSG((const char*)NULL)
#else // !wxDEBUG_LEVEL
    #define wxTrap()

    #define wxASSERT(cond)
    #define wxASSERT_MSG(cond, msg)
    #define wxFAIL
    #define wxFAIL_MSG(msg)
    #define wxFAIL_COND_MSG(cond, msg)
#endif  // wxDEBUG_LEVEL

#if wxDEBUG_LEVEL >= 2
    #define wxASSERT_LEVEL_2_MSG(cond, msg) wxASSERT_MSG(cond, msg)
    #define wxASSERT_LEVEL_2(cond) wxASSERT(cond)
#else // wxDEBUG_LEVEL < 2
    #define wxASSERT_LEVEL_2_MSG(cond, msg)
    #define wxASSERT_LEVEL_2(cond)
#endif

// This is simply a wrapper for the standard abort() which is not available
// under all platforms.
//
// It isn't really debug-related but there doesn't seem to be any better place
// for it, so declare it here and define it in appbase.cpp, together with
// wxTrap().
extern void WXDLLIMPEXP_BASE wxAbort();

/*
    wxCHECK macros always check their conditions, setting debug level to 0 only
    makes them silent in case of failure, otherwise -- including at default
    debug level 1 -- they call the assert handler if the condition is false

    They are supposed to be used only in invalid situation: for example, an
    invalid parameter (e.g. a NULL pointer) is passed to a function. Instead of
    dereferencing it and causing core dump the function might use

        wxCHECK_RET( p != NULL, "pointer can't be NULL" )
*/

// the generic macro: takes the condition to check, the statement to be executed
// in case the condition is false and the message to pass to the assert handler
#define wxCHECK2_MSG(cond, op, msg)                                       \
    if ( cond )                                                           \
    {}                                                                    \
    else                                                                  \
    {                                                                     \
        wxFAIL_COND_MSG(#cond, msg);                                      \
        op;                                                               \
    }                                                                     \
    struct wxDummyCheckStruct /* just to force a semicolon */

// check which returns with the specified return code if the condition fails
#define wxCHECK_MSG(cond, rc, msg)   wxCHECK2_MSG(cond, return rc, msg)

// check that expression is true, "return" if not (also FAILs in debug mode)
#define wxCHECK(cond, rc)            wxCHECK_MSG(cond, rc, (const char*)NULL)

// check that expression is true, perform op if not
#define wxCHECK2(cond, op)           wxCHECK2_MSG(cond, op, (const char*)NULL)

// special form of wxCHECK2: as wxCHECK, but for use in void functions
//
// NB: there is only one form (with msg parameter) and it's intentional:
//     there is no other way to tell the caller what exactly went wrong
//     from the void function (of course, the function shouldn't be void
//     to begin with...)
#define wxCHECK_RET(cond, msg)       wxCHECK2_MSG(cond, return, msg)


// ----------------------------------------------------------------------------
// Compile time asserts
//
// Unlike the normal assert and related macros above which are checked during
// the program run-time the macros below will result in a compilation error if
// the condition they check is false. This is usually used to check the
// expressions containing sizeof()s which cannot be tested with the
// preprocessor. If you can use the #if's, do use them as you can give a more
// detailed error message then.
// ----------------------------------------------------------------------------

/*
  How this works (you don't have to understand it to be able to use the
  macros): we rely on the fact that it is invalid to define a named bit field
  in a struct of width 0. All the rest are just the hacks to minimize the
  possibility of the compiler warnings when compiling this macro: in
  particular, this is why we define a struct and not an object (which would
  result in a warning about unused variable) and a named struct (otherwise we'd
  get a warning about an unnamed struct not used to define an object!).
 */

#define wxMAKE_UNIQUE_ASSERT_NAME           wxMAKE_UNIQUE_NAME(wxAssert_)

/*
  The second argument of this macro must be a valid C++ identifier and not a
  string. I.e. you should use it like this:

    wxCOMPILE_TIME_ASSERT( sizeof(int) >= 2, YourIntsAreTooSmall );

 It may be used both within a function and in the global scope.
*/
#if defined(__WATCOMC__)
    /* avoid "unused symbol" warning */
    #define wxCOMPILE_TIME_ASSERT(expr, msg) \
        class wxMAKE_UNIQUE_ASSERT_NAME { \
          unsigned int msg: expr; \
          wxMAKE_UNIQUE_ASSERT_NAME() { wxUnusedVar(msg); } \
        }
#elif defined( __VMS )
namespace wxdebug{

// HP aCC cannot deal with missing names for template value parameters
template <bool x> struct STATIC_ASSERTION_FAILURE;

template <> struct STATIC_ASSERTION_FAILURE<true> { enum { value = 1 }; };

// HP aCC cannot deal with missing names for template value parameters
template<int x> struct static_assert_test{};

}
    #define WX_JOIN( X, Y ) X##Y
    #define WX_STATIC_ASSERT_BOOL_CAST(x) (bool)(x)
    #define wxCOMPILE_TIME_ASSERT(expr, msg) \
       typedef ::wxdebug::static_assert_test<\
          sizeof(::wxdebug::STATIC_ASSERTION_FAILURE< WX_STATIC_ASSERT_BOOL_CAST( expr ) >)>\
            WX_JOIN(wx_static_assert_typedef_, __LINE__)
#else
    #define wxCOMPILE_TIME_ASSERT(expr, msg) \
        struct wxMAKE_UNIQUE_ASSERT_NAME { unsigned int msg: expr; }
#endif

/*
   When using VC++ 6 with "Edit and Continue" on, the compiler completely
   mishandles __LINE__ and so wxCOMPILE_TIME_ASSERT() doesn't work, provide a
   way to make "unique" assert names by specifying a unique prefix explicitly
 */
#define wxMAKE_UNIQUE_ASSERT_NAME2(text) wxCONCAT(wxAssert_, text)

#define wxCOMPILE_TIME_ASSERT2(expr, msg, text) \
    struct wxMAKE_UNIQUE_ASSERT_NAME2(text) { unsigned int msg: expr; }

// helpers for wxCOMPILE_TIME_ASSERT below, for private use only
#define wxMAKE_BITSIZE_MSG(type, size) type ## SmallerThan ## size ## Bits

// a special case of compile time assert: check that the size of the given type
// is at least the given number of bits
#define wxASSERT_MIN_BITSIZE(type, size) \
    wxCOMPILE_TIME_ASSERT(sizeof(type) * CHAR_BIT >= size, \
                          wxMAKE_BITSIZE_MSG(type, size))


// ----------------------------------------------------------------------------
// other miscellaneous debugger-related functions
// ----------------------------------------------------------------------------

/*
    Return true if we're running under debugger.

    Currently only really works under Win32 and just returns false elsewhere.
 */
#if defined(__WIN32__)
    extern bool WXDLLIMPEXP_BASE wxIsDebuggerRunning();
#else // !Mac
    inline bool wxIsDebuggerRunning() { return false; }
#endif // Mac/!Mac

// An assert helper used to avoid warning when testing constant expressions,
// i.e. wxASSERT( sizeof(int) == 4 ) can generate a compiler warning about
// expression being always true, but not using
// wxASSERT( wxAssertIsEqual(sizeof(int), 4) )
//
// NB: this is made obsolete by wxCOMPILE_TIME_ASSERT() and should no
//     longer be used.
extern bool WXDLLIMPEXP_BASE wxAssertIsEqual(int x, int y);

// Use of wxFalse instead of false suppresses compiler warnings about testing
// constant expression
extern WXDLLIMPEXP_DATA_BASE(const bool) wxFalse;

#define wxAssertFailure wxFalse

// This is similar to WXUNUSED() and useful for parameters which are only used
// in assertions.
#if wxDEBUG_LEVEL
    #define WXUNUSED_UNLESS_DEBUG(param)  param
#else
    #define WXUNUSED_UNLESS_DEBUG(param)  WXUNUSED(param)
#endif


#endif // _WX_DEBUG_H_
