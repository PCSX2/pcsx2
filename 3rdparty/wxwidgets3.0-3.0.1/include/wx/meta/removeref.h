///////////////////////////////////////////////////////////////////////////////
// Name:        wx/meta/removeref.h
// Purpose:     Allows to remove a reference from a type.
// Author:      Vadim Zeitlin
// Created:     2012-10-21
// Copyright:   (c) 2012 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_META_REMOVEREF_H_
#define _WX_META_REMOVEREF_H_

// wxRemoveRef<> is similar to C++11 std::remove_reference<> but works with all
// compilers (but, to compensate for this, doesn't work with rvalue references).

// Except that it doesn't work with VC++ 6 as there doesn't seem to be any way
// to partially specialize a template for references with it.
#ifndef __VISUALC6__

template <typename T>
struct wxRemoveRef
{
    typedef T type;
};

template <typename T>
struct wxRemoveRef<T&>
{
    typedef T type;
};

#define wxHAS_REMOVEREF

#endif // !__VISUALC6__

#endif // _WX_META_REMOVEREF_H_
