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

#include "SmallString.h"
#include "Assertions.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#ifdef _MSC_VER
#define CASE_COMPARE _stricmp
#define CASE_N_COMPARE _strnicmp
#else
#define CASE_COMPARE strcasecmp
#define CASE_N_COMPARE strncasecmp
#endif

SmallStringBase::SmallStringBase() = default;

SmallStringBase::SmallStringBase(const SmallStringBase& copy)
{
	assign(copy.m_buffer, copy.m_length);
}

SmallStringBase::SmallStringBase(const char* str)
{
	assign(str);
}

SmallStringBase::SmallStringBase(const char* str, u32 count)
{
	assign(str, count);
}

SmallStringBase::SmallStringBase(SmallStringBase&& move)
{
	assign(std::move(move));
}

SmallStringBase::SmallStringBase(const std::string_view& sv)
{
	assign(sv);
}

 SmallStringBase::SmallStringBase(const std::string& str)
{
	 assign(str);
}

SmallStringBase::~SmallStringBase()
{
	if (m_on_heap)
		std::free(m_buffer);
}

void SmallStringBase::reserve(u32 new_reserve)
{
	if (m_buffer_size >= new_reserve)
		return;

	if (m_on_heap)
	{
		char* new_ptr = static_cast<char*>(std::realloc(m_buffer, new_reserve));
		if (!new_ptr)
			pxFailRel("Memory allocation failed.");

#ifdef _DEBUG
		std::memset(new_ptr + m_length, 0, new_reserve - m_length);
#endif
		m_buffer = new_ptr;
	}
	else
	{
		char* new_ptr = static_cast<char*>(std::malloc(new_reserve));
		if (!new_ptr)
			pxFailRel("Memory allocation failed.");

		if (m_length > 0)
			std::memcpy(new_ptr, m_buffer, m_length);
#ifdef _DEBUG
		std::memset(new_ptr + m_length, 0, new_reserve - m_length);
#else
		new_ptr[m_length] = 0;
#endif
		m_buffer = new_ptr;
		m_on_heap = true;
	}
}

void SmallStringBase::shrink_to_fit()
{
	if (!m_on_heap || m_length == m_buffer_size)
		return;

	if (m_length == 0)
	{
		std::free(m_buffer);
		m_buffer_size = 0;
		return;
	}

	char* new_ptr = static_cast<char*>(std::realloc(m_buffer, m_length));
	if (!new_ptr)
		pxFailRel("Memory allocation failed.");

	m_buffer = new_ptr;
	m_buffer_size = m_length;
}

std::string_view SmallStringBase::view() const
{
	return (m_length == 0) ? std::string_view() : std::string_view(m_buffer, m_length);
}

SmallStringBase& SmallStringBase::operator=(SmallStringBase&& move)
{
	assign(move);
	return *this;
}

SmallStringBase& SmallStringBase::operator=(const std::string_view& str)
{
	assign(str);
	return *this;
}

SmallStringBase& SmallStringBase::operator=(const std::string& str)
{
	assign(str);
	return *this;
}

SmallStringBase& SmallStringBase::operator=(const char* str)
{
	assign(str);
	return *this;
}

SmallStringBase& SmallStringBase::operator=(const SmallStringBase& copy)
{
	assign(copy);
	return *this;
}

void SmallStringBase::make_room_for(u32 space)
{
	const u32 required_size = m_length + space + 1;
	if (m_buffer_size >= required_size)
		return;

	reserve(std::max(required_size, m_buffer_size * 2));
}

void SmallStringBase::append(const char* str, u32 length)
{
	if (length == 0)
		return;

	make_room_for(length);

	pxAssert((length + m_length) < m_buffer_size);

	std::memcpy(m_buffer + m_length, str, length);
	m_length += length;
	m_buffer[m_length] = 0;
}

void SmallStringBase::prepend(const char* str, u32 length)
{
	if (length == 0)
		return;

	make_room_for(length);

	pxAssert((length + m_length) < m_buffer_size);

	std::memmove(m_buffer + length, m_buffer, m_length);
	std::memcpy(m_buffer, str, length);
	m_length += length;
	m_buffer[m_length] = 0;
}

void SmallStringBase::append(char c)
{
	append(&c, 1);
}

void SmallStringBase::append(const SmallStringBase& str)
{
	append(str.m_buffer, str.m_length);
}

void SmallStringBase::append(const char* str)
{
	append(str, static_cast<u32>(std::strlen(str)));
}

void SmallStringBase::append(const std::string& str)
{
	append(str.c_str(), static_cast<u32>(str.length()));
}

void SmallStringBase::append(const std::string_view& str)
{
	append(str.data(), static_cast<u32>(str.length()));
}

void SmallStringBase::append_format(const char* format, ...)
{
	std::va_list ap;
	va_start(ap, format);
	append_format_va(format, ap);
	va_end(ap);
}

void SmallStringBase::append_format_va(const char* format, va_list ap)
{
	// We have a 1KB byte buffer on the stack here. If this is too little, we'll grow it via the heap,
	// but 1KB should be enough for most strings.
	char stack_buffer[1024];
	char* heap_buffer = nullptr;
	char* buffer = stack_buffer;
	u32 buffer_size = std::size(stack_buffer);
	u32 written;

	for (;;)
	{
		std::va_list ap_copy;
		va_copy(ap_copy, ap);
		const int ret = std::vsnprintf(buffer, buffer_size, format, ap_copy);
		va_end(ap_copy);
		if (ret < 0 || ((u32)ret >= (buffer_size - 1)))
		{
			buffer_size *= 2;
			buffer = heap_buffer = reinterpret_cast<char*>(std::realloc(heap_buffer, buffer_size));
			continue;
		}

		written = static_cast<u32>(ret);
		break;
	}

	append(buffer, written);

	if (heap_buffer)
		std::free(heap_buffer);
}

void SmallStringBase::prepend(char c)
{
	prepend(&c, 1);
}

void SmallStringBase::prepend(const SmallStringBase& str)
{
	prepend(str.m_buffer, str.m_length);
}

void SmallStringBase::prepend(const char* str)
{
	prepend(str, static_cast<u32>(std::strlen(str)));
}

void SmallStringBase::prepend(const std::string& str)
{
	prepend(str.c_str(), static_cast<u32>(str.length()));
}

void SmallStringBase::prepend(const std::string_view& str)
{
	prepend(str.data(), static_cast<u32>(str.length()));
}

void SmallStringBase::prepend_format(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	prepend_format_va(format, ap);
	va_end(ap);
}

void SmallStringBase::prepend_format_va(const char* format, va_list ArgPtr)
{
	// We have a 1KB byte buffer on the stack here. If this is too little, we'll grow it via the heap,
	// but 1KB should be enough for most strings.
	char stack_buffer[1024];
	char* heap_buffer = NULL;
	char* buffer = stack_buffer;
	u32 buffer_size = std::size(stack_buffer);
	u32 written;

	for (;;)
	{
		int ret = std::vsnprintf(buffer, buffer_size, format, ArgPtr);
		if (ret < 0 || (static_cast<u32>(ret) >= (buffer_size - 1)))
		{
			buffer_size *= 2;
			buffer = heap_buffer = reinterpret_cast<char*>(std::realloc(heap_buffer, buffer_size));
			continue;
		}

		written = static_cast<u32>(ret);
		break;
	}

	prepend(buffer, written);

	if (heap_buffer)
		std::free(heap_buffer);
}

void SmallStringBase::insert(s32 offset, const char* str)
{
	insert(offset, str, static_cast<u32>(std::strlen(str)));
}

void SmallStringBase::insert(s32 offset, const SmallStringBase& str)
{
	insert(offset, str, str.m_length);
}

void SmallStringBase::insert(s32 offset, const char* str, u32 length)
{
	if (length == 0)
		return;

	make_room_for(length);

	// calc real offset
	u32 real_offset;
	if (offset < 0)
		real_offset = static_cast<u32>(std::max<s32>(0, static_cast<s32>(m_length) + offset));
	else
		real_offset = std::min(static_cast<u32>(offset), m_length);

	// determine number of characters after offset
	pxAssert(real_offset <= m_length);
	const u32 chars_after_offset = m_length - real_offset;
	if (chars_after_offset > 0)
		std::memmove(m_buffer + offset + length, m_buffer + offset, chars_after_offset);

	// insert the string
	std::memcpy(m_buffer + real_offset, str, length);
	m_length += length;

	// ensure null termination
	m_buffer[m_length] = 0;
}

void SmallStringBase::insert(s32 offset, const std::string& str)
{
	insert(offset, str.c_str(), static_cast<u32>(str.size()));
}

void SmallStringBase::insert(s32 offset, const std::string_view& str)
{
	insert(offset, str.data(), static_cast<u32>(str.size()));
}

void SmallStringBase::format(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	format_va(format, ap);
	va_end(ap);
}

void SmallStringBase::format_va(const char* format, va_list ap)
{
	clear();
	append_format_va(format, ap);
}

void SmallStringBase::assign(const SmallStringBase& copy)
{
	assign(copy.c_str(), copy.length());
}

void SmallStringBase::assign(const char* str)
{
	assign(str, static_cast<u32>(std::strlen(str)));
}

void SmallStringBase::assign(const char* str, u32 length)
{
	clear();
	if (length > 0)
		append(str, length);
}

void SmallStringBase::assign(SmallStringBase&& move)
{
	if (move.m_on_heap)
	{
		if (m_on_heap)
			std::free(m_buffer);
		m_buffer = move.m_buffer;
		m_buffer_size = move.m_buffer_size;
		m_length = move.m_length;
		m_on_heap = true;
		move.m_buffer = nullptr;
		move.m_buffer_size = 0;
		move.m_length = 0;
	}
	else
	{
		assign(move.m_buffer, move.m_buffer_size);
	}
}

void SmallStringBase::assign(const std::string& str)
{
	clear();
	append(str.data(), static_cast<u32>(str.size()));
}

void SmallStringBase::assign(const std::string_view& str)
{
	clear();
	append(str.data(), static_cast<u32>(str.size()));
}

bool SmallStringBase::equals(const char* str) const
{
	if (m_length == 0)
		return (std::strlen(str) == 0);
	else
		return (std::strcmp(m_buffer, str) == 0);
}

bool SmallStringBase::equals(const SmallStringBase& str) const
{
	return (m_length == str.m_length && (m_length == 0 || std::strcmp(m_buffer, str.m_buffer) == 0));
}

bool SmallStringBase::equals(const std::string_view& str) const
{
	return (m_length == static_cast<u32>(str.length()) &&
			(m_length == 0 || CASE_N_COMPARE(m_buffer, str.data(), m_length) == 0));
}

bool SmallStringBase::iequals(const char* otherText) const
{
	if (m_length == 0)
		return (std::strlen(otherText) == 0);
	else
		return (CASE_COMPARE(m_buffer, otherText) == 0);
}

bool SmallStringBase::iequals(const SmallStringBase& str) const
{
	return (m_length == str.m_length && (m_length == 0 || std::strcmp(m_buffer, str.m_buffer) == 0));
}

bool SmallStringBase::iequals(const std::string_view& str) const
{
	return (m_length == static_cast<u32>(str.length()) &&
			(m_length == 0 || CASE_N_COMPARE(m_buffer, str.data(), m_length) == 0));
}

int SmallStringBase::compare(const SmallStringBase& str) const
{
	return std::strcmp(m_buffer, str.m_buffer);
}

int SmallStringBase::compare(const char* otherText) const
{
	return std::strcmp(m_buffer, otherText);
}

int SmallStringBase::icompare(const SmallStringBase& otherString) const
{
	return CASE_COMPARE(m_buffer, otherString.m_buffer);
}

int SmallStringBase::icompare(const char* otherText) const
{
	return CASE_COMPARE(m_buffer, otherText);
}

bool SmallStringBase::starts_with(const char* str, bool case_sensitive) const
{
	const u32 other_length = static_cast<u32>(std::strlen(str));
	if (other_length > m_length)
		return false;

	return (case_sensitive) ? (std::strncmp(str, m_buffer, other_length) == 0) :
							  (CASE_N_COMPARE(str, m_buffer, other_length) == 0);
}

bool SmallStringBase::starts_with(const SmallStringBase& str, bool case_sensitive) const
{
	const u32 other_length = str.m_length;
	if (other_length > m_length)
		return false;

	return (case_sensitive) ? (std::strncmp(str.m_buffer, m_buffer, other_length) == 0) :
							  (CASE_N_COMPARE(str.m_buffer, m_buffer, other_length) == 0);
}

bool SmallStringBase::starts_with(const std::string_view& str, bool case_sensitive) const
{
	const u32 other_length = static_cast<u32>(str.length());
	if (other_length > m_length)
		return false;

	return (case_sensitive) ? (std::strncmp(str.data(), m_buffer, other_length) == 0) :
							  (CASE_N_COMPARE(str.data(), m_buffer, other_length) == 0);
}

bool SmallStringBase::ends_with(const char* str, bool case_sensitive) const
{
	const u32 other_length = static_cast<u32>(std::strlen(str));
	if (other_length > m_length)
		return false;

	u32 start_offset = m_length - other_length;
	return (case_sensitive) ? (std::strncmp(str, m_buffer + start_offset, other_length) == 0) :
							  (CASE_N_COMPARE(str, m_buffer + start_offset, other_length) == 0);
}

bool SmallStringBase::ends_with(const SmallStringBase& str, bool case_sensitive) const
{
	const u32 other_length = str.m_length;
	if (other_length > m_length)
		return false;

	const u32 start_offset = m_length - other_length;
	return (case_sensitive) ? (std::strncmp(str.m_buffer, m_buffer + start_offset, other_length) == 0) :
							  (CASE_N_COMPARE(str.m_buffer, m_buffer + start_offset, other_length) == 0);
}

bool SmallStringBase::ends_with(const std::string_view& str, bool case_sensitive) const
{
	const u32 other_length = static_cast<u32>(str.length());
	if (other_length > m_length)
		return false;

	const u32 start_offset = m_length - other_length;
	return (case_sensitive) ? (std::strncmp(str.data(), m_buffer + start_offset, other_length) == 0) :
							  (CASE_N_COMPARE(str.data(), m_buffer + start_offset, other_length) == 0);
}

void SmallStringBase::clear()
{
	// in debug, zero whole string, in release, zero only the first character
#if _DEBUG
	std::memset(m_buffer, 0, m_buffer_size);
#else
	m_buffer[0] = '\0';
#endif
	m_length = 0;
}

s32 SmallStringBase::find(char c, u32 offset) const
{
	if (m_length == 0)
		return -1;

	pxAssert(offset <= m_length);
	const char* at = std::strchr(m_buffer + offset, c);
	return at ? static_cast<s32>(at - m_buffer) : -1;
}

s32 SmallStringBase::rfind(char c, u32 offset) const
{
	if (m_length == 0)
		return -1;

	pxAssert(offset <= m_length);
	const char* at = std::strrchr(m_buffer + offset, c);
	return at ? static_cast<s32>(at - m_buffer) : -1;
}

s32 SmallStringBase::find(const char* str, u32 offset) const
{
	if (m_length == 0)
		return -1;

	pxAssert(offset <= m_length);
	const char* at = std::strstr(m_buffer + offset, str);
	return at ? static_cast<s32>(at - m_buffer) : -1;
}

void SmallStringBase::resize(u32 new_size, char fill, bool shrink_if_smaller)
{
	// if going larger, or we don't own the buffer, realloc
	if (new_size >= m_buffer_size)
	{
		reserve(new_size);

		if (m_length < new_size)
		{
			std::memset(m_buffer + m_length, fill, m_buffer_size - m_length - 1);
		}

		m_length = new_size;
	}
	else
	{
		// update length and terminator
#if _DEBUG
		std::memset(m_buffer + new_size, 0, m_buffer_size - new_size);
#else
		m_buffer[new_size] = 0;
#endif
		m_length = new_size;

		// shrink if requested
		if (shrink_if_smaller)
			shrink_to_fit();
	}
}

void SmallStringBase::update_size()
{
	m_length = static_cast<u32>(std::strlen(m_buffer));
}

std::string_view SmallStringBase::substr(s32 offset, s32 count) const
{
	// calc real offset
	u32 real_offset;
	if (offset < 0)
		real_offset = static_cast<u32>(std::max<s32>(0, static_cast<s32>(m_length + offset)));
	else
		real_offset = std::min((u32)offset, m_length);

	// calc real count
	u32 real_count;
	if (count < 0)
	{
		real_count =
			std::min(m_length - real_offset, static_cast<u32>(std::max<s32>(0, static_cast<s32>(m_length) + count)));
	}
	else
	{
		real_count = std::min(m_length - real_offset, static_cast<u32>(count));
	}

	return (real_count > 0) ? std::string_view(m_buffer + real_offset, real_count) : std::string_view();
}

void SmallStringBase::erase(s32 offset, s32 count)
{
	// calc real offset
	u32 real_offset;
	if (offset < 0)
		real_offset = static_cast<u32>(std::max<s32>(0, static_cast<s32>(m_length + offset)));
	else
		real_offset = std::min((u32)offset, m_length);

	// calc real count
	u32 real_count;
	if (count < 0)
	{
		real_count =
			std::min(m_length - real_offset, static_cast<u32>(std::max<s32>(0, static_cast<s32>(m_length) + count)));
	}
	else
	{
		real_count = std::min(m_length - real_offset, static_cast<u32>(count));
	}

	// Fastpath: offset == 0, count < 0, wipe whole string.
	if (real_offset == 0 && real_count == m_length)
	{
		clear();
		return;
	}

	// Fastpath: offset >= 0, count < 0, wipe everything after offset + count
	if ((real_offset + real_count) == m_length)
	{
		m_length -= real_count;
#ifdef _DEBUG
		std::memset(m_buffer + m_length, 0, m_buffer_size - m_length);
#else
		m_buffer[m_length] = 0;
#endif
	}
	// Slowpath: offset >= 0, count < length
	else
	{
		const u32 after_erase_block = m_length - real_offset - real_count;
		pxAssert(after_erase_block > 0);

		std::memmove(m_buffer + offset, m_buffer + real_offset + real_count, after_erase_block);
		m_length = m_length - real_count;

#ifdef _DEBUG
		std::memset(m_buffer + m_length, 0, m_buffer_size - m_length);
#else
		m_buffer[m_length] = 0;
#endif
	}
}
