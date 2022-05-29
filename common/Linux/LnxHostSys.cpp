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

#if !defined(_WIN32)
#include <cstdio>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "fmt/core.h"

#include "common/PageFaultSource.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Exceptions.h"

// Apple uses the MAP_ANON define instead of MAP_ANONYMOUS, but they mean
// the same thing.
#if defined(__APPLE__) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif

extern void SignalExit(int sig);

static const uptr m_pagemask = getpagesize() - 1;

// Linux implementation of SIGSEGV handler.  Bind it using sigaction().
static void SysPageFaultSignalFilter(int signal, siginfo_t* siginfo, void*)
{
	// [TODO] : Add a thread ID filter to the Linux Signal handler here.
	// Rationale: On windows, the __try/__except model allows per-thread specific behavior
	// for page fault handling.  On linux, there is a single signal handler for the whole
	// process, but the handler is executed by the thread that caused the exception.


	// Stdio Usage note: SIGSEGV handling is a synchronous in-thread signal.  It is done
	// from the context of the current thread and stackframe.  So long as the thread is not
	// the main/ui thread, use of the px assertion system should be safe.  Use of stdio should
	// be safe even on the main thread.
	//  (in other words, stdio limitations only really apply to process-level asynchronous
	//   signals)

	// Note: Use of stdio functions isn't safe here.  Avoid console logs,
	// assertions, file logs, or just about anything else useful.


	// Note: This signal can be accessed by the EE or MTVU thread
	// Source_PageFault is a global variable with its own state information
	// so for now we lock this exception code unless someone can fix this better...
	std::unique_lock lock(PageFault_Mutex);

	Source_PageFault->Dispatch(PageFaultInfo((uptr)siginfo->si_addr & ~m_pagemask));

	// resumes execution right where we left off (re-executes instruction that
	// caused the SIGSEGV).
	if (Source_PageFault->WasHandled())
		return;

	std::fprintf(stderr, "Unhandled page fault @ 0x%08x", siginfo->si_addr);
	pxFailRel("Unhandled page fault");

	// Bad mojo!  Completely invalid address.
	// Instigate a trap if we're in a debugger, and if not then do a SIGKILL.

	pxTrap();
	if (!IsDebugBuild)
		raise(SIGKILL);
}

void _platform_InstallSignalHandler()
{
	Console.WriteLn("Installing POSIX SIGSEGV handler...");
	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = SysPageFaultSignalFilter;
#ifdef __APPLE__
	// MacOS uses SIGBUS for memory permission violations
	sigaction(SIGBUS, &sa, NULL);
#else
	sigaction(SIGSEGV, &sa, NULL);
#endif
}

// returns FALSE if the mprotect call fails with an ENOMEM.
// Raises assertions on other types of POSIX errors (since those typically reflect invalid object
// or memory states).
static bool _memprotect(void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	pxAssertDev((size & (__pagesize - 1)) == 0, "Size is page aligned");

	uint lnxmode = 0;

	if (mode.CanWrite())
		lnxmode |= PROT_WRITE;
	if (mode.CanRead())
		lnxmode |= PROT_READ;
	if (mode.CanExecute())
		lnxmode |= PROT_EXEC | PROT_READ;

	const int result = mprotect(baseaddr, size, lnxmode);

	if (result == 0)
		return true;

	switch (errno)
	{
		case EINVAL:
			pxFailDev(fmt::format("mprotect returned EINVAL @ 0x{:X} -> 0x{:X}  (mode={})",
				baseaddr, (uptr)baseaddr + size, mode.ToString()).c_str());
			break;

		case EACCES:
			pxFailDev(fmt::format("mprotect returned EACCES @ 0x{:X} -> 0x{:X}  (mode={})",
				baseaddr, (uptr)baseaddr + size, mode.ToString()).c_str());
			break;

		case ENOMEM:
			// caller handles assertion or exception, or whatever.
			break;
	}
	return false;
}

void* HostSys::MmapReservePtr(void* base, size_t size)
{
	pxAssertDev((size & (__pagesize - 1)) == 0, "Size is page aligned");

	// On linux a reserve-without-commit is performed by using mmap on a read-only
	// or anonymous source, with PROT_NONE (no-access) permission.  Since the mapping
	// is completely inaccessible, the OS will simply reserve it and will not put it
	// against the commit table.
	void* result = mmap(base, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	if (result == MAP_FAILED)
		result = nullptr;

	return result;
}

bool HostSys::MmapCommitPtr(void* base, size_t size, const PageProtectionMode& mode)
{
	// In linux, reserved memory is automatically committed when its permissions are
	// changed to something other than PROT_NONE.  If the user is committing memory
	// as PROT_NONE, then just ignore this call (memory will be committed automatically
	// later when the user changes permissions to something useful via calls to MemProtect).

	if (mode.IsNone())
		return false;

	if (_memprotect(base, size, mode))
		return true;

	return false;
}

void HostSys::MmapResetPtr(void* base, size_t size)
{
	pxAssertDev((size & (__pagesize - 1)) == 0, "Size is page aligned");

	void* result = mmap(base, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	pxAssertRel((uptr)result == (uptr)base, "Virtual memory decommit failed");
}

void* HostSys::MmapReserve(uptr base, size_t size)
{
	return MmapReservePtr((void*)base, size);
}

bool HostSys::MmapCommit(uptr base, size_t size, const PageProtectionMode& mode)
{
	return MmapCommitPtr((void*)base, size, mode);
}

void HostSys::MmapReset(uptr base, size_t size)
{
	MmapResetPtr((void*)base, size);
}

void* HostSys::Mmap(uptr base, size_t size)
{
	pxAssertDev((size & (__pagesize - 1)) == 0, "Size is page aligned");

	// MAP_ANONYMOUS - means we have no associated file handle (or device).

	return mmap((void*)base, size, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void HostSys::Munmap(uptr base, size_t size)
{
	if (!base)
		return;
	munmap((void*)base, size);
}

void HostSys::MemProtect(void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	if (!_memprotect(baseaddr, size, mode))
	{
		throw Exception::OutOfMemory("MemProtect")
			.SetDiagMsg(fmt::format("mprotect failed @ 0x{:X} -> 0x{:X}  (mode={})",
				baseaddr, (uptr)baseaddr + size, mode.ToString()));
	}
}
#endif
