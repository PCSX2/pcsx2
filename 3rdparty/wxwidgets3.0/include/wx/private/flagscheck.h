/////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/flagscheck.h
// Purpose:     helpers for checking that (bit)flags don't overlap
// Author:      Vaclav Slavik
// Created:     2008-02-21
// Copyright:   (c) 2008 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_FLAGSCHECK_H_
#define _WX_PRIVATE_FLAGSCHECK_H_

#include "wx/debug.h"

// IBM xlC 8 can't parse the template syntax
#if !defined(__IBMCPP__)

#include "wx/meta/if.h"

namespace wxPrivate
{

// These templates are used to implement wxADD_FLAG macro below.
//
// The idea is that we want to trigger *compilation* error if the flags
// overlap, not just runtime assert failure. We can't implement the check
// using just a simple logical operation, we need checks equivalent to this
// code:
//
//   mask = wxFLAG_1;
//   assert( (mask & wxFLAG_2) == 0 ); // no overlap
//   mask |= wxFLAG_3;
//   assert( (mask & wxFLAG_3) == 0 ); // no overlap
//   mask |= wxFLAG_3;
//   ...
//
// This can be done at compilation time by using templates metaprogramming
// technique that makes the compiler carry on the computation.
//
// NB: If any of this doesn't compile with your compiler and would be too
//     hard to make work, it's probably best to disable this code and replace
//     the macros below with empty stubs, this isn't anything critical.

template<int val> struct FlagsHaveConflictingValues
{
    // no value here - triggers compilation error
};

template<int val> struct FlagValue
{
    enum { value = val };
};

// This template adds its template parameter integer 'add' to another integer
// 'all' and produces their OR-combination (all | add). The result is "stored"
// as constant SafelyAddToMask<>::value. Combination of many flags is achieved
// by chaining parameter lists: the 'add' parameter is value member of
// another (different) SafelyAddToMask<> instantiation.
template<int all, int add> struct SafelyAddToMask
{
    // This typedefs ensures that no flags in the list conflict. If there's
    // any overlap between the already constructed part of the mask ('all')
    // and the value being added to it ('add'), the test that is wxIf<>'s
    // first parameter will be non-zero and so Added value will be
    // FlagsHaveConflictingValues<add>. The next statement will try to use
    // AddedValue::value, but there's no such thing in
    // FlagsHaveConflictingValues<> and so compilation will fail.
    typedef typename wxIf<(all & add) == 0,
                          FlagValue<add>,
                          FlagsHaveConflictingValues<add> >::value
            AddedValue;

    enum { value = all | AddedValue::value };
};

} // wxPrivate namespace



// This macro is used to ensure that no two flags that can be combined in
// the same integer value have overlapping bits. This is sometimes not entirely
// trivial to ensure, for example in wxWindow styles or flags for wxSizerItem
// that span several enums, some of them used for multiple purposes.
//
// By constructing allowed flags mask using wxADD_FLAG macro and then using
// this mask to check flags passed as arguments, you can ensure that
//
// a) if any of the allowed flags overlap, you will get compilation error
// b) if invalid flag is used, there will be an assert at runtime
//
// Example usage:
//
//   static const int SIZER_FLAGS_MASK =
//       wxADD_FLAG(wxCENTRE,
//       wxADD_FLAG(wxHORIZONTAL,
//       wxADD_FLAG(wxVERTICAL,
//       ...
//       0))...);
//
// And wherever flags are used:
//
//   wxASSERT_VALID_FLAG( m_flag, SIZER_FLAGS_MASK );

#define wxADD_FLAG(f, others) \
    ::wxPrivate::SafelyAddToMask<f, others>::value

#else
    #define wxADD_FLAG(f, others) (f | others)
#endif

// Checks if flags value 'f' is within the mask of allowed values
#define wxASSERT_VALID_FLAGS(f, mask)                   \
    wxASSERT_MSG( (f & mask) == f,                      \
                  "invalid flag: not within " #mask )

#endif // _WX_PRIVATE_FLAGSCHECK_H_
