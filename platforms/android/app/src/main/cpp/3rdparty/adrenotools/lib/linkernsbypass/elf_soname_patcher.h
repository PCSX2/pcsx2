// SPDX-License-Identifier: BSD-2-Clause
// Copyright © 2021 Billy Laws

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Overwrites a portion of the soname in an elf by loading it into shared memory and modifying .dynstr
 * @note   IMPORTANT: The supplied soname patch will overwrite the first strlen(sonamePatch) chars of the soname
 * @param  elfPath Full path to the elf to patch
 * @param  targetFd FD to use for storing the patched library
 * @return True on success
 */
bool elf_soname_patch(const char *elfPath, int targetFd, const char *newSoname);

#ifdef __cplusplus
}
#endif
