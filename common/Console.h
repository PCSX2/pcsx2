// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Pcsx2Defs.h"

#include "fmt/core.h"

#include <cstdarg>
#include <string>

// TODO: This whole thing needs to get ripped out.

enum ConsoleColors
{
	Color_Default = 0,

	Color_Black,
	Color_Green,
	Color_Red,
	Color_Blue,
	Color_Magenta,
	Color_Orange,
	Color_Gray,

	Color_Cyan, // faint visibility, intended for logging PS2/IOP output
	Color_Yellow, // faint visibility, intended for logging PS2/IOP output
	Color_White, // faint visibility, intended for logging PS2/IOP output

	// Strong text *may* result in mis-aligned text in the console, depending on the
	// font and the platform, so use these with caution.
	Color_StrongBlack,
	Color_StrongRed, // intended for errors
	Color_StrongGreen, // intended for infrequent state information
	Color_StrongBlue, // intended for block headings
	Color_StrongMagenta,
	Color_StrongOrange, // intended for warnings
	Color_StrongGray,

	Color_StrongCyan,
	Color_StrongYellow,
	Color_StrongWhite,

	ConsoleColors_Count
};

enum LOGLEVEL
{
	LOGLEVEL_NONE, // Silences all log traffic
	LOGLEVEL_ERROR,
	LOGLEVEL_WARNING,
	LOGLEVEL_INFO,
	LOGLEVEL_DEV,
	LOGLEVEL_DEBUG,
	LOGLEVEL_TRACE,

	LOGLEVEL_COUNT,
};

// TODO: Move this elsewhere, add channels.

namespace Log
{
	// log message callback type
	using HostCallbackType = void (*)(LOGLEVEL level, ConsoleColors color, std::string_view message);

	// returns the time in seconds since the start of the process
	float GetCurrentMessageTime();

	// adds a standard console output
	bool IsConsoleOutputEnabled();
	void SetConsoleOutputLevel(LOGLEVEL level);

	// adds a debug console output
	bool IsDebugOutputAvailable();
	bool IsDebugOutputEnabled();
	void SetDebugOutputLevel(LOGLEVEL level);

	// adds a file output
	bool IsFileOutputEnabled();
	bool SetFileOutputLevel(LOGLEVEL level, std::string path);

	// returns the log file, this is really dangerous to use if it changes...
	std::FILE* GetFileLogHandle();

	// adds host output
	bool IsHostOutputEnabled();
	void SetHostOutputLevel(LOGLEVEL level, HostCallbackType callback);

	// sets logging timestamps
	bool AreTimestampsEnabled();
	void SetTimestampsEnabled(bool enabled);

	// Returns the current global filtering level.
	LOGLEVEL GetMaxLevel();

	// writes a message to the log
	void Write(LOGLEVEL level, ConsoleColors color, std::string_view message);
	void Writef(LOGLEVEL level, ConsoleColors color, const char* format, ...);
	void Writev(LOGLEVEL level, ConsoleColors color, const char* format, va_list ap);
	void WriteFmtArgs(LOGLEVEL level, ConsoleColors color, fmt::string_view fmt, fmt::format_args args);

	template <typename... T>
	__fi static void Write(LOGLEVEL level, ConsoleColors color, fmt::format_string<T...> fmt, T&&... args)
	{
		// Avoid arg packing if filtered.
		if (level <= GetMaxLevel())
			return WriteFmtArgs(level, color, fmt, fmt::make_format_args(args...));
	}
} // namespace Log

// Adapter classes to handle old code.
template <LOGLEVEL level>
struct ConsoleLogWriter
{
	__fi static void Error(std::string_view str) { Log::Write(level, Color_StrongRed, str); }
	__fi static void Warning(std::string_view str) { Log::Write(level, Color_StrongOrange, str); }
	__fi static void WriteLn(std::string_view str) { Log::Write(level, Color_Default, str); }
	__fi static void WriteLn(ConsoleColors color, std::string_view str) { Log::Write(level, color, str); }
	__fi static void WriteLn() { Log::Write(level, Color_Default, std::string_view()); }
	__fi static void FormatV(const char* format, va_list ap) { Log::Writev(level, Color_Default, format, ap); }
	__fi static void FormatV(ConsoleColors color, const char* format, va_list ap) { Log::Writev(level, color, format, ap); }

#define MAKE_PRINTF_CONSOLE_WRITER(color) \
	do \
	{ \
		std::va_list ap; \
		va_start(ap, format); \
		Log::Writev(level, color, format, ap); \
		va_end(ap); \
	} while (0)

	// clang-format off
	static void Error(const char* format, ...) { MAKE_PRINTF_CONSOLE_WRITER(Color_StrongRed); }
	static void Warning(const char* format, ...) { MAKE_PRINTF_CONSOLE_WRITER(Color_StrongOrange); }
	static void WriteLn(const char* format, ...) { MAKE_PRINTF_CONSOLE_WRITER(Color_Default); }
	static void WriteLn(ConsoleColors color, const char* format, ...) { MAKE_PRINTF_CONSOLE_WRITER(color); }
	// clang-format on	

#undef MAKE_PRINTF_CONSOLE_WRITER

#define MAKE_FMT_CONSOLE_WRITER(color) do \
	{ \
		if (level <= Log::GetMaxLevel()) \
			Log::WriteFmtArgs(level, color, fmt, fmt::make_format_args(args...)); \
} \
	while (0)

	// clang-format off
	template<typename... T> __fi static void ErrorFmt(fmt::format_string<T...> fmt, T&&... args) { MAKE_FMT_CONSOLE_WRITER(Color_StrongRed); }
	template<typename... T> __fi static void WarningFmt(fmt::format_string<T...> fmt, T&&... args) { MAKE_FMT_CONSOLE_WRITER(Color_StrongOrange); }
	template<typename... T> __fi static void WriteLnFmt(fmt::format_string<T...> fmt, T&&... args) { MAKE_FMT_CONSOLE_WRITER(Color_Default); }
	template<typename... T> __fi static void WriteLnFmt(ConsoleColors color, fmt::format_string<T...> fmt, T&&... args) { MAKE_FMT_CONSOLE_WRITER(color); }
	// clang-format on

#undef MAKE_FMT_CONSOLE_WRITER
};

struct NullLogWriter
{
	// clang-format off
	__fi static bool Error(std::string_view str) { return false; }
	__fi static bool Warning(std::string_view str) { return false; }
	__fi static bool WriteLn(std::string_view str) { return false; }
	__fi static bool WriteLn(ConsoleColors color, std::string_view str) { return false; }
	__fi static bool WriteLn() { return false; }

	__fi static bool Error(const char* format, ...) { return false; }
	__fi static bool Warning(const char* format, ...) { return false; }
	__fi static bool WriteLn(const char* format, ...) { return false; }
	__fi static bool WriteLn(ConsoleColors color, const char* format, ...) { return false; }

	template<typename... T> __fi static bool ErrorFmt(fmt::format_string<T...> fmt, T&&... args) { return false; }
	template<typename... T> __fi static bool WarningFmt(fmt::format_string<T...> fmt, T&&... args) { return false; }
	template<typename... T> __fi static bool WriteLnFmt(fmt::format_string<T...> fmt, T&&... args) { return false; }
	template<typename... T> __fi static bool WriteLnFmt(ConsoleColors color, fmt::format_string<T...> fmt, T&&... args) { return false; }
	// clang-format on	
};

extern ConsoleLogWriter<LOGLEVEL_INFO> Console;
extern ConsoleLogWriter<LOGLEVEL_DEV> DevCon;

#define ERROR_LOG(...) Log::Write(LOGLEVEL_ERROR, Color_StrongRed, __VA_ARGS__)
#define WARNING_LOG(...) Log::Write(LOGLEVEL_WARNING, Color_StrongOrange, __VA_ARGS__)
#define INFO_LOG(...) Log::Write(LOGLEVEL_INFO, Color_White, __VA_ARGS__)
#define DEV_LOG(...) Log::Write(LOGLEVEL_DEV, Color_StrongGray, __VA_ARGS__)

#ifdef _DEBUG
extern ConsoleLogWriter<LOGLEVEL_DEBUG> DbgConWriter;
#define DbgCon DbgConWriter
#define DEBUG_LOG(...) Log::Write(LOGLEVEL_TRACE, Color_Gray, __VA_ARGS__)
#define TRACE_LOG(...) Log::Write(LOGLEVEL_TRACE, Color_Blue, __VA_ARGS__)
#else
extern NullLogWriter DbgConWriter;
#define DbgCon 0 && DbgConWriter
#define DEBUG_LOG(...) (void)0
#define TRACE_LOG(...) (void)0
#endif
