// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "MultiISA.h"
#include <xbyak/xbyak_util.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

static Xbyak::util::Cpu s_cpu;

static ProcessorFeatures::VectorISA getCurrentISA()
{
	// For debugging
	if (const char* over = getenv("OVERRIDE_VECTOR_ISA"))
	{
		if (strcasecmp(over, "avx2") == 0)
		{
			fprintf(stderr, "Vector ISA Override: AVX2\n");
			return ProcessorFeatures::VectorISA::AVX2;
		}
		if (strcasecmp(over, "avx") == 0)
		{
			fprintf(stderr, "Vector ISA Override: AVX\n");
			return ProcessorFeatures::VectorISA::AVX;
		}
		if (strcasecmp(over, "sse4") == 0)
		{
			fprintf(stderr, "Vector ISA Override: SSE4\n");
			return ProcessorFeatures::VectorISA::SSE4;
		}
	}
	if (s_cpu.has(Xbyak::util::Cpu::tAVX2) && s_cpu.has(Xbyak::util::Cpu::tBMI1) && s_cpu.has(Xbyak::util::Cpu::tBMI2))
		return ProcessorFeatures::VectorISA::AVX2;
	else if (s_cpu.has(Xbyak::util::Cpu::tAVX))
		return ProcessorFeatures::VectorISA::AVX;
	else if (s_cpu.has(Xbyak::util::Cpu::tSSE41))
		return ProcessorFeatures::VectorISA::SSE4;
	else
		return ProcessorFeatures::VectorISA::None;
}

static ProcessorFeatures getProcessorFeatures()
{
	ProcessorFeatures features = {};
	features.vectorISA = getCurrentISA();
	features.hasFMA = s_cpu.has(Xbyak::util::Cpu::tFMA);
	if (const char* over = getenv("OVERRIDE_FMA"))
	{
		features.hasFMA = over[0] == 'Y' || over[0] == 'y' || over[0] == '1';
		fprintf(stderr, "Processor FMA override: %s\n", features.hasFMA ? "Supported" : "Unsupported");
	}
	features.hasSlowGather = false;
	if (const char* over = getenv("OVERRIDE_SLOW_GATHER")) // Easy override for comparing on vs off
	{
		features.hasSlowGather = over[0] == 'Y' || over[0] == 'y' || over[0] == '1';
		fprintf(stderr, "Processor gather override: %s\n", features.hasSlowGather ? "Slow" : "Fast");
	}
	else if (features.vectorISA == ProcessorFeatures::VectorISA::AVX2)
	{
		if (s_cpu.has(Xbyak::util::Cpu::tINTEL))
		{
			// Slow on Haswell
			// CPUID data from https://en.wikichip.org/wiki/intel/cpuid
			features.hasSlowGather = s_cpu.displayModel == 0x46 || s_cpu.displayModel == 0x45 || s_cpu.displayModel == 0x3c;
		}
		else
		{
			// Currently no Zen CPUs with fast VPGATHERDD
			// Check https://uops.info/table.html as new CPUs come out for one that doesn't split it into like 40 µops
			// Doing it manually is about 28 µops (8x xmm -> gpr, 6x extr, 8x load, 6x insr)
			features.hasSlowGather = true;
		}
	}
	return features;
}

const ProcessorFeatures g_cpu = getProcessorFeatures();

// Keep init order by defining these here

#include "GSXXH.h"

u64 (&MultiISAFunctions::GSXXH3_64_Long)(const void* data, size_t len) = MULTI_ISA_SELECT(GSXXH3_64_Long);
u32 (&MultiISAFunctions::GSXXH3_64_Update)(void* state, const void* data, size_t len) = MULTI_ISA_SELECT(GSXXH3_64_Update);
u64 (&MultiISAFunctions::GSXXH3_64_Digest)(void* state) = MULTI_ISA_SELECT(GSXXH3_64_Digest);
