// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Assertions.h"
#include "common/BitUtils.h"
#include "common/Console.h"
#include "common/CrashHandler.h"
#include "common/Error.h"
#include "common/HostSys.h"

#include <cstdio>
#include <csignal>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <mutex>
#include <sys/mman.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libkern/OSCacheControl.h>
#include <TargetConditionals.h>
#endif
#ifndef __APPLE__
#include <ucontext.h>
#endif

#include "fmt/format.h"

#if defined(__ANDROID__)
#include <sys/syscall.h>
// bionic lacks shm_open until API 30; memfd_create is a libc wrapper only from
// API 30 too, but the raw syscall works from API 26 (our minSdk).
static int memfd_create_wrapper(const char* name, unsigned int flags)
{
	return static_cast<int>(syscall(__NR_memfd_create, name, flags));
}
#endif

#if defined(__FreeBSD__)
#include "cpuinfo.h"
#endif

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

#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
static int CreateIOSFileBackedSharedMemory(const char* name, size_t size, int shm_errno)
{
	const char* tmpdir = std::getenv("TMPDIR");
	if (!tmpdir || tmpdir[0] == '\0')
		tmpdir = "/tmp";

	for (int attempt = 0; attempt < 16; attempt++)
	{
		char path[PATH_MAX];
		const int written = std::snprintf(path, sizeof(path), "%s/armsx2_%s_%u_%d.mem",
			tmpdir, name, static_cast<unsigned>(getpid()), attempt);
		if (written <= 0 || static_cast<size_t>(written) >= sizeof(path))
			break;

		const int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd < 0)
		{
			if (errno == EEXIST)
				continue;

			std::fprintf(stderr,
				"@@IOS_SHM_FILE_FALLBACK_FAIL@@ shm_open_errno=%d open_errno=%d path=\"%s\" size=%zu\n",
				shm_errno, errno, path, size);
			return -1;
		}

		if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
			std::fprintf(stderr, "@@IOS_SHM_CLOEXEC_FAIL@@ errno=%d fd=%d\n", errno, fd);

		if (ftruncate(fd, static_cast<off_t>(size)) < 0)
		{
			const int truncate_errno = errno;
			unlink(path);
			close(fd);
			std::fprintf(stderr,
				"@@IOS_SHM_FILE_FALLBACK_FAIL@@ shm_open_errno=%d ftruncate_errno=%d path=\"%s\" size=%zu\n",
				shm_errno, truncate_errno, path, size);
			return -1;
		}

		const int unlink_errno = (unlink(path) != 0) ? errno : 0;
		std::fprintf(stderr,
			"@@IOS_SHM_FILE_FALLBACK@@ shm_open_errno=%d fd=%d size=%zu path=\"%s\" unlink_errno=%d\n",
			shm_errno, fd, size, path, unlink_errno);
		return fd;
	}

	std::fprintf(stderr,
		"@@IOS_SHM_FILE_FALLBACK_FAIL@@ shm_open_errno=%d reason=path_exhausted size=%zu\n",
		shm_errno, size);
	return -1;
}
#endif

void* HostSys::CreateSharedMemory(const char* name, size_t size)
{
#if defined(__ANDROID__)
	// Android: memfd_create available since API 26 (our minSdk), no shm_open until API 30.
	const int fd = memfd_create_wrapper(name, 0);
	if (fd < 0)
	{
		std::fprintf(stderr, "memfd_create failed: %d\n", errno);
		return nullptr;
	}
#else
	const int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
	if (fd < 0)
	{
		const int shm_errno = errno;
#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
		const int file_fd = CreateIOSFileBackedSharedMemory(name, size, shm_errno);
		if (file_fd >= 0)
			return reinterpret_cast<void*>(static_cast<intptr_t>(file_fd));
#endif
		std::fprintf(stderr, "shm_open failed: %d\n", shm_errno);
		return nullptr;
	}

	// we're not going to be opening this mapping in other processes, so remove the file
	shm_unlink(name);
#endif

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

#ifndef __APPLE__

size_t HostSys::GetRuntimePageSize()
{
	int res = sysconf(_SC_PAGESIZE);
	return (res > 0) ? static_cast<size_t>(res) : 0;
}

size_t HostSys::GetRuntimeCacheLineSize()
{
#if defined(__FreeBSD__)
	if (!cpuinfo_initialize())
		return 0;

	u32 max_line_size = 0;
	for (u32 i = 0; i < cpuinfo_get_processors_count(); i++)
	{
		const u32 l1i = cpuinfo_get_processor(i)->cache.l1i->line_size;
		const u32 l1d = cpuinfo_get_processor(i)->cache.l1d->line_size;
		const u32 res = std::max<u32>(l1i, l1d);

		max_line_size = std::max<u32>(max_line_size, res);
	}

	return static_cast<size_t>(max_line_size);
#else
	int l1i = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
	int l1d = sysconf(_SC_LEVEL1_ICACHE_LINESIZE);
	int res = (l1i > l1d) ? l1i : l1d;
	for (int index = 0; index < 16; index++)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "/sys/devices/system/cpu/cpu0/cache/index%d/coherency_line_size", index);
		std::FILE* fp = std::fopen(buf, "rb");
		if (!fp)
			break;

		std::fread(buf, sizeof(buf), 1, fp);
		std::fclose(fp);
		int val = std::atoi(buf);
		res = (val > res) ? val : res;
	}

	return (res > 0) ? static_cast<size_t>(res) : 0;
#endif
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


std::unique_ptr<SharedMemoryMappingArea> SharedMemoryMappingArea::Create(size_t size, bool jit)
{
	pxAssertRel(Common::IsAlignedPow2(size, __pagesize), "Size is page aligned");

	uint flags = MAP_ANONYMOUS | MAP_PRIVATE;
	uint prot = PROT_NONE;
#ifdef __APPLE__
	if (jit) {
		flags |= MAP_JIT;
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
		// iOS rejects PROT_NONE MAP_JIT reservations on the LiveContainer/JIT path.
		// Code memory must be born as an actual JIT mapping; callers can mprotect it
		// afterward, but we never fall back to a non-MAP_JIT executable region.
		prot = PROT_READ | PROT_WRITE | PROT_EXEC;
#endif
	}
#endif
	void* alloc = mmap(nullptr, size, prot, flags, -1, 0);
	if (alloc == MAP_FAILED)
	{
		const int err = errno;
		std::fprintf(stderr,
			"@@HOST_MMAP_FAIL@@ op=reserve size=%zu jit=%d flags=0x%x prot=0x%x err=%d message=\"%s\"\n",
			size, jit ? 1 : 0, flags, prot, err, std::strerror(err));
		return nullptr;
	}

	return std::unique_ptr<SharedMemoryMappingArea>(new SharedMemoryMappingArea(static_cast<u8*>(alloc), size, size / __pagesize));
}

u8* SharedMemoryMappingArea::Map(void* file_handle, size_t file_offset, void* map_base, size_t map_size, const PageProtectionMode& mode)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	const uint lnxmode = LinuxProt(mode);
	if (file_handle)
	{
		const int fd = static_cast<int>(reinterpret_cast<intptr_t>(file_handle));
		// MAP_FIXED is okay here, since we've reserved the entire region, and *want* to overwrite the mapping.
		void* const ptr = mmap(map_base, map_size, lnxmode, MAP_SHARED | MAP_FIXED, fd, static_cast<off_t>(file_offset));
		if (ptr == MAP_FAILED)
		{
			const int err = errno;
			std::fprintf(stderr,
				"@@HOST_MMAP_FAIL@@ op=map_shared base=%p size=%zu prot=0x%x err=%d message=\"%s\"\n",
				map_base, map_size, lnxmode, err, std::strerror(err));
			return nullptr;
		}
	}
	else
	{
		// macOS doesn't seem to allow MAP_JIT with MAP_FIXED
		// So we do the MAP_JIT in the allocation, and just mprotect here
		// Note that this will only work the first time for a given region
		if (mprotect(map_base, map_size, lnxmode) < 0)
		{
			const int err = errno;
			std::fprintf(stderr,
				"@@HOST_MMAP_FAIL@@ op=mprotect base=%p size=%zu prot=0x%x err=%d message=\"%s\"\n",
				map_base, map_size, lnxmode, err, std::strerror(err));
			return nullptr;
		}
	}

	m_num_mappings++;
	return static_cast<u8*>(map_base);
}

bool SharedMemoryMappingArea::Unmap(void* map_base, size_t map_size, bool is_file)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	if (mmap(map_base, map_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED)
		return false;

	m_num_mappings--;
	return true;
}

#ifdef ARCH_ARM64

void HostSys::FlushInstructionCache(void* address, u32 size)
{
#ifdef __APPLE__
	sys_icache_invalidate(address, size);
#else
	__builtin___clear_cache(reinterpret_cast<char*>(address), reinterpret_cast<char*>(address) + size);
#endif
}

#endif

#ifndef __APPLE__ // These are done in DarwinMisc

namespace PageFaultHandler
{
	static std::recursive_mutex s_exception_handler_mutex;
	static bool s_in_exception_handler = false;
	static bool s_installed = false;
} // namespace PageFaultHandler

#ifdef ARCH_ARM64

[[maybe_unused]] static bool IsStoreInstruction(const void* ptr)
{
	u32 bits;
	std::memcpy(&bits, ptr, sizeof(bits));

	// Based on vixl's disassembler Instruction::IsStore().
	// if (Mask(LoadStoreAnyFMask) != LoadStoreAnyFixed)
	if ((bits & 0x0a000000) != 0x08000000)
		return false;

	// if (Mask(LoadStorePairAnyFMask) == LoadStorePairAnyFixed)
	if ((bits & 0x3a000000) == 0x28000000)
	{
		// return Mask(LoadStorePairLBit) == 0
		return (bits & (1 << 22)) == 0;
	}

	switch (bits & 0xC4C00000)
	{
		case 0x00000000: // STRB_w
		case 0x40000000: // STRH_w
		case 0x80000000: // STR_w
		case 0xC0000000: // STR_x
		case 0x04000000: // STR_b
		case 0x44000000: // STR_h
		case 0x84000000: // STR_s
		case 0xC4000000: // STR_d
		case 0x04800000: // STR_q
			return true;

		default:
			return false;
	}
}

#endif // ARCH_ARM64

namespace PageFaultHandler
{
	static void SignalHandler(int sig, siginfo_t* info, void* ctx);
} // namespace PageFaultHandler

void PageFaultHandler::SignalHandler(int sig, siginfo_t* info, void* ctx)
{
#if defined(__linux__)
	void* const exception_address = reinterpret_cast<void*>(info->si_addr);

#if defined(ARCH_X86)
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.gregs[REG_RIP]);
	const bool is_write = (static_cast<ucontext_t*>(ctx)->uc_mcontext.gregs[REG_ERR] & 2) != 0;
#elif defined(ARCH_ARM64)
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.pc);
	const bool is_write = IsStoreInstruction(exception_pc);
#endif

#elif defined(__FreeBSD__)

#if defined(ARCH_X86)
	void* const exception_address = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.mc_addr);
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.mc_rip);
	const bool is_write = (static_cast<ucontext_t*>(ctx)->uc_mcontext.mc_err & 2) != 0;
#elif defined(ARCH_ARM64)
	void* const exception_address = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__es.__far);
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__ss.__pc);
	const bool is_write = IsStoreInstruction(exception_pc);
#endif

#endif

	// Executing the handler concurrently from multiple threads wouldn't go down well.
	s_exception_handler_mutex.lock();

	// Prevent recursive exception filtering.
	HandlerResult result = HandlerResult::ExecuteNextHandler;
	if (!s_in_exception_handler)
	{
		s_in_exception_handler = true;
		result = HandlePageFault(exception_pc, exception_address, is_write);
		s_in_exception_handler = false;
	}

	s_exception_handler_mutex.unlock();

	// Resumes execution right where we left off (re-executes instruction that caused the SIGSEGV).
	if (result == HandlerResult::ContinueExecution)
		return;

	// We couldn't handle it. Pass it off to the crash dumper.
	CrashHandler::CrashSignalHandler(sig, info, ctx);
}

bool PageFaultHandler::Install(Error* error)
{
	std::unique_lock lock(s_exception_handler_mutex);
	// Android keeps one process across game launches, so the VM lifecycle can call
	// Install() more than once. Make it idempotent instead of tripping the release
	// assert below (which aborts the CPU thread on the second game boot).
	if (s_installed)
		return true;
	pxAssertRel(!s_installed, "Page fault handler has already been installed.");

	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO | SA_NODEFER;
	sa.sa_sigaction = SignalHandler;

	if (sigaction(SIGSEGV, &sa, nullptr) != 0)
	{
		Error::SetErrno(error, "sigaction() for SIGSEGV failed: ", errno);
		return false;
	}

#ifdef ARCH_ARM64
	// We can get SIGBUS on ARM64.
	if (sigaction(SIGBUS, &sa, nullptr) != 0)
	{
		Error::SetErrno(error, "sigaction() for SIGBUS failed: ", errno);
		return false;
	}
#endif

	s_installed = true;
	return true;
}

bool PageFaultHandler::InstallSecondaryThread() { return true; }

#endif // __APPLE__
