#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cpuinfo.h>
#include <x86/cpuid.h>

#if CPUINFO_ARCH_X86
#ifdef _MSC_VER
#pragma pack(push, 2)
#endif
struct fxsave_region {
	uint16_t fpu_control_word;
	uint16_t fpu_status_word;
	uint16_t fpu_tag_word;
	uint16_t fpu_opcode;
	uint32_t fpu_instruction_pointer_offset;
	uint32_t fpu_instruction_pointer_selector;
	uint32_t fpu_operand_pointer_offset;
	uint32_t fpu_operand_pointer_selector;
	uint32_t mxcsr_state;
	uint32_t mxcsr_mask;
	uint64_t fpu_registers[8 * 2];
	uint64_t xmm_registers[8 * 2];
	uint64_t padding[28];
}
#ifndef _MSC_VER
__attribute__((__aligned__(16), __packed__))
#endif
; /* end of fxsave_region structure */
#ifdef _MSC_VER
#pragma pack(pop, 2)
#endif
#endif

struct cpuinfo_x86_isa cpuinfo_x86_detect_isa(
	const struct cpuid_regs basic_info,
	const struct cpuid_regs extended_info,
	uint32_t max_base_index,
	uint32_t max_extended_index,
	enum cpuinfo_vendor vendor,
	enum cpuinfo_uarch uarch) {
	struct cpuinfo_x86_isa isa = {0};

	const struct cpuid_regs structured_feature_info0 =
		(max_base_index >= 7) ? cpuidex(7, 0) : (struct cpuid_regs){0, 0, 0, 0};
	const struct cpuid_regs structured_feature_info1 =
		(max_base_index >= 7) ? cpuidex(7, 1) : (struct cpuid_regs){0, 0, 0, 0};

	const uint32_t processor_capacity_info_index = UINT32_C(0x80000008);
	const struct cpuid_regs processor_capacity_info = (max_extended_index >= processor_capacity_info_index)
		? cpuid(processor_capacity_info_index)
		: (struct cpuid_regs){0, 0, 0, 0};

	bool avx_regs = false, avx512_regs = false, mpx_regs = false;
	/*
	 * OSXSAVE: Operating system enabled XSAVE instructions for application
	 * use:
	 * - Intel, AMD: ecx[bit 26] in basic info = XSAVE/XRSTOR instructions
	 * supported by a chip.
	 * - Intel, AMD: ecx[bit 27] in basic info = XSAVE/XRSTOR instructions
	 * enabled by OS.
	 */
	const uint32_t osxsave_mask = UINT32_C(0x0C000000);
	if ((basic_info.ecx & osxsave_mask) == osxsave_mask) {
		uint64_t xcr0_valid_bits = 0;
		if (max_base_index >= 0xD) {
			const struct cpuid_regs regs = cpuidex(0xD, 0);
			xcr0_valid_bits = ((uint64_t)regs.edx << 32) | regs.eax;
		}

		const uint64_t xfeature_enabled_mask = xgetbv(0);

		/*
		 * AVX registers:
		 * - Intel, AMD: XFEATURE_ENABLED_MASK[bit 1] for low 128 bits
		 * of ymm registers
		 * - Intel, AMD: XFEATURE_ENABLED_MASK[bit 2] for high 128 bits
		 * of ymm registers
		 */
		const uint64_t avx_regs_mask = UINT64_C(0x0000000000000006);
		if ((xcr0_valid_bits & avx_regs_mask) == avx_regs_mask) {
			avx_regs = (xfeature_enabled_mask & avx_regs_mask) == avx_regs_mask;
		}

		/*
		 * AVX512 registers:
		 * - Intel, AMD: XFEATURE_ENABLED_MASK[bit 1] for low 128 bits
		 * of zmm registers
		 * - Intel, AMD: XFEATURE_ENABLED_MASK[bit 2] for bits 128-255
		 * of zmm registers
		 * - Intel: XFEATURE_ENABLED_MASK[bit 5] for 8 64-bit OpMask
		 * registers (k0-k7)
		 * - Intel: XFEATURE_ENABLED_MASK[bit 6] for the high 256 bits
		 * of the zmm registers zmm0-zmm15
		 * - Intel: XFEATURE_ENABLED_MASK[bit 7] for the 512-bit zmm
		 * registers zmm16-zmm31
		 */
		const uint64_t avx512_regs_mask = UINT64_C(0x00000000000000E6);
		if ((xcr0_valid_bits & avx512_regs_mask) == avx512_regs_mask) {
			avx512_regs = (xfeature_enabled_mask & avx512_regs_mask) == avx512_regs_mask;
		}

		/*
		 * MPX registers:
		 * - Intel: XFEATURE_ENABLED_MASK[bit 3] for BNDREGS
		 * - Intel: XFEATURE_ENABLED_MASK[bit 4] for BNDCSR
		 */
		const uint64_t mpx_regs_mask = UINT64_C(0x0000000000000018);
		if ((xcr0_valid_bits & mpx_regs_mask) == mpx_regs_mask) {
			mpx_regs = (xfeature_enabled_mask & mpx_regs_mask) == mpx_regs_mask;
		}
	}

#if CPUINFO_ARCH_X86
	/*
	 * RDTSC instruction:
	 * - Intel, AMD: edx[bit 4] in basic info.
	 * - AMD: edx[bit 4] in extended info (reserved bit on Intel CPUs).
	 */
	isa.rdtsc = !!((basic_info.edx | extended_info.edx) & UINT32_C(0x00000010));
#endif

	/*
	 * SYSENTER/SYSEXIT instructions:
	 * - Intel, AMD: edx[bit 11] in basic info.
	 */
	isa.sysenter = !!(basic_info.edx & UINT32_C(0x00000800));

#if CPUINFO_ARCH_X86
	/*
	 * SYSCALL/SYSRET instructions:
	 * - Intel, AMD: edx[bit 11] in extended info.
	 */
	isa.syscall = !!(extended_info.edx & UINT32_C(0x00000800));
#endif

	/*
	 * RDMSR/WRMSR instructions:
	 * - Intel, AMD: edx[bit 5] in basic info.
	 * - AMD: edx[bit 5] in extended info (reserved bit on Intel CPUs).
	 */
	isa.msr = !!((basic_info.edx | extended_info.edx) & UINT32_C(0x00000020));

	/*
	 * CLZERO instruction:
	 * - AMD: ebx[bit 0] in processor capacity info (reserved bit on Intel
	 * CPUs).
	 */
	isa.clzero = !!(processor_capacity_info.ebx & UINT32_C(0x00000001));

	/*
	 * CLFLUSH instruction:
	 * - Intel, AMD: edx[bit 19] in basic info.
	 */
	isa.clflush = !!(basic_info.edx & UINT32_C(0x00080000));

	/*
	 * CLFLUSHOPT instruction:
	 * - Intel: ebx[bit 23] in structured feature info (ecx = 0).
	 */
	isa.clflushopt = !!(structured_feature_info0.ebx & UINT32_C(0x00800000));

	/*
	 * MWAIT/MONITOR instructions:
	 * - Intel, AMD: ecx[bit 3] in basic info.
	 */
	isa.mwait = !!(basic_info.ecx & UINT32_C(0x00000008));

	/*
	 * MWAITX/MONITORX instructions:
	 * - AMD: ecx[bit 29] in extended info.
	 */
	isa.mwaitx = !!(extended_info.ecx & UINT32_C(0x20000000));

	/*
	 * FXSAVE/FXRSTOR instructions:
	 * - Intel, AMD: edx[bit 24] in basic info.
	 * - AMD: edx[bit 24] in extended info (zero bit on Intel CPUs, EMMX bit
	 * on Cyrix CPUs).
	 */
	switch (vendor) {
#if CPUINFO_ARCH_X86
		case cpuinfo_vendor_cyrix:
		case cpuinfo_vendor_nsc:
			isa.emmx = !!(extended_info.edx & UINT32_C(0x01000000));
			break;
#endif
		default:
			isa.fxsave = !!((basic_info.edx | extended_info.edx) & UINT32_C(0x01000000));
			break;
	}

	/*
	 * XSAVE/XRSTOR instructions:
	 * - Intel, AMD: ecx[bit 26] in basic info.
	 */
	isa.xsave = !!(basic_info.ecx & UINT32_C(0x04000000));

#if CPUINFO_ARCH_X86
	/*
	 * x87 FPU instructions:
	 * - Intel, AMD: edx[bit 0] in basic info.
	 * - AMD: edx[bit 0] in extended info (reserved bit on Intel CPUs).
	 */
	isa.fpu = !!((basic_info.edx | extended_info.edx) & UINT32_C(0x00000001));

	/*
	 * MMX instructions:
	 * - Intel, AMD: edx[bit 23] in basic info.
	 * - AMD: edx[bit 23] in extended info (zero bit on Intel CPUs).
	 */
	isa.mmx = !!((basic_info.edx | extended_info.edx) & UINT32_C(0x00800000));

	/*
	 * MMX+/Integer SSE instructions:
	 * - Intel, AMD: edx[bit 25] in basic info (SSE feature flag).
	 * - Pre-SSE AMD: edx[bit 22] in extended info (zero bit on Intel CPUs).
	 */
	isa.mmx_plus = !!((basic_info.edx & UINT32_C(0x02000000)) | (extended_info.edx & UINT32_C(0x00400000)));
#endif

	/*
	 * 3dnow! instructions:
	 * - AMD: edx[bit 31] of extended info (zero bit on Intel CPUs).
	 */
	isa.three_d_now = !!(extended_info.edx & UINT32_C(0x80000000));

	/*
	 * 3dnow!+ instructions:
	 * - AMD: edx[bit 30] of extended info (zero bit on Intel CPUs).
	 */
	isa.three_d_now_plus = !!(extended_info.edx & UINT32_C(0x40000000));

#if CPUINFO_ARCH_X86
	/*
	 * 3dnow! Geode instructions:
	 * - No CPUID bit, detect as Geode microarchitecture + 3dnow!+ support
	 */
	isa.three_d_now_geode = isa.three_d_now_plus && (uarch == cpuinfo_uarch_geode);
#endif

	/*
	 * PREFETCH instruction:
	 * - AMD: ecx[bit 8] of extended info (one of 3dnow! prefetch
	 * instructions). On Intel this bit indicates PREFETCHW, but not
	 * PREFETCH support.
	 * - AMD: edx[bit 31] of extended info (implied by 3dnow! support).
	 * Reserved bit on Intel CPUs.
	 * - AMD: edx[bit 30] of extended info (implied by 3dnow!+ support).
	 * Reserved bit on Intel CPUs.
	 * - AMD: edx[bit 29] of extended info (x86-64 support). Does not imply
	 * PREFETCH support on non-AMD CPUs!!!
	 */
	switch (vendor) {
		case cpuinfo_vendor_intel:
			/*
			 * Instruction is not documented in the manual,
			 * and the 3dnow! prefetch CPUID bit indicates PREFETCHW
			 * instruction.
			 */
			break;
		case cpuinfo_vendor_amd:
		case cpuinfo_vendor_hygon:
			isa.prefetch =
				!!((extended_info.ecx & UINT32_C(0x00000100)) |
				   (extended_info.edx & UINT32_C(0xE0000000)));
			break;
		default:
			/*
			 * Conservatively assume, that 3dnow!/3dnow!+ support
			 * implies PREFETCH support, but 3dnow! prefetch CPUID
			 * bit follows Intel spec (PREFETCHW, but not PREFETCH).
			 */
			isa.prefetch = !!(extended_info.edx & UINT32_C(0xC0000000));
			break;
	}

	/*
	 * PREFETCHW instruction:
	 * - AMD: ecx[bit 8] of extended info (one of 3dnow! prefetch
	 * instructions).
	 * - Intel: ecx[bit 8] of extended info (PREFETCHW instruction only).
	 * - AMD: edx[bit 31] of extended info (implied by 3dnow! support).
	 * Reserved bit on Intel CPUs.
	 * - AMD: edx[bit 30] of extended info (implied by 3dnow!+ support).
	 * Reserved bit on Intel CPUs.
	 * - AMD: edx[bit 29] of extended info (x86-64 support). Does not imply
	 * PREFETCHW support on non-AMD CPUs!!!
	 */
	switch (vendor) {
		case cpuinfo_vendor_amd:
		case cpuinfo_vendor_hygon:
			isa.prefetchw =
				!!((extended_info.ecx & UINT32_C(0x00000100)) |
				   (extended_info.edx & UINT32_C(0xE0000000)));
			break;
		default:
			/* Assume, that 3dnow!/3dnow!+ support implies PREFETCHW
			 * support, not implications from x86-64 support */
			isa.prefetchw =
				!!((extended_info.ecx & UINT32_C(0x00000100)) |
				   (extended_info.edx & UINT32_C(0xC0000000)));
			break;
	}

	/*
	 * PREFETCHWT1 instruction:
	 * - Intel: ecx[bit 0] of structured feature info (ecx = 0). Reserved
	 * bit on AMD.
	 */
	isa.prefetchwt1 = !!(structured_feature_info0.ecx & UINT32_C(0x00000001));

#if CPUINFO_ARCH_X86
	/*
	 * SSE instructions:
	 * - Intel, AMD: edx[bit 25] in basic info.
	 */
	isa.sse = !!(basic_info.edx & UINT32_C(0x02000000));

	/*
	 * SSE2 instructions:
	 * - Intel, AMD: edx[bit 26] in basic info.
	 */
	isa.sse2 = !!(basic_info.edx & UINT32_C(0x04000000));
#endif

	/*
	 * SSE3 instructions:
	 * - Intel, AMD: ecx[bit 0] in basic info.
	 */
	isa.sse3 = !!(basic_info.ecx & UINT32_C(0x00000001));

#if CPUINFO_ARCH_X86
	/*
	 * CPUs with x86-64 or SSE3 always support DAZ (denormals-as-zero) mode.
	 * Only early Pentium 4 models may not support it.
	 */
	if (isa.sse3) {
		isa.daz = true;
	} else {
		/* Detect DAZ support from masked MXCSR bits */
		if (isa.sse && isa.fxsave) {
			struct fxsave_region region = {0};
#ifdef _MSC_VER
			_fxsave(&region);
#else
			__asm__ __volatile__("fxsave %[region];" : [region] "+m"(region));
#endif

			/*
			 * Denormals-as-zero (DAZ) flag:
			 * - Intel, AMD: MXCSR[bit 6]
			 */
			isa.daz = !!(region.mxcsr_mask & UINT32_C(0x00000040));
		}
	}
#endif

	/*
	 * SSSE3 instructions:
	 * - Intel, AMD: ecx[bit 9] in basic info.
	 */
	isa.ssse3 = !!(basic_info.ecx & UINT32_C(0x0000200));

	/*
	 * SSE4.1 instructions:
	 * - Intel, AMD: ecx[bit 19] in basic info.
	 */
	isa.sse4_1 = !!(basic_info.ecx & UINT32_C(0x00080000));

	/*
	 * SSE4.2 instructions:
	 * - Intel: ecx[bit 20] in basic info (reserved bit on AMD CPUs).
	 */
	isa.sse4_2 = !!(basic_info.ecx & UINT32_C(0x00100000));

	/*
	 * SSE4A instructions:
	 * - AMD: ecx[bit 6] in extended info (reserved bit on Intel CPUs).
	 */
	isa.sse4a = !!(extended_info.ecx & UINT32_C(0x00000040));

	/*
	 * Misaligned memory operands in SSE instructions:
	 * - AMD: ecx[bit 7] in extended info (reserved bit on Intel CPUs).
	 */
	isa.misaligned_sse = !!(extended_info.ecx & UINT32_C(0x00000080));

	/*
	 * AVX instructions:
	 * - Intel, AMD: ecx[bit 28] in basic info.
	 */
	isa.avx = avx_regs && !!(basic_info.ecx & UINT32_C(0x10000000));

	/*
	 * FMA3 instructions:
	 * - Intel: ecx[bit 12] in basic info (reserved bit on AMD CPUs).
	 */
	isa.fma3 = avx_regs && !!(basic_info.ecx & UINT32_C(0x00001000));

	/*
	 * FMA4 instructions:
	 * - AMD: ecx[bit 16] in extended info (reserved bit on Intel CPUs).
	 */
	isa.fma4 = avx_regs && !!(extended_info.ecx & UINT32_C(0x00010000));

	/*
	 * XOP instructions:
	 * - AMD: ecx[bit 11] in extended info (reserved bit on Intel CPUs).
	 */
	isa.xop = avx_regs && !!(extended_info.ecx & UINT32_C(0x00000800));

	/*
	 * F16C instructions:
	 * - Intel, AMD: ecx[bit 29] in basic info.
	 */
	isa.f16c = avx_regs && !!(basic_info.ecx & UINT32_C(0x20000000));

	/*
	 * AVX2 instructions:
	 * - Intel: ebx[bit 5] in structured feature info (ecx = 0).
	 */
	isa.avx2 = avx_regs && !!(structured_feature_info0.ebx & UINT32_C(0x00000020));

	/*
	 * AVX512F instructions:
	 * - Intel: ebx[bit 16] in structured feature info (ecx = 0).
	 */
	isa.avx512f = avx512_regs && !!(structured_feature_info0.ebx & UINT32_C(0x00010000));

	/*
	 * AVX512PF instructions:
	 * - Intel: ebx[bit 26] in structured feature info (ecx = 0).
	 */
	isa.avx512pf = avx512_regs && !!(structured_feature_info0.ebx & UINT32_C(0x04000000));

	/*
	 * AVX512ER instructions:
	 * - Intel: ebx[bit 27] in structured feature info (ecx = 0).
	 */
	isa.avx512er = avx512_regs && !!(structured_feature_info0.ebx & UINT32_C(0x08000000));

	/*
	 * AVX512CD instructions:
	 * - Intel: ebx[bit 28] in structured feature info (ecx = 0).
	 */
	isa.avx512cd = avx512_regs && !!(structured_feature_info0.ebx & UINT32_C(0x10000000));

	/*
	 * AVX512DQ instructions:
	 * - Intel: ebx[bit 17] in structured feature info (ecx = 0).
	 */
	isa.avx512dq = avx512_regs && !!(structured_feature_info0.ebx & UINT32_C(0x00020000));

	/*
	 * AVX512BW instructions:
	 * - Intel: ebx[bit 30] in structured feature info (ecx = 0).
	 */
	isa.avx512bw = avx512_regs && !!(structured_feature_info0.ebx & UINT32_C(0x40000000));

	/*
	 * AVX512VL instructions:
	 * - Intel: ebx[bit 31] in structured feature info (ecx = 0).
	 */
	isa.avx512vl = avx512_regs && !!(structured_feature_info0.ebx & UINT32_C(0x80000000));

	/*
	 * AVX512IFMA instructions:
	 * - Intel: ebx[bit 21] in structured feature info (ecx = 0).
	 */
	isa.avx512ifma = avx512_regs && !!(structured_feature_info0.ebx & UINT32_C(0x00200000));

	/*
	 * AVX512VBMI instructions:
	 * - Intel: ecx[bit 1] in structured feature info (ecx = 0).
	 */
	isa.avx512vbmi = avx512_regs && !!(structured_feature_info0.ecx & UINT32_C(0x00000002));

	/*
	 * AVX512VBMI2 instructions:
	 * - Intel: ecx[bit 6] in structured feature info (ecx = 0).
	 */
	isa.avx512vbmi2 = avx512_regs && !!(structured_feature_info0.ecx & UINT32_C(0x00000040));

	/*
	 * AVX512BITALG instructions:
	 * - Intel: ecx[bit 12] in structured feature info (ecx = 0).
	 */
	isa.avx512bitalg = avx512_regs && !!(structured_feature_info0.ecx & UINT32_C(0x00001000));

	/*
	 * AVX512VPOPCNTDQ instructions:
	 * - Intel: ecx[bit 14] in structured feature info (ecx = 0).
	 */
	isa.avx512vpopcntdq = avx512_regs && !!(structured_feature_info0.ecx & UINT32_C(0x00004000));

	/*
	 * AVX512VNNI instructions:
	 * - Intel: ecx[bit 11] in structured feature info (ecx = 0).
	 */
	isa.avx512vnni = avx512_regs && !!(structured_feature_info0.ecx & UINT32_C(0x00000800));

	/*
	 * AVX512_4VNNIW instructions:
	 * - Intel: edx[bit 2] in structured feature info (ecx = 0).
	 */
	isa.avx512_4vnniw = avx512_regs && !!(structured_feature_info0.edx & UINT32_C(0x00000004));

	/*
	 * AVX512_4FMAPS instructions:
	 * - Intel: edx[bit 3] in structured feature info (ecx = 0).
	 */
	isa.avx512_4fmaps = avx512_regs && !!(structured_feature_info0.edx & UINT32_C(0x00000008));

	/*
	 * AVX512_VP2INTERSECT instructions:
	 * - Intel: edx[bit 8] in structured feature info (ecx = 0).
	 */
	isa.avx512vp2intersect = avx512_regs && !!(structured_feature_info0.edx & UINT32_C(0x00000100));

	/*
	 * AVX512_FP16 instructions:
	 * - Intel: edx[bit 23] in structured feature info (ecx = 0).
	 */
	isa.avx512fp16 = avx512_regs && !!(structured_feature_info0.edx & UINT32_C(0x00800000));

	/*
	 * AVX_VNNI instructions:
	 * - Intel: eax[bit 4] in structured feature info (ecx = 1).
	 */
	isa.avxvnni = avx_regs && !!(structured_feature_info1.eax & UINT32_C(0x00000010));

	/*
	 * AVX512_BF16 instructions:
	 * - Intel: eax[bit 5] in structured feature info (ecx = 1).
	 */
	isa.avx512bf16 = avx512_regs && !!(structured_feature_info1.eax & UINT32_C(0x00000020));

	/*
	 * AMX_BF16 instructions:
	 * - Intel: edx[bit 22] in structured feature info (ecx = 0).
	 */
	isa.amx_bf16 = avx512_regs && !!(structured_feature_info0.edx & UINT32_C(0x00400000));

	/*
	 * AMX_TILE instructions:
	 * - Intel: edx[bit 24] in structured feature info (ecx = 0).
	 */
	isa.amx_tile = avx512_regs && !!(structured_feature_info0.edx & UINT32_C(0x01000000));

	/*
	 * AMX_INT8 instructions:
	 * - Intel: edx[bit 25] in structured feature info (ecx = 0).
	 */
	isa.amx_int8 = avx512_regs && !!(structured_feature_info0.edx & UINT32_C(0x02000000));

	/*
	 * AMX_FP16 instructions:
	 * - Intel: eax[bit 21] in structured feature info (ecx = 1).
	 */
	isa.amx_fp16 = avx512_regs && !!(structured_feature_info1.eax & UINT32_C(0x00200000));

	/*
	 * AVX_VNNI_INT8 instructions:
	 * - Intel: edx[bit 4] in structured feature info (ecx = 1).
	 */
	isa.avx_vnni_int8 = avx_regs && !!(structured_feature_info1.edx & UINT32_C(0x00000010));

	/*
	 * AVX_VNNI_INT16 instructions:
	 * - Intel: edx[bit 10] in structured feature info (ecx = 1).
	 */
	isa.avx_vnni_int16 = avx_regs && !!(structured_feature_info1.edx & UINT32_C(0x00000400));

	/*
	 * AVX_NE_CONVERT instructions:
	 * - Intel: edx[bit 5] in structured feature info (ecx = 1).
	 */
	isa.avx_ne_convert = avx_regs && !!(structured_feature_info1.edx & UINT32_C(0x00000020));

	/*
	 * HLE instructions:
	 * - Intel: ebx[bit 4] in structured feature info (ecx = 0).
	 */
	isa.hle = !!(structured_feature_info0.ebx & UINT32_C(0x00000010));

	/*
	 * RTM instructions:
	 * - Intel: ebx[bit 11] in structured feature info (ecx = 0).
	 */
	isa.rtm = !!(structured_feature_info0.ebx & UINT32_C(0x00000800));

	/*
	 * XTEST instruction:
	 * - Intel: either HLE or RTM is supported
	 */
	isa.xtest = isa.hle || isa.rtm;

	/*
	 * MPX registers and instructions:
	 * - Intel: ebx[bit 14] in structured feature info (ecx = 0).
	 */
	isa.mpx = mpx_regs && !!(structured_feature_info0.ebx & UINT32_C(0x00004000));

#if CPUINFO_ARCH_X86
	/*
	 * CMOV instructions:
	 * - Intel, AMD: edx[bit 15] in basic info.
	 * - AMD: edx[bit 15] in extended info (zero bit on Intel CPUs).
	 */
	isa.cmov = !!((basic_info.edx | extended_info.edx) & UINT32_C(0x00008000));

	/*
	 * CMPXCHG8B instruction:
	 * - Intel, AMD: edx[bit 8] in basic info.
	 * - AMD: edx[bit 8] in extended info (reserved bit on Intel CPUs).
	 */
	isa.cmpxchg8b = !!((basic_info.edx | extended_info.edx) & UINT32_C(0x00000100));
#endif

	/*
	 * CMPXCHG16B instruction:
	 * - Intel, AMD: ecx[bit 13] in basic info.
	 */
	isa.cmpxchg16b = !!(basic_info.ecx & UINT32_C(0x00002000));

	/*
	 * CLWB instruction:
	 * - Intel: ebx[bit 24] in structured feature info (ecx = 0).
	 */
	isa.clwb = !!(structured_feature_info0.ebx & UINT32_C(0x01000000));

	/*
	 * MOVBE instruction:
	 * - Intel: ecx[bit 22] in basic info.
	 */
	isa.movbe = !!(basic_info.ecx & UINT32_C(0x00400000));

#if CPUINFO_ARCH_X86_64
	/*
	 * Some early x86-64 CPUs lack LAHF & SAHF instructions.
	 * A special CPU feature bit must be checked to ensure their
	 * availability:
	 * - Intel, AMD: ecx[bit 0] in extended info.
	 */
	isa.lahf_sahf = !!(extended_info.ecx & UINT32_C(0x00000001));
#endif

	/*
	 * RDFSBASE/RDGSBASE/WRFSBASE/WRGSBASE instructions.
	 * - Intel: ebx[bit 0] in structured feature info (ecx = 0).
	 */
	isa.fs_gs_base = !!(structured_feature_info0.ebx & UINT32_C(0x00000001));

	/*
	 * LZCNT instruction:
	 * - Intel, AMD: ecx[bit 5] in extended info.
	 */
	isa.lzcnt = !!(extended_info.ecx & UINT32_C(0x00000020));

	/*
	 * POPCNT instruction:
	 * - Intel, AMD: ecx[bit 23] in basic info.
	 */
	isa.popcnt = !!(basic_info.ecx & UINT32_C(0x00800000));

	/*
	 * TBM instructions:
	 * - AMD: ecx[bit 21] in extended info (reserved bit on Intel CPUs).
	 */
	isa.tbm = !!(extended_info.ecx & UINT32_C(0x00200000));

	/*
	 * BMI instructions:
	 * - Intel, AMD: ebx[bit 3] in structured feature info (ecx = 0).
	 */
	isa.bmi = !!(structured_feature_info0.ebx & UINT32_C(0x00000008));

	/*
	 * BMI2 instructions:
	 * - Intel: ebx[bit 8] in structured feature info (ecx = 0).
	 */
	isa.bmi2 = !!(structured_feature_info0.ebx & UINT32_C(0x00000100));

	/*
	 * ADCX/ADOX instructions:
	 * - Intel: ebx[bit 19] in structured feature info (ecx = 0).
	 */
	isa.adx = !!(structured_feature_info0.ebx & UINT32_C(0x00080000));

	/*
	 * AES instructions:
	 * - Intel: ecx[bit 25] in basic info (reserved bit on AMD CPUs).
	 */
	isa.aes = !!(basic_info.ecx & UINT32_C(0x02000000));

	/*
	 * VAES instructions:
	 * - Intel: ecx[bit 9] in structured feature info (ecx = 0).
	 */
	isa.vaes = !!(structured_feature_info0.ecx & UINT32_C(0x00000200));

	/*
	 * PCLMULQDQ instruction:
	 * - Intel: ecx[bit 1] in basic info (reserved bit on AMD CPUs).
	 */
	isa.pclmulqdq = !!(basic_info.ecx & UINT32_C(0x00000002));

	/*
	 * VPCLMULQDQ instruction:
	 * - Intel: ecx[bit 10] in structured feature info (ecx = 0).
	 */
	isa.vpclmulqdq = !!(structured_feature_info0.ecx & UINT32_C(0x00000400));

	/*
	 * GFNI instructions:
	 * - Intel: ecx[bit 8] in structured feature info (ecx = 0).
	 */
	isa.gfni = !!(structured_feature_info0.ecx & UINT32_C(0x00000100));

	/*
	 * RDRAND instruction:
	 * - Intel: ecx[bit 30] in basic info (reserved bit on AMD CPUs).
	 */
	isa.rdrand = !!(basic_info.ecx & UINT32_C(0x40000000));

	/*
	 * RDSEED instruction:
	 * - Intel: ebx[bit 18] in structured feature info (ecx = 0).
	 */
	isa.rdseed = !!(structured_feature_info0.ebx & UINT32_C(0x00040000));

	/*
	 * SHA instructions:
	 * - Intel: ebx[bit 29] in structured feature info (ecx = 0).
	 */
	isa.sha = !!(structured_feature_info0.ebx & UINT32_C(0x20000000));

	if (vendor == cpuinfo_vendor_via) {
		const struct cpuid_regs padlock_meta_info = cpuid(UINT32_C(0xC0000000));
		const uint32_t max_padlock_index = padlock_meta_info.eax;
		const uint32_t padlock_info_index = UINT32_C(0xC0000001);
		if (max_padlock_index >= padlock_info_index) {
			const struct cpuid_regs padlock_info = cpuid(padlock_info_index);

			/*
			 * Padlock RNG extension:
			 * - VIA: edx[bit 2] in padlock info = RNG exists on
			 * chip flag.
			 * - VIA: edx[bit 3] in padlock info = RNG enabled by
			 * OS.
			 */
			const uint32_t padlock_rng_mask = UINT32_C(0x0000000C);
			isa.rng = (padlock_info.edx & padlock_rng_mask) == padlock_rng_mask;

			/*
			 * Padlock ACE extension:
			 * - VIA: edx[bit 6] in padlock info = ACE exists on
			 * chip flag.
			 * - VIA: edx[bit 7] in padlock info = ACE enabled by
			 * OS.
			 */
			const uint32_t padlock_ace_mask = UINT32_C(0x000000C0);
			isa.ace = (padlock_info.edx & padlock_ace_mask) == padlock_ace_mask;

			/*
			 * Padlock ACE 2 extension:
			 * - VIA: edx[bit 8] in padlock info = ACE2 exists on
			 * chip flag.
			 * - VIA: edx[bit 9] in padlock info = ACE 2 enabled by
			 * OS.
			 */
			const uint32_t padlock_ace2_mask = UINT32_C(0x00000300);
			isa.ace2 = (padlock_info.edx & padlock_ace2_mask) == padlock_ace2_mask;

			/*
			 * Padlock PHE extension:
			 * - VIA: edx[bit 10] in padlock info = PHE exists on
			 * chip flag.
			 * - VIA: edx[bit 11] in padlock info = PHE enabled by
			 * OS.
			 */
			const uint32_t padlock_phe_mask = UINT32_C(0x00000C00);
			isa.phe = (padlock_info.edx & padlock_phe_mask) == padlock_phe_mask;

			/*
			 * Padlock PMM extension:
			 * - VIA: edx[bit 12] in padlock info = PMM exists on
			 * chip flag.
			 * - VIA: edx[bit 13] in padlock info = PMM enabled by
			 * OS.
			 */
			const uint32_t padlock_pmm_mask = UINT32_C(0x00003000);
			isa.pmm = (padlock_info.edx & padlock_pmm_mask) == padlock_pmm_mask;
		}
	}

	/*
	 * LWP instructions:
	 * - AMD: ecx[bit 15] in extended info (reserved bit on Intel CPUs).
	 */
	isa.lwp = !!(extended_info.ecx & UINT32_C(0x00008000));

	/*
	 * RDTSCP instruction:
	 * - Intel, AMD: edx[bit 27] in extended info.
	 */
	isa.rdtscp = !!(extended_info.edx & UINT32_C(0x08000000));

	/*
	 * RDPID instruction:
	 * - Intel: ecx[bit 22] in structured feature info (ecx = 0).
	 */
	isa.rdpid = !!(structured_feature_info0.ecx & UINT32_C(0x00400000));

	return isa;
}
