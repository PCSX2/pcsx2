#include <stdint.h>

#include <cpuinfo.h>
#include <cpuinfo/log.h>
#include <cpuinfo/utils.h>
#include <x86/cpuid.h>

enum cache_type {
	cache_type_none = 0,
	cache_type_data = 1,
	cache_type_instruction = 2,
	cache_type_unified = 3,
};

bool cpuinfo_x86_decode_deterministic_cache_parameters(
	struct cpuid_regs regs,
	struct cpuinfo_x86_caches* cache,
	uint32_t* package_cores_max) {
	const uint32_t type = regs.eax & UINT32_C(0x1F);
	if (type == cache_type_none) {
		return false;
	}

	/* Level starts at 1 */
	const uint32_t level = (regs.eax >> 5) & UINT32_C(0x7);

	const uint32_t sets = 1 + regs.ecx;
	const uint32_t line_size = 1 + (regs.ebx & UINT32_C(0x00000FFF));
	const uint32_t partitions = 1 + ((regs.ebx >> 12) & UINT32_C(0x000003FF));
	const uint32_t associativity = 1 + (regs.ebx >> 22);

	*package_cores_max = 1 + (regs.eax >> 26);
	const uint32_t processors = 1 + ((regs.eax >> 14) & UINT32_C(0x00000FFF));
	const uint32_t apic_bits = bit_length(processors);

	uint32_t flags = 0;
	if (regs.edx & UINT32_C(0x00000002)) {
		flags |= CPUINFO_CACHE_INCLUSIVE;
	}
	if (regs.edx & UINT32_C(0x00000004)) {
		flags |= CPUINFO_CACHE_COMPLEX_INDEXING;
	}
	switch (level) {
		case 1:
			switch (type) {
				case cache_type_unified:
					cache->l1d = cache->l1i = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags | CPUINFO_CACHE_UNIFIED,
						.apic_bits = apic_bits};
					break;
				case cache_type_data:
					cache->l1d = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
						.apic_bits = apic_bits};
					break;
				case cache_type_instruction:
					cache->l1i = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
						.apic_bits = apic_bits};
					break;
			}
			break;
		case 2:
			switch (type) {
				case cache_type_instruction:
					cpuinfo_log_warning(
						"unexpected L2 instruction cache reported in leaf 0x00000004 is ignored");
					break;
				case cache_type_unified:
					flags |= CPUINFO_CACHE_UNIFIED;
				case cache_type_data:
					cache->l2 = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
						.apic_bits = apic_bits};
					break;
			}
			break;
		case 3:
			switch (type) {
				case cache_type_instruction:
					cpuinfo_log_warning(
						"unexpected L3 instruction cache reported in leaf 0x00000004 is ignored");
					break;
				case cache_type_unified:
					flags |= CPUINFO_CACHE_UNIFIED;
				case cache_type_data:
					cache->l3 = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
						.apic_bits = apic_bits};
					break;
			}
			break;
		case 4:
			switch (type) {
				case cache_type_instruction:
					cpuinfo_log_warning(
						"unexpected L4 instruction cache reported in leaf 0x00000004 is ignored");
					break;
				case cache_type_unified:
					flags |= CPUINFO_CACHE_UNIFIED;
				case cache_type_data:
					cache->l4 = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
						.apic_bits = apic_bits};
					break;
			}
			break;
		default:
			cpuinfo_log_warning(
				"unexpected L%" PRIu32 " cache reported in leaf 0x00000004 is ignored", level);
			break;
	}
	return true;
}

bool cpuinfo_x86_decode_cache_properties(struct cpuid_regs regs, struct cpuinfo_x86_caches* cache) {
	const uint32_t type = regs.eax & UINT32_C(0x1F);
	if (type == cache_type_none) {
		return false;
	}

	const uint32_t level = (regs.eax >> 5) & UINT32_C(0x7);
	const uint32_t cores = 1 + ((regs.eax >> 14) & UINT32_C(0x00000FFF));
	const uint32_t apic_bits = bit_length(cores);

	const uint32_t sets = 1 + regs.ecx;
	const uint32_t line_size = 1 + (regs.ebx & UINT32_C(0x00000FFF));
	const uint32_t partitions = 1 + ((regs.ebx >> 12) & UINT32_C(0x000003FF));
	const uint32_t associativity = 1 + (regs.ebx >> 22);

	uint32_t flags = 0;
	if (regs.edx & UINT32_C(0x00000002)) {
		flags |= CPUINFO_CACHE_INCLUSIVE;
	}

	switch (level) {
		case 1:
			switch (type) {
				case cache_type_unified:
					cache->l1d = cache->l1i = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags | CPUINFO_CACHE_UNIFIED,
						.apic_bits = apic_bits};
					break;
				case cache_type_data:
					cache->l1d = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
						.apic_bits = apic_bits};
					break;
				case cache_type_instruction:
					cache->l1i = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
						.apic_bits = apic_bits};
					break;
			}
			break;
		case 2:
			switch (type) {
				case cache_type_instruction:
					cpuinfo_log_warning(
						"unexpected L2 instruction cache reported in leaf 0x8000001D is ignored");
					break;
				case cache_type_unified:
					flags |= CPUINFO_CACHE_UNIFIED;
				case cache_type_data:
					cache->l2 = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
						.apic_bits = apic_bits};
					break;
			}
			break;
		case 3:
			switch (type) {
				case cache_type_instruction:
					cpuinfo_log_warning(
						"unexpected L3 instruction cache reported in leaf 0x8000001D is ignored");
					break;
				case cache_type_unified:
					flags |= CPUINFO_CACHE_UNIFIED;
				case cache_type_data:
					cache->l3 = (struct cpuinfo_x86_cache){
						.size = associativity * partitions * line_size * sets,
						.associativity = associativity,
						.sets = sets,
						.partitions = partitions,
						.line_size = line_size,
						.flags = flags,
						.apic_bits = apic_bits};
					break;
			}
			break;
		default:
			cpuinfo_log_warning(
				"unexpected L%" PRIu32 " cache reported in leaf 0x8000001D is ignored", level);
			break;
	}
	return true;
}
