/*
 * Only enable the C standard library hwprobe interface on Android for now.
 * Patches to add a compatible hwprobe API to glibc are available but not
 * merged at the time of writing and so cannot easily be tested.  The
 * #ifdef __ANDROID__ check will be removed in the future.
 */
#ifdef __ANDROID__
#ifdef __has_include
#if __has_include(<sys/hwprobe.h>)
#define CPUINFO_RISCV_LINUX_HAVE_C_HWPROBE
#include <sys/hwprobe.h>
#endif
#endif
#endif

#include <sched.h>

#include <cpuinfo/log.h>
#include <riscv/api.h>
#include <riscv/linux/api.h>

#ifndef CPUINFO_RISCV_LINUX_HAVE_C_HWPROBE

#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>

struct riscv_hwprobe {
	int64_t key;
	uint64_t value;
};

/*
 * The standard C library our binary was compiled with does not support
 * hwprobe but the kernel on which we are running might do.  The
 * constants below are copied from
 * /usr/include/riscv64-linux-gnu/asm/hwprobe.h.  They allow us to
 * invoke the hwprobe syscall directly.  We duplicate the constants
 * rather than including the kernel hwprobe.h header, as this header
 * will only be present if we're building Linux 6.4 or greater.
 */

#define RISCV_HWPROBE_KEY_MVENDORID 0
#define RISCV_HWPROBE_KEY_MARCHID 1
#define RISCV_HWPROBE_KEY_MIMPID 2
#define RISCV_HWPROBE_KEY_BASE_BEHAVIOR 3
#define RISCV_HWPROBE_BASE_BEHAVIOR_IMA (1 << 0)
#define RISCV_HWPROBE_KEY_IMA_EXT_0 4
#define RISCV_HWPROBE_IMA_FD (1 << 0)
#define RISCV_HWPROBE_IMA_C (1 << 1)
#define RISCV_HWPROBE_IMA_V (1 << 2)
#define RISCV_HWPROBE_EXT_ZBA (1 << 3)
#define RISCV_HWPROBE_EXT_ZBB (1 << 4)
#define RISCV_HWPROBE_EXT_ZBS (1 << 5)
#define RISCV_HWPROBE_EXT_ZICBOZ (1 << 6)
#define RISCV_HWPROBE_KEY_CPUPERF_0 5
#define RISCV_HWPROBE_MISALIGNED_UNKNOWN (0 << 0)
#define RISCV_HWPROBE_MISALIGNED_EMULATED (1 << 0)
#define RISCV_HWPROBE_MISALIGNED_SLOW (2 << 0)
#define RISCV_HWPROBE_MISALIGNED_FAST (3 << 0)
#define RISCV_HWPROBE_MISALIGNED_UNSUPPORTED (4 << 0)
#define RISCV_HWPROBE_MISALIGNED_MASK (7 << 0)

#ifndef NR_riscv_hwprobe
#ifndef NR_arch_specific_syscall
#define NR_arch_specific_syscall 244
#endif
#define NR_riscv_hwprobe (NR_arch_specific_syscall + 14)
#endif
#endif

void cpuinfo_riscv_linux_decode_vendor_uarch_from_hwprobe(
	uint32_t processor,
	enum cpuinfo_vendor vendor[restrict static 1],
	enum cpuinfo_uarch uarch[restrict static 1]) {
	struct riscv_hwprobe pairs[] = {
		{
			.key = RISCV_HWPROBE_KEY_MVENDORID,
		},
		{
			.key = RISCV_HWPROBE_KEY_MARCHID,
		},
		{
			.key = RISCV_HWPROBE_KEY_MIMPID,
		},
	};
	const size_t pairs_count = sizeof(pairs) / sizeof(struct riscv_hwprobe);

	/* In case of failure, report unknown. */
	*vendor = cpuinfo_vendor_unknown;
	*uarch = cpuinfo_uarch_unknown;

	/* Create a CPU set with this processor flagged. */
	const size_t cpu_count = processor + 1;
	cpu_set_t* cpu_set = CPU_ALLOC(cpu_count);
	if (cpu_set == NULL) {
		cpuinfo_log_warning("failed to allocate space for cpu_set");
		return;
	}

	const size_t cpu_set_size = CPU_ALLOC_SIZE(cpu_count);
	CPU_ZERO_S(cpu_set_size, cpu_set);
	CPU_SET_S(processor, cpu_set_size, cpu_set);

	/* Request all available information from hwprobe. */
#ifndef CPUINFO_RISCV_LINUX_HAVE_C_HWPROBE
	/*
	 * No standard library support for hwprobe.  We'll need to invoke the
	 * syscall directly.  See
	 *
	 * https://docs.kernel.org/arch/riscv/hwprobe.html
	 *
	 * for more details.
	 */
	int ret = syscall(NR_riscv_hwprobe, pairs, pairs_count, cpu_set_size, (unsigned long*)cpu_set, 0 /* flags */);
#else
	int ret = __riscv_hwprobe(pairs, pairs_count, cpu_set_size, (unsigned long*)cpu_set, 0 /* flags */);
#endif
	if (ret < 0) {
		cpuinfo_log_warning("failed to get hwprobe information, err: %d", ret);
		goto cleanup;
	}

	/**
	 * The syscall may not have populated all requested keys, loop through
	 * the list and store the values that were discovered.
	 */
	uint32_t vendor_id = 0;
	uint32_t arch_id = 0;
	uint32_t imp_id = 0;
	for (size_t pair = 0; pair < pairs_count; pair++) {
		switch (pairs[pair].key) {
			case RISCV_HWPROBE_KEY_MVENDORID:
				vendor_id = pairs[pair].value;
				break;
			case RISCV_HWPROBE_KEY_MARCHID:
				arch_id = pairs[pair].value;
				break;
			case RISCV_HWPROBE_KEY_MIMPID:
				imp_id = pairs[pair].value;
				break;
			default:
				/* The key value may be -1 if unsupported. */
				break;
		}
	}
	cpuinfo_riscv_decode_vendor_uarch(vendor_id, arch_id, imp_id, vendor, uarch);

cleanup:
	CPU_FREE(cpu_set);
}
