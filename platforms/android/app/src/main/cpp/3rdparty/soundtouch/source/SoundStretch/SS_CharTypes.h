////////////////////////////////////////////////////////////////////////////////
///
/// Char type for SoundStretch
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#ifndef SS_CHARTYPE_H
#define SS_CHARTYPE_H

#include <string>

namespace soundstretch
{
#if _WIN32
    // wide-char types for supporting non-latin file paths in Windows
    using CHARTYPE = wchar_t;
    using STRING = std::wstring;
    #define STRING_CONST(x) (L"" x)
#else
    // gnu platform can natively support UTF-8 paths using "char*" set
    using CHARTYPE = char;
    using STRING = std::string;
    #define STRING_CONST(x) (x)
#endif
}

#endif //SS_CHARTYPE_H
