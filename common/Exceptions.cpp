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

#include "Threading.h"
#include "General.h"
#include "Exceptions.h"
#include "CrashHandler.h"

#include <mutex>
#include "fmt/core.h"

#ifdef _WIN32
#include "RedtapeWindows.h"
#include <intrin.h>
#include <tlhelp32.h>
#endif

#ifdef __UNIX__
#include <signal.h>
#endif

// Because wxTrap isn't available on Linux builds of wxWidgets (non-Debug, typically)
void pxTrap()
{
#if defined(_WIN32)
	__debugbreak();
#elif defined(__UNIX__)
	raise(SIGTRAP);
#else
	abort();
#endif
}

static std::mutex s_assertion_failed_mutex;

static inline void FreezeThreads(void** handle)
{
#if defined(_WIN32) && !defined(_UWP)
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snapshot != INVALID_HANDLE_VALUE)
	{
		THREADENTRY32 threadEntry;
		if (Thread32First(snapshot, &threadEntry))
		{
			do
			{
				if (threadEntry.th32ThreadID == GetCurrentThreadId())
					continue;

				HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, threadEntry.th32ThreadID);
				if (hThread != nullptr)
				{
					SuspendThread(hThread);
					CloseHandle(hThread);
				}
			} while (Thread32Next(snapshot, &threadEntry));
		}
	}

	*handle = static_cast<void*>(snapshot);
#else
	* handle = nullptr;
#endif
}

static inline void ResumeThreads(void* handle)
{
#if defined(_WIN32) && !defined(_UWP)
	if (handle != INVALID_HANDLE_VALUE)
	{
		THREADENTRY32 threadEntry;
		if (Thread32First(reinterpret_cast<HANDLE>(handle), &threadEntry))
		{
			do
			{
				if (threadEntry.th32ThreadID == GetCurrentThreadId())
					continue;

				HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, threadEntry.th32ThreadID);
				if (hThread != nullptr)
				{
					ResumeThread(hThread);
					CloseHandle(hThread);
				}
			} while (Thread32Next(reinterpret_cast<HANDLE>(handle), &threadEntry));
		}
		CloseHandle(reinterpret_cast<HANDLE>(handle));
	}
#else
#endif
}

void pxOnAssertFail(const char* file, int line, const char* func, const char* msg)
{
	std::unique_lock guard(s_assertion_failed_mutex);

	void* handle;
	FreezeThreads(&handle);

	char full_msg[512];
	std::snprintf(full_msg, sizeof(full_msg), "%s:%d: assertion failed in function %s: %s\n", file, line, func, msg);

#if defined(_WIN32) && !defined(_UWP)
	HANDLE error_handle = GetStdHandle(STD_ERROR_HANDLE);
	if (error_handle != INVALID_HANDLE_VALUE)
		WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), full_msg, static_cast<DWORD>(std::strlen(full_msg)), NULL, NULL);
	OutputDebugStringA(full_msg);

	std::snprintf(
		full_msg, sizeof(full_msg),
		"Assertion failed in function %s (%s:%d):\n\n%s\n\nPress Abort to exit, Retry to break to debugger, or Ignore to attempt to continue.",
		func, file, line, msg);

	int result = MessageBoxA(NULL, full_msg, NULL, MB_ABORTRETRYIGNORE | MB_ICONERROR);
	if (result == IDRETRY)
	{
		__debugbreak();
	}
	else if (result != IDIGNORE)
	{
		// try to save a crash dump before exiting
		CrashHandler::WriteDumpForCaller();
		TerminateProcess(GetCurrentProcess(), 0xBAADC0DE);
	}
#else
	fputs(full_msg, stderr);
	fputs("\nAborting application.\n", stderr);
	fflush(stderr);
	abort();
#endif

	ResumeThreads(handle);
}

// --------------------------------------------------------------------------------------
//  BaseException  (implementations)
// --------------------------------------------------------------------------------------

BaseException& BaseException::SetBothMsgs(const char* msg_diag)
{
	m_message_user = msg_diag ? std::string(msg_diag) : std::string();
	return SetDiagMsg(msg_diag);
}

BaseException& BaseException::SetDiagMsg(std::string msg_diag)
{
	m_message_diag = std::move(msg_diag);
	return *this;
}

BaseException& BaseException::SetUserMsg(std::string msg_user)
{
	m_message_user = std::move(msg_user);
	return *this;
}

std::string BaseException::FormatDiagnosticMessage() const
{
	return m_message_diag;
}

std::string BaseException::FormatDisplayMessage() const
{
	return m_message_user.empty() ? m_message_diag : m_message_user;
}

// --------------------------------------------------------------------------------------
//  Exception::RuntimeError   (implementations)
// --------------------------------------------------------------------------------------
Exception::RuntimeError::RuntimeError(const std::runtime_error& ex, const char* prefix /* = nullptr */)
{
	IsSilent = false;

	const bool has_prefix = prefix && prefix[0] != 0;

	SetDiagMsg(fmt::format("STL Runtime Error{}{}{}: {}",
		has_prefix ? " (" : "", prefix ? prefix : "", has_prefix ? ")" : "",
		ex.what()));
}

Exception::RuntimeError::RuntimeError(const std::exception& ex, const char* prefix /* = nullptr */)
{
	IsSilent = false;

	const bool has_prefix = prefix && prefix[0] != 0;

	SetDiagMsg(fmt::format("STL Exception{}{}{}: {}",
		has_prefix ? " (" : "", prefix ? prefix : "", has_prefix ? ")" : "",
		ex.what()));
}

// --------------------------------------------------------------------------------------
//  Exception::OutOfMemory   (implementations)
// --------------------------------------------------------------------------------------
Exception::OutOfMemory::OutOfMemory(std::string allocdesc)
  : AllocDescription(std::move(allocdesc))
{
}

std::string Exception::OutOfMemory::FormatDiagnosticMessage() const
{
	std::string retmsg;
	retmsg = "Out of memory";
	if (!AllocDescription.empty())
		fmt::format_to(std::back_inserter(retmsg), " while allocating '{}'", AllocDescription);

	if (!m_message_diag.empty())
		fmt::format_to(std::back_inserter(retmsg), ":\n{}", m_message_diag);

	return retmsg;
}

std::string Exception::OutOfMemory::FormatDisplayMessage() const
{
	std::string retmsg;
	retmsg = "Oh noes! Out of memory!";

	if (!m_message_user.empty())
		fmt::format_to(std::back_inserter(retmsg), "\n\n{}", m_message_user);

	return retmsg;
}


// --------------------------------------------------------------------------------------
//  Exception::VirtualMemoryMapConflict   (implementations)
// --------------------------------------------------------------------------------------
Exception::VirtualMemoryMapConflict::VirtualMemoryMapConflict(std::string allocdesc)
{
	AllocDescription = std::move(allocdesc);
	m_message_user = "Virtual memory mapping failure!  Your system may have conflicting device drivers, services, or may simply have insufficient memory or resources to meet PCSX2's lofty needs.";
}

std::string Exception::VirtualMemoryMapConflict::FormatDiagnosticMessage() const
{
	std::string retmsg;
	retmsg = "Virtual memory map failed";
	if (!AllocDescription.empty())
		fmt::format_to(std::back_inserter(retmsg), " while reserving '{}'", AllocDescription);

	if (!m_message_diag.empty())
		fmt::format_to(std::back_inserter(retmsg), ":\n{}", m_message_diag);

	return retmsg;
}

std::string Exception::VirtualMemoryMapConflict::FormatDisplayMessage() const
{
	std::string retmsg;
	retmsg = "There is not enough virtual memory available, or necessary virtual memory mappings have already been reserved by other processes, services, or DLLs.";

	if (!m_message_diag.empty())
		fmt::format_to(std::back_inserter(retmsg), "\n\n{}", m_message_diag);

	return retmsg;
}


// ------------------------------------------------------------------------
std::string Exception::CancelEvent::FormatDiagnosticMessage() const
{
	return "Action canceled: " + m_message_diag;
}

std::string Exception::CancelEvent::FormatDisplayMessage() const
{
	return "Action canceled: " + m_message_diag;
}

// --------------------------------------------------------------------------------------
//  Exception::BadStream  (implementations)
// --------------------------------------------------------------------------------------
std::string Exception::BadStream::FormatDiagnosticMessage() const
{
	std::string retval;
	_formatDiagMsg(retval);
	return retval;
}

std::string Exception::BadStream::FormatDisplayMessage() const
{
	std::string retval;
	_formatUserMsg(retval);
	return retval;
}

void Exception::BadStream::_formatDiagMsg(std::string& dest) const
{
	fmt::format_to(std::back_inserter(dest), "Path: ");
	if (!StreamName.empty())
		fmt::format_to(std::back_inserter(dest), "{}", StreamName);
	else
		dest += "[Unnamed or unknown]";

	if (!m_message_diag.empty())
		fmt::format_to(std::back_inserter(dest), "\n{}", m_message_diag);
}

void Exception::BadStream::_formatUserMsg(std::string& dest) const
{
	fmt::format_to(std::back_inserter(dest), "Path: ");
	if (!StreamName.empty())
		fmt::format_to(std::back_inserter(dest), "{}", StreamName);
	else
		dest += "[Unnamed or unknown]";

	if (!m_message_user.empty())
		fmt::format_to(std::back_inserter(dest), "\n{}", m_message_user);
}

// --------------------------------------------------------------------------------------
//  Exception::CannotCreateStream  (implementations)
// --------------------------------------------------------------------------------------
std::string Exception::CannotCreateStream::FormatDiagnosticMessage() const
{
	std::string retval;
	retval = "File could not be created.";
	_formatDiagMsg(retval);
	return retval;
}

std::string Exception::CannotCreateStream::FormatDisplayMessage() const
{
	std::string retval;
	retval = "A file could not be created.\n";
	_formatUserMsg(retval);
	return retval;
}

// --------------------------------------------------------------------------------------
//  Exception::FileNotFound  (implementations)
// --------------------------------------------------------------------------------------
std::string Exception::FileNotFound::FormatDiagnosticMessage() const
{
	std::string retval;
	retval = "File not found.\n";
	_formatDiagMsg(retval);
	return retval;
}

std::string Exception::FileNotFound::FormatDisplayMessage() const
{
	std::string retval;
	retval = "File not found.\n";
	_formatUserMsg(retval);
	return retval;
}

// --------------------------------------------------------------------------------------
//  Exception::AccessDenied  (implementations)
// --------------------------------------------------------------------------------------
std::string Exception::AccessDenied::FormatDiagnosticMessage() const
{
	std::string retval;
	retval = "Permission denied to file.\n";
	_formatDiagMsg(retval);
	return retval;
}

std::string Exception::AccessDenied::FormatDisplayMessage() const
{
	std::string retval;
	retval = "Permission denied while trying to open file, likely due to insufficient user account rights.\n";
	_formatUserMsg(retval);
	return retval;
}

// --------------------------------------------------------------------------------------
//  Exception::EndOfStream  (implementations)
// --------------------------------------------------------------------------------------
std::string Exception::EndOfStream::FormatDiagnosticMessage() const
{
	std::string retval;
	retval = "Unexpected end of file or stream.\n";
	_formatDiagMsg(retval);
	return retval;
}

std::string Exception::EndOfStream::FormatDisplayMessage() const
{
	std::string retval;
	retval = "Unexpected end of file or stream encountered.  File is probably truncated or corrupted.\n";
	_formatUserMsg(retval);
	return retval;
}

// --------------------------------------------------------------------------------------
//  Exceptions from Errno (POSIX)
// --------------------------------------------------------------------------------------

// Translates an Errno code into an exception.
// Throws an exception based on the given error code (usually taken from ANSI C's errno)
BaseException* Exception::FromErrno(std::string streamname, int errcode)
{
	pxAssumeDev(errcode != 0, "Invalid NULL error code?  (errno)");

	switch (errcode)
	{
		case EINVAL:
			pxFailDev("Invalid argument");
			return &(new Exception::BadStream(streamname))->SetDiagMsg("Invalid argument? (likely caused by an unforgivable programmer error!)");

		case EACCES: // Access denied!
			return new Exception::AccessDenied(streamname);

		case EMFILE: // Too many open files!
			return &(new Exception::CannotCreateStream(streamname))->SetDiagMsg("Too many open files"); // File handle allocation failure

		case EEXIST:
			return &(new Exception::CannotCreateStream(streamname))->SetDiagMsg("File already exists");

		case ENOENT: // File not found!
			return new Exception::FileNotFound(streamname);

		case EPIPE:
			return &(new Exception::BadStream(streamname))->SetDiagMsg("Broken pipe");

		case EBADF:
			return &(new Exception::BadStream(streamname))->SetDiagMsg("Bad file number");

		default:
			return &(new Exception::BadStream(streamname))->SetDiagMsg(fmt::format("General file/stream error [errno: {}]", errcode));
	}
}
