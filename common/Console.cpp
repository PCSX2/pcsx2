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

#include "common/Threading.h"
#include "common/TraceLog.h"
#include "common/Assertions.h"
#include "common/RedtapeWindows.h" // OutputDebugString

using namespace Threading;

// thread-local console indentation setting.
static thread_local int conlog_Indent(0);

// thread-local console color storage.
static thread_local ConsoleColors conlog_Color(DefaultConsoleColor);

#ifdef __POSIX__
#include <unistd.h>

static FILE* stdout_fp = stdout;

static bool checkSupportsColor()
{
	if (!isatty(fileno(stdout_fp)))
		return false;
	char* term = getenv("TERM");
	if (!term || (0 == strcmp(term, "dumb")))
		return false;
	return true; // Probably supports color
}

static bool supports_color = checkSupportsColor();

void Console_SetStdout(FILE* fp)
{
	stdout_fp = fp;
}
#endif

// This function re-assigns the console log writer(s) to the specified target.  It makes sure
// to flush any contents from the buffered console log (which typically accumulates due to
// log suspension during log file/window re-init operations) into the new log.
//
// Important!  Only Assert and Null console loggers are allowed during C++ startup init (when
// the program or DLL first loads).  Other log targets rely on the static buffer and a
// threaded mutex lock, which are only valid after C++ initialization has finished.
void Console_SetActiveHandler(const IConsoleWriter& writer, FILE* flushfp)
{
	pxAssertDev(
		(writer.WriteRaw != NULL) && (writer.DoWriteLn != NULL) &&
			(writer.Newline != NULL) && (writer.SetTitle != NULL) &&
			(writer.DoSetColor != NULL),
		"Invalid IConsoleWriter object!  All function pointer interfaces must be implemented.");

	Console = writer;
	DevConWriter = writer;

#ifdef PCSX2_DEBUG
	DbgCon = writer;
#endif
}

// Writes text to the Visual Studio Output window (Microsoft Windows only).
// On all other platforms this pipes to Stdout instead.
static void MSW_OutputDebugString(const char* text)
{
#ifdef _WIN32
	static bool hasDebugger = IsDebuggerPresent();
	if (hasDebugger)
		OutputDebugStringA(text);
#else
	fputs(text, stdout_fp);
	fflush(stdout_fp);
#endif
}


// --------------------------------------------------------------------------------------
//  ConsoleNull
// --------------------------------------------------------------------------------------

static void ConsoleNull_SetTitle(const char* title) {}
static void ConsoleNull_DoSetColor(ConsoleColors color) {}
static void ConsoleNull_Newline() {}
static void ConsoleNull_DoWrite(const char* fmt) {}
static void ConsoleNull_DoWriteLn(const char* fmt) {}

const IConsoleWriter ConsoleWriter_Null =
	{
		ConsoleNull_DoWrite,
		ConsoleNull_DoWriteLn,
		ConsoleNull_DoSetColor,

		ConsoleNull_DoWrite,
		ConsoleNull_Newline,
		ConsoleNull_SetTitle,

		0, // instance-level indentation (should always be 0)
};

// --------------------------------------------------------------------------------------
//  Console_Stdout
// --------------------------------------------------------------------------------------

#if defined(__POSIX__)
static __fi const char* GetLinuxConsoleColor(ConsoleColors color)
{
	switch (color)
	{
		case Color_Black:
		case Color_StrongBlack:
			return "\033[30m\033[1m";

		case Color_Red:
			return "\033[31m";
		case Color_StrongRed:
			return "\033[31m\033[1m";

		case Color_Green:
			return "\033[32m";
		case Color_StrongGreen:
			return "\033[32m\033[1m";

		case Color_Yellow:
			return "\033[33m";
		case Color_StrongYellow:
			return "\033[33m\033[1m";

		case Color_Blue:
			return "\033[34m";
		case Color_StrongBlue:
			return "\033[34m\033[1m";

		// No orange, so use magenta.
		case Color_Orange:
		case Color_Magenta:
			return "\033[35m";
		case Color_StrongOrange:
		case Color_StrongMagenta:
			return "\033[35m\033[1m";

		case Color_Cyan:
			return "\033[36m";
		case Color_StrongCyan:
			return "\033[36m\033[1m";

		// Use 'white' instead of grey.
		case Color_Gray:
		case Color_White:
			return "\033[37m";
		case Color_StrongGray:
		case Color_StrongWhite:
			return "\033[37m\033[1m";

		// On some other value being passed, clear any formatting.
		case Color_Default:
		default:
			return "\033[0m";
	}
}
#endif

// One possible default write action at startup and shutdown is to use the stdout.
static void ConsoleStdout_DoWrite(const char* fmt)
{
	MSW_OutputDebugString(fmt);
}

// Default write action at startup and shutdown is to use the stdout.
static void ConsoleStdout_DoWriteLn(const char* fmt)
{
	MSW_OutputDebugString(fmt);
	MSW_OutputDebugString("\n");
}

static void ConsoleStdout_Newline()
{
	MSW_OutputDebugString("\n");
}

static void ConsoleStdout_DoSetColor(ConsoleColors color)
{
#if defined(__POSIX__)
	if (!supports_color)
		return;
	fprintf(stdout_fp, "\033[0m%s", GetLinuxConsoleColor(color));
	fflush(stdout_fp);
#endif
}

static void ConsoleStdout_SetTitle(const char* title)
{
#if defined(__POSIX__)
	if (supports_color)
		fputs("\033]0;", stdout_fp);
	fputs(title, stdout_fp);
	if (supports_color)
		fputs("\007", stdout_fp);
#endif
}

const IConsoleWriter ConsoleWriter_Stdout =
	{
		ConsoleStdout_DoWrite, // Writes without newlines go to buffer to avoid error log spam.
		ConsoleStdout_DoWriteLn,
		ConsoleStdout_DoSetColor,

		ConsoleNull_DoWrite, // writes from re-piped stdout are ignored here, lest we create infinite loop hell >_<
		ConsoleStdout_Newline,
		ConsoleStdout_SetTitle,
		0, // instance-level indentation (should always be 0)
};

// --------------------------------------------------------------------------------------
//  ConsoleAssert
// --------------------------------------------------------------------------------------

static void ConsoleAssert_DoWrite(const char* fmt)
{
	pxFailRel("Console class has not been initialized");
}

static void ConsoleAssert_DoWriteLn(const char* fmt)
{
	pxFailRel("Console class has not been initialized");
}

const IConsoleWriter ConsoleWriter_Assert =
	{
		ConsoleAssert_DoWrite,
		ConsoleAssert_DoWriteLn,
		ConsoleNull_DoSetColor,

		ConsoleNull_DoWrite,
		ConsoleNull_Newline,
		ConsoleNull_SetTitle,

		0, // instance-level indentation (should always be 0)
};

// =====================================================================================================
//  IConsoleWriter  (implementations)
// =====================================================================================================
// (all non-virtual members that do common work and then pass the result through DoWrite
//  or DoWriteLn)

// Parameters:
//   glob_indent - this parameter is used to specify a global indentation setting.  It is used by
//      WriteLn function, but defaults to 0 for Warning and Error calls.  Local indentation always
//      applies to all writes.
std::string IConsoleWriter::_addIndentation(const std::string& src, int glob_indent = 0) const
{
	const int indent = glob_indent + _imm_indentation;

	std::string indentStr;
	for (int i = 0; i < indent; i++)
		indentStr += '\t';

	std::string result;
	result.reserve(src.length() + 16 * indent);
	result.append(indentStr);
	result.append(src);

	std::string::size_type pos = result.find('\n');
	while (pos != std::string::npos)
	{
		result.insert(pos + 1, indentStr);
		pos = result.find('\n', pos + 1);
	}

	return result;
}

// Sets the indentation to be applied to all WriteLn's.  The indentation is added to the
// primary write, and to any newlines specified within the write.  Note that this applies
// to calls to WriteLn *only* -- calls to Write bypass the indentation parser.
const IConsoleWriter& IConsoleWriter::SetIndent(int tabcount) const
{
	conlog_Indent += tabcount;
	pxAssert(conlog_Indent >= 0);
	return *this;
}

IConsoleWriter IConsoleWriter::Indent(int tabcount) const
{
	IConsoleWriter retval = *this;
	retval._imm_indentation = tabcount;
	return retval;
}

// Changes the active console color.
// This color will be unset by calls to colored text methods
// such as ErrorMsg and Notice.
const IConsoleWriter& IConsoleWriter::SetColor(ConsoleColors color) const
{
	// Ignore current color requests since, well, the current color is already set. ;)
	if (color == Color_Current)
		return *this;

	pxAssertMsg((color > Color_Current) && (color < ConsoleColors_Count), "Invalid ConsoleColor specified.");

	if (conlog_Color != color)
		DoSetColor(conlog_Color = color);

	return *this;
}

ConsoleColors IConsoleWriter::GetColor() const
{
	return conlog_Color;
}

// Restores the console color to default (usually black, or low-intensity white if the console uses a black background)
const IConsoleWriter& IConsoleWriter::ClearColor() const
{
	if (conlog_Color != DefaultConsoleColor)
		DoSetColor(conlog_Color = DefaultConsoleColor);

	return *this;
}

// --------------------------------------------------------------------------------------
//  ASCII/UTF8 (char*)
// --------------------------------------------------------------------------------------

bool IConsoleWriter::FormatV(const char* fmt, va_list args) const
{
	// TODO: Make this less rubbish
	if ((_imm_indentation + conlog_Indent) > 0)
	{
		DoWriteLn(_addIndentation(StringUtil::StdStringFromFormatV(fmt, args), conlog_Indent).c_str());
	}
	else
	{
		DoWriteLn(StringUtil::StdStringFromFormatV(fmt, args).c_str());
	}

	return false;
}

bool IConsoleWriter::WriteLn(const char* fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	FormatV(fmt, args);
	va_end(args);

	return false;
}

bool IConsoleWriter::WriteLn(ConsoleColors color, const char* fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	ConsoleColorScope cs(color);
	FormatV(fmt, args);
	va_end(args);

	return false;
}

bool IConsoleWriter::Error(const char* fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	ConsoleColorScope cs(Color_StrongRed);
	FormatV(fmt, args);
	va_end(args);

	return false;
}

bool IConsoleWriter::Warning(const char* fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	ConsoleColorScope cs(Color_StrongOrange);
	FormatV(fmt, args);
	va_end(args);

	return false;
}

bool IConsoleWriter::WriteLn(ConsoleColors color, const std::string& str) const
{
	ConsoleColorScope cs(color);
	return WriteLn(str);
}

bool IConsoleWriter::WriteLn(const std::string& str) const
{
	// TODO: Make this less rubbish
	if ((_imm_indentation + conlog_Indent) > 0)
	{
		DoWriteLn(_addIndentation(str, conlog_Indent).c_str());
	}
	else
	{
		DoWriteLn(str.c_str());
	}

	return false;
}

bool IConsoleWriter::Error(const std::string& str) const
{
	return WriteLn(Color_StrongRed, str);
}

bool IConsoleWriter::Warning(const std::string& str) const
{
	return WriteLn(Color_StrongOrange, str);
}

// --------------------------------------------------------------------------------------
//  ConsoleColorScope / ConsoleIndentScope
// --------------------------------------------------------------------------------------

ConsoleColorScope::ConsoleColorScope(ConsoleColors newcolor)
{
	m_IsScoped = false;
	m_newcolor = newcolor;
	EnterScope();
}

ConsoleColorScope::~ConsoleColorScope()
{
	LeaveScope();
}

void ConsoleColorScope::EnterScope()
{
	if (!m_IsScoped)
	{
		m_old_color = Console.GetColor();
		Console.SetColor(m_newcolor);
		m_IsScoped = true;
	}
}

void ConsoleColorScope::LeaveScope()
{
	m_IsScoped = m_IsScoped && (Console.SetColor(m_old_color), false);
}

ConsoleIndentScope::ConsoleIndentScope(int tabs)
{
	m_IsScoped = false;
	m_amount = tabs;
	EnterScope();
}

ConsoleIndentScope::~ConsoleIndentScope()
{
	LeaveScope();
}

void ConsoleIndentScope::EnterScope()
{
	m_IsScoped = m_IsScoped || (Console.SetIndent(m_amount), true);
}

void ConsoleIndentScope::LeaveScope()
{
	m_IsScoped = m_IsScoped && (Console.SetIndent(-m_amount), false);
}


ConsoleAttrScope::ConsoleAttrScope(ConsoleColors newcolor, int indent)
{
	m_old_color = Console.GetColor();
	Console.SetIndent(m_tabsize = indent);
	Console.SetColor(newcolor);
}

ConsoleAttrScope::~ConsoleAttrScope()
{
	Console.SetColor(m_old_color);
	Console.SetIndent(-m_tabsize);
}


// --------------------------------------------------------------------------------------
//  Default Writer for C++ init / startup:
// --------------------------------------------------------------------------------------
// Currently all build types default to Stdout, which is very functional on Linux but not
// always so useful on Windows (which itself lacks a proper stdout console without using
// platform specific code).  Under windows Stdout will attempt to write to the IDE Debug
// console, if one is available (such as running pcsx2 via MSVC).  If not available, then
// the log message will pretty much be lost into the ether.
//
#define _DefaultWriter_ ConsoleWriter_Stdout

IConsoleWriter Console = _DefaultWriter_;
IConsoleWriter DevConWriter = _DefaultWriter_;
bool DevConWriterEnabled = false;

#ifdef PCSX2_DEBUG
IConsoleWriter DbgConWriter = _DefaultWriter_;
#endif

NullConsoleWriter NullCon = {};

// --------------------------------------------------------------------------------------
//  ConsoleLogSource  (implementations)
// --------------------------------------------------------------------------------------

// Writes to the console using the specified color.  This overrides the default color setting
// for this log.
bool ConsoleLogSource::WriteV(ConsoleColors color, const char* fmt, va_list list) const
{
	ConsoleColorScope cs(color);
	Console.WriteLn(StringUtil::StdStringFromFormatV(fmt, list));
	return false;
}

// Writes to the console using the source's default color.  Note that the source's default
// color will always be used, thus ConsoleColorScope() will not be effectual unless the
// console's default color is Color_Default.
bool ConsoleLogSource::WriteV(const char* fmt, va_list list) const
{
	WriteV(DefaultColor, fmt, list);
	return false;
}
