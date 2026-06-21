// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "HostSys.h"
#include "Console.h"
#include "VectorIntrin.h"
#include "fmt/format.h"

#ifndef __APPLE__
#include "cpuinfo.h"
#endif

static u32 PAUSE_TIME = 0;

static void MultiPause()
{
#ifdef ARCH_X86
	_mm_pause();
	_mm_pause();
	_mm_pause();
	_mm_pause();
	_mm_pause();
	_mm_pause();
	_mm_pause();
	_mm_pause();
#elif defined(ARCH_ARM64) && defined(_MSC_VER)
	__isb(_ARM64_BARRIER_SY);
	__isb(_ARM64_BARRIER_SY);
	__isb(_ARM64_BARRIER_SY);
	__isb(_ARM64_BARRIER_SY);
	__isb(_ARM64_BARRIER_SY);
	__isb(_ARM64_BARRIER_SY);
	__isb(_ARM64_BARRIER_SY);
	__isb(_ARM64_BARRIER_SY);
#elif defined(ARCH_ARM64)
	__asm__ __volatile__("isb");
	__asm__ __volatile__("isb");
	__asm__ __volatile__("isb");
	__asm__ __volatile__("isb");
	__asm__ __volatile__("isb");
	__asm__ __volatile__("isb");
	__asm__ __volatile__("isb");
	__asm__ __volatile__("isb");
#else
#error Unknown architecture.
#endif
}

static u32 MeasurePauseTime()
{
	// GetCPUTicks may have resolution as low as 1µs
	// One call to MultiPause could take anywhere from 20ns (fast Haswell) to 400ns (slow Skylake)
	// We want a measurement of reasonable resolution, but don't want to take too long
	// So start at a fairly small number and increase it if it's too fast
	for (int testcnt = 64; true; testcnt *= 2)
	{
		u64 start = GetCPUTicks();
		for (int i = 0; i < testcnt; i++)
		{
			MultiPause();
		}
		u64 time = GetCPUTicks() - start;
		if (time > 100)
		{
			u64 nanos = (time * 1000000000) / GetTickFrequency();
			return (nanos / testcnt) + 1;
		}
	}
}

__noinline static void UpdatePauseTime()
{
	u64 wait = GetCPUTicks() + GetTickFrequency() / 100; // Wake up processor (spin for 10ms)
	while (GetCPUTicks() < wait)
		;
	u32 pause = MeasurePauseTime();
	// Take a few measurements in case something weird happens during one
	// (e.g. OS interrupt)
	for (int i = 0; i < 4; i++)
		pause = std::min(pause, MeasurePauseTime());
	PAUSE_TIME = pause;
	DevCon.WriteLn("MultiPause time: %uns", pause);
}

u32 ShortSpin()
{
	u32 inc = PAUSE_TIME;
	if (inc == 0) [[unlikely]]
	{
		UpdatePauseTime();
		inc = PAUSE_TIME;
	}

	u32 time = 0;
	// Sleep for approximately 500ns
	for (; time < 500; time += inc)
		MultiPause();

	return time;
}

static u32 GetSpinTime()
{
	if (char* req = getenv("WAIT_SPIN_MICROSECONDS"))
	{
		return 1000 * atoi(req);
	}
	else
	{
		return 50 * 1000; // 50µs
	}
}

const u32 SPIN_TIME_NS = GetSpinTime();

#ifdef __APPLE__
// https://alastairs-place.net/blog/2013/01/10/interesting-os-x-crash-report-tidbits/
// https://opensource.apple.com/source/WebKit2/WebKit2-7608.3.10.0.3/Platform/spi/Cocoa/CrashReporterClientSPI.h.auto.html
struct crash_info_t
{
	u64 version;
	u64 message;
	u64 signature;
	u64 backtrace;
	u64 message2;
	u64 reserved;
	u64 reserved2;
};
#define CRASH_ANNOTATION __attribute__((used, section("__DATA,__crash_info")))
#define CRASH_VERSION 4
extern "C" crash_info_t gCRAnnotations CRASH_ANNOTATION = { CRASH_VERSION };
#endif

void AbortWithMessage(const char* msg)
{
#ifdef __APPLE__
	gCRAnnotations.message = reinterpret_cast<size_t>(msg);
	// Some macOS's seem to have issues displaying non-static `message`s, so throw it in here too
	gCRAnnotations.backtrace = gCRAnnotations.message;
#endif
	abort();
}

#ifndef __APPLE__
// MacOS version is in DarwinMisc

#ifdef __aarch64__
// cpuinfo library often returns empty/unknown names on ARM Linux.
// Fall back to reading MIDR fields from /proc/cpuinfo.
static std::string DetectArmCPUName()
{
	FILE* f = fopen("/proc/cpuinfo", "r");
	if (!f)
		return {};

	u32 implementer = 0, part = 0;
	char line[256];
	while (fgets(line, sizeof(line), f))
	{
		if (sscanf(line, "CPU implementer : %x", &implementer) == 1)
			continue;
		if (sscanf(line, "CPU part : %x", &part) == 1)
			break; // got both from first core
	}
	fclose(f);

	// Map common implementer+part to names
	if (implementer == 0x41) // ARM Ltd
	{
		switch (part)
		{
			case 0xd03: return "ARM Cortex-A53";
			case 0xd04: return "ARM Cortex-A35";
			case 0xd05: return "ARM Cortex-A55";
			case 0xd07: return "ARM Cortex-A57";
			case 0xd08: return "ARM Cortex-A72";
			case 0xd09: return "ARM Cortex-A73";
			case 0xd0a: return "ARM Cortex-A75";
			case 0xd0b: return "ARM Cortex-A76";
			case 0xd0c: return "ARM Neoverse N1";
			case 0xd0d: return "ARM Cortex-A77";
			case 0xd40: return "ARM Neoverse V1";
			case 0xd41: return "ARM Cortex-A78";
			case 0xd44: return "ARM Cortex-X1";
			case 0xd46: return "ARM Cortex-A510";
			case 0xd47: return "ARM Cortex-A710";
			case 0xd48: return "ARM Cortex-X2";
			case 0xd4d: return "ARM Cortex-A715";
			case 0xd4e: return "ARM Cortex-X3";
			case 0xd80: return "ARM Cortex-A520";
			case 0xd81: return "ARM Cortex-A720";
			case 0xd82: return "ARM Cortex-X4";
		}
	}
	else if (implementer == 0x51) // Qualcomm
	{
		switch (part)
		{
			case 0x802: return "Qualcomm Kryo 385 Gold";
			case 0x803: return "Qualcomm Kryo 385 Silver";
			case 0xc00: return "Qualcomm Falkor";
			case 0x001: return "Qualcomm Oryon";
		}
	}
	else if (implementer == 0x61) // Apple
	{
		switch (part)
		{
			case 0x022: return "Apple M1 Icestorm";
			case 0x023: return "Apple M1 Firestorm";
			case 0x032: return "Apple M2 Blizzard";
			case 0x033: return "Apple M2 Avalanche";
		}
	}

	if (implementer != 0 && part != 0)
		return fmt::format("ARM (impl 0x{:02X} part 0x{:03X})", implementer, part);
	return {};
}
#endif

static CPUInfo CalcCPUInfo()
{
	CPUInfo out;
	out.name = cpuinfo_get_package(0)->name;
#ifdef __aarch64__
	// cpuinfo often returns empty/unknown on ARM Linux — use MIDR fallback
	if (out.name.empty() || out.name == "unknown" || out.name == "Unknown")
	{
		std::string arm_name = DetectArmCPUName();
		if (!arm_name.empty())
			out.name = std::move(arm_name);
	}
#endif
	out.num_threads = cpuinfo_get_processors_count();
	out.num_clusters = cpuinfo_get_clusters_count();
	out.num_big_cores = 0;
	out.num_small_cores = 0;
	const cpuinfo_cluster* clusters = cpuinfo_get_clusters();
	uint64_t big_freq = 0;
	for (uint32_t i = 0; i < out.num_clusters; i++)
	{
		const cpuinfo_cluster& cluster = clusters[i];
		if (cluster.frequency > big_freq)
		{
			out.num_small_cores += out.num_big_cores;
			out.num_big_cores = cluster.core_count;
			big_freq = cluster.frequency;
		}
		else if (cluster.frequency == big_freq)
		{
			out.num_big_cores += cluster.core_count;
		}
		else
		{
			out.num_small_cores += cluster.core_count;
		}
	}
	return out;
}

const CPUInfo& GetCPUInfo()
{
	static const CPUInfo info = CalcCPUInfo();
	return info;
}
#endif
