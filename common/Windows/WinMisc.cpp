/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#if defined(_WIN32)

#include "common/Pcsx2Defs.h"
#include "common/RedtapeWindows.h"
#include "common/Exceptions.h"
#include "common/StringUtil.h"

#include "fmt/core.h"

#pragma comment(lib, "User32.lib")

alignas(16) static LARGE_INTEGER lfreq;

void InitCPUTicks()
{
	QueryPerformanceFrequency(&lfreq);
}

u64 GetTickFrequency()
{
	return lfreq.QuadPart;
}

u64 GetCPUTicks()
{
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return count.QuadPart;
}

u64 GetPhysicalMemory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullTotalPhys;
}

// Calculates the Windows OS Version and processor architecture, and returns it as a
// human-readable string. :)
std::string GetOSVersionString()
{
	std::string retval;

	SYSTEM_INFO si;
	GetNativeSystemInfo(&si);

	if (!IsWindows8Point1OrGreater())
	{
		retval = "Unsupported Operating System!";
	}
	else
	{
		retval = "Microsoft ";

		if (IsWindows10OrGreater())
			retval += IsWindowsServer() ? "Windows Server 2016" : "Windows 10";
		else // IsWindows8Point1OrGreater()
			retval += IsWindowsServer() ? "Windows Server 2012 R2" : "Windows 8.1";
	}

	return retval;
}

// --------------------------------------------------------------------------------------
//  Exception::WinApiError   (implementations)
// --------------------------------------------------------------------------------------
Exception::WinApiError::WinApiError()
{
	ErrorId = GetLastError();
	m_message_diag = "Unspecified Windows API error.";
}

std::string Exception::WinApiError::GetMsgFromWindows() const
{
	if (!ErrorId)
		return "No valid error number was assigned to this exception!";

	const DWORD BUF_LEN = 2048;
	wchar_t t_Msg[BUF_LEN];
	if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, 0, ErrorId, 0, t_Msg, BUF_LEN, 0))
		return fmt::format("Win32 Error #{}: {}", ErrorId, StringUtil::WideStringToUTF8String(t_Msg));

	return fmt::format("Win32 Error #{} (no text msg available)", ErrorId);
}

std::string Exception::WinApiError::FormatDisplayMessage() const
{
	return m_message_user + "\n\n" + GetMsgFromWindows();
}

std::string Exception::WinApiError::FormatDiagnosticMessage() const
{
	return m_message_diag + "\n\t" + GetMsgFromWindows();
}

void ScreensaverAllow(bool allow)
{
	EXECUTION_STATE flags = ES_CONTINUOUS;
	if (!allow)
		flags |= ES_DISPLAY_REQUIRED;
	SetThreadExecutionState(flags);
}
#endif
