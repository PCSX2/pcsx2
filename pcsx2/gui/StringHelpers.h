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

#pragma once

#include <cinttypes>
#include <string_view>
#include <wx/gdicmn.h>
#include <wx/tokenzr.h>
#include "common/Pcsx2Defs.h"
#include "common/SafeArray.h"
#include "common/AlignedMalloc.h"

// This should prove useful....
#define wxsFormat wxString::Format

#define WX_STR(str) ((str).wc_str())

extern void px_fputs(FILE* fp, const char* src);

// wxWidgets lacks one of its own...
extern const wxRect wxDefaultRect;

extern void SplitString(wxArrayString& dest, const wxString& src, const wxString& delims, wxStringTokenizerMode mode = wxTOKEN_RET_EMPTY_ALL);
extern wxString JoinString(const wxArrayString& src, const wxString& separator);
extern wxString JoinString(const wxChar** src, const wxString& separator);

extern wxString ToString(const wxPoint& src, const wxString& separator = L",");
extern wxString ToString(const wxSize& src, const wxString& separator = L",");
extern wxString ToString(const wxRect& src, const wxString& separator = L",");

extern bool TryParse(wxPoint& dest, const wxStringTokenizer& parts);
extern bool TryParse(wxSize& dest, const wxStringTokenizer& parts);

extern bool TryParse(wxPoint& dest, const wxString& src, const wxPoint& defval = wxDefaultPosition, const wxString& separators = L",");
extern bool TryParse(wxSize& dest, const wxString& src, const wxSize& defval = wxDefaultSize, const wxString& separators = L",");
extern bool TryParse(wxRect& dest, const wxString& src, const wxRect& defval = wxDefaultRect, const wxString& separators = L",");


// ======================================================================================
//  FastFormatAscii / FastFormatUnicode  (overview!)
// ======================================================================================
// Fast formatting of ASCII or Unicode text.  These classes uses a series of thread-local
// format buffers that are allocated only once and grown to accommodate string formatting
// needs.  Because the buffers are thread-local, no thread synch objects are required in
// order to format strings, allowing for multi-threaded string formatting operations to be
// performed with maximum efficiency.  This class also reduces the overhead typically required
// to allocate string buffers off the heap.
//
// Drawbacks:
//  * Some overhead is added to the creation and destruction of threads, however since thread
//    construction is a typically slow process, and often avoided to begin with, this should
//    be a sound trade-off.
//
// Notes:
//  * conversion to wxString requires a heap allocation.
//  * FastFormatUnicode can accept either UTF8 or UTF16/32 (wchar_t) input, but FastFormatAscii
//    accepts Ascii/UTF8 only.
//

typedef AlignedBuffer<char, 16> CharBufferType;
// --------------------------------------------------------------------------------------
//  FastFormatAscii
// --------------------------------------------------------------------------------------

class FastFormatAscii
{
protected:
	CharBufferType m_dest;

public:
	FastFormatAscii();
	~FastFormatAscii() = default;
	FastFormatAscii& Write(const char* fmt, ...);
	FastFormatAscii& WriteV(const char* fmt, va_list argptr);

	void Clear();
	bool IsEmpty() const;

	const char* c_str() const { return m_dest.GetPtr(); }
	operator const char*() const { return m_dest.GetPtr(); }

	const wxString GetString() const;
	//operator wxString() const;

	FastFormatAscii& operator+=(const wxString& s)
	{
		Write("%s", WX_STR(s));
		return *this;
	}

	FastFormatAscii& operator+=(const wxChar* psz)
	{
		Write("%ls", psz);
		return *this;
	}

	FastFormatAscii& operator+=(const char* psz)
	{
		Write("%s", psz);
		return *this;
	}
};

// --------------------------------------------------------------------------------------
//  FastFormatUnicode
// --------------------------------------------------------------------------------------
class FastFormatUnicode
{
protected:
	CharBufferType m_dest;
	uint m_Length;

public:
	FastFormatUnicode();
	~FastFormatUnicode() = default;

	FastFormatUnicode& Write(const char* fmt, ...);
	FastFormatUnicode& Write(const wxChar* fmt, ...);
	FastFormatUnicode& Write(const wxString fmt, ...);
	FastFormatUnicode& WriteV(const char* fmt, va_list argptr);
	FastFormatUnicode& WriteV(const wxChar* fmt, va_list argptr);

	void Clear();
	bool IsEmpty() const;
	uint Length() const { return m_Length; }

	FastFormatUnicode& ToUpper();
	FastFormatUnicode& ToLower();

	const wxChar* c_str() const { return (const wxChar*)m_dest.GetPtr(); }
	operator const wxChar*() const { return (const wxChar*)m_dest.GetPtr(); }
	operator wxString() const { return (const wxChar*)m_dest.GetPtr(); }

	FastFormatUnicode& operator+=(const wxString& s)
	{
		Write(L"%s", WX_STR(s));
		return *this;
	}

	FastFormatUnicode& operator+=(const wxChar* psz)
	{
		Write(L"%s", psz);
		return *this;
	}

	FastFormatUnicode& operator+=(const char* psz);

	wxScopedCharBuffer ToUTF8() const { return wxString(m_dest.GetPtr()).ToUTF8(); }
	std::string ToStdString() const { return wxString(m_dest.GetPtr()).ToStdString(); }
};

#define pxsFmt FastFormatUnicode().Write
#define pxsFmtV FastFormatUnicode().WriteV
#define pxsPtr(ptr) pxsFmt("0x%016" PRIXPTR, (ptr)).c_str()

extern wxString& operator+=(wxString& str1, const FastFormatUnicode& str2);
extern wxString operator+(const wxString& str1, const FastFormatUnicode& str2);
extern wxString operator+(const wxChar* str1, const FastFormatUnicode& str2);
extern wxString operator+(const FastFormatUnicode& str1, const wxString& str2);
extern wxString operator+(const FastFormatUnicode& str1, const wxChar* str2);

extern wxString fromUTF8(const std::string& str);
extern wxString fromUTF8(const char* src);
extern wxString fromAscii(const char* src);


// --------------------------------------------------------------------------------------
//  _(x) / _t(x) / _d(x) / pxL(x) / pxLt(x)  [macros]
// --------------------------------------------------------------------------------------
// Define pxWex's own i18n helpers.  These override the wxWidgets helpers and provide
// additional functionality.  Define them FIRST THING, to make sure that wx's own gettext
// macros aren't in place.
//
// _   is for standard translations
// _t  is for tertiary low priority translations
// _d  is for debug/devel build translations

#define WXINTL_NO_GETTEXT_MACRO

#ifndef _
#define _(s) pxGetTranslation(_T(s))
#endif

#ifndef _t
#define _t(s) pxGetTranslation(_T(s))
#endif

#ifndef _d
#define _d(s) pxGetTranslation(_T(s))
#endif

// pxL / pxLt / pxDt -- macros provided for tagging translation strings, without actually running
// them through the translator (which the _() does automatically, and sometimes we don't
// want that).  This is a shorthand replacement for wxTRANSLATE.  pxL is a standard translation
// moniker.  pxLt is for tertiary strings that have a very low translation priority.  pxDt is for
// debug/devel specific translations.
//
#ifndef pxL
#define pxL(a) wxT(a)
#endif

#ifndef pxLt
#define pxLt(a) wxT(a)
#endif

#ifndef pxDt
#define pxDt(a) wxT(a)
#endif

// --------------------------------------------------------------------------------------
//  pxE(msg) and pxEt(msg)  [macros] => now same as _/_t/_d
// --------------------------------------------------------------------------------------
#define pxE(english) pxExpandMsg((english))

// For use with tertiary translations (low priority).
#define pxEt(english) pxExpandMsg((english))

// For use with Dev/debug build translations (low priority).
#define pxE_dev(english) pxExpandMsg((english))


extern const wxChar* pxExpandMsg(const wxChar* message);
extern const wxChar* pxGetTranslation(const wxChar* message);
extern bool pxIsEnglish(int id);

namespace StringUtil
{
	/// Converts a wxString to a UTF-8 std::string.
	static inline std::string wxStringToUTF8String(const wxString& str)
	{
		const wxScopedCharBuffer buf(str.ToUTF8());
		return std::string(buf.data(), buf.length());
	}

	/// Converts a UTF-8 std::string to a wxString.
	static inline wxString UTF8StringToWxString(const std::string_view& str)
	{
		return wxString::FromUTF8(str.data(), str.length());
	}
}
