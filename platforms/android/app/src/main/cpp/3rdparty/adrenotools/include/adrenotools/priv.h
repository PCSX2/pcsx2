// SPDX-License-Identifier: BSD-2-Clause
// Copyright © 2021 Billy Laws

#pragma once

#include <stdint.h>

/**
 * @brief Bitfield enum of additional driver features that can be used with adrenotools_open_libvulkan
 */
enum {
    ADRENOTOOLS_DRIVER_CUSTOM = 1 << 0,
    ADRENOTOOLS_DRIVER_FILE_REDIRECT = 1 << 1,
    ADRENOTOOLS_DRIVER_GPU_MAPPING_IMPORT = 1 << 2,
};

#define ADRENOTOOLS_GPU_MAPPING_SUCCEEDED_MAGIC 0xDEADBEEF

/**
 * @brief A replacement GPU memory mapping for use with ADRENOTOOLS_DRIVER_GPU_MAPPING_IMPORT
 */
struct adrenotools_gpu_mapping {
    void *host_ptr;
    uint64_t gpu_addr; //!< The GPU address of the mapping to import, if mapping import/init succeeds this will be set to ADRENOTOOLS_GPU_MAPPING_SUCCEEDED_MAGIC
    uint64_t size;
    uint64_t flags;
};