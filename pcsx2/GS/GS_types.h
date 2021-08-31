/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include <cfloat>

// clang-format off

#ifdef _WIN32
inline std::string convert_utf16_to_utf8(const std::wstring& utf16_string)
{
	const int size = WideCharToMultiByte(CP_UTF8, 0, utf16_string.c_str(), utf16_string.size(), nullptr, 0, nullptr, nullptr);
	std::string converted_string(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, utf16_string.c_str(), utf16_string.size(), converted_string.data(), converted_string.size(), nullptr, nullptr);
	return converted_string;
}

inline std::wstring convert_utf8_to_utf16(const std::string& utf8_string)
{
	int size = MultiByteToWideChar(CP_UTF8, 0, utf8_string.c_str(), -1, nullptr, 0);
	std::vector<wchar_t> converted_string(size);
	MultiByteToWideChar(CP_UTF8, 0, utf8_string.c_str(), -1, converted_string.data(), converted_string.size());
	return {converted_string.data()};
}
#endif

// _wfopen has to be used on Windows for pathnames containing non-ASCII characters.
inline FILE* px_fopen(const std::string& filename, const std::string& mode)
{
#ifdef _WIN32
	return _wfopen(convert_utf8_to_utf16(filename).c_str(), convert_utf8_to_utf16(mode).c_str());
#else
	return fopen(filename.c_str(), mode.c_str());
#endif
}

#define _MM_TRANSPOSE4_SI128(row0, row1, row2, row3) \
	{ \
		__m128 tmp0 = _mm_shuffle_ps(_mm_castsi128_ps(row0), _mm_castsi128_ps(row1), 0x44); \
		__m128 tmp2 = _mm_shuffle_ps(_mm_castsi128_ps(row0), _mm_castsi128_ps(row1), 0xEE); \
		__m128 tmp1 = _mm_shuffle_ps(_mm_castsi128_ps(row2), _mm_castsi128_ps(row3), 0x44); \
		__m128 tmp3 = _mm_shuffle_ps(_mm_castsi128_ps(row2), _mm_castsi128_ps(row3), 0xEE); \
		(row0) = _mm_castps_si128(_mm_shuffle_ps(tmp0, tmp1, 0x88)); \
		(row1) = _mm_castps_si128(_mm_shuffle_ps(tmp0, tmp1, 0xDD)); \
		(row2) = _mm_castps_si128(_mm_shuffle_ps(tmp2, tmp3, 0x88)); \
		(row3) = _mm_castps_si128(_mm_shuffle_ps(tmp2, tmp3, 0xDD)); \
	}


extern void* vmalloc(size_t size, bool code);
extern void vmfree(void* ptr, size_t size);
