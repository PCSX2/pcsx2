// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
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

#include <csignal>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <sys/sysctl.h>
#include <time.h>
#include <mach/mach_time.h>
#include <mach/task.h>
#include <mutex>
#include <IOKit/pwr_mgt/IOPMLib.h>

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
	return tickfreq;
}

// return the number of "ticks" since some arbitrary, fixed time in the
// past. On OSX x86(-64), this is actually the number of nanoseconds passed,
// because mach_timebase_info.numer == denom == 1. So "ticks" ==
// nanoseconds.
u64 GetCPUTicks()
{
	return mach_absolute_time();
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

static IOPMAssertionID s_pm_assertion;

bool Common::InhibitScreensaver(bool inhibit)
{
	if (s_pm_assertion)
	{
		IOPMAssertionRelease(s_pm_assertion);
		s_pm_assertion = 0;
	}

	if (inhibit)
		IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn, CFSTR("Playing a game"), &s_pm_assertion);

	return true;
}

void Threading::Sleep(int ms)
{
	usleep(1000 * ms);
}

void Threading::SleepUntil(u64 ticks)
{
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
		out.push_back({"Default", *physcpu, sysctlbyname_T<u32>("hw.logicalcpu").value_or(0)});
	}
	else
	{
		Console.Warning("(DarwinMisc) Couldn't get cpu core count!");
	}

	return out;
}

size_t HostSys::GetRuntimePageSize()
{
	return sysctlbyname_T<u32>("hw.pagesize").value_or(0);
}

size_t HostSys::GetRuntimeCacheLineSize()
{
	return static_cast<size_t>(std::max<s64>(sysctlbyname_T<s64>("hw.cachelinesize").value_or(0), 0));
}

#ifdef _M_ARM64

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

#endif // _M_ARM64

namespace PageFaultHandler
{
	static void SignalHandler(int sig, siginfo_t* info, void* ctx);

	static std::recursive_mutex s_exception_handler_mutex;
	static bool s_in_exception_handler = false;
	static bool s_installed = false;
} // namespace PageFaultHandler

void PageFaultHandler::SignalHandler(int sig, siginfo_t* info, void* ctx)
{
#if defined(_M_X86)
	void* const exception_address =
		reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__es.__faultvaddr);
	void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__ss.__rip);
	const bool is_write = (static_cast<ucontext_t*>(ctx)->uc_mcontext->__es.__err & 2) != 0;
#elif defined(_M_ARM64)
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

#ifdef _M_ARM64
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
