#include <stdint.h>

#include <cpuinfo/log.h>
#include <riscv/api.h>

void cpuinfo_riscv_decode_vendor_uarch(
	uint32_t vendor_id,
	uint32_t arch_id,
	uint32_t imp_id,
	enum cpuinfo_vendor vendor[restrict static 1],
	enum cpuinfo_uarch uarch[restrict static 1]) {
	/* The vendor ID is sufficient to determine the cpuinfo_vendor. */
	switch (vendor_id) {
		case cpuinfo_riscv_chipset_vendor_sifive:
			*vendor = cpuinfo_vendor_sifive;
			break;
		default:
			*vendor = cpuinfo_vendor_unknown;
			cpuinfo_log_warning("unknown vendor ID: %" PRIu32, vendor_id);
			break;
	}
	/**
	 * TODO: Add support for parsing chipset architecture and implementation
	 * IDs here, when a chipset of interest comes along.
	 */
	*uarch = cpuinfo_uarch_unknown;
}
