#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <cpuinfo.h>
#include <cpuinfo/common.h>

struct cpuid_regs {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
};

struct cpuinfo_x86_cache {
	uint32_t size;
	uint32_t associativity;
	uint32_t sets;
	uint32_t partitions;
	uint32_t line_size;
	uint32_t flags;
	uint32_t apic_bits;
};

struct cpuinfo_x86_caches {
	struct cpuinfo_trace_cache trace;
	struct cpuinfo_x86_cache l1i;
	struct cpuinfo_x86_cache l1d;
	struct cpuinfo_x86_cache l2;
	struct cpuinfo_x86_cache l3;
	struct cpuinfo_x86_cache l4;
	uint32_t prefetch_size;
};

struct cpuinfo_x86_model_info {
	uint32_t model;
	uint32_t family;

	uint32_t base_model;
	uint32_t base_family;
	uint32_t stepping;
	uint32_t extended_model;
	uint32_t extended_family;
	uint32_t processor_type;
};

struct cpuinfo_x86_topology {
	uint32_t apic_id;
	uint32_t thread_bits_offset;
	uint32_t thread_bits_length;
	uint32_t core_bits_offset;
	uint32_t core_bits_length;
};

struct cpuinfo_x86_processor {
	uint32_t cpuid;
	enum cpuinfo_vendor vendor;
	enum cpuinfo_uarch uarch;
#ifdef __linux__
	int linux_id;
#endif
	struct cpuinfo_x86_caches cache;
	struct {
		struct cpuinfo_tlb itlb_4KB;
		struct cpuinfo_tlb itlb_2MB;
		struct cpuinfo_tlb itlb_4MB;
		struct cpuinfo_tlb dtlb0_4KB;
		struct cpuinfo_tlb dtlb0_2MB;
		struct cpuinfo_tlb dtlb0_4MB;
		struct cpuinfo_tlb dtlb_4KB;
		struct cpuinfo_tlb dtlb_2MB;
		struct cpuinfo_tlb dtlb_4MB;
		struct cpuinfo_tlb dtlb_1GB;
		struct cpuinfo_tlb stlb2_4KB;
		struct cpuinfo_tlb stlb2_2MB;
		struct cpuinfo_tlb stlb2_1GB;
	} tlb;
	struct cpuinfo_x86_topology topology;
	char brand_string[CPUINFO_PACKAGE_NAME_MAX];
};

CPUINFO_INTERNAL void cpuinfo_x86_init_processor(struct cpuinfo_x86_processor* processor);

CPUINFO_INTERNAL enum cpuinfo_vendor cpuinfo_x86_decode_vendor(uint32_t ebx, uint32_t ecx, uint32_t edx);
CPUINFO_INTERNAL struct cpuinfo_x86_model_info cpuinfo_x86_decode_model_info(uint32_t eax);
CPUINFO_INTERNAL enum cpuinfo_uarch cpuinfo_x86_decode_uarch(
	enum cpuinfo_vendor vendor,
	const struct cpuinfo_x86_model_info* model_info);

CPUINFO_INTERNAL struct cpuinfo_x86_isa cpuinfo_x86_detect_isa(
	const struct cpuid_regs basic_info,
	const struct cpuid_regs extended_info,
	uint32_t max_base_index,
	uint32_t max_extended_index,
	enum cpuinfo_vendor vendor,
	enum cpuinfo_uarch uarch);

CPUINFO_INTERNAL void cpuinfo_x86_detect_topology(
	uint32_t max_base_index,
	uint32_t max_extended_index,
	struct cpuid_regs leaf1,
	struct cpuinfo_x86_topology* topology);

CPUINFO_INTERNAL void cpuinfo_x86_detect_cache(
	uint32_t max_base_index,
	uint32_t max_extended_index,
	bool amd_topology_extensions,
	enum cpuinfo_vendor vendor,
	const struct cpuinfo_x86_model_info* model_info,
	struct cpuinfo_x86_caches* cache,
	struct cpuinfo_tlb* itlb_4KB,
	struct cpuinfo_tlb* itlb_2MB,
	struct cpuinfo_tlb* itlb_4MB,
	struct cpuinfo_tlb* dtlb0_4KB,
	struct cpuinfo_tlb* dtlb0_2MB,
	struct cpuinfo_tlb* dtlb0_4MB,
	struct cpuinfo_tlb* dtlb_4KB,
	struct cpuinfo_tlb* dtlb_2MB,
	struct cpuinfo_tlb* dtlb_4MB,
	struct cpuinfo_tlb* dtlb_1GB,
	struct cpuinfo_tlb* stlb2_4KB,
	struct cpuinfo_tlb* stlb2_2MB,
	struct cpuinfo_tlb* stlb2_1GB,
	uint32_t* log2_package_cores_max);

CPUINFO_INTERNAL void cpuinfo_x86_decode_cache_descriptor(
	uint8_t descriptor,
	enum cpuinfo_vendor vendor,
	const struct cpuinfo_x86_model_info* model_info,
	struct cpuinfo_x86_caches* cache,
	struct cpuinfo_tlb* itlb_4KB,
	struct cpuinfo_tlb* itlb_2MB,
	struct cpuinfo_tlb* itlb_4MB,
	struct cpuinfo_tlb* dtlb0_4KB,
	struct cpuinfo_tlb* dtlb0_2MB,
	struct cpuinfo_tlb* dtlb0_4MB,
	struct cpuinfo_tlb* dtlb_4KB,
	struct cpuinfo_tlb* dtlb_2MB,
	struct cpuinfo_tlb* dtlb_4MB,
	struct cpuinfo_tlb* dtlb_1GB,
	struct cpuinfo_tlb* stlb2_4KB,
	struct cpuinfo_tlb* stlb2_2MB,
	struct cpuinfo_tlb* stlb2_1GB,
	uint32_t* prefetch_size);

CPUINFO_INTERNAL bool cpuinfo_x86_decode_deterministic_cache_parameters(
	struct cpuid_regs regs,
	struct cpuinfo_x86_caches* cache,
	uint32_t* package_cores_max);

CPUINFO_INTERNAL bool cpuinfo_x86_decode_cache_properties(struct cpuid_regs regs, struct cpuinfo_x86_caches* cache);

CPUINFO_INTERNAL uint32_t cpuinfo_x86_normalize_brand_string(const char raw_name[48], char normalized_name[48]);

CPUINFO_INTERNAL uint32_t cpuinfo_x86_format_package_name(
	enum cpuinfo_vendor vendor,
	const char normalized_brand_string[48],
	char package_name[CPUINFO_PACKAGE_NAME_MAX]);
