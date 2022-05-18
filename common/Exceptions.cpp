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

#include "fmt/core.h"

#ifdef _WIN32
#include "RedtapeWindows.h"
#endif

#ifdef __UNIX__
#include <signal.h>
#endif

// ------------------------------------------------------------------------
// Force DevAssert to *not* inline for devel builds (allows using breakpoints to trap assertions,
// and force it to inline for release builds (optimizes it out completely since IsDevBuild is a
// const false).
//
#ifdef PCSX2_DEVBUILD
#define DEVASSERT_INLINE __noinline
#else
#define DEVASSERT_INLINE __fi
#endif

pxDoAssertFnType* pxDoAssert = pxAssertImpl_LogIt;

// make life easier for people using VC++ IDE by using this format, which allows double-click
// response times from the Output window...
std::string DiagnosticOrigin::ToString(const char* msg) const
{
	std::string message;

	fmt::format_to(std::back_inserter(message), "{}({}) : assertion failed:\n", srcfile, line);

	if (function)
		fmt::format_to(std::back_inserter(message), "    Function:  {}\n", function);

	if (condition)
		fmt::format_to(std::back_inserter(message), "    Condition: {}\n", condition);

	if (msg)
		fmt::format_to(std::back_inserter(message), "    Message:   {}\n", msg);

	return message;
}


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


bool pxAssertImpl_LogIt(const DiagnosticOrigin& origin, const char* msg)
{
	//wxLogError( L"%s", origin.ToString( msg ).c_str() );
	std::string full_msg(origin.ToString(msg));
#ifdef _WIN32
	OutputDebugStringA(full_msg.c_str());
	OutputDebugStringA("\n");
#endif

	std::fprintf(stderr, "%s\n", full_msg.c_str());

	pxTrap();
	return false;
}


DEVASSERT_INLINE void pxOnAssert(const DiagnosticOrigin& origin, const char* msg)
{
	// wxWidgets doesn't come with debug builds on some Linux distros, and other distros make
	// it difficult to use the debug build (compilation failures).  To handle these I've had to
	// bypass the internal wxWidgets assertion handler entirely, since it may not exist even if
	// PCSX2 itself is compiled in debug mode (assertions enabled).

	bool trapit;

	if (pxDoAssert == NULL)
	{
		// Note: Format uses MSVC's syntax for output window hotlinking.
		trapit = pxAssertImpl_LogIt(origin, msg);
	}
	else
	{
		trapit = pxDoAssert(origin, msg);
	}

	if (trapit)
	{
		pxTrap();
	}
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
