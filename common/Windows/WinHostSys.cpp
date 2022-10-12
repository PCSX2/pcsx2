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

#if defined(_WIN32)

#include "common/Align.h"
#include "common/RedtapeWindows.h"
#include "common/PageFaultSource.h"
#include "common/Console.h"
#include "common/Exceptions.h"
#include "common/StringUtil.h"
#include "common/AlignedMalloc.h"
#include "fmt/core.h"

static long DoSysPageFaultExceptionFilter(EXCEPTION_POINTERS* eps)
{
	if (eps->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;

	// Note: This exception can be accessed by the EE or MTVU thread
	// Source_PageFault is a global variable with its own state information
	// so for now we lock this exception code unless someone can fix this better...
	std::unique_lock lock(PageFault_Mutex);
	Source_PageFault->Dispatch(PageFaultInfo((uptr)eps->ExceptionRecord->ExceptionInformation[1]));
	return Source_PageFault->WasHandled() ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

long __stdcall SysPageFaultExceptionFilter(EXCEPTION_POINTERS* eps)
{
	// Prevent recursive exception filtering by catching the exception from the filter here.
	// In the event that the filter causes an access violation (happened during shutdown
	// because Source_PageFault was deallocated), this will allow the debugger to catch the
	// exception.
	// TODO: find a reliable way to debug the filter itself, I've come up with a few ways that
	// work but I don't fully understand why some do and some don't.
	__try
	{
		return DoSysPageFaultExceptionFilter(eps);
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

void _platform_InstallSignalHandler()
{
#ifdef _WIN64 // We don't handle SEH properly on Win64 so use a vectored exception handler instead
	AddVectoredExceptionHandler(true, SysPageFaultExceptionFilter);
#endif
}


static DWORD ConvertToWinApi(const PageProtectionMode& mode)
{
	DWORD winmode = PAGE_NOACCESS;

	// Windows has some really bizarre memory protection enumeration that uses bitwise
	// numbering (like flags) but is in fact not a flag value.  *Someone* from the early
	// microsoft days wasn't a very good coder, me thinks.  --air

	if (mode.CanExecute())
	{
		winmode = mode.CanWrite() ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
	}
	else if (mode.CanRead())
	{
		winmode = mode.CanWrite() ? PAGE_READWRITE : PAGE_READONLY;
	}

	return winmode;
}

void* HostSys::Mmap(void* base, size_t size, const PageProtectionMode& mode)
{
	if (mode.IsNone())
		return nullptr;

	return VirtualAlloc(base, size, MEM_RESERVE | MEM_COMMIT, ConvertToWinApi(mode));
}

void HostSys::Munmap(void* base, size_t size)
{
	if (!base)
		return;

	VirtualFree((void*)base, 0, MEM_RELEASE);
}

void HostSys::MemProtect(void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	pxAssert((size & (__pagesize - 1)) == 0);

	DWORD OldProtect; // enjoy my uselessness, yo!
	if (!VirtualProtect(baseaddr, size, ConvertToWinApi(mode), &OldProtect))
		pxFail("VirtualProtect() failed");
}

std::string HostSys::GetFileMappingName(const char* prefix)
{
	const unsigned pid = GetCurrentProcessId();
	return fmt::format("{}_{}", prefix, pid);
}

void* HostSys::CreateSharedMemory(const char* name, size_t size)
{
	return static_cast<void*>(CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
		static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), StringUtil::UTF8StringToWideString(name).c_str()));
}

void HostSys::DestroySharedMemory(void* ptr)
{
	CloseHandle(static_cast<HANDLE>(ptr));
}

void* HostSys::MapSharedMemory(void* handle, size_t offset, void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	void* ret = MapViewOfFileEx(static_cast<HANDLE>(handle), FILE_MAP_READ | FILE_MAP_WRITE,
		static_cast<DWORD>(offset >> 32), static_cast<DWORD>(offset), size, baseaddr);
	if (!ret)
		return nullptr;

	const DWORD prot = ConvertToWinApi(mode);
	if (prot != PAGE_READWRITE)
	{
		DWORD old_prot;
		if (!VirtualProtect(ret, size, prot, &old_prot))
			pxFail("Failed to protect memory mapping");
	}
	return ret;
}

void HostSys::UnmapSharedMemory(void* baseaddr, size_t size)
{
	if (!UnmapViewOfFile(baseaddr))
		pxFail("Failed to unmap shared memory");
}

#endif
