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

#include <wx/string.h>
#include "common/Pcsx2Defs.h"
#include "common/RedtapeWindows.h"
#include "common/Exceptions.h"
#include "common/Dependencies.h"

#pragma comment(lib, "User32.lib")

static __aligned16 LARGE_INTEGER lfreq;

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
wxString GetOSVersionString()
{
	wxString retval;

	SYSTEM_INFO si;
	GetNativeSystemInfo(&si);

	if (!IsWindows8Point1OrGreater())
		return L"Unsupported Operating System!";

	retval += L"Microsoft ";

	if (IsWindows10OrGreater())
		retval += IsWindowsServer() ? L"Windows Server 2016" : L"Windows 10";
	else // IsWindows8Point1OrGreater()
		retval += IsWindowsServer() ? L"Windows Server 2012 R2" : L"Windows 8.1";

	if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
		retval += L", 64-bit";
	else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
		retval += L", 32-bit";

	return retval;
}

// --------------------------------------------------------------------------------------
//  Exception::WinApiError   (implementations)
// --------------------------------------------------------------------------------------
Exception::WinApiError::WinApiError()
{
	ErrorId = GetLastError();
	m_message_diag = L"Unspecified Windows API error.";
}

wxString Exception::WinApiError::GetMsgFromWindows() const
{
	if (!ErrorId)
		return L"No valid error number was assigned to this exception!";

	const DWORD BUF_LEN = 2048;
	TCHAR t_Msg[BUF_LEN];
	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, ErrorId, 0, t_Msg, BUF_LEN, 0))
		return wxsFormat(L"Win32 Error #%d: %s", ErrorId, t_Msg);

	return wxsFormat(L"Win32 Error #%d (no text msg available)", ErrorId);
}

wxString Exception::WinApiError::FormatDisplayMessage() const
{
	return m_message_user + L"\n\n" + GetMsgFromWindows();
}

wxString Exception::WinApiError::FormatDiagnosticMessage() const
{
	return m_message_diag + L"\n\t" + GetMsgFromWindows();
}

void ScreensaverAllow(bool allow)
{
	EXECUTION_STATE flags = ES_CONTINUOUS;
	if (!allow)
		flags |= ES_DISPLAY_REQUIRED;
	SetThreadExecutionState(flags);
}
#endif
