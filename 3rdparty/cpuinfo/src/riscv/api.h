#pragma once

#include <stdint.h>

#include <cpuinfo.h>
#include <cpuinfo/common.h>

/* RISC-V Vendor IDs. */
enum cpuinfo_riscv_chipset_vendor {
	cpuinfo_riscv_chipset_vendor_unknown = 0,
	cpuinfo_riscv_chipset_vendor_sifive = 0x489,
	cpuinfo_riscv_chipset_vendor_max,
};

/* RISC-V Architecture IDs. */
enum cpuinfo_riscv_chipset_arch {
	cpuinfo_riscv_chipset_arch_unknown = 0,
	cpuinfo_riscv_chipset_arch_max,
};

/* RISC-V Implementation IDs. */
enum cpuinfo_riscv_chipset_impl {
	cpuinfo_riscv_chipset_impl_unknown = 0,
	cpuinfo_riscv_chipset_impl_max,
};

/**
 * Decodes the vendor and micro-architecture based on the provided input
 * parameters, regardless of underlying operating system.
 *
 * @param[vendor_id]: The 'mvendorid' as described by the RISC-V Manual.
 * @param[arch_id]: The 'marchid' as described by the RISC-V Manual.
 * @param[imp_id]: The 'mimplid' as described by the RISC-V Manual.
 * @param[vendor] - Reference to the cpuinfo_vendor to populate.
 * @param[uarch] - Reference to the cpuinfo_uarch to populate.
 */
CPUINFO_INTERNAL void cpuinfo_riscv_decode_vendor_uarch(
	uint32_t vendor_id,
	uint32_t arch_id,
	uint32_t imp_id,
	enum cpuinfo_vendor vendor[restrict static 1],
	enum cpuinfo_uarch uarch[restrict static 1]);
