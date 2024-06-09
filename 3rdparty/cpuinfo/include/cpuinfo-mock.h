#pragma once
#ifndef CPUINFO_MOCK_H
#define CPUINFO_MOCK_H

#include <stddef.h>
#include <stdint.h>

#include <cpuinfo.h>
#if defined(__linux__)
#include <sys/types.h>
#endif

#if !defined(CPUINFO_MOCK) || !(CPUINFO_MOCK)
#error This header is intended only for test use
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if CPUINFO_ARCH_ARM
void CPUINFO_ABI cpuinfo_set_fpsid(uint32_t fpsid);
void CPUINFO_ABI cpuinfo_set_wcid(uint32_t wcid);
#endif /* CPUINFO_ARCH_ARM */

#if CPUINFO_ARCH_X86 || CPUINFO_ARCH_X86_64
struct cpuinfo_mock_cpuid {
	uint32_t input_eax;
	uint32_t input_ecx;
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
};

void CPUINFO_ABI cpuinfo_mock_set_cpuid(struct cpuinfo_mock_cpuid* dump, size_t entries);
void CPUINFO_ABI cpuinfo_mock_get_cpuid(uint32_t eax, uint32_t regs[4]);
void CPUINFO_ABI cpuinfo_mock_get_cpuidex(uint32_t eax, uint32_t ecx, uint32_t regs[4]);
#endif /* CPUINFO_ARCH_X86 || CPUINFO_ARCH_X86_64 */

struct cpuinfo_mock_file {
	const char* path;
	size_t size;
	const char* content;
	size_t offset;
};

struct cpuinfo_mock_property {
	const char* key;
	const char* value;
};

#if defined(__linux__)
void CPUINFO_ABI cpuinfo_mock_filesystem(struct cpuinfo_mock_file* files);
int CPUINFO_ABI cpuinfo_mock_open(const char* path, int oflag);
int CPUINFO_ABI cpuinfo_mock_close(int fd);
ssize_t CPUINFO_ABI cpuinfo_mock_read(int fd, void* buffer, size_t capacity);

#if CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
void CPUINFO_ABI cpuinfo_set_hwcap(uint32_t hwcap);
#endif
#if CPUINFO_ARCH_ARM
void CPUINFO_ABI cpuinfo_set_hwcap2(uint32_t hwcap2);
#endif
#endif

#if defined(__ANDROID__)
void CPUINFO_ABI cpuinfo_mock_android_properties(struct cpuinfo_mock_property* properties);
void CPUINFO_ABI cpuinfo_mock_gl_renderer(const char* renderer);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CPUINFO_MOCK_H */
