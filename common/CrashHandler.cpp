/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "Pcsx2Defs.h"
#include "CrashHandler.h"
#include "FileSystem.h"
#include "StringUtil.h"
#include <cinttypes>
#include <cstdio>
#include <ctime>

#if defined(_WIN32) && !defined(_UWP)
#include "RedtapeWindows.h"

#include "StackWalker.h"
#include <DbgHelp.h>

class CrashHandlerStackWalker : public StackWalker
{
public:
	explicit CrashHandlerStackWalker(HANDLE out_file);
	~CrashHandlerStackWalker();

protected:
	void OnOutput(LPCSTR szText) override;

private:
	HANDLE m_out_file;
};

CrashHandlerStackWalker::CrashHandlerStackWalker(HANDLE out_file)
	: StackWalker(RetrieveVerbose, nullptr, GetCurrentProcessId(), GetCurrentProcess())
	, m_out_file(out_file)
{
}

CrashHandlerStackWalker::~CrashHandlerStackWalker() = default;

void CrashHandlerStackWalker::OnOutput(LPCSTR szText)
{
	if (m_out_file)
	{
		DWORD written;
		WriteFile(m_out_file, szText, static_cast<DWORD>(std::strlen(szText)), &written, nullptr);
	}

	OutputDebugStringA(szText);
}

static bool WriteMinidump(HMODULE hDbgHelp, HANDLE hFile, HANDLE hProcess, DWORD process_id, DWORD thread_id,
	PEXCEPTION_POINTERS exception, MINIDUMP_TYPE type)
{
	using PFNMINIDUMPWRITEDUMP =
		BOOL(WINAPI*)(HANDLE hProcess, DWORD ProcessId, HANDLE hFile, MINIDUMP_TYPE DumpType,
			PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
			PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

	PFNMINIDUMPWRITEDUMP minidump_write_dump = hDbgHelp ?
												   reinterpret_cast<PFNMINIDUMPWRITEDUMP>(GetProcAddress(hDbgHelp, "MiniDumpWriteDump")) :
                                                   nullptr;
	if (!minidump_write_dump)
		return false;

	MINIDUMP_EXCEPTION_INFORMATION mei;
	PMINIDUMP_EXCEPTION_INFORMATION mei_ptr = nullptr;
	if (exception)
	{
		mei.ThreadId = thread_id;
		mei.ExceptionPointers = exception;
		mei.ClientPointers = FALSE;
		mei_ptr = &mei;
	}

	return minidump_write_dump(hProcess, process_id, hFile, type, mei_ptr, nullptr, nullptr);
}

static std::wstring s_write_directory;
static HMODULE s_dbghelp_module = nullptr;
static PVOID s_veh_handle = nullptr;
static bool s_in_crash_handler = false;

static void GenerateCrashFilename(wchar_t* buf, size_t len, const wchar_t* prefix, const wchar_t* extension)
{
	SYSTEMTIME st = {};
	GetLocalTime(&st);

	_snwprintf_s(buf, len, _TRUNCATE, L"%s%scrash-%04u-%02u-%02u-%02u-%02u-%02u-%03u.%s",
		prefix ? prefix : L"", prefix ? L"\\" : L"",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, extension);
}

static void WriteMinidumpAndCallstack(PEXCEPTION_POINTERS exi)
{
	s_in_crash_handler = true;

	wchar_t filename[1024] = {};
	GenerateCrashFilename(filename, std::size(filename), s_write_directory.empty() ? nullptr : s_write_directory.c_str(), L"txt");

	// might fail
	HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (exi && hFile != INVALID_HANDLE_VALUE)
	{
		char line[1024];
		DWORD written;
		std::snprintf(line, std::size(line), "Exception 0x%08X at 0x%p\n", exi->ExceptionRecord->ExceptionCode,
			exi->ExceptionRecord->ExceptionAddress);
		WriteFile(hFile, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
	}

	GenerateCrashFilename(filename, std::size(filename), s_write_directory.empty() ? nullptr : s_write_directory.c_str(), L"dmp");

	const MINIDUMP_TYPE minidump_type =
		static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithHandleData | MiniDumpWithProcessThreadData |
								   MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory);
	const HANDLE hMinidumpFile = CreateFileW(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (hMinidumpFile == INVALID_HANDLE_VALUE ||
		!WriteMinidump(s_dbghelp_module, hMinidumpFile, GetCurrentProcess(), GetCurrentProcessId(),
			GetCurrentThreadId(), exi, minidump_type))
	{
		static const char error_message[] = "Failed to write minidump file.\n";
		if (hFile != INVALID_HANDLE_VALUE)
		{
			DWORD written;
			WriteFile(hFile, error_message, sizeof(error_message) - 1, &written, nullptr);
		}
	}
	if (hMinidumpFile != INVALID_HANDLE_VALUE)
		CloseHandle(hMinidumpFile);

	CrashHandlerStackWalker sw(hFile);
	sw.ShowCallstack(GetCurrentThread(), exi ? exi->ContextRecord : nullptr);

	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);
}

static LONG NTAPI ExceptionHandler(PEXCEPTION_POINTERS exi)
{
	if (s_in_crash_handler)
		return EXCEPTION_CONTINUE_SEARCH;

	switch (exi->ExceptionRecord->ExceptionCode)
	{
		case EXCEPTION_ACCESS_VIOLATION:
		case EXCEPTION_BREAKPOINT:
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
		case EXCEPTION_INT_OVERFLOW:
		case EXCEPTION_PRIV_INSTRUCTION:
		case EXCEPTION_ILLEGAL_INSTRUCTION:
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		case EXCEPTION_STACK_OVERFLOW:
		case EXCEPTION_GUARD_PAGE:
			break;

		default:
			return EXCEPTION_CONTINUE_SEARCH;
	}

	// if the debugger is attached, let it take care of it.
	if (IsDebuggerPresent())
		return EXCEPTION_CONTINUE_SEARCH;

	WriteMinidumpAndCallstack(exi);
	return EXCEPTION_CONTINUE_SEARCH;
}

bool CrashHandler::Install()
{
	// load dbghelp at install/startup, that way we're not LoadLibrary()'ing after a crash
	// .. because that probably wouldn't go down well.
	s_dbghelp_module = StackWalker::LoadDbgHelpLibrary();

	s_veh_handle = AddVectoredExceptionHandler(0, ExceptionHandler);
	return (s_veh_handle != nullptr);
}

void CrashHandler::SetWriteDirectory(const std::string_view& dump_directory)
{
	if (!s_veh_handle)
		return;

	s_write_directory = StringUtil::UTF8StringToWideString(dump_directory);
}

void CrashHandler::WriteDumpForCaller()
{
	WriteMinidumpAndCallstack(nullptr);
}

void CrashHandler::Uninstall()
{
	if (s_veh_handle)
	{
		RemoveVectoredExceptionHandler(s_veh_handle);
		s_veh_handle = nullptr;
	}

	if (s_dbghelp_module)
	{
		FreeLibrary(s_dbghelp_module);
		s_dbghelp_module = nullptr;
	}
}

#else

bool CrashHandler::Install()
{
	return false;
}

void CrashHandler::SetWriteDirectory(const std::string_view& dump_directory)
{
}

void CrashHandler::WriteDumpForCaller()
{
}

void CrashHandler::Uninstall()
{
}

#endif