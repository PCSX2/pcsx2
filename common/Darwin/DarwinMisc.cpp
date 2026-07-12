// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Assertions.h"
#include "common/BitUtils.h"
#include "common/Console.h"
#include "common/CrashHandler.h"
#include "common/Darwin/DarwinMisc.h"
#include "common/Error.h"
#include "common/Pcsx2Types.h"
#include "common/Threading.h"
#include "common/WindowInfo.h"
#include "common/HostSys.h"
#include "fmt/format.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <errno.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <optional>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <mach/mach_time.h>
#include <mach/message.h>
#include <mach/task.h>
#include <mach/thread_state.h>
#include <mach/vm_map.h>
#include <mutex>
#include <pthread.h>
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#else
#include <glob.h>
#endif

#if TARGET_OS_IPHONE
extern "C" int csops(pid_t pid, unsigned int ops, void* useraddr, size_t usersize);
#endif

// Darwin (OSX) is a bit different from Linux when requesting properties of
// the OS because of its BSD/Mach heritage. Helpfully, most of this code
// should translate pretty well to other *BSD systems. (e.g.: the sysctl(3)
// interface).
//
// For an overview of all of Darwin's sysctls, check:
// https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/sysctl.3.html

// Return the total physical memory on the machine, in bytes. Returns 0 on
// failure (not supported by the operating system).
u64 GetPhysicalMemory()
{
	u64 getmem = 0;
	size_t len = sizeof(getmem);
	int mib[] = {CTL_HW, HW_MEMSIZE};
	if (sysctl(mib, std::size(mib), &getmem, &len, NULL, 0) < 0)
		perror("sysctl:");
	return getmem;
}

u64 GetAvailablePhysicalMemory()
{
	const mach_port_t host_port = mach_host_self();
	vm_size_t page_size;

	// Get the system's page size.
	if (host_page_size(host_port, &page_size) != KERN_SUCCESS)
		return 0;

	vm_statistics64_data_t vm_stat;
	mach_msg_type_number_t host_size = sizeof(vm_statistics64_data_t) / sizeof(integer_t);

	// Get system memory statistics.
	if (host_statistics64(host_port, HOST_VM_INFO, reinterpret_cast<host_info64_t>(&vm_stat), &host_size) != KERN_SUCCESS)
		return 0;

	// Get the number of free and inactive pages.
	const u64 free_pages = static_cast<u64>(vm_stat.free_count);
	const u64 inactive_pages = static_cast<u64>(vm_stat.inactive_count);

	// Calculate available memory.
	const u64 get_available_mem = (free_pages + inactive_pages) * page_size;

	return get_available_mem;
}

static mach_timebase_info_data_t s_timebase_info;
static const u64 tickfreq = []() {
	if (mach_timebase_info(&s_timebase_info) != KERN_SUCCESS)
		abort();
	return (u64)1e9 * (u64)s_timebase_info.denom / (u64)s_timebase_info.numer;
}();

// returns the performance-counter frequency: ticks per second (Hz)
//
// usage:
//   u64 seconds_passed = GetCPUTicks() / GetTickFrequency();
//   u64 millis_passed = (GetCPUTicks() * 1000) / GetTickFrequency();
//
// NOTE: multiply, subtract, ... your ticks before dividing by
// GetTickFrequency() to maintain good precision.
u64 GetTickFrequency()
{
#if defined(__APPLE__) && TARGET_OS_IPHONE && defined(__aarch64__)
	// iOS: read the frequency of the architected virtual counter directly,
	// avoiding the mach_absolute_time trap overhead on every GetCPUTicks call.
	u64 freq;
	asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	return freq;
#else
	return tickfreq;
#endif
}

// return the number of "ticks" since some arbitrary, fixed time in the
// past. On OSX x86(-64), this is actually the number of nanoseconds passed,
// because mach_timebase_info.numer == denom == 1. So "ticks" ==
// nanoseconds.
u64 GetCPUTicks()
{
#if defined(__APPLE__) && TARGET_OS_IPHONE && defined(__aarch64__)
	// iOS: read the monotonic architected virtual counter directly, avoiding
	// the mach_absolute_time trap on every tick (frame limiter, profiling).
	u64 val;
	asm volatile("mrs %0, cntvct_el0" : "=r"(val)::"memory");
	return val;
#else
	return mach_absolute_time();
#endif
}

static std::string sysctl_str(int category, int name)
{
	char buf[32];
	size_t len = sizeof(buf);
	int mib[] = {category, name};
	sysctl(mib, std::size(mib), buf, &len, nullptr, 0);
	return std::string(buf, len > 0 ? len - 1 : 0);
}

template <typename T>
static std::optional<T> sysctlbyname_T(const char* name)
{
	T output = 0;
	size_t output_size = sizeof(output);
	if (sysctlbyname(name, &output, &output_size, nullptr, 0) != 0)
		return std::nullopt;
	if (output_size != sizeof(output))
	{
		ERROR_LOG("(DarwinMisc) sysctl {} gave unexpected size {}", name, output_size);
		return std::nullopt;
	}

	return output;
}

std::string GetOSVersionString()
{
	std::string type = sysctl_str(CTL_KERN, KERN_OSTYPE);
	std::string release = sysctl_str(CTL_KERN, KERN_OSRELEASE);
	std::string arch = sysctl_str(CTL_HW, HW_MACHINE);
	return type + " " + release + " " + arch;
}

#if !TARGET_OS_IPHONE
static IOPMAssertionID s_pm_assertion;
#endif

bool Common::InhibitScreensaver(bool inhibit)
{
#if !TARGET_OS_IPHONE
	if (s_pm_assertion)
	{
		IOPMAssertionRelease(s_pm_assertion);
		s_pm_assertion = 0;
	}

	if (inhibit)
		IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn, CFSTR("Playing a game"), &s_pm_assertion);
#endif
	return true;
}

#if TARGET_OS_IPHONE
// iOS has no mouse cursor — stub these out.
void Common::SetMousePosition(int x, int y) {}
bool Common::AttachMousePositionCb(std::function<void(int, int)> cb) { return false; }
void Common::DetachMousePositionCb() {}
#else
void Common::SetMousePosition(int x, int y)
{
	// Little bit ugly but;
	// Creating mouse move events and posting them wasn't very reliable.
	// Calling CGWarpMouseCursorPosition without CGAssociateMouseAndMouseCursorPosition(false)
	// ends up with the cursor feeling "sticky".
	CGAssociateMouseAndMouseCursorPosition(false);
	CGWarpMouseCursorPosition(CGPointMake(x, y));
	CGAssociateMouseAndMouseCursorPosition(true); // The default state
	return;
}

CFMachPortRef mouseEventTap = nullptr;
CFRunLoopSourceRef mouseRunLoopSource = nullptr;

static std::function<void(int, int)> fnMouseMoveCb;
CGEventRef mouseMoveCallback(CGEventTapProxy, CGEventType type, CGEventRef event, void* arg)
{
	if (type == kCGEventMouseMoved)
	{
		const CGPoint location = CGEventGetLocation(event);
		fnMouseMoveCb(location.x, location.y);
	}
	return event;
}

bool Common::AttachMousePositionCb(std::function<void(int, int)> cb)
{
	if (!AXIsProcessTrusted())
	{
		Console.Warning("Process isn't trusted with accessibility permissions. Mouse tracking will not work!");
	}

	fnMouseMoveCb = cb;
	mouseEventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
		CGEventMaskBit(kCGEventMouseMoved), mouseMoveCallback, nullptr);
	if (!mouseEventTap)
	{
		Console.Warning("Unable to create mouse moved event tap. Mouse tracking will not work!");
		return false;
	}

	mouseRunLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, mouseEventTap, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), mouseRunLoopSource, kCFRunLoopCommonModes);

	return true;
}

void Common::DetachMousePositionCb()
{
	if (mouseRunLoopSource)
	{
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), mouseRunLoopSource, kCFRunLoopCommonModes);
		CFRelease(mouseRunLoopSource);
	}
	if (mouseEventTap)
	{
		CFRelease(mouseEventTap);
	}
	mouseRunLoopSource = nullptr;
	mouseEventTap = nullptr;
}
#endif // !TARGET_OS_IPHONE

void Threading::Sleep(int ms)
{
	usleep(1000 * ms);
}

void Threading::SleepUntil(u64 ticks)
{
#if defined(__APPLE__) && TARGET_OS_IPHONE && defined(__aarch64__)
	// iOS: convert the remaining counter ticks and retry after interrupted sleeps.
	for (;;)
	{
		const s64 diff = static_cast<s64>(ticks - GetCPUTicks());
		if (diff <= 0)
			return;

		const u64 freq = GetTickFrequency();
		struct timespec ts;
		ts.tv_sec = static_cast<time_t>(static_cast<u64>(diff) / freq);
		ts.tv_nsec = static_cast<long>(((static_cast<u64>(diff) % freq) * 1000000000ULL) / freq);

		const int err = nanosleep(&ts, nullptr);
		if (err != 0 && errno != EINTR)
			return;
	}
#else
	// This is definitely sub-optimal, but apparently clock_nanosleep() doesn't exist.
	const s64 diff = static_cast<s64>(ticks - GetCPUTicks());
	if (diff <= 0)
		return;

	const u64 nanos = (static_cast<u64>(diff) * static_cast<u64>(s_timebase_info.denom)) / static_cast<u64>(s_timebase_info.numer);
	if (nanos == 0)
		return;

	struct timespec ts;
	ts.tv_sec = nanos / 1000000000ULL;
	ts.tv_nsec = nanos % 1000000000ULL;
	nanosleep(&ts, nullptr);
#endif
}

std::vector<DarwinMisc::CPUClass> DarwinMisc::GetCPUClasses()
{
	std::vector<CPUClass> out;

	if (std::optional<u32> nperflevels = sysctlbyname_T<u32>("hw.nperflevels"))
	{
		char name[64];
		for (u32 i = 0; i < *nperflevels; i++)
		{
			snprintf(name, sizeof(name), "hw.perflevel%u.physicalcpu", i);
			std::optional<u32> physicalcpu = sysctlbyname_T<u32>(name);
			snprintf(name, sizeof(name), "hw.perflevel%u.logicalcpu", i);
			std::optional<u32> logicalcpu = sysctlbyname_T<u32>(name);

			char levelname[64];
			size_t levelname_size = sizeof(levelname);
			snprintf(name, sizeof(name), "hw.perflevel%u.name", i);
			if (0 != sysctlbyname(name, levelname, &levelname_size, nullptr, 0))
				strcpy(levelname, "???");

			if (!physicalcpu.has_value() || !logicalcpu.has_value())
			{
				Console.Warning("(DarwinMisc) Perf level %u is missing data on %s cpus!",
					i, !physicalcpu.has_value() ? "physical" : "logical");
				continue;
			}

			out.push_back({levelname, *physicalcpu, *logicalcpu});
		}
	}
	else if (std::optional<u32> physcpu = sysctlbyname_T<u32>("hw.physicalcpu"))
	{
		out.push_back({"Default", *physcpu, sysctlbyname_T<u32>("hw.logicalcpu").value_or(*physcpu)});
	}
	else
	{
		Console.Warning("(DarwinMisc) Couldn't get cpu core count!");
	}

	return out;
}

static CPUInfo CalcCPUInfo()
{
	CPUInfo out;
	char name[256];
	size_t name_size = sizeof(name);
	if (0 != sysctlbyname("machdep.cpu.brand_string", name, &name_size, nullptr, 0))
		strcpy(name, "Unknown");
	out.name = name;
	if (sysctlbyname_T<u32>("sysctl.proc_translated").value_or(0))
		out.name += " (Rosetta)";
	std::vector<DarwinMisc::CPUClass> classes = DarwinMisc::GetCPUClasses();
	out.num_clusters = static_cast<u32>(classes.size());
	out.num_big_cores = classes.empty() ? 0 : classes[0].num_physical;
	out.num_threads   = classes.empty() ? 0 : classes[0].num_logical;
	out.num_small_cores = 0;
	for (std::size_t i = 1; i < classes.size(); i++)
	{
		out.num_small_cores += classes[i].num_physical;
		out.num_threads += classes[i].num_logical;
	}
	return out;
}

const CPUInfo& GetCPUInfo()
{
	static const CPUInfo info = CalcCPUInfo();
	return info;
}

size_t HostSys::GetRuntimePageSize()
{
	return sysctlbyname_T<u32>("hw.pagesize").value_or(0);
}

size_t HostSys::GetRuntimeCacheLineSize()
{
	return static_cast<size_t>(std::max<s64>(sysctlbyname_T<s64>("hw.cachelinesize").value_or(0), 0));
}

#ifdef ARCH_ARM64

#if TARGET_OS_IPHONE
static void* s_legacy_code_base = nullptr;
static size_t s_legacy_code_size = 0;
static bool s_legacy_is_writable = true;
static bool s_legacy_range_log_done = false;
#endif

static thread_local int s_code_write_depth = 0;
static thread_local int s_code_write_range_full_depth = 0;

#if TARGET_OS_IPHONE
static bool LegacyProtectCodeRange(void* address, size_t size, int prot, const char* tag)
{
	if (!s_legacy_code_base || !address || size == 0)
		return false;

	const uintptr_t base = reinterpret_cast<uintptr_t>(s_legacy_code_base);
	const uintptr_t limit = base + s_legacy_code_size;
	const uintptr_t start = reinterpret_cast<uintptr_t>(address);
	uintptr_t end = start + size;
	if (end < start || end > limit)
		end = limit;

	if (start < base || start >= limit || end <= start)
		return false;

	static const size_t page_size = []() {
		size_t detected_page_size = HostSys::GetRuntimePageSize();
		return detected_page_size ? detected_page_size : static_cast<size_t>(getpagesize());
	}();

	const uintptr_t page_mask = ~(static_cast<uintptr_t>(page_size) - 1);
	const uintptr_t aligned_start = start & page_mask;
	const uintptr_t aligned_end = (end + page_size - 1) & page_mask;
	if (aligned_end <= aligned_start)
		return false;

	if (mprotect(reinterpret_cast<void*>(aligned_start), aligned_end - aligned_start, prot) != 0)
	{
		static int s_range_fail_count = 0;
		if (s_range_fail_count++ < 8)
		{
			std::fprintf(stderr, "@@JIT_ALLOC@@ legacy_mprotect_%s_fail addr=%p size=0x%zx err=%d count=%d\n",
				tag, address, size, errno, s_range_fail_count);
			std::fflush(stderr);
		}
		return false;
	}

	if (!s_legacy_range_log_done)
	{
		std::fprintf(stderr, "@@JIT_ALLOC@@ legacy_range_wx_enabled page=0x%zx base=%p size=0x%zx\n",
			page_size, s_legacy_code_base, s_legacy_code_size);
		std::fflush(stderr);
		s_legacy_range_log_done = true;
	}

	return true;
}
#endif

void HostSys::BeginCodeWrite()
{
	if ((s_code_write_depth++) == 0)
	{
#if TARGET_OS_IPHONE
		if (DarwinMisc::g_code_rw_offset == 0 &&
			DarwinMisc::GetJitMode() == DarwinMisc::JitMode::Legacy && s_legacy_code_base)
		{
			if (!s_legacy_is_writable)
			{
				if (mprotect(s_legacy_code_base, s_legacy_code_size, PROT_READ | PROT_WRITE) != 0)
				{
					static int s_begin_fail_count = 0;
					if (s_begin_fail_count++ < 5)
						std::fprintf(stderr, "@@JIT_ALLOC@@ legacy_mprotect_rw_fail err=%d count=%d\n",
							errno, s_begin_fail_count);
				}
				s_legacy_is_writable = true;
			}
		}
		else if (DarwinMisc::g_code_rw_offset == 0)
		{
			static auto func = reinterpret_cast<void (*)(int)>(dlsym(RTLD_DEFAULT, "pthread_jit_write_protect_np"));
			if (func)
				func(0);
		}
#else
		pthread_jit_write_protect_np(0);
#endif
	}
}

void HostSys::EndCodeWrite()
{
	pxAssert(s_code_write_depth > 0);
	if ((--s_code_write_depth) == 0)
	{
#if TARGET_OS_IPHONE
		if (DarwinMisc::g_code_rw_offset == 0 &&
			DarwinMisc::GetJitMode() == DarwinMisc::JitMode::Legacy && s_legacy_code_base)
		{
			if (s_legacy_is_writable)
			{
				if (mprotect(s_legacy_code_base, s_legacy_code_size, PROT_READ | PROT_EXEC) != 0)
				{
					static int s_end_fail_count = 0;
					if (s_end_fail_count++ < 5)
						std::fprintf(stderr, "@@JIT_ALLOC@@ legacy_mprotect_rx_fail err=%d count=%d\n",
							errno, s_end_fail_count);
				}
				s_legacy_is_writable = false;
			}
		}
		else if (DarwinMisc::g_code_rw_offset == 0)
		{
			static auto func = reinterpret_cast<void (*)(int)>(dlsym(RTLD_DEFAULT, "pthread_jit_write_protect_np"));
			if (func)
				func(1);
		}
#else
		pthread_jit_write_protect_np(1);
#endif
	}
}

void HostSys::BeginCodeWriteRange(void* address, size_t size)
{
#if TARGET_OS_IPHONE
	if (DarwinMisc::g_code_rw_offset == 0 &&
		DarwinMisc::GetJitMode() == DarwinMisc::JitMode::Legacy && s_legacy_code_base &&
		LegacyProtectCodeRange(address, size, PROT_READ | PROT_WRITE, "range_rw"))
	{
		return;
	}
#endif
	s_code_write_range_full_depth++;
	BeginCodeWrite();
}

void HostSys::EndCodeWriteRange(void* address, size_t size)
{
	if (s_code_write_range_full_depth > 0)
	{
		s_code_write_range_full_depth--;
		EndCodeWrite();
		return;
	}

#if TARGET_OS_IPHONE
	if (DarwinMisc::g_code_rw_offset == 0 &&
		DarwinMisc::GetJitMode() == DarwinMisc::JitMode::Legacy && s_legacy_code_base)
	{
		LegacyProtectCodeRange(address, size, PROT_READ | PROT_EXEC, "range_rx");
	}
#endif
}

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

namespace
{
	static int s_user_crash_log_fd = -1;
	static uintptr_t s_jit_base = 0;
	static uintptr_t s_jit_end = 0;
	static u32 s_last_guest_pc = 0;
	static uintptr_t s_last_rec_ptr = 0;
	static DarwinMisc::JitMode s_jit_mode = DarwinMisc::JitMode::Simulator;
	static bool s_jit_mode_detected = false;
}

int DarwinMisc::iPSX2_CRASH_DIAG = 0;
int DarwinMisc::iPSX2_REC_DIAG = 0;
int DarwinMisc::iPSX2_FORCE_EE_INTERP = 0;
int DarwinMisc::iPSX2_FORCE_JIT_VERIFY = 0;
int DarwinMisc::iPSX2_CALL_TGT_X9 = 0;
int DarwinMisc::iPSX2_CRASH_PACK = 0;
int DarwinMisc::iPSX2_WX_TRACE = 0;
int DarwinMisc::iPSX2_CALLPROBE = 0;
int DarwinMisc::iPSX2_JIT_HLE = 1;
int DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_ONLY = 0;
int DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_LOADSTORE = 0;
int DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MMI = 0;
int DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_COP2_VU = 0;
int DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MULTDIV = 0;
int DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_SHIFTS = 0;
int DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_MOVES = 0;
int DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_INTEGER_ALU = 0;
int DarwinMisc::iPSX2_BISECT_COP1_EVERYTHING_PLUS_BRANCHES = 0;
volatile DarwinMisc::IndirectEvent DarwinMisc::g_ie[8] = {};
volatile u32 DarwinMisc::g_ie_idx = 0;
volatile DarwinMisc::WXTraceEvent DarwinMisc::g_wx_events[16] = {};
volatile u32 DarwinMisc::g_wx_idx = 0;
volatile DarwinMisc::EmitEvent DarwinMisc::g_emit_events[32] = {};
volatile u32 DarwinMisc::g_emit_idx = 0;
volatile int DarwinMisc::g_jit_write_state = 0;
volatile int DarwinMisc::g_rec_stage = 0;
ptrdiff_t DarwinMisc::g_code_rw_offset = 0;
uintptr_t DarwinMisc::g_code_rw_base = 0;
size_t DarwinMisc::g_code_rw_size = 0;

void DarwinMisc::SetCrashLogFD(int fd)
{
	s_user_crash_log_fd = fd;
}

void DarwinMisc::SetJitRange(void* base, size_t size)
{
	if (!base || size == 0)
		return; // interpreter-only mode: no code region
	s_jit_base = reinterpret_cast<uintptr_t>(base);
	s_jit_end = s_jit_base + size;
}

void DarwinMisc::SetLastGuestPC(u32 pc)
{
	s_last_guest_pc = pc;
}

void DarwinMisc::SetLastRecPtr(void* ptr)
{
	s_last_rec_ptr = reinterpret_cast<uintptr_t>(ptr);
}

uintptr_t DarwinMisc::GetJitBase()
{
	return s_jit_base;
}

uintptr_t DarwinMisc::GetJitEnd()
{
	return s_jit_end;
}

u32 DarwinMisc::GetLastGuestPC()
{
	return s_last_guest_pc;
}

uintptr_t DarwinMisc::GetLastRecPtr()
{
	return s_last_rec_ptr;
}

static const char* JitModeName(DarwinMisc::JitMode mode)
{
	switch (mode)
	{
		case DarwinMisc::JitMode::Simulator:
			return "Simulator";
		case DarwinMisc::JitMode::LuckTXM:
			return "LuckTXM";
		case DarwinMisc::JitMode::LuckNoTXM:
			return "LuckNoTXM";
		case DarwinMisc::JitMode::Legacy:
			return "Legacy";
		default:
			return "Unknown";
	}
}

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
static bool HasTXM()
{
	glob_t g = {};
	const int ret = glob("/System/Volumes/Preboot/*/boot/*/usr/standalone/firmware/FUD/Ap,TrustedExecutionMonitor.img4",
		GLOB_NOSORT, nullptr, &g);
	const bool found = (ret == 0 && g.gl_pathc > 0);
	globfree(&g);
	return found;
}
#endif

bool DarwinMisc::IsJITAvailable()
{
#if TARGET_OS_SIMULATOR
	return true;
#elif TARGET_OS_IPHONE
	u32 cs_flags = 0;
	const int rv = csops(getpid(), 0, &cs_flags, sizeof(cs_flags));
	const bool cs_debugged = (rv == 0) && ((cs_flags & 0x10000000u) != 0);
	std::fprintf(stderr, "@@JIT_DETECT@@ csops=%d cs_flags=0x%08x CS_DEBUGGED=%d\n",
		rv, cs_flags, cs_debugged ? 1 : 0);
	std::fflush(stderr);

	if (!cs_debugged)
	{
		s_jit_mode = JitMode::Legacy;
		s_jit_mode_detected = true;
		std::fprintf(stderr, "@@JIT_DETECT@@ result=UNAVAILABLE reason=no_cs_debugged\n");
		std::fflush(stderr);
		return false;
	}

	constexpr size_t probe_size = 16 * 1024;
	constexpr int probe_prot = PROT_READ | PROT_WRITE | PROT_EXEC;
	constexpr int probe_flags = MAP_PRIVATE | MAP_ANON | MAP_JIT;
	void* probe = mmap(nullptr, probe_size, probe_prot, probe_flags, -1, 0);
	if (probe == MAP_FAILED)
	{
		const int probe_errno = errno;
		if (!s_jit_mode_detected)
			DetectJitMode();
		std::fprintf(stderr,
			"@@JIT_DETECT@@ map_jit_probe=0 size=%zu flags=0x%x prot=0x%x err=%d cs_debugged=1 result=AVAILABLE mode=%s\n",
			probe_size, probe_flags, probe_prot, probe_errno, JitModeName(s_jit_mode));
		std::fflush(stderr);
		return true;
	}

	munmap(probe, probe_size);
	s_jit_mode = JitMode::Simulator;
	s_jit_mode_detected = true;
	std::fprintf(stderr,
		"@@JIT_DETECT@@ map_jit_probe=1 size=%zu flags=0x%x prot=0x%x cs_debugged=%d result=AVAILABLE mode=%s\n",
		probe_size, probe_flags, probe_prot, cs_debugged ? 1 : 0, JitModeName(s_jit_mode));
	std::fflush(stderr);
	return true;
#else
	return true;
#endif
}

bool DarwinMisc::ValidateJITAlive()
{
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
	// Check 1: CS_DEBUGGED still set?
	u32 cs_flags = 0;
	const int rv = csops(getpid(), 0, &cs_flags, sizeof(cs_flags));
	const bool cs_debugged = (rv == 0) && ((cs_flags & 0x10000000u) != 0);
	if (!cs_debugged)
	{
		std::fprintf(stderr, "@@JIT_KEEPALIVE@@ alive=0 reason=cs_debugged_revoked\n");
		std::fflush(stderr);
		return false;
	}

	// Check 2: RW alias still writable? Write a canary, read it back.
	if (g_code_rw_base != 0 && g_code_rw_size > 0)
	{
		volatile u8* canary = reinterpret_cast<volatile u8*>(g_code_rw_base);
		const u8 saved = *canary;
		*canary = 0x42;
		const u8 readback = *canary;
		*canary = saved; // restore so we don't corrupt the first code byte
		if (readback != 0x42)
		{
			std::fprintf(stderr, "@@JIT_KEEPALIVE@@ alive=0 reason=rw_alias_dead readback=0x%02x\n", readback);
			std::fflush(stderr);
			return false;
		}
	}

	std::fprintf(stderr, "@@JIT_KEEPALIVE@@ alive=1 cs_debugged=1 canary=ok\n");
	std::fflush(stderr);
	return true;
#else
	return true; // macOS and Simulator always have JIT
#endif
}

DarwinMisc::JitMode DarwinMisc::DetectJitMode()
{
#if TARGET_OS_SIMULATOR
	s_jit_mode = JitMode::Simulator;
#elif TARGET_OS_IPHONE
	char version[64] = {};
	size_t version_len = sizeof(version);
	if (sysctlbyname("kern.osproductversion", version, &version_len, nullptr, 0) != 0)
		std::snprintf(version, sizeof(version), "0");

	const int major = std::atoi(version);
	const bool has_txm = HasTXM();
	if (major >= 26)
		s_jit_mode = JitMode::LuckTXM;
	else
		s_jit_mode = JitMode::Legacy;

	if (const char* force_dual = std::getenv("ARMSX2_FORCE_DUAL_MAP"))
	{
		if (std::atoi(force_dual) == 1)
			s_jit_mode = JitMode::LuckNoTXM;
	}

	std::fprintf(stderr, "@@JIT_MODE@@ version=%s major=%d txm_probe=%d mode=%s (%d)\n",
		version, major, has_txm ? 1 : 0, JitModeName(s_jit_mode), static_cast<int>(s_jit_mode));
	std::fflush(stderr);
#else
	s_jit_mode = JitMode::Legacy;
#endif
	s_jit_mode_detected = true;
	return s_jit_mode;
}

DarwinMisc::JitMode DarwinMisc::GetJitMode()
{
	if (!s_jit_mode_detected)
		DetectJitMode();
	return s_jit_mode;
}

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
__attribute__((noinline, optnone))
static void JIT26PrepareRegion(void* addr, size_t len)
{
	asm volatile("mov x0, %0\n"
	             "mov x1, %1\n"
	             "mov x16, #1\n"
	             "brk #0xf00d\n"
	             :: "r"(addr), "r"(len) : "x0", "x1", "x16", "memory");
}

__attribute__((noinline, optnone))
static void JIT26Detach(void)
{
	asm volatile("mov x16, #0\n"
	             "brk #0xf00d\n"
	             ::: "x16", "memory");
}
#endif

void* DarwinMisc::MmapCodeDualMap(size_t size)
{
#if !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
	void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
	if (ptr == MAP_FAILED)
	{
		std::fprintf(stderr, "@@JIT_ALLOC@@ map_jit_fail size=0x%zx err=%d\n", size, errno);
		std::fflush(stderr);
		return nullptr;
	}

	g_code_rw_offset = 0;
	g_code_rw_base = reinterpret_cast<uintptr_t>(ptr);
	g_code_rw_size = size;
	std::fprintf(stderr, "@@JIT_ALLOC@@ map_jit_ok rx=%p rw=%p offset=0 size=0x%zx mode=%s\n",
		ptr, ptr, size, JitModeName(JitMode::Simulator));
	std::fflush(stderr);
	return ptr;
#else
	JitMode mode = GetJitMode();

	void* jit_ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
	if (jit_ptr != MAP_FAILED)
	{
		s_jit_mode = JitMode::Simulator;
		s_jit_mode_detected = true;
		g_code_rw_offset = 0;
		g_code_rw_base = reinterpret_cast<uintptr_t>(jit_ptr);
		g_code_rw_size = size;
		std::fprintf(stderr, "@@JIT_ALLOC@@ map_jit_ok rx=%p rw=%p offset=0 size=0x%zx mode=%s\n",
			jit_ptr, jit_ptr, size, JitModeName(s_jit_mode));
		std::fflush(stderr);
		return jit_ptr;
	}

	const int map_jit_errno = errno;
	if (mode == JitMode::Simulator)
		mode = DetectJitMode();
	std::fprintf(stderr, "@@JIT_ALLOC@@ map_jit_fail size=0x%zx err=%d selected_mode=%s\n",
		size, map_jit_errno, JitModeName(mode));
	std::fflush(stderr);

	if (mode == JitMode::Legacy)
	{
		void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		if (ptr == MAP_FAILED)
		{
			std::fprintf(stderr, "@@JIT_ALLOC@@ legacy_rw_fail size=0x%zx err=%d\n", size, errno);
			std::fflush(stderr);
			return nullptr;
		}

		g_code_rw_offset = 0;
		g_code_rw_base = reinterpret_cast<uintptr_t>(ptr);
		g_code_rw_size = size;
		s_legacy_code_base = ptr;
		s_legacy_code_size = size;
		s_legacy_is_writable = true;
		s_legacy_range_log_done = false;

		std::fprintf(stderr, "@@JIT_ALLOC@@ legacy_wx_toggle_ok rx=%p rw=%p offset=0 size=0x%zx\n",
			ptr, ptr, size);
		std::fflush(stderr);
		return ptr;
	}

	void* rx_ptr = mmap(nullptr, size, PROT_READ | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (rx_ptr == MAP_FAILED)
	{
		std::fprintf(stderr, "@@JIT_ALLOC@@ dualmap_rx_fail size=0x%zx err=%d\n", size, errno);
		std::fflush(stderr);
		return nullptr;
	}
	std::fprintf(stderr, "@@JIT_ALLOC@@ dualmap_rx_ok rx=%p size=0x%zx mode=%s\n",
		rx_ptr, size, JitModeName(mode));
	std::fflush(stderr);

	if (mode == JitMode::LuckTXM)
	{
		static thread_local sigjmp_buf s_alloc_brk_jmp;
		struct sigaction sa_brk = {};
		struct sigaction sa_brk_old = {};
		sa_brk.sa_handler = +[](int) { siglongjmp(s_alloc_brk_jmp, 1); };
		sigemptyset(&sa_brk.sa_mask);
		sigaction(SIGTRAP, &sa_brk, &sa_brk_old);

		const bool useLegacy = []() {
			const char* proto = std::getenv("ARMSX2_JIT_PROTOCOL");
			return proto && std::strcmp(proto, "legacy") == 0;
		}();

		std::fprintf(stderr, "@@JIT_ALLOC@@ txm_protocol=%s\n", useLegacy ? "legacy" : "universal");
		std::fflush(stderr);

		bool brk_ok = false;
		if (useLegacy)
		{
			std::fprintf(stderr, "@@JIT_ALLOC@@ txm_register_begin rx=%p size=0x%zx\n", rx_ptr, size);
			std::fflush(stderr);
			if (sigsetjmp(s_alloc_brk_jmp, 1) == 0)
			{
				asm volatile("mov x0, %0\n"
				             "mov x1, %1\n"
				             "brk #0x69"
				             :: "r"(rx_ptr), "r"(size) : "x0", "x1");
				brk_ok = true;
				std::fprintf(stderr, "@@JIT_ALLOC@@ txm_register_ok\n");
				std::fflush(stderr);
			}
			else
			{
				std::fprintf(stderr, "@@JIT_ALLOC@@ txm_register_sigtrap\n");
				std::fflush(stderr);
			}
		}
		else // universal protocol
		{
			// Run TXM prepare on a worker thread with an 8-second timeout.
			// The brk #0xf00d instruction can hang on large code regions; if it
			// doesn't complete in 8 seconds, fall back to the Legacy brk #0x69 path.
			std::atomic<bool> universal_done{false};
			std::atomic<bool> universal_ok{false};

			std::fprintf(stderr, "@@JIT_ALLOC@@ txm_register_universal_begin rx=%p size=0x%zx\n", rx_ptr, size);
			std::fflush(stderr);

			std::thread txm_worker([&]() {
				if (sigsetjmp(s_alloc_brk_jmp, 1) == 0)
				{
					JIT26PrepareRegion(rx_ptr, size);
					std::fprintf(stderr, "@@JIT_ALLOC@@ txm_register_universal_ok\n");
					std::fflush(stderr);

					std::fprintf(stderr, "@@JIT_ALLOC@@ txm_detach_begin\n");
					std::fflush(stderr);
					if (sigsetjmp(s_alloc_brk_jmp, 1) == 0)
					{
						JIT26Detach();
						universal_ok.store(true);
						std::fprintf(stderr, "@@JIT_ALLOC@@ txm_detach_ok\n");
						std::fflush(stderr);
					}
					else
					{
						std::fprintf(stderr, "@@JIT_ALLOC@@ txm_detach_sigtrap\n");
						std::fflush(stderr);
					}
				}
				else
				{
					std::fprintf(stderr, "@@JIT_ALLOC@@ txm_register_universal_sigtrap\n");
					std::fflush(stderr);
				}
				universal_done.store(true);
			});
			txm_worker.detach();

			// Wait up to 8 seconds
			for (int i = 0; i < 80; i++)
			{
				if (universal_done.load(std::memory_order_relaxed))
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			if (universal_done.load() && universal_ok.load())
			{
				brk_ok = true;
			}
			else if (!universal_done.load())
			{
				// Worker hung — try Legacy fallback
				std::fprintf(stderr, "@@JIT_ALLOC@@ txm_universal_timeout — falling back to legacy brk #0x69\n");
				std::fflush(stderr);
				// The worker thread is still running (hung). It's detached so it won't
				// block shutdown, but the SIGTRAP handler is still installed. Try Legacy.
				std::fprintf(stderr, "@@JIT_ALLOC@@ txm_register_begin rx=%p size=0x%zx\n", rx_ptr, size);
				std::fflush(stderr);
				if (sigsetjmp(s_alloc_brk_jmp, 1) == 0)
				{
					asm volatile("mov x0, %0\n"
					             "mov x1, %1\n"
					             "brk #0x69"
					             :: "r"(rx_ptr), "r"(size) : "x0", "x1");
					brk_ok = true;
					std::fprintf(stderr, "@@JIT_ALLOC@@ txm_register_ok\n");
					std::fflush(stderr);
				}
				else
				{
					std::fprintf(stderr, "@@JIT_ALLOC@@ txm_register_sigtrap\n");
					std::fflush(stderr);
				}
			}
			// else: universal completed but failed (sigtrap) — brk_ok stays false
		}
		// NOTE: If the Universal TXM worker (detached, possibly hung) traps late
		// after the Legacy fallback, it may hit the handler after restoration.
		// This race is bounded: it only occurs with Universal protocol + hang +
		// late trap. ARMSX2_JIT_PROTOCOL=legacy avoids the Universal path entirely.
		sigaction(SIGTRAP, &sa_brk_old, nullptr);

		if (!brk_ok)
		{
			munmap(rx_ptr, size);
			return nullptr;
		}
	}

	vm_address_t rw_region = 0;
	vm_address_t target = reinterpret_cast<vm_address_t>(rx_ptr);
	vm_prot_t cur_protection = 0;
	vm_prot_t max_protection = 0;
	const kern_return_t kr = vm_remap(mach_task_self(), &rw_region, static_cast<vm_size_t>(size), 0,
		VM_FLAGS_ANYWHERE, mach_task_self(), target, false, &cur_protection, &max_protection, VM_INHERIT_DEFAULT);
	if (kr != KERN_SUCCESS)
	{
		std::fprintf(stderr, "@@JIT_ALLOC@@ dualmap_remap_fail kr=%d\n", kr);
		std::fflush(stderr);
		munmap(rx_ptr, size);
		return nullptr;
	}

	u8* const rw_ptr = reinterpret_cast<u8*>(rw_region);
	if (mprotect(rw_ptr, size, PROT_READ | PROT_WRITE) != 0)
	{
		std::fprintf(stderr, "@@JIT_ALLOC@@ dualmap_rw_protect_fail err=%d\n", errno);
		std::fflush(stderr);
		vm_deallocate(mach_task_self(), rw_region, static_cast<vm_size_t>(size));
		munmap(rx_ptr, size);
		return nullptr;
	}

	g_code_rw_offset = rw_ptr - static_cast<u8*>(rx_ptr);
	g_code_rw_base = reinterpret_cast<uintptr_t>(rw_ptr);
	g_code_rw_size = size;
	std::fprintf(stderr, "@@JIT_ALLOC@@ dualmap_ok rx=%p rw=%p offset=%td size=0x%zx mode=%s\n",
		rx_ptr, rw_ptr, g_code_rw_offset, size, JitModeName(mode));
	std::fflush(stderr);
	return rx_ptr;
#endif
}

void DarwinMisc::MunmapCodeDualMap(void* rx_ptr, size_t size)
{
	if (g_code_rw_base && g_code_rw_offset != 0)
		vm_deallocate(mach_task_self(), static_cast<vm_address_t>(g_code_rw_base), static_cast<vm_size_t>(g_code_rw_size));
	if (rx_ptr)
		munmap(rx_ptr, size);
	g_code_rw_offset = 0;
	g_code_rw_base = 0;
	g_code_rw_size = 0;
#if TARGET_OS_IPHONE
	s_legacy_code_base = nullptr;
	s_legacy_code_size = 0;
	s_legacy_is_writable = true;
	s_legacy_range_log_done = false;
#endif
}

void DarwinMisc::LegacyEnsureExecutable()
{
#if TARGET_OS_IPHONE
	if (!s_legacy_code_base || !s_legacy_is_writable)
		return;

	if (mprotect(s_legacy_code_base, s_legacy_code_size, PROT_READ | PROT_EXEC) != 0)
	{
		std::fprintf(stderr, "@@JIT_ALLOC@@ legacy_ensure_rx_fail err=%d\n", errno);
		std::fflush(stderr);
		return;
	}
	s_legacy_is_writable = false;
#endif
}
void DarwinMisc::LogDyldMain() {}
void DarwinMisc::RecordJitBlock(u32 guest_pc, void* recptr, u32 size) {}
bool DarwinMisc::FindJitBlock(uintptr_t site, u32* out_guest_pc, void** out_recptr) { return false; }

#define USE_MACH_EXCEPTION_PORTS

namespace PageFaultHandler
{
#ifdef USE_MACH_EXCEPTION_PORTS
	static void SignalHandler(mach_port_t port);
	static mach_port_t s_port = 0;
#else
	static void SignalHandler(int sig, siginfo_t* info, void* ctx);
#endif

	static std::recursive_mutex s_exception_handler_mutex;
	static bool s_in_exception_handler = false;
	static bool s_installed = false;
} // namespace PageFaultHandler

#ifdef USE_MACH_EXCEPTION_PORTS

#if defined(ARCH_X86)
#define THREAD_STATE64_COUNT x86_THREAD_STATE64_COUNT
#define THREAD_STATE64 x86_THREAD_STATE64
#define thread_state64_t x86_thread_state64_t
#elif defined(ARCH_ARM64)
#define THREAD_STATE64_COUNT ARM_THREAD_STATE64_COUNT
#define THREAD_STATE64 ARM_THREAD_STATE64
#define thread_state64_t arm_thread_state64_t
#else
#error Unknown Darwin Platform
#endif

void PageFaultHandler::SignalHandler(mach_port_t port)
{
	Threading::SetNameOfCurrentThread("Mach Exception Thread");

#pragma pack(4)
	struct
	{
		mach_msg_header_t Head;
		NDR_record_t NDR;
		exception_type_t exception;
		mach_msg_type_number_t codeCnt;
		int64_t code[2];
		int flavor;
		mach_msg_type_number_t old_stateCnt;
		natural_t old_state[THREAD_STATE64_COUNT];
		mach_msg_trailer_t trailer;
	} msg_in;

	struct
	{
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
		int flavor;
		mach_msg_type_number_t new_stateCnt;
		natural_t new_state[THREAD_STATE64_COUNT];
	} msg_out;
#pragma pack()
	memset(&msg_in, 0xee, sizeof(msg_in));
	memset(&msg_out, 0xee, sizeof(msg_out));
	mach_msg_size_t send_size = 0;
	mach_msg_option_t option = MACH_RCV_MSG;
	while (true)
	{
		kern_return_t r;
		if ((r = mach_msg_overwrite(&msg_out.Head, option, send_size, sizeof(msg_in), port,
				 MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL, &msg_in.Head, 0)))
		{
			pxFail(fmt::format("CRITICAL: mach_msg_overwrite: {:x}", r).c_str());
		}

		if (msg_in.Head.msgh_id == MACH_NOTIFY_NO_SENDERS)
		{
			// the other thread exited
			mach_port_deallocate(mach_task_self(), port);
			return;
		}

		if (msg_in.Head.msgh_id != 2406)
		{
			pxFailRel("unknown message received");
			return;
		}

		if (msg_in.flavor != THREAD_STATE64)
		{
			pxFailRel(fmt::format("unknown flavour {}, expected {}", msg_in.flavor, THREAD_STATE64).c_str());
			return;
		}

		thread_state64_t* state = (thread_state64_t*)msg_in.old_state;

		HandlerResult result = HandlerResult::ExecuteNextHandler;
		if (!s_in_exception_handler)
		{
			s_in_exception_handler = true;

#ifdef ARCH_ARM64
			result = HandlePageFault(reinterpret_cast<void*>(state->__pc), reinterpret_cast<void*>(msg_in.code[1]), (msg_in.code[0] & 2) != 0);
#else
			result = HandlePageFault(reinterpret_cast<void*>(state->__rip), reinterpret_cast<void*>(msg_in.code[1]), (msg_in.code[0] & 2) != 0);
#endif
			s_in_exception_handler = false;
		}

		// Set up the reply.
		msg_out.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(msg_in.Head.msgh_bits), 0);
		msg_out.Head.msgh_remote_port = msg_in.Head.msgh_remote_port;
		msg_out.Head.msgh_local_port = MACH_PORT_NULL;
		msg_out.Head.msgh_id = msg_in.Head.msgh_id + 100;
		msg_out.NDR = msg_in.NDR;

		if (result != HandlerResult::ContinueExecution) // cooked
		{
			// Continue to the next exception handler (debugger or crash)
			msg_out.RetCode = KERN_FAILURE;
			msg_out.flavor = 0;
			msg_out.new_stateCnt = 0;
		}
		else
		{
			// Resumes execution right where we left off (re-executes instruction that caused the SIGSEGV)
			msg_out.RetCode = KERN_SUCCESS;
			msg_out.flavor = THREAD_STATE64;
			msg_out.new_stateCnt = THREAD_STATE64_COUNT;
			memcpy(msg_out.new_state, msg_in.old_state, THREAD_STATE64_COUNT * sizeof(natural_t));
		}

		msg_out.Head.msgh_size =
			offsetof(__typeof__(msg_out), new_state) + msg_out.new_stateCnt * sizeof(natural_t);
		send_size = msg_out.Head.msgh_size;
		option |= MACH_SEND_MSG;
	}
}

bool PageFaultHandler::Install(Error* error)
{
	exception_mask_t masks[EXC_TYPES_COUNT];
	mach_port_t ports[EXC_TYPES_COUNT];
	exception_behavior_t behaviors[EXC_TYPES_COUNT];
	thread_state_flavor_t flavors[EXC_TYPES_COUNT];
	mach_msg_type_number_t count = EXC_TYPES_COUNT;

	kern_return_t r = task_get_exception_ports(mach_task_self(), EXC_MASK_ALL,
		masks, &count, ports, behaviors, flavors);

	mach_port_t port;
	if ((r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port)))
	{
		pxFailRel(fmt::format("mach_port_allocate: {:x}", r).c_str());
		return false;
	}

	std::thread sig_thread(PageFaultHandler::SignalHandler, port);
	sig_thread.detach();

	if ((r = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND)))
	{
		mach_port_deallocate(mach_task_self(), port);
		pxFailRel(fmt::format("mach_port_insert_right: {:x}", r).c_str());
		return false;
	}

	task_set_exception_ports(mach_task_self(), EXC_MASK_BAD_ACCESS, MACH_PORT_NULL, EXCEPTION_DEFAULT, THREAD_STATE_NONE);

	if ((r = thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, port, EXCEPTION_STATE | MACH_EXCEPTION_CODES, THREAD_STATE64)))
	{
		mach_port_deallocate(mach_task_self(), port);
		pxFailRel(fmt::format("thread_set_exception_ports: {:x}", r).c_str());
		return false;
	}

	mach_port_t previous;
	if ((r = mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_NO_SENDERS, 0, port, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous)))
	{
		mach_port_deallocate(mach_task_self(), port);
		pxFailRel(fmt::format("mach_port_request_notification: {:x}", r).c_str());
		return false;
	}

	s_installed = true;
	s_port = port;
	return true;
}

bool PageFaultHandler::InstallSecondaryThread()
{
	kern_return_t r = thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, s_port, EXCEPTION_STATE | MACH_EXCEPTION_CODES, THREAD_STATE64);
	if (r)
	{
		pxFailRel(fmt::format("thread_set_exception_ports(secondary): {:x}", r).c_str());
		return false;
	}
	return true;
}

#else

void PageFaultHandler::SignalHandler(int sig, siginfo_t* info, void* ctx)
{
#if defined(ARCH_X86)
	void* const exception_address =
		reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__es.__faultvaddr);
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__ss.__rip);
	const bool is_write = (static_cast<ucontext_t*>(ctx)->uc_mcontext->__es.__err & 2) != 0;
#elif defined(ARCH_ARM64)
	void* const exception_address = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__es.__far);
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__ss.__pc);
	const bool is_write = IsStoreInstruction(exception_pc);
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
	pxAssertRel(!s_installed, "Page fault handler has already been installed.");

	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = SignalHandler;

	// MacOS uses SIGBUS for memory permission violations, as well as SIGSEGV on ARM64.
	if (sigaction(SIGBUS, &sa, nullptr) != 0)
	{
		Error::SetErrno(error, "sigaction() for SIGBUS failed: ", errno);
		return false;
	}

#ifdef ARCH_ARM64
	if (sigaction(SIGSEGV, &sa, nullptr) != 0)
	{
		Error::SetErrno(error, "sigaction() for SIGSEGV failed: ", errno);
		return false;
	}
#endif

	// Allow us to ignore faults when running under lldb.
	task_set_exception_ports(mach_task_self(), EXC_MASK_BAD_ACCESS, MACH_PORT_NULL, EXCEPTION_DEFAULT, 0);

	s_installed = true;
	return true;
}

bool PageFaultHandler::InstallSecondaryThread() { return true; }
#endif
