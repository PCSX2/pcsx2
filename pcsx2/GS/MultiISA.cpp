// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "MultiISA.h"

#include "common/Console.h"

#include "cpuinfo.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

#ifdef _M_X86

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

	if (cpuinfo_has_x86_avx2() && cpuinfo_has_x86_bmi() && cpuinfo_has_x86_bmi2())
		return ProcessorFeatures::VectorISA::AVX2;
	else if (cpuinfo_has_x86_avx())
		return ProcessorFeatures::VectorISA::AVX;
	else
		return ProcessorFeatures::VectorISA::SSE4;
}

#endif

static ProcessorFeatures getProcessorFeatures()
{
	cpuinfo_initialize();

	ProcessorFeatures features = {};
#if defined(_M_X86)
	features.vectorISA = getCurrentISA();
	features.hasFMA = cpuinfo_has_x86_fma3();
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
		if (cpuinfo_get_cores_count() > 0 && cpuinfo_get_core(0)->vendor == cpuinfo_vendor_intel)
		{
			// Slow on Haswell
			features.hasSlowGather = (cpuinfo_get_uarchs_count() == 0 || cpuinfo_get_uarch(0)->uarch == cpuinfo_uarch_haswell);
		}
		else
		{
			// Currently no Zen CPUs with fast VPGATHERDD
			// Check https://uops.info/table.html as new CPUs come out for one that doesn't split it into like 40 µops
			// Doing it manually is about 28 µops (8x xmm -> gpr, 6x extr, 8x load, 6x insr)
			features.hasSlowGather = true;
		}
	}
#endif
	return features;
}

const ProcessorFeatures g_cpu = getProcessorFeatures();

// Keep init order by defining these here

#include "GSXXH.h"

u64 (&MultiISAFunctions::GSXXH3_64_Long)(const void* data, size_t len) = MULTI_ISA_SELECT(GSXXH3_64_Long);
u32 (&MultiISAFunctions::GSXXH3_64_Update)(void* state, const void* data, size_t len) = MULTI_ISA_SELECT(GSXXH3_64_Update);
u64 (&MultiISAFunctions::GSXXH3_64_Digest)(void* state) = MULTI_ISA_SELECT(GSXXH3_64_Digest);
