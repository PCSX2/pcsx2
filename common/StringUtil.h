/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "Pcsx2Types.h"
#include <algorithm>
#include <charconv>
#include <cstdarg>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fast_float/fast_float.h"

// Older versions of libstdc++ are missing support for from_chars() with floats, and was only recently
// merged in libc++. So, just fall back to stringstream (yuck!) on everywhere except MSVC.
#if !defined(_MSC_VER)
#include <locale>
#include <sstream>
#ifdef __APPLE__
#include <Availability.h>
#endif
#endif

namespace StringUtil
{
	/// Constructs a std::string from a format string.
#ifdef __GNUC__
	std::string StdStringFromFormat(const char* format, ...) __attribute__((format(printf, 1, 2)));
#else
	std::string StdStringFromFormat(const char* format, ...);
#endif
	std::string StdStringFromFormatV(const char* format, std::va_list ap);

	/// Checks if a wildcard matches a search string.
	bool WildcardMatch(const char* subject, const char* mask, bool case_sensitive = true);

	/// Safe version of strlcpy.
	std::size_t Strlcpy(char* dst, const char* src, std::size_t size);

	/// Strlcpy from string_view.
	std::size_t Strlcpy(char* dst, const std::string_view& src, std::size_t size);

	/// Platform-independent strcasecmp
	static inline int Strcasecmp(const char* s1, const char* s2)
	{
#ifdef _MSC_VER
		return _stricmp(s1, s2);
#else
		return strcasecmp(s1, s2);
#endif
	}

	/// Platform-independent strcasecmp
	static inline int Strncasecmp(const char* s1, const char* s2, std::size_t n)
	{
#ifdef _MSC_VER
		return _strnicmp(s1, s2, n);
#else
		return strncasecmp(s1, s2, n);
#endif
	}

	/// Wrapper around std::from_chars
	template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline std::optional<T> FromChars(const std::string_view& str, int base = 10)
	{
		T value;

		const std::from_chars_result result = std::from_chars(str.data(), str.data() + str.length(), value, base);
		if (result.ec != std::errc())
			return std::nullopt;

		return value;
	}
	template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline std::optional<T> FromChars(const std::string_view& str, int base, std::string_view* endptr)
	{
		T value;

		const char* ptr = str.data();
		const char* end = ptr + str.length();
		const std::from_chars_result result = std::from_chars(ptr, end, value, base);
		if (result.ec != std::errc())
			return std::nullopt;

		if (endptr)
			*endptr = (result.ptr < end) ? std::string_view(result.ptr, end - result.ptr) : std::string_view();

		return value;
	}

	template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline std::optional<T> FromChars(const std::string_view& str)
	{
		T value;

		const fast_float::from_chars_result result = fast_float::from_chars(str.data(), str.data() + str.length(), value);
		if (result.ec != std::errc())
			return std::nullopt;

		return value;
	}
	template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline std::optional<T> FromChars(const std::string_view& str, std::string_view* endptr)
	{
		T value;

		const char* ptr = str.data();
		const char* end = ptr + str.length();
		const fast_float::from_chars_result result = fast_float::from_chars(ptr, end, value);
		if (result.ec != std::errc())
			return std::nullopt;

		if (endptr)
			*endptr = (result.ptr < end) ? std::string_view(result.ptr, end - result.ptr) : std::string_view();

		return value;
	}

	/// Wrapper around std::to_chars
	template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline std::string ToChars(T value, int base = 10)
	{
		// to_chars() requires macOS 10.15+.
#if !defined(__APPLE__) || MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
		constexpr size_t MAX_SIZE = 32;
		char buf[MAX_SIZE];
		std::string ret;

		const std::to_chars_result result = std::to_chars(buf, buf + MAX_SIZE, value, base);
		if (result.ec == std::errc())
			ret.append(buf, result.ptr - buf);

		return ret;
#else
		std::ostringstream ss;
		ss.imbue(std::locale::classic());
		ss << std::setbase(base) << value;
		return ss.str();
#endif
	}

	template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline std::string ToChars(T value)
	{
		// No to_chars() in older versions of libstdc++/libc++.
#ifdef _MSC_VER
		constexpr size_t MAX_SIZE = 64;
		char buf[MAX_SIZE];
		std::string ret;
		const std::to_chars_result result = std::to_chars(buf, buf + MAX_SIZE, value);
		if (result.ec == std::errc())
			ret.append(buf, result.ptr - buf);
		return ret;
#else
		std::ostringstream ss;
		ss.imbue(std::locale::classic());
		ss << value;
		return ss.str();
#endif
	}


	/// Explicit override for booleans
	template <>
	inline std::optional<bool> FromChars(const std::string_view& str, int base)
	{
		if (Strncasecmp("true", str.data(), str.length()) == 0 || Strncasecmp("yes", str.data(), str.length()) == 0 ||
			Strncasecmp("on", str.data(), str.length()) == 0 || Strncasecmp("1", str.data(), str.length()) == 0 ||
			Strncasecmp("enabled", str.data(), str.length()) == 0 || Strncasecmp("1", str.data(), str.length()) == 0)
		{
			return true;
		}

		if (Strncasecmp("false", str.data(), str.length()) == 0 || Strncasecmp("no", str.data(), str.length()) == 0 ||
			Strncasecmp("off", str.data(), str.length()) == 0 || Strncasecmp("0", str.data(), str.length()) == 0 ||
			Strncasecmp("disabled", str.data(), str.length()) == 0 || Strncasecmp("0", str.data(), str.length()) == 0)
		{
			return false;
		}

		return std::nullopt;
	}

	template <>
	inline std::string ToChars(bool value, int base)
	{
		return std::string(value ? "true" : "false");
	}

	/// Encode/decode hexadecimal byte buffers
	std::optional<std::vector<u8>> DecodeHex(const std::string_view& str);
	std::string EncodeHex(const u8* data, int length);

	/// starts_with from C++20
	static inline bool StartsWith(const std::string_view& str, const std::string_view& prefix)
	{
		return (str.compare(0, prefix.length(), prefix) == 0);
	}
	static inline bool EndsWith(const std::string_view& str, const std::string_view& suffix)
	{
		const std::size_t suffix_length = suffix.length();
		return (str.length() >= suffix_length && str.compare(str.length() - suffix_length, suffix_length, suffix) == 0);
	}

	/// StartsWith/EndsWith variants which aren't case sensitive.
	static inline bool StartsWithNoCase(const std::string_view& str, const std::string_view& prefix)
	{
		return (!str.empty() && Strncasecmp(str.data(), prefix.data(), prefix.length()) == 0);
	}
	static inline bool EndsWithNoCase(const std::string_view& str, const std::string_view& suffix)
	{
		const std::size_t suffix_length = suffix.length();
		return (str.length() >= suffix_length && Strncasecmp(str.data() + (str.length() - suffix_length), suffix.data(), suffix_length) == 0);
	}

	/// Strip whitespace from the start/end of the string.
	std::string_view StripWhitespace(const std::string_view& str);
	void StripWhitespace(std::string* str);

	/// Splits a string based on a single character delimiter.
	std::vector<std::string_view> SplitString(const std::string_view& str, char delimiter, bool skip_empty = true);

	/// Joins a string together using the specified delimiter.
	template <typename T>
	static inline std::string JoinString(const T& start, const T& end, char delimiter)
	{
		std::string ret;
		for (auto it = start; it != end; ++it)
		{
			if (it != start)
				ret += delimiter;
			ret.append(*it);
		}
		return ret;
	}
	template <typename T>
	static inline std::string JoinString(const T& start, const T& end, const std::string_view& delimiter)
	{
		std::string ret;
		for (auto it = start; it != end; ++it)
		{
			if (it != start)
				ret.append(delimiter);
			ret.append(*it);
		}
		return ret;
	}

	/// Replaces all instances of search in subject with replacement.
	std::string ReplaceAll(const std::string_view& subject, const std::string_view& search, const std::string_view& replacement);
	void ReplaceAll(std::string* subject, const std::string_view& search, const std::string_view& replacement);

	/// Parses an assignment string (Key = Value) into its two components.
	bool ParseAssignmentString(const std::string_view& str, std::string_view* key, std::string_view* value);

	/// Appends a UTF-16/UTF-32 codepoint to a UTF-8 string.
	void AppendUTF16CharacterToUTF8(std::string& s, u16 ch);

	/// Appends a UTF-16/UTF-32 codepoint to a UTF-8 string.
	void EncodeAndAppendUTF8(std::string& s, char32_t ch);

	/// Decodes UTF-8 to a single codepoint, updating the position parameter.
	/// Returns the number of bytes the codepoint took in the original string.
	size_t DecodeUTF8(const void* bytes, size_t length, char32_t* ch);
	size_t DecodeUTF8(const std::string_view& str, size_t offset, char32_t* ch);
	size_t DecodeUTF8(const std::string& str, size_t offset, char32_t* ch);

	/// Strided memcpy/memcmp.
	static inline void StrideMemCpy(void* dst, std::size_t dst_stride, const void* src, std::size_t src_stride,
		std::size_t copy_size, std::size_t count)
	{
		if (src_stride == dst_stride && src_stride == copy_size)
		{
			std::memcpy(dst, src, src_stride * count);
			return;
		}

		const u8* src_ptr = static_cast<const u8*>(src);
		u8* dst_ptr = static_cast<u8*>(dst);
		for (std::size_t i = 0; i < count; i++)
		{
			std::memcpy(dst_ptr, src_ptr, copy_size);
			src_ptr += src_stride;
			dst_ptr += dst_stride;
		}
	}

	static inline int StrideMemCmp(const void* p1, std::size_t p1_stride, const void* p2, std::size_t p2_stride,
		std::size_t copy_size, std::size_t count)
	{
		if (p1_stride == p2_stride && p1_stride == copy_size)
			return std::memcmp(p1, p2, p1_stride * count);

		const u8* p1_ptr = static_cast<const u8*>(p1);
		const u8* p2_ptr = static_cast<const u8*>(p2);
		for (std::size_t i = 0; i < count; i++)
		{
			int result = std::memcmp(p1_ptr, p2_ptr, copy_size);
			if (result != 0)
				return result;
			p2_ptr += p2_stride;
			p1_ptr += p1_stride;
		}

		return 0;
	}

	std::string toLower(const std::string_view& str);
	std::string toUpper(const std::string_view& str);
	bool compareNoCase(const std::string_view& str1, const std::string_view& str2);
	std::vector<std::string> splitOnNewLine(const std::string& str);

#ifdef _WIN32
	/// Converts the specified UTF-8 string to a wide string.
	std::wstring UTF8StringToWideString(const std::string_view& str);
	bool UTF8StringToWideString(std::wstring& dest, const std::string_view& str);

	/// Converts the specified wide string to a UTF-8 string.
	std::string WideStringToUTF8String(const std::wstring_view& str);
	bool WideStringToUTF8String(std::string& dest, const std::wstring_view& str);
#endif

	/// Converts unsigned 128-bit data to string.
	std::string U128ToString(const u128& u);
	std::string& AppendU128ToString(const u128& u, std::string& s);

	template <typename ContainerType>
	static inline bool ContainsSubString(const ContainerType& haystack, const std::string_view& needle)
	{
		using ValueType = typename ContainerType::value_type;
		if (needle.empty())
			return std::empty(haystack);

		return std::search(std::begin(haystack), std::end(haystack), reinterpret_cast<const ValueType*>(needle.data()),
				   reinterpret_cast<const ValueType*>(needle.data() + needle.length())) != std::end(haystack);
	}
} // namespace StringUtil
