/*  Cpudetection lib
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "common/MemcpyFast.h"
#include "common/General.h"
#include "common/emitter/cpudetect_internal.h"
#include "common/emitter/internal.h"
#include "common/emitter/x86_intrin.h"
#include <atomic>

// CPU information support
#if defined(_WIN32)

#define cpuid __cpuid
#define cpuidex __cpuidex

#else

#include <cpuid.h>

static __inline__ __attribute__((always_inline)) void cpuidex(int CPUInfo[], const int InfoType, const int count)
{
	__cpuid_count(InfoType, count, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
}

static __inline__ __attribute__((always_inline)) void cpuid(int CPUInfo[], const int InfoType)
{
	__cpuid(InfoType, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
}

#endif

using namespace x86Emitter;

alignas(16) x86capabilities x86caps;

#ifdef _MSC_VER
// We disable optimizations for this function, because we need x86capabilities for AVX
// detection, but if we keep opts on, it'll use AVX instructions for inlining memzero.
#pragma optimize("", off)
#endif
x86capabilities::x86capabilities()
	: isIdentified(false)
	, VendorID(x86Vendor_Unknown)
	, FamilyID(0)
	, Model(0)
	, TypeID(0)
	, StepID(0)
	, Flags(0)
	, Flags2(0)
	, EFlags(0)
	, EFlags2(0)
	, SEFlag(0)
	, AllCapabilities(0)
	, PhysicalCores(0)
	, LogicalCores(0)
{
	memzero(VendorName);
	memzero(FamilyName);
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

// Warning!  We've had problems with the MXCSR detection code causing stack corruption in
// MSVC PGO builds.  The problem was fixed when I moved the MXCSR code to this function, and
// moved the recSSE[] array to a global static (it was local to cpudetectInit).  Commented
// here in case the nutty crash ever re-surfaces. >_<
// Note: recSSE was deleted
void x86capabilities::SIMD_EstablishMXCSRmask()
{
	if (!hasStreamingSIMDExtensions)
		return;

	MXCSR_Mask.bitmask = 0xFFBF; // MMX/SSE default

	if (hasStreamingSIMD2Extensions)
	{
		// This is generally safe assumption, but FXSAVE is the "correct" way to
		// detect MXCSR masking features of the cpu, so we use it's result below
		// and override this.

		MXCSR_Mask.bitmask = 0xFFFF; // SSE2 features added
	}

	alignas(16) u8 targetFXSAVE[512];

	// Work for recent enough GCC/CLANG/MSVC 2012
	_fxsave(&targetFXSAVE);

	u32 result;
	memcpy(&result, &targetFXSAVE[28], 4); // bytes 28->32 are the MXCSR_Mask.
	if (result != 0)
		MXCSR_Mask.bitmask = result;
}

// Counts the number of cpu cycles executed over the requested number of PerformanceCounter
// ticks. Returns that exact count.
// For best results you should pick a period of time long enough to get a reading that won't
// be prone to rounding error; but short enough that it'll be highly unlikely to be interrupted
// by the operating system task switches.
s64 x86capabilities::_CPUSpeedHz(u64 time) const
{
	u64 timeStart, timeStop;
	s64 startCycle, endCycle;

	if (!hasTimeStampCounter)
		return 0;

	SingleCoreAffinity affinity_lock;

	// Align the cpu execution to a cpuTick boundary.

	do
	{
		timeStart = GetCPUTicks();
		startCycle = __rdtsc();
	} while (GetCPUTicks() == timeStart);

	do
	{
		timeStop = GetCPUTicks();
		endCycle = __rdtsc();
	} while ((timeStop - timeStart) < time);

	s64 cycleCount = endCycle - startCycle;
	s64 timeCount = timeStop - timeStart;
	s64 overrun = timeCount - time;
	if (!overrun)
		return cycleCount;

	// interference could cause us to overshoot the target time, compensate:

	double cyclesPerTick = (double)cycleCount / (double)timeCount;
	double newCycleCount = (double)cycleCount - (cyclesPerTick * overrun);

	return (s64)newCycleCount;
}

const char* x86capabilities::GetTypeName() const
{
	switch (TypeID)
	{
		case 0:
			return "Standard OEM";
		case 1:
			return "Overdrive";
		case 2:
			return "Dual";
		case 3:
			return "Reserved";
		default:
			return "Unknown";
	}
}

void x86capabilities::CountCores()
{
	Identify();

	s32 regs[4];
	u32 cmds;

	cpuid(regs, 0x80000000);
	cmds = regs[0];

	// detect multicore for AMD cpu

	if ((cmds >= 0x80000008) && (VendorID == x86Vendor_AMD))
	{
		// AMD note: they don't support hyperthreading, but they like to flag this true
		// anyway.  Let's force-unflag it until we come up with a better solution.
		// (note: seems to affect some Phenom II's only? -- Athlon X2's and PhenomI's do
		// not seem to do this) --air
		hasMultiThreading = 0;
	}

	// This will assign values into LogicalCores and PhysicalCores
	CountLogicalCores();
}

static const char* tbl_x86vendors[] =
	{
		"GenuineIntel",
		"AuthenticAMD",
		"Unknown     ",
};

// Performs all _cpuid-related activity.  This fills *most* of the x86caps structure, except for
// the cpuSpeed and the mxcsr masks.  Those must be completed manually.
void x86capabilities::Identify()
{
	if (isIdentified)
		return;
	isIdentified = true;

	s32 regs[4];
	u32 cmds;

	memzero(VendorName);
	cpuid(regs, 0);

	cmds = regs[0];
	memcpy(&VendorName[0], &regs[1], 4);
	memcpy(&VendorName[4], &regs[3], 4);
	memcpy(&VendorName[8], &regs[2], 4);

	// Determine Vendor Specifics!
	// It's really not recommended that we base much (if anything) on CPU vendor names,
	// however it's currently necessary in order to gain a (pseudo)reliable count of cores
	// and threads used by the CPU (AMD and Intel can't agree on how to make this info available).

	int vid;
	for (vid = 0; vid < x86Vendor_Unknown; ++vid)
	{
		if (memcmp(VendorName, tbl_x86vendors[vid], 12) == 0)
			break;
	}
	VendorID = static_cast<x86VendorType>(vid);

	if (cmds >= 0x00000001)
	{
		cpuid(regs, 0x00000001);

		StepID = regs[0] & 0xf;
		Model = (regs[0] >> 4) & 0xf;
		FamilyID = (regs[0] >> 8) & 0xf;
		TypeID = (regs[0] >> 12) & 0x3;
		//u32 x86_64_8BITBRANDID = regs[1] & 0xff;
		Flags = regs[3];
		Flags2 = regs[2];
	}

	if (cmds >= 0x00000007)
	{
		// Note: ECX must be 0 for AVX2 detection.
		cpuidex(regs, 0x00000007, 0);

		SEFlag = regs[1];
	}

	cpuid(regs, 0x80000000);
	cmds = regs[0];
	if (cmds >= 0x80000001)
	{
		cpuid(regs, 0x80000001);

		//u32 x86_64_12BITBRANDID = regs[1] & 0xfff;
		EFlags2 = regs[2];
		EFlags = regs[3];
	}

	memzero(FamilyName);
	cpuid((int*)FamilyName, 0x80000002);
	cpuid((int*)(FamilyName + 16), 0x80000003);
	cpuid((int*)(FamilyName + 32), 0x80000004);

	hasFloatingPointUnit = (Flags >> 0) & 1;
	hasVirtual8086ModeEnhancements = (Flags >> 1) & 1;
	hasDebuggingExtensions = (Flags >> 2) & 1;
	hasPageSizeExtensions = (Flags >> 3) & 1;
	hasTimeStampCounter = (Flags >> 4) & 1;
	hasModelSpecificRegisters = (Flags >> 5) & 1;
	hasPhysicalAddressExtension = (Flags >> 6) & 1;
	hasMachineCheckArchitecture = (Flags >> 7) & 1;
	hasCOMPXCHG8BInstruction = (Flags >> 8) & 1;
	hasAdvancedProgrammableInterruptController = (Flags >> 9) & 1;
	hasSEPFastSystemCall = (Flags >> 11) & 1;
	hasMemoryTypeRangeRegisters = (Flags >> 12) & 1;
	hasPTEGlobalFlag = (Flags >> 13) & 1;
	hasMachineCheckArchitecture = (Flags >> 14) & 1;
	hasConditionalMoveAndCompareInstructions = (Flags >> 15) & 1;
	hasFGPageAttributeTable = (Flags >> 16) & 1;
	has36bitPageSizeExtension = (Flags >> 17) & 1;
	hasProcessorSerialNumber = (Flags >> 18) & 1;
	hasCFLUSHInstruction = (Flags >> 19) & 1;
	hasDebugStore = (Flags >> 21) & 1;
	hasACPIThermalMonitorAndClockControl = (Flags >> 22) & 1;
	hasFastStreamingSIMDExtensionsSaveRestore = (Flags >> 24) & 1;
	hasStreamingSIMDExtensions = (Flags >> 25) & 1; //sse
	hasStreamingSIMD2Extensions = (Flags >> 26) & 1; //sse2
	hasSelfSnoop = (Flags >> 27) & 1;
	hasMultiThreading = (Flags >> 28) & 1;
	hasThermalMonitor = (Flags >> 29) & 1;
	hasIntel64BitArchitecture = (Flags >> 30) & 1;

	// -------------------------------------------------
	// --> SSE3 / SSSE3 / SSE4.1 / SSE 4.2 detection <--
	// -------------------------------------------------

	hasStreamingSIMD3Extensions = (Flags2 >> 0) & 1; //sse3
	hasSupplementalStreamingSIMD3Extensions = (Flags2 >> 9) & 1; //ssse3
	hasStreamingSIMD4Extensions = (Flags2 >> 19) & 1; //sse4.1
	hasStreamingSIMD4Extensions2 = (Flags2 >> 20) & 1; //sse4.2

	if ((Flags2 >> 27) & 1) // OSXSAVE
	{
		// Note: In theory, we should use xgetbv to check OS support
		// but all OSes we officially run under support it
		// and its intrinsic requires extra compiler flags
		hasAVX = (Flags2 >> 28) & 1; //avx
		hasFMA = (Flags2 >> 12) & 1; //fma
		hasAVX2 = (SEFlag >> 5) & 1; //avx2
	}

	hasBMI1 = (SEFlag >> 3) & 1;
	hasBMI2 = (SEFlag >> 8) & 1;

	// Ones only for AMDs:
	hasAMD64BitArchitecture = (EFlags >> 29) & 1; //64bit cpu
	hasStreamingSIMD4ExtensionsA = (EFlags2 >> 6) & 1; //INSERTQ / EXTRQ / MOVNT

	isIdentified = true;
}

u32 x86capabilities::CalculateMHz() const
{
	InitCPUTicks();
	u64 span = GetTickFrequency();

	if ((span % 1000) < 400) // helps minimize rounding errors
		return (u32)(_CPUSpeedHz(span / 1000) / 1000);
	else
		return (u32)(_CPUSpeedHz(span / 500) / 2000);
}

u32 x86capabilities::CachedMHz()
{
	static std::atomic<u32> cached{0};
	u32 local = cached.load(std::memory_order_relaxed);
	if (unlikely(local == 0))
	{
		x86capabilities caps;
		caps.Identify();
		local = caps.CalculateMHz();
		cached.store(local, std::memory_order_relaxed);
	}
	return local;
}
