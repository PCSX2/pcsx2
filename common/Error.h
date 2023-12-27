// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "Pcsx2Defs.h"
#include <string>

class Error
{
public:
	Error();
	Error(const Error& e);
	Error(Error&& e);
	~Error();

	enum class Type
	{
		None = 0,
		Errno = 1,
		Socket = 2,
		User = 3,
		Win32 = 4,
		HResult = 5,
	};

	__fi Type GetType() const { return m_type; }
	__fi const std::string& GetDescription() const { return m_description; }

	void Clear();

	/// Error that is set by system functions, such as open().
	void SetErrno(int err);

	/// Error that is set by socket functions, such as socket(). On Unix this is the same as errno.
	void SetSocket(int err);

	/// Set both description and message.
	void SetString(std::string description);

#ifdef _WIN32
	/// Error that is returned by some Win32 functions, such as RegOpenKeyEx. Also used by other APIs through GetLastError().
	void SetWin32(unsigned long err);

	/// Error that is returned by Win32 COM methods, e.g. S_OK.
	void SetHResult(long err);
#endif

	static Error CreateNone();
	static Error CreateErrno(int err);
	static Error CreateSocket(int err);
	static Error CreateString(std::string description);
#ifdef _WIN32
	static Error CreateWin32(unsigned long err);
	static Error CreateHResult(long err);
#endif

	// helpers for setting
	static void SetErrno(Error* errptr, int err);
	static void SetSocket(Error* errptr, int err);
	static void SetString(Error* errptr, std::string description);
	static void SetWin32(Error* errptr, unsigned long err);
	static void SetHResult(Error* errptr, long err);

	Error& operator=(const Error& e);
	Error& operator=(Error&& e);
	bool operator==(const Error& e) const;
	bool operator!=(const Error& e) const;

private:
	Type m_type = Type::None;
	std::string m_description;
};
