

switch (uarch) {
	case cpuinfo_uarch_cortex_a5:
		/*
		 * Cortex-A5 Technical Reference Manual:
		 * 6.3.1. Micro TLB
		 *   The first level of caching for the page table information is a micro TLB of
		 *   10 entries that is implemented on each of the instruction and data sides.
		 * 6.3.2. Main TLB
		 *   Misses from the instruction and data micro TLBs are handled by a unified main TLB.
		 *   The main TLB is 128-entry two-way set-associative.
		 */
		break;
	case cpuinfo_uarch_cortex_a7:
		/*
		 * Cortex-A7 MPCore Technical Reference Manual:
		 * 5.3.1. Micro TLB
		 *   The first level of caching for the page table information is a micro TLB of
		 *   10 entries that is implemented on each of the instruction and data sides.
		 * 5.3.2. Main TLB
		 *   Misses from the micro TLBs are handled by a unified main TLB. This is a 256-entry 2-way
		 *   set-associative structure. The main TLB supports all the VMSAv7 page sizes of
		 *   4KB, 64KB, 1MB and 16MB in addition to the LPAE page sizes of 2MB and 1G.
		 */
		break;
	case cpuinfo_uarch_cortex_a8:
		/*
		 * Cortex-A8 Technical Reference Manual:
		 * 6.1. About the MMU
		 *    The MMU features include the following:
		 *     - separate, fully-associative, 32-entry data and instruction TLBs
		 *     - TLB entries that support 4KB, 64KB, 1MB, and 16MB pages
		 */
		break;
	case cpuinfo_uarch_cortex_a9:
		/*
		 * ARM Cortex‑A9 Technical Reference Manual:
		 * 6.2.1 Micro TLB
		 *    The first level of caching for the page table information is a micro TLB of 32 entries on the data side,
		 *    and configurable 32 or 64 entries on the instruction side.
		 * 6.2.2 Main TLB
		 *    The main TLB is implemented as a combination of:
		 *     - A fully-associative, lockable array of four elements.
		 *     - A 2-way associative structure of 2x32, 2x64, 2x128 or 2x256 entries.
		 */
		break;
	case cpuinfo_uarch_cortex_a15:
		/*
		 * ARM Cortex-A15 MPCore Processor Technical Reference Manual:
		 * 5.2.1. L1 instruction TLB
		 *    The L1 instruction TLB is a 32-entry fully-associative structure. This TLB caches entries at the 4KB
		 *    granularity of Virtual Address (VA) to Physical Address (PA) mapping only. If the page tables map the
		 *    memory region to a larger granularity than 4K, it only allocates one mapping for the particular 4K region
		 *    to which the current access corresponds.
		 * 5.2.2. L1 data TLB
		 *    There are two separate 32-entry fully-associative TLBs that are used for data loads and stores,
		 *    respectively. Similar to the L1 instruction TLB, both of these cache entries at the 4KB granularity of
		 *    VA to PA mappings only. At implementation time, the Cortex-A15 MPCore processor can be configured with
		 *    the -l1tlb_1m option, to have the L1 data TLB cache entries at both the 4KB and 1MB granularity.
		 *    With this configuration, any translation that results in a 1MB or larger page is cached in the L1 data
		 *    TLB as a 1MB entry. Any translation that results in a page smaller than 1MB is cached in the L1 data TLB
		 *    as a 4KB entry. By default, all translations are cached in the L1 data TLB as a 4KB entry.
		 * 5.2.3. L2 TLB
		 *    Misses from the L1 instruction and data TLBs are handled by a unified L2 TLB. This is a 512-entry 4-way
		 *    set-associative structure. The L2 TLB supports all the VMSAv7 page sizes of 4K, 64K, 1MB and 16MB in
		 *    addition to the LPAE page sizes of 2MB and 1GB.
		 */
		break;
	case cpuinfo_uarch_cortex_a17:
		/*
		 * ARM Cortex-A17 MPCore Processor Technical Reference Manual:
		 * 5.2.1. Instruction micro TLB
		 *    The instruction micro TLB is implemented as a 32, 48 or 64 entry, fully-associative structure. This TLB
		 *    caches entries at the 4KB and 1MB granularity of Virtual Address (VA) to Physical Address (PA) mapping
		 *    only. If the translation tables map the memory region to a larger granularity than 4KB or 1MB, it only
		 *    allocates one mapping for the particular 4KB region to which the current access corresponds.
		 * 5.2.2. Data micro TLB
		 *    The data micro TLB is a 32 entry fully-associative TLB that is used for data loads and stores. The cache
		 *    entries have a 4KB and 1MB granularity of VA to PA mappings only.
		 * 5.2.3. Unified main TLB
		 *    Misses from the instruction and data micro TLBs are handled by a unified main TLB. This is a 1024 entry
		 *    4-way set-associative structure. The main TLB supports all the VMSAv7 page sizes of 4K, 64K, 1MB and 16MB
		 *    in addition to the LPAE page sizes of 2MB and 1GB.
		 */
		break;
	case cpuinfo_uarch_cortex_a35:
		/*
		 * ARM Cortex‑A35 Processor Technical Reference Manual:
		 * A6.2 TLB Organization
		 *   Micro TLB
		 *     The first level of caching for the translation table information is a micro TLB of ten entries that
		 *     is implemented on each of the instruction and data sides.
		 *   Main TLB
		 *     A unified main TLB handles misses from the micro TLBs. It has a 512-entry, 2-way, set-associative
		 *     structure and supports all VMSAv8 block sizes, except 1GB. If it fetches a 1GB block, the TLB splits
		 *     it into 512MB blocks and stores the appropriate block for the lookup.
		 */
		break;
	case cpuinfo_uarch_cortex_a53:
		/*
		 * ARM Cortex-A53 MPCore Processor Technical Reference Manual:
		 * 5.2.1. Micro TLB
		 *    The first level of caching for the translation table information is a micro TLB of ten entries that is
		 *    implemented on each of the instruction and data sides.
		 * 5.2.2. Main TLB
		 *    A unified main TLB handles misses from the micro TLBs. This is a 512-entry, 4-way, set-associative
		 *    structure. The main TLB supports all VMSAv8 block sizes, except 1GB. If a 1GB block is fetched, it is
		 *    split into 512MB blocks and the appropriate block for the lookup stored.
		 */
		break;
	case cpuinfo_uarch_cortex_a57:
		/*
		 * ARM® Cortex-A57 MPCore Processor Technical Reference Manual:
		 * 5.2.1 L1 instruction TLB
		 *    The L1 instruction TLB is a 48-entry fully-associative structure. This TLB caches entries of three
		 *    different page sizes, natively 4KB, 64KB, and 1MB, of VA to PA mappings. If the page tables map the memory
		 *    region to a larger granularity than 1MB, it only allocates one mapping for the particular 1MB region to
		 *    which the current access corresponds.
		 * 5.2.2 L1 data TLB
		 *    The L1 data TLB is a 32-entry fully-associative TLB that is used for data loads and stores. This TLB
		 *    caches entries of three different page sizes, natively 4KB, 64KB, and 1MB, of VA to PA mappings.
		 * 5.2.3 L2 TLB
		 *    Misses from the L1 instruction and data TLBs are handled by a unified L2 TLB. This is a 1024-entry 4-way
		 *    set-associative structure. The L2 TLB supports the page sizes of 4K, 64K, 1MB and 16MB. It also supports
		 *    page sizes of 2MB and 1GB for the long descriptor format translation in AArch32 state and in AArch64 state
		 *    when using the 4KB translation granule. In addition, the L2 TLB supports the 512MB page map size defined
		 *    for the AArch64 translations that use a 64KB translation granule.
		 */
		break;
}


