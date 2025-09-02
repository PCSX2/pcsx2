// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
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

#include <csignal>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <thread>
#include <time.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>
#include <mach/message.h>
#include <mach/task.h>
#include <mach/thread_state.h>
#include <mach/vm_map.h>
#include <mutex>
#include <ApplicationServices/ApplicationServices.h>
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

static __ri vm_prot_t MachProt(const PageProtectionMode& mode)
{
	vm_prot_t machmode = (mode.CanWrite()) ? VM_PROT_WRITE : 0;
	machmode |= (mode.CanRead()) ? VM_PROT_READ : 0;
	machmode |= (mode.CanExecute()) ? (VM_PROT_EXECUTE | VM_PROT_READ) : 0;
	return machmode;
}

void* HostSys::Mmap(void* base, size_t size, const PageProtectionMode& mode)
{
	pxAssertMsg((size & (__pagesize - 1)) == 0, "Size is page aligned");
	if (mode.IsNone())
		return nullptr;

#ifdef __aarch64__
	// We can't allocate executable memory with mach_vm_allocate() on Apple Silicon.
	// Instead, we need to use MAP_JIT with mmap(), which does not support fixed mappings.
	if (mode.CanExecute())
	{
		if (base)
			return nullptr;

		const u32 mmap_prot = mode.CanWrite() ? (PROT_READ | PROT_WRITE | PROT_EXEC) : (PROT_READ | PROT_EXEC);
		const u32 flags = MAP_PRIVATE | MAP_ANON | MAP_JIT;
		void* const res = mmap(nullptr, size, mmap_prot, flags, -1, 0);
		return (res == MAP_FAILED) ? nullptr : res;
	}
#endif

	kern_return_t ret = mach_vm_allocate(mach_task_self(), reinterpret_cast<mach_vm_address_t*>(&base), size,
		base ? VM_FLAGS_FIXED : VM_FLAGS_ANYWHERE);
	if (ret != KERN_SUCCESS)
	{
		DEV_LOG("mach_vm_allocate() returned {}", ret);
		return nullptr;
	}

	ret = mach_vm_protect(mach_task_self(), reinterpret_cast<mach_vm_address_t>(base), size, false, MachProt(mode));
	if (ret != KERN_SUCCESS)
	{
		DEV_LOG("mach_vm_protect() returned {}", ret);
		mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(base), size);
		return nullptr;
	}

	return base;
}

void HostSys::Munmap(void* base, size_t size)
{
	if (!base)
		return;

	mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(base), size);
}

void HostSys::MemProtect(void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	pxAssertMsg((size & (__pagesize - 1)) == 0, "Size is page aligned");

	kern_return_t res = mach_vm_protect(mach_task_self(), reinterpret_cast<mach_vm_address_t>(baseaddr), size, false,
		MachProt(mode));
	if (res != KERN_SUCCESS) [[unlikely]]
	{
		ERROR_LOG("mach_vm_protect() failed: {}", res);
		pxFailRel("mach_vm_protect() failed");
	}
}

std::string HostSys::GetFileMappingName(const char* prefix)
{
	// name actually is not used.
	return {};
}

void* HostSys::CreateSharedMemory(const char* name, size_t size)
{
	mach_vm_size_t vm_size = size;
	mach_port_t port;
	const kern_return_t res = mach_make_memory_entry_64(
		mach_task_self(), &vm_size, 0, MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE, &port, MACH_PORT_NULL);
	if (res != KERN_SUCCESS)
	{
		ERROR_LOG("mach_make_memory_entry_64() failed: {}", res);
		return nullptr;
	}

	return reinterpret_cast<void*>(static_cast<uintptr_t>(port));
}

void HostSys::DestroySharedMemory(void* ptr)
{
	mach_port_deallocate(mach_task_self(), static_cast<mach_port_t>(reinterpret_cast<uintptr_t>(ptr)));
}

void* HostSys::MapSharedMemory(void* handle, size_t offset, void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	mach_vm_address_t ptr = reinterpret_cast<mach_vm_address_t>(baseaddr);
	const kern_return_t res = mach_vm_map(mach_task_self(), &ptr, size, 0, baseaddr ? VM_FLAGS_FIXED : VM_FLAGS_ANYWHERE,
		static_cast<mach_port_t>(reinterpret_cast<uintptr_t>(handle)), offset, FALSE,
		MachProt(mode), VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
	if (res != KERN_SUCCESS)
	{
		ERROR_LOG("mach_vm_map() failed: {}", res);
		return nullptr;
	}

	return reinterpret_cast<void*>(ptr);
}

void HostSys::UnmapSharedMemory(void* baseaddr, size_t size)
{
	const kern_return_t res = mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(baseaddr), size);
	if (res != KERN_SUCCESS)
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

	if (mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(m_base_ptr), m_size) != KERN_SUCCESS)
		pxFailRel("Failed to release shared memory area");
}


std::unique_ptr<SharedMemoryMappingArea> SharedMemoryMappingArea::Create(size_t size)
{
	pxAssertRel(Common::IsAlignedPow2(size, __pagesize), "Size is page aligned");

	mach_vm_address_t alloc = 0;
	const kern_return_t res =
		mach_vm_map(mach_task_self(), &alloc, size, 0, VM_FLAGS_ANYWHERE,
			MEMORY_OBJECT_NULL, 0, false, VM_PROT_NONE, VM_PROT_NONE, VM_INHERIT_NONE);
	if (res != KERN_SUCCESS)
	{
		ERROR_LOG("mach_vm_map() failed: {}", res);
		return {};
	}

	return std::unique_ptr<SharedMemoryMappingArea>(new SharedMemoryMappingArea(reinterpret_cast<u8*>(alloc), size, size / __pagesize));
}

u8* SharedMemoryMappingArea::Map(void* file_handle, size_t file_offset, void* map_base, size_t map_size, const PageProtectionMode& mode)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	const kern_return_t res =
		mach_vm_map(mach_task_self(), reinterpret_cast<mach_vm_address_t*>(&map_base), map_size, 0, VM_FLAGS_OVERWRITE,
			static_cast<mach_port_t>(reinterpret_cast<uintptr_t>(file_handle)), file_offset, false,
			MachProt(mode), VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
	if (res != KERN_SUCCESS) [[unlikely]]
	{
		ERROR_LOG("mach_vm_map() failed: {}", res);
		return nullptr;
	}

	m_num_mappings++;
	return static_cast<u8*>(map_base);
}

bool SharedMemoryMappingArea::Unmap(void* map_base, size_t map_size)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	const kern_return_t res =
		mach_vm_map(mach_task_self(), reinterpret_cast<mach_vm_address_t*>(&map_base), map_size, 0, VM_FLAGS_OVERWRITE,
			MEMORY_OBJECT_NULL, 0, false, VM_PROT_NONE, VM_PROT_NONE, VM_INHERIT_NONE);
	if (res != KERN_SUCCESS) [[unlikely]]
	{
		ERROR_LOG("mach_vm_map() failed: {}", res);
		return false;
	}

	m_num_mappings--;
	return true;
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

#define HANDLE_MACH_EXCEPTIONS

namespace PageFaultHandler
{
#ifdef HANDLE_MACH_EXCEPTIONS
	static void SignalHandler(mach_port_t port);
#else
	static void SignalHandler(int sig, siginfo_t* info, void* ctx);
#endif

	static std::recursive_mutex s_exception_handler_mutex;
	static bool s_in_exception_handler = false;
	static bool s_installed = false;
} // namespace PageFaultHandler

#ifdef HANDLE_MACH_EXCEPTIONS

#if defined(_M_X86)
#define THREAD_STATE64_COUNT x86_THREAD_STATE64_COUNT
#define THREAD_STATE64 x86_THREAD_STATE64
#define thread_state64_t x86_thread_state64_t
#elif defined(_M_ARM64)
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
		if((r = mach_msg_overwrite(&msg_out.Head, option, send_size, sizeof(msg_in), port,
								   MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL, &msg_in.Head, 0)))
		{
			pxFail(fmt::format("CRITICAL: mach_msg_overwrite: {:x}", r).c_str());
		}

		if (msg_in.Head.msgh_id == MACH_NOTIFY_NO_SENDERS)
		{
			// the other thread exited
			mach_port_destroy(mach_task_self(), port);
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

		s_exception_handler_mutex.lock();
		thread_state64_t* state = (thread_state64_t*)msg_in.old_state;

		HandlerResult result = HandlerResult::ExecuteNextHandler;
		if (!s_in_exception_handler)
		{
			s_in_exception_handler = true;
			result = HandlePageFault((void*)state->__rip, (void*)msg_in.code[1], (msg_in.code[0] & 2) != 0);
			//result = HandlePageFault((void*)state->__pc, (void*)msg_in.code[1], (msg_in.code[0] & 2) != 0); ARM64
			s_in_exception_handler = false;
		}

		// Set up the reply.
		msg_out.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(msg_in.Head.msgh_bits), 0);
		msg_out.Head.msgh_remote_port = msg_in.Head.msgh_remote_port;
		msg_out.Head.msgh_local_port = MACH_PORT_NULL;
		msg_out.Head.msgh_id = msg_in.Head.msgh_id + 100;
		msg_out.NDR = msg_in.NDR;
		
		// Resumes execution right where we left off (re-executes instruction that caused the SIGSEGV).
		if (result == HandlerResult::ContinueExecution)
		{
			msg_out.RetCode = KERN_SUCCESS;
			msg_out.flavor = THREAD_STATE64;
			msg_out.new_stateCnt = THREAD_STATE64_COUNT;
			memcpy(msg_out.new_state, msg_in.old_state, THREAD_STATE64_COUNT * sizeof(natural_t));
		}
		else // cooked
		{
			msg_out.RetCode = KERN_FAILURE;
			msg_out.flavor = 0;
			msg_out.new_stateCnt = 0;
		}

		// We couldn't handle it. Pass it off to the crash dumper.
		//CrashHandler::CrashSignalHandler(sig, info, ctx);

		msg_out.Head.msgh_size =
			offsetof(__typeof__(msg_out), new_state) + msg_out.new_stateCnt * sizeof(natural_t);

		send_size = msg_out.Head.msgh_size;
		option |= MACH_SEND_MSG;
		
		s_exception_handler_mutex.unlock();
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

	for (mach_msg_type_number_t i = 0; i < count; i++) {
		Console.Warning("##### Port %d: %d (behavior: %d, flavor: %d)\n", i, ports[i], behaviors[i], flavors[i]);
	}

	mach_port_t port;
	if((r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port)))
	{
		pxFailRel(fmt::format("mach_port_allocate: {:x}", r).c_str());
		return false;
	}
	
	std::thread sig_thread(PageFaultHandler::SignalHandler, port);
	sig_thread.detach();
	
	if((r = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND)))
	{
		pxFailRel(fmt::format("mach_port_insert_right: {:x}", r).c_str());
		return false;
	}

	task_set_exception_ports(mach_task_self(), EXC_MASK_BAD_ACCESS, MACH_PORT_NULL, EXCEPTION_DEFAULT, THREAD_STATE_NONE);

	if((r = thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, port, EXCEPTION_STATE | MACH_EXCEPTION_CODES, THREAD_STATE64)))
	{
		pxFailRel(fmt::format("thread_set_exception_ports: {:x}", r).c_str());
		return false;
	}

	if((r = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, -1)))
	{
		pxFailRel(fmt::format("mach_port_mod_refs: {:x}", r).c_str());
		return false;
	}

	mach_port_t previous;
	mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_NO_SENDERS, 0, port, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);

	s_installed = true;
	return true;
}

#else
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
#endif
