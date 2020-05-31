/////////////////////////////////////////////////////////////////////////////
// Name:        wx/meta/implicitconversion.h
// Purpose:     Determine resulting type from implicit conversion
// Author:      Vaclav Slavik
// Created:     2010-10-22
// Copyright:   (c) 2010 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_META_IMPLICITCONVERSION_H_
#define _WX_META_IMPLICITCONVERSION_H_

#include "wx/defs.h"
#include "wx/meta/if.h"

// C++ hierarchy of data types is:
//
//   Long double (highest)
//   Double
//   Float
//   Unsigned long int
//   Long int
//   Unsigned int
//   Int (lowest)
//
// Types lower in the hierarchy are converted into ones higher up if both are
// involved e.g. in arithmetic expressions.

namespace wxPrivate
{

// Helper macro to define a constant inside a template class: it's needed
// because MSVC6 doesn't support initializing static integer members but the
// usual workaround of using enums instead doesn't work for Borland (at least
// in template classes).
#ifdef __VISUALC6__
    #define wxDEFINE_CLASS_INT_CONST(name, value) enum { name = value }
#else
    #define wxDEFINE_CLASS_INT_CONST(name, value) static const int name = value
#endif

template<typename T>
struct TypeHierarchy
{
    // consider unknown types (e.g. objects, pointers) to be of highest
    // level, always convert to them if they occur
    wxDEFINE_CLASS_INT_CONST( level, 9999 );
};

#define WX_TYPE_HIERARCHY_LEVEL(level_num, type)        \
    template<> struct TypeHierarchy<type>               \
    {                                                   \
        wxDEFINE_CLASS_INT_CONST( level, level_num );   \
    }

WX_TYPE_HIERARCHY_LEVEL( 1, char);
WX_TYPE_HIERARCHY_LEVEL( 2, unsigned char);
WX_TYPE_HIERARCHY_LEVEL( 3, short);
WX_TYPE_HIERARCHY_LEVEL( 4, unsigned short);
WX_TYPE_HIERARCHY_LEVEL( 5, int);
WX_TYPE_HIERARCHY_LEVEL( 6, unsigned int);
WX_TYPE_HIERARCHY_LEVEL( 7, long);
WX_TYPE_HIERARCHY_LEVEL( 8, unsigned long);
#ifdef wxLongLong_t
WX_TYPE_HIERARCHY_LEVEL( 9, wxLongLong_t);
WX_TYPE_HIERARCHY_LEVEL(10, wxULongLong_t);
#endif
WX_TYPE_HIERARCHY_LEVEL(11, float);
WX_TYPE_HIERARCHY_LEVEL(12, double);
WX_TYPE_HIERARCHY_LEVEL(13, long double);

#if wxWCHAR_T_IS_REAL_TYPE
    #if SIZEOF_WCHAR_T == SIZEOF_SHORT
      template<> struct TypeHierarchy<wchar_t> : public TypeHierarchy<short> {};
    #elif SIZEOF_WCHAR_T == SIZEOF_INT
      template<> struct TypeHierarchy<wchar_t> : public TypeHierarchy<int> {};
    #elif SIZEOF_WCHAR_T == SIZEOF_LONG
      template<> struct TypeHierarchy<wchar_t> : public TypeHierarchy<long> {};
    #else
      #error "weird wchar_t size, please update this code"
    #endif
#endif

#undef WX_TYPE_HIERARCHY_LEVEL

} // namespace wxPrivate

// Helper to determine resulting type of implicit conversion in
// an expression with two arithmetic types.
template<typename T1, typename T2>
struct wxImplicitConversionType
{
    typedef typename wxIf
            <
                // if T2 is "higher" type, convert to it
                (int)(wxPrivate::TypeHierarchy<T1>::level) < (int)(wxPrivate::TypeHierarchy<T2>::level),
                T2,
                // otherwise use T1
                T1
            >::value
            value;
};


template<typename T1, typename T2, typename T3>
struct wxImplicitConversionType3 : public wxImplicitConversionType<
                        T1,
                        typename wxImplicitConversionType<T2,T3>::value>
{
};

#endif // _WX_META_IMPLICITCONVERSION_H_
