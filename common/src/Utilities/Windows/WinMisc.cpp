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

#pragma comment(lib, "User32.lib")

#include "PrecompiledHeader.h"
#include "RedtapeWindows.h"

#include <ShTypes.h>

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

// Windows SDK 7 provides this but previous ones do not, so roll our own in those cases:
#ifndef VER_SUITE_WH_SERVER
#define VER_SUITE_WH_SERVER 0x00008000
#endif

typedef void(WINAPI *PGNSI)(LPSYSTEM_INFO);
typedef BOOL(WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

// Win 10 SDK
#ifndef PRODUCT_CORE_N
#define PRODUCT_CORE_N 0x00000062
#endif
#ifndef PRODUCT_CORE
#define PRODUCT_CORE 0x00000065
#endif
#ifndef PRODUCT_PROFESSIONAL_WMC
#define PRODUCT_PROFESSIONAL_WMC 0x00000067
#endif
#ifndef PRODUCT_EDUCATION
#define PRODUCT_EDUCATION 0x00000079
#endif
#ifndef PRODUCT_EDUCATION_N
#define PRODUCT_EDUCATION_N 0x0000007A
#endif
#ifndef PRODUCT_ENTERPRISE_S
#define PRODUCT_ENTERPRISE_S 0x0000007D
#endif
#ifndef PRODUCT_ENTERPRISE_S_N
#define PRODUCT_ENTERPRISE_S_N 0x0000007E
#endif

// Calculates the Windows OS Version and install information, and returns it as a
// human-readable string. :)
// (Handy function borrowed from Microsoft's MSDN Online, and reformatted to use wxString.)
wxString GetOSVersionString()
{
    wxString retval;

    OSVERSIONINFOEX osvi;
    SYSTEM_INFO si;
    PGNSI pGNSI;
    PGPI pGPI;
    BOOL bOsVersionInfoEx;
    DWORD dwType;

    memzero(si);
    memzero(osvi);

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if (!(bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO *)&osvi)))
        return L"GetVersionEx Error!";

    // Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.

    pGNSI = (PGNSI)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "GetNativeSystemInfo");
    if (NULL != pGNSI)
        pGNSI(&si);
    else
        GetSystemInfo(&si);

    if (VER_PLATFORM_WIN32_NT != osvi.dwPlatformId || osvi.dwMajorVersion <= 5)
        return L"Unsupported Operating System!";

    retval += L"Microsoft ";

    // Test for the specific product.

    if (osvi.dwMajorVersion == 10) {
        if (osvi.dwMinorVersion == 0)
            retval += (osvi.wProductType == VER_NT_WORKSTATION) ? L"Windows 10 " : L"Windows Server 2016 ";

        pGPI = (PGPI)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetProductInfo");

        pGPI(osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);

        switch (dwType) {
            case PRODUCT_CORE:
                retval += L"Home";
                break;
            case PRODUCT_CORE_N:
                retval += L"Home N";
                break;
            case PRODUCT_PROFESSIONAL:
                retval += L"Pro";
                break;
            case PRODUCT_PROFESSIONAL_N:
                retval += L"Pro N";
                break;
            case PRODUCT_ENTERPRISE:
                retval += L"Enterprise";
                break;
            case PRODUCT_ENTERPRISE_N:
                retval += L"Enterprise N";
                break;
            case PRODUCT_ENTERPRISE_S:
                retval += L"Enterprise 2015 LTSB";
                break;
            case PRODUCT_ENTERPRISE_S_N:
                retval += L"Enterprise 2015 LTSB N";
                break;
            case PRODUCT_EDUCATION:
                retval += L"Education";
                break;
            case PRODUCT_EDUCATION_N:
                retval += L"Education N";
                break;
        }
    }

    if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion > 1) {
        if (osvi.dwMinorVersion == 2)
            retval += (osvi.wProductType == VER_NT_WORKSTATION) ? L"Windows 8 " : L"Windows Server 2012 ";

        if (osvi.dwMinorVersion == 3)
            retval += (osvi.wProductType == VER_NT_WORKSTATION) ? L"Windows 8.1 " : L"Windows Server 2012 R2 ";

        pGPI = (PGPI)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetProductInfo");

        pGPI(osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);

        switch (dwType) {
            case PRODUCT_PROFESSIONAL:
                retval += L"Pro";
                break;
            case PRODUCT_PROFESSIONAL_N:
                retval += L"Pro N";
                break;
            case PRODUCT_PROFESSIONAL_WMC:
                retval += L"Pro with Media Center";
                break;
            case PRODUCT_ENTERPRISE:
                retval += L"Enterprise";
                break;
            case PRODUCT_ENTERPRISE_N:
                retval += L"Enterprise N";
                break;
            case PRODUCT_SERVER_FOUNDATION:
                retval += L"Foundation";
                break;
            case PRODUCT_STANDARD_SERVER:
                retval += L"Standard";
                break;
            case PRODUCT_STANDARD_SERVER_CORE:
                retval += L"Standard (core)";
                break;
            case PRODUCT_DATACENTER_SERVER:
                retval += L"Datacenter";
                break;
            case PRODUCT_DATACENTER_SERVER_CORE:
                retval += L"Datacenter (core)";
                break;
        }
    }

    if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion <= 1) {
        if (osvi.dwMinorVersion == 0)
            retval += (osvi.wProductType == VER_NT_WORKSTATION) ? L"Windows Vista " : L"Windows Server 2008 ";

        if (osvi.dwMinorVersion == 1)
            retval += (osvi.wProductType == VER_NT_WORKSTATION) ? L"Windows 7 " : L"Windows Server 2008 R2 ";

        pGPI = (PGPI)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetProductInfo");

        pGPI(osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);

        switch (dwType) {
            case PRODUCT_ULTIMATE:
                retval += L"Ultimate Edition";
                break;
            case PRODUCT_HOME_PREMIUM:
                retval += L"Home Premium Edition";
                break;
            case PRODUCT_HOME_BASIC:
                retval += L"Home Basic Edition";
                break;
            case PRODUCT_ENTERPRISE:
                retval += L"Enterprise Edition";
                break;
            case PRODUCT_BUSINESS:
                retval += L"Business Edition";
                break;
            case PRODUCT_STARTER:
                retval += L"Starter Edition";
                break;
            case PRODUCT_CLUSTER_SERVER:
                retval += L"Cluster Server Edition";
                break;
            case PRODUCT_DATACENTER_SERVER:
                retval += L"Datacenter Edition";
                break;
            case PRODUCT_DATACENTER_SERVER_CORE:
                retval += L"Datacenter Edition (core installation)";
                break;
            case PRODUCT_ENTERPRISE_SERVER:
                retval += L"Enterprise Edition";
                break;
            case PRODUCT_ENTERPRISE_SERVER_CORE:
                retval += L"Enterprise Edition (core installation)";
                break;
            case PRODUCT_SMALLBUSINESS_SERVER:
                retval += L"Small Business Server";
                break;
            case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
                retval += L"Small Business Server Premium Edition";
                break;
            case PRODUCT_STANDARD_SERVER:
                retval += L"Standard Edition";
                break;
            case PRODUCT_STANDARD_SERVER_CORE:
                retval += L"Standard Edition (core installation)";
                break;
            case PRODUCT_WEB_SERVER:
                retval += L"Web Server Edition";
                break;
        }
    }

    // Include service pack (if any) and build number.

    if (_tcslen(osvi.szCSDVersion) > 0)
        retval += (wxString)L" " + osvi.szCSDVersion;

    retval += wxsFormat(L" (build %d)", osvi.dwBuildNumber);

    if (osvi.dwMajorVersion >= 6) {
        if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
            retval += L", 64-bit";
        else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
            retval += L", 32-bit";
    }

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
