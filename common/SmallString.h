/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "Pcsx2Defs.h"

#include "fmt/core.h"

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

//
// SmallString
// Lightweight string class which can be allocated on the stack, instead of with heap allocations.
//
class SmallStringBase
{
public:
	using value_type = char;

	SmallStringBase();
	SmallStringBase(const char* str);
	SmallStringBase(const char* str, u32 length);
	SmallStringBase(const SmallStringBase& copy);
	SmallStringBase(SmallStringBase&& move);
	SmallStringBase(const std::string& str);
	SmallStringBase(const std::string_view& sv);

	// Destructor. Child classes may not have any destructors, as this is not virtual.
	~SmallStringBase();

	// manual assignment
	void assign(const char* str);
	void assign(const char* str, u32 length);
	void assign(const std::string& copy);
	void assign(const std::string_view& copy);
	void assign(const SmallStringBase& copy);
	void assign(SmallStringBase&& move);

	// Ensures that we have space bytes free in the buffer.
	void make_room_for(u32 space);

	// clears the contents of the string
	void clear();

	// append a single character to this string
	void append(char c);

	// append a string to this string
	void append(const char* appendText);
	void append(const char* str, u32 length);
	void append(const std::string& str);
	void append(const std::string_view& str);
	void append(const SmallStringBase& str);

	// append formatted string to this string
	void append_format(const char* format, ...) /*printflike(2, 3)*/;
	void append_format_va(const char* format, va_list ap);

	template <typename... T>
	void append_fmt(fmt::format_string<T...> fmt, T&&... args);

	// append a single character to this string
	void prepend(char c);

	// append a string to this string
	void prepend(const char* str);
	void prepend(const char* str, u32 length);
	void prepend(const std::string& str);
	void prepend(const std::string_view& str);
	void prepend(const SmallStringBase& str);

	// append formatted string to this string
	void prepend_format(const char* format, ...) /*printflike(2, 3)*/;
	void prepend_format_va(const char* format, va_list ap);

	template <typename... T>
	void prepend_fmt(fmt::format_string<T...> fmt, T&&... args);

	// insert a string at the specified offset
	void insert(s32 offset, const char* str);
	void insert(s32 offset, const char* str, u32 length);
	void insert(s32 offset, const std::string& str);
	void insert(s32 offset, const std::string_view& str);
	void insert(s32 offset, const SmallStringBase& str);

	// set to formatted string
	void format(const char* format, ...) /*printflike(2, 3)*/;
	void format_va(const char* format, va_list ap);

	template <typename... T>
	void fmt(fmt::format_string<T...> fmt, T&&... args);

	// compare one string to another
	bool equals(const char* str) const;
	bool equals(const SmallStringBase& str) const;
	bool equals(const std::string_view& str) const;
	bool iequals(const char* str) const;
	bool iequals(const SmallStringBase& str) const;
	bool iequals(const std::string_view& str) const;

	// numerical compares
	int compare(const char* str) const;
	int compare(const SmallStringBase& str) const;
	int compare(const std::string_view& str) const;
	int icompare(const char* str) const;
	int icompare(const SmallStringBase& str) const;
	int icompare(const std::string_view& str) const;

	// starts with / ends with
	bool starts_with(const char* str, bool case_sensitive = true) const;
	bool starts_with(const std::string_view& str, bool case_sensitive = true) const;
	bool starts_with(const SmallStringBase& str, bool case_sensitive = true) const;
	bool ends_with(const char* str, bool case_sensitive = true) const;
	bool ends_with(const std::string_view& str, bool case_sensitive = true) const;
	bool ends_with(const SmallStringBase& str, bool case_sensitive = true) const;

	// searches for a character inside a string
	// rfind is the same except it starts at the end instead of the start
	// returns -1 if it is not found, otherwise the offset in the string
	s32 find(char c, u32 offset = 0) const;
	s32 rfind(char c, u32 offset = 0) const;

	// searches for a string inside a string
	// rfind is the same except it starts at the end instead of the start
	// returns -1 if it is not found, otherwise the offset in the string
	s32 find(const char* str, u32 offset = 0) const;

	// removes characters from string
	void erase(s32 offset, s32 count = std::numeric_limits<s32>::max());

	// alters the length of the string to be at least len bytes long
	void reserve(u32 new_reserve);

	// Cuts characters off the string to reduce it to len bytes long.
	void resize(u32 new_size, char fill = ' ', bool shrink_if_smaller = false);

	// updates the internal length counter when the string is externally modified
	void update_size();

	// shrink the string to the minimum size possible
	void shrink_to_fit();

	// gets the size of the string
	__fi u32 length() const { return m_length; }
	__fi bool empty() const { return (m_length == 0); }

	// gets the maximum number of bytes we can write to the string, currently
	__fi u32 buffer_size() const { return m_buffer_size; }

	// gets a constant pointer to the C string
	__fi const char* c_str() const { return m_buffer; }

	// gets a writable char array, do not write more than reserve characters to it.
	__fi char* data() { return m_buffer; }

	// STL adapters
	__fi void push_back(value_type&& val) { append(val); }

	// returns a string view for this string
	std::string_view view() const;

	// returns a substring view for this string
	std::string_view substr(s32 offset, s32 count) const;

	// accessor operators
	__fi operator const char*() const { return c_str(); }
	__fi operator char*() { return data(); }
	__fi operator std::string_view() const { return view(); }

	// comparative operators
	__fi bool operator==(const char* str) const { return equals(str); }
	__fi bool operator==(const SmallStringBase& str) const { return equals(str); }
	__fi bool operator==(const std::string_view& str) const { return equals(str); }
	__fi bool operator!=(const char* str) const { return !equals(str); }
	__fi bool operator!=(const SmallStringBase& str) const { return !equals(str); }
	__fi bool operator!=(const std::string_view& str) const { return !equals(str); }
	__fi bool operator<(const char* str) const { return (compare(str) < 0); }
	__fi bool operator<(const SmallStringBase& str) const { return (compare(str) < 0); }
	__fi bool operator<(const std::string_view& str) const { return (compare(str) < 0); }
	__fi bool operator>(const char* str) const { return (compare(str) > 0); }
	__fi bool operator>(const SmallStringBase& str) const { return (compare(str) > 0); }
	__fi bool operator>(const std::string_view& str) const { return (compare(str) > 0); }

	SmallStringBase& operator=(const SmallStringBase& copy);
	SmallStringBase& operator=(const char* str);
	SmallStringBase& operator=(const std::string& str);
	SmallStringBase& operator=(const std::string_view& str);
	SmallStringBase& operator=(SmallStringBase&& move);

protected:
	// Pointer to memory where the string is located
	char* m_buffer = nullptr;

	// Length of the string located in pBuffer (in characters)
	u32 m_length = 0;

	// Size of the buffer pointed to by pBuffer
	u32 m_buffer_size = 0;

	// True if the string is dynamically allocated on the heap.
	bool m_on_heap = false;
};

// stack-allocated string
template <u32 L>
class SmallStackString : public SmallStringBase
{
public:
	__fi SmallStackString() { init(); }

	__fi SmallStackString(const char* str)
	{
		init();
		assign(str);
	}

	__fi SmallStackString(const char* str, u32 length)
	{
		init();
		assign(str, length);
	}

	__fi SmallStackString(const SmallStringBase& copy)
	{
		init();
		assign(copy);
	}

	__fi SmallStackString(SmallStringBase&& move)
	{
		init();
		assign(move);
	}

	__fi SmallStackString(const std::string_view& sv)
	{
		init();
		assign(sv);
	}

	// Override the fromstring method
	__fi static SmallStackString from_format(const char* format, ...) /*printflike(1, 2)*/
	{
		std::va_list ap;
		va_start(ap, format);

		SmallStackString ret;
		ret.format_va(format, ap);

		va_end(ap);

		return ret;
	}

	template <typename... T>
	__fi static SmallStackString from_fmt(fmt::format_string<T...> fmt, T&&... args)
	{
		SmallStackString ret;
		fmt::vformat_to(std::back_inserter(ret), fmt, fmt::make_format_args(args...));
		return ret;
	}

private:
	char m_stack_buffer[L + 1];

	__fi void init()
	{
		m_buffer = m_stack_buffer;
		m_buffer_size = L + 1;

#ifdef _DEBUG
		std::memset(m_stack_buffer, 0, sizeof(m_stack_buffer));
#else
		m_stack_buffer[0] = '\0';
#endif
	}
};

// stack string types
typedef SmallStackString<64> TinyString;
typedef SmallStackString<256> SmallString;

template <typename... T>
__fi void SmallStringBase::append_fmt(fmt::format_string<T...> fmt, T&&... args)
{
	fmt::vformat_to(std::back_inserter(*this), fmt, fmt::make_format_args(args...));
}

template <typename... T>
__fi void SmallStringBase::prepend_fmt(fmt::format_string<T...> fmt, T&&... args)
{
	TinyString str;
	fmt::vformat_to(std::back_inserter(str), fmt, fmt::make_format_args(args...));
	prepend(str);
}

template <typename... T>
__fi void SmallStringBase::fmt(fmt::format_string<T...> fmt, T&&... args)
{
	clear();
	fmt::vformat_to(std::back_inserter(*this), fmt, fmt::make_format_args(args...));
}
