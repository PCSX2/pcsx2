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

#include <memory>
#include <stdexcept>
#include "common/Assertions.h"
#include "common/Pcsx2Defs.h"

// Because wxTrap isn't available on Linux builds of wxWidgets (non-Debug, typically)
void pxTrap();

// --------------------------------------------------------------------------------------
//  DESTRUCTOR_CATCHALL - safe destructor helper
// --------------------------------------------------------------------------------------
// In C++ destructors *really* need to be "nothrow" garaunteed, otherwise you can have
// disasterous nested exception throws during the unwinding process of an originating
// exception.  Use this macro to dispose of these dangerous exceptions, and generate a
// friendly error log in their wake.
//
// Note: Console can also fire an Exception::OutOfMemory
#define __DESTRUCTOR_CATCHALL(funcname) \
	catch (BaseException & ex) \
	{ \
		try \
		{ \
			Console.Error("Unhandled BaseException in %s (ignored!):", funcname); \
			Console.Error(ex.FormatDiagnosticMessage()); \
		} \
		catch (...) \
		{ \
			fprintf(stderr, "ERROR: (out of memory?)\n"); \
		} \
	} \
	catch (std::exception & ex) \
	{ \
		try \
		{ \
			Console.Error("Unhandled std::exception in %s (ignored!):", funcname); \
			Console.Error(ex.what()); \
		} \
		catch (...) \
		{ \
			fprintf(stderr, "ERROR: (out of memory?)\n"); \
		} \
	} \
	catch (...) \
	{ \
		/* Unreachable code */ \
	}

#define DESTRUCTOR_CATCHALL __DESTRUCTOR_CATCHALL(__pxFUNCTION__)

namespace Exception
{
	class BaseException;

	int MakeNewType();
	BaseException* FromErrno(std::string streamname, int errcode);

	// --------------------------------------------------------------------------------------
	//  BaseException
	// --------------------------------------------------------------------------------------
	// std::exception sucks, and isn't entirely cross-platform reliable in its implementation,
	// so I made a replacement.  The internal messages are non-const, which means that a
	// catch clause can optionally modify them and then re-throw to a top-level handler.
	//
	// Note, this class is "abstract" which means you shouldn't use it directly like, ever.
	// Use Exception::RuntimeError instead for generic exceptions.
	//
	// Because exceptions are the (only!) really useful example of multiple inheritance,
	// this class has only a trivial constructor, and must be manually initialized using
	// InitBaseEx() or by individual member assignments.  This is because C++ multiple inheritence
	// is, by design, a lot of fail, especially when class initializers are mixed in.
	//
	// [TODO] : Add an InnerException component, and Clone() facility.
	//
	class BaseException
	{
	protected:
		std::string m_message_diag; // (untranslated) a "detailed" message of what disastrous thing has occurred!
		std::string m_message_user; // (translated) a "detailed" message of what disastrous thing has occurred!

	public:
		virtual ~BaseException() = default;

		const std::string& DiagMsg() const { return m_message_diag; }
		const std::string& UserMsg() const { return m_message_user; }

		std::string& DiagMsg() { return m_message_diag; }
		std::string& UserMsg() { return m_message_user; }

		BaseException& SetBothMsgs(const char* msg_diag);
		BaseException& SetDiagMsg(std::string msg_diag);
		BaseException& SetUserMsg(std::string msg_user);

		// Returns a message suitable for diagnostic / logging purposes.
		// This message is always in English, and includes a full stack trace.
		virtual std::string FormatDiagnosticMessage() const;

		// Returns a message suitable for end-user display.
		// This message is usually meant for display in a user popup or such.
		virtual std::string FormatDisplayMessage() const;

		virtual void Rethrow() const = 0;
		virtual BaseException* Clone() const = 0;
	};

	typedef std::unique_ptr<BaseException> ScopedExcept;

	// --------------------------------------------------------------------------------------
	//  Ps2Generic Exception
	// --------------------------------------------------------------------------------------
	// This class is used as a base exception for things tossed by PS2 cpus (EE, IOP, etc).
	//
	// Implementation note: does not derive from BaseException, so that we can use different
	// catch block hierarchies to handle them (if needed).
	//
	// Translation Note: Currently these exceptions are never translated.  English/diagnostic
	// format only. :)
	//
	class Ps2Generic
	{
	protected:
		std::string m_message; // a "detailed" message of what disastrous thing has occurred!

	public:
		virtual ~Ps2Generic() = default;

		virtual u32 GetPc() const = 0;
		virtual bool IsDelaySlot() const = 0;
		virtual std::string& Message() { return m_message; }

		virtual void Rethrow() const = 0;
		virtual Ps2Generic* Clone() const = 0;
	};

// Some helper macros for defining the standard constructors of internationalized constructors
// Parameters:
//  classname - Yeah, the name of this class being defined. :)
//
//  defmsg - default message (in english), which will be used for both english and i18n messages.
//     The text string will be passed through the translator, so if it's int he gettext database
//     it will be optionally translated.
//
// BUGZ??  I'd rather use 'classname' on the Clone() prototype, but for some reason it generates
// ambiguity errors on virtual inheritance (it really shouldn't!).  So I have to force it to the
// BaseException base class.  Not sure if this is Stupid Standard Tricks or Stupid MSVC Tricks. --air
//
// (update: web searches indicate it's MSVC specific -- happens in 2008, not sure about 2010).
//
#define DEFINE_EXCEPTION_COPYTORS(classname, parent) \
private: \
	typedef parent _parent; \
\
public: \
	virtual ~classname() = default; \
\
	virtual void Rethrow() const override \
	{ \
		throw *this; \
	} \
\
	virtual classname* Clone() const override \
	{ \
		return new classname(*this); \
	}

#define DEFINE_EXCEPTION_MESSAGES(classname) \
public: \
	classname& SetBothMsgs(const char* msg_diag) \
	{ \
		BaseException::SetBothMsgs(msg_diag); \
		return *this; \
	} \
\
	classname& SetDiagMsg(std::string msg_diag) \
	{ \
		m_message_diag = msg_diag; \
		return *this; \
	} \
\
	classname& SetUserMsg(std::string msg_user) \
	{ \
		m_message_user = std::move(msg_user); \
		return *this; \
	}

#define DEFINE_RUNTIME_EXCEPTION(classname, parent, message) \
	DEFINE_EXCEPTION_COPYTORS(classname, parent) \
	classname() \
	{ \
		SetDiagMsg(message); \
	} \
	DEFINE_EXCEPTION_MESSAGES(classname)


	// ---------------------------------------------------------------------------------------
	//  RuntimeError - Generalized Exceptions with Recoverable Traits!
	// ---------------------------------------------------------------------------------------

	class RuntimeError : public BaseException
	{
		DEFINE_EXCEPTION_COPYTORS(RuntimeError, BaseException)
		DEFINE_EXCEPTION_MESSAGES(RuntimeError)

	public:
		bool IsSilent;

		RuntimeError() { IsSilent = false; }
		RuntimeError(const std::runtime_error& ex, const char* prefix = nullptr);
		RuntimeError(const std::exception& ex, const char* prefix = nullptr);
	};

	// --------------------------------------------------------------------------------------
	//  CancelAppEvent  -  Exception for canceling an event in a non-verbose fashion
	// --------------------------------------------------------------------------------------
	// Typically the PCSX2 interface issues popup dialogs for runtime errors.  This exception
	// instead issues a "silent" cancelation that is handled by the app gracefully (generates
	// log, and resumes messages queue processing).
	//
	// I chose to have this exception derive from RuntimeError, since if one is thrown from outside
	// an App message loop we'll still want it to be handled in a reasonably graceful manner.
	class CancelEvent : public RuntimeError
	{
		DEFINE_RUNTIME_EXCEPTION(CancelEvent, RuntimeError, "No reason given.")

	public:
		explicit CancelEvent(std::string logmsg)
		{
			m_message_diag = std::move(logmsg);
			// overridden message formatters only use the diagnostic version...
		}

		virtual std::string FormatDisplayMessage() const override;
		virtual std::string FormatDiagnosticMessage() const override;
	};

	// ---------------------------------------------------------------------------------------
	//  OutOfMemory
	// ---------------------------------------------------------------------------------------
	// This exception has a custom-formatted Diagnostic string.  The parameter give when constructing
	// the exception is a block/alloc name, which is used as a formatting parameter in the diagnostic
	// output.  The default diagnostic message is "Out of memory exception, while allocating the %s."
	// where %s is filled in with the block name.
	//
	// The user string is not custom-formatted, and should contain *NO* %s tags.
	//
	class OutOfMemory : public RuntimeError
	{
		DEFINE_RUNTIME_EXCEPTION(OutOfMemory, RuntimeError, "")

	public:
		std::string AllocDescription;

	public:
		OutOfMemory(std::string allocdesc);

		virtual std::string FormatDisplayMessage() const override;
		virtual std::string FormatDiagnosticMessage() const override;
	};

	class ParseError : public RuntimeError
	{
		DEFINE_RUNTIME_EXCEPTION(ParseError, RuntimeError, "Parse error");
	};

	// ---------------------------------------------------------------------------------------
	// Hardware/OS Exceptions:
	//   HardwareDeficiency / VirtualMemoryMapConflict
	// ---------------------------------------------------------------------------------------

	// This exception is a specific type of OutOfMemory error that isn't "really" an out of
	// memory error.  More likely it's caused by a plugin or driver reserving a range of memory
	// we'd really like to have access to.
	class VirtualMemoryMapConflict : public OutOfMemory
	{
		DEFINE_RUNTIME_EXCEPTION(VirtualMemoryMapConflict, OutOfMemory, "")

		VirtualMemoryMapConflict(std::string allocdesc);

		virtual std::string FormatDisplayMessage() const override;
		virtual std::string FormatDiagnosticMessage() const override;
	};

	class HardwareDeficiency : public RuntimeError
	{
	public:
		DEFINE_RUNTIME_EXCEPTION(HardwareDeficiency, RuntimeError, "Your machine's hardware is incapable of running PCSX2.  Sorry dood.");
	};

	// ---------------------------------------------------------------------------------------
	// Streaming (file) Exceptions:
	//   Stream / BadStream / CannotCreateStream / FileNotFound / AccessDenied / EndOfStream
	// ---------------------------------------------------------------------------------------

#define DEFINE_STREAM_EXCEPTION(classname, parent) \
	DEFINE_RUNTIME_EXCEPTION(classname, parent, "") \
	classname(std::string filename) \
	{ \
		StreamName = filename; \
	} \
	virtual classname& SetStreamName(std::string name) override \
	{ \
		StreamName = std::move(name); \
		return *this; \
	} \
\
	virtual classname& SetStreamName(const char* name) override \
	{ \
		StreamName = name; \
		return *this; \
	}

	// A generic base error class for bad streams -- corrupted data, sudden closures, loss of
	// connection, or anything else that would indicate a failure to open a stream or read the
	// data after the stream was successfully opened.
	//
	class BadStream : public RuntimeError
	{
		DEFINE_RUNTIME_EXCEPTION(BadStream, RuntimeError, "")

	public:
		BadStream(std::string filename)
			: StreamName(std::move(filename))
		{
		}
		virtual BadStream& SetStreamName(std::string name)
		{
			StreamName = std::move(name);
			return *this;
		}
		virtual BadStream& SetStreamName(const char* name)
		{
			StreamName = name;
			return *this;
		}

		std::string StreamName; // name of the stream (if applicable)

		virtual std::string FormatDiagnosticMessage() const override;
		virtual std::string FormatDisplayMessage() const override;

	protected:
		void _formatDiagMsg(std::string& dest) const;
		void _formatUserMsg(std::string& dest) const;
	};

	// A generic exception for odd-ball stream creation errors.
	//
	class CannotCreateStream : public BadStream
	{
		DEFINE_STREAM_EXCEPTION(CannotCreateStream, BadStream)

		virtual std::string FormatDiagnosticMessage() const override;
		virtual std::string FormatDisplayMessage() const override;
	};

	// Exception thrown when an attempt to open a non-existent file is made.
	// (this exception can also mean file permissions are invalid)
	//
	class FileNotFound : public CannotCreateStream
	{
	public:
		DEFINE_STREAM_EXCEPTION(FileNotFound, CannotCreateStream)

		virtual std::string FormatDiagnosticMessage() const override;
		virtual std::string FormatDisplayMessage() const override;
	};

	class AccessDenied : public CannotCreateStream
	{
	public:
		DEFINE_STREAM_EXCEPTION(AccessDenied, CannotCreateStream)

		virtual std::string FormatDiagnosticMessage() const override;
		virtual std::string FormatDisplayMessage() const override;
	};

	// EndOfStream can be used either as an error, or used just as a shortcut for manual
	// feof checks.
	//
	class EndOfStream : public BadStream
	{
	public:
		DEFINE_STREAM_EXCEPTION(EndOfStream, BadStream)

		virtual std::string FormatDiagnosticMessage() const override;
		virtual std::string FormatDisplayMessage() const override;
	};

#ifdef _WIN32
	// --------------------------------------------------------------------------------------
	//  Exception::WinApiError
	// --------------------------------------------------------------------------------------
	class WinApiError : public RuntimeError
	{
		DEFINE_EXCEPTION_COPYTORS(WinApiError, RuntimeError)
		DEFINE_EXCEPTION_MESSAGES(WinApiError)

	public:
		int ErrorId;

	public:
		WinApiError();

		std::string GetMsgFromWindows() const;
		virtual std::string FormatDisplayMessage() const override;
		virtual std::string FormatDiagnosticMessage() const override;
	};
#endif
} // namespace Exception

using Exception::BaseException;
using Exception::ScopedExcept;
