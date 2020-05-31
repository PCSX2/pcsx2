/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2017-2017  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>

// make_unique is quite handy but it requires to enable C++14 support (of
// course on C++14 compliant compiler)
//
// Instead just provide the std++ implementation when only C++11 is enabled
// File could be dropped when we switch to C++14

#if __cplusplus <= 201103L && !defined(_MSC_VER)

namespace std
{

template <typename _Tp>
struct _MakeUniq
{
    typedef std::unique_ptr<_Tp> __single_object;
};

template <typename _Tp>
struct _MakeUniq<_Tp[]>
{
    typedef std::unique_ptr<_Tp[]> __array;
};

template <typename _Tp, size_t _Bound>
struct _MakeUniq<_Tp[_Bound]>
{
    struct __invalid_type
    {
    };
};

/// std::make_unique for single objects
template <typename _Tp, typename... _Args>
inline typename _MakeUniq<_Tp>::__single_object
make_unique(_Args &&... __args)
{
    return std::unique_ptr<_Tp>(new _Tp(std::forward<_Args>(__args)...));
}

/// std::make_unique for arrays of unknown bound
template <typename _Tp>
inline typename _MakeUniq<_Tp>::__array
make_unique(size_t __num)
{
    return std::unique_ptr<_Tp>(new typename remove_extent<_Tp>::type[__num]());
}

/// Disable std::make_unique for arrays of known bound
template <typename _Tp, typename... _Args>
inline typename _MakeUniq<_Tp>::__invalid_type
make_unique(_Args &&...) = delete;
}

#endif
