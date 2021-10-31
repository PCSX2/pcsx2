#include <stdint.h>


#if CPUINFO_MOCK
	extern uint32_t cpuinfo_arm_fpsid;
	extern uint32_t cpuinfo_arm_mvfr0;
	extern uint32_t cpuinfo_arm_wcid;

	static inline uint32_t read_fpsid(void) {
		return cpuinfo_arm_fpsid;
	}

	static inline uint32_t read_mvfr0(void) {
		return cpuinfo_arm_mvfr0;
	}

	static inline uint32_t read_wcid(void) {
		return cpuinfo_arm_wcid;
	}
#else
	#if !defined(__ARM_ARCH_7A__) && !defined(__ARM_ARCH_8A__) && !(defined(__ARM_ARCH) && (__ARM_ARCH >= 7))
		/*
		 * CoProcessor 10 is inaccessible from user mode since ARMv7,
		 * and clang refuses to compile inline assembly when targeting ARMv7+
		 */
		static inline uint32_t read_fpsid(void) {
			uint32_t fpsid;
			__asm__ __volatile__("MRC p10, 0x7, %[fpsid], cr0, cr0, 0" : [fpsid] "=r" (fpsid));
			return fpsid;
		}

		static inline uint32_t read_mvfr0(void) {
			uint32_t mvfr0;
			__asm__ __volatile__("MRC p10, 0x7, %[mvfr0], cr7, cr0, 0" : [mvfr0] "=r" (mvfr0));
			return mvfr0;
		}
	#endif

	static inline uint32_t read_wcid(void) {
		uint32_t wcid;
		__asm__ __volatile__("MRC p1, 0, %[wcid], c0, c0" : [wcid] "=r" (wcid));
		return wcid;
	}
#endif
