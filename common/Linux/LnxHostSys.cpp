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
#include <wx/thread.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "common/Align.h"
#include "common/PageFaultSource.h"

// Apple uses the MAP_ANON define instead of MAP_ANONYMOUS, but they mean
// the same thing.
#if defined(__APPLE__) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif

#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ucontext.h>

extern void SignalExit(int sig);

static const uptr m_pagemask = getpagesize() - 1;

static struct sigaction s_old_sigsegv_action;
#if defined(__APPLE__)
static struct sigaction s_old_sigbus_action;
#endif

// Linux implementation of SIGSEGV handler.  Bind it using sigaction().
static void SysPageFaultSignalFilter(int signal, siginfo_t* siginfo, void* ctx)
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

#if defined(__x86_64__)
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.gregs[REG_RIP]);
#else
	void* const exception_pc = nullptr;
#endif

	// Note: This signal can be accessed by the EE or MTVU thread
	// Source_PageFault is a global variable with its own state information
	// so for now we lock this exception code unless someone can fix this better...
	Threading::ScopedLock lock(PageFault_Mutex);

	Source_PageFault->Dispatch(PageFaultInfo((uptr)exception_pc, (uptr)siginfo->si_addr & ~m_pagemask));

	// resumes execution right where we left off (re-executes instruction that
	// caused the SIGSEGV).
	if (Source_PageFault->WasHandled())
		return;

	if (!wxThread::IsMain())
	{
		pxFailRel(pxsFmt("Unhandled page fault @ 0x%08x", siginfo->si_addr));
	}

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
#if defined(__APPLE__)
	// MacOS uses SIGBUS for memory permission violations
	sigaction(SIGBUS, &sa, &s_old_sigbus_action);
#else
	sigaction(SIGSEGV, &sa, &s_old_sigsegv_action);
#endif
}

static __ri void PageSizeAssertionTest(size_t size)
{
	pxAssertMsg((__pagesize == getpagesize()), pxsFmt(
												   "Internal system error: Operating system pagesize does not match compiled pagesize.\n\t"
												   L"\tOS Page Size: 0x%x (%d), Compiled Page Size: 0x%x (%u)",
												   getpagesize(), getpagesize(), __pagesize, __pagesize));

	pxAssertDev((size & (__pagesize - 1)) == 0, pxsFmt(
													L"Memory block size must be a multiple of the target platform's page size.\n"
													L"\tPage Size: 0x%x (%u), Block Size: 0x%x (%u)",
													__pagesize, __pagesize, size, size));
}

static __ri uint LinuxProt(const PageProtectionMode& mode)
{
	uint lnxmode = 0;

	if (mode.CanWrite())
		lnxmode |= PROT_WRITE;
	if (mode.CanRead())
		lnxmode |= PROT_READ;
	if (mode.CanExecute())
		lnxmode |= PROT_EXEC | PROT_READ;

	return lnxmode;
}

// returns FALSE if the mprotect call fails with an ENOMEM.
// Raises assertions on other types of POSIX errors (since those typically reflect invalid object
// or memory states).
static bool _memprotect(void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	PageSizeAssertionTest(size);

	uint lnxmode = LinuxProt(mode);

	const int result = mprotect(baseaddr, size, lnxmode);

	if (result == 0)
		return true;

	switch (errno)
	{
		case EINVAL:
			pxFailDev(pxsFmt(L"mprotect returned EINVAL @ 0x%08X -> 0x%08X  (mode=%s)",
				baseaddr, (uptr)baseaddr + size, WX_STR(mode.ToString())));
			break;

		case EACCES:
			pxFailDev(pxsFmt(L"mprotect returned EACCES @ 0x%08X -> 0x%08X  (mode=%s)",
				baseaddr, (uptr)baseaddr + size, WX_STR(mode.ToString())));
			break;

		case ENOMEM:
			// caller handles assertion or exception, or whatever.
			break;
	}
	return false;
}

void* HostSys::MmapReservePtr(void* base, size_t size)
{
	PageSizeAssertionTest(size);

	// On linux a reserve-without-commit is performed by using mmap on a read-only
	// or anonymous source, with PROT_NONE (no-access) permission.  Since the mapping
	// is completely inaccessible, the OS will simply reserve it and will not put it
	// against the commit table.
	return mmap(base, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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

	if (!pxDoOutOfMemory)
		return false;
	pxDoOutOfMemory(size);
	return _memprotect(base, size, mode);
}

void HostSys::MmapResetPtr(void* base, size_t size)
{
	PageSizeAssertionTest(size);

	void* result = mmap(base, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	pxAssertRel((uptr)result == (uptr)base, pxsFmt(
												"Virtual memory decommit failed: memory at 0x%08X -> 0x%08X could not be remapped.",
												base, (uptr)base + size));
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
	PageSizeAssertionTest(size);

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
		throw Exception::OutOfMemory(L"MemProtect")
			.SetDiagMsg(pxsFmt(L"mprotect failed @ 0x%08X -> 0x%08X  (mode=%s)",
				baseaddr, (uptr)baseaddr + size, WX_STR(mode.ToString())));
	}
}

wxString HostSys::GetFileMappingName(const char* prefix)
{
	const unsigned pid = static_cast<unsigned>(getpid());

	FastFormatAscii ret;
	ret.Write("%s_%u", prefix, pid);

	return ret.GetString();
}

void* HostSys::CreateSharedMemory(const wxString& name, size_t size)
{
	const int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
	if (fd < 0)
	{
		std::fprintf(stderr, "shm_open failed: %d\n", errno);
		return nullptr;
	}

	// we're not going to be opening this mapping in other processes, so remove the file
	shm_unlink(name.c_str());

	// ensure it's the correct size
	if (ftruncate64(fd, static_cast<off64_t>(size)) < 0)
	{
		std::fprintf(stderr, "ftruncate64(%zu) failed: %d\n", size, errno);
		return nullptr;
	}

	return reinterpret_cast<void*>(static_cast<intptr_t>(fd));
}

void HostSys::DestroySharedMemory(void* ptr)
{
	close(static_cast<int>(reinterpret_cast<intptr_t>(ptr)));
}

void* HostSys::MapSharedMemory(void* handle, size_t offset, void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	uint lnxmode = LinuxProt(mode);

	const int flags = (baseaddr != nullptr) ? (MAP_SHARED | MAP_FIXED) : MAP_SHARED;
	void* ptr = mmap(baseaddr, size, lnxmode, flags, static_cast<int>(reinterpret_cast<intptr_t>(handle)), static_cast<off_t>(offset));
	if (!ptr || ptr == reinterpret_cast<void*>(-1))
		return nullptr;

	return ptr;
}

void HostSys::UnmapSharedMemory(void* baseaddr, size_t size)
{
	if (mmap(baseaddr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED)
		pxFailRel("Failed to unmap shared memory");
}

SharedMemoryMappingArea::SharedMemoryMappingArea(u8* base_ptr, size_t size, size_t num_pages)
	: m_base_ptr(base_ptr)
	, m_size(size)
	, m_num_pages(num_pages)
{
}

SharedMemoryMappingArea::~SharedMemoryMappingArea()
{
	pxAssertRel(m_num_mappings == 0, "No mappings left");

	if (munmap(m_base_ptr, m_size) != 0)
		pxFailRel("Failed to release shared memory area");
}


std::unique_ptr<SharedMemoryMappingArea> SharedMemoryMappingArea::Create(size_t size)
{
	pxAssertRel(Common::IsAlignedPow2(size, HOST_PAGE_SIZE), "Size is page aligned");

	void* alloc = mmap(nullptr, size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (alloc == MAP_FAILED)
		return nullptr;

	return std::unique_ptr<SharedMemoryMappingArea>(new SharedMemoryMappingArea(static_cast<u8*>(alloc), size, size / HOST_PAGE_SIZE));
}

u8* SharedMemoryMappingArea::Map(void* file_handle, size_t file_offset, void* map_base, size_t map_size, const PageProtectionMode& mode)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	const uint lnxmode = LinuxProt(mode);
	void* const ptr = mmap(map_base, map_size, lnxmode, MAP_SHARED | MAP_FIXED,
		static_cast<int>(reinterpret_cast<intptr_t>(file_handle)), static_cast<off_t>(file_offset));
	if (!ptr || ptr == reinterpret_cast<void*>(-1))
		return nullptr;

	m_num_mappings++;
	return static_cast<u8*>(ptr);
}

bool SharedMemoryMappingArea::Unmap(void* map_base, size_t map_size)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	if (mmap(map_base, map_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED)
		return false;

	m_num_mappings--;
	return true;
}

#endif
