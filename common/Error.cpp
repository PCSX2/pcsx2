// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Error.h"
#include "StringUtil.h"

#include "fmt/format.h"

#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <type_traits>

// Platform-specific includes
#if defined(_WIN32)
#include "RedtapeWindows.h"
static_assert(std::is_same<DWORD, unsigned long>::value, "DWORD is unsigned long");
static_assert(std::is_same<HRESULT, long>::value, "HRESULT is long");
#endif

Error::Error() = default;

Error::Error(const Error& c) = default;

Error::Error(Error&& e) = default;

Error::~Error() = default;

void Error::Clear()
{
	m_description = {};
}

void Error::Clear(Error* errptr)
{
	if (errptr)
		errptr->Clear();
}

void Error::SetErrno(int err)
{
	SetErrno(std::string_view(), err);
}

void Error::SetErrno(std::string_view prefix, int err)
{
	m_type = Type::Errno;

#ifdef _MSC_VER
	char buf[128];
	if (strerror_s(buf, sizeof(buf), err) == 0)
		m_description = fmt::format("{}errno {}: {}", prefix, err, buf);
	else
		m_description = fmt::format("{}errno {}: <Could not get error message>", prefix, err);
#else
	const char* buf = std::strerror(err);
	if (buf)
		m_description = fmt::format("{}errno {}: {}", prefix, err, buf);
	else
		m_description = fmt::format("{}errno {}: <Could not get error message>", prefix, err);
#endif
}

void Error::SetErrno(Error* errptr, int err)
{
	if (errptr)
		errptr->SetErrno(err);
}

void Error::SetErrno(Error* errptr, std::string_view prefix, int err)
{
	if (errptr)
		errptr->SetErrno(prefix, err);
}

void Error::SetString(std::string description)
{
	m_type = Type::User;
	m_description = std::move(description);
}

void Error::SetStringView(std::string_view description)
{
	m_type = Type::User;
	m_description = std::string(description);
}

void Error::SetString(Error* errptr, std::string description)
{
	if (errptr)
		errptr->SetString(std::move(description));
}

void Error::SetStringView(Error* errptr, std::string_view description)
{
	if (errptr)
		errptr->SetStringView(std::move(description));
}

#ifdef _WIN32

void Error::SetWin32(unsigned long err)
{
	SetWin32(std::string_view(), err);
}

void Error::SetWin32(std::string_view prefix, unsigned long err)
{
	m_type = Type::Win32;

	WCHAR buf[128];
	DWORD r = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_USER_DEFAULT, buf,
		static_cast<DWORD>(std::size(buf)), nullptr);
	while (r > 0 && std::iswspace(buf[r - 1]))
		r--;

	if (r > 0)
	{
		m_description =
			fmt::format("{}Win32 Error {}: {}", prefix, err, StringUtil::WideStringToUTF8String(std::wstring_view(buf, r)));
	}
	else
	{
		m_description = fmt::format("{}Win32 Error {}: <Could not resolve system error ID>", prefix, err);
	}
}

void Error::SetWin32(Error* errptr, unsigned long err)
{
	if (errptr)
		errptr->SetWin32(err);
}

void Error::SetWin32(Error* errptr, std::string_view prefix, unsigned long err)
{
	if (errptr)
		errptr->SetWin32(prefix, err);
}

void Error::SetHResult(long err)
{
	SetHResult(std::string_view(), err);
}

void Error::SetHResult(std::string_view prefix, long err)
{
	m_type = Type::HResult;

	WCHAR buf[128];
	DWORD r = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_USER_DEFAULT, buf,
		static_cast<DWORD>(std::size(buf)), nullptr);
	while (r > 0 && std::iswspace(buf[r - 1]))
		r--;

	if (r > 0)
	{
		m_description = fmt::format("{}HRESULT {:08X}: {}", prefix, static_cast<unsigned>(err),
			StringUtil::WideStringToUTF8String(std::wstring_view(buf, r)));
	}
	else
	{
		m_description = fmt::format("{}HRESULT {:08X}: <Could not resolve system error ID>", prefix,
			static_cast<unsigned>(err));
	}
}

void Error::SetHResult(Error* errptr, long err)
{
	if (errptr)
		errptr->SetHResult(err);
}

void Error::SetHResult(Error* errptr, std::string_view prefix, long err)
{
	if (errptr)
		errptr->SetHResult(prefix, err);
}

#endif

void Error::SetSocket(int err)
{
	SetSocket(std::string_view(), err);
}

void Error::SetSocket(std::string_view prefix, int err)
{
	// Socket errors are win32 errors on windows
#ifdef _WIN32
	SetWin32(prefix, err);
#else
	SetErrno(prefix, err);
#endif
	m_type = Type::Socket;
}

void Error::SetSocket(Error* errptr, int err)
{
	if (errptr)
		errptr->SetSocket(err);
}

void Error::SetSocket(Error* errptr, std::string_view prefix, int err)
{
	if (errptr)
		errptr->SetSocket(prefix, err);
}

Error Error::CreateNone()
{
	return Error();
}

Error Error::CreateErrno(int err)
{
	Error ret;
	ret.SetErrno(err);
	return ret;
}

Error Error::CreateSocket(int err)
{
	Error ret;
	ret.SetSocket(err);
	return ret;
}

Error Error::CreateString(std::string description)
{
	Error ret;
	ret.SetString(std::move(description));
	return ret;
}

#ifdef _WIN32
Error Error::CreateWin32(unsigned long err)
{
	Error ret;
	ret.SetWin32(err);
	return ret;
}

Error Error::CreateHResult(long err)
{
	Error ret;
	ret.SetHResult(err);
	return ret;
}

#endif

void Error::AddPrefix(std::string_view prefix)
{
	m_description.insert(0, prefix);
}

void Error::AddSuffix(std::string_view suffix)
{
	m_description.append(suffix);
}

void Error::AddPrefix(Error* errptr, std::string_view prefix)
{
	if (errptr)
		errptr->AddPrefix(prefix);
}

void Error::AddSuffix(Error* errptr, std::string_view prefix)
{
	if (errptr)
		errptr->AddSuffix(prefix);
}

Error& Error::operator=(const Error& e) = default;

Error& Error::operator=(Error&& e) = default;

bool Error::operator==(const Error& e) const
{
	return (m_type == e.m_type && m_description == e.m_description);
}

bool Error::operator!=(const Error& e) const
{
	return (m_type != e.m_type || m_description != e.m_description);
}
