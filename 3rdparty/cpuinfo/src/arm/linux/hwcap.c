#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <elf.h>

#if CPUINFO_MOCK
	#include <cpuinfo-mock.h>
#endif
#include <cpuinfo.h>
#include <arm/linux/api.h>
#include <cpuinfo/log.h>

#if CPUINFO_ARCH_ARM64 || CPUINFO_ARCH_ARM && !defined(__ANDROID__)
	#include <sys/auxv.h>
#else
	#define AT_HWCAP 16
	#define AT_HWCAP2 26
#endif


#if CPUINFO_MOCK
	static uint32_t mock_hwcap = 0;
	void cpuinfo_set_hwcap(uint32_t hwcap) {
		mock_hwcap = hwcap;
	}

	static uint32_t mock_hwcap2 = 0;
	void cpuinfo_set_hwcap2(uint32_t hwcap2) {
		mock_hwcap2 = hwcap2;
	}
#endif


#if CPUINFO_ARCH_ARM
	typedef unsigned long (*getauxval_function_t)(unsigned long);

	bool cpuinfo_arm_linux_hwcap_from_getauxval(
		uint32_t hwcap[restrict static 1],
		uint32_t hwcap2[restrict static 1])
	{
		#if CPUINFO_MOCK
			*hwcap  = mock_hwcap;
			*hwcap2 = mock_hwcap2;
			return true;
		#elif defined(__ANDROID__)
			/* Android: dynamically check if getauxval is supported */
			void* libc = NULL;
			getauxval_function_t getauxval = NULL;

			dlerror();
			libc = dlopen("libc.so", RTLD_LAZY);
			if (libc == NULL) {
				cpuinfo_log_warning("failed to load libc.so: %s", dlerror());
				goto cleanup;
			}

			getauxval = (getauxval_function_t) dlsym(libc, "getauxval");
			if (getauxval == NULL) {
				cpuinfo_log_info("failed to locate getauxval in libc.so: %s", dlerror());
				goto cleanup;
			}

			*hwcap  = getauxval(AT_HWCAP);
			*hwcap2 = getauxval(AT_HWCAP2);

		cleanup:
			if (libc != NULL) {
				dlclose(libc);
				libc = NULL;
			}
			return getauxval != NULL;
		#else
			/* GNU/Linux: getauxval is always supported */
			*hwcap  = getauxval(AT_HWCAP);
			*hwcap2 = getauxval(AT_HWCAP2);
			return true;
		#endif
	}

	#ifdef __ANDROID__
		bool cpuinfo_arm_linux_hwcap_from_procfs(
			uint32_t hwcap[restrict static 1],
			uint32_t hwcap2[restrict static 1])
		{
			#if CPUINFO_MOCK
				*hwcap  = mock_hwcap;
				*hwcap2 = mock_hwcap2;
				return true;
			#else
				uint32_t hwcaps[2] = { 0, 0 };
				bool result = false;
				int file = -1;

				file = open("/proc/self/auxv", O_RDONLY);
				if (file == -1) {
					cpuinfo_log_warning("failed to open /proc/self/auxv: %s", strerror(errno));
					goto cleanup;
				}

				ssize_t bytes_read;
				do {
					Elf32_auxv_t elf_auxv;
					bytes_read = read(file, &elf_auxv, sizeof(Elf32_auxv_t));
					if (bytes_read < 0) {
						cpuinfo_log_warning("failed to read /proc/self/auxv: %s", strerror(errno));
						goto cleanup;
					} else if (bytes_read > 0) {
						if (bytes_read == sizeof(elf_auxv)) {
							switch (elf_auxv.a_type) {
								case AT_HWCAP:
									hwcaps[0] = (uint32_t) elf_auxv.a_un.a_val;
									break;
								case AT_HWCAP2:
									hwcaps[1] = (uint32_t) elf_auxv.a_un.a_val;
									break;
							}
						} else {
							cpuinfo_log_warning(
								"failed to read %zu bytes from /proc/self/auxv: %zu bytes available",
								sizeof(elf_auxv), (size_t) bytes_read);
							goto cleanup;
						}
					}
				} while (bytes_read == sizeof(Elf32_auxv_t));

				/* Success, commit results */
				*hwcap  = hwcaps[0];
				*hwcap2 = hwcaps[1];
				result = true;

			cleanup:
				if (file != -1) {
					close(file);
					file = -1;
				}
				return result;
			#endif
		}
	#endif /* __ANDROID__ */
#elif CPUINFO_ARCH_ARM64
	void cpuinfo_arm_linux_hwcap_from_getauxval(
		uint32_t hwcap[restrict static 1],
		uint32_t hwcap2[restrict static 1])
	{
		#if CPUINFO_MOCK
			*hwcap  = mock_hwcap;
			*hwcap2 = mock_hwcap2;
		#else
			*hwcap  = (uint32_t) getauxval(AT_HWCAP);
			*hwcap2 = (uint32_t) getauxval(AT_HWCAP2);
			return ;
		#endif
	}
#endif
