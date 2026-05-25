#ifndef XBYAK_XBYAK_UTIL_H_
#define XBYAK_XBYAK_UTIL_H_

#ifdef XBYAK_ONLY_CLASS_CPU
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#ifndef XBYAK_THROW
	#define XBYAK_THROW(x) ;
	#define XBYAK_THROW_RET(x, y) return y;
#endif
#ifndef XBYAK_CONSTEXPR
#if ((__cplusplus >= 201402L) && !(!defined(__clang__) && defined(__GNUC__) && (__GNUC__ <= 5))) || (defined(_MSC_VER) && _MSC_VER >= 1910)
	#define XBYAK_CONSTEXPR constexpr
#else
	#define XBYAK_CONSTEXPR
#endif
#define XBYAK_CPUMASK_COMPACT 0
#endif
#else
#include <string.h>
#include <stdio.h>

/**
	utility class and functions for Xbyak
	Xbyak::util::Clock ; rdtsc timer
	Xbyak::util::Cpu ; detect CPU
*/
#include "xbyak.h"
#endif // XBYAK_ONLY_CLASS_CPU

#if defined(__i386__) || (defined(__x86_64__) && !defined(__arm64ec__)) || defined(_M_IX86) || (defined(_M_X64) && !defined(_M_ARM64EC))
	#define XBYAK_INTEL_CPU_SPECIFIC
#endif

#ifdef XBYAK_INTEL_CPU_SPECIFIC
#ifdef _WIN32
	#if defined(_MSC_VER) && (_MSC_VER < 1400) && defined(XBYAK32)
		static inline __declspec(naked) void __cpuid(int[4], int)
		{
			__asm {
				push	ebx
				push	esi
				mov		eax, dword ptr [esp + 4 * 2 + 8] // eaxIn
				cpuid
				mov		esi, dword ptr [esp + 4 * 2 + 4] // data
				mov		dword ptr [esi], eax
				mov		dword ptr [esi + 4], ebx
				mov		dword ptr [esi + 8], ecx
				mov		dword ptr [esi + 12], edx
				pop		esi
				pop		ebx
				ret
			}
		}
	#else
		#include <intrin.h> // for __cpuid
	#endif
#else
	#ifndef __GNUC_PREREQ
    	#define __GNUC_PREREQ(major, minor) ((((__GNUC__) << 16) + (__GNUC_MINOR__)) >= (((major) << 16) + (minor)))
	#endif
	#if __GNUC_PREREQ(4, 3) && !defined(__APPLE__)
		#include <cpuid.h>
	#else
		#if defined(__APPLE__) && defined(XBYAK32) // avoid err : can't find a register in class `BREG' while reloading `asm'
			#define __cpuid(eaxIn, a, b, c, d) __asm__ __volatile__("pushl %%ebx\ncpuid\nmovl %%ebp, %%esi\npopl %%ebx" : "=a"(a), "=S"(b), "=c"(c), "=d"(d) : "0"(eaxIn))
			#define __cpuid_count(eaxIn, ecxIn, a, b, c, d) __asm__ __volatile__("pushl %%ebx\ncpuid\nmovl %%ebp, %%esi\npopl %%ebx" : "=a"(a), "=S"(b), "=c"(c), "=d"(d) : "0"(eaxIn), "2"(ecxIn))
		#else
			#define __cpuid(eaxIn, a, b, c, d) __asm__ __volatile__("cpuid\n" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(eaxIn))
			#define __cpuid_count(eaxIn, ecxIn, a, b, c, d) __asm__ __volatile__("cpuid\n" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(eaxIn), "2"(ecxIn))
		#endif
	#endif
#endif
#endif

#ifdef XBYAK_USE_VTUNE
	// -I /opt/intel/vtune_amplifier/include/ -L /opt/intel/vtune_amplifier/lib64 -ljitprofiling -ldl
	#include <jitprofiling.h>
	#ifdef _MSC_VER
		#pragma comment(lib, "libittnotify.lib")
	#endif
	#ifdef __linux__
		#include <dlfcn.h>
	#endif
#endif
#ifdef __linux__
	#define XBYAK_USE_PERF
#endif

#ifndef XBYAK_CPU_CACHE
	#define XBYAK_CPU_CACHE 1
#endif
#if XBYAK_CPU_CACHE == 1
#include <vector>
#ifndef XBYAK_CPUMASK_COMPACT
	#define XBYAK_CPUMASK_COMPACT 1
#endif
#if XBYAK_CPUMASK_COMPACT == 0
	#include <set>
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#endif
namespace Xbyak { namespace util {
class CpuTopology;
class Cpu;
namespace impl {

bool initCpuTopology(CpuTopology& cpuTopo);

} // Xbyak::util::impl
} } // Xbyak::util
#endif // XBYAK_CPU_CACHE


namespace Xbyak { namespace util {

typedef enum {
   SmtLevel = 1,
   CoreLevel = 2
} CpuTopologyLevel;
typedef CpuTopologyLevel IntelCpuTopologyLevel; // for backward compatibility

namespace local {

template<uint64_t L, uint64_t H = 0>
struct TypeT {
};

template<uint64_t L1, uint64_t H1, uint64_t L2, uint64_t H2>
XBYAK_CONSTEXPR TypeT<L1 | L2, H1 | H2> operator|(TypeT<L1, H1>, TypeT<L2, H2>) { return TypeT<L1 | L2, H1 | H2>(); }

template<typename T>
inline T max_(T x, T y) { return x >= y ? x : y; }
template<typename T>
inline T min_(T x, T y) { return x < y ? x : y; }

} // local

/**
	CPU detection class
	@note static inline const member is supported by c++17 or later, so use template hack
*/
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable : 4459)
#endif
class Cpu {
public:
	class Type {
		uint64_t L;
		uint64_t H;
	public:
		Type(uint64_t L = 0, uint64_t H = 0) : L(L), H(H) { }
		template<uint64_t L_, uint64_t H_>
		Type(local::TypeT<L_, H_>) : L(L_), H(H_) {}
		Type& operator&=(const Type& rhs) { L &= rhs.L; H &= rhs.H; return *this; }
		Type& operator|=(const Type& rhs) { L |= rhs.L; H |= rhs.H; return *this; }
		Type operator&(const Type& rhs) const { Type t = *this; t &= rhs; return t; }
		Type operator|(const Type& rhs) const { Type t = *this; t |= rhs; return t; }
		bool operator==(const Type& rhs) const { return H == rhs.H && L == rhs.L; }
		bool operator!=(const Type& rhs) const { return !operator==(rhs); }
		// without explicit because backward compatilibity
		operator bool() const { return (H | L) != 0; }
		uint64_t getL() const { return L; }
		uint64_t getH() const { return H; }
	};
private:
	Type type_;
	//system topology
	static const size_t maxTopologyLevels = 2;
	uint32_t numCores_[maxTopologyLevels];

	static const uint32_t maxNumberCacheLevels = 10;
	uint32_t dataCacheSize_[maxNumberCacheLevels];
	uint32_t coresSharingDataCache_[maxNumberCacheLevels];
	uint32_t dataCacheLevels_;
	uint32_t avx10version_;

	uint32_t get32bitAsBE(const char *x) const
	{
		return x[0] | (x[1] << 8) | (x[2] << 16) | (x[3] << 24);
	}
	uint32_t mask(int n) const
	{
		return (1U << n) - 1;
	}
	// [ebx:ecx:edx] == s?
	bool isEqualStr(uint32_t ebx, uint32_t ecx, uint32_t edx, const char s[12]) const
	{
		return get32bitAsBE(&s[0]) == ebx && get32bitAsBE(&s[4]) == edx && get32bitAsBE(&s[8]) == ecx;
	}
	uint32_t extractBit(uint32_t val, uint32_t base, uint32_t end) const
	{
		return (val >> base) & ((1u << (end + 1 - base)) - 1);
	}
	void setFamily()
	{
		uint32_t data[4] = {};
		getCpuid(1, data);
		stepping = extractBit(data[0], 0, 3);
		model = extractBit(data[0], 4, 7);
		family = extractBit(data[0], 8, 11);
		//type = extractBit(data[0], 12, 13);
		extModel = extractBit(data[0], 16, 19);
		extFamily = extractBit(data[0], 20, 27);
		if (family == 0x0f) {
			displayFamily = family + extFamily;
		} else {
			displayFamily = family;
		}
		if ((has(tINTEL) && family == 6) || family == 0x0f) {
			displayModel = (extModel << 4) + model;
		} else {
			displayModel = model;
		}
	}
	void setNumCores()
	{
		if (!has(tINTEL) && !has(tAMD)) return;

		uint32_t data[4] = {};
		getCpuid(0x0, data);
		if (data[0] >= 0xB) {
			// Check if "Extended Topology Enumeration" is implemented.
			getCpuidEx(0xB, 0, data);
			if (data[0] != 0 || data[1] != 0) {
				/*
					if leaf 11 exists(x2APIC is supported),
					we use it to get the number of smt cores and cores on socket

					leaf 0xB can be zeroed-out by a hypervisor
				*/
				for (uint32_t i = 0; i < maxTopologyLevels; i++) {
					getCpuidEx(0xB, i, data);
					CpuTopologyLevel level = (CpuTopologyLevel)extractBit(data[2], 8, 15);
					if (level == SmtLevel || level == CoreLevel) {
						numCores_[level - 1] = extractBit(data[1], 0, 15);
					}
				}
				/*
					Fallback values in case a hypervisor has the leaf zeroed-out.
				*/
				numCores_[SmtLevel - 1] = local::max_(1u, numCores_[SmtLevel - 1]);
				numCores_[CoreLevel - 1] = local::max_(numCores_[SmtLevel - 1], numCores_[CoreLevel - 1]);
				return;
			}
		}
		// "Extended Topology Enumeration" is not supported.
		if (has(tAMD)) {
			/*
				AMD - Legacy Method
			*/
			int physicalThreadCount = 0;
			getCpuid(0x1, data);
			int logicalProcessorCount = extractBit(data[1], 16, 23);
			int htt = extractBit(data[3], 28, 28); // Hyper-threading technology.
			getCpuid(0x80000000, data);
			uint32_t highestExtendedLeaf = data[0];
			if (highestExtendedLeaf >= 0x80000008) {
				getCpuid(0x80000008, data);
				physicalThreadCount = extractBit(data[2], 0, 7) + 1;
			}
			if (htt == 0) {
				numCores_[SmtLevel - 1] = 1;
				numCores_[CoreLevel - 1] = 1;
			} else if (physicalThreadCount > 1) {
				if ((displayFamily >= 0x17) && (highestExtendedLeaf >= 0x8000001E)) {
					// Zen overreports its core count by a factor of two.
					getCpuid(0x8000001E, data);
					int threadsPerComputeUnit = extractBit(data[1], 8, 15) + 1;
					physicalThreadCount /= threadsPerComputeUnit;
				}
				numCores_[SmtLevel - 1] = logicalProcessorCount / physicalThreadCount;
				numCores_[CoreLevel - 1] = logicalProcessorCount;
			} else {
				numCores_[SmtLevel - 1] = 1;
				numCores_[CoreLevel - 1] = logicalProcessorCount > 1 ? logicalProcessorCount : 2;
			}
		} else {
			/*
				Intel - Legacy Method
			*/
			int physicalThreadCount = 0;
			getCpuid(0x1, data);
			int logicalProcessorCount = extractBit(data[1], 16, 23);
			int htt = extractBit(data[3], 28, 28); // Hyper-threading technology.
			getCpuid(0, data);
			if (data[0] >= 0x4) {
				getCpuid(0x4, data);
				physicalThreadCount = extractBit(data[0], 26, 31) + 1;
			}
			if (htt == 0) {
				numCores_[SmtLevel - 1] = 1;
				numCores_[CoreLevel - 1] = 1;
			} else if (physicalThreadCount > 1) {
				numCores_[SmtLevel - 1] = logicalProcessorCount / physicalThreadCount;
				numCores_[CoreLevel - 1] = logicalProcessorCount;
			} else {
				numCores_[SmtLevel - 1] = 1;
				numCores_[CoreLevel - 1] = logicalProcessorCount > 0 ? logicalProcessorCount : 1;
			}
		}
	}
	void setCacheHierarchy()
	{
		uint32_t data[4] = {};
		if (has(tAMD)) {
			getCpuid(0x80000000, data);
			if (data[0] >= 0x8000001D) {
				// For modern AMD CPUs.
				dataCacheLevels_ = 0;
				for (uint32_t subLeaf = 0; dataCacheLevels_ < maxNumberCacheLevels; subLeaf++) {
					getCpuidEx(0x8000001D, subLeaf, data);
					int cacheType = extractBit(data[0], 0, 4);
					/*
					  cacheType
						00h - Null; no more caches
						01h - Data cache
						02h - Instrution cache
						03h - Unified cache
						04h-1Fh - Reserved
					*/
					if (cacheType == 0) break; // No more caches.
					if (cacheType == 0x2) continue; // Skip instruction cache.
					int fullyAssociative = extractBit(data[0], 9, 9);
					int numSharingCache = extractBit(data[0], 14, 25) + 1;
					int cacheNumWays = extractBit(data[1], 22, 31) + 1;
					int cachePhysPartitions = extractBit(data[1], 12, 21) + 1;
					int cacheLineSize = extractBit(data[1], 0, 11) + 1;
					int cacheNumSets = data[2] + 1;
					dataCacheSize_[dataCacheLevels_] =
						cacheLineSize * cachePhysPartitions * cacheNumWays;
					if (fullyAssociative == 0) {
						dataCacheSize_[dataCacheLevels_] *= cacheNumSets;
					}
					if (subLeaf > 0) {
						numSharingCache = local::min_(numSharingCache, (int)numCores_[1]);
						numSharingCache /= local::max_(1u, coresSharingDataCache_[0]);
					}
					coresSharingDataCache_[dataCacheLevels_] = numSharingCache;
					dataCacheLevels_ += 1;
				}
				coresSharingDataCache_[0] = local::min_(1u, coresSharingDataCache_[0]);
			} else if (data[0] >= 0x80000006) {
				// For legacy AMD CPUs, use leaf 0x80000005 for L1 cache
				// and 0x80000006 for L2 and L3 cache.
				dataCacheLevels_ = 1;
				getCpuid(0x80000005, data);
				int l1dc_size = extractBit(data[2], 24, 31);
				dataCacheSize_[0] = l1dc_size * 1024;
				coresSharingDataCache_[0] = 1;
				getCpuid(0x80000006, data);
				// L2 cache
				int l2_assoc = extractBit(data[2], 12, 15);
				if (l2_assoc > 0) {
					dataCacheLevels_ = 2;
					int l2_size = extractBit(data[2], 16, 31);
					dataCacheSize_[1] = l2_size * 1024;
					coresSharingDataCache_[1] = 1;
				}
				// L3 cache
				int l3_assoc = extractBit(data[3], 12, 15);
				if (l3_assoc > 0) {
					dataCacheLevels_ = 3;
					int l3_size = extractBit(data[3], 18, 31);
					dataCacheSize_[2] = l3_size * 512 * 1024;
					coresSharingDataCache_[2] = numCores_[1];
				}
			}
		} else if (has(tINTEL)) {
			// Use the "Deterministic Cache Parameters" leaf is supported.
			const uint32_t NO_CACHE = 0;
			const uint32_t DATA_CACHE = 1;
			//const uint32_t INSTRUCTION_CACHE = 2;
			const uint32_t UNIFIED_CACHE = 3;
			uint32_t smt_width = 0;
			uint32_t logical_cores = 0;

			smt_width = numCores_[0];
			logical_cores = numCores_[1];

			/*
				Assumptions:
				the first level of data cache is not shared (which is the
				case for every existing architecture) and use this to
				determine the SMT width for arch not supporting leaf 11.
				when leaf 4 reports a number of core less than numCores_
				on socket reported by leaf 11, then it is a correct number
				of cores not an upperbound.
			*/
			for (int i = 0; dataCacheLevels_ < maxNumberCacheLevels; i++) {
				getCpuidEx(0x4, i, data);
				uint32_t cacheType = extractBit(data[0], 0, 4);
				if (cacheType == NO_CACHE) break;
				if (cacheType == DATA_CACHE || cacheType == UNIFIED_CACHE) {
					uint32_t actual_logical_cores = extractBit(data[0], 14, 25) + 1;
					if (logical_cores != 0) { // true only if leaf 0xB is supported and valid
						actual_logical_cores = local::min_(actual_logical_cores, logical_cores);
					}
					assert(actual_logical_cores != 0);
					dataCacheSize_[dataCacheLevels_] =
						(extractBit(data[1], 22, 31) + 1)
						* (extractBit(data[1], 12, 21) + 1)
						* (extractBit(data[1], 0, 11) + 1)
						* (data[2] + 1);
					if (cacheType == DATA_CACHE && smt_width == 0) smt_width = actual_logical_cores;
					assert(smt_width != 0);
					coresSharingDataCache_[dataCacheLevels_] = local::max_(actual_logical_cores / smt_width, 1u);
					dataCacheLevels_++;
				}
			}
		}
	}

public:
	int model;
	int family;
	int stepping;
	int extModel;
	int extFamily;
	int displayFamily; // family + extFamily
	int displayModel; // model + extModel

	uint32_t getNumCores(CpuTopologyLevel level) const {
		switch (level) {
		case SmtLevel: return numCores_[level - 1];
		case CoreLevel: return numCores_[level - 1] / numCores_[SmtLevel - 1];
		default: XBYAK_THROW_RET(ERR_X2APIC_IS_NOT_SUPPORTED, 0)
		}
	}

	uint32_t getDataCacheLevels() const { return dataCacheLevels_; }
	uint32_t getCoresSharingDataCache(uint32_t i) const
	{
		if (i >= dataCacheLevels_) XBYAK_THROW_RET(ERR_BAD_PARAMETER, 0)
		return coresSharingDataCache_[i];
	}
	uint32_t getDataCacheSize(uint32_t i) const
	{
		if (i >= dataCacheLevels_) XBYAK_THROW_RET(ERR_BAD_PARAMETER, 0)
		return dataCacheSize_[i];
	}

	/*
		data[] = { eax, ebx, ecx, edx }
	*/
	static inline void getCpuidEx(uint32_t eaxIn, uint32_t ecxIn, uint32_t data[4])
	{
#ifdef XBYAK_INTEL_CPU_SPECIFIC
	#ifdef _MSC_VER
		__cpuidex(reinterpret_cast<int*>(data), eaxIn, ecxIn);
	#else
		__cpuid_count(eaxIn, ecxIn, data[0], data[1], data[2], data[3]);
	#endif
#else
		(void)eaxIn;
		(void)ecxIn;
		(void)data;
#endif
	}
	static inline void getCpuid(uint32_t eaxIn, uint32_t data[4])
	{
		getCpuidEx(eaxIn, 0, data);
	}
	static inline uint64_t getXfeature()
	{
#ifdef XBYAK_INTEL_CPU_SPECIFIC
	#ifdef _MSC_VER
		return _xgetbv(0);
	#else
		uint32_t eax, edx;
		// xgetvb is not support on gcc 4.2
//		__asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
		__asm__ volatile(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(0));
		return ((uint64_t)edx << 32) | eax;
	#endif
#else
		return 0;
#endif
	}

#define XBYAK_SPLIT_ID(id) ((0 <= id && id < 64) ? (1ull << (id % 64)) : 0), (id >= 64 ? (1ull << (id % 64)) : 0)
#if (__cplusplus >= 201103) || (defined(_MSC_VER) && (_MSC_VER >= 1700)) /* VS2012 */
	#define XBYAK_DEFINE_TYPE(id, NAME) static const constexpr local::TypeT<XBYAK_SPLIT_ID(id)> NAME{}
#else
	#define XBYAK_DEFINE_TYPE(id, NAME) static const local::TypeT<XBYAK_SPLIT_ID(id)> NAME
#endif
	XBYAK_DEFINE_TYPE(0, tMMX);
	XBYAK_DEFINE_TYPE(1, tMMX2);
	XBYAK_DEFINE_TYPE(2, tCMOV);
	XBYAK_DEFINE_TYPE(3, tSSE);
	XBYAK_DEFINE_TYPE(4, tSSE2);
	XBYAK_DEFINE_TYPE(5, tSSE3);
	XBYAK_DEFINE_TYPE(6, tSSSE3);
	XBYAK_DEFINE_TYPE(7, tSSE41);
	XBYAK_DEFINE_TYPE(8, tSSE42);
	XBYAK_DEFINE_TYPE(9, tPOPCNT);
	XBYAK_DEFINE_TYPE(10, tAESNI);
	XBYAK_DEFINE_TYPE(11, tAVX512_FP16);
	XBYAK_DEFINE_TYPE(12, tOSXSAVE);
	XBYAK_DEFINE_TYPE(13, tPCLMULQDQ);
	XBYAK_DEFINE_TYPE(14, tAVX);
	XBYAK_DEFINE_TYPE(15, tFMA);
	XBYAK_DEFINE_TYPE(16, t3DN);
	XBYAK_DEFINE_TYPE(17, tE3DN);
	XBYAK_DEFINE_TYPE(18, tWAITPKG);
	XBYAK_DEFINE_TYPE(19, tRDTSCP);
	XBYAK_DEFINE_TYPE(20, tAVX2);
	XBYAK_DEFINE_TYPE(21, tBMI1); // andn, bextr, blsi, blsmsk, blsr, tzcnt
	XBYAK_DEFINE_TYPE(22, tBMI2); // bzhi, mulx, pdep, pext, rorx, sarx, shlx, shrx
	XBYAK_DEFINE_TYPE(23, tLZCNT);
	XBYAK_DEFINE_TYPE(24, tINTEL);
	XBYAK_DEFINE_TYPE(25, tAMD);
	XBYAK_DEFINE_TYPE(26, tENHANCED_REP); // enhanced rep movsb/stosb
	XBYAK_DEFINE_TYPE(27, tRDRAND);
	XBYAK_DEFINE_TYPE(28, tADX); // adcx, adox
	XBYAK_DEFINE_TYPE(29, tRDSEED); // rdseed
	XBYAK_DEFINE_TYPE(30, tSMAP); // stac
	XBYAK_DEFINE_TYPE(31, tHLE); // xacquire, xrelease, xtest
	XBYAK_DEFINE_TYPE(32, tRTM); // xbegin, xend, xabort
	XBYAK_DEFINE_TYPE(33, tF16C); // vcvtph2ps, vcvtps2ph
	XBYAK_DEFINE_TYPE(34, tMOVBE); // mobve
	XBYAK_DEFINE_TYPE(35, tAVX512F);
	XBYAK_DEFINE_TYPE(36, tAVX512DQ);
	XBYAK_DEFINE_TYPE(37, tAVX512_IFMA);
	XBYAK_DEFINE_TYPE(37, tAVX512IFMA);// = tAVX512_IFMA;
//	XBYAK_DEFINE_TYPE(38, tAVX512PF); // Xeon Phi only
//	XBYAK_DEFINE_TYPE(39, tAVX512ER);
	XBYAK_DEFINE_TYPE(40, tAVX512CD);
	XBYAK_DEFINE_TYPE(41, tAVX512BW);
	XBYAK_DEFINE_TYPE(42, tAVX512VL);
	XBYAK_DEFINE_TYPE(43, tAVX512_VBMI);
	XBYAK_DEFINE_TYPE(43, tAVX512VBMI); // = tAVX512_VBMI; // changed by Intel's manual
//	XBYAK_DEFINE_TYPE(44, tAVX512_4VNNIW);
//	XBYAK_DEFINE_TYPE(45, tAVX512_4FMAPS);
//	XBYAK_DEFINE_TYPE(46, tPREFETCHWT1);
	XBYAK_DEFINE_TYPE(47, tPREFETCHW);
	XBYAK_DEFINE_TYPE(48, tSHA);
	XBYAK_DEFINE_TYPE(49, tMPX);
	XBYAK_DEFINE_TYPE(50, tAVX512_VBMI2);
	XBYAK_DEFINE_TYPE(51, tGFNI);
	XBYAK_DEFINE_TYPE(52, tVAES);
	XBYAK_DEFINE_TYPE(53, tVPCLMULQDQ);
	XBYAK_DEFINE_TYPE(54, tAVX512_VNNI);
	XBYAK_DEFINE_TYPE(55, tAVX512_BITALG);
	XBYAK_DEFINE_TYPE(56, tAVX512_VPOPCNTDQ);
	XBYAK_DEFINE_TYPE(57, tAVX512_BF16);
	XBYAK_DEFINE_TYPE(58, tAVX512_VP2INTERSECT);
	XBYAK_DEFINE_TYPE(59, tAMX_TILE);
	XBYAK_DEFINE_TYPE(60, tAMX_INT8);
	XBYAK_DEFINE_TYPE(61, tAMX_BF16);
	XBYAK_DEFINE_TYPE(62, tAVX_VNNI);
	XBYAK_DEFINE_TYPE(63, tCLFLUSHOPT);
	XBYAK_DEFINE_TYPE(64, tCLDEMOTE);
	XBYAK_DEFINE_TYPE(65, tMOVDIRI);
	XBYAK_DEFINE_TYPE(66, tMOVDIR64B);
	XBYAK_DEFINE_TYPE(67, tCLZERO); // AMD Zen
	XBYAK_DEFINE_TYPE(68, tAMX_FP16);
	XBYAK_DEFINE_TYPE(69, tAVX_VNNI_INT8);
	XBYAK_DEFINE_TYPE(70, tAVX_NE_CONVERT);
	XBYAK_DEFINE_TYPE(71, tAVX_IFMA);
	XBYAK_DEFINE_TYPE(72, tRAO_INT);
	XBYAK_DEFINE_TYPE(73, tCMPCCXADD);
	XBYAK_DEFINE_TYPE(74, tPREFETCHITI);
	XBYAK_DEFINE_TYPE(75, tSERIALIZE);
	XBYAK_DEFINE_TYPE(76, tUINTR);
	XBYAK_DEFINE_TYPE(77, tXSAVE);
	XBYAK_DEFINE_TYPE(78, tSHA512);
	XBYAK_DEFINE_TYPE(79, tSM3);
	XBYAK_DEFINE_TYPE(80, tSM4);
	XBYAK_DEFINE_TYPE(81, tAVX_VNNI_INT16);
	XBYAK_DEFINE_TYPE(82, tAPX_F);
	XBYAK_DEFINE_TYPE(83, tAVX10);
	XBYAK_DEFINE_TYPE(84, tAESKLE);
	XBYAK_DEFINE_TYPE(85, tWIDE_KL);
	XBYAK_DEFINE_TYPE(86, tKEYLOCKER);
	XBYAK_DEFINE_TYPE(87, tKEYLOCKER_WIDE);
	XBYAK_DEFINE_TYPE(88, tSSE4a);
	XBYAK_DEFINE_TYPE(89, tCLWB);
	XBYAK_DEFINE_TYPE(90, tTSXLDTRK);
	XBYAK_DEFINE_TYPE(91, tAMX_TRANSPOSE);
	XBYAK_DEFINE_TYPE(92, tAMX_TF32);
	XBYAK_DEFINE_TYPE(93, tAMX_AVX512);
	XBYAK_DEFINE_TYPE(94, tAMX_MOVRS);
	XBYAK_DEFINE_TYPE(95, tAMX_FP8);
	XBYAK_DEFINE_TYPE(96, tMOVRS);
	XBYAK_DEFINE_TYPE(97, tHYBRID);
	XBYAK_DEFINE_TYPE(98, tAMX_COMPLEX);

#undef XBYAK_SPLIT_ID
#undef XBYAK_DEFINE_TYPE

	Cpu()
		: type_()
		, numCores_()
		, dataCacheSize_()
		, coresSharingDataCache_()
		, dataCacheLevels_(0)
		, avx10version_(0)
	{
		uint32_t data[4] = {};
		const uint32_t& eax = data[0];
		const uint32_t& ebx = data[1];
		const uint32_t& ecx = data[2];
		const uint32_t& edx = data[3];
		getCpuid(0, data);
		const uint32_t maxNum = eax;
		if (isEqualStr(ebx, ecx, edx, "AuthenticAMD")) {
			type_ |= tAMD;
			getCpuid(0x80000001, data);
			if (edx & (1U << 31)) {
				type_ |= t3DN;
				// 3DNow! implies support for PREFETCHW on AMD
				type_ |= tPREFETCHW;
			}

			if (edx & (1U << 29)) {
				// Long mode implies support for PREFETCHW on AMD
				type_ |= tPREFETCHW;
			}
		} else if (isEqualStr(ebx, ecx, edx, "GenuineIntel")) {
			type_ |= tINTEL;
		}

		// Extended flags information
		getCpuid(0x80000000, data);
		const uint32_t maxExtendedNum = eax;
		if (maxExtendedNum >= 0x80000001) {
			getCpuid(0x80000001, data);

			if (ecx & (1U << 5)) type_ |= tLZCNT;
			if (ecx & (1U << 6)) type_ |= tSSE4a;
			if (ecx & (1U << 8)) type_ |= tPREFETCHW;
			if (edx & (1U << 15)) type_ |= tCMOV;
			if (edx & (1U << 22)) type_ |= tMMX2;
			if (edx & (1U << 27)) type_ |= tRDTSCP;
			if (edx & (1U << 30)) type_ |= tE3DN;
			if (edx & (1U << 31)) type_ |= t3DN;
		}

		if (maxExtendedNum >= 0x80000008) {
			getCpuid(0x80000008, data);
			if (ebx & (1U << 0)) type_ |= tCLZERO;
		}

		getCpuid(1, data);
		if (ecx & (1U << 0)) type_ |= tSSE3;
		if (ecx & (1U << 1)) type_ |= tPCLMULQDQ;
		if (ecx & (1U << 9)) type_ |= tSSSE3;
		if (ecx & (1U << 19)) type_ |= tSSE41;
		if (ecx & (1U << 20)) type_ |= tSSE42;
		if (ecx & (1U << 22)) type_ |= tMOVBE;
		if (ecx & (1U << 23)) type_ |= tPOPCNT;
		if (ecx & (1U << 25)) type_ |= tAESNI;
		if (ecx & (1U << 26)) type_ |= tXSAVE;
		if (ecx & (1U << 27)) type_ |= tOSXSAVE;
		if (ecx & (1U << 29)) type_ |= tF16C;
		if (ecx & (1U << 30)) type_ |= tRDRAND;

		if (edx & (1U << 15)) type_ |= tCMOV;
		if (edx & (1U << 23)) type_ |= tMMX;
		if (edx & (1U << 25)) type_ |= tMMX2 | tSSE;
		if (edx & (1U << 26)) type_ |= tSSE2;

		if (type_ & tOSXSAVE) {
			// check XFEATURE_ENABLED_MASK[2:1] = '11b'
			uint64_t bv = getXfeature();
			if ((bv & 6) == 6) {
				if (ecx & (1U << 12)) type_ |= tFMA;
				if (ecx & (1U << 28)) type_ |= tAVX;
				// do *not* check AVX-512 state on macOS because it has on-demand AVX-512 support
#if !defined(__APPLE__)
				if (((bv >> 5) & 7) == 7)
#endif
				{
					getCpuidEx(7, 0, data);
					if (ebx & (1U << 16)) type_ |= tAVX512F;
					if (type_ & tAVX512F) {
						if (ebx & (1U << 17)) type_ |= tAVX512DQ;
						if (ebx & (1U << 21)) type_ |= tAVX512_IFMA;
						if (ebx & (1U << 28)) type_ |= tAVX512CD;
						if (ebx & (1U << 30)) type_ |= tAVX512BW;
						if (ebx & (1U << 31)) type_ |= tAVX512VL;
						if (ecx & (1U << 1)) type_ |= tAVX512_VBMI;
						if (ecx & (1U << 6)) type_ |= tAVX512_VBMI2;
						if (ecx & (1U << 11)) type_ |= tAVX512_VNNI;
						if (ecx & (1U << 12)) type_ |= tAVX512_BITALG;
						if (ecx & (1U << 14)) type_ |= tAVX512_VPOPCNTDQ;
						if (edx & (1U << 8)) type_ |= tAVX512_VP2INTERSECT;
						if ((type_ & tAVX512BW) && (edx & (1U << 23))) type_ |= tAVX512_FP16;
					}
				}
			}
		}
		if (maxNum >= 7) {
			getCpuidEx(7, 0, data);
			const uint32_t maxNumSubLeaves = eax;
			if (type_ & tAVX && (ebx & (1U << 5))) type_ |= tAVX2;
			if (ebx & (1U << 3)) type_ |= tBMI1;
			if (ebx & (1U << 4)) type_ |= tHLE;
			if (ebx & (1U << 8)) type_ |= tBMI2;
			if (ebx & (1U << 9)) type_ |= tENHANCED_REP;
			if (ebx & (1U << 11)) type_ |= tRTM;
			if (ebx & (1U << 14)) type_ |= tMPX;
			if (ebx & (1U << 18)) type_ |= tRDSEED;
			if (ebx & (1U << 19)) type_ |= tADX;
			if (ebx & (1U << 20)) type_ |= tSMAP;
			if (ebx & (1U << 23)) type_ |= tCLFLUSHOPT;
			if (ebx & (1U << 24)) type_ |= tCLWB;
			if (ebx & (1U << 29)) type_ |= tSHA;
			if (ecx & (1U << 5)) type_ |= tWAITPKG;
			if (ecx & (1U << 8)) type_ |= tGFNI;
			if (ecx & (1U << 9)) type_ |= tVAES;
			if (ecx & (1U << 10)) type_ |= tVPCLMULQDQ;
			if (ecx & (1U << 23)) type_ |= tKEYLOCKER;
			if (ecx & (1U << 25)) type_ |= tCLDEMOTE;
			if (ecx & (1U << 27)) type_ |= tMOVDIRI;
			if (ecx & (1U << 28)) type_ |= tMOVDIR64B;
			if (edx & (1U << 5)) type_ |= tUINTR;
			if (edx & (1U << 14)) type_ |= tSERIALIZE;
			if (edx & (1U << 15)) type_ |= tHYBRID;
			if (edx & (1U << 16)) type_ |= tTSXLDTRK;
			if (edx & (1U << 22)) type_ |= tAMX_BF16;
			if (edx & (1U << 24)) type_ |= tAMX_TILE;
			if (edx & (1U << 25)) type_ |= tAMX_INT8;
			if (maxNumSubLeaves >= 1) {
				getCpuidEx(7, 1, data);
				if (eax & (1U << 0)) type_ |= tSHA512;
				if (eax & (1U << 1)) type_ |= tSM3;
				if (eax & (1U << 2)) type_ |= tSM4;
				if (eax & (1U << 3)) type_ |= tRAO_INT;
				if (eax & (1U << 4)) type_ |= tAVX_VNNI;
				if (type_ & tAVX512F) {
					if (eax & (1U << 5)) type_ |= tAVX512_BF16;
				}
				if (eax & (1U << 7)) type_ |= tCMPCCXADD;
				if (eax & (1U << 21)) type_ |= tAMX_FP16;
				if (eax & (1U << 23)) type_ |= tAVX_IFMA;
				if (eax & (1U << 31)) type_ |= tMOVRS;
				if (edx & (1U << 4)) type_ |= tAVX_VNNI_INT8;
				if (edx & (1U << 5)) type_ |= tAVX_NE_CONVERT;
				if (edx & (1U << 8)) type_ |= tAMX_COMPLEX;
				if (edx & (1U << 10)) type_ |= tAVX_VNNI_INT16;
				if (edx & (1U << 14)) type_ |= tPREFETCHITI;
				if (edx & (1U << 19)) type_ |= tAVX10;
				if (edx & (1U << 21)) type_ |= tAPX_F;

				getCpuidEx(0x1e, 1, data);
				if (eax & (1U << 4)) type_ |= tAMX_FP8;
				if (eax & (1U << 5)) type_ |= tAMX_TRANSPOSE;
				if (eax & (1U << 6)) type_ |= tAMX_TF32;
				if (eax & (1U << 7)) type_ |= tAMX_AVX512;
				if (eax & (1U << 8)) type_ |= tAMX_MOVRS;
			}
		}
		if (maxNum >= 0x19) {
			getCpuidEx(0x19, 0, data);
			if (ebx & (1U << 0)) type_ |= tAESKLE;
			if (ebx & (1U << 2)) type_ |= tWIDE_KL;
			if (type_ & (tKEYLOCKER|tAESKLE|tWIDE_KL)) type_ |= tKEYLOCKER_WIDE;
		}
		if (has(tAVX10) && maxNum >= 0x24) {
			getCpuidEx(0x24, 0, data);
			avx10version_ = ebx & mask(7);
		}
		setFamily();
		setNumCores();
		setCacheHierarchy();
	}
	void putFamily() const
	{
#ifndef XBYAK_ONLY_CLASS_CPU
		printf("family=%d, model=%X, stepping=%d, extFamily=%d, extModel=%X\n",
			family, model, stepping, extFamily, extModel);
		printf("display:family=%X, model=%X\n", displayFamily, displayModel);
#endif
	}
	bool has(const Type& type) const
	{
		return (type & type_) == type;
	}
	int getAVX10version() const { return avx10version_; }
};
#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#ifndef XBYAK_ONLY_CLASS_CPU
#if XBYAK_CPU_CACHE == 1

enum CoreType {
	Unknown,
	Performance, // P-core (Intel)
	Efficient, // E-core (Intel)
	Standard // Non-hybrid
};

inline const char *getCoreTypeStr(int coreType)
{
	switch (coreType) {
	case Performance: return "P-core";
	case Efficient: return "E-core";
	case Standard: return "Standard";
	default: return "Unknown";
	}
}

enum CacheType {
	L1i,
	L1d,
	L2,
	L3,
	CACHE_UNKNOWN,
	CACHE_TYPE_NUM = CACHE_UNKNOWN
};

inline const char* getCacheTypeStr(int type)
{
	switch (type) {
	case L1i: return "L1i";
	case L1d: return "L1d";
	case L2: return "L2";
	case L3: return "L3";
	default: return "Unknown";
	}
}

namespace impl {

inline void appendStr(std::string& s, uint32_t v)
{
#if __cplusplus >= 201103L
	s += std::to_string(v);
#else
	char buf[16];
	snprintf(buf, sizeof(buf), "%u", v);
	s += buf;
#endif
}

// str = "(int|range)[,(int|range)]*"
// range = int-int
// e.g. "1,3,5", "0-3,5-7", ""
template<class T>
bool setStr(T& x, const char *str)
{
	const char *p = str;
	while (*p) {
		if (p != str) {
			if (*p != ',') return false;
			p++;
		}
		char *endp;
		uint32_t v = uint32_t(strtoul(p, &endp, 10));
		if (endp == p) return false;
		if (*endp == '-') {
			const char *rangeStart = endp + 1;
			uint32_t next = uint32_t(strtoul(rangeStart, &endp, 10));
			if (endp == rangeStart) return false;
			if (!x.appendRange(v, next)) return false;
		} else {
			if (!x.append(v)) return false;
		}
		if (*endp == '\0') return true;
		p = endp;
	}
	return true;
}

} // impl

#ifndef XBYAK_CPUMASK_N
#define XBYAK_CPUMASK_N 6
#endif
#ifndef XBYAK_CPUMASK_BITN
#define XBYAK_CPUMASK_BITN 10 // max number of logical cpu = 1024
#endif
#if XBYAK_CPUMASK_COMPACT == 1
/*
	a_ is treated as an array of N elements, each being bitN bits
	a_ = 1<<bitN and n_ = 0 and range_ = 0 means empty set
	n_ is length of a_[] - 1
	When range_ is false (discrete values):
		Values satisfy a_[i] + 1 < a_[i+1] for all 0 <= i <= n_
	When range_ is true (intervals):
		v = a_[i*2] is the start of the interval
		n = a_[i*2+1] is the interval length - 1
		Represents the interval [v, v+n]
	Max number of cpu = 2**bitN - 1
	Max value that can be stored = N
	Max interval length = N/2
*/
class CpuMask {
	static const uint32_t N = XBYAK_CPUMASK_N;
	static const uint32_t bitN = XBYAK_CPUMASK_BITN;
	static const uint64_t mask = (uint64_t(1) << bitN) - 1;
	uint64_t a_:N*bitN;
	uint64_t n_:3;
	uint64_t range_:1;

	// Set a_[idx] = v
	void set_a(size_t idx, uint32_t v)
	{
		assert(idx < N);
		assert(v <= mask);
		a_ &= ~(mask << (idx*bitN));
		a_ |= (v & mask) << (idx*bitN);
	}
	// Get a_[idx]
	uint32_t get_a(size_t idx) const
	{
		assert(idx < N);
		return (a_ >> (idx*bitN)) & mask;
	}
#ifndef NDEBUG
	// Return true if the idx-th value exists
	bool hasNext(uint32_t idx) const
	{
		if (empty()) return false;
		if (!range_) return idx <= n_;
		uint32_t n = 0;
		for (uint32_t i = 1; i <= n_; i += 2) {
			n += get_a(i) + 1;
			if (idx < n) return true;
		}
		return false;
	}
#endif
public:
	CpuMask() { clear(); }
	class ConstIterator {
		const CpuMask& parent_;
		uint32_t idx_;
		uint32_t size_;
		friend class CpuMask;
	public:
		ConstIterator(const CpuMask& parent)
			: parent_(parent), idx_(0), size_(uint32_t(parent.size())) {}
		uint32_t operator*() const { return parent_.get(idx_); }
		ConstIterator& operator++() { idx_++; return *this; }
		bool operator==(const ConstIterator& rhs) const { return idx_ == rhs.idx_; }
		bool operator!=(const ConstIterator& rhs) const { return !operator==(rhs); }
	};
	ConstIterator begin() const { return ConstIterator(*this); }
	ConstIterator end() const {
		ConstIterator it(*this);
		it.idx_ = uint32_t(size());
		return it;
	}
	typedef ConstIterator iterator;
	typedef ConstIterator const_iterator;
	void clear() { a_ = 1 << bitN; n_ = 0; range_ = 0; }
	bool empty() const
	{
		return a_ == 1 << bitN && n_ == 0 && range_ == 0;
	}
	uint64_t to_u64() const { return a_ | (uint64_t(n_) << (N * bitN)) | (uint64_t(range_) << (N * bitN + 3)); }
	bool operator<(const CpuMask& rhs) const { return to_u64() < rhs.to_u64(); }
	bool operator>(const CpuMask& rhs) const { return to_u64() > rhs.to_u64(); }
	bool operator>=(const CpuMask& rhs) const { return !operator<(rhs); }
	bool operator<=(const CpuMask& rhs) const { return !operator>(rhs); }
	bool operator==(const CpuMask& rhs) const { return to_u64() == rhs.to_u64(); }
	bool operator!=(const CpuMask& rhs) const { return !operator==(rhs); }
	// Add element v
	// v should be monotonically increasing
	bool append(uint32_t v)
	{
		uint32_t prev = 0, n = 0;
		if (v > mask) goto ERR;
		// When adding for the first time, treat as discrete value
		if (empty()) {
			a_ = v;
			n_ = 0;
			return true;
		}
		if (!range_) {
			prev = get_a(n_);
			if (v <= prev) goto ERR;
			// If there's one discrete value and it forms an interval with the new value, switch to interval mode
			if (n_ == 0 && prev + 1 == v) {
				set_a(1, 1);
				range_ = 1;
				n_ = 1;
				return true;
			}
			if (n_ >= N - 1) goto ERR;
			// Add discrete value
			n_++;
			set_a(n_, v);
			return true;
		}
		// If the value to add is 1 greater than the end of the current interval
		n = get_a(n_);
		prev = get_a(n_ - 1) + n;
		if (prev >= v) goto ERR;
		if (prev + 1 == v) {
			// Increase the interval length by one
			set_a(n_, n + 1);
			return true;
		} else {
			if (n_ >= N - 1) goto ERR;
			// If not continuous with the previous interval
			// Add a new interval [v]
			set_a(n_ + 1, v);
			n_ += 2;
			return true;
		}
	ERR:
		XBYAK_THROW_RET(ERR_INVALID_CPUMASK_INDEX, false)
	}
	// add range [a, b] which means a, a+1, ..., b
	bool appendRange(uint32_t a, uint32_t b)
	{
		if ((empty() || (range_ && n_ < N - 1)) && (a <= b && b <= mask)) {
			range_ = true;
			n_ += n_ == 0 ? 1 : 2;
			set_a(n_ - 1, a);
			set_a(n_, b - a);
			return true;
		}
		return false;
	}
	// str = "(int|range)[,(int|range)]*"
	// range = int-int
	bool setStr(const char *str)
	{
		return impl::setStr(*this, str);
	}
	bool setStr(const std::string& str) { return setStr(str.c_str()); }
	std::string getStr() const
	{
		std::string s;
		if (empty()) return s;
		if (!range_) {
			for (uint32_t i = 0; i <= n_; i++) {
				if (!s.empty()) s += ",";
				impl::appendStr(s, get_a(i));
			}
			return s;
		}
		for (uint32_t i = 0; i <= n_; i += 2) {
			uint32_t v = get_a(i);
			uint32_t len = get_a(i + 1);
			if (!s.empty()) s += ",";
			impl::appendStr(s, v);
			if (len > 0) {
				s += "-";
				impl::appendStr(s, v + len);
			}
		}
		return s;
	}
	size_t size() const
	{
		if (empty()) return 0;
		if (!range_) return n_ + 1;
		size_t n = 0;
		for (uint32_t i = 1; i <= n_; i += 2) {
			n += get_a(i) + 1;
		}
		return n;
	}

	uint32_t get(uint32_t idx) const
	{
		assert(hasNext(idx));
		if (!range_) return get_a(idx);
		uint32_t n = 0;
		for (uint32_t i = 1; i <= n_; i += 2) {
			uint32_t range = get_a(i) + 1;
			if (idx < n + range) {
				return get_a(i - 1) + (idx - n);
			}
			n += range;
		}
		return false;
	}
	void dump() const
	{
		printf("a_:");
		for (int i = int(N) - 1; i >= 0; i--) {
			printf("%u ", uint32_t((a_ >> (i * bitN)) & mask));
		}
		printf("\n");
		printf("n_: %u\n", (uint32_t)n_);
		printf("range_: %u\n", (uint32_t)range_);
	}
	void put(const char *label = NULL) const
	{
		if (label) printf("%s: ", label);
		printf("%s\n", getStr().c_str());
	}
};
#else
class CpuMask {
	typedef std::set<uint32_t> IntSet;
	IntSet indices_;
public:
	CpuMask() : indices_() {}
	typedef IntSet::const_iterator const_iterator;
	typedef const_iterator iterator;
	const_iterator begin() const { return indices_.begin(); }
	const_iterator end() const { return indices_.end(); }

	void clear() { indices_.clear(); }
	bool empty() const { return indices_.empty(); }
	bool operator<(const CpuMask& rhs) const { return indices_ < rhs.indices_; }
	bool operator>(const CpuMask& rhs) const { return indices_ > rhs.indices_; }
	bool operator>=(const CpuMask& rhs) const { return !operator<(rhs); }
	bool operator<=(const CpuMask& rhs) const { return !operator>(rhs); }
	bool operator==(const CpuMask& rhs) const { return indices_ == rhs.indices_; }
	bool operator!=(const CpuMask& rhs) const { return !operator==(rhs); }
	// idx should be monotonically increasing
	bool append(uint32_t idx)
	{
		if (idx >= (1u << XBYAK_CPUMASK_BITN)) return false;
		if (!indices_.empty() && *indices_.rbegin() >= idx) return false;
		indices_.insert(idx);
		return true;
	}
	// add range [a, b] which means a, a+1, ..., b
	bool appendRange(uint32_t a, uint32_t b)
	{
		if (a > b) return false;
		while (a <= b) {
			if (!append(a)) return false;
			a++;
		}
		return true;
	}
	bool setStr(const char *str)
	{
		return impl::setStr(*this, str);
	}
	bool setStr(const std::string& str) { return setStr(str.c_str()); }
	std::string getStr() const
	{
		std::string s;
		bool inRange = false;
		uint32_t prev = 0x80000000;
		for (const_iterator i = indices_.begin(); i != indices_.end(); ++i) {
			uint32_t v = *i;
			if (inRange) {
				if (prev + 1 != v) {
					impl::appendStr(s, prev);
					inRange = false;
					s += ',';
					impl::appendStr(s, v);
				}
			} else {
				if (prev + 1 == v) {
					// start range
					s += '-';
					inRange = true;
				} else {
					if (!s.empty()) s += ',';
					impl::appendStr(s, v);
				}
			}
			prev = v;
		}
		if (inRange) {
			impl::appendStr(s, prev);
		}
		return s;
	}
	size_t size() const { return indices_.size(); }
	uint32_t get(uint32_t idx) const
	{
		assert(idx < size());
		const_iterator it = indices_.begin();
		std::advance(it, idx);
		return *it;
	}
	void put(const char *label = NULL) const
	{
		if (label) printf("%s: ", label);
		printf("%s\n", getStr().c_str());
	}
};
#endif

class CpuCache {
public:
	CpuCache() : size(0), associativity(0) {}

	// Cache size in bytes
	uint32_t size;

	// number of ways of associativity
	uint32_t associativity;

	// Set of logical CPU indices sharing this cache
	CpuMask sharedCpuIndices;

	// Whether this is a shared cache
	bool isShared() const { return sharedCpuIndices.size() > 1; }

	// Number of logical CPUs sharing this cache
	size_t getSharedCpuNum() const { return sharedCpuIndices.size(); }

	void put(const char *label = NULL) const
	{
		if (label) printf("%s: ", label);
		printf("%u KiB, assoc. %u, shared ", size / 1024, associativity);
		sharedCpuIndices.put();
	}
};

struct LogicalCpu {
	LogicalCpu()
		: coreId(0)
		, coreType(Unknown)
		, cache()
	{
	}
	uint32_t coreId; // index of physical core
	CoreType coreType; // for hybrid systems
	CpuCache cache[CACHE_TYPE_NUM];
	const CpuMask& getSiblings() const { return cache[L1i].sharedCpuIndices; }

	void put(const char *label = NULL) const
	{
		if (label) printf("%s: ", label);
		printf("coreId %u, type %s\n", coreId, getCoreTypeStr(coreType));
		for (int i = 0; i < CACHE_TYPE_NUM; i++) {
			cache[i].put(getCacheTypeStr(i));
		}
	}
};

class CpuTopology {
public:
	explicit CpuTopology(const Cpu& cpu)
		: logicalCpus_()
		, physicalCoreNum_(0)
		, lineSize_(0)
		, isHybrid_(cpu.has(cpu.tHYBRID))
	{
		if (!impl::initCpuTopology(*this)) {
			XBYAK_THROW(ERR_CANT_INIT_CPUTOPOLOGY);
		}
	}

	// Number of logical CPUs
	size_t getLogicalCpuNum() const { return logicalCpus_.size(); }

	// Number of physical cores
	size_t getPhysicalCoreNum() const { return physicalCoreNum_; }

	// Cache line size in bytes
	uint32_t getLineSize() const { return lineSize_; }

	// Get logical CPU information
	const LogicalCpu& getLogicalCpu(size_t cpuIdx) const
	{
		return logicalCpus_[cpuIdx];
	}

	// Get cache information for a specific logical CPU
	const CpuCache& getCache(size_t cpuIdx, CacheType type) const
	{
		return logicalCpus_[cpuIdx].cache[type];
	}

	// Whether this is a hybrid system
	bool isHybrid() const { return isHybrid_; }
private:
	friend bool impl::initCpuTopology(CpuTopology&);
	std::vector<LogicalCpu> logicalCpus_;
	size_t physicalCoreNum_;
	uint32_t lineSize_;
	bool isHybrid_;
};

namespace impl {

inline uint32_t popcnt(uint64_t mask)
{
#if defined(_M_X64) || defined(_M_AMD64)
	return (int)__popcnt64(mask);
#elif defined(__GNUC__) || defined(__clang__)
	return __builtin_popcountll(mask);
#else
	uint32_t count = 0;
	while (mask) {
		count += (mask & 1);
		mask >>= 1;
	}
	return count;
#endif
}

// fall back to CPUID leaf 0x1A
inline CoreType getCoreType()
{
	uint32_t data[4] = {};
	Cpu::getCpuidEx(0x1A, 0, data);
	const uint32_t coreTypeField = (data[0] >> 24) & 0xFF;
	if (coreTypeField == 0x40) return Performance; // P-core
	if (coreTypeField == 0x20) return Efficient; // E-core
	return Standard;
}

#ifdef _WIN32

typedef std::vector<uint32_t> U32Vec;

#if (defined(NTDDI_VERSION) && NTDDI_VERSION >= 0x06010000) || (defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0601)
	#define XBYAK_WINSDK_HAS_RELATIONSHIP_GROUP_AFFINITY 1
#else
	#define XBYAK_WINSDK_HAS_RELATIONSHIP_GROUP_AFFINITY 0
#endif

#if (defined(NTDDI_VERSION) && NTDDI_VERSION >= 0x0A000000) || (defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0A00)
	#define XBYAK_WINSDK_HAS_EFFICIENCY_CLASS 1
#else
	#define XBYAK_WINSDK_HAS_EFFICIENCY_CLASS 0
#endif

// GroupMasks[] / GroupCount on CACHE_RELATIONSHIP added in Win10 20H1 (SDK 10.0.19041, NTDDI_WIN10_VB)
// NOTE: _WIN32_WINNT has no sub-version granularity for Win10, so only
// NTDDI_VERSION can distinguish 20H1 (0x0A00000C) from earlier Win10 builds.
// If NTDDI_VERSION is not set, this macro will be 0 (safe/conservative fallback).
#if defined(NTDDI_VERSION) && NTDDI_VERSION >= 0x0A00000C
	#define XBYAK_WINSDK_HAS_CACHE_RELATIONSHIP_GROUPMASKS 1
#else
	#define XBYAK_WINSDK_HAS_CACHE_RELATIONSHIP_GROUPMASKS 0
#endif

#if XBYAK_WINSDK_HAS_RELATIONSHIP_GROUP_AFFINITY
typedef SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ProcInfo;

inline CoreType getCoreTypeForAffinity(const GROUP_AFFINITY& affinity)
{
	GROUP_AFFINITY previousMask = {};
	if (!SetThreadGroupAffinity(GetCurrentThread(), &affinity, &previousMask)) {
		return Standard;
	}
	CoreType type = impl::getCoreType();
	SetThreadGroupAffinity(GetCurrentThread(), &previousMask, NULL);
	return type;
}

// return total logical cpus if sucessful, 0 if failed
inline uint32_t getGroupAcc(U32Vec& v)
{
	DWORD len = 0;
	GetLogicalProcessorInformationEx(RelationGroup, NULL, &len);
	std::vector<char> buf(len);
	if (!GetLogicalProcessorInformationEx(RelationGroup, reinterpret_cast<ProcInfo*>(buf.data()), &len)) {
		return 0;
	}
	const auto& entry = *reinterpret_cast<const ProcInfo*>(buf.data());
	const GROUP_RELATIONSHIP& gr = entry.Group;

	const uint32_t n = gr.ActiveGroupCount;
	if (n == 0) return 0;

	v.resize(n);

	uint32_t acc = 0;
	for (uint32_t g = 0; g < n; g++) {
		v[g] = acc;
		acc += gr.GroupInfo[g].ActiveProcessorCount;
	}
	return acc;
}

// return number of physical cores if successful, 0 if failed
static inline uint32_t getCores(std::vector<LogicalCpu>& cpus, bool isHybrid, const U32Vec& groupAcc) {
	DWORD len = 0;
	GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &len);
	std::vector<char> buf(len);
	if (!GetLogicalProcessorInformationEx(RelationProcessorCore, reinterpret_cast<ProcInfo*>(buf.data()), &len)) return 0;

	// get core indices
	const char *p = buf.data();
	const char *end = p + len;
	uint32_t coreIdx = 0;

	while (p < end) {
		const auto& entry = *reinterpret_cast<const ProcInfo*>(p);
		if (entry.Relationship == RelationProcessorCore) {
			const PROCESSOR_RELATIONSHIP& core = entry.Processor;
			LogicalCpu cpu;
			cpu.coreId = coreIdx++;
			if (!isHybrid) {
				cpu.coreType = Standard;
			} else {
#if XBYAK_WINSDK_HAS_EFFICIENCY_CLASS
				cpu.coreType = core.EfficiencyClass > 0 ? Performance : Efficient;
#else
				cpu.coreType = getCoreTypeForAffinity(core.GroupMask[0]);
#endif
			}

			const GROUP_AFFINITY* masks = core.GroupMask;
			for (WORD i = 0; i < core.GroupCount; i++) {
				const WORD group = masks[i].Group;
				const KAFFINITY m = masks[i].Mask;
				const uint32_t base = groupAcc[group];

				for (uint32_t b = 0; b < sizeof(KAFFINITY) * 8; b++) {
					if (m & (KAFFINITY(1) << b)) {
						const uint32_t idx = base + b;
						if (idx >= cpus.size()) return 0;
						cpus[idx] = cpu;
					}
				}
			}
		}
		p += entry.Size;
	}
	return coreIdx;
}

inline bool convertMask(CpuMask& mask, const U32Vec& groupAcc, const CACHE_RELATIONSHIP& cache)
{
#if XBYAK_WINSDK_HAS_CACHE_RELATIONSHIP_GROUPMASKS
	const WORD count = cache.GroupCount;
#else
	const WORD count = 1;
#endif
	for (WORD i = 0; i < count; i++) {
#if XBYAK_WINSDK_HAS_CACHE_RELATIONSHIP_GROUPMASKS
		const GROUP_AFFINITY& cg = cache.GroupMasks[i];
#else
		const GROUP_AFFINITY& cg = cache.GroupMask;
#endif
		const KAFFINITY m = cg.Mask;
		const uint32_t base = groupAcc[cg.Group];
		for (uint32_t b = 0; b < sizeof(KAFFINITY) * 8; b++) {
			if (m & (KAFFINITY(1) << b)) {
				if (!mask.append(base + b)) return false;
			}
		}
	}
	return true;
}

inline bool initCpuTopology(CpuTopology& cpuTopo)
{
	U32Vec groupAcc;
	const uint32_t logicalCpuNum = getGroupAcc(groupAcc);
	if (logicalCpuNum == 0) return false;
	if (logicalCpuNum >= (1u << XBYAK_CPUMASK_BITN)) return false;

	cpuTopo.logicalCpus_.resize(logicalCpuNum);
	cpuTopo.physicalCoreNum_ = getCores(cpuTopo.logicalCpus_, cpuTopo.isHybrid(), groupAcc);
	if (cpuTopo.physicalCoreNum_ == 0) return false;

	DWORD len = 0;
	GetLogicalProcessorInformationEx(RelationCache, NULL, &len);
	std::vector<char> buf(len);
	if (!GetLogicalProcessorInformationEx(RelationCache, reinterpret_cast<ProcInfo*>(buf.data()), &len)) return false;

	const char *p = buf.data();
	const char *end = p + len;

	while (p < end) {
		const auto& entry = *reinterpret_cast<const ProcInfo*>(p);
		if (entry.Relationship == RelationCache) {
			const CACHE_RELATIONSHIP& cache = entry.Cache;
			uint32_t type = CACHE_UNKNOWN;
			if (cache.Level == 1) {
				if (cache.Type == CacheInstruction) {
					type = L1i;
				} else if (cache.Type == CacheData) {
					type = L1d;
				}
			} else if (cache.Level == 2) {
				type = L2;
			} else if (cache.Level == 3) {
				type = L3;
			}
			if (type != CACHE_UNKNOWN) {
				CpuMask mask;
				if (!convertMask(mask, groupAcc, cache)) return false;
				for (const auto& i : mask) {
					if (i >= cpuTopo.logicalCpus_.size()) return false;
					cpuTopo.logicalCpus_[i].cache[type].size = cache.CacheSize;
					if (cpuTopo.lineSize_ == 0) cpuTopo.lineSize_ = cache.LineSize;
					cpuTopo.logicalCpus_[i].cache[type].associativity = cache.Associativity;
					cpuTopo.logicalCpus_[i].cache[type].sharedCpuIndices = mask;
				}
			}
		}
		p += entry.Size;
	}
	return true;
}
#else
inline bool initCpuTopology(CpuTopology& cpuTopo)
{
	(void)cpuTopo;
	return false;
}
#endif
// unset WinSDK version macros to avoid Macro pollution
#undef XBYAK_WINSDK_HAS_RELATIONSHIP_GROUP_AFFINITY
#undef XBYAK_WINSDK_HAS_EFFICIENCY_CLASS
#undef XBYAK_WINSDK_HAS_CACHE_RELATIONSHIP_GROUPMASKS
#elif defined(__linux__) // Linux

struct WrapFILE {
	FILE *f;
	explicit WrapFILE(const char *name)
		: f(fopen(name, "r"))
	{
	}
	~WrapFILE() { if (f) fclose(f); }
};

inline uint32_t readIntFromFile(const char* path) {
	WrapFILE wf(path);
	if (!wf.f) return 0;
	uint32_t val = 0;
	int n = fscanf(wf.f, "%u", &val);
	return (n == 1) ? val : 0;
}

inline bool parseCpuList(CpuMask& mask, const char* path) {
	WrapFILE wf(path);
	if (!wf.f) return false;
	char buf[1024];
	if (!fgets(buf, sizeof(buf), wf.f)) return false;
	size_t n = strlen(buf);
	if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';
	return setStr(mask, buf);
}

inline CoreType setAffinityAndGetCoreType(uint32_t cpu)
{
	cpu_set_t cpuMask;
	CPU_ZERO(&cpuMask);
	CPU_SET(cpu, &cpuMask);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuMask)) return Standard;
	return impl::getCoreType();
}

inline bool initCpuTopology(CpuTopology& cpuTopo)
{
	const uint32_t logicalCpuNum = sysconf(_SC_NPROCESSORS_ONLN);

	if (logicalCpuNum == 0) return false;
	if (logicalCpuNum >= (1u << XBYAK_CPUMASK_BITN)) return false;

	cpuTopo.logicalCpus_.resize(logicalCpuNum);
	uint32_t maxPhisicalIdx = 0;

	for (uint32_t cpuIdx = 0; cpuIdx < logicalCpuNum; cpuIdx++) {
		char path[256];
		LogicalCpu& logCpu = cpuTopo.logicalCpus_[cpuIdx];

		snprintf(path, sizeof(path),
			"/sys/devices/system/cpu/cpu%u/topology/core_id", cpuIdx);
		logCpu.coreId = readIntFromFile(path);
		maxPhisicalIdx = (std::max)(maxPhisicalIdx, logCpu.coreId);

		logCpu.coreType = Standard;

		for (uint32_t cacheIdx = 0; cacheIdx < CACHE_TYPE_NUM; cacheIdx++) {
			CacheType cacheType = CACHE_UNKNOWN;

			// Map cache index to cache type
			{
				snprintf(path, sizeof(path),
					"/sys/devices/system/cpu/cpu%u/cache/index%u/type", cpuIdx, cacheIdx);
				char typeStr[32];
				WrapFILE wf(path);

				if (wf.f && fgets(typeStr, sizeof(typeStr), wf.f)) {
					if (strncmp(typeStr, "Instruction", 11) == 0) {
						cacheType = L1i;
					} else if (strncmp(typeStr, "Data", 4) == 0) {
						// Determine level
						char path[256];
						snprintf(path, sizeof(path),
							"/sys/devices/system/cpu/cpu%u/cache/index%u/level", cpuIdx, cacheIdx);
						switch (readIntFromFile(path)) {
						case 1: cacheType = L1d; break;
						case 2: cacheType = L2; break;
						case 3: cacheType = L3; break;
						default: break;;
						}
					} else if (strncmp(typeStr, "Unified", 7) == 0) {
						snprintf(path, sizeof(path),
							"/sys/devices/system/cpu/cpu%u/cache/index%u/level", cpuIdx, cacheIdx);
						switch (readIntFromFile(path)) {
						case 2: cacheType = L2; break;
						case 3: cacheType = L3; break;
						default: break;;
						}
					}
				}
			}
			if (cacheType == CACHE_UNKNOWN) continue;
			CpuCache& cache = logCpu.cache[cacheType];

			// Read cache size
			{
				snprintf(path, sizeof(path),
					"/sys/devices/system/cpu/cpu%u/cache/index%u/size", cpuIdx, cacheIdx);
				char sizeStr[32];
				WrapFILE wf(path);
				if (wf.f && fgets(sizeStr, sizeof(sizeStr), wf.f)) {
					char *endp;
					uint32_t size = (uint32_t)strtoul(sizeStr, &endp, 10);
					switch (*endp) {
					case '\0': case '\n': cache.size = size; break;
					case 'K': case 'k':   cache.size = size * 1024; break;
					case 'M': case 'm':   cache.size = size * 1024 * 1024; break;
					default: break;
					}
				}
			}

			// Read ways of associativity
			snprintf(path, sizeof(path),
				"/sys/devices/system/cpu/cpu%u/cache/index%u/ways_of_associativity", cpuIdx, cacheIdx);
			cache.associativity = readIntFromFile(path);

			// Read shared CPU list
			snprintf(path, sizeof(path),
				"/sys/devices/system/cpu/cpu%u/cache/index%u/shared_cpu_list", cpuIdx, cacheIdx);
			parseCpuList(cache.sharedCpuIndices, path);

		}
	}

	// Assign core types for hybrid architectures
	const bool isHybrid = cpuTopo.isHybrid();
	if (isHybrid) {
		// For hybrid systems, try toread P-core and E-core lists from sysfs first
		CpuMask pCoreMask;
		const bool hasPCoreSysfs = parseCpuList(pCoreMask, "/sys/devices/cpu_core/cpus");
		if (hasPCoreSysfs) {
			// Set Performance core types
			for (CpuMask::const_iterator it = pCoreMask.begin(); it != pCoreMask.end(); ++it) {
				uint32_t cpuIdx = *it;
				if (cpuIdx < logicalCpuNum) {
					cpuTopo.logicalCpus_[cpuIdx].coreType = Performance;
				}
			}
		}
		CpuMask eCoreMask;
		const bool hasECoreSysfs = parseCpuList(eCoreMask, "/sys/devices/cpu_atom/cpus");
		if (hasECoreSysfs) {
			// Set Efficient core types
			for (CpuMask::const_iterator it = eCoreMask.begin(); it != eCoreMask.end(); ++it) {
				uint32_t cpuIdx = *it;
				if (cpuIdx < logicalCpuNum) {
					cpuTopo.logicalCpus_[cpuIdx].coreType = Efficient;
				}
			}
		}
		// Fallback: if either sysfs paths are unavailable, detect both core type per-CPU
		if (!hasPCoreSysfs || !hasECoreSysfs) {
			cpu_set_t originalMask;
			CPU_ZERO(&originalMask);
			if (sched_getaffinity(0, sizeof(cpu_set_t), &originalMask) == 0) {
				for (uint32_t cpu = 0; cpu < logicalCpuNum; cpu++) {
					cpuTopo.logicalCpus_[cpu].coreType = impl::setAffinityAndGetCoreType(cpu);
				}
				sched_setaffinity(0, sizeof(cpu_set_t), &originalMask);
			}
		}
	}

	// Read coherency line size
	cpuTopo.lineSize_ = readIntFromFile("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");

	cpuTopo.physicalCoreNum_ = maxPhisicalIdx + 1;
	return true;
}
#else // Other OS (e.g., macOS)
inline bool initCpuTopology(CpuTopology& cpuTopo)
{
	// CPU topology detection not yet implemented
	(void)cpuTopo;
	return false;
}
#endif // _WIN32 / __linux__ / other OS

} // namespace impl
#endif // XBYAK_CPU_CACHE

class Clock {
public:
	static inline uint64_t getRdtsc()
	{
#ifdef XBYAK_INTEL_CPU_SPECIFIC
	#ifdef _MSC_VER
		return __rdtsc();
	#else
		uint32_t eax, edx;
		__asm__ volatile("rdtsc" : "=a"(eax), "=d"(edx));
		return ((uint64_t)edx << 32) | eax;
	#endif
#else
		// TODO: Need another impl of Clock or rdtsc-equivalent for non-x86 cpu
		return 0;
#endif
	}
	Clock()
		: clock_(0)
		, count_(0)
	{
	}
	void begin()
	{
		clock_ -= getRdtsc();
	}
	void end()
	{
		clock_ += getRdtsc();
		count_++;
	}
	int getCount() const { return count_; }
	uint64_t getClock() const { return clock_; }
	void clear() { count_ = 0; clock_ = 0; }
private:
	uint64_t clock_;
	int count_;
};

#ifdef XBYAK64

class Pack {
	static const size_t maxTblNum = 15;
	Xbyak::Reg64 tbl_[maxTblNum];
	size_t n_;
public:
	Pack() : tbl_(), n_(0) {}
	Pack(const Xbyak::Reg64 *tbl, size_t n) { init(tbl, n); }
	Pack(const Pack& rhs)
		: n_(rhs.n_)
	{
		for (size_t i = 0; i < n_; i++) tbl_[i] = rhs.tbl_[i];
	}
	Pack& operator=(const Pack& rhs)
	{
		n_ = rhs.n_;
		for (size_t i = 0; i < n_; i++) tbl_[i] = rhs.tbl_[i];
		return *this;
	}
	Pack(const Xbyak::Reg64& t0)
	{ n_ = 1; tbl_[0] = t0; }
	Pack(const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 2; tbl_[0] = t0; tbl_[1] = t1; }
	Pack(const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 3; tbl_[0] = t0; tbl_[1] = t1; tbl_[2] = t2; }
	Pack(const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 4; tbl_[0] = t0; tbl_[1] = t1; tbl_[2] = t2; tbl_[3] = t3; }
	Pack(const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 5; tbl_[0] = t0; tbl_[1] = t1; tbl_[2] = t2; tbl_[3] = t3; tbl_[4] = t4; }
	Pack(const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 6; tbl_[0] = t0; tbl_[1] = t1; tbl_[2] = t2; tbl_[3] = t3; tbl_[4] = t4; tbl_[5] = t5; }
	Pack(const Xbyak::Reg64& t6, const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 7; tbl_[0] = t0; tbl_[1] = t1; tbl_[2] = t2; tbl_[3] = t3; tbl_[4] = t4; tbl_[5] = t5; tbl_[6] = t6; }
	Pack(const Xbyak::Reg64& t7, const Xbyak::Reg64& t6, const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 8; tbl_[0] = t0; tbl_[1] = t1; tbl_[2] = t2; tbl_[3] = t3; tbl_[4] = t4; tbl_[5] = t5; tbl_[6] = t6; tbl_[7] = t7; }
	Pack(const Xbyak::Reg64& t8, const Xbyak::Reg64& t7, const Xbyak::Reg64& t6, const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 9; tbl_[0] = t0; tbl_[1] = t1; tbl_[2] = t2; tbl_[3] = t3; tbl_[4] = t4; tbl_[5] = t5; tbl_[6] = t6; tbl_[7] = t7; tbl_[8] = t8; }
	Pack(const Xbyak::Reg64& t9, const Xbyak::Reg64& t8, const Xbyak::Reg64& t7, const Xbyak::Reg64& t6, const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 10; tbl_[0] = t0; tbl_[1] = t1; tbl_[2] = t2; tbl_[3] = t3; tbl_[4] = t4; tbl_[5] = t5; tbl_[6] = t6; tbl_[7] = t7; tbl_[8] = t8; tbl_[9] = t9; }
	Pack(const Xbyak::Reg64& ta, const Xbyak::Reg64& t9, const Xbyak::Reg64& t8, const Xbyak::Reg64& t7, const Xbyak::Reg64& t6, const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 11; tbl_[0] = t0; tbl_[1] = t1; tbl_[2] = t2; tbl_[3] = t3; tbl_[4] = t4; tbl_[5] = t5; tbl_[6] = t6; tbl_[7] = t7; tbl_[8] = t8; tbl_[9] = t9; tbl_[10] = ta; }
	Pack(const Xbyak::Reg64& tb, const Xbyak::Reg64& ta, const Xbyak::Reg64& t9, const Xbyak::Reg64& t8, const Xbyak::Reg64& t7, const Xbyak::Reg64& t6, const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 12; tbl_[0] = t0; tbl_[1] = t1; tbl_[2] = t2; tbl_[3] = t3; tbl_[4] = t4; tbl_[5] = t5; tbl_[6] = t6; tbl_[7] = t7; tbl_[8] = t8; tbl_[9] = t9; tbl_[10] = ta; tbl_[11] = tb; }
	Pack& append(const Xbyak::Reg64& t)
	{
		if (n_ == maxTblNum) {
			fprintf(stderr, "ERR Pack::can't append\n");
			XBYAK_THROW_RET(ERR_BAD_PARAMETER, *this)
		}
		tbl_[n_++] = t;
		return *this;
	}
	void init(const Xbyak::Reg64 *tbl, size_t n)
	{
		if (n > maxTblNum) {
			fprintf(stderr, "ERR Pack::init bad n=%d\n", (int)n);
			XBYAK_THROW(ERR_BAD_PARAMETER)
		}
		n_ = n;
		for (size_t i = 0; i < n; i++) {
			tbl_[i] = tbl[i];
		}
	}
	const Xbyak::Reg64& operator[](size_t n) const
	{
		if (n >= n_) {
			fprintf(stderr, "ERR Pack bad n=%d(%d)\n", (int)n, (int)n_);
			XBYAK_THROW_RET(ERR_BAD_PARAMETER, rax)
		}
		return tbl_[n];
	}
	size_t size() const { return n_; }
	/*
		get tbl[pos, pos + num)
	*/
	Pack sub(size_t pos, size_t num = size_t(-1)) const
	{
		if (num == size_t(-1)) num = n_ - pos;
		if (pos + num > n_) {
			fprintf(stderr, "ERR Pack::sub bad pos=%d, num=%d\n", (int)pos, (int)num);
			XBYAK_THROW_RET(ERR_BAD_PARAMETER, Pack())
		}
		Pack pack;
		pack.n_ = num;
		for (size_t i = 0; i < num; i++) {
			pack.tbl_[i] = tbl_[pos + i];
		}
		return pack;
	}
	void put() const
	{
		for (size_t i = 0; i < n_; i++) {
			printf("%s ", tbl_[i].toString());
		}
		printf("\n");
	}
};

// start from a bit position larger than the number of GPRs
const int UseRBP = 1 << 5;
const int UseRCX = 1 << 6;
const int UseRDX = 1 << 7;
const int UseRSI = 1 << 8;
const int UseRDI = 1 << 9;
const int UseRBPAsFramePointer = UseRBP | (1 << 10);

class StackFrame {
#ifdef XBYAK64_WIN
	static const int noSaveNum = 6;
#else
	static const int noSaveNum = 8;
#endif
	static const int maxPnum = 4;
	static const int maxRegNum = 14; // maxRegNum = 16 - rsp - rax
	static const int calleeSaveNum = maxRegNum - noSaveNum;
	static const int UseMASK = UseRCX|UseRDX|UseRSI|UseRDI|UseRBP;
	Xbyak::CodeGenerator *code_;
	Xbyak::Reg64 pTbl_[maxPnum];
	Xbyak::Reg64 tTbl_[maxRegNum];
	Pack p_;
	Pack t_;
	int pNum_;
	int tNum_;
	int useRegs_;
	int saveNum_;
	int saveRegs_[calleeSaveNum];
	int P_;
	bool makeEpilog_;
	StackFrame(const StackFrame&);
	void operator=(const StackFrame&);
public:
	const Pack& p;
	const Pack& t;
	/*
		make stack frame
		@param sf [in] this
		@param pNum [in] number of function parameters(0 <= pNum <= 4)
		@param tNum [in] number of temporary registers(0 <= tNum, can be OR-ed with Use{RCX,RDX,RSI,RDI,RBP}, e.g., 3|UseRCX)
		@param stackSizeByte [in] local stack size
		@param makeEpilog [in] automatically call close() if true

		pNum + tNum + #Use must be <= 14

		you can use
		rax
		p[0], ..., p[pNum-1] as function parameters
		t[0], ..., t[tNum-1] as temporary registers
		{rcx,rdx,rsi,rdi,rbp} are explicitly available by specifying Use{RCX,RDX,RSI,RDI,RBP} in tNum
		rsp[0..stackSizeByte-1] if stackSizeByte > 0
	*/
	StackFrame(Xbyak::CodeGenerator *code, int pNum, int tNum = 0, int stackSizeByte = 0, bool makeEpilog = true)
		: code_(code)
		, pNum_(pNum)
		, tNum_(tNum & ~(UseMASK|UseRBPAsFramePointer))
		, useRegs_(tNum & UseMASK) // drop UseRBPAsFramePointer bit
		, saveNum_(0)
		, P_(0)
		, makeEpilog_(makeEpilog)
		, p(p_)
		, t(t_)
	{
		if (pNum < 0 || pNum > 4) XBYAK_THROW(ERR_BAD_PNUM)
		if (tNum < 0) XBYAK_THROW(ERR_BAD_TNUM)
		const int *const fullTbl = getRegEntryTbl();
		const int *const calleeTbl = fullTbl + noSaveNum;
		int callerUseNum = 0;
		int calleeUseNum = 0;
		for (int i = 0; i < maxRegNum; i++) {
			if (useRegs_ & useFlagOf(fullTbl[i])) {
				if (i < noSaveNum) {
					callerUseNum++;
				} else {
					calleeUseNum++;
				}
			}
		}
		const int useNum = callerUseNum + calleeUseNum;
		if (pNum + tNum_ + useNum > maxRegNum) XBYAK_THROW(ERR_BAD_TNUM)
		const int baseSaveNum = local::max_(0, pNum + tNum_ + useNum - noSaveNum);
		bool pushedRbp = false;
		if (useRegs_ & UseRBP) {
			code->push(rbp);
			saveRegs_[saveNum_++] = Operand::RBP;
			pushedRbp = true;
			if ((tNum & UseRBPAsFramePointer) == UseRBPAsFramePointer) code->mov(rbp, rsp);
		}
		for (int i = 0; i < calleeSaveNum; i++) {
			int r = calleeTbl[i];
			if (i < baseSaveNum || isUseReg(r)) {
				if (pushedRbp && r == Operand::RBP) continue;
				saveRegs_[saveNum_++] = r;
				code->push(Reg64(r));
			}
		}
		P_ = (stackSizeByte + 7) / 8;
		// (rsp % 16) == 8, then increment P_ for 16 byte alignment
		if (P_ > 0 && (P_ & 1) == (saveNum_ & 1)) P_++;
		P_ *= 8;
		if (P_ > 0) code->sub(rsp, P_);
		int pos = 0;
		for (int i = 0; i < pNum; i++) {
			pTbl_[i] = Xbyak::Reg64(getRegIdx(pos));
		}
		for (int i = 0; i < tNum_; i++) {
			tTbl_[i] = Xbyak::Reg64(getRegIdx(pos));
		}
		// replace reserved reg with backup reg if needed
		for (size_t i = 0; i < maxPnum; i++) {
			const RegSlot& rp = getRegSlotTbl()[i];
			if (isUseReg(rp.target) && rp.pos < pNum && rp.alt >= 0) {
				code->mov(Xbyak::Reg64(rp.alt), Xbyak::Reg64(rp.target));
			}
		}
		p_.init(pTbl_, pNum);
		t_.init(tTbl_, tNum_);
	}
	/*
		make epilog manually
		@param callRet [in] call ret() if true
	*/
	void close(bool callRet = true)
	{
		if (P_ > 0) code_->add(code_->rsp, P_);
		for (int i = saveNum_ - 1; i >= 0; i--) {
			code_->pop(Reg64(saveRegs_[i]));
		}
		if (callRet) code_->ret();
	}
	~StackFrame()
	{
		if (!makeEpilog_) return;
		close();
	}
private:
	static int useFlagOf(int r)
	{
		switch (r) {
		case Operand::RCX: return UseRCX;
		case Operand::RDX: return UseRDX;
		case Operand::RSI: return UseRSI;
		case Operand::RDI: return UseRDI;
		case Operand::RBP: return UseRBP;
		default: return 0;
		}
	}
	bool isUseReg(int r) const { return (useRegs_ & useFlagOf(r)) != 0; }
	// Register allocation for the first 4 function parameters
	struct RegSlot {
		int target;
		int pos; // position of target in getRegEntryTbl()
		int alt; // alternative if target is used for parameter. -1 means no alternative.
	};
	const RegSlot *getRegSlotTbl() const
	{
		// Win: p[] = rcx(r10), rdx(r11), r8, r9:
		// Linux: p[] = rdi(r8), rsi(r9), rdx(r11), rcx(r10)
		// reg(alt) means a reserved reg if Use<reg> is used.

		static const RegSlot tbl[maxPnum] = {
#ifdef XBYAK64_WIN
			{ Operand::RCX, 0, Operand::R10 },
			{ Operand::RDX, 1, Operand::R11 },
			{ Operand::RDI, 6, -1 },
			{ Operand::RSI, 7, -1 },
#else
			{ Operand::RCX, 3, Operand::R10 },
			{ Operand::RDX, 2, Operand::R11 },
			{ Operand::RDI, 0, Operand::R8 },
			{ Operand::RSI, 1, Operand::R9 },
#endif
		};
		return tbl;
	}
	const int *getRegEntryTbl() const
	{
		static const int tbl[maxRegNum] = {
#ifdef XBYAK64_WIN
			Operand::RCX, Operand::RDX, Operand::R8, Operand::R9, Operand::R10, Operand::R11, Operand::RDI, Operand::RSI,
#else
			Operand::RDI, Operand::RSI, Operand::RDX, Operand::RCX, Operand::R8, Operand::R9, Operand::R10, Operand::R11,
#endif
			Operand::RBX, Operand::RBP, Operand::R12, Operand::R13, Operand::R14, Operand::R15
		};
		return &tbl[0];
	}
	// get an available register index from tbl, skipping reserved registers
	int getRegIdx(int& pos) const
	{
		const int *tbl = getRegEntryTbl();
		const RegSlot *slotTbl = getRegSlotTbl();
		for (;;) {
		NEXT:;
			assert(pos < maxRegNum);
			int r = tbl[pos++];
			// if r is a Use*** target with alt, return alt as backup
			// otherwise skip Use*** targets, their alts, and UseRBP's rbp
			for (size_t i = 0; i < maxPnum; i++) {
				const RegSlot& slot = slotTbl[i];
				if (!isUseReg(slot.target)) continue;
				if (r == slot.alt) goto NEXT;
				if (r == slot.target) {
					if (slot.alt >= 0) return slot.alt;
					goto NEXT;
				}
			}
			if (!isUseReg(r)) return r;
		}
	}
};
#endif

class Profiler {
	int mode_;
	const char *suffix_;
	const void *startAddr_;
#ifdef XBYAK_USE_PERF
	FILE *fp_;
#endif
public:
	enum {
		None = 0,
		Perf = 1,
		VTune = 2
	};
	Profiler()
		: mode_(None)
		, suffix_("")
		, startAddr_(0)
#ifdef XBYAK_USE_PERF
		, fp_(0)
#endif
	{
	}
	// append suffix to funcName
	void setNameSuffix(const char *suffix)
	{
		suffix_ = suffix;
	}
	void setStartAddr(const void *startAddr)
	{
		startAddr_ = startAddr;
	}
	void init(int mode)
	{
		mode_ = None;
		switch (mode) {
		default:
		case None:
			return;
		case Perf:
#ifdef XBYAK_USE_PERF
			close();
			{
				const int pid = getpid();
				char name[128];
				snprintf(name, sizeof(name), "/tmp/perf-%d.map", pid);
				fp_ = fopen(name, "a+");
				if (fp_ == 0) {
					fprintf(stderr, "can't open %s\n", name);
					return;
				}
			}
			mode_ = Perf;
#endif
			return;
		case VTune:
#ifdef XBYAK_USE_VTUNE
			dlopen("dummy", RTLD_LAZY); // force to load dlopen to enable jit profiling
			if (iJIT_IsProfilingActive() != iJIT_SAMPLING_ON) {
				fprintf(stderr, "VTune profiling is not active\n");
				return;
			}
			mode_ = VTune;
#endif
			return;
		}
	}
	~Profiler()
	{
		close();
	}
	void close()
	{
#ifdef XBYAK_USE_PERF
		if (fp_ == 0) return;
		fclose(fp_);
		fp_ = 0;
#endif
	}
	void set(const char *funcName, const void *startAddr, size_t funcSize) const
	{
		if (mode_ == None) return;
#if !defined(XBYAK_USE_PERF) && !defined(XBYAK_USE_VTUNE)
		(void)funcName;
		(void)startAddr;
		(void)funcSize;
#endif
#ifdef XBYAK_USE_PERF
		if (mode_ == Perf) {
			if (fp_ == 0) return;
			fprintf(fp_, "%llx %zx %s%s", (long long)startAddr, funcSize, funcName, suffix_);
			/*
				perf does not recognize the function name which is less than 3,
				so append '_' at the end of the name if necessary
			*/
			size_t n = strlen(funcName) + strlen(suffix_);
			for (size_t i = n; i < 3; i++) {
				fprintf(fp_, "_");
			}
			fprintf(fp_, "\n");
			fflush(fp_);
		}
#endif
#ifdef XBYAK_USE_VTUNE
		if (mode_ != VTune) return;
		char className[] = "";
		char fileName[] = "";
		iJIT_Method_Load jmethod = {};
		jmethod.method_id = iJIT_GetNewMethodID();
		jmethod.class_file_name = className;
		jmethod.source_file_name = fileName;
		jmethod.method_load_address = const_cast<void*>(startAddr);
		jmethod.method_size = funcSize;
		jmethod.line_number_size = 0;
		char buf[128];
		snprintf(buf, sizeof(buf), "%s%s", funcName, suffix_);
		jmethod.method_name = buf;
		iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&jmethod);
#endif
	}
	/*
		for continuous set
		funcSize = endAddr - <previous set endAddr>
	*/
	void set(const char *funcName, const void *endAddr)
	{
		set(funcName, startAddr_, (size_t)endAddr - (size_t)startAddr_);
		startAddr_ = endAddr;
	}
};
#endif // XBYAK_ONLY_CLASS_CPU

} } // end of util

#if XBYAK_CPUMASK_COMPACT == 1 && __cplusplus >= 201103

namespace std {

template<>
struct hash<Xbyak::util::CpuMask> {
	size_t operator()(const Xbyak::util::CpuMask& m) const noexcept {
		return std::hash<uint64_t>{}(m.to_u64());
	}
};

} // std

#endif

#endif
