#include <stdint.h>

#include <cpuinfo.h>
#include <x86/api.h>


void cpuinfo_x86_decode_cache_descriptor(
	uint8_t descriptor, enum cpuinfo_vendor vendor,
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
	uint32_t* prefetch_size)
{
	/*
	 * Descriptors are parsed according to:
	 * - Application Note 485: Intel Processor Indentification and CPUID Instruction, May 2012, Order Number 241618-039
	 * - Intel 64 and IA-32 Architectures Software Developer’s Manual, Volume 2 (2A, 2B, 2C & 2D): Instruction Set
	 *   Reference, A-Z, December 2016. Order Number: 325383-061US
	 * - Cyrix CPU Detection Guide, Preliminary Revision 1.01
	 * - Geode(TM) GX1 Processor Series: Low Power Integrated x86 Solution
	 */
	switch (descriptor) {
		case 0x01:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4 KByte pages, 4-way set associative, 32 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 4-KB Pages, 4-way set associative, 32 entries"
			 */
			*itlb_4KB = (struct cpuinfo_tlb) {
				.entries = 32,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0x02:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4 MByte pages, fully associative, 2 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 4-MB Pages, fully associative, 2 entries"
			 */
			*itlb_4MB = (struct cpuinfo_tlb) {
				.entries = 2,
				.associativity = 2,
				.pages = CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x03:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB: 4 KByte pages, 4-way set associative, 64 entries"
			 * Application Note 485:
			 *     "Data TLB: 4-KB Pages, 4-way set associative, 64 entries"
			 */
			*dtlb_4KB = (struct cpuinfo_tlb) {
				.entries = 64,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0x04:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB: 4 MByte pages, 4-way set associative, 8 entries"
			 * Application Note 485:
			 *     "Data TLB: 4-MB Pages, 4-way set associative, 8 entries"
			 */
			*dtlb_4MB = (struct cpuinfo_tlb) {
				.entries = 8,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x05:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB1: 4 MByte pages, 4-way set associative, 32 entries"
			 * Application Note 485:
			 *     "Data TLB: 4-MB Pages, 4-way set associative, 32 entries"
			 */
			*dtlb_4MB = (struct cpuinfo_tlb) {
				.entries = 32,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x06:
			/*
			 * Intel ISA Reference:
			 *     "1st-level instruction cache: 8 KBytes, 4-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "1st-level instruction cache: 8-KB, 4-way set associative, 32-byte line size"
			 */
			cache->l1i = (struct cpuinfo_x86_cache) {
				.size = 8 * 1024,
				.associativity = 4,
				.sets = 64,
				.partitions = 1,
				.line_size = 32,
			};
			break;
		case 0x08:
			/*
			 * Intel ISA Reference:
			 *     "1st-level instruction cache: 16 KBytes, 4-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "1st-level instruction cache: 16-KB, 4-way set associative, 32-byte line size"
			 */
			cache->l1i = (struct cpuinfo_x86_cache) {
				.size = 16 * 1024,
				.associativity = 4,
				.sets = 128,
				.partitions = 1,
				.line_size = 32,
			};
			break;
		case 0x09:
			/*
			 * Intel ISA Reference:
			 *     "1st-level instruction cache: 32KBytes, 4-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "1st-level Instruction Cache: 32-KB, 4-way set associative, 64-byte line size"
			 */
			cache->l1i = (struct cpuinfo_x86_cache) {
				.size = 32 * 1024,
				.associativity = 4,
				.sets = 128,
				.partitions = 1,
				.line_size = 64,
			};
			break;
		case 0x0A:
			/*
			 * Intel ISA Reference:
			 *     "1st-level data cache: 8 KBytes, 2-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "1st-level data cache: 8-KB, 2-way set associative, 32-byte line size"
			 */
			cache->l1d = (struct cpuinfo_x86_cache) {
				.size = 8 * 1024,
				.associativity = 2,
				.sets = 128,
				.partitions = 1,
				.line_size = 32,
			};
			break;
		case 0x0B:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4 MByte pages, 4-way set associative, 4 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 4-MB pages, 4-way set associative, 4 entries"
			 */
			*itlb_4MB = (struct cpuinfo_tlb) {
				.entries = 4,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x0C:
			/*
			 * Intel ISA Reference:
			 *     "1st-level data cache: 16 KBytes, 4-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "1st-level data cache: 16-KB, 4-way set associative, 32-byte line size"
			 */
			cache->l1d = (struct cpuinfo_x86_cache) {
				.size = 16 * 1024,
				.associativity = 4,
				.sets = 128,
				.partitions = 1,
				.line_size = 32,
			};
			break;
		case 0x0D:
			/*
			 * Intel ISA Reference:
			 *     "1st-level data cache: 16 KBytes, 4-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "1st-level Data Cache: 16-KB, 4-way set associative, 64-byte line size"
			 */
			cache->l1d = (struct cpuinfo_x86_cache) {
				.size = 16 * 1024,
				.associativity = 4,
				.sets = 64,
				.partitions = 1,
				.line_size = 64,
			};
			break;
		case 0x0E:
			/*
			 * Intel ISA Reference:
			 *     "1st-level data cache: 24 KBytes, 6-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "1st-level Data Cache: 24-KB, 6-way set associative, 64-byte line size"
			 */
			cache->l1d = (struct cpuinfo_x86_cache) {
				.size = 24 * 1024,
				.associativity = 6,
				.sets = 64,
				.partitions = 1,
				.line_size = 64,
			};
			break;
		case 0x1D:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 128 KBytes, 2-way set associative, 64 byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 128 * 1024,
				.associativity = 2,
				.sets = 1024,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
		case 0x21:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 256 KBytes, 8-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 256-KB, 8-way set associative, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 256 * 1024,
				.associativity = 8,
				.sets = 512,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x22:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 512 KBytes, 4-way set associative, 64 byte line size, 2 lines per sector"
			 * Application Note 485:
			 *     "3rd-level cache: 512-KB, 4-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 512 * 1024,
				.associativity = 4,
				.sets = 2048,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x23:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 1 MBytes, 8-way set associative, 64 byte line size, 2 lines per sector"
			 * Application Note 485:
			 *     "3rd-level cache: 1-MB, 8-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 1024 * 1024,
				.associativity = 8,
				.sets = 2048,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x24:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 1 MBytes, 16-way set associative, 64 byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 1024 * 1024,
				.associativity = 16,
				.sets = 1024,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x25:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 2 MBytes, 8-way set associative, 64 byte line size, 2 lines per sector"
			 * Application Note 485:
			 *     "3rd-level cache: 2-MB, 8-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 2 * 1024 * 1024,
				.associativity = 8,
				.sets = 4096,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x29:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 4 MBytes, 8-way set associative, 64 byte line size, 2 lines per sector"
			 * Application Note 485:
			 *     "3rd-level cache: 4-MB, 8-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 4 * 1024 * 1024,
				.associativity = 8,
				.sets = 8192,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x2C:
			/*
			 * Intel ISA Reference:
			 *     "1st-level data cache: 32 KBytes, 8-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "1st-level data cache: 32-KB, 8-way set associative, 64-byte line size"
			 */
			cache->l1d = (struct cpuinfo_x86_cache) {
				.size = 32 * 1024,
				.associativity = 8,
				.sets = 64,
				.partitions = 1,
				.line_size = 64,
			};
			break;
		case 0x30:
			/*
			 * Intel ISA Reference:
			 *     "1st-level instruction cache: 32 KBytes, 8-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "1st-level instruction cache: 32-KB, 8-way set associative, 64-byte line size"
			 */
			cache->l1i = (struct cpuinfo_x86_cache) {
				.size = 32 * 1024,
				.associativity = 8,
				.sets = 64,
				.partitions = 1,
				.line_size = 64,
			};
			break;
		case 0x39:
			/* Where does this come from? */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 128 * 1024,
				.associativity = 4,
				.sets = 512,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x3A:
			/* Where does this come from? */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 192 * 1024,
				.associativity = 6,
				.sets = 512,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x3B:
			/* Where does this come from? */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 128 * 1024,
				.associativity = 2,
				.sets = 1024,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x3C:
			/* Where does this come from? */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 256 * 1024,
				.associativity = 4,
				.sets = 1024,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x3D:
			/* Where does this come from? */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 384 * 1024,
				.associativity = 6,
				.sets = 1024,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x3E:
			/* Where does this come from? */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 512 * 1024,
				.associativity = 4,
				.sets = 2048,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x40:
			/*
			 * Intel ISA Reference:
			 *     "No 2nd-level cache or, if processor contains a valid 2nd-level cache, no 3rd-level cache"
			 * Application Note 485:
			 *     "No 2nd-level cache or, if processor contains a valid 2nd-level cache, no 3rd-level cache"
			 */
			break;
		case 0x41:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 128 KBytes, 4-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 128-KB, 4-way set associative, 32-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 128 * 1024,
				.associativity = 4,
				.sets = 1024,
				.partitions = 1,
				.line_size = 32,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x42:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 256 KBytes, 4-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 256-KB, 4-way set associative, 32-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 256 * 1024,
				.associativity = 4,
				.sets = 2048,
				.partitions = 1,
				.line_size = 32,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x43:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 512 KBytes, 4-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 512-KB, 4-way set associative, 32-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 512 * 1024,
				.associativity = 4,
				.sets = 4096,
				.partitions = 1,
				.line_size = 32,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x44:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 1 MByte, 4-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 1-MB, 4-way set associative, 32-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 1024 * 1024,
				.associativity = 4,
				.sets = 8192,
				.partitions = 1,
				.line_size = 32,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x45:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 2 MByte, 4-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 2-MB, 4-way set associative, 32-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 2 * 1024 * 1024,
				.associativity = 4,
				.sets = 16384,
				.partitions = 1,
				.line_size = 32,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x46:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 4 MByte, 4-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 4-MB, 4-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 4 * 1024 * 1024,
				.associativity = 4,
				.sets = 16384,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x47:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 8 MByte, 8-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 8-MB, 8-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 8 * 1024 * 1024,
				.associativity = 8,
				.sets = 16384,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x48:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 3MByte, 12-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 3-MB, 12-way set associative, 64-byte line size, unified on-die"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 3 * 1024 * 1024,
				.associativity = 12,
				.sets = 4096,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x49:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 4MB, 16-way set associative, 64-byte line size (Intel Xeon processor MP,
			 *      Family 0FH, Model 06H); 2nd-level cache: 4 MByte, 16-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 4-MB, 16-way set associative, 64-byte line size (Intel Xeon processor MP,
			 *      Family 0Fh, Model 06h)
			 *      2nd-level cache: 4-MB, 16-way set associative, 64-byte line size"
			 */
			if ((vendor == cpuinfo_vendor_intel) && (model_info->model == 0x06) && (model_info->family == 0x0F)) {
				cache->l3 = (struct cpuinfo_x86_cache) {
					.size = 4 * 1024 * 1024,
					.associativity = 16,
					.sets = 4096,
					.partitions = 1,
					.line_size = 64,
					.flags = CPUINFO_CACHE_INCLUSIVE,
				};
			} else {
				cache->l2 = (struct cpuinfo_x86_cache) {
					.size = 4 * 1024 * 1024,
					.associativity = 16,
					.sets = 4096,
					.partitions = 1,
					.line_size = 64,
					.flags = CPUINFO_CACHE_INCLUSIVE,
				};
			}
			break;
		case 0x4A:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 6MByte, 12-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 6-MB, 12-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 6 * 1024 * 1024,
				.associativity = 12,
				.sets = 8192,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x4B:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 8MByte, 16-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 8-MB, 16-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 8 * 1024 * 1024,
				.associativity = 16,
				.sets = 8192,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x4C:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 12MByte, 12-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 12-MB, 12-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 12 * 1024 * 1024,
				.associativity = 12,
				.sets = 16384,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x4D:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 16MByte, 16-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 16-MB, 16-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 16 * 1024 * 1024,
				.associativity = 16,
				.sets = 16384,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x4E:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 6MByte, 24-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 6-MB, 24-way set associative, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 6 * 1024 * 1024,
				.associativity = 24,
				.sets = 4096,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x4F:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4 KByte pages, 32 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 4-KB pages, 32 entries"
			 */
			*itlb_4KB = (struct cpuinfo_tlb) {
				.entries = 32,
				/* Assume full associativity from nearby entries: manual lacks detail */
				.associativity = 32,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0x50:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4 KByte and 2-MByte or 4-MByte pages, 64 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 4-KB, 2-MB or 4-MB pages, fully associative, 64 entries"
			 */
			*itlb_4KB = *itlb_2MB = *itlb_4MB = (struct cpuinfo_tlb) {
				.entries = 64,
				.associativity = 64,
				.pages = CPUINFO_PAGE_SIZE_4KB | CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x51:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4 KByte and 2-MByte or 4-MByte pages, 128 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 4-KB, 2-MB or 4-MB pages, fully associative, 128 entries"
			 */
			*itlb_4KB = *itlb_2MB = *itlb_4MB = (struct cpuinfo_tlb) {
				.entries = 128,
				.associativity = 128,
				.pages = CPUINFO_PAGE_SIZE_4KB | CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x52:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4 KByte and 2-MByte or 4-MByte pages, 256 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 4-KB, 2-MB or 4-MB pages, fully associative, 256 entries"
			 */
			*itlb_4KB = *itlb_2MB = *itlb_4MB = (struct cpuinfo_tlb) {
				.entries = 256,
				.associativity = 256,
				.pages = CPUINFO_PAGE_SIZE_4KB | CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x55:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 2-MByte or 4-MByte pages, fully associative, 7 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 2-MB or 4-MB pages, fully associative, 7 entries"
			 */
			*itlb_2MB = *itlb_4MB = (struct cpuinfo_tlb) {
				.entries = 7,
				.associativity = 7,
				.pages = CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x56:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB0: 4 MByte pages, 4-way set associative, 16 entries"
			 * Application Note 485:
			 *     "L1 Data TLB: 4-MB pages, 4-way set associative, 16 entries"
			 */
			*dtlb0_4MB = (struct cpuinfo_tlb) {
				.entries = 16,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x57:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB0: 4 KByte pages, 4-way associative, 16 entries"
			 * Application Note 485:
			 *     "L1 Data TLB: 4-KB pages, 4-way set associative, 16 entries"
			 */
			*dtlb0_4KB = (struct cpuinfo_tlb) {
				.entries = 16,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0x59:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB0: 4 KByte pages, fully associative, 16 entries"
			 * Application Note 485:
			 *     "Data TLB0: 4-KB pages, fully associative, 16 entries"
			 */
			*dtlb0_4KB = (struct cpuinfo_tlb) {
				.entries = 16,
				.associativity = 16,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0x5A:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB0: 2 MByte or 4 MByte pages, 4-way set associative, 32 entries"
			 * Application Note 485:
			 *     "Data TLB0: 2-MB or 4-MB pages, 4-way associative, 32 entries"
			 */
			*dtlb0_2MB = *dtlb0_4MB = (struct cpuinfo_tlb) {
				.entries = 32,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x5B:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB: 4 KByte and 4 MByte pages, 64 entries"
			 * Application Note 485:
			 *     "Data TLB: 4-KB or 4-MB pages, fully associative, 64 entries"
			 */
			*dtlb_4KB = *dtlb_4MB = (struct cpuinfo_tlb) {
				.entries = 64,
				.associativity = 64,
				.pages = CPUINFO_PAGE_SIZE_4KB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x5C:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB: 4 KByte and 4 MByte pages, 128 entries"
			 * Application Note 485:
			 *     "Data TLB: 4-KB or 4-MB pages, fully associative, 128 entries"
			 */
			*dtlb_4KB = *dtlb_4MB = (struct cpuinfo_tlb) {
				.entries = 128,
				.associativity = 128,
				.pages = CPUINFO_PAGE_SIZE_4KB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x5D:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB: 4 KByte and 4 MByte pages, 256 entries"
			 * Application Note 485:
			 *     "Data TLB: 4-KB or 4-MB pages, fully associative, 256 entries"
			 */
			*dtlb_4KB = *dtlb_4MB = (struct cpuinfo_tlb) {
				.entries = 256,
				.associativity = 256,
				.pages = CPUINFO_PAGE_SIZE_4KB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x60:
			/*
			 * Application Note 485:
			 *     "1st-level data cache: 16-KB, 8-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l1d = (struct cpuinfo_x86_cache) {
				.size = 16 * 1024,
				.associativity = 8,
				.sets = 32,
				.partitions = 1,
				.line_size = 64,
			};
			break;
		case 0x61:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4 KByte pages, fully associative, 48 entries"
			 */
			*itlb_4KB = (struct cpuinfo_tlb) {
				.entries = 48,
				.associativity = 48,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0x63:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB: 2 MByte or 4 MByte pages, 4-way set associative, 32 entries and
			 *      a separate array with 1 GByte pages, 4-way set associative, 4 entries"
			 */
			*dtlb_2MB = *dtlb_4MB = (struct cpuinfo_tlb) {
				.entries = 32,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			*dtlb_1GB = (struct cpuinfo_tlb) {
				.entries = 4,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_1GB,
			};
			break;
		case 0x64:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB: 4 KByte pages, 4-way set associative, 512 entries"
			 *
			 */
			*dtlb_4KB = (struct cpuinfo_tlb) {
				.entries = 512,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0x66:
			/*
			 * Application Note 485:
			 *     "1st-level data cache: 8-KB, 4-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l1d = (struct cpuinfo_x86_cache) {
				.size = 8 * 1024,
				.associativity = 4,
				.sets = 32,
				.partitions = 1,
				.line_size = 64,
			};
			break;
		case 0x67:
			/*
			 * Application Note 485:
			 *     "1st-level data cache: 16-KB, 4-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l1d = (struct cpuinfo_x86_cache) {
				.size = 16 * 1024,
				.associativity = 4,
				.sets = 64,
				.partitions = 1,
				.line_size = 64,
			};
			break;
		case 0x68:
			/*
			 * Application Note 485:
			 *     "1st-level data cache: 32-KB, 4 way set associative, sectored cache, 64-byte line size"
			 */
			cache->l1d = (struct cpuinfo_x86_cache) {
				.size = 32 * 1024,
				.associativity = 4,
				.sets = 128,
				.partitions = 1,
				.line_size = 64,
			};
			break;
		case 0x6A:
			/*
			 * Intel ISA Reference:
			 *     "uTLB: 4 KByte pages, 8-way set associative, 64 entries"
			 */

			/* uTLB is, an fact, a normal 1-level DTLB on Silvermont & Knoghts Landing */
			*dtlb_4KB = (struct cpuinfo_tlb) {
				.entries = 64,
				.associativity = 8,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0x6B:
			/*
			 * Intel ISA Reference:
			 *     "DTLB: 4 KByte pages, 8-way set associative, 256 entries"
			 */
			*dtlb_4KB = (struct cpuinfo_tlb) {
				.entries = 256,
				.associativity = 8,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0x6C:
			/*
			 * Intel ISA Reference:
			 *     "DTLB: 2M/4M pages, 8-way set associative, 128 entries"
			 */
			*dtlb_2MB = *dtlb_4MB = (struct cpuinfo_tlb) {
				.entries = 128,
				.associativity = 8,
				.pages = CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x6D:
			/*
			 * Intel ISA Reference:
			 *     "DTLB: 1 GByte pages, fully associative, 16 entries"
			 */
			*dtlb_1GB = (struct cpuinfo_tlb) {
				.entries = 16,
				.associativity = 16,
				.pages = CPUINFO_PAGE_SIZE_1GB,
			};
			break;
		case 0x70:
			/*
			 * Intel ISA Reference:
			 *     "Trace cache: 12 K-uop, 8-way set associative"
			 * Application Note 485:
			 *     "Trace cache: 12K-uops, 8-way set associative"
			 * Cyrix CPU Detection Guide and Geode GX1 Processor Series:
			 *     "TLB, 32 entries, 4-way set associative, 4K-Byte Pages"
			 */
			switch (vendor) {
#if CPUINFO_ARCH_X86
				case cpuinfo_vendor_cyrix:
				case cpuinfo_vendor_nsc:
					*dtlb_4KB = *itlb_4KB = (struct cpuinfo_tlb) {
						.entries = 32,
						.associativity = 4,
						.pages = CPUINFO_PAGE_SIZE_4KB,
					};
					break;
#endif /* CPUINFO_ARCH_X86 */
				default:
					cache->trace = (struct cpuinfo_trace_cache) {
						.uops = 12 * 1024,
						.associativity = 8,
					};
			}
			break;
		case 0x71:
			/*
			 * Intel ISA Reference:
			 *     "Trace cache: 16 K-uop, 8-way set associative"
			 * Application Note 485:
			 *     "Trace cache: 16K-uops, 8-way set associative"
			 */
			cache->trace = (struct cpuinfo_trace_cache) {
				.uops = 16 * 1024,
				.associativity = 8,
			};
			break;
		case 0x72:
			/*
			 * Intel ISA Reference:
			 *     "Trace cache: 32 K-μop, 8-way set associative"
			 * Application Note 485:
			 *     "Trace cache: 32K-uops, 8-way set associative"
			 */
			cache->trace = (struct cpuinfo_trace_cache) {
				.uops = 32 * 1024,
				.associativity = 8,
			};
			break;
		case 0x73:
			/* Where does this come from? */
			cache->trace = (struct cpuinfo_trace_cache) {
				.uops = 64 * 1024,
				.associativity = 8,
			};
			break;
		case 0x76:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 2M/4M pages, fully associative, 8 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 2M/4M pages, fully associative, 8 entries"
			 */
			*itlb_2MB = *itlb_4MB = (struct cpuinfo_tlb) {
				.entries = 8,
				.associativity = 8,
				.pages = CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0x78:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 1 MByte, 4-way set associative, 64byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 1-MB, 4-way set associative, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 1024 * 1024,
				.associativity = 4,
				.sets = 4096,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x79:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 128 KByte, 8-way set associative, 64 byte line size, 2 lines per sector"
			 * Application Note 485:
			 *     "2nd-level cache: 128-KB, 8-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 128 * 1024,
				.associativity = 8,
				.sets = 256,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x7A:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 256 KByte, 8-way set associative, 64 byte line size, 2 lines per sector"
			 * Application Note 485:
			 *     "2nd-level cache: 256-KB, 8-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 256 * 1024,
				.associativity = 8,
				.sets = 512,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x7B:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 512 KByte, 8-way set associative, 64 byte line size, 2 lines per sector"
			 * Application Note 485:
			 *     "2nd-level cache: 512-KB, 8-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 512 * 1024,
				.associativity = 8,
				.sets = 1024,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x7C:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 1 MByte, 8-way set associative, 64 byte line size, 2 lines per sector"
			 * Application Note 485:
			 *     "2nd-level cache: 1-MB, 8-way set associative, sectored cache, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 1024 * 1024,
				.associativity = 8,
				.sets = 2048,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x7D:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 2 MByte, 8-way set associative, 64byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 2-MB, 8-way set associative, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 2 * 1024 * 1024,
				.associativity = 8,
				.sets = 4096,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x7F:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 512 KByte, 2-way set associative, 64-byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 512-KB, 2-way set associative, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 512 * 1024,
				.associativity = 2,
				.sets = 4096,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x80:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 512 KByte, 8-way set associative, 64-byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 512-KB, 8-way set associative, 64-byte line size"
			 * Cyrix CPU Detection Guide and Geode GX1 Processor Series:
			 *     "Level 1 Cache, 16K, 4-way set associative, 16 Bytes/Line"
			 */
			switch (vendor) {
#if CPUINFO_ARCH_X86 && !defined(__ANDROID__)
				case cpuinfo_vendor_cyrix:
				case cpuinfo_vendor_nsc:
					cache->l1i = cache->l1d = (struct cpuinfo_x86_cache) {
						.size = 16 * 1024,
						.associativity = 4,
						.sets = 256,
						.partitions = 1,
						.line_size = 16,
						.flags = CPUINFO_CACHE_UNIFIED,
					};
					break;
#endif /* CPUINFO_ARCH_X86 */
				default:
					cache->l2 = (struct cpuinfo_x86_cache) {
						.size = 512 * 1024,
						.associativity = 8,
						.sets = 1024,
						.partitions = 1,
						.line_size = 64,
						.flags = CPUINFO_CACHE_INCLUSIVE,
					};
			}
			break;
		case 0x82:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 256 KByte, 8-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 256-KB, 8-way set associative, 32-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 256 * 1024,
				.associativity = 4,
				.sets = 2048,
				.partitions = 1,
				.line_size = 32,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x83:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 512 KByte, 8-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 512-KB, 8-way set associative, 32-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 512 * 1024,
				.associativity = 8,
				.sets = 2048,
				.partitions = 1,
				.line_size = 32,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x84:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 1 MByte, 8-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 1-MB, 8-way set associative, 32-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 1024 * 1024,
				.associativity = 8,
				.sets = 4096,
				.partitions = 1,
				.line_size = 32,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x85:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 2 MByte, 8-way set associative, 32 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 2-MB, 8-way set associative, 32-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 2 * 1024 * 1024,
				.associativity = 8,
				.sets = 8192,
				.partitions = 1,
				.line_size = 32,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x86:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 512 KByte, 4-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 512-KB, 4-way set associative, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 512 * 1024,
				.associativity = 4,
				.sets = 2048,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0x87:
			/*
			 * Intel ISA Reference:
			 *     "2nd-level cache: 1 MByte, 8-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "2nd-level cache: 1-MB, 8-way set associative, 64-byte line size"
			 */
			cache->l2 = (struct cpuinfo_x86_cache) {
				.size = 1024 * 1024,
				.associativity = 8,
				.sets = 2048,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xA0:
			/*
			 * Intel ISA Reference:
			 *     "DTLB: 4k pages, fully associative, 32 entries"
			 */
			*dtlb_4KB = (struct cpuinfo_tlb) {
				.entries = 32,
				.associativity = 32,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0xB0:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4 KByte pages, 4-way set associative, 128 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 4-KB Pages, 4-way set associative, 128 entries"
			 */
			*itlb_4KB = (struct cpuinfo_tlb) {
				.entries = 128,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0xB1:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 2M pages, 4-way, 8 entries or 4M pages, 4-way, 4 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 2-MB pages, 4-way, 8 entries or 4M pages, 4-way, 4 entries"
			 */
			*itlb_2MB = (struct cpuinfo_tlb) {
				.entries = 8,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			*itlb_4MB = (struct cpuinfo_tlb) {
				.entries = 4,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0xB2:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4KByte pages, 4-way set associative, 64 entries"
			 * Application Note 485:
			 *     "Instruction TLB: 4-KB pages, 4-way set associative, 64 entries"
			 */
			*itlb_4KB = (struct cpuinfo_tlb) {
				.entries = 64,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0xB3:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB: 4 KByte pages, 4-way set associative, 128 entries"
			 * Application Note 485:
			 *     "Data TLB: 4-KB Pages, 4-way set associative, 128 entries"
			 */
			*dtlb_4KB = (struct cpuinfo_tlb) {
				.entries = 128,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0xB4:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB1: 4 KByte pages, 4-way associative, 256 entries"
			 * Application Note 485:
			 *     "Data TLB: 4-KB Pages, 4-way set associative, 256 entries"
			 */
			*dtlb_4KB = (struct cpuinfo_tlb) {
				.entries = 256,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0xB5:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4KByte pages, 8-way set associative, 64 entries"
			 */
			*itlb_4KB = (struct cpuinfo_tlb) {
				.entries = 64,
				.associativity = 8,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0xB6:
			/*
			 * Intel ISA Reference:
			 *     "Instruction TLB: 4KByte pages, 8-way set associative, 128 entries"
			 */
			*itlb_4KB = (struct cpuinfo_tlb) {
				.entries = 128,
				.associativity = 8,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0xBA:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB1: 4 KByte pages, 4-way associative, 64 entries"
			 * Application Note 485:
			 *     "Data TLB: 4-KB Pages, 4-way set associative, 64 entries"
			 */
			*itlb_4KB = (struct cpuinfo_tlb) {
				.entries = 64,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0xC0:
			/*
			 * Intel ISA Reference:
			 *     "Data TLB: 4 KByte and 4 MByte pages, 4-way associative, 8 entries"
			 * Application Note 485:
			 *     "Data TLB: 4-KB or 4-MB Pages, 4-way set associative, 8 entries"
			 */
			*itlb_4KB = *itlb_4MB = (struct cpuinfo_tlb) {
				.entries = 8,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0xC1:
			/*
			 * Intel ISA Reference:
			 *     "Shared 2nd-Level TLB: 4 KByte/2MByte pages, 8-way associative, 1024 entries"
			 */
			*stlb2_4KB = *stlb2_2MB = (struct cpuinfo_tlb) {
				.entries = 1024,
				.associativity = 8,
				.pages = CPUINFO_PAGE_SIZE_4KB | CPUINFO_PAGE_SIZE_2MB,
			};
			break;
		case 0xC2:
			/*
			 * Intel ISA Reference:
			 *     "DTLB: 4 KByte/2 MByte pages, 4-way associative, 16 entries"
			 */
			*dtlb_4KB = *dtlb_2MB = (struct cpuinfo_tlb) {
				.entries = 16,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB | CPUINFO_PAGE_SIZE_2MB,
			};
			break;
		case 0xC3:
			/*
			 * Intel ISA Reference:
			 *     "Shared 2nd-Level TLB: 4 KByte/2 MByte pages, 6-way associative, 1536 entries.
			 *      Also 1GBbyte pages, 4-way, 16 entries."
			 */
			*stlb2_4KB = *stlb2_2MB = (struct cpuinfo_tlb) {
				.entries = 1536,
				.associativity = 6,
				.pages = CPUINFO_PAGE_SIZE_4KB | CPUINFO_PAGE_SIZE_2MB,
			};
			*stlb2_1GB = (struct cpuinfo_tlb) {
				.entries = 16,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_1GB,
			};
			break;
		case 0xC4:
			/*
			 * Intel ISA Reference:
			 *     "DTLB: 2M/4M Byte pages, 4-way associative, 32 entries"
			 */
			*dtlb_2MB = *dtlb_4MB = (struct cpuinfo_tlb) {
				.entries = 32,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_2MB | CPUINFO_PAGE_SIZE_4MB,
			};
			break;
		case 0xCA:
			/*
			 * Intel ISA Reference:
			 *     "Shared 2nd-Level TLB: 4 KByte pages, 4-way associative, 512 entries"
			 * Application Note 485:
			 *     "Shared 2nd-level TLB: 4 KB pages, 4-way set associative, 512 entries"
			 */
			*stlb2_4KB = (struct cpuinfo_tlb) {
				.entries = 512,
				.associativity = 4,
				.pages = CPUINFO_PAGE_SIZE_4KB,
			};
			break;
		case 0xD0:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 512 KByte, 4-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 512-kB, 4-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 512 * 1024,
				.associativity = 4,
				.sets = 2048,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xD1:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 1 MByte, 4-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 1-MB, 4-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 1024 * 1024,
				.associativity = 4,
				.sets = 4096,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xD2:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 2 MByte, 4-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 2-MB, 4-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 2 * 1024 * 2014,
				.associativity = 4,
				.sets = 8192,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xD6:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 1 MByte, 8-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 1-MB, 8-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 1024 * 1024,
				.associativity = 8,
				.sets = 2048,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xD7:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 2 MByte, 8-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 2-MB, 8-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 2 * 1024 * 1024,
				.associativity = 8,
				.sets = 4096,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xD8:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 4 MByte, 8-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 4-MB, 8-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 4 * 1024 * 1024,
				.associativity = 8,
				.sets = 8192,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xDC:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 1.5 MByte, 12-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 1.5-MB, 12-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 3 * 512 * 1024,
				.associativity = 12,
				.sets = 2048,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xDD:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 3 MByte, 12-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 3-MB, 12-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 3 * 1024 * 1024,
				.associativity = 12,
				.sets = 4096,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xDE:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 6 MByte, 12-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 6-MB, 12-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 6 * 1024 * 1024,
				.associativity = 12,
				.sets = 8192,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xE2:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 2 MByte, 16-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 2-MB, 16-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 2 * 1024 * 1024,
				.associativity = 16,
				.sets = 2048,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xE3:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 4 MByte, 16-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 4-MB, 16-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 4 * 1024 * 1024,
				.associativity = 16,
				.sets = 4096,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xE4:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 8 MByte, 16-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 8-MB, 16-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 8 * 1024 * 1024,
				.associativity = 16,
				.sets = 8192,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xEA:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 12MByte, 24-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 12-MB, 24-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 12 * 1024 * 1024,
				.associativity = 24,
				.sets = 8192,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xEB:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 18MByte, 24-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 18-MB, 24-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 18 * 1024 * 1024,
				.associativity = 24,
				.sets = 12288,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xEC:
			/*
			 * Intel ISA Reference:
			 *     "3rd-level cache: 24MByte, 24-way set associative, 64 byte line size"
			 * Application Note 485:
			 *     "3rd-level cache: 24-MB, 24-way set associative, 64-byte line size"
			 */
			cache->l3 = (struct cpuinfo_x86_cache) {
				.size = 24 * 1024 * 1024,
				.associativity = 24,
				.sets = 16384,
				.partitions = 1,
				.line_size = 64,
				.flags = CPUINFO_CACHE_INCLUSIVE,
			};
			break;
		case 0xF0:
			/*
			 * Intel ISA Reference:
			 *     "64-Byte prefetching"
			 * Application Note 485:
			 *     "64-byte Prefetching"
			 */
			cache->prefetch_size = 64;
			break;
		case 0xF1:
			/*
			 * Intel ISA Reference:
			 *     "128-Byte prefetching"
			 * Application Note 485:
			 *     "128-byte Prefetching"
			 */
			cache->prefetch_size = 128;
			break;
	}
}
