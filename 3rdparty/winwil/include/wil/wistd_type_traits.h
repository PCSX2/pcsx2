// -*- C++ -*-
//===------------------------ type_traits ---------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// STL common functionality
//
// Some aspects of STL are core language concepts that should be used from all C++ code, regardless
// of whether exceptions are enabled in the component.  Common library code that expects to be used
// from exception-free components want these concepts, but including <type_traits> directly introduces
// friction as it requires components not using STL to declare their STL version.  Doing so creates
// ambiguity around whether STL use is safe in a particular component and implicitly brings in
// a long list of headers (including <new>) which can create further ambiguity around throwing new
// support (some routines pulled in may expect it).  Secondarily, pulling in these headers also has
// the potential to create naming conflicts or other implied dependencies.
//
// To promote the use of these core language concepts outside of STL-based binaries, this file is
// selectively pulling those concepts *directly* from corresponding STL headers.  The corresponding
// "std::" namespace STL functions and types should be preferred over these in code that is bound to
// STL.  The implementation and naming of all functions are taken directly from STL, instead using
// "wistd" (Windows Implementation std) as the namespace.
//
// Routines in this namespace should always be considered a reflection of the *current* STL implementation
// of those routines.  Updates from STL should be taken, but no "bugs" should be fixed here.
//
// New, exception-based code should not use this namespace, but instead should prefer the std:: implementation.
// Only code that is not exception-based and libraries that expect to be utilized across both exception
// and non-exception based code should utilize this functionality.

#ifndef _WISTD_TYPE_TRAITS_H_
#define _WISTD_TYPE_TRAITS_H_

// DO NOT add *any* additional includes to this file -- there should be no dependencies from its usage
#include "wistd_config.h"

#if !defined(__WI_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#pragma GCC system_header
#endif

/// @cond
namespace wistd // ("Windows Implementation" std)
{
template <class _T1, class _T2>
struct __WI_LIBCPP_TEMPLATE_VIS pair;
template <class _Tp>
class __WI_LIBCPP_TEMPLATE_VIS reference_wrapper;
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS hash;

template <class>
struct __void_t
{
    typedef void type;
};

template <class _Tp>
struct __identity
{
    typedef _Tp type;
};

template <class _Tp, bool>
struct __WI_LIBCPP_TEMPLATE_VIS __dependent_type : public _Tp
{
};

template <bool _Bp, class _If, class _Then>
struct __WI_LIBCPP_TEMPLATE_VIS conditional
{
    typedef _If type;
};
template <class _If, class _Then>
struct __WI_LIBCPP_TEMPLATE_VIS conditional<false, _If, _Then>
{
    typedef _Then type;
};

#if __WI_LIBCPP_STD_VER > 11
template <bool _Bp, class _If, class _Then>
using conditional_t = typename conditional<_Bp, _If, _Then>::type;
#endif

template <bool, class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS __lazy_enable_if{};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS __lazy_enable_if<true, _Tp>
{
    typedef typename _Tp::type type;
};

template <bool, class _Tp = void>
struct __WI_LIBCPP_TEMPLATE_VIS enable_if{};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS enable_if<true, _Tp>
{
    typedef _Tp type;
};

#if __WI_LIBCPP_STD_VER > 11
template <bool _Bp, class _Tp = void>
using enable_if_t = typename enable_if<_Bp, _Tp>::type;
#endif

// addressof
#ifndef __WI_LIBCPP_HAS_NO_BUILTIN_ADDRESSOF

template <class _Tp>
inline __WI_LIBCPP_CONSTEXPR_AFTER_CXX14 __WI_LIBCPP_NO_CFI __WI_LIBCPP_INLINE_VISIBILITY _Tp* addressof(_Tp& __x) WI_NOEXCEPT
{
    return __builtin_addressof(__x);
}

#else

template <class _Tp>
inline __WI_LIBCPP_NO_CFI __WI_LIBCPP_INLINE_VISIBILITY _Tp* addressof(_Tp& __x) WI_NOEXCEPT
{
    return reinterpret_cast<_Tp*>(const_cast<char*>(&reinterpret_cast<const volatile char&>(__x)));
}

#endif // __WI_LIBCPP_HAS_NO_BUILTIN_ADDRESSOF

#if !defined(__WI_LIBCPP_CXX03_LANG)
template <class _Tp>
_Tp* addressof(const _Tp&&) WI_NOEXCEPT = delete;
#endif

struct __two
{
    char __lx[2];
};

// helper class:

template <class _Tp, _Tp __v>
struct __WI_LIBCPP_TEMPLATE_VIS integral_constant
{
    static __WI_LIBCPP_CONSTEXPR const _Tp value = __v;
    typedef _Tp value_type;
    typedef integral_constant type;
    __WI_LIBCPP_NODISCARD_ATTRIBUTE __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR operator value_type() const WI_NOEXCEPT
    {
        return value;
    }
#if __WI_LIBCPP_STD_VER > 11
    __WI_LIBCPP_NODISCARD_ATTRIBUTE __WI_LIBCPP_INLINE_VISIBILITY constexpr value_type operator()() const WI_NOEXCEPT
    {
        return value;
    }
#endif
};

template <class _Tp, _Tp __v>
__WI_LIBCPP_CONSTEXPR const _Tp integral_constant<_Tp, __v>::value;

#if !defined(__WI_LIBCPP_CXX03_LANG)
template <bool __b>
using bool_constant = integral_constant<bool, __b>;
#define __WI_LIBCPP_BOOL_CONSTANT(__b) bool_constant<(__b)>
#else
#define __WI_LIBCPP_BOOL_CONSTANT(__b) integral_constant<bool, (__b)>
#endif

typedef __WI_LIBCPP_BOOL_CONSTANT(true) true_type;
typedef __WI_LIBCPP_BOOL_CONSTANT(false) false_type;

#if !defined(__WI_LIBCPP_CXX03_LANG)

// __lazy_and

template <bool _Last, class... _Preds>
struct __lazy_and_impl;

template <class... _Preds>
struct __lazy_and_impl<false, _Preds...> : false_type
{
};

template <>
struct __lazy_and_impl<true> : true_type
{
};

template <class _Pred>
struct __lazy_and_impl<true, _Pred> : integral_constant<bool, _Pred::type::value>
{
};

template <class _Hp, class... _Tp>
struct __lazy_and_impl<true, _Hp, _Tp...> : __lazy_and_impl<_Hp::type::value, _Tp...>
{
};

template <class _P1, class... _Pr>
struct __lazy_and : __lazy_and_impl<_P1::type::value, _Pr...>
{
};

// __lazy_or

template <bool _List, class... _Preds>
struct __lazy_or_impl;

template <class... _Preds>
struct __lazy_or_impl<true, _Preds...> : true_type
{
};

template <>
struct __lazy_or_impl<false> : false_type
{
};

template <class _Hp, class... _Tp>
struct __lazy_or_impl<false, _Hp, _Tp...> : __lazy_or_impl<_Hp::type::value, _Tp...>
{
};

template <class _P1, class... _Pr>
struct __lazy_or : __lazy_or_impl<_P1::type::value, _Pr...>
{
};

// __lazy_not

template <class _Pred>
struct __lazy_not : integral_constant<bool, !_Pred::type::value>
{
};

// __and_
template <class...>
struct __and_;
template <>
struct __and_<> : true_type
{
};

template <class _B0>
struct __and_<_B0> : _B0
{
};

template <class _B0, class _B1>
struct __and_<_B0, _B1> : conditional<_B0::value, _B1, _B0>::type
{
};

template <class _B0, class _B1, class _B2, class... _Bn>
struct __and_<_B0, _B1, _B2, _Bn...> : conditional<_B0::value, __and_<_B1, _B2, _Bn...>, _B0>::type
{
};

// __or_
template <class...>
struct __or_;
template <>
struct __or_<> : false_type
{
};

template <class _B0>
struct __or_<_B0> : _B0
{
};

template <class _B0, class _B1>
struct __or_<_B0, _B1> : conditional<_B0::value, _B0, _B1>::type
{
};

template <class _B0, class _B1, class _B2, class... _Bn>
struct __or_<_B0, _B1, _B2, _Bn...> : conditional<_B0::value, _B0, __or_<_B1, _B2, _Bn...>>::type
{
};

// __not_
template <class _Tp>
struct __not_ : conditional<_Tp::value, false_type, true_type>::type
{
};

#endif // !defined(__WI_LIBCPP_CXX03_LANG)

// is_const

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_const : public false_type
{
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_const<_Tp const> : public true_type
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_const_v = is_const<_Tp>::value;
#endif

// is_volatile

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_volatile : public false_type
{
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_volatile<_Tp volatile> : public true_type
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_volatile_v = is_volatile<_Tp>::value;
#endif

// remove_const

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_const
{
    typedef _Tp type;
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_const<const _Tp>
{
    typedef _Tp type;
};
#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using remove_const_t = typename remove_const<_Tp>::type;
#endif

// remove_volatile

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_volatile
{
    typedef _Tp type;
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_volatile<volatile _Tp>
{
    typedef _Tp type;
};
#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using remove_volatile_t = typename remove_volatile<_Tp>::type;
#endif

// remove_cv

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_cv
{
    typedef typename remove_volatile<typename remove_const<_Tp>::type>::type type;
};
#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using remove_cv_t = typename remove_cv<_Tp>::type;
#endif

// is_void

template <class _Tp>
struct __libcpp_is_void : public false_type
{
};
template <>
struct __libcpp_is_void<void> : public true_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_void : public __libcpp_is_void<typename remove_cv<_Tp>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_void_v = is_void<_Tp>::value;
#endif

// __is_nullptr_t

template <class _Tp>
struct __is_nullptr_t_impl : public false_type
{
};
template <>
struct __is_nullptr_t_impl<nullptr_t> : public true_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS __is_nullptr_t : public __is_nullptr_t_impl<typename remove_cv<_Tp>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_null_pointer : public __is_nullptr_t_impl<typename remove_cv<_Tp>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_null_pointer_v = is_null_pointer<_Tp>::value;
#endif
#endif

// is_integral

template <class _Tp>
struct __libcpp_is_integral : public false_type
{
};
template <>
struct __libcpp_is_integral<bool> : public true_type
{
};
template <>
struct __libcpp_is_integral<char> : public true_type
{
};
template <>
struct __libcpp_is_integral<signed char> : public true_type
{
};
template <>
struct __libcpp_is_integral<unsigned char> : public true_type
{
};
#ifdef _MSC_VER
template <>
struct __libcpp_is_integral<__wchar_t> : public true_type
{
};
#else
template <>
struct __libcpp_is_integral<wchar_t> : public true_type
{
};
#endif
#ifndef __WI_LIBCPP_HAS_NO_UNICODE_CHARS
template <>
struct __libcpp_is_integral<char16_t> : public true_type
{
};
template <>
struct __libcpp_is_integral<char32_t> : public true_type
{
};
#endif // __WI_LIBCPP_HAS_NO_UNICODE_CHARS
template <>
struct __libcpp_is_integral<short> : public true_type
{
};
template <>
struct __libcpp_is_integral<unsigned short> : public true_type
{
};
template <>
struct __libcpp_is_integral<int> : public true_type
{
};
template <>
struct __libcpp_is_integral<unsigned int> : public true_type
{
};
template <>
struct __libcpp_is_integral<long> : public true_type
{
};
template <>
struct __libcpp_is_integral<unsigned long> : public true_type
{
};
template <>
struct __libcpp_is_integral<long long> : public true_type
{
};
template <>
struct __libcpp_is_integral<unsigned long long> : public true_type
{
};
#ifndef __WI_LIBCPP_HAS_NO_INT128
template <>
struct __libcpp_is_integral<__int128_t> : public true_type
{
};
template <>
struct __libcpp_is_integral<__uint128_t> : public true_type
{
};
#endif

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_integral : public __libcpp_is_integral<typename remove_cv<_Tp>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_integral_v = is_integral<_Tp>::value;
#endif

// is_floating_point

template <class _Tp>
struct __libcpp_is_floating_point : public false_type
{
};
template <>
struct __libcpp_is_floating_point<float> : public true_type
{
};
template <>
struct __libcpp_is_floating_point<double> : public true_type
{
};
template <>
struct __libcpp_is_floating_point<long double> : public true_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_floating_point : public __libcpp_is_floating_point<typename remove_cv<_Tp>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_floating_point_v = is_floating_point<_Tp>::value;
#endif

// is_array

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_array : public false_type
{
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_array<_Tp[]> : public true_type
{
};
template <class _Tp, size_t _Np>
struct __WI_LIBCPP_TEMPLATE_VIS is_array<_Tp[_Np]> : public true_type
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_array_v = is_array<_Tp>::value;
#endif

// is_pointer

template <class _Tp>
struct __libcpp_is_pointer : public false_type
{
};
template <class _Tp>
struct __libcpp_is_pointer<_Tp*> : public true_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_pointer : public __libcpp_is_pointer<typename remove_cv<_Tp>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_pointer_v = is_pointer<_Tp>::value;
#endif

// is_reference

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_lvalue_reference : public false_type
{
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_lvalue_reference<_Tp&> : public true_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_rvalue_reference : public false_type
{
};
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_rvalue_reference<_Tp&&> : public true_type
{
};
#endif

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_reference : public false_type
{
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_reference<_Tp&> : public true_type
{
};
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_reference<_Tp&&> : public true_type
{
};
#endif

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_reference_v = is_reference<_Tp>::value;

template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_lvalue_reference_v = is_lvalue_reference<_Tp>::value;

template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_rvalue_reference_v = is_rvalue_reference<_Tp>::value;
#endif
// is_union

#if __WI_HAS_FEATURE_IS_UNION || (__WI_GNUC_VER >= 403)

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_union : public integral_constant<bool, __is_union(_Tp)>
{
};

#else

template <class _Tp>
struct __libcpp_union : public false_type
{
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_union : public __libcpp_union<typename remove_cv<_Tp>::type>
{
};

#endif

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_union_v = is_union<_Tp>::value;
#endif

// is_class

#if __WI_HAS_FEATURE_IS_CLASS || (__WI_GNUC_VER >= 403)

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_class : public integral_constant<bool, __is_class(_Tp)>
{
};

#else

namespace __is_class_imp
{
    template <class _Tp>
    char __test(int _Tp::*);
    template <class _Tp>
    __two __test(...);
} // namespace __is_class_imp

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_class
    : public integral_constant<bool, sizeof(__is_class_imp::__test<_Tp>(0)) == 1 && !is_union<_Tp>::value>
{
};

#endif

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_class_v = is_class<_Tp>::value;
#endif

// is_same

template <class _Tp, class _Up>
struct __WI_LIBCPP_TEMPLATE_VIS is_same : public false_type
{
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_same<_Tp, _Tp> : public true_type
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp, class _Up>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_same_v = is_same<_Tp, _Up>::value;
#endif

// is_function

namespace __libcpp_is_function_imp
{
    struct __dummy_type
    {
    };
    template <class _Tp>
    char __test(_Tp*);
    template <class _Tp>
    char __test(__dummy_type);
    template <class _Tp>
    __two __test(...);
    template <class _Tp>
    _Tp& __source(int);
    template <class _Tp>
    __dummy_type __source(...);
} // namespace __libcpp_is_function_imp

template <class _Tp, bool = is_class<_Tp>::value || is_union<_Tp>::value || is_void<_Tp>::value || is_reference<_Tp>::value || __is_nullptr_t<_Tp>::value>
struct __libcpp_is_function
    : public integral_constant<bool, sizeof(__libcpp_is_function_imp::__test<_Tp>(__libcpp_is_function_imp::__source<_Tp>(0))) == 1>
{
};
template <class _Tp>
struct __libcpp_is_function<_Tp, true> : public false_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_function : public __libcpp_is_function<_Tp>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_function_v = is_function<_Tp>::value;
#endif

// is_member_function_pointer

// template <class _Tp> struct            __libcpp_is_member_function_pointer             : public false_type {};
// template <class _Tp, class _Up> struct __libcpp_is_member_function_pointer<_Tp _Up::*> : public is_function<_Tp> {};
//

template <class _Mp, bool _IsMemberFunctionPtr, bool _IsMemberObjectPtr>
struct __member_pointer_traits_imp
{ // forward declaration; specializations later
};

template <class _Tp>
struct __libcpp_is_member_function_pointer : public false_type
{
};

template <class _Ret, class _Class>
struct __libcpp_is_member_function_pointer<_Ret _Class::*> : public is_function<_Ret>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_member_function_pointer
    : public __libcpp_is_member_function_pointer<typename remove_cv<_Tp>::type>::type
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_member_function_pointer_v = is_member_function_pointer<_Tp>::value;
#endif

// is_member_pointer

template <class _Tp>
struct __libcpp_is_member_pointer : public false_type
{
};
template <class _Tp, class _Up>
struct __libcpp_is_member_pointer<_Tp _Up::*> : public true_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_member_pointer : public __libcpp_is_member_pointer<typename remove_cv<_Tp>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_member_pointer_v = is_member_pointer<_Tp>::value;
#endif

// is_member_object_pointer

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_member_object_pointer
    : public integral_constant<bool, is_member_pointer<_Tp>::value && !is_member_function_pointer<_Tp>::value>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_member_object_pointer_v = is_member_object_pointer<_Tp>::value;
#endif

// is_enum

#if __WI_HAS_FEATURE_IS_ENUM || (__WI_GNUC_VER >= 403)

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_enum : public integral_constant<bool, __is_enum(_Tp)>
{
};

#else

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_enum
    : public integral_constant<
          bool,
          !is_void<_Tp>::value && !is_integral<_Tp>::value && !is_floating_point<_Tp>::value && !is_array<_Tp>::value &&
              !is_pointer<_Tp>::value && !is_reference<_Tp>::value && !is_member_pointer<_Tp>::value && !is_union<_Tp>::value &&
              !is_class<_Tp>::value && !is_function<_Tp>::value>
{
};

#endif

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_enum_v = is_enum<_Tp>::value;
#endif

// is_arithmetic

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_arithmetic : public integral_constant<bool, is_integral<_Tp>::value || is_floating_point<_Tp>::value>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_arithmetic_v = is_arithmetic<_Tp>::value;
#endif

// is_fundamental

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_fundamental
    : public integral_constant<bool, is_void<_Tp>::value || __is_nullptr_t<_Tp>::value || is_arithmetic<_Tp>::value>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_fundamental_v = is_fundamental<_Tp>::value;
#endif

// is_scalar

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_scalar
    : public integral_constant<bool, is_arithmetic<_Tp>::value || is_member_pointer<_Tp>::value || is_pointer<_Tp>::value || __is_nullptr_t<_Tp>::value || is_enum<_Tp>::value>
{
};

template <>
struct __WI_LIBCPP_TEMPLATE_VIS is_scalar<nullptr_t> : public true_type
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_scalar_v = is_scalar<_Tp>::value;
#endif

// is_object

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_object
    : public integral_constant<bool, is_scalar<_Tp>::value || is_array<_Tp>::value || is_union<_Tp>::value || is_class<_Tp>::value>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_object_v = is_object<_Tp>::value;
#endif

// is_compound

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_compound : public integral_constant<bool, !is_fundamental<_Tp>::value>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_compound_v = is_compound<_Tp>::value;
#endif

// __is_referenceable  [defns.referenceable]

struct __is_referenceable_impl
{
    template <class _Tp>
    static _Tp& __test(int);
    template <class _Tp>
    static __two __test(...);
};

template <class _Tp>
struct __is_referenceable : integral_constant<bool, !is_same<decltype(__is_referenceable_impl::__test<_Tp>(0)), __two>::value>{};

// add_const

template <class _Tp, bool = is_reference<_Tp>::value || is_function<_Tp>::value || is_const<_Tp>::value>
struct __add_const
{
    typedef _Tp type;
};

template <class _Tp>
struct __add_const<_Tp, false>
{
    typedef const _Tp type;
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS add_const
{
    typedef typename __add_const<_Tp>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using add_const_t = typename add_const<_Tp>::type;
#endif

// add_volatile

template <class _Tp, bool = is_reference<_Tp>::value || is_function<_Tp>::value || is_volatile<_Tp>::value>
struct __add_volatile
{
    typedef _Tp type;
};

template <class _Tp>
struct __add_volatile<_Tp, false>
{
    typedef volatile _Tp type;
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS add_volatile
{
    typedef typename __add_volatile<_Tp>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using add_volatile_t = typename add_volatile<_Tp>::type;
#endif

// add_cv

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS add_cv
{
    typedef typename add_const<typename add_volatile<_Tp>::type>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using add_cv_t = typename add_cv<_Tp>::type;
#endif

// remove_reference

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_reference
{
    typedef _Tp type;
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_reference<_Tp&>
{
    typedef _Tp type;
};
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_reference<_Tp&&>
{
    typedef _Tp type;
};
#endif

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using remove_reference_t = typename remove_reference<_Tp>::type;
#endif

// add_lvalue_reference

template <class _Tp, bool = __is_referenceable<_Tp>::value>
struct __add_lvalue_reference_impl
{
    typedef _Tp type;
};
template <class _Tp>
struct __add_lvalue_reference_impl<_Tp, true>
{
    typedef _Tp& type;
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS add_lvalue_reference
{
    typedef typename __add_lvalue_reference_impl<_Tp>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using add_lvalue_reference_t = typename add_lvalue_reference<_Tp>::type;
#endif

#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

template <class _Tp, bool = __is_referenceable<_Tp>::value>
struct __add_rvalue_reference_impl
{
    typedef _Tp type;
};
template <class _Tp>
struct __add_rvalue_reference_impl<_Tp, true>
{
    typedef _Tp&& type;
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS add_rvalue_reference
{
    typedef typename __add_rvalue_reference_impl<_Tp>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using add_rvalue_reference_t = typename add_rvalue_reference<_Tp>::type;
#endif

#endif // __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

// MSVC has issues compiling some source code that uses the libc++ definition of 'declval'
#ifdef _MSC_VER
template <typename _Tp>
typename add_rvalue_reference<_Tp>::type declval() WI_NOEXCEPT;
#else
template <class _Tp>
_Tp&& __declval(int);
template <class _Tp>
_Tp __declval(long);

template <class _Tp>
decltype(__declval<_Tp>(0)) declval() WI_NOEXCEPT;
#endif

#else // __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

template <class _Tp>
typename add_lvalue_reference<_Tp>::type declval();

#endif // __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

// __uncvref

template <class _Tp>
struct __uncvref
{
    typedef typename remove_cv<typename remove_reference<_Tp>::type>::type type;
};

template <class _Tp>
struct __unconstref
{
    typedef typename remove_const<typename remove_reference<_Tp>::type>::type type;
};

#ifndef __WI_LIBCPP_CXX03_LANG
template <class _Tp>
using __uncvref_t = typename __uncvref<_Tp>::type;
#endif

// __is_same_uncvref

template <class _Tp, class _Up>
struct __is_same_uncvref : is_same<typename __uncvref<_Tp>::type, typename __uncvref<_Up>::type>
{
};

#if __WI_LIBCPP_STD_VER > 17
// remove_cvref - same as __uncvref
template <class _Tp>
struct remove_cvref : public __uncvref<_Tp>
{
};

template <class _Tp>
using remove_cvref_t = typename remove_cvref<_Tp>::type;
#endif

struct __any
{
    __any(...);
};

// remove_pointer

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_pointer
{
    typedef _Tp type;
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_pointer<_Tp*>
{
    typedef _Tp type;
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_pointer<_Tp* const>
{
    typedef _Tp type;
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_pointer<_Tp* volatile>
{
    typedef _Tp type;
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_pointer<_Tp* const volatile>
{
    typedef _Tp type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using remove_pointer_t = typename remove_pointer<_Tp>::type;
#endif

// add_pointer

template <class _Tp, bool = __is_referenceable<_Tp>::value || is_same<typename remove_cv<_Tp>::type, void>::value>
struct __add_pointer_impl
{
    typedef typename remove_reference<_Tp>::type* type;
};
template <class _Tp>
struct __add_pointer_impl<_Tp, false>
{
    typedef _Tp type;
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS add_pointer
{
    typedef typename __add_pointer_impl<_Tp>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using add_pointer_t = typename add_pointer<_Tp>::type;
#endif

// type_identity
#if __WI_LIBCPP_STD_VER > 17
template <class _Tp>
struct type_identity
{
    typedef _Tp type;
};
template <class _Tp>
using type_identity_t = typename type_identity<_Tp>::type;
#endif

// is_signed

template <class _Tp, bool = is_integral<_Tp>::value>
struct __libcpp_is_signed_impl : public __WI_LIBCPP_BOOL_CONSTANT(_Tp(-1) < _Tp(0)){};

template <class _Tp>
struct __libcpp_is_signed_impl<_Tp, false> : public true_type
{
}; // floating point

template <class _Tp, bool = is_arithmetic<_Tp>::value>
struct __libcpp_is_signed : public __libcpp_is_signed_impl<_Tp>
{
};

template <class _Tp>
struct __libcpp_is_signed<_Tp, false> : public false_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_signed : public __libcpp_is_signed<_Tp>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_signed_v = is_signed<_Tp>::value;
#endif

// is_unsigned

template <class _Tp, bool = is_integral<_Tp>::value>
struct __libcpp_is_unsigned_impl : public __WI_LIBCPP_BOOL_CONSTANT(_Tp(0) < _Tp(-1)){};

template <class _Tp>
struct __libcpp_is_unsigned_impl<_Tp, false> : public false_type
{
}; // floating point

template <class _Tp, bool = is_arithmetic<_Tp>::value>
struct __libcpp_is_unsigned : public __libcpp_is_unsigned_impl<_Tp>
{
};

template <class _Tp>
struct __libcpp_is_unsigned<_Tp, false> : public false_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_unsigned : public __libcpp_is_unsigned<_Tp>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_unsigned_v = is_unsigned<_Tp>::value;
#endif

// rank

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS rank : public integral_constant<size_t, 0>
{
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS rank<_Tp[]> : public integral_constant<size_t, rank<_Tp>::value + 1>
{
};
template <class _Tp, size_t _Np>
struct __WI_LIBCPP_TEMPLATE_VIS rank<_Tp[_Np]> : public integral_constant<size_t, rank<_Tp>::value + 1>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR size_t rank_v = rank<_Tp>::value;
#endif

// extent

template <class _Tp, unsigned _Ip = 0>
struct __WI_LIBCPP_TEMPLATE_VIS extent : public integral_constant<size_t, 0>
{
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS extent<_Tp[], 0> : public integral_constant<size_t, 0>
{
};
template <class _Tp, unsigned _Ip>
struct __WI_LIBCPP_TEMPLATE_VIS extent<_Tp[], _Ip> : public integral_constant<size_t, extent<_Tp, _Ip - 1>::value>
{
};
template <class _Tp, size_t _Np>
struct __WI_LIBCPP_TEMPLATE_VIS extent<_Tp[_Np], 0> : public integral_constant<size_t, _Np>
{
};
template <class _Tp, size_t _Np, unsigned _Ip>
struct __WI_LIBCPP_TEMPLATE_VIS extent<_Tp[_Np], _Ip> : public integral_constant<size_t, extent<_Tp, _Ip - 1>::value>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp, unsigned _Ip = 0>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR size_t extent_v = extent<_Tp, _Ip>::value;
#endif

// remove_extent

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_extent
{
    typedef _Tp type;
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_extent<_Tp[]>
{
    typedef _Tp type;
};
template <class _Tp, size_t _Np>
struct __WI_LIBCPP_TEMPLATE_VIS remove_extent<_Tp[_Np]>
{
    typedef _Tp type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using remove_extent_t = typename remove_extent<_Tp>::type;
#endif

// remove_all_extents

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_all_extents
{
    typedef _Tp type;
};
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS remove_all_extents<_Tp[]>
{
    typedef typename remove_all_extents<_Tp>::type type;
};
template <class _Tp, size_t _Np>
struct __WI_LIBCPP_TEMPLATE_VIS remove_all_extents<_Tp[_Np]>
{
    typedef typename remove_all_extents<_Tp>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using remove_all_extents_t = typename remove_all_extents<_Tp>::type;
#endif

// decay

template <class _Up, bool>
struct __decay
{
    typedef typename remove_cv<_Up>::type type;
};

template <class _Up>
struct __decay<_Up, true>
{
public:
    typedef typename conditional<
        is_array<_Up>::value,
        typename remove_extent<_Up>::type*,
        typename conditional<is_function<_Up>::value, typename add_pointer<_Up>::type, typename remove_cv<_Up>::type>::type>::type type;
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS decay
{
private:
    typedef typename remove_reference<_Tp>::type _Up;

public:
    typedef typename __decay<_Up, __is_referenceable<_Up>::value>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using decay_t = typename decay<_Tp>::type;
#endif

// is_abstract

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_abstract : public integral_constant<bool, __is_abstract(_Tp)>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_abstract_v = is_abstract<_Tp>::value;
#endif

// is_final

#if defined(__WI_LIBCPP_HAS_IS_FINAL)
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS __libcpp_is_final : public integral_constant<bool, __is_final(_Tp)>
{
};
#else
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS __libcpp_is_final : public false_type
{
};
#endif

#if defined(__WI_LIBCPP_HAS_IS_FINAL) && __WI_LIBCPP_STD_VER > 11
template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_final : public integral_constant<bool, __is_final(_Tp)>
{
};
#endif

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_final_v = is_final<_Tp>::value;
#endif

// is_aggregate
#if __WI_LIBCPP_STD_VER > 14 && !defined(__WI_LIBCPP_HAS_NO_IS_AGGREGATE)

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_aggregate : public integral_constant<bool, __is_aggregate(_Tp)>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR constexpr bool is_aggregate_v = is_aggregate<_Tp>::value;
#endif

#endif // __WI_LIBCPP_STD_VER > 14 && !defined(__WI_LIBCPP_HAS_NO_IS_AGGREGATE)

// is_base_of

#ifdef __WI_LIBCPP_HAS_IS_BASE_OF

template <class _Bp, class _Dp>
struct __WI_LIBCPP_TEMPLATE_VIS is_base_of : public integral_constant<bool, __is_base_of(_Bp, _Dp)>
{
};

#else // __WI_LIBCPP_HAS_IS_BASE_OF

namespace __is_base_of_imp
{
    template <class _Tp>
    struct _Dst
    {
        _Dst(const volatile _Tp&);
    };
    template <class _Tp>
    struct _Src
    {
        operator const volatile _Tp&();
        template <class _Up>
        operator const _Dst<_Up>&();
    };
    template <size_t>
    struct __one
    {
        typedef char type;
    };
    template <class _Bp, class _Dp>
    typename __one<sizeof(_Dst<_Bp>(declval<_Src<_Dp>>()))>::type __test(int);
    template <class _Bp, class _Dp>
    __two __test(...);
} // namespace __is_base_of_imp

template <class _Bp, class _Dp>
struct __WI_LIBCPP_TEMPLATE_VIS is_base_of
    : public integral_constant<bool, is_class<_Bp>::value && sizeof(__is_base_of_imp::__test<_Bp, _Dp>(0)) == 2>
{
};

#endif // __WI_LIBCPP_HAS_IS_BASE_OF

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Bp, class _Dp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_base_of_v = is_base_of<_Bp, _Dp>::value;
#endif

// is_convertible

#if __WI_HAS_FEATURE_IS_CONVERTIBLE_TO && !defined(__WI_LIBCPP_USE_IS_CONVERTIBLE_FALLBACK)

template <class _T1, class _T2>
struct __WI_LIBCPP_TEMPLATE_VIS is_convertible
    : public integral_constant<bool, __is_convertible_to(_T1, _T2) && !is_abstract<_T2>::value>
{
};

#else // __WI_HAS_FEATURE_IS_CONVERTIBLE_TO

namespace __is_convertible_imp
{
    template <class _Tp>
    void __test_convert(_Tp);

    template <class _From, class _To, class = void>
    struct __is_convertible_test : public false_type
    {
    };

    template <class _From, class _To>
    struct __is_convertible_test<_From, _To, decltype(__is_convertible_imp::__test_convert<_To>(declval<_From>()))> : public true_type
    {
    };

    template <class _Tp, bool _IsArray = is_array<_Tp>::value, bool _IsFunction = is_function<_Tp>::value, bool _IsVoid = is_void<_Tp>::value>
    struct __is_array_function_or_void
    {
        enum
        {
            value = 0
        };
    };
    template <class _Tp>
    struct __is_array_function_or_void<_Tp, true, false, false>
    {
        enum
        {
            value = 1
        };
    };
    template <class _Tp>
    struct __is_array_function_or_void<_Tp, false, true, false>
    {
        enum
        {
            value = 2
        };
    };
    template <class _Tp>
    struct __is_array_function_or_void<_Tp, false, false, true>
    {
        enum
        {
            value = 3
        };
    };
} // namespace __is_convertible_imp

template <class _Tp, unsigned = __is_convertible_imp::__is_array_function_or_void<typename remove_reference<_Tp>::type>::value>
struct __is_convertible_check
{
    static const size_t __v = 0;
};

template <class _Tp>
struct __is_convertible_check<_Tp, 0>
{
    static const size_t __v = sizeof(_Tp);
};

template <
    class _T1,
    class _T2,
    unsigned _T1_is_array_function_or_void = __is_convertible_imp::__is_array_function_or_void<_T1>::value,
    unsigned _T2_is_array_function_or_void = __is_convertible_imp::__is_array_function_or_void<_T2>::value>
struct __is_convertible
    : public integral_constant<
          bool,
          __is_convertible_imp::__is_convertible_test<_T1, _T2>::value
#if defined(__WI_LIBCPP_HAS_NO_RVALUE_REFERENCES)
              && !(!is_function<_T1>::value && !is_reference<_T1>::value && is_reference<_T2>::value &&
                   (!is_const<typename remove_reference<_T2>::type>::value || is_volatile<typename remove_reference<_T2>::type>::value) &&
                   (is_same<typename remove_cv<_T1>::type, typename remove_cv<typename remove_reference<_T2>::type>::type>::value ||
                    is_base_of<typename remove_reference<_T2>::type, _T1>::value))
#endif
          >{};

template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 0, 1> : public false_type{};
template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 1, 1> : public false_type{};
template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 2, 1> : public false_type{};
template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 3, 1> : public false_type{};

template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 0, 2> : public false_type{};
template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 1, 2> : public false_type{};
template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 2, 2> : public false_type{};
template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 3, 2> : public false_type{};

template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 0, 3> : public false_type{};
template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 1, 3> : public false_type{};
template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 2, 3> : public false_type{};
template <class _T1, class _T2>
struct __is_convertible<_T1, _T2, 3, 3> : public true_type{};

template <class _T1, class _T2>
struct __WI_LIBCPP_TEMPLATE_VIS is_convertible : public __is_convertible<_T1, _T2>
{
    static const size_t __complete_check1 = __is_convertible_check<_T1>::__v;
    static const size_t __complete_check2 = __is_convertible_check<_T2>::__v;
};

#endif // __WI_HAS_FEATURE_IS_CONVERTIBLE_TO

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _From, class _To>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_convertible_v = is_convertible<_From, _To>::value;
#endif

// is_empty

#if __WI_HAS_FEATURE_IS_EMPTY || (__WI_GNUC_VER >= 407)

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_empty : public integral_constant<bool, __is_empty(_Tp)>
{
};

#else // __WI_HAS_FEATURE_IS_EMPTY

template <class _Tp>
struct __is_empty1 : public _Tp
{
    double __lx;
};

struct __is_empty2
{
    double __lx;
};

template <class _Tp, bool = is_class<_Tp>::value>
struct __libcpp_empty : public integral_constant<bool, sizeof(__is_empty1<_Tp>) == sizeof(__is_empty2)>
{
};

template <class _Tp>
struct __libcpp_empty<_Tp, false> : public false_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_empty : public __libcpp_empty<_Tp>
{
};

#endif // __WI_HAS_FEATURE_IS_EMPTY

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_empty_v = is_empty<_Tp>::value;
#endif

// is_polymorphic

#if __WI_HAS_FEATURE_IS_POLYMORPHIC

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_polymorphic : public integral_constant<bool, __is_polymorphic(_Tp)>
{
};

#else

template <typename _Tp>
char& __is_polymorphic_impl(typename enable_if<sizeof((_Tp*)dynamic_cast<const volatile void*>(declval<_Tp*>())) != 0, int>::type);
template <typename _Tp>
__two& __is_polymorphic_impl(...);

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_polymorphic : public integral_constant<bool, sizeof(__is_polymorphic_impl<_Tp>(0)) == 1>
{
};

#endif // __WI_HAS_FEATURE_IS_POLYMORPHIC

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_polymorphic_v = is_polymorphic<_Tp>::value;
#endif

// has_virtual_destructor

#if __WI_HAS_FEATURE_HAS_VIRTUAL_DESTRUCTOR || (__WI_GNUC_VER >= 403)

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS has_virtual_destructor : public integral_constant<bool, __has_virtual_destructor(_Tp)>
{
};

#else

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS has_virtual_destructor : public false_type
{
};

#endif

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool has_virtual_destructor_v = has_virtual_destructor<_Tp>::value;
#endif

// has_unique_object_representations

#if __WI_LIBCPP_STD_VER > 14 && defined(__WI_LIBCPP_HAS_UNIQUE_OBJECT_REPRESENTATIONS)

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS has_unique_object_representations
    : public integral_constant<bool, __has_unique_object_representations(remove_cv_t<remove_all_extents_t<_Tp>>)>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool has_unique_object_representations_v = has_unique_object_representations<_Tp>::value;
#endif

#endif

// alignment_of

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS alignment_of : public integral_constant<size_t, __alignof__(_Tp)>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR size_t alignment_of_v = alignment_of<_Tp>::value;
#endif

// aligned_storage

template <class _Hp, class _Tp>
struct __type_list
{
    typedef _Hp _Head;
    typedef _Tp _Tail;
};

struct __nat
{
#ifndef __WI_LIBCPP_CXX03_LANG
    __nat() = delete;
    __nat(const __nat&) = delete;
    __nat& operator=(const __nat&) = delete;
    ~__nat() = delete;
#endif
};

template <class _Tp>
struct __align_type
{
    static const size_t value = alignment_of<_Tp>::value;
    typedef _Tp type;
};

struct __struct_double
{
    long double __lx;
};
struct __struct_double4
{
    double __lx[4];
};

typedef __type_list<
    __align_type<unsigned char>,
    __type_list<
        __align_type<unsigned short>,
        __type_list<
            __align_type<unsigned int>,
            __type_list<
                __align_type<unsigned long>,
                __type_list<
                    __align_type<unsigned long long>,
                    __type_list<__align_type<double>, __type_list<__align_type<long double>, __type_list<__align_type<__struct_double>, __type_list<__align_type<__struct_double4>, __type_list<__align_type<int*>, __nat>>>>>>>>>>
    __all_types;

template <class _TL, size_t _Align>
struct __find_pod;

template <class _Hp, size_t _Align>
struct __find_pod<__type_list<_Hp, __nat>, _Align>
{
    typedef typename conditional<_Align == _Hp::value, typename _Hp::type, void>::type type;
};

template <class _Hp, class _Tp, size_t _Align>
struct __find_pod<__type_list<_Hp, _Tp>, _Align>
{
    typedef typename conditional<_Align == _Hp::value, typename _Hp::type, typename __find_pod<_Tp, _Align>::type>::type type;
};

template <size_t _Align>
struct __has_pod_with_align : public integral_constant<bool, !is_same<typename __find_pod<__all_types, _Align>::type, void>::value>
{
};

template <class _TL, size_t _Len>
struct __find_max_align;

template <class _Hp, size_t _Len>
struct __find_max_align<__type_list<_Hp, __nat>, _Len> : public integral_constant<size_t, _Hp::value>
{
};

template <size_t _Len, size_t _A1, size_t _A2>
struct __select_align
{
private:
    static const size_t __min = _A2 < _A1 ? _A2 : _A1;
    static const size_t __max = _A1 < _A2 ? _A2 : _A1;

public:
    static const size_t value = _Len < __max ? __min : __max;
};

template <class _Hp, class _Tp, size_t _Len>
struct __find_max_align<__type_list<_Hp, _Tp>, _Len>
    : public integral_constant<size_t, __select_align<_Len, _Hp::value, __find_max_align<_Tp, _Len>::value>::value>
{
};

template <size_t _Len, size_t _Align, bool = __has_pod_with_align<_Align>::value>
struct __aligned_storage
{
    typedef typename __find_pod<__all_types, _Align>::type _Aligner;
    static_assert(!is_void<_Aligner>::value, "");
    union type
    {
        _Aligner __align;
        unsigned char __data[(_Len + _Align - 1) / _Align * _Align];
    };
};

#define __WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(n) \
    template <size_t _Len> \
    struct __aligned_storage<_Len, n, false> \
    { \
        struct __WI_ALIGNAS(n) type \
        { \
            unsigned char __lx[(_Len + n - 1) / n * n]; \
        }; \
    }

__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x1);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x2);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x4);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x8);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x10);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x20);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x40);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x80);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x100);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x200);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x400);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x800);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x1000);
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x2000);
// PE/COFF does not support alignment beyond 8192 (=0x2000)
#if !defined(__WI_LIBCPP_OBJECT_FORMAT_COFF)
__WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION(0x4000);
#endif // !defined(__WI_LIBCPP_OBJECT_FORMAT_COFF)

#undef __WI_CREATE_ALIGNED_STORAGE_SPECIALIZATION

template <size_t _Len, size_t _Align = __find_max_align<__all_types, _Len>::value>
struct __WI_LIBCPP_TEMPLATE_VIS aligned_storage : public __aligned_storage<_Len, _Align>
{
};

#if __WI_LIBCPP_STD_VER > 11
template <size_t _Len, size_t _Align = __find_max_align<__all_types, _Len>::value>
using aligned_storage_t = typename aligned_storage<_Len, _Align>::type;
#endif

#ifndef __WI_LIBCPP_HAS_NO_VARIADICS

// aligned_union

template <size_t _I0, size_t... _In>
struct __static_max;

template <size_t _I0>
struct __static_max<_I0>
{
    static const size_t value = _I0;
};

template <size_t _I0, size_t _I1, size_t... _In>
struct __static_max<_I0, _I1, _In...>
{
    static const size_t value = _I0 >= _I1 ? __static_max<_I0, _In...>::value : __static_max<_I1, _In...>::value;
};

template <size_t _Len, class _Type0, class... _Types>
struct aligned_union
{
    static const size_t alignment_value = __static_max<__alignof__(_Type0), __alignof__(_Types)...>::value;
    static const size_t __len = __static_max<_Len, sizeof(_Type0), sizeof(_Types)...>::value;
    typedef typename aligned_storage<__len, alignment_value>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <size_t _Len, class... _Types>
using aligned_union_t = typename aligned_union<_Len, _Types...>::type;
#endif

#endif // __WI_LIBCPP_HAS_NO_VARIADICS

template <class _Tp>
struct __numeric_type
{
    static void __test(...);
    static float __test(float);
    static double __test(char);
    static double __test(int);
    static double __test(unsigned);
    static double __test(long);
    static double __test(unsigned long);
    static double __test(long long);
    static double __test(unsigned long long);
    static double __test(double);
    static long double __test(long double);

    typedef decltype(__test(declval<_Tp>())) type;
    static const bool value = !is_same<type, void>::value;
};

template <>
struct __numeric_type<void>
{
    static const bool value = true;
};

// __promote

template <class _A1, class _A2 = void, class _A3 = void, bool = __numeric_type<_A1>::value && __numeric_type<_A2>::value && __numeric_type<_A3>::value>
class __promote_imp
{
public:
    static const bool value = false;
};

template <class _A1, class _A2, class _A3>
class __promote_imp<_A1, _A2, _A3, true>
{
private:
    typedef typename __promote_imp<_A1>::type __type1;
    typedef typename __promote_imp<_A2>::type __type2;
    typedef typename __promote_imp<_A3>::type __type3;

public:
    typedef decltype(__type1() + __type2() + __type3()) type;
    static const bool value = true;
};

template <class _A1, class _A2>
class __promote_imp<_A1, _A2, void, true>
{
private:
    typedef typename __promote_imp<_A1>::type __type1;
    typedef typename __promote_imp<_A2>::type __type2;

public:
    typedef decltype(__type1() + __type2()) type;
    static const bool value = true;
};

template <class _A1>
class __promote_imp<_A1, void, void, true>
{
public:
    typedef typename __numeric_type<_A1>::type type;
    static const bool value = true;
};

template <class _A1, class _A2 = void, class _A3 = void>
class __promote : public __promote_imp<_A1, _A2, _A3>
{
};

// make_signed / make_unsigned

typedef __type_list<
    signed char,
    __type_list<
        signed short,
        __type_list<
            signed int,
            __type_list<
                signed long,
                __type_list<
                    signed long long,
#ifndef __WI_LIBCPP_HAS_NO_INT128
                    __type_list<
                        __int128_t,
#endif
                        __nat
#ifndef __WI_LIBCPP_HAS_NO_INT128
                        >
#endif
                    >>>>>
    __signed_types;

typedef __type_list<
    unsigned char,
    __type_list<
        unsigned short,
        __type_list<
            unsigned int,
            __type_list<
                unsigned long,
                __type_list<
                    unsigned long long,
#ifndef __WI_LIBCPP_HAS_NO_INT128
                    __type_list<
                        __uint128_t,
#endif
                        __nat
#ifndef __WI_LIBCPP_HAS_NO_INT128
                        >
#endif
                    >>>>>
    __unsigned_types;

template <class _TypeList, size_t _Size, bool = _Size <= sizeof(typename _TypeList::_Head)>
struct __find_first;

template <class _Hp, class _Tp, size_t _Size>
struct __find_first<__type_list<_Hp, _Tp>, _Size, true>
{
    typedef _Hp type;
};

template <class _Hp, class _Tp, size_t _Size>
struct __find_first<__type_list<_Hp, _Tp>, _Size, false>
{
    typedef typename __find_first<_Tp, _Size>::type type;
};

template <class _Tp, class _Up, bool = is_const<typename remove_reference<_Tp>::type>::value, bool = is_volatile<typename remove_reference<_Tp>::type>::value>
struct __apply_cv
{
    typedef _Up type;
};

template <class _Tp, class _Up>
struct __apply_cv<_Tp, _Up, true, false>
{
    typedef const _Up type;
};

template <class _Tp, class _Up>
struct __apply_cv<_Tp, _Up, false, true>
{
    typedef volatile _Up type;
};

template <class _Tp, class _Up>
struct __apply_cv<_Tp, _Up, true, true>
{
    typedef const volatile _Up type;
};

template <class _Tp, class _Up>
struct __apply_cv<_Tp&, _Up, false, false>
{
    typedef _Up& type;
};

template <class _Tp, class _Up>
struct __apply_cv<_Tp&, _Up, true, false>
{
    typedef const _Up& type;
};

template <class _Tp, class _Up>
struct __apply_cv<_Tp&, _Up, false, true>
{
    typedef volatile _Up& type;
};

template <class _Tp, class _Up>
struct __apply_cv<_Tp&, _Up, true, true>
{
    typedef const volatile _Up& type;
};

template <class _Tp, bool = is_integral<_Tp>::value || is_enum<_Tp>::value>
struct __make_signed{};

template <class _Tp>
struct __make_signed<_Tp, true>
{
    typedef typename __find_first<__signed_types, sizeof(_Tp)>::type type;
};

template <>
struct __make_signed<bool, true>{};
template <>
struct __make_signed<signed short, true>
{
    typedef short type;
};
template <>
struct __make_signed<unsigned short, true>
{
    typedef short type;
};
template <>
struct __make_signed<signed int, true>
{
    typedef int type;
};
template <>
struct __make_signed<unsigned int, true>
{
    typedef int type;
};
template <>
struct __make_signed<signed long, true>
{
    typedef long type;
};
template <>
struct __make_signed<unsigned long, true>
{
    typedef long type;
};
template <>
struct __make_signed<signed long long, true>
{
    typedef long long type;
};
template <>
struct __make_signed<unsigned long long, true>
{
    typedef long long type;
};
#ifndef __WI_LIBCPP_HAS_NO_INT128
template <>
struct __make_signed<__int128_t, true>
{
    typedef __int128_t type;
};
template <>
struct __make_signed<__uint128_t, true>
{
    typedef __int128_t type;
};
#endif

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS make_signed
{
    typedef typename __apply_cv<_Tp, typename __make_signed<typename remove_cv<_Tp>::type>::type>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using make_signed_t = typename make_signed<_Tp>::type;
#endif

template <class _Tp, bool = is_integral<_Tp>::value || is_enum<_Tp>::value>
struct __make_unsigned{};

template <class _Tp>
struct __make_unsigned<_Tp, true>
{
    typedef typename __find_first<__unsigned_types, sizeof(_Tp)>::type type;
};

template <>
struct __make_unsigned<bool, true>{};
template <>
struct __make_unsigned<signed short, true>
{
    typedef unsigned short type;
};
template <>
struct __make_unsigned<unsigned short, true>
{
    typedef unsigned short type;
};
template <>
struct __make_unsigned<signed int, true>
{
    typedef unsigned int type;
};
template <>
struct __make_unsigned<unsigned int, true>
{
    typedef unsigned int type;
};
template <>
struct __make_unsigned<signed long, true>
{
    typedef unsigned long type;
};
template <>
struct __make_unsigned<unsigned long, true>
{
    typedef unsigned long type;
};
template <>
struct __make_unsigned<signed long long, true>
{
    typedef unsigned long long type;
};
template <>
struct __make_unsigned<unsigned long long, true>
{
    typedef unsigned long long type;
};
#ifndef __WI_LIBCPP_HAS_NO_INT128
template <>
struct __make_unsigned<__int128_t, true>
{
    typedef __uint128_t type;
};
template <>
struct __make_unsigned<__uint128_t, true>
{
    typedef __uint128_t type;
};
#endif

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS make_unsigned
{
    typedef typename __apply_cv<_Tp, typename __make_unsigned<typename remove_cv<_Tp>::type>::type>::type type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using make_unsigned_t = typename make_unsigned<_Tp>::type;
#endif

#ifdef __WI_LIBCPP_HAS_NO_VARIADICS

template <class _Tp, class _Up = void, class _Vp = void>
struct __WI_LIBCPP_TEMPLATE_VIS common_type
{
public:
    typedef typename common_type<typename common_type<_Tp, _Up>::type, _Vp>::type type;
};

template <>
struct __WI_LIBCPP_TEMPLATE_VIS common_type<void, void, void>
{
public:
    typedef void type;
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS common_type<_Tp, void, void>
{
public:
    typedef typename common_type<_Tp, _Tp>::type type;
};

template <class _Tp, class _Up>
struct __WI_LIBCPP_TEMPLATE_VIS common_type<_Tp, _Up, void>
{
    typedef typename decay<decltype(true ? declval<_Tp>() : declval<_Up>())>::type type;
};

#else // __WI_LIBCPP_HAS_NO_VARIADICS

// bullet 1 - sizeof...(Tp) == 0

template <class... _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS common_type{};

// bullet 2 - sizeof...(Tp) == 1

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS common_type<_Tp> : public common_type<_Tp, _Tp>
{
};

// bullet 3 - sizeof...(Tp) == 2

template <class _Tp, class _Up, class = void>
struct __common_type2_imp
{
};

template <class _Tp, class _Up>
struct __common_type2_imp<_Tp, _Up, typename __void_t<decltype(true ? declval<_Tp>() : declval<_Up>())>::type>
{
    typedef typename decay<decltype(true ? declval<_Tp>() : declval<_Up>())>::type type;
};

template <class _Tp, class _Up, class _DTp = typename decay<_Tp>::type, class _DUp = typename decay<_Up>::type>
using __common_type2 =
    typename conditional<is_same<_Tp, _DTp>::value && is_same<_Up, _DUp>::value, __common_type2_imp<_Tp, _Up>, common_type<_DTp, _DUp>>::type;

template <class _Tp, class _Up>
struct __WI_LIBCPP_TEMPLATE_VIS common_type<_Tp, _Up> : __common_type2<_Tp, _Up>
{
};

// bullet 4 - sizeof...(Tp) > 2

template <class... Tp>
struct __common_types;

template <class, class = void>
struct __common_type_impl
{
};

template <class _Tp, class _Up>
struct __common_type_impl<__common_types<_Tp, _Up>, typename __void_t<typename common_type<_Tp, _Up>::type>::type>
{
    typedef typename common_type<_Tp, _Up>::type type;
};

template <class _Tp, class _Up, class... _Vp>
struct __common_type_impl<__common_types<_Tp, _Up, _Vp...>, typename __void_t<typename common_type<_Tp, _Up>::type>::type>
    : __common_type_impl<__common_types<typename common_type<_Tp, _Up>::type, _Vp...>>
{
};

template <class _Tp, class _Up, class... _Vp>
struct __WI_LIBCPP_TEMPLATE_VIS common_type<_Tp, _Up, _Vp...> : __common_type_impl<__common_types<_Tp, _Up, _Vp...>>
{
};

#if __WI_LIBCPP_STD_VER > 11
template <class... _Tp>
using common_type_t = typename common_type<_Tp...>::type;
#endif

#endif // __WI_LIBCPP_HAS_NO_VARIADICS

// is_assignable

template <typename, typename _Tp>
struct __select_2nd
{
    typedef _Tp type;
};

template <class _Tp, class _Arg>
typename __select_2nd<decltype((declval<_Tp>() = declval<_Arg>())), true_type>::type __is_assignable_test(int);

template <class, class>
false_type __is_assignable_test(...);

template <class _Tp, class _Arg, bool = is_void<_Tp>::value || is_void<_Arg>::value>
struct __is_assignable_imp : public decltype((__is_assignable_test<_Tp, _Arg>(0)))
{
};

template <class _Tp, class _Arg>
struct __is_assignable_imp<_Tp, _Arg, true> : public false_type
{
};

template <class _Tp, class _Arg>
struct is_assignable : public __is_assignable_imp<_Tp, _Arg>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp, class _Arg>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_assignable_v = is_assignable<_Tp, _Arg>::value;
#endif

// is_copy_assignable

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_copy_assignable
    : public is_assignable<typename add_lvalue_reference<_Tp>::type, typename add_lvalue_reference<typename add_const<_Tp>::type>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_copy_assignable_v = is_copy_assignable<_Tp>::value;
#endif

// is_move_assignable

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_move_assignable
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
    : public is_assignable<typename add_lvalue_reference<_Tp>::type, typename add_rvalue_reference<_Tp>::type>
{
};
#else
    : public is_copy_assignable<_Tp>
{
};
#endif

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_move_assignable_v = is_move_assignable<_Tp>::value;
#endif

// is_destructible

#if __WI_HAS_FEATURE_IS_DESTRUCTIBLE

template <class _Tp>
struct is_destructible : public integral_constant<bool, __is_destructible(_Tp)>
{
};

#else

//  if it's a reference, return true
//  if it's a function, return false
//  if it's   void,     return false
//  if it's an array of unknown bound, return false
//  Otherwise, return "std::declval<_Up&>().~_Up()" is well-formed
//    where _Up is remove_all_extents<_Tp>::type

template <class>
struct __is_destructible_apply
{
    typedef int type;
};

template <typename _Tp>
struct __is_destructor_wellformed
{
    template <typename _Tp1>
    static char __test(typename __is_destructible_apply<decltype(declval<_Tp1&>().~_Tp1())>::type);

    template <typename _Tp1>
    static __two __test(...);

    static const bool value = sizeof(__test<_Tp>(12)) == sizeof(char);
};

template <class _Tp, bool>
struct __destructible_imp;

template <class _Tp>
struct __destructible_imp<_Tp, false>
    : public integral_constant<bool, __is_destructor_wellformed<typename remove_all_extents<_Tp>::type>::value>
{
};

template <class _Tp>
struct __destructible_imp<_Tp, true> : public true_type
{
};

template <class _Tp, bool>
struct __destructible_false;

template <class _Tp>
struct __destructible_false<_Tp, false> : public __destructible_imp<_Tp, is_reference<_Tp>::value>
{
};

template <class _Tp>
struct __destructible_false<_Tp, true> : public false_type
{
};

template <class _Tp>
struct is_destructible : public __destructible_false<_Tp, is_function<_Tp>::value>
{
};

template <class _Tp>
struct is_destructible<_Tp[]> : public false_type
{
};

template <>
struct is_destructible<void> : public false_type
{
};

#endif // __WI_HAS_FEATURE_IS_DESTRUCTIBLE

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_destructible_v = is_destructible<_Tp>::value;
#endif

// move

#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

template <class _Tp>
inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR typename remove_reference<_Tp>::type&& move(_Tp&& __t) WI_NOEXCEPT
{
    typedef typename remove_reference<_Tp>::type _Up;
    return static_cast<_Up&&>(__t);
}

template <class _Tp>
inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR _Tp&& forward(typename remove_reference<_Tp>::type& __t) WI_NOEXCEPT
{
    return static_cast<_Tp&&>(__t);
}

template <class _Tp>
inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR _Tp&& forward(typename remove_reference<_Tp>::type&& __t) WI_NOEXCEPT
{
    static_assert(!is_lvalue_reference<_Tp>::value, "can not forward an rvalue as an lvalue");
    return static_cast<_Tp&&>(__t);
}

template <class _T1, class _T2 = _T1>
inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR_AFTER_CXX17 _T1 exchange(_T1& __obj, _T2&& __new_value)
{
    _T1 __old_value = wistd::move(__obj);
    __obj = wistd::forward<_T2>(__new_value);
    return __old_value;
}

#else // __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

template <class _Tp>
inline __WI_LIBCPP_INLINE_VISIBILITY _Tp& move(_Tp& __t)
{
    return __t;
}

template <class _Tp>
inline __WI_LIBCPP_INLINE_VISIBILITY const _Tp& move(const _Tp& __t)
{
    return __t;
}

template <class _Tp>
inline __WI_LIBCPP_INLINE_VISIBILITY _Tp& forward(typename remove_reference<_Tp>::type& __t) WI_NOEXCEPT
{
    return __t;
}

template <class _T1, class _T2 = _T1>
inline __WI_LIBCPP_INLINE_VISIBILITY _T1 exchange(_T1& __obj, const _T2& __new_value)
{
    _T1 __old_value = __obj;
    __obj = __new_value;
    return __old_value;
}

template <class _Tp>
class __rv
{
    typedef typename remove_reference<_Tp>::type _Trr;
    _Trr& t_;

public:
    __WI_LIBCPP_INLINE_VISIBILITY
    _Trr* operator->()
    {
        return &t_;
    }
    __WI_LIBCPP_INLINE_VISIBILITY
    explicit __rv(_Trr& __t) : t_(__t)
    {
    }
};

#endif // __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct __WI_LIBCPP_TEMPLATE_VIS less : binary_function<_Tp, _Tp, bool>
{
    __WI_LIBCPP_NODISCARD_ATTRIBUTE __WI_LIBCPP_CONSTEXPR_AFTER_CXX11 __WI_LIBCPP_INLINE_VISIBILITY bool operator()(
        const _Tp& __x, const _Tp& __y) const
    {
        return __x < __y;
    }
};

#if __WI_LIBCPP_STD_VER > 11
template <>
struct __WI_LIBCPP_TEMPLATE_VIS less<void>
{
    template <class _T1, class _T2>
    __WI_LIBCPP_NODISCARD_ATTRIBUTE __WI_LIBCPP_CONSTEXPR_AFTER_CXX11 __WI_LIBCPP_INLINE_VISIBILITY auto operator()(_T1&& __t, _T2&& __u) const
        __WI_NOEXCEPT_(noexcept(wistd::forward<_T1>(__t) < wistd::forward<_T2>(__u)))
            -> decltype(wistd::forward<_T1>(__t) < wistd::forward<_T2>(__u))
    {
        return wistd::forward<_T1>(__t) < wistd::forward<_T2>(__u);
    }
    typedef void is_transparent;
};
#endif

#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

template <class _Tp>
inline __WI_LIBCPP_INLINE_VISIBILITY typename decay<_Tp>::type __decay_copy(_Tp&& __t)
{
    return wistd::forward<_Tp>(__t);
}

#else

template <class _Tp>
inline __WI_LIBCPP_INLINE_VISIBILITY typename decay<_Tp>::type __decay_copy(const _Tp& __t)
{
    return wistd::forward<_Tp>(__t);
}

#endif

#ifndef __WI_LIBCPP_HAS_NO_VARIADICS

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...), true, false>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...), true, false>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...) const, true, false>
{
    typedef _Class const _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...) const, true, false>
{
    typedef _Class const _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...) volatile, true, false>
{
    typedef _Class volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...) volatile, true, false>
{
    typedef _Class volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...) const volatile, true, false>
{
    typedef _Class const volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...) const volatile, true, false>
{
    typedef _Class const volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

#if __WI_HAS_FEATURE_REFERENCE_QUALIFIED_FUNCTIONS || (defined(__WI_GNUC_VER) && __WI_GNUC_VER >= 409)

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...)&, true, false>
{
    typedef _Class& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...)&, true, false>
{
    typedef _Class& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...) const&, true, false>
{
    typedef _Class const& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...) const&, true, false>
{
    typedef _Class const& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...) volatile&, true, false>
{
    typedef _Class volatile& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...) volatile&, true, false>
{
    typedef _Class volatile& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...) const volatile&, true, false>
{
    typedef _Class const volatile& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...) const volatile&, true, false>
{
    typedef _Class const volatile& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...)&&, true, false>
{
    typedef _Class&& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...)&&, true, false>
{
    typedef _Class&& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...) const&&, true, false>
{
    typedef _Class const&& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...) const&&, true, false>
{
    typedef _Class const&& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...) volatile&&, true, false>
{
    typedef _Class volatile&& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...) volatile&&, true, false>
{
    typedef _Class volatile&& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param...) const volatile&&, true, false>
{
    typedef _Class const volatile&& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param...);
};

template <class _Rp, class _Class, class... _Param>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_Param..., ...) const volatile&&, true, false>
{
    typedef _Class const volatile&& _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_Param..., ...);
};

#endif // __WI_HAS_FEATURE_REFERENCE_QUALIFIED_FUNCTIONS || __WI_GNUC_VER >= 409

#else // __WI_LIBCPP_HAS_NO_VARIADICS

template <class _Rp, class _Class>
struct __member_pointer_traits_imp<_Rp (_Class::*)(), true, false>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)();
};

template <class _Rp, class _Class>
struct __member_pointer_traits_imp<_Rp (_Class::*)(...), true, false>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(...);
};

template <class _Rp, class _Class, class _P0>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0), true, false>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0);
};

template <class _Rp, class _Class, class _P0>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, ...), true, false>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, ...);
};

template <class _Rp, class _Class, class _P0, class _P1>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1), true, false>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1);
};

template <class _Rp, class _Class, class _P0, class _P1>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, ...), true, false>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, ...);
};

template <class _Rp, class _Class, class _P0, class _P1, class _P2>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, _P2), true, false>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, _P2);
};

template <class _Rp, class _Class, class _P0, class _P1, class _P2>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, _P2, ...), true, false>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, _P2, ...);
};

template <class _Rp, class _Class>
struct __member_pointer_traits_imp<_Rp (_Class::*)() const, true, false>
{
    typedef _Class const _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)();
};

template <class _Rp, class _Class>
struct __member_pointer_traits_imp<_Rp (_Class::*)(...) const, true, false>
{
    typedef _Class const _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(...);
};

template <class _Rp, class _Class, class _P0>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0) const, true, false>
{
    typedef _Class const _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0);
};

template <class _Rp, class _Class, class _P0>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, ...) const, true, false>
{
    typedef _Class const _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, ...);
};

template <class _Rp, class _Class, class _P0, class _P1>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1) const, true, false>
{
    typedef _Class const _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1);
};

template <class _Rp, class _Class, class _P0, class _P1>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, ...) const, true, false>
{
    typedef _Class const _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, ...);
};

template <class _Rp, class _Class, class _P0, class _P1, class _P2>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, _P2) const, true, false>
{
    typedef _Class const _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, _P2);
};

template <class _Rp, class _Class, class _P0, class _P1, class _P2>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, _P2, ...) const, true, false>
{
    typedef _Class const _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, _P2, ...);
};

template <class _Rp, class _Class>
struct __member_pointer_traits_imp<_Rp (_Class::*)() volatile, true, false>
{
    typedef _Class volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)();
};

template <class _Rp, class _Class>
struct __member_pointer_traits_imp<_Rp (_Class::*)(...) volatile, true, false>
{
    typedef _Class volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(...);
};

template <class _Rp, class _Class, class _P0>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0) volatile, true, false>
{
    typedef _Class volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0);
};

template <class _Rp, class _Class, class _P0>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, ...) volatile, true, false>
{
    typedef _Class volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, ...);
};

template <class _Rp, class _Class, class _P0, class _P1>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1) volatile, true, false>
{
    typedef _Class volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1);
};

template <class _Rp, class _Class, class _P0, class _P1>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, ...) volatile, true, false>
{
    typedef _Class volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, ...);
};

template <class _Rp, class _Class, class _P0, class _P1, class _P2>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, _P2) volatile, true, false>
{
    typedef _Class volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, _P2);
};

template <class _Rp, class _Class, class _P0, class _P1, class _P2>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, _P2, ...) volatile, true, false>
{
    typedef _Class volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, _P2, ...);
};

template <class _Rp, class _Class>
struct __member_pointer_traits_imp<_Rp (_Class::*)() const volatile, true, false>
{
    typedef _Class const volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)();
};

template <class _Rp, class _Class>
struct __member_pointer_traits_imp<_Rp (_Class::*)(...) const volatile, true, false>
{
    typedef _Class const volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(...);
};

template <class _Rp, class _Class, class _P0>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0) const volatile, true, false>
{
    typedef _Class const volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0);
};

template <class _Rp, class _Class, class _P0>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, ...) const volatile, true, false>
{
    typedef _Class const volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, ...);
};

template <class _Rp, class _Class, class _P0, class _P1>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1) const volatile, true, false>
{
    typedef _Class const volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1);
};

template <class _Rp, class _Class, class _P0, class _P1>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, ...) const volatile, true, false>
{
    typedef _Class const volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, ...);
};

template <class _Rp, class _Class, class _P0, class _P1, class _P2>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, _P2) const volatile, true, false>
{
    typedef _Class const volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, _P2);
};

template <class _Rp, class _Class, class _P0, class _P1, class _P2>
struct __member_pointer_traits_imp<_Rp (_Class::*)(_P0, _P1, _P2, ...) const volatile, true, false>
{
    typedef _Class const volatile _ClassType;
    typedef _Rp _ReturnType;
    typedef _Rp(_FnType)(_P0, _P1, _P2, ...);
};

#endif // __WI_LIBCPP_HAS_NO_VARIADICS

template <class _Rp, class _Class>
struct __member_pointer_traits_imp<_Rp _Class::*, false, true>
{
    typedef _Class _ClassType;
    typedef _Rp _ReturnType;
};

template <class _Mp>
struct __member_pointer_traits
    : public __member_pointer_traits_imp<typename remove_cv<_Mp>::type, is_member_function_pointer<_Mp>::value, is_member_object_pointer<_Mp>::value>
{
    //     typedef ... _ClassType;
    //     typedef ... _ReturnType;
    //     typedef ... _FnType;
};

template <class _DecayedFp>
struct __member_pointer_class_type
{
};

template <class _Ret, class _ClassType>
struct __member_pointer_class_type<_Ret _ClassType::*>
{
    typedef _ClassType type;
};

// result_of

template <class _Callable>
class result_of;

#ifdef __WI_LIBCPP_HAS_NO_VARIADICS

template <class _Fn, bool, bool>
class __result_of
{
};

template <class _Fn>
class __result_of<_Fn(), true, false>
{
public:
    typedef decltype(declval<_Fn>()()) type;
};

template <class _Fn, class _A0>
class __result_of<_Fn(_A0), true, false>
{
public:
    typedef decltype(declval<_Fn>()(declval<_A0>())) type;
};

template <class _Fn, class _A0, class _A1>
class __result_of<_Fn(_A0, _A1), true, false>
{
public:
    typedef decltype(declval<_Fn>()(declval<_A0>(), declval<_A1>())) type;
};

template <class _Fn, class _A0, class _A1, class _A2>
class __result_of<_Fn(_A0, _A1, _A2), true, false>
{
public:
    typedef decltype(declval<_Fn>()(declval<_A0>(), declval<_A1>(), declval<_A2>())) type;
};

template <class _Mp, class _Tp, bool _IsMemberFunctionPtr>
struct __result_of_mp;

// member function pointer

template <class _Mp, class _Tp>
struct __result_of_mp<_Mp, _Tp, true> : public __identity<typename __member_pointer_traits<_Mp>::_ReturnType>
{
};

// member data pointer

template <class _Mp, class _Tp, bool>
struct __result_of_mdp;

template <class _Rp, class _Class, class _Tp>
struct __result_of_mdp<_Rp _Class::*, _Tp, false>
{
    typedef typename __apply_cv<decltype(*declval<_Tp>()), _Rp>::type& type;
};

template <class _Rp, class _Class, class _Tp>
struct __result_of_mdp<_Rp _Class::*, _Tp, true>
{
    typedef typename __apply_cv<_Tp, _Rp>::type& type;
};

template <class _Rp, class _Class, class _Tp>
struct __result_of_mp<_Rp _Class::*, _Tp, false>
    : public __result_of_mdp<_Rp _Class::*, _Tp, is_base_of<_Class, typename remove_reference<_Tp>::type>::value>
{
};

template <class _Fn, class _Tp>
class __result_of<_Fn(_Tp), false, true> // _Fn must be member pointer
    : public __result_of_mp<typename remove_reference<_Fn>::type, _Tp, is_member_function_pointer<typename remove_reference<_Fn>::type>::value>
{
};

template <class _Fn, class _Tp, class _A0>
class __result_of<_Fn(_Tp, _A0), false, true> // _Fn must be member pointer
    : public __result_of_mp<typename remove_reference<_Fn>::type, _Tp, is_member_function_pointer<typename remove_reference<_Fn>::type>::value>
{
};

template <class _Fn, class _Tp, class _A0, class _A1>
class __result_of<_Fn(_Tp, _A0, _A1), false, true> // _Fn must be member pointer
    : public __result_of_mp<typename remove_reference<_Fn>::type, _Tp, is_member_function_pointer<typename remove_reference<_Fn>::type>::value>
{
};

template <class _Fn, class _Tp, class _A0, class _A1, class _A2>
class __result_of<_Fn(_Tp, _A0, _A1, _A2), false, true> // _Fn must be member pointer
    : public __result_of_mp<typename remove_reference<_Fn>::type, _Tp, is_member_function_pointer<typename remove_reference<_Fn>::type>::value>
{
};

// result_of

template <class _Fn>
class __WI_LIBCPP_TEMPLATE_VIS result_of<_Fn()>
    : public __result_of<
          _Fn(),
          is_class<typename remove_reference<_Fn>::type>::value ||
              is_function<typename remove_pointer<typename remove_reference<_Fn>::type>::type>::value,
          is_member_pointer<typename remove_reference<_Fn>::type>::value>
{
};

template <class _Fn, class _A0>
class __WI_LIBCPP_TEMPLATE_VIS result_of<_Fn(_A0)>
    : public __result_of<
          _Fn(_A0),
          is_class<typename remove_reference<_Fn>::type>::value ||
              is_function<typename remove_pointer<typename remove_reference<_Fn>::type>::type>::value,
          is_member_pointer<typename remove_reference<_Fn>::type>::value>
{
};

template <class _Fn, class _A0, class _A1>
class __WI_LIBCPP_TEMPLATE_VIS result_of<_Fn(_A0, _A1)>
    : public __result_of<
          _Fn(_A0, _A1),
          is_class<typename remove_reference<_Fn>::type>::value ||
              is_function<typename remove_pointer<typename remove_reference<_Fn>::type>::type>::value,
          is_member_pointer<typename remove_reference<_Fn>::type>::value>
{
};

template <class _Fn, class _A0, class _A1, class _A2>
class __WI_LIBCPP_TEMPLATE_VIS result_of<_Fn(_A0, _A1, _A2)>
    : public __result_of<
          _Fn(_A0, _A1, _A2),
          is_class<typename remove_reference<_Fn>::type>::value ||
              is_function<typename remove_pointer<typename remove_reference<_Fn>::type>::type>::value,
          is_member_pointer<typename remove_reference<_Fn>::type>::value>
{
};

#endif // __WI_LIBCPP_HAS_NO_VARIADICS

// template <class T, class... Args> struct is_constructible;

namespace __is_construct
{
    struct __nat
    {
    };
} // namespace __is_construct

#if !defined(__WI_LIBCPP_CXX03_LANG) && (!__WI_HAS_FEATURE_IS_CONSTRUCTIBLE || defined(__WI_LIBCPP_TESTING_FALLBACK_IS_CONSTRUCTIBLE))

template <class _Tp, class... _Args>
struct __libcpp_is_constructible;

template <class _To, class _From>
struct __is_invalid_base_to_derived_cast
{
    static_assert(is_reference<_To>::value, "Wrong specialization");
    using _RawFrom = __uncvref_t<_From>;
    using _RawTo = __uncvref_t<_To>;
    static const bool value =
        __lazy_and<__lazy_not<is_same<_RawFrom, _RawTo>>, is_base_of<_RawFrom, _RawTo>, __lazy_not<__libcpp_is_constructible<_RawTo, _From>>>::value;
};

template <class _To, class _From>
struct __is_invalid_lvalue_to_rvalue_cast : false_type
{
    static_assert(is_reference<_To>::value, "Wrong specialization");
};

template <class _ToRef, class _FromRef>
struct __is_invalid_lvalue_to_rvalue_cast<_ToRef&&, _FromRef&>
{
    using _RawFrom = __uncvref_t<_FromRef>;
    using _RawTo = __uncvref_t<_ToRef>;
    static const bool value =
        __lazy_and<__lazy_not<is_function<_RawTo>>, __lazy_or<is_same<_RawFrom, _RawTo>, is_base_of<_RawTo, _RawFrom>>>::value;
};

struct __is_constructible_helper
{
    template <class _To>
    static void __eat(_To);

    // This overload is needed to work around a Clang bug that disallows
    // static_cast<T&&>(e) for non-reference-compatible types.
    // Example: static_cast<int&&>(declval<double>());
    // NOTE: The static_cast implementation below is required to support
    //  classes with explicit conversion operators.
    template <class _To, class _From, class = decltype(__eat<_To>(declval<_From>()))>
    static true_type __test_cast(int);

    template <class _To, class _From, class = decltype(static_cast<_To>(declval<_From>()))>
    static integral_constant<bool, !__is_invalid_base_to_derived_cast<_To, _From>::value && !__is_invalid_lvalue_to_rvalue_cast<_To, _From>::value> __test_cast(
        long);

    template <class, class>
    static false_type __test_cast(...);

    template <class _Tp, class... _Args, class = decltype(_Tp(declval<_Args>()...))>
    static true_type __test_nary(int);
    template <class _Tp, class...>
    static false_type __test_nary(...);

    template <class _Tp, class _A0, class = decltype(::new _Tp(declval<_A0>()))>
    static is_destructible<_Tp> __test_unary(int);
    template <class, class>
    static false_type __test_unary(...);
};

template <class _Tp, bool = is_void<_Tp>::value>
struct __is_default_constructible : decltype(__is_constructible_helper::__test_nary<_Tp>(0))
{
};

template <class _Tp>
struct __is_default_constructible<_Tp, true> : false_type
{
};

template <class _Tp>
struct __is_default_constructible<_Tp[], false> : false_type
{
};

template <class _Tp, size_t _Nx>
struct __is_default_constructible<_Tp[_Nx], false> : __is_default_constructible<typename remove_all_extents<_Tp>::type>
{
};

template <class _Tp, class... _Args>
struct __libcpp_is_constructible
{
    static_assert(sizeof...(_Args) > 1, "Wrong specialization");
    typedef decltype(__is_constructible_helper::__test_nary<_Tp, _Args...>(0)) type;
};

template <class _Tp>
struct __libcpp_is_constructible<_Tp> : __is_default_constructible<_Tp>
{
};

template <class _Tp, class _A0>
struct __libcpp_is_constructible<_Tp, _A0> : public decltype(__is_constructible_helper::__test_unary<_Tp, _A0>(0))
{
};

template <class _Tp, class _A0>
struct __libcpp_is_constructible<_Tp&, _A0> : public decltype(__is_constructible_helper::__test_cast<_Tp&, _A0>(0))
{
};

template <class _Tp, class _A0>
struct __libcpp_is_constructible<_Tp&&, _A0> : public decltype(__is_constructible_helper::__test_cast<_Tp&&, _A0>(0))
{
};

#endif

#if __WI_HAS_FEATURE_IS_CONSTRUCTIBLE
template <class _Tp, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS is_constructible : public integral_constant<bool, __is_constructible(_Tp, _Args...)>
{
};
#elif !defined(__WI_LIBCPP_CXX03_LANG)
template <class _Tp, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS is_constructible : public __libcpp_is_constructible<_Tp, _Args...>::type
{
};
#else
// template <class T> struct is_constructible0;

//      main is_constructible0 test

template <class _Tp>
decltype((_Tp(), true_type())) __is_constructible0_test(_Tp&);

false_type __is_constructible0_test(__any);

template <class _Tp, class _A0>
decltype((_Tp(declval<_A0>()), true_type())) __is_constructible1_test(_Tp&, _A0&);

template <class _A0>
false_type __is_constructible1_test(__any, _A0&);

template <class _Tp, class _A0, class _A1>
decltype((_Tp(declval<_A0>(), declval<_A1>()), true_type())) __is_constructible2_test(_Tp&, _A0&, _A1&);

template <class _A0, class _A1>
false_type __is_constructible2_test(__any, _A0&, _A1&);

template <class _Tp, class _A0, class _A1, class _A2>
decltype((_Tp(declval<_A0>(), declval<_A1>(), declval<_A2>()), true_type())) __is_constructible3_test(_Tp&, _A0&, _A1&, _A2&);

template <class _A0, class _A1, class _A2>
false_type __is_constructible3_test(__any, _A0&, _A1&, _A2&);

template <bool, class _Tp>
struct __is_constructible0_imp // false, _Tp is not a scalar
    : public common_type<decltype(__is_constructible0_test(declval<_Tp&>()))>::type
{
};

template <bool, class _Tp, class _A0>
struct __is_constructible1_imp // false, _Tp is not a scalar
    : public common_type<decltype(__is_constructible1_test(declval<_Tp&>(), declval<_A0&>()))>::type
{
};

template <bool, class _Tp, class _A0, class _A1>
struct __is_constructible2_imp // false, _Tp is not a scalar
    : public common_type<decltype(__is_constructible2_test(declval<_Tp&>(), declval<_A0>(), declval<_A1>()))>::type
{
};

template <bool, class _Tp, class _A0, class _A1, class _A2>
struct __is_constructible3_imp // false, _Tp is not a scalar
    : public common_type<decltype(__is_constructible3_test(declval<_Tp&>(), declval<_A0>(), declval<_A1>(), declval<_A2>()))>::type
{
};

//      handle scalars and reference types

//      Scalars are default constructible, references are not

template <class _Tp>
struct __is_constructible0_imp<true, _Tp> : public is_scalar<_Tp>
{
};

template <class _Tp, class _A0>
struct __is_constructible1_imp<true, _Tp, _A0> : public is_convertible<_A0, _Tp>
{
};

template <class _Tp, class _A0, class _A1>
struct __is_constructible2_imp<true, _Tp, _A0, _A1> : public false_type
{
};

template <class _Tp, class _A0, class _A1, class _A2>
struct __is_constructible3_imp<true, _Tp, _A0, _A1, _A2> : public false_type
{
};

//      Treat scalars and reference types separately

template <bool, class _Tp>
struct __is_constructible0_void_check : public __is_constructible0_imp<is_scalar<_Tp>::value || is_reference<_Tp>::value, _Tp>
{
};

template <bool, class _Tp, class _A0>
struct __is_constructible1_void_check : public __is_constructible1_imp<is_scalar<_Tp>::value || is_reference<_Tp>::value, _Tp, _A0>
{
};

template <bool, class _Tp, class _A0, class _A1>
struct __is_constructible2_void_check
    : public __is_constructible2_imp<is_scalar<_Tp>::value || is_reference<_Tp>::value, _Tp, _A0, _A1>
{
};

template <bool, class _Tp, class _A0, class _A1, class _A2>
struct __is_constructible3_void_check
    : public __is_constructible3_imp<is_scalar<_Tp>::value || is_reference<_Tp>::value, _Tp, _A0, _A1, _A2>
{
};

//      If any of T or Args is void, is_constructible should be false

template <class _Tp>
struct __is_constructible0_void_check<true, _Tp> : public false_type
{
};

template <class _Tp, class _A0>
struct __is_constructible1_void_check<true, _Tp, _A0> : public false_type
{
};

template <class _Tp, class _A0, class _A1>
struct __is_constructible2_void_check<true, _Tp, _A0, _A1> : public false_type
{
};

template <class _Tp, class _A0, class _A1, class _A2>
struct __is_constructible3_void_check<true, _Tp, _A0, _A1, _A2> : public false_type
{
};

//      is_constructible entry point

template <class _Tp, class _A0 = __is_construct::__nat, class _A1 = __is_construct::__nat, class _A2 = __is_construct::__nat>
struct __WI_LIBCPP_TEMPLATE_VIS is_constructible
    : public __is_constructible3_void_check<
          is_void<_Tp>::value || is_abstract<_Tp>::value || is_function<_Tp>::value || is_void<_A0>::value || is_void<_A1>::value || is_void<_A2>::value,
          _Tp,
          _A0,
          _A1,
          _A2>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_constructible<_Tp, __is_construct::__nat, __is_construct::__nat>
    : public __is_constructible0_void_check<is_void<_Tp>::value || is_abstract<_Tp>::value || is_function<_Tp>::value, _Tp>
{
};

template <class _Tp, class _A0>
struct __WI_LIBCPP_TEMPLATE_VIS is_constructible<_Tp, _A0, __is_construct::__nat>
    : public __is_constructible1_void_check<is_void<_Tp>::value || is_abstract<_Tp>::value || is_function<_Tp>::value || is_void<_A0>::value, _Tp, _A0>
{
};

template <class _Tp, class _A0, class _A1>
struct __WI_LIBCPP_TEMPLATE_VIS is_constructible<_Tp, _A0, _A1, __is_construct::__nat>
    : public __is_constructible2_void_check<is_void<_Tp>::value || is_abstract<_Tp>::value || is_function<_Tp>::value || is_void<_A0>::value || is_void<_A1>::value, _Tp, _A0, _A1>
{
};

//      Array types are default constructible if their element type
//      is default constructible

template <class _Ap, size_t _Np>
struct __is_constructible0_imp<false, _Ap[_Np]> : public is_constructible<typename remove_all_extents<_Ap>::type>
{
};

template <class _Ap, size_t _Np, class _A0>
struct __is_constructible1_imp<false, _Ap[_Np], _A0> : public false_type
{
};

template <class _Ap, size_t _Np, class _A0, class _A1>
struct __is_constructible2_imp<false, _Ap[_Np], _A0, _A1> : public false_type
{
};

template <class _Ap, size_t _Np, class _A0, class _A1, class _A2>
struct __is_constructible3_imp<false, _Ap[_Np], _A0, _A1, _A2> : public false_type
{
};

//      Incomplete array types are not constructible

template <class _Ap>
struct __is_constructible0_imp<false, _Ap[]> : public false_type
{
};

template <class _Ap, class _A0>
struct __is_constructible1_imp<false, _Ap[], _A0> : public false_type
{
};

template <class _Ap, class _A0, class _A1>
struct __is_constructible2_imp<false, _Ap[], _A0, _A1> : public false_type
{
};

template <class _Ap, class _A0, class _A1, class _A2>
struct __is_constructible3_imp<false, _Ap[], _A0, _A1, _A2> : public false_type
{
};

#endif // __WI_HAS_FEATURE_IS_CONSTRUCTIBLE

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES) && !defined(__WI_LIBCPP_HAS_NO_VARIADICS)
template <class _Tp, class... _Args>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_constructible_v = is_constructible<_Tp, _Args...>::value;
#endif

// is_default_constructible

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_default_constructible : public is_constructible<_Tp>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_default_constructible_v = is_default_constructible<_Tp>::value;
#endif

// is_copy_constructible

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_copy_constructible
    : public is_constructible<_Tp, typename add_lvalue_reference<typename add_const<_Tp>::type>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_copy_constructible_v = is_copy_constructible<_Tp>::value;
#endif

// is_move_constructible

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_move_constructible
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
    : public is_constructible<_Tp, typename add_rvalue_reference<_Tp>::type>
#else
    : public is_copy_constructible<_Tp>
#endif
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_move_constructible_v = is_move_constructible<_Tp>::value;
#endif

// is_trivially_constructible

#ifndef __WI_LIBCPP_HAS_NO_VARIADICS

#if __WI_HAS_FEATURE_IS_TRIVIALLY_CONSTRUCTIBLE || __WI_GNUC_VER >= 501

template <class _Tp, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible : integral_constant<bool, __is_trivially_constructible(_Tp, _Args...)>
{
};

#else // !__WI_HAS_FEATURE_IS_TRIVIALLY_CONSTRUCTIBLE

template <class _Tp, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible : false_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp>
#if __WI_HAS_FEATURE_HAS_TRIVIAL_CONSTRUCTOR
    : integral_constant<bool, __has_trivial_constructor(_Tp)>
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
#endif
{
};

template <class _Tp>
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, _Tp&&>
#else
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, _Tp>
#endif
    : integral_constant<bool, is_scalar<_Tp>::value>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, const _Tp&> : integral_constant<bool, is_scalar<_Tp>::value>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, _Tp&> : integral_constant<bool, is_scalar<_Tp>::value>
{
};

#endif // !__WI_HAS_FEATURE_IS_TRIVIALLY_CONSTRUCTIBLE

#else // __WI_LIBCPP_HAS_NO_VARIADICS

template <class _Tp, class _A0 = __is_construct::__nat, class _A1 = __is_construct::__nat>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible : false_type
{
};

#if __WI_HAS_FEATURE_IS_TRIVIALLY_CONSTRUCTIBLE || __WI_GNUC_VER >= 501

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, __is_construct::__nat, __is_construct::__nat>
    : integral_constant<bool, __is_trivially_constructible(_Tp)>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, _Tp, __is_construct::__nat>
    : integral_constant<bool, __is_trivially_constructible(_Tp, _Tp)>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, const _Tp&, __is_construct::__nat>
    : integral_constant<bool, __is_trivially_constructible(_Tp, const _Tp&)>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, _Tp&, __is_construct::__nat>
    : integral_constant<bool, __is_trivially_constructible(_Tp, _Tp&)>
{
};

#else // !__WI_HAS_FEATURE_IS_TRIVIALLY_CONSTRUCTIBLE

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, __is_construct::__nat, __is_construct::__nat>
    : integral_constant<bool, is_scalar<_Tp>::value>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, _Tp, __is_construct::__nat>
    : integral_constant<bool, is_scalar<_Tp>::value>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, const _Tp&, __is_construct::__nat>
    : integral_constant<bool, is_scalar<_Tp>::value>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_constructible<_Tp, _Tp&, __is_construct::__nat>
    : integral_constant<bool, is_scalar<_Tp>::value>
{
};

#endif // !__WI_HAS_FEATURE_IS_TRIVIALLY_CONSTRUCTIBLE

#endif // __WI_LIBCPP_HAS_NO_VARIADICS

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES) && !defined(__WI_LIBCPP_HAS_NO_VARIADICS)
template <class _Tp, class... _Args>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_trivially_constructible_v = is_trivially_constructible<_Tp, _Args...>::value;
#endif

// is_trivially_default_constructible

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_default_constructible : public is_trivially_constructible<_Tp>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_trivially_default_constructible_v = is_trivially_default_constructible<_Tp>::value;
#endif

// is_trivially_copy_constructible

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_copy_constructible
    : public is_trivially_constructible<_Tp, typename add_lvalue_reference<const _Tp>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_trivially_copy_constructible_v = is_trivially_copy_constructible<_Tp>::value;
#endif

// is_trivially_move_constructible

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_move_constructible
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
    : public is_trivially_constructible<_Tp, typename add_rvalue_reference<_Tp>::type>
#else
    : public is_trivially_copy_constructible<_Tp>
#endif
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_trivially_move_constructible_v = is_trivially_move_constructible<_Tp>::value;
#endif

// is_trivially_assignable

#if __WI_HAS_FEATURE_IS_TRIVIALLY_ASSIGNABLE || __WI_GNUC_VER >= 501

template <class _Tp, class _Arg>
struct is_trivially_assignable : integral_constant<bool, __is_trivially_assignable(_Tp, _Arg)>
{
};

#else // !__WI_HAS_FEATURE_IS_TRIVIALLY_ASSIGNABLE

template <class _Tp, class _Arg>
struct is_trivially_assignable : public false_type
{
};

template <class _Tp>
struct is_trivially_assignable<_Tp&, _Tp> : integral_constant<bool, is_scalar<_Tp>::value>
{
};

template <class _Tp>
struct is_trivially_assignable<_Tp&, _Tp&> : integral_constant<bool, is_scalar<_Tp>::value>
{
};

template <class _Tp>
struct is_trivially_assignable<_Tp&, const _Tp&> : integral_constant<bool, is_scalar<_Tp>::value>
{
};

#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

template <class _Tp>
struct is_trivially_assignable<_Tp&, _Tp&&> : integral_constant<bool, is_scalar<_Tp>::value>
{
};

#endif // __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

#endif // !__WI_HAS_FEATURE_IS_TRIVIALLY_ASSIGNABLE

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp, class _Arg>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_trivially_assignable_v = is_trivially_assignable<_Tp, _Arg>::value;
#endif

// is_trivially_copy_assignable

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_copy_assignable
    : public is_trivially_assignable<typename add_lvalue_reference<_Tp>::type, typename add_lvalue_reference<typename add_const<_Tp>::type>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_trivially_copy_assignable_v = is_trivially_copy_assignable<_Tp>::value;
#endif

// is_trivially_move_assignable

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_move_assignable : public is_trivially_assignable<
                                                                   typename add_lvalue_reference<_Tp>::type,
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
                                                                   typename add_rvalue_reference<_Tp>::type>
#else
                                                                   typename add_lvalue_reference<_Tp>::type>
#endif
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_trivially_move_assignable_v = is_trivially_move_assignable<_Tp>::value;
#endif

// is_trivially_destructible

#if __WI_HAS_FEATURE_HAS_TRIVIAL_DESTRUCTOR || (__WI_GNUC_VER >= 403)

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_destructible
    : public integral_constant<bool, is_destructible<_Tp>::value&& __has_trivial_destructor(_Tp)>
{
};

#else

template <class _Tp>
struct __libcpp_trivial_destructor : public integral_constant<bool, is_scalar<_Tp>::value || is_reference<_Tp>::value>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_destructible : public __libcpp_trivial_destructor<typename remove_all_extents<_Tp>::type>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_destructible<_Tp[]> : public false_type
{
};

#endif

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_trivially_destructible_v = is_trivially_destructible<_Tp>::value;
#endif

// is_nothrow_constructible

#if 0
    template <class _Tp, class... _Args>
    struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible
        : public integral_constant<bool, __is_nothrow_constructible(_Tp(_Args...))>
    {
    };

#else

#ifndef __WI_LIBCPP_HAS_NO_VARIADICS

#if !defined(__WI_LIBCPP_HAS_NO_NOEXCEPT) || (__WI_GNUC_VER >= 407 && __cplusplus >= 201103L)

template <bool, bool, class _Tp, class... _Args>
struct __libcpp_is_nothrow_constructible;

template <class _Tp, class... _Args>
struct __libcpp_is_nothrow_constructible</*is constructible*/ true, /*is reference*/ false, _Tp, _Args...>
    : public integral_constant<bool, noexcept(_Tp(declval<_Args>()...))>
{
};

template <class _Tp>
void __implicit_conversion_to(_Tp) noexcept
{
}

template <class _Tp, class _Arg>
struct __libcpp_is_nothrow_constructible</*is constructible*/ true, /*is reference*/ true, _Tp, _Arg>
    : public integral_constant<bool, noexcept(__implicit_conversion_to<_Tp>(declval<_Arg>()))>
{
};

template <class _Tp, bool _IsReference, class... _Args>
struct __libcpp_is_nothrow_constructible</*is constructible*/ false, _IsReference, _Tp, _Args...> : public false_type
{
};

template <class _Tp, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible
    : __libcpp_is_nothrow_constructible<is_constructible<_Tp, _Args...>::value, is_reference<_Tp>::value, _Tp, _Args...>
{
};

template <class _Tp, size_t _Ns>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible<_Tp[_Ns]>
    : __libcpp_is_nothrow_constructible<is_constructible<_Tp>::value, is_reference<_Tp>::value, _Tp>
{
};

#else // !defined(__WI_LIBCPP_HAS_NO_NOEXCEPT)

template <class _Tp, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible : false_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible<_Tp>
#if __WI_HAS_FEATURE_HAS_NOTHROW_CONSTRUCTOR
    : integral_constant<bool, __has_nothrow_constructor(_Tp)>
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
#endif
{
};

template <class _Tp>
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible<_Tp, _Tp&&>
#else
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible<_Tp, _Tp>
#endif
#if __WI_HAS_FEATURE_HAS_NOTHROW_COPY
    : integral_constant<bool, __has_nothrow_copy(_Tp)>
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
#endif
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible<_Tp, const _Tp&>
#if __WI_HAS_FEATURE_HAS_NOTHROW_COPY
    : integral_constant<bool, __has_nothrow_copy(_Tp)>
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
#endif
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible<_Tp, _Tp&>
#if __WI_HAS_FEATURE_HAS_NOTHROW_COPY
    : integral_constant<bool, __has_nothrow_copy(_Tp)>
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
#endif
{
};

#endif // !defined(__WI_LIBCPP_HAS_NO_NOEXCEPT)

#else // __WI_LIBCPP_HAS_NO_VARIADICS

template <class _Tp, class _A0 = __is_construct::__nat, class _A1 = __is_construct::__nat>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible : false_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible<_Tp, __is_construct::__nat, __is_construct::__nat>
#if __WI_HAS_FEATURE_HAS_NOTHROW_CONSTRUCTOR
    : integral_constant<bool, __has_nothrow_constructor(_Tp)>
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
#endif
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible<_Tp, _Tp, __is_construct::__nat>
#if __WI_HAS_FEATURE_HAS_NOTHROW_COPY
    : integral_constant<bool, __has_nothrow_copy(_Tp)>
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
#endif
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible<_Tp, const _Tp&, __is_construct::__nat>
#if __WI_HAS_FEATURE_HAS_NOTHROW_COPY
    : integral_constant<bool, __has_nothrow_copy(_Tp)>
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
#endif
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_constructible<_Tp, _Tp&, __is_construct::__nat>
#if __WI_HAS_FEATURE_HAS_NOTHROW_COPY
    : integral_constant<bool, __has_nothrow_copy(_Tp)>
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
#endif
{
};

#endif // __WI_LIBCPP_HAS_NO_VARIADICS
#endif // __has_feature(is_nothrow_constructible)

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES) && !defined(__WI_LIBCPP_HAS_NO_VARIADICS)
template <class _Tp, class... _Args>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_nothrow_constructible_v = is_nothrow_constructible<_Tp, _Args...>::value;
#endif

// is_nothrow_default_constructible

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_default_constructible : public is_nothrow_constructible<_Tp>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_nothrow_default_constructible_v = is_nothrow_default_constructible<_Tp>::value;
#endif

// is_nothrow_copy_constructible

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_copy_constructible
    : public is_nothrow_constructible<_Tp, typename add_lvalue_reference<typename add_const<_Tp>::type>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_nothrow_copy_constructible_v = is_nothrow_copy_constructible<_Tp>::value;
#endif

// is_nothrow_move_constructible

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_move_constructible
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
    : public is_nothrow_constructible<_Tp, typename add_rvalue_reference<_Tp>::type>
#else
    : public is_nothrow_copy_constructible<_Tp>
#endif
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_nothrow_move_constructible_v = is_nothrow_move_constructible<_Tp>::value;
#endif

// is_nothrow_assignable

#if !defined(__WI_LIBCPP_HAS_NO_NOEXCEPT) || (__WI_GNUC_VER >= 407 && __cplusplus >= 201103L)

template <bool, class _Tp, class _Arg>
struct __libcpp_is_nothrow_assignable;

template <class _Tp, class _Arg>
struct __libcpp_is_nothrow_assignable<false, _Tp, _Arg> : public false_type
{
};

template <class _Tp, class _Arg>
struct __libcpp_is_nothrow_assignable<true, _Tp, _Arg> : public integral_constant<bool, noexcept(declval<_Tp>() = declval<_Arg>())>
{
};

template <class _Tp, class _Arg>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_assignable
    : public __libcpp_is_nothrow_assignable<is_assignable<_Tp, _Arg>::value, _Tp, _Arg>
{
};

#else // !defined(__WI_LIBCPP_HAS_NO_NOEXCEPT)

template <class _Tp, class _Arg>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_assignable : public false_type
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_assignable<_Tp&, _Tp>
#if __WI_HAS_FEATURE_HAS_NOTHROW_ASSIGN
    : integral_constant<bool, __has_nothrow_assign(_Tp)>
{
};
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
{
};
#endif

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_assignable<_Tp&, _Tp&>
#if __WI_HAS_FEATURE_HAS_NOTHROW_ASSIGN
    : integral_constant<bool, __has_nothrow_assign(_Tp)>
{
};
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
{
};
#endif

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_assignable<_Tp&, const _Tp&>
#if __WI_HAS_FEATURE_HAS_NOTHROW_ASSIGN
    : integral_constant<bool, __has_nothrow_assign(_Tp)>
{
};
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
{
};
#endif

#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

template <class _Tp>
struct is_nothrow_assignable<_Tp&, _Tp&&>
#if __WI_HAS_FEATURE_HAS_NOTHROW_ASSIGN
    : integral_constant<bool, __has_nothrow_assign(_Tp)>
{
};
#else
    : integral_constant<bool, is_scalar<_Tp>::value>
{
};
#endif

#endif // __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

#endif // !defined(__WI_LIBCPP_HAS_NO_NOEXCEPT)

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp, class _Arg>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_nothrow_assignable_v = is_nothrow_assignable<_Tp, _Arg>::value;
#endif

// is_nothrow_copy_assignable

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_copy_assignable
    : public is_nothrow_assignable<typename add_lvalue_reference<_Tp>::type, typename add_lvalue_reference<typename add_const<_Tp>::type>::type>
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_nothrow_copy_assignable_v = is_nothrow_copy_assignable<_Tp>::value;
#endif

// is_nothrow_move_assignable

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_move_assignable : public is_nothrow_assignable<
                                                                 typename add_lvalue_reference<_Tp>::type,
#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES
                                                                 typename add_rvalue_reference<_Tp>::type>
#else
                                                                 typename add_lvalue_reference<_Tp>::type>
#endif
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_nothrow_move_assignable_v = is_nothrow_move_assignable<_Tp>::value;
#endif

// is_nothrow_destructible

#if !defined(__WI_LIBCPP_HAS_NO_NOEXCEPT) || (__WI_GNUC_VER >= 407 && __cplusplus >= 201103L)

template <bool, class _Tp>
struct __libcpp_is_nothrow_destructible;

template <class _Tp>
struct __libcpp_is_nothrow_destructible<false, _Tp> : public false_type
{
};

template <class _Tp>
struct __libcpp_is_nothrow_destructible<true, _Tp> : public integral_constant<bool, noexcept(declval<_Tp>().~_Tp())>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_destructible : public __libcpp_is_nothrow_destructible<is_destructible<_Tp>::value, _Tp>
{
};

template <class _Tp, size_t _Ns>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_destructible<_Tp[_Ns]> : public is_nothrow_destructible<_Tp>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_destructible<_Tp&> : public true_type
{
};

#ifndef __WI_LIBCPP_HAS_NO_RVALUE_REFERENCES

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_destructible<_Tp&&> : public true_type
{
};

#endif

#else

template <class _Tp>
struct __libcpp_nothrow_destructor : public integral_constant<bool, is_scalar<_Tp>::value || is_reference<_Tp>::value>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_destructible : public __libcpp_nothrow_destructor<typename remove_all_extents<_Tp>::type>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_destructible<_Tp[]> : public false_type
{
};

#endif

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_nothrow_destructible_v = is_nothrow_destructible<_Tp>::value;
#endif

// is_pod

#if __WI_HAS_FEATURE_IS_POD || (__WI_GNUC_VER >= 403)

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_pod : public integral_constant<bool, __is_pod(_Tp)>
{
};

#else

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_pod
    : public integral_constant<
          bool,
          is_trivially_default_constructible<_Tp>::value && is_trivially_copy_constructible<_Tp>::value &&
              is_trivially_copy_assignable<_Tp>::value && is_trivially_destructible<_Tp>::value>
{
};

#endif

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_pod_v = is_pod<_Tp>::value;
#endif

// is_literal_type;

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_literal_type
#ifdef __WI_LIBCPP_IS_LITERAL
    : public integral_constant<bool, __WI_LIBCPP_IS_LITERAL(_Tp)>
#else
    : integral_constant<bool, is_scalar<typename remove_all_extents<_Tp>::type>::value || is_reference<typename remove_all_extents<_Tp>::type>::value>
#endif
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_literal_type_v = is_literal_type<_Tp>::value;
#endif

// is_standard_layout;

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_standard_layout
#if __WI_HAS_FEATURE_IS_STANDARD_LAYOUT || (__WI_GNUC_VER >= 407)
    : public integral_constant<bool, __is_standard_layout(_Tp)>
#else
    : integral_constant<bool, is_scalar<typename remove_all_extents<_Tp>::type>::value>
#endif
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_standard_layout_v = is_standard_layout<_Tp>::value;
#endif

// is_trivially_copyable;

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivially_copyable
#if __WI_HAS_FEATURE_IS_TRIVIALLY_COPYABLE
    : public integral_constant<bool, __is_trivially_copyable(_Tp)>
#elif __WI_GNUC_VER >= 501
    : public integral_constant<bool, !is_volatile<_Tp>::value && __is_trivially_copyable(_Tp)>
#else
    : integral_constant<bool, is_scalar<typename remove_all_extents<_Tp>::type>::value>
#endif
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_trivially_copyable_v = is_trivially_copyable<_Tp>::value;
#endif

// is_trivial;

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_trivial
#if __WI_HAS_FEATURE_IS_TRIVIAL || __WI_GNUC_VER >= 407
    : public integral_constant<bool, __is_trivial(_Tp)>
#else
    : integral_constant<bool, is_trivially_copyable<_Tp>::value && is_trivially_default_constructible<_Tp>::value>
#endif
{
};

#if __WI_LIBCPP_STD_VER > 11 && !defined(__WI_LIBCPP_HAS_NO_VARIABLE_TEMPLATES)
template <class _Tp>
__WI_LIBCPP_INLINE_VAR __WI_LIBCPP_CONSTEXPR bool is_trivial_v = is_trivial<_Tp>::value;
#endif

template <class _Tp>
struct __is_reference_wrapper_impl : public false_type
{
};
template <class _Tp>
struct __is_reference_wrapper_impl<reference_wrapper<_Tp>> : public true_type
{
};
template <class _Tp>
struct __is_reference_wrapper : public __is_reference_wrapper_impl<typename remove_cv<_Tp>::type>
{
};

#ifndef __WI_LIBCPP_CXX03_LANG

template <class _Fp, class _A0, class _DecayFp = typename decay<_Fp>::type, class _DecayA0 = typename decay<_A0>::type, class _ClassT = typename __member_pointer_class_type<_DecayFp>::type>
using __enable_if_bullet1 =
    typename enable_if<is_member_function_pointer<_DecayFp>::value && is_base_of<_ClassT, _DecayA0>::value>::type;

template <class _Fp, class _A0, class _DecayFp = typename decay<_Fp>::type, class _DecayA0 = typename decay<_A0>::type>
using __enable_if_bullet2 =
    typename enable_if<is_member_function_pointer<_DecayFp>::value && __is_reference_wrapper<_DecayA0>::value>::type;

template <class _Fp, class _A0, class _DecayFp = typename decay<_Fp>::type, class _DecayA0 = typename decay<_A0>::type, class _ClassT = typename __member_pointer_class_type<_DecayFp>::type>
using __enable_if_bullet3 =
    typename enable_if<is_member_function_pointer<_DecayFp>::value && !is_base_of<_ClassT, _DecayA0>::value && !__is_reference_wrapper<_DecayA0>::value>::type;

template <class _Fp, class _A0, class _DecayFp = typename decay<_Fp>::type, class _DecayA0 = typename decay<_A0>::type, class _ClassT = typename __member_pointer_class_type<_DecayFp>::type>
using __enable_if_bullet4 =
    typename enable_if<is_member_object_pointer<_DecayFp>::value && is_base_of<_ClassT, _DecayA0>::value>::type;

template <class _Fp, class _A0, class _DecayFp = typename decay<_Fp>::type, class _DecayA0 = typename decay<_A0>::type>
using __enable_if_bullet5 =
    typename enable_if<is_member_object_pointer<_DecayFp>::value && __is_reference_wrapper<_DecayA0>::value>::type;

template <class _Fp, class _A0, class _DecayFp = typename decay<_Fp>::type, class _DecayA0 = typename decay<_A0>::type, class _ClassT = typename __member_pointer_class_type<_DecayFp>::type>
using __enable_if_bullet6 =
    typename enable_if<is_member_object_pointer<_DecayFp>::value && !is_base_of<_ClassT, _DecayA0>::value && !__is_reference_wrapper<_DecayA0>::value>::type;

// __invoke forward declarations

// fall back - none of the bullets

#define __WI_LIBCPP_INVOKE_RETURN(...) \
    __WI_NOEXCEPT_(__WI_NOEXCEPT_(__VA_ARGS__))->decltype(__VA_ARGS__) \
    { \
        return __VA_ARGS__; \
    }

template <class... _Args>
auto __invoke(__any, _Args&&... __args) -> __nat;

template <class... _Args>
auto __invoke_constexpr(__any, _Args&&... __args) -> __nat;

// bullets 1, 2 and 3

template <class _Fp, class _A0, class... _Args, class = __enable_if_bullet1<_Fp, _A0>>
inline __WI_LIBCPP_INLINE_VISIBILITY auto __invoke(_Fp&& __f, _A0&& __a0, _Args&&... __args)
    __WI_LIBCPP_INVOKE_RETURN((wistd::forward<_A0>(__a0).*__f)(wistd::forward<_Args>(__args)...))

        template <class _Fp, class _A0, class... _Args, class = __enable_if_bullet1<_Fp, _A0>>
        inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR auto __invoke_constexpr(_Fp&& __f, _A0&& __a0, _Args&&... __args)
            __WI_LIBCPP_INVOKE_RETURN((wistd::forward<_A0>(__a0).*__f)(wistd::forward<_Args>(__args)...))

                template <class _Fp, class _A0, class... _Args, class = __enable_if_bullet2<_Fp, _A0>>
                inline __WI_LIBCPP_INLINE_VISIBILITY auto __invoke(_Fp&& __f, _A0&& __a0, _Args&&... __args)
                    __WI_LIBCPP_INVOKE_RETURN((__a0.get().*__f)(wistd::forward<_Args>(__args)...))

                        template <class _Fp, class _A0, class... _Args, class = __enable_if_bullet2<_Fp, _A0>>
                        inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR
    auto __invoke_constexpr(_Fp&& __f, _A0&& __a0, _Args&&... __args)
        __WI_LIBCPP_INVOKE_RETURN((__a0.get().*__f)(wistd::forward<_Args>(__args)...))

            template <class _Fp, class _A0, class... _Args, class = __enable_if_bullet3<_Fp, _A0>>
            inline __WI_LIBCPP_INLINE_VISIBILITY auto __invoke(_Fp&& __f, _A0&& __a0, _Args&&... __args)
                __WI_LIBCPP_INVOKE_RETURN(((*wistd::forward<_A0>(__a0)).*__f)(wistd::forward<_Args>(__args)...))

                    template <class _Fp, class _A0, class... _Args, class = __enable_if_bullet3<_Fp, _A0>>
                    inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR
    auto __invoke_constexpr(_Fp&& __f, _A0&& __a0, _Args&&... __args)
        __WI_LIBCPP_INVOKE_RETURN(((*wistd::forward<_A0>(__a0)).*__f)(wistd::forward<_Args>(__args)...))

    // bullets 4, 5 and 6

    template <class _Fp, class _A0, class = __enable_if_bullet4<_Fp, _A0>>
    inline __WI_LIBCPP_INLINE_VISIBILITY auto __invoke(_Fp&& __f, _A0&& __a0) __WI_LIBCPP_INVOKE_RETURN(wistd::forward<_A0>(__a0).*__f)

        template <class _Fp, class _A0, class = __enable_if_bullet4<_Fp, _A0>>
        inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR
    auto __invoke_constexpr(_Fp&& __f, _A0&& __a0) __WI_LIBCPP_INVOKE_RETURN(wistd::forward<_A0>(__a0).*__f)

        template <class _Fp, class _A0, class = __enable_if_bullet5<_Fp, _A0>>
        inline __WI_LIBCPP_INLINE_VISIBILITY auto __invoke(_Fp&& __f, _A0&& __a0) __WI_LIBCPP_INVOKE_RETURN(__a0.get().*__f)

            template <class _Fp, class _A0, class = __enable_if_bullet5<_Fp, _A0>>
            inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR
    auto __invoke_constexpr(_Fp&& __f, _A0&& __a0) __WI_LIBCPP_INVOKE_RETURN(__a0.get().*__f)

        template <class _Fp, class _A0, class = __enable_if_bullet6<_Fp, _A0>>
        inline __WI_LIBCPP_INLINE_VISIBILITY auto __invoke(_Fp&& __f, _A0&& __a0)
            __WI_LIBCPP_INVOKE_RETURN((*wistd::forward<_A0>(__a0)).*__f)

                template <class _Fp, class _A0, class = __enable_if_bullet6<_Fp, _A0>>
                inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR auto __invoke_constexpr(_Fp&& __f, _A0&& __a0)
                    __WI_LIBCPP_INVOKE_RETURN((*wistd::forward<_A0>(__a0)).*__f)

    // bullet 7

    template <class _Fp, class... _Args>
    inline __WI_LIBCPP_INLINE_VISIBILITY auto __invoke(_Fp&& __f, _Args&&... __args)
        __WI_LIBCPP_INVOKE_RETURN(wistd::forward<_Fp>(__f)(wistd::forward<_Args>(__args)...))

            template <class _Fp, class... _Args>
            inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR auto __invoke_constexpr(_Fp&& __f, _Args&&... __args)
                __WI_LIBCPP_INVOKE_RETURN(wistd::forward<_Fp>(__f)(wistd::forward<_Args>(__args)...))

#undef __WI_LIBCPP_INVOKE_RETURN

    // __invokable

    template <class _Ret, class _Fp, class... _Args>
    struct __invokable_r
{
    // FIXME: Check that _Ret, _Fp, and _Args... are all complete types, cv void,
    // or incomplete array types as required by the standard.
    using _Result = decltype(__invoke(declval<_Fp>(), declval<_Args>()...));

    using type =
        typename conditional<!is_same<_Result, __nat>::value, typename conditional<is_void<_Ret>::value, true_type, is_convertible<_Result, _Ret>>::type, false_type>::type;
    static const bool value = type::value;
};

template <class _Fp, class... _Args>
using __invokable = __invokable_r<void, _Fp, _Args...>;

template <bool _IsInvokable, bool _IsCVVoid, class _Ret, class _Fp, class... _Args>
struct __nothrow_invokable_r_imp
{
    static const bool value = false;
};

template <class _Ret, class _Fp, class... _Args>
struct __nothrow_invokable_r_imp<true, false, _Ret, _Fp, _Args...>
{
    typedef __nothrow_invokable_r_imp _ThisT;

    template <class _Tp>
    static void __test_noexcept(_Tp) noexcept;

    static const bool value = noexcept(_ThisT::__test_noexcept<_Ret>(__invoke(declval<_Fp>(), declval<_Args>()...)));
};

template <class _Ret, class _Fp, class... _Args>
struct __nothrow_invokable_r_imp<true, true, _Ret, _Fp, _Args...>
{
    static const bool value = noexcept(__invoke(declval<_Fp>(), declval<_Args>()...));
};

template <class _Ret, class _Fp, class... _Args>
using __nothrow_invokable_r =
    __nothrow_invokable_r_imp<__invokable_r<_Ret, _Fp, _Args...>::value, is_void<_Ret>::value, _Ret, _Fp, _Args...>;

template <class _Fp, class... _Args>
using __nothrow_invokable = __nothrow_invokable_r_imp<__invokable<_Fp, _Args...>::value, true, void, _Fp, _Args...>;

template <class _Fp, class... _Args>
struct __invoke_of : public enable_if<__invokable<_Fp, _Args...>::value, typename __invokable_r<void, _Fp, _Args...>::_Result>
{
};

// result_of

template <class _Fp, class... _Args>
class __WI_LIBCPP_TEMPLATE_VIS result_of<_Fp(_Args...)> : public __invoke_of<_Fp, _Args...>
{
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using result_of_t = typename result_of<_Tp>::type;
#endif

#if __WI_LIBCPP_STD_VER > 14

// invoke_result

template <class _Fn, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS invoke_result : __invoke_of<_Fn, _Args...>
{
};

template <class _Fn, class... _Args>
using invoke_result_t = typename invoke_result<_Fn, _Args...>::type;

// is_invocable

template <class _Fn, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS is_invocable : integral_constant<bool, __invokable<_Fn, _Args...>::value>
{
};

template <class _Ret, class _Fn, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS is_invocable_r : integral_constant<bool, __invokable_r<_Ret, _Fn, _Args...>::value>
{
};

template <class _Fn, class... _Args>
__WI_LIBCPP_INLINE_VAR constexpr bool is_invocable_v = is_invocable<_Fn, _Args...>::value;

template <class _Ret, class _Fn, class... _Args>
__WI_LIBCPP_INLINE_VAR constexpr bool is_invocable_r_v = is_invocable_r<_Ret, _Fn, _Args...>::value;

// is_nothrow_invocable

template <class _Fn, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_invocable : integral_constant<bool, __nothrow_invokable<_Fn, _Args...>::value>
{
};

template <class _Ret, class _Fn, class... _Args>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_invocable_r : integral_constant<bool, __nothrow_invokable_r<_Ret, _Fn, _Args...>::value>
{
};

template <class _Fn, class... _Args>
__WI_LIBCPP_INLINE_VAR constexpr bool is_nothrow_invocable_v = is_nothrow_invocable<_Fn, _Args...>::value;

template <class _Ret, class _Fn, class... _Args>
__WI_LIBCPP_INLINE_VAR constexpr bool is_nothrow_invocable_r_v = is_nothrow_invocable_r<_Ret, _Fn, _Args...>::value;

#endif // __WI_LIBCPP_STD_VER > 14

#endif // !defined(__WI_LIBCPP_CXX03_LANG)

template <class _Tp>
struct __is_swappable;
template <class _Tp>
struct __is_nothrow_swappable;

template <class _Tp>
inline __WI_LIBCPP_INLINE_VISIBILITY
#ifndef __WI_LIBCPP_CXX03_LANG
    typename enable_if<is_move_constructible<_Tp>::value && is_move_assignable<_Tp>::value>::type
#else
    void
#endif
    swap_wil(_Tp& __x, _Tp& __y) __WI_NOEXCEPT_(is_nothrow_move_constructible<_Tp>::value&& is_nothrow_move_assignable<_Tp>::value)
{
    _Tp __t(wistd::move(__x));
    __x = wistd::move(__y);
    __y = wistd::move(__t);
}

template <class _ForwardIterator1, class _ForwardIterator2>
inline __WI_LIBCPP_INLINE_VISIBILITY _ForwardIterator2 swap_ranges_wil(_ForwardIterator1 __first1, _ForwardIterator1 __last1, _ForwardIterator2 __first2)
{
    for (; __first1 != __last1; ++__first1, (void)++__first2)
        swap_wil(*__first1, *__first2);
    return __first2;
}

template <class _Tp, size_t _Np>
inline __WI_LIBCPP_INLINE_VISIBILITY typename enable_if<__is_swappable<_Tp>::value>::type swap_wil(_Tp (&__a)[_Np], _Tp (&__b)[_Np])
    __WI_NOEXCEPT_(__is_nothrow_swappable<_Tp>::value)
{
    wistd::swap_ranges_wil(__a, __a + _Np, __b);
}

template <class _ForwardIterator1, class _ForwardIterator2>
inline __WI_LIBCPP_INLINE_VISIBILITY void iter_swap_wil(_ForwardIterator1 __a, _ForwardIterator2 __b)
    //                                  __WI_NOEXCEPT_(__WI_NOEXCEPT_(swap_wil(*__a, *__b)))
    __WI_NOEXCEPT_(__WI_NOEXCEPT_(swap_wil(*declval<_ForwardIterator1>(), *declval<_ForwardIterator2>())))
{
    swap_wil(*__a, *__b);
}

// __swappable

namespace __detail
{
    // ALL generic swap overloads MUST already have a declaration available at this point.

    template <class _Tp, class _Up = _Tp, bool _NotVoid = !is_void<_Tp>::value && !is_void<_Up>::value>
    struct __swappable_with
    {
        template <class _LHS, class _RHS>
        static decltype(swap_wil(declval<_LHS>(), declval<_RHS>())) __test_swap(int);
        template <class, class>
        static __nat __test_swap(long);

        // Extra parens are needed for the C++03 definition of decltype.
        typedef decltype((__test_swap<_Tp, _Up>(0))) __swap1;
        typedef decltype((__test_swap<_Up, _Tp>(0))) __swap2;

        static const bool value = !is_same<__swap1, __nat>::value && !is_same<__swap2, __nat>::value;
    };

    template <class _Tp, class _Up>
    struct __swappable_with<_Tp, _Up, false> : false_type
    {
    };

    template <class _Tp, class _Up = _Tp, bool _Swappable = __swappable_with<_Tp, _Up>::value>
    struct __nothrow_swappable_with
    {
        static const bool value =
#ifndef __WI_LIBCPP_HAS_NO_NOEXCEPT
            noexcept(swap_wil(declval<_Tp>(), declval<_Up>())) && noexcept(swap_wil(declval<_Up>(), declval<_Tp>()));
#else
            false;
#endif
    };

    template <class _Tp, class _Up>
    struct __nothrow_swappable_with<_Tp, _Up, false> : false_type
    {
    };

} // namespace __detail

template <class _Tp>
struct __is_swappable : public integral_constant<bool, __detail::__swappable_with<_Tp&>::value>
{
};

template <class _Tp>
struct __is_nothrow_swappable : public integral_constant<bool, __detail::__nothrow_swappable_with<_Tp&>::value>
{
};

#if __WI_LIBCPP_STD_VER > 14

template <class _Tp, class _Up>
struct __WI_LIBCPP_TEMPLATE_VIS is_swappable_with : public integral_constant<bool, __detail::__swappable_with<_Tp, _Up>::value>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_swappable
    : public conditional<__is_referenceable<_Tp>::value, is_swappable_with<typename add_lvalue_reference<_Tp>::type, typename add_lvalue_reference<_Tp>::type>, false_type>::type
{
};

template <class _Tp, class _Up>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_swappable_with
    : public integral_constant<bool, __detail::__nothrow_swappable_with<_Tp, _Up>::value>
{
};

template <class _Tp>
struct __WI_LIBCPP_TEMPLATE_VIS is_nothrow_swappable
    : public conditional<__is_referenceable<_Tp>::value, is_nothrow_swappable_with<typename add_lvalue_reference<_Tp>::type, typename add_lvalue_reference<_Tp>::type>, false_type>::type
{
};

template <class _Tp, class _Up>
__WI_LIBCPP_INLINE_VAR constexpr bool is_swappable_with_v = is_swappable_with<_Tp, _Up>::value;

template <class _Tp>
__WI_LIBCPP_INLINE_VAR constexpr bool is_swappable_v = is_swappable<_Tp>::value;

template <class _Tp, class _Up>
__WI_LIBCPP_INLINE_VAR constexpr bool is_nothrow_swappable_with_v = is_nothrow_swappable_with<_Tp, _Up>::value;

template <class _Tp>
__WI_LIBCPP_INLINE_VAR constexpr bool is_nothrow_swappable_v = is_nothrow_swappable<_Tp>::value;

#endif // __WI_LIBCPP_STD_VER > 14

#ifdef __WI_LIBCPP_UNDERLYING_TYPE

template <class _Tp>
struct underlying_type
{
    typedef __WI_LIBCPP_UNDERLYING_TYPE(_Tp) type;
};

#if __WI_LIBCPP_STD_VER > 11
template <class _Tp>
using underlying_type_t = typename underlying_type<_Tp>::type;
#endif

#else // __WI_LIBCPP_UNDERLYING_TYPE

template <class _Tp, bool _Support = false>
struct underlying_type
{
    static_assert(
        _Support,
        "The underlying_type trait requires compiler "
        "support. Either no such support exists or "
        "libc++ does not know how to use it.");
};

#endif // __WI_LIBCPP_UNDERLYING_TYPE

template <class _Tp, bool = is_enum<_Tp>::value>
struct __sfinae_underlying_type
{
    typedef typename underlying_type<_Tp>::type type;
    typedef decltype(((type)1) + 0) __promoted_type;
};

template <class _Tp>
struct __sfinae_underlying_type<_Tp, false>
{
};

inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR int __convert_to_integral(int __val)
{
    return __val;
}

inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR unsigned __convert_to_integral(unsigned __val)
{
    return __val;
}

inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR long __convert_to_integral(long __val)
{
    return __val;
}

inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR unsigned long __convert_to_integral(unsigned long __val)
{
    return __val;
}

inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR long long __convert_to_integral(long long __val)
{
    return __val;
}

inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR unsigned long long __convert_to_integral(unsigned long long __val)
{
    return __val;
}

template <typename _Fp>
inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR typename enable_if<is_floating_point<_Fp>::value, long long>::type __convert_to_integral(_Fp __val)
{
    return __val;
}

#ifndef __WI_LIBCPP_HAS_NO_INT128
inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR __int128_t __convert_to_integral(__int128_t __val)
{
    return __val;
}

inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR __uint128_t __convert_to_integral(__uint128_t __val)
{
    return __val;
}
#endif

template <class _Tp>
inline __WI_LIBCPP_INLINE_VISIBILITY __WI_LIBCPP_CONSTEXPR typename __sfinae_underlying_type<_Tp>::__promoted_type __convert_to_integral(_Tp __val)
{
    return __val;
}

#ifndef __WI_LIBCPP_CXX03_LANG

template <class _Tp>
struct __has_operator_addressof_member_imp
{
    template <class _Up>
    static auto __test(int) -> typename __select_2nd<decltype(declval<_Up>().operator&()), true_type>::type;
    template <class>
    static auto __test(long) -> false_type;

    static const bool value = decltype(__test<_Tp>(0))::value;
};

template <class _Tp>
struct __has_operator_addressof_free_imp
{
    template <class _Up>
    static auto __test(int) -> typename __select_2nd<decltype(operator&(declval<_Up>())), true_type>::type;
    template <class>
    static auto __test(long) -> false_type;

    static const bool value = decltype(__test<_Tp>(0))::value;
};

template <class _Tp>
struct __has_operator_addressof
    : public integral_constant<bool, __has_operator_addressof_member_imp<_Tp>::value || __has_operator_addressof_free_imp<_Tp>::value>
{
};

#endif // __WI_LIBCPP_CXX03_LANG

#ifndef __WI_LIBCPP_CXX03_LANG

template <class...>
using void_t = void;

#ifndef __WI_LIBCPP_HAS_NO_VARIADICS
template <class... _Args>
struct conjunction : __and_<_Args...>
{
};
template <class... _Args>
__WI_LIBCPP_INLINE_VAR constexpr bool conjunction_v = conjunction<_Args...>::value;

template <class... _Args>
struct disjunction : __or_<_Args...>
{
};
template <class... _Args>
__WI_LIBCPP_INLINE_VAR constexpr bool disjunction_v = disjunction<_Args...>::value;

template <class _Tp>
struct negation : __not_<_Tp>
{
};
template <class _Tp>
__WI_LIBCPP_INLINE_VAR constexpr bool negation_v = negation<_Tp>::value;
#endif // __WI_LIBCPP_HAS_NO_VARIADICS
#endif // __WI_LIBCPP_CXX03_LANG

// These traits are used in __tree and __hash_table
#ifndef __WI_LIBCPP_CXX03_LANG
struct __extract_key_fail_tag
{
};
struct __extract_key_self_tag
{
};
struct __extract_key_first_tag
{
};

template <class _ValTy, class _Key, class _RawValTy = typename __unconstref<_ValTy>::type>
struct __can_extract_key : conditional<is_same<_RawValTy, _Key>::value, __extract_key_self_tag, __extract_key_fail_tag>::type
{
};

template <class _Pair, class _Key, class _First, class _Second>
struct __can_extract_key<_Pair, _Key, pair<_First, _Second>>
    : conditional<is_same<typename remove_const<_First>::type, _Key>::value, __extract_key_first_tag, __extract_key_fail_tag>::type
{
};

// __can_extract_map_key uses true_type/false_type instead of the tags.
// It returns true if _Key != _ContainerValueTy (the container is a map not a set)
// and _ValTy == _Key.
template <class _ValTy, class _Key, class _ContainerValueTy, class _RawValTy = typename __unconstref<_ValTy>::type>
struct __can_extract_map_key : integral_constant<bool, is_same<_RawValTy, _Key>::value>
{
};

// This specialization returns __extract_key_fail_tag for non-map containers
// because _Key == _ContainerValueTy
template <class _ValTy, class _Key, class _RawValTy>
struct __can_extract_map_key<_ValTy, _Key, _Key, _RawValTy> : false_type
{
};

#endif

#if __WI_LIBCPP_STD_VER > 17
enum class endian
{
    little = 0xDEAD,
    big = 0xFACE,
#if defined(__WI_LIBCPP_LITTLE_ENDIAN)
    native = little
#elif defined(__WI_LIBCPP_BIG_ENDIAN)
    native = big
#else
    native = 0xCAFE
#endif
};
#endif
} // namespace wistd
/// @endcond

#endif // _WISTD_TYPE_TRAITS_H_
