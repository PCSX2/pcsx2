/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include "common/Pcsx2Defs.h"
#include "common/SafeArray.inl"
#include "gui/StringHelpers.h"
#include <wx/wxcrt.h>
#include <wx/wxcrtvararg.h>

// Implement some very commonly used SafeArray types here
// (done here for lack of a better place)

template class SafeArray<char>;
template class SafeArray<wchar_t>;
template class SafeArray<u8>;

template class SafeAlignedArray<char, 16>;
template class SafeAlignedArray<wchar_t, 16>;
template class SafeAlignedArray<u8, 16>;

// Sanity check: truncate strings if they exceed 512k in length.  Anything like that
// is either a bug or really horrible code that needs to be stopped before it causes
// system deadlock.
static const int MaxFormattedStringLength = 0x80000;

#ifndef __linux__
static __ri void format_that_ascii_mess(CharBufferType& buffer, uint writepos, const char* fmt, va_list argptr)
#else
static void format_that_ascii_mess(CharBufferType& buffer, uint writepos, const char* fmt, va_list argptr)
#endif
{
	va_list args;
	while (true)
	{
		int size = buffer.GetLength();

		va_copy(args, argptr);
		int len = vsnprintf(buffer.GetPtr(writepos), size - writepos, fmt, args);
		va_end(args);

		// some implementations of vsnprintf() don't NUL terminate
		// the string if there is not enough space for it so
		// always do it manually
		buffer[size - 1] = '\0';

		if (size >= MaxFormattedStringLength)
			break;

		// vsnprintf() may return either -1 (traditional Unix behavior) or the
		// total number of characters which would have been written if the
		// buffer were large enough (newer standards such as Unix98)

		if (len < 0)
			len = size + (size / 4);

		len += writepos;
		if (len < size)
			break;
		buffer.Resize(len + 128);
	};

	// performing an assertion or log of a truncated string is unsafe, so let's not; even
	// though it'd be kinda nice if we did.
}

// returns the length of the formatted string, in characters (wxChars).
#ifndef __linux__
static __ri uint format_that_unicode_mess(CharBufferType& buffer, uint writepos, const wxChar* fmt, va_list argptr)
#else
static uint format_that_unicode_mess(CharBufferType& buffer, uint writepos, const wxChar* fmt, va_list argptr)
#endif
{
	va_list args;
	while (true)
	{
		int size = buffer.GetLength() / sizeof(wxChar);

		va_copy(args, argptr);
		int len = wxVsnprintf((wxChar*)buffer.GetPtr(writepos * sizeof(wxChar)), size - writepos, fmt, args);
		va_end(args);

		// some implementations of vsnprintf() don't NUL terminate
		// the string if there is not enough space for it so
		// always do it manually
		((wxChar*)buffer.GetPtr())[size - 1] = L'\0';

		if (size >= MaxFormattedStringLength)
			return size - 1;

		// vsnprintf() may return either -1 (traditional Unix behavior) or the
		// total number of characters which would have been written if the
		// buffer were large enough (newer standards such as Unix98)

		if (len < 0)
			len = size + (size / 4);

		len += writepos;
		if (len < size)
			return len;
		buffer.Resize((len + 128) * sizeof(wxChar));
	};

	// performing an assertion or log of a truncated string is unsafe, so let's not; even
	// though it'd be kinda nice if we did.

	pxAssume(false);
	return 0; // unreachable.
}

// --------------------------------------------------------------------------------------
//  FastFormatUnicode  (implementations)
// --------------------------------------------------------------------------------------
// [TODO] This class should actually be renamed to FastFormatNative or FastFormatString, and
// adopted to properly support 1-byte wxChar types (mostly requiring some changes to the
// WriteV functions).  The current implementation is fine for wx2.8, which always defaults
// to wide-varieties of wxChar -- but wx3.0 will use UTF8 for linux distros, which will break
// this class nicely in its current state. --air

FastFormatUnicode::FastFormatUnicode()
	: m_dest(2048)
{
	Clear();
}

void FastFormatUnicode::Clear()
{
	m_Length = 0;
	((wxChar*)m_dest.GetPtr())[0] = 0;
}

FastFormatUnicode& FastFormatUnicode::WriteV(const char* fmt, va_list argptr)
{
	wxString converted(fromUTF8(FastFormatAscii().WriteV(fmt, argptr)));

	const uint inspos = m_Length;
	const uint convLen = converted.Length();
	m_dest.MakeRoomFor((inspos + convLen + 64) * sizeof(wxChar));
	memcpy(&((wxChar*)m_dest.GetPtr())[inspos], converted.wc_str(), (convLen + 1) * sizeof(wxChar));
	m_Length += convLen;

	return *this;
}

FastFormatUnicode& FastFormatUnicode::WriteV(const wxChar* fmt, va_list argptr)
{
	m_Length = format_that_unicode_mess(m_dest, m_Length, fmt, argptr);
	return *this;
}

FastFormatUnicode& FastFormatUnicode::Write(const char* fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	WriteV(fmt, list);
	va_end(list);
	return *this;
}

FastFormatUnicode& FastFormatUnicode::Write(const wxChar* fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	WriteV(fmt, list);
	va_end(list);
	return *this;
}

FastFormatUnicode& FastFormatUnicode::Write(const wxString fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	WriteV(fmt.wx_str(), list);
	va_end(list);
	return *this;
}

bool FastFormatUnicode::IsEmpty() const
{
	return ((wxChar&)m_dest[0]) == 0;
}

FastFormatUnicode& FastFormatUnicode::ToUpper()
{
	wxChar* ch = (wxChar*)m_dest.GetPtr();
	for (uint i = 0; i < m_Length; ++i, ++ch)
		*ch = (wxChar)wxToupper(*ch);

	return *this;
}

FastFormatUnicode& FastFormatUnicode::ToLower()
{
	wxChar* ch = (wxChar*)m_dest.GetPtr();
	for (uint i = 0; i < m_Length; ++i, ++ch)
		*ch = (wxChar)wxTolower(*ch);

	return *this;
}

FastFormatUnicode& FastFormatUnicode::operator+=(const char* psz)
{
	Write(L"%s", WX_STR(fromUTF8(psz)));
	return *this;
}

wxString& operator+=(wxString& str1, const FastFormatUnicode& str2)
{
	str1.Append(str2.c_str(), str2.Length());
	return str1;
}

wxString operator+(const wxString& str1, const FastFormatUnicode& str2)
{
	wxString s = str1;
	s += str2;
	return s;
}

wxString operator+(const wxChar* str1, const FastFormatUnicode& str2)
{
	wxString s = str1;
	s += str2;
	return s;
}

wxString operator+(const FastFormatUnicode& str1, const wxString& str2)
{
	wxString s = str1;
	s += str2;
	return s;
}

wxString operator+(const FastFormatUnicode& str1, const wxChar* str2)
{
	wxString s = str1;
	s += str2;
	return s;
}


// --------------------------------------------------------------------------------------
//  FastFormatAscii  (implementations)
// --------------------------------------------------------------------------------------
FastFormatAscii::FastFormatAscii()
	: m_dest(2048)
{
	Clear();
}

void FastFormatAscii::Clear()
{
	m_dest.GetPtr()[0] = 0;
}

FastFormatAscii& FastFormatAscii::WriteV(const char* fmt, va_list argptr)
{
	format_that_ascii_mess(m_dest, strlen(m_dest.GetPtr()), fmt, argptr);
	return *this;
}

FastFormatAscii& FastFormatAscii::Write(const char* fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	WriteV(fmt, list);
	va_end(list);
	return *this;
}


bool FastFormatAscii::IsEmpty() const
{
	return m_dest[0] == 0;
}
