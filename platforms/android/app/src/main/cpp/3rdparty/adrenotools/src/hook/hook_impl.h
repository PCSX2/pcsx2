// SPDX-License-Identifier: BSD-2-Clause
// Copyright © 2021 Billy Laws

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <android/dlext.h>

void init_hook_param(const void *param);

void init_gsl(void *alloc, void *alloc64, void *free);

void *hook_android_dlopen_ext(const char *filename, int flags, const android_dlextinfo *extinfo);

void *hook_android_load_sphal_library(const char *filename, int flags);

FILE *hook_fopen(const char *filename, const char *mode);

int hook_gsl_memory_alloc_pure_64(uint64_t size, uint32_t flags, void *memDesc);

int hook_gsl_memory_free_pure(void *memDesc);

#ifdef __cplusplus
}
#endif

