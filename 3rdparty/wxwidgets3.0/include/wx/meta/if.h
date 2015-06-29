/////////////////////////////////////////////////////////////////////////////
// Name:        wx/meta/if.h
// Purpose:     declares wxIf<> metaprogramming construct
// Author:      Vaclav Slavik
// Created:     2008-01-22
// Copyright:   (c) 2008 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_META_IF_H_
#define _WX_META_IF_H_

#include "wx/defs.h"

// NB: This code is intentionally written without partial templates
//     specialization, because some older compilers (notably VC6) don't
//     support it.

namespace wxPrivate
{

template <bool Cond>
struct wxIfImpl

// broken VC6 needs not just an incomplete template class declaration but a
// "skeleton" declaration of the specialized versions below as it apparently
// tries to look up the types in the generic template definition at some moment
// even though it ends up by using the correct specialization in the end -- but
// without this skeleton it doesn't recognize Result as a class at all below
#if defined(__VISUALC__) && !wxCHECK_VISUALC_VERSION(7)
{
    template<typename TTrue, typename TFalse> struct Result {};
}
#endif // VC++ <= 6
;

// specialization for true:
template <>
struct wxIfImpl<true>
{
    template<typename TTrue, typename TFalse> struct Result
    {
        typedef TTrue value;
    };
};

// specialization for false:
template<>
struct wxIfImpl<false>
{
    template<typename TTrue, typename TFalse> struct Result
    {
        typedef TFalse value;
    };
};

} // namespace wxPrivate

// wxIf<> template defines nested type "value" which is the same as
// TTrue if the condition Cond (boolean compile-time constant) was met and
// TFalse if it wasn't.
//
// See wxVector<T> in vector.h for usage example
template<bool Cond, typename TTrue, typename TFalse>
struct wxIf
{
    typedef typename wxPrivate::wxIfImpl<Cond>
                     ::template Result<TTrue, TFalse>::value
            value;
};

#endif // _WX_META_IF_H_
