/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cppunit.h
// Purpose:     wrapper header for CppUnit headers
// Author:      Vadim Zeitlin
// Created:     15.02.04
// Copyright:   (c) 2004 Vadim Zeitlin
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CPPUNIT_H_
#define _WX_CPPUNIT_H_

///////////////////////////////////////////////////////////////////////////////
// using CPPUNIT_TEST() macro results in this warning, disable it as there is
// no other way to get rid of it and it's not very useful anyhow
#ifdef __VISUALC__
    // typedef-name 'foo' used as synonym for class-name 'bar'
    #pragma warning(disable:4097)

    // unreachable code: we don't care about warnings in CppUnit headers
    #pragma warning(disable:4702)

    // 'id': identifier was truncated to 'num' characters in the debug info
    #pragma warning(disable:4786)
#endif // __VISUALC__

#ifdef __BORLANDC__
    #pragma warn -8022
#endif

#ifndef CPPUNIT_STD_NEED_ALLOCATOR
    #define CPPUNIT_STD_NEED_ALLOCATOR 0
#endif

///////////////////////////////////////////////////////////////////////////////
// Set the default format for the errors, which can be used by an IDE to jump
// to the error location. This default gets overridden by the cppunit headers
// for some compilers (e.g. VC++).

#ifndef CPPUNIT_COMPILER_LOCATION_FORMAT
    #define CPPUNIT_COMPILER_LOCATION_FORMAT "%p:%l:"
#endif


///////////////////////////////////////////////////////////////////////////////
// Include all needed cppunit headers.
//

#include "wx/beforestd.h"
#ifdef __VISUALC__
    #pragma warning(push)

    // with cppunit 1.12 we get many bogus warnings 4701 (local variable may be
    // used without having been initialized) in TestAssert.h
    #pragma warning(disable:4701)

    // and also 4100 (unreferenced formal parameter) in extensions/
    // ExceptionTestCaseDecorator.h
    #pragma warning(disable:4100)
#endif

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/TestCase.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/CompilerOutputter.h>

#ifdef __VISUALC__
    #pragma warning(pop)
#endif
#include "wx/afterstd.h"

#include "wx/string.h"


///////////////////////////////////////////////////////////////////////////////
// Set of helpful test macros.
//

// Base macro for wrapping CPPUNIT_TEST macros and so making them conditional
// tests, meaning that the test only get registered and thus run when a given
// runtime condition is true.
// In case the condition is evaluated as false a skip message is logged
// (the message will only be shown in verbose mode).
#define WXTEST_ANY_WITH_CONDITION(suiteName, Condition, testMethod, anyTest) \
    if (Condition) \
        { anyTest; } \
    else \
        wxLogInfo(wxString::Format(wxT("skipping: %s.%s\n  reason: %s equals false\n"), \
                                    wxString(suiteName, wxConvUTF8).c_str(), \
                                    wxString(#testMethod, wxConvUTF8).c_str(), \
                                    wxString(#Condition, wxConvUTF8).c_str()))

// Conditional CPPUNIT_TEST macro.
#define WXTEST_WITH_CONDITION(suiteName, Condition, testMethod) \
    WXTEST_ANY_WITH_CONDITION(suiteName, Condition, testMethod, CPPUNIT_TEST(testMethod))
// Conditional CPPUNIT_TEST_FAIL macro.
#define WXTEST_FAIL_WITH_CONDITION(suiteName, Condition, testMethod) \
    WXTEST_ANY_WITH_CONDITION(suiteName, Condition, testMethod, CPPUNIT_TEST_FAIL(testMethod))

CPPUNIT_NS_BEGIN

// provide an overload of cppunit assertEquals(T, T) which can be used to
// compare wxStrings directly with C strings
inline void
assertEquals(const char *expected,
             const char *actual,
             CppUnit::SourceLine sourceLine,
             const std::string& message)
{
    assertEquals(wxString(expected), wxString(actual), sourceLine, message);
}

inline void
assertEquals(const char *expected,
             const wxString& actual,
             CppUnit::SourceLine sourceLine,
             const std::string& message)
{
    assertEquals(wxString(expected), actual, sourceLine, message);
}

inline void
assertEquals(const wxString& expected,
             const char *actual,
             CppUnit::SourceLine sourceLine,
             const std::string& message)
{
    assertEquals(expected, wxString(actual), sourceLine, message);
}

inline void
assertEquals(const wchar_t *expected,
             const wxString& actual,
             CppUnit::SourceLine sourceLine,
             const std::string& message)
{
    assertEquals(wxString(expected), actual, sourceLine, message);
}

inline void
assertEquals(const wxString& expected,
             const wchar_t *actual,
             CppUnit::SourceLine sourceLine,
             const std::string& message)
{
    assertEquals(expected, wxString(actual), sourceLine, message);
}

CPPUNIT_NS_END

// define an assertEquals() overload for the given types, this is a helper and
// shouldn't be used directly because of VC6 complications, see below
#define WX_CPPUNIT_ASSERT_EQUALS(T1, T2)                                      \
    inline void                                                               \
    assertEquals(T1 expected,                                                 \
                 T2 actual,                                                   \
                 CppUnit::SourceLine sourceLine,                              \
                 const std::string& message)                                  \
    {                                                                         \
        if ( !assertion_traits<T1>::equal(expected,actual) )                  \
        {                                                                     \
            Asserter::failNotEqual( assertion_traits<T1>::toString(expected), \
                                    assertion_traits<T2>::toString(actual),   \
                                    sourceLine,                               \
                                    message );                                \
        }                                                                     \
    }

// this macro allows us to specify (usually literal) ints as expected values
// for functions returning integral types different from "int"
//
// FIXME-VC6: due to incorrect resolution of overloaded/template functions in
//            this compiler (it basically doesn't use the template version at
//            all if any overloaded function matches partially even if none of
//            them matches fully) we also need to provide extra overloads

#ifdef __VISUALC6__
    #define WX_CPPUNIT_ALLOW_EQUALS_TO_INT(T) \
        CPPUNIT_NS_BEGIN \
            WX_CPPUNIT_ASSERT_EQUALS(int, T) \
            WX_CPPUNIT_ASSERT_EQUALS(T, int) \
            WX_CPPUNIT_ASSERT_EQUALS(T, T) \
        CPPUNIT_NS_END

    CPPUNIT_NS_BEGIN
        WX_CPPUNIT_ASSERT_EQUALS(int, int)
    CPPUNIT_NS_END
#else // !VC6
    #define WX_CPPUNIT_ALLOW_EQUALS_TO_INT(T) \
        CPPUNIT_NS_BEGIN \
            WX_CPPUNIT_ASSERT_EQUALS(int, T) \
            WX_CPPUNIT_ASSERT_EQUALS(T, int) \
        CPPUNIT_NS_END
#endif // VC6/!VC6

WX_CPPUNIT_ALLOW_EQUALS_TO_INT(long)
WX_CPPUNIT_ALLOW_EQUALS_TO_INT(short)
WX_CPPUNIT_ALLOW_EQUALS_TO_INT(unsigned)
WX_CPPUNIT_ALLOW_EQUALS_TO_INT(unsigned long)

#if defined( __VMS ) && defined( __ia64 )
WX_CPPUNIT_ALLOW_EQUALS_TO_INT(std::basic_streambuf<char>::pos_type);
#endif

#ifdef wxHAS_LONG_LONG_T_DIFFERENT_FROM_LONG
WX_CPPUNIT_ALLOW_EQUALS_TO_INT(wxLongLong_t)
WX_CPPUNIT_ALLOW_EQUALS_TO_INT(unsigned wxLongLong_t)
#endif // wxHAS_LONG_LONG_T_DIFFERENT_FROM_LONG

// Use this macro to compare a wxArrayString with the pipe-separated elements
// of the given string
//
// NB: it's a macro and not a function to have the correct line numbers in the
//     test failure messages
#define WX_ASSERT_STRARRAY_EQUAL(s, a)                                        \
    {                                                                         \
        wxArrayString expected(wxSplit(s, '|', '\0'));                        \
                                                                              \
        CPPUNIT_ASSERT_EQUAL( expected.size(), a.size() );                    \
                                                                              \
        for ( size_t n = 0; n < a.size(); n++ )                               \
        {                                                                     \
            CPPUNIT_ASSERT_EQUAL( expected[n], a[n] );                        \
        }                                                                     \
    }

// Use this macro to assert with the given formatted message (it should contain
// the format string and arguments in a separate pair of parentheses)
#define WX_ASSERT_MESSAGE(msg, cond) \
    CPPUNIT_ASSERT_MESSAGE(std::string(wxString::Format msg .mb_str()), (cond))

#define WX_ASSERT_EQUAL_MESSAGE(msg, expected, actual) \
    CPPUNIT_ASSERT_EQUAL_MESSAGE(std::string(wxString::Format msg .mb_str()), \
                                 (expected), (actual))

///////////////////////////////////////////////////////////////////////////////
// define stream inserter for wxString if it's not defined in the main library,
// we need it to output the test failures involving wxString
#if !wxUSE_STD_IOSTREAM

#include "wx/string.h"

#include <iostream>

inline std::ostream& operator<<(std::ostream& o, const wxString& s)
{
#if wxUSE_UNICODE
    return o << (const char *)wxSafeConvertWX2MB(s.wc_str());
#else
    return o << s.c_str();
#endif
}

#endif // !wxUSE_STD_IOSTREAM

// VC6 doesn't provide overloads for operator<<(__int64) in its stream classes
// so do it ourselves
#if defined(__VISUALC6__) && defined(wxLongLong_t)

#include "wx/longlong.h"

inline std::ostream& operator<<(std::ostream& ostr, wxLongLong_t ll)
{
    ostr << wxLongLong(ll).ToString();

    return ostr;
}

inline std::ostream& operator<<(std::ostream& ostr, unsigned wxLongLong_t llu)
{
    ostr << wxULongLong(llu).ToString();

    return ostr;
}

#endif // VC6 && wxLongLong_t

///////////////////////////////////////////////////////////////////////////////
// Some more compiler warning tweaking and auto linking.
//

#ifdef __BORLANDC__
    #pragma warn .8022
#endif

#ifdef _MSC_VER
  #pragma warning(default:4702)
#endif // _MSC_VER

// for VC++ automatically link in cppunit library
#ifdef __VISUALC__
  #ifdef NDEBUG
    #pragma comment(lib, "cppunit.lib")
  #else // Debug
    #pragma comment(lib, "cppunitd.lib")
  #endif // Release/Debug
#endif

#endif // _WX_CPPUNIT_H_

