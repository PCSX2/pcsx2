// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#if defined(__APPLE__)
#define _XOPEN_SOURCE
#endif

#if !defined(_WIN32)
#include <cstdio>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <mutex>

#include "fmt/core.h"

#include "common/BitUtils.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/HostSys.h"

// Apple uses the MAP_ANON define instead of MAP_ANONYMOUS, but they mean
// the same thing.
#if defined(__APPLE__) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif

#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef __APPLE__
#include <ucontext.h>
#endif

static std::recursive_mutex s_exception_handler_mutex;
static PageFaultHandler s_exception_handler_callback;
static bool s_in_exception_handler;

#ifdef __APPLE__
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#endif

#if defined(__APPLE__) || defined(__aarch64__)
static struct sigaction s_old_sigbus_action;
#endif
#if !defined(__APPLE__) || defined(__aarch64__)
static struct sigaction s_old_sigsegv_action;
#endif

static void CallExistingSignalHandler(int signal, siginfo_t* siginfo, void* ctx)
{
#if defined(__aarch64__)
	const struct sigaction& sa = (signal == SIGBUS) ? s_old_sigbus_action : s_old_sigsegv_action;
#elif defined(__APPLE__)
	const struct sigaction& sa = s_old_sigbus_action;
#else
	const struct sigaction& sa = s_old_sigsegv_action;
#endif

	if (sa.sa_flags & SA_SIGINFO)
	{
		sa.sa_sigaction(signal, siginfo, ctx);
	}
	else if (sa.sa_handler == SIG_DFL)
	{
		// Re-raising the signal would just queue it, and since we'd restore the handler back to us,
		// we'd end up right back here again. So just abort, because that's probably what it'd do anyway.
		abort();
	}
	else if (sa.sa_handler != SIG_IGN)
	{
		sa.sa_handler(signal);
	}
}

// Linux implementation of SIGSEGV handler.  Bind it using sigaction().
static void SysPageFaultSignalFilter(int signal, siginfo_t* siginfo, void* ctx)
{
	// Executing the handler concurrently from multiple threads wouldn't go down well.
	std::unique_lock lock(s_exception_handler_mutex);

	// Prevent recursive exception filtering.
	if (s_in_exception_handler)
	{
		lock.unlock();
		CallExistingSignalHandler(signal, siginfo, ctx);
		return;
	}

	// Note: Use of stdio functions isn't safe here.  Avoid console logs, assertions, file logs,
	// or just about anything else useful. However, that's really only a concern if the signal
	// occurred within those functions. The logging which we do only happens when the exception
	// occurred within JIT code.

#if defined(__APPLE__) && defined(__x86_64__)
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__ss.__rip);
#elif defined(__FreeBSD__) && defined(__x86_64__)
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.mc_rip);
#elif defined(__x86_64__)
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.gregs[REG_RIP]);
#elif defined(__aarch64__)
	#ifndef __APPLE__
		void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.pc);
	#else
		void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__ss.__pc);
	#endif
#else
	void* const exception_pc = nullptr;
#endif

	const PageFaultInfo pfi{
		reinterpret_cast<uptr>(exception_pc), reinterpret_cast<uptr>(siginfo->si_addr) & ~static_cast<uptr>(__pagemask)};

	s_in_exception_handler = true;

	const bool handled = s_exception_handler_callback(pfi);

	s_in_exception_handler = false;

	// Resumes execution right where we left off (re-executes instruction that caused the SIGSEGV).
	if (handled)
		return;

	// Call old signal handler, which will likely dump core.
	lock.unlock();
	CallExistingSignalHandler(signal, siginfo, ctx);
}

bool HostSys::InstallPageFaultHandler(PageFaultHandler handler)
{
	std::unique_lock lock(s_exception_handler_mutex);
	pxAssertRel(!s_exception_handler_callback, "A page fault handler is already registered.");
	if (!s_exception_handler_callback)
	{
		struct sigaction sa;

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = SysPageFaultSignalFilter;
#ifdef __linux__
		// Don't block the signal from executing recursively, we want to fire the original handler.
		sa.sa_flags |= SA_NODEFER;
#endif
#if defined(__APPLE__) || defined(__aarch64__)
		// MacOS uses SIGBUS for memory permission violations, as well as SIGSEGV on ARM64.
		if (sigaction(SIGBUS, &sa, &s_old_sigbus_action) != 0)
			return false;
#endif
#if !defined(__APPLE__) || defined(__aarch64__)
		if (sigaction(SIGSEGV, &sa, &s_old_sigsegv_action) != 0)
			return false;
#endif
#if defined(__APPLE__) && defined(__aarch64__)
		// Stops LLDB getting in a EXC_BAD_ACCESS loop when passing page faults to PCSX2.
		task_set_exception_ports(mach_task_self(), EXC_MASK_BAD_ACCESS, MACH_PORT_NULL, EXCEPTION_DEFAULT, 0);
#endif
	}

	s_exception_handler_callback = handler;
	return true;
}

void HostSys::RemovePageFaultHandler(PageFaultHandler handler)
{
	std::unique_lock lock(s_exception_handler_mutex);
	pxAssertRel(!s_exception_handler_callback || s_exception_handler_callback == handler,
		"Not removing the same handler previously registered.");
	if (!s_exception_handler_callback)
		return;

	s_exception_handler_callback = nullptr;

	struct sigaction sa;
#if defined(__APPLE__) || defined(__aarch64__)
	sigaction(SIGBUS, &s_old_sigbus_action, &sa);
#endif
#if !defined(__APPLE__) || defined(__aarch64__)
	sigaction(SIGSEGV, &s_old_sigsegv_action, &sa);
#endif
}

static __ri uint LinuxProt(const PageProtectionMode& mode)
{
	u32 lnxmode = 0;

	if (mode.CanWrite())
		lnxmode |= PROT_WRITE;
	if (mode.CanRead())
		lnxmode |= PROT_READ;
	if (mode.CanExecute())
		lnxmode |= PROT_EXEC | PROT_READ;

	return lnxmode;
}

void* HostSys::Mmap(void* base, size_t size, const PageProtectionMode& mode)
{
	pxAssertMsg((size & (__pagesize - 1)) == 0, "Size is page aligned");

	if (mode.IsNone())
		return nullptr;

	const u32 prot = LinuxProt(mode);

	u32 flags = MAP_PRIVATE | MAP_ANONYMOUS;
	if (base)
		flags |= MAP_FIXED;

#if defined(__APPLE__) && defined(_M_ARM64)
	if (mode.CanExecute())
		flags |= MAP_JIT;
#endif

	void* res = mmap(base, size, prot, flags, -1, 0);
	if (res == MAP_FAILED)
		return nullptr;

	return res;
}

void HostSys::Munmap(void* base, size_t size)
{
	if (!base)
		return;

	munmap((void*)base, size);
}

void HostSys::MemProtect(void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	pxAssertMsg((size & (__pagesize - 1)) == 0, "Size is page aligned");

	const u32 lnxmode = LinuxProt(mode);

	const int result = mprotect(baseaddr, size, lnxmode);
	if (result != 0)
		pxFail("mprotect() failed");
}

std::string HostSys::GetFileMappingName(const char* prefix)
{
	const unsigned pid = static_cast<unsigned>(getpid());
#if defined(__FreeBSD__)
	// FreeBSD's shm_open(3) requires name to be absolute
	return fmt::format("/tmp/{}_{}", prefix, pid);
#else
	return fmt::format("{}_{}", prefix, pid);
#endif
}

void* HostSys::CreateSharedMemory(const char* name, size_t size)
{
	const int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
	if (fd < 0)
	{
		std::fprintf(stderr, "shm_open failed: %d\n", errno);
		return nullptr;
	}

	// we're not going to be opening this mapping in other processes, so remove the file
	shm_unlink(name);

	// ensure it's the correct size
	if (ftruncate(fd, static_cast<off_t>(size)) < 0)
	{
		std::fprintf(stderr, "ftruncate(%zu) failed: %d\n", size, errno);
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
	const uint lnxmode = LinuxProt(mode);

	const int flags = (baseaddr != nullptr) ? (MAP_SHARED | MAP_FIXED) : MAP_SHARED;
	void* ptr = mmap(baseaddr, size, lnxmode, flags, static_cast<int>(reinterpret_cast<intptr_t>(handle)), static_cast<off_t>(offset));
	if (ptr == MAP_FAILED)
		return nullptr;

	return ptr;
}

void HostSys::UnmapSharedMemory(void* baseaddr, size_t size)
{
	if (mmap(baseaddr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED)
		pxFailRel("Failed to unmap shared memory");
}

#ifdef _M_ARM64

void HostSys::FlushInstructionCache(void* address, u32 size)
{
	__builtin___clear_cache(reinterpret_cast<char*>(address), reinterpret_cast<char*>(address) + size);
}

#endif

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
	pxAssertRel(Common::IsAlignedPow2(size, __pagesize), "Size is page aligned");

	void* alloc = mmap(nullptr, size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (alloc == MAP_FAILED)
		return nullptr;

	return std::unique_ptr<SharedMemoryMappingArea>(new SharedMemoryMappingArea(static_cast<u8*>(alloc), size, size / __pagesize));
}

u8* SharedMemoryMappingArea::Map(void* file_handle, size_t file_offset, void* map_base, size_t map_size, const PageProtectionMode& mode)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	const uint lnxmode = LinuxProt(mode);
	void* const ptr = mmap(map_base, map_size, lnxmode, MAP_SHARED | MAP_FIXED,
		static_cast<int>(reinterpret_cast<intptr_t>(file_handle)), static_cast<off_t>(file_offset));
	if (ptr == MAP_FAILED)
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

#if defined(_M_ARM64) && defined(__APPLE__)

static thread_local int s_code_write_depth = 0;

void HostSys::BeginCodeWrite()
{
	if ((s_code_write_depth++) == 0)
		pthread_jit_write_protect_np(0);
}

void HostSys::EndCodeWrite()
{
	pxAssert(s_code_write_depth > 0);
	if ((--s_code_write_depth) == 0)
		pthread_jit_write_protect_np(1);
}

#endif
