#include <stdint.h>

#include <cpuinfo.h>
#include <cpuinfo/log.h>
#include <cpuinfo/utils.h>
#include <x86/api.h>
#include <x86/cpuid.h>

union cpuinfo_x86_cache_descriptors {
	struct cpuid_regs regs;
	uint8_t as_bytes[16];
};

enum cache_type {
	cache_type_none = 0,
	cache_type_data = 1,
	cache_type_instruction = 2,
	cache_type_unified = 3,
};

void cpuinfo_x86_detect_cache(
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
	uint32_t* log2_package_cores_max) {
	if (max_base_index >= 2) {
		union cpuinfo_x86_cache_descriptors descriptors;
		descriptors.regs = cpuid(2);
		uint32_t iterations = (uint8_t)descriptors.as_bytes[0];
		if (iterations != 0) {
		iterate_descriptors:
			for (uint32_t i = 1 /* note: not 0 */; i < 16; i++) {
				const uint8_t descriptor = descriptors.as_bytes[i];
				if (descriptor != 0) {
					cpuinfo_x86_decode_cache_descriptor(
						descriptor,
						vendor,
						model_info,
						cache,
						itlb_4KB,
						itlb_2MB,
						itlb_4MB,
						dtlb0_4KB,
						dtlb0_2MB,
						dtlb0_4MB,
						dtlb_4KB,
						dtlb_2MB,
						dtlb_4MB,
						dtlb_1GB,
						stlb2_4KB,
						stlb2_2MB,
						stlb2_1GB,
						&cache->prefetch_size);
				}
			}
			if (--iterations != 0) {
				descriptors.regs = cpuid(2);
				goto iterate_descriptors;
			}
		}

		if (vendor != cpuinfo_vendor_amd && vendor != cpuinfo_vendor_hygon && max_base_index >= 4) {
			struct cpuid_regs leaf4;
			uint32_t input_ecx = 0;
			uint32_t package_cores_max = 0;
			do {
				leaf4 = cpuidex(4, input_ecx++);
			} while (cpuinfo_x86_decode_deterministic_cache_parameters(leaf4, cache, &package_cores_max));
			if (package_cores_max != 0) {
				*log2_package_cores_max = bit_length(package_cores_max);
			}
		}
	}
	if (amd_topology_extensions && max_extended_index >= UINT32_C(0x8000001D)) {
		struct cpuid_regs leaf0x8000001D;
		uint32_t input_ecx = 0;
		do {
			leaf0x8000001D = cpuidex(UINT32_C(0x8000001D), input_ecx++);
		} while (cpuinfo_x86_decode_cache_properties(leaf0x8000001D, cache));
	}
}
