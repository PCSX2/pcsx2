// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "HostSys.h"
#include "Console.h"
#include "VectorIntrin.h"

#ifndef __APPLE__
#include "cpuinfo.h"
#endif

#if defined(__ANDROID__)
#include <sys/system_properties.h>
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
static CPUInfo CalcCPUInfo()
{
	CPUInfo out;
	const cpuinfo_package* pkg = cpuinfo_get_package(0);
	out.name = (pkg && pkg->name[0] != '\0') ? pkg->name : "Unknown";

#if defined(__ANDROID__)
	// cpuinfo's bundled SoC database may not recognise newer chips (e.g. QCS8550),
	// leaving the package name empty or "Unknown". Fall back to the Android SoC build
	// properties so the OSD shows a real name instead of "Unknown".
	if (out.name.empty() || out.name.find("Unknown") != std::string::npos)
	{
		char model[PROP_VALUE_MAX] = {};
		char manuf[PROP_VALUE_MAX] = {};
		__system_property_get("ro.soc.model", model);
		__system_property_get("ro.soc.manufacturer", manuf);
		if (model[0] != '\0')
			out.name = (manuf[0] != '\0') ? (std::string(manuf) + " " + model) : std::string(model);
		else if (manuf[0] != '\0')
			out.name = manuf;
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
