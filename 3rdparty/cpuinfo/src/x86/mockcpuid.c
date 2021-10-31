#include <stdint.h>
#include <stddef.h>

#if !CPUINFO_MOCK
	#error This file should be built only in mock mode
#endif

#include <cpuinfo-mock.h>


static struct cpuinfo_mock_cpuid* cpuinfo_mock_cpuid_data = NULL;
static uint32_t cpuinfo_mock_cpuid_entries = 0;
static uint32_t cpuinfo_mock_cpuid_leaf4_iteration = 0;

void CPUINFO_ABI cpuinfo_mock_set_cpuid(struct cpuinfo_mock_cpuid* dump, size_t entries) {
	cpuinfo_mock_cpuid_data = dump;
	cpuinfo_mock_cpuid_entries = entries;
};

void CPUINFO_ABI cpuinfo_mock_get_cpuid(uint32_t eax, uint32_t regs[4]) {
	if (eax != 4) {
		cpuinfo_mock_cpuid_leaf4_iteration = 0;
	}
	if (cpuinfo_mock_cpuid_data != NULL && cpuinfo_mock_cpuid_entries != 0) {
		if (eax == 4) {
			uint32_t skip_entries = cpuinfo_mock_cpuid_leaf4_iteration;
			for (uint32_t i = 0; i < cpuinfo_mock_cpuid_entries; i++) {
				if (eax == cpuinfo_mock_cpuid_data[i].input_eax) {
					if (skip_entries-- == 0) {
						regs[0] = cpuinfo_mock_cpuid_data[i].eax;
						regs[1] = cpuinfo_mock_cpuid_data[i].ebx;
						regs[2] = cpuinfo_mock_cpuid_data[i].ecx;
						regs[3] = cpuinfo_mock_cpuid_data[i].edx;
						cpuinfo_mock_cpuid_leaf4_iteration++;
						return;
					}
				}
			}
		} else {
			for (uint32_t i = 0; i < cpuinfo_mock_cpuid_entries; i++) {
				if (eax == cpuinfo_mock_cpuid_data[i].input_eax) {
					regs[0] = cpuinfo_mock_cpuid_data[i].eax;
					regs[1] = cpuinfo_mock_cpuid_data[i].ebx;
					regs[2] = cpuinfo_mock_cpuid_data[i].ecx;
					regs[3] = cpuinfo_mock_cpuid_data[i].edx;
					return;
				}
			}
		}
	}
	regs[0] = regs[1] = regs[2] = regs[3] = 0;
}

void CPUINFO_ABI cpuinfo_mock_get_cpuidex(uint32_t eax, uint32_t ecx, uint32_t regs[4]) {
	cpuinfo_mock_cpuid_leaf4_iteration = 0;
	if (cpuinfo_mock_cpuid_data != NULL && cpuinfo_mock_cpuid_entries != 0) {
		for (uint32_t i = 0; i < cpuinfo_mock_cpuid_entries; i++) {
			if (eax == cpuinfo_mock_cpuid_data[i].input_eax &&
				ecx == cpuinfo_mock_cpuid_data[i].input_ecx)
			{
				regs[0] = cpuinfo_mock_cpuid_data[i].eax;
				regs[1] = cpuinfo_mock_cpuid_data[i].ebx;
				regs[2] = cpuinfo_mock_cpuid_data[i].ecx;
				regs[3] = cpuinfo_mock_cpuid_data[i].edx;
				return;
			}
		}
	}
	regs[0] = regs[1] = regs[2] = regs[3] = 0;
}
