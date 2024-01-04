// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DebugTools/Debug.h"
#include "Host.h"
#include "LogSink.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

#include <csignal>
#include "fmt/core.h"

// Used on both Windows and Linux.
#ifdef _WIN32
static const wchar_t s_console_colors[][ConsoleColors_Count] = {
#define CC(x) L##x
#else
static const char s_console_colors[][ConsoleColors_Count] = {
#define CC(x) x
#endif
	CC("\033[0m"), // default
	CC("\033[30m\033[1m"), // black
	CC("\033[32m"), // green
	CC("\033[31m"), // red
	CC("\033[34m"), // blue
	CC("\033[35m"), // magenta
	CC("\033[35m"), // orange (FIXME)
	CC("\033[37m"), // gray
	CC("\033[36m"), // cyan
	CC("\033[33m"), // yellow
	CC("\033[37m"), // white
	CC("\033[30m\033[1m"), // strong black
	CC("\033[31m\033[1m"), // strong red
	CC("\033[32m\033[1m"), // strong green
	CC("\033[34m\033[1m"), // strong blue
	CC("\033[35m\033[1m"), // strong magenta
	CC("\033[35m\033[1m"), // strong orange (FIXME)
	CC("\033[37m\033[1m"), // strong gray
	CC("\033[36m\033[1m"), // strong cyan
	CC("\033[33m\033[1m"), // strong yellow
	CC("\033[37m\033[1m") // strong white
};
#undef CC

static bool s_block_system_console = false;
static Common::Timer::Value s_log_start_timestamp = Common::Timer::GetCurrentValue();
static bool s_log_timestamps = false;
static std::mutex s_log_mutex;

// Replacement for Console so we actually get output to our console window on Windows.
#ifdef _WIN32

static bool s_debugger_attached = false;
static bool s_console_handle_set = false;
static bool s_console_allocated = false;
static HANDLE s_console_handle = INVALID_HANDLE_VALUE;
static HANDLE s_old_console_stdin = NULL;
static HANDLE s_old_console_stdout = NULL;
static HANDLE s_old_console_stderr = NULL;

static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
	Console.WriteLn("Handler %u", dwCtrlType);
	if (dwCtrlType != CTRL_C_EVENT)
		return FALSE;

	::raise(SIGTERM);
	return TRUE;
}

static bool EnableVirtualTerminalProcessing(HANDLE hConsole)
{
	if (hConsole == INVALID_HANDLE_VALUE)
		return false;

	DWORD old_mode;
	if (!GetConsoleMode(hConsole, &old_mode))
		return false;

	// already enabled?
	if (old_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
		return true;

	return SetConsoleMode(hConsole, old_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

#endif

static void ConsoleQt_SetTitle(const char* title)
{
#ifdef _WIN32
	SetConsoleTitleW(StringUtil::UTF8StringToWideString(title).c_str());
#else
	std::fprintf(stdout, "\033]0;%s\007", title);
#endif
}

static void ConsoleQt_DoSetColor(ConsoleColors color)
{
#ifdef _WIN32
	if (s_console_handle == INVALID_HANDLE_VALUE)
		return;

	const wchar_t* colortext = s_console_colors[static_cast<u32>(color)];
	DWORD written;
	WriteConsoleW(s_console_handle, colortext, std::wcslen(colortext), &written, nullptr);
#else
	const char* colortext = s_console_colors[static_cast<u32>(color)];
	std::fputs(colortext, stdout);
#endif
}

static void ConsoleQt_DoWrite(const char* fmt)
{
	std::unique_lock lock(s_log_mutex);

#ifdef _WIN32
	if (s_console_handle != INVALID_HANDLE_VALUE || s_debugger_attached)
	{
		// TODO: Put this on the stack.
		std::wstring wfmt(StringUtil::UTF8StringToWideString(fmt));

		if (s_debugger_attached)
			OutputDebugStringW(wfmt.c_str());

		if (s_console_handle != INVALID_HANDLE_VALUE)
		{
			DWORD written;
			WriteConsoleW(s_console_handle, wfmt.c_str(), static_cast<DWORD>(wfmt.length()), &written, nullptr);
		}
	}
#else
	std::fputs(fmt, stdout);
#endif

	if (emuLog)
	{
		std::fputs(fmt, emuLog);
	}
}

static void ConsoleQt_DoWriteLn(const char* fmt)
{
	std::unique_lock lock(s_log_mutex);

	// find time since start of process, but save a syscall if we're not writing timestamps
	float message_time =
		s_log_timestamps ?
			static_cast<float>(Common::Timer::ConvertValueToSeconds(Common::Timer::GetCurrentValue() - s_log_start_timestamp)) :
            0.0f;

	// split newlines up
	const char* start = fmt;
	do
	{
		const char* end = std::strchr(start, '\n');

		std::string_view line;
		if (end)
		{
			line = std::string_view(start, end - start);
			start = end + 1;
		}
		else
		{
			line = std::string_view(start);
			start = nullptr;
		}

#ifdef _WIN32
		if (s_console_handle != INVALID_HANDLE_VALUE || s_debugger_attached)
		{
			// TODO: Put this on the stack.
			std::wstring wfmt(StringUtil::UTF8StringToWideString(line));

			if (s_debugger_attached)
			{
				// VS already timestamps logs (at least with the productivity power tools).
				if (!wfmt.empty())
					OutputDebugStringW(wfmt.c_str());
				OutputDebugStringW(L"\n");
			}

			if (s_console_handle != INVALID_HANDLE_VALUE)
			{
				DWORD written;
				if (s_log_timestamps)
				{
					wchar_t timestamp_text[128];
					const int timestamp_len = _swprintf(timestamp_text, L"[%10.4f] ", message_time);
					WriteConsoleW(s_console_handle, timestamp_text, static_cast<DWORD>(timestamp_len), &written, nullptr);
				}

				if (!wfmt.empty())
					WriteConsoleW(s_console_handle, wfmt.c_str(), static_cast<DWORD>(wfmt.length()), &written, nullptr);

				WriteConsoleW(s_console_handle, L"\n", 1, &written, nullptr);
			}
		}
#else
		if (s_log_timestamps)
		{
			std::fprintf(stdout, "[%10.4f] %.*s\n", message_time, static_cast<int>(line.length()), line.data());
		}
		else
		{
			if (!line.empty())
				std::fwrite(line.data(), line.length(), 1, stdout);
			std::fputc('\n', stdout);
		}
#endif

		if (emuLog)
		{
			if (s_log_timestamps)
			{
				std::fprintf(emuLog, "[%10.4f] %.*s\n", message_time, static_cast<int>(line.length()), line.data());
			}
			else
			{
				std::fwrite(line.data(), line.length(), 1, emuLog);
				std::fputc('\n', emuLog);
			}
		}
	} while (start);
}

static void ConsoleQt_Newline()
{
	ConsoleQt_DoWriteLn("");
}

static const IConsoleWriter ConsoleWriter_WinQt = {
	ConsoleQt_DoWrite,
	ConsoleQt_DoWriteLn,
	ConsoleQt_DoSetColor,

	ConsoleQt_DoWrite,
	ConsoleQt_Newline,
	ConsoleQt_SetTitle,
};

static void UpdateLoggingSinks(bool system_console, bool file_log)
{
#ifdef _WIN32
	const bool debugger_attached = IsDebuggerPresent();
	s_debugger_attached = debugger_attached;
	if (system_console)
	{
		if (!s_console_handle_set)
		{
			s_old_console_stdin = GetStdHandle(STD_INPUT_HANDLE);
			s_old_console_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
			s_old_console_stderr = GetStdHandle(STD_ERROR_HANDLE);

			bool handle_valid = (GetConsoleWindow() != NULL);
			if (!handle_valid)
			{
				s_console_allocated = AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole();
				handle_valid = (GetConsoleWindow() != NULL);
			}

			if (handle_valid)
			{
				s_console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
				if (s_console_handle != INVALID_HANDLE_VALUE)
				{
					s_console_handle_set = true;
					SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

					// This gets us unix-style coloured output.
					EnableVirtualTerminalProcessing(GetStdHandle(STD_OUTPUT_HANDLE));
					EnableVirtualTerminalProcessing(GetStdHandle(STD_ERROR_HANDLE));

					// Redirect stdout/stderr.
					std::FILE* fp;
					freopen_s(&fp, "CONIN$", "r", stdin);
					freopen_s(&fp, "CONOUT$", "w", stdout);
					freopen_s(&fp, "CONOUT$", "w", stderr);
				}
			}
		}

		// just in case it fails
		system_console = s_console_handle_set;
	}
	else
	{
		if (s_console_handle_set)
		{
			s_console_handle_set = false;
			SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);

			// redirect stdout/stderr back to null.
			std::FILE* fp;
			freopen_s(&fp, "NUL:", "w", stderr);
			freopen_s(&fp, "NUL:", "w", stdout);
			freopen_s(&fp, "NUL:", "w", stdin);

			// release console and restore state
			SetStdHandle(STD_INPUT_HANDLE, s_old_console_stdin);
			SetStdHandle(STD_OUTPUT_HANDLE, s_old_console_stdout);
			SetStdHandle(STD_ERROR_HANDLE, s_old_console_stderr);
			s_old_console_stdin = NULL;
			s_old_console_stdout = NULL;
			s_old_console_stderr = NULL;
			if (s_console_allocated)
			{
				s_console_allocated = false;
				FreeConsole();
			}
		}
	}
#else
	const bool debugger_attached = false;
#endif

	if (file_log)
	{
		if (!emuLog)
		{
			if (emuLogName.empty())
				emuLogName = Path::Combine(EmuFolders::Logs, "emulog.txt");

			emuLog = FileSystem::OpenCFile(emuLogName.c_str(), "wb");
			file_log = (emuLog != nullptr);
		}
	}
	else
	{
		if (emuLog)
		{
			std::fclose(emuLog);
			emuLog = nullptr;
			emuLogName = {};
		}
	}

	// Discard logs completely if there's no sinks.
	if (debugger_attached || system_console || file_log)
		Console_SetActiveHandler(ConsoleWriter_WinQt);
	else
		Console_SetActiveHandler(ConsoleWriter_Null);
}

void LogSink::SetFileLogPath(std::string path)
{
	if (emuLogName == path)
		return;

	emuLogName = std::move(path);

	// reopen on change
	if (emuLog)
	{
		std::fclose(emuLog);
		if (!emuLogName.empty())
			emuLog = FileSystem::OpenCFile(emuLogName.c_str(), "wb");
	}
}

void LogSink::CloseFileLog()
{
	if (!emuLog)
		return;

	std::fclose(emuLog);
	emuLog = nullptr;
}

void LogSink::SetBlockSystemConsole(bool block)
{
	s_block_system_console = block;
}

void LogSink::InitializeEarlyConsole()
{
	UpdateLoggingSinks(true, false);
}

void LogSink::UpdateLogging(SettingsInterface& si)
{
	const bool system_console_enabled = !s_block_system_console && si.GetBoolValue("Logging", "EnableSystemConsole", false);
	const bool file_logging_enabled = si.GetBoolValue("Logging", "EnableFileLogging", false);

	s_log_timestamps = si.GetBoolValue("Logging", "EnableTimestamps", true);

	const bool any_logging_sinks = system_console_enabled || file_logging_enabled;
	DevConWriterEnabled = any_logging_sinks && (IsDevBuild || si.GetBoolValue("Logging", "EnableVerbose", false));

	const bool ee_console_enabled = any_logging_sinks && si.GetBoolValue("Logging", "EnableEEConsole", false);
	SysConsole.eeConsole.Enabled = ee_console_enabled;

	SysConsole.iopConsole.Enabled = any_logging_sinks && si.GetBoolValue("Logging", "EnableIOPConsole", false);
	SysTrace.IOP.R3000A.Enabled = true;
	SysTrace.IOP.COP2.Enabled = true;
	SysTrace.IOP.Memory.Enabled = true;
	SysTrace.SIF.Enabled = true;

	// Input Recording Logs
	SysConsole.recordingConsole.Enabled = any_logging_sinks && si.GetBoolValue("Logging", "EnableInputRecordingLogs", true);
	SysConsole.controlInfo.Enabled = any_logging_sinks && si.GetBoolValue("Logging", "EnableControllerLogs", false);

	UpdateLoggingSinks(system_console_enabled, file_logging_enabled);
}

void LogSink::SetDefaultLoggingSettings(SettingsInterface& si)
{
	si.SetBoolValue("Logging", "EnableSystemConsole", false);
	si.SetBoolValue("Logging", "EnableFileLogging", false);
	si.SetBoolValue("Logging", "EnableTimestamps", true);
	si.SetBoolValue("Logging", "EnableVerbose", false);
	si.SetBoolValue("Logging", "EnableEEConsole", false);
	si.SetBoolValue("Logging", "EnableIOPConsole", false);
	si.SetBoolValue("Logging", "EnableInputRecordingLogs", true);
	si.SetBoolValue("Logging", "EnableControllerLogs", false);
}
