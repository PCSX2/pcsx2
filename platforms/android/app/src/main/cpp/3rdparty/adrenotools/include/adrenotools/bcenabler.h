// SPDX-License-Identifier: BSD-2-Clause
// Copyright © 2021 Billy Laws

#pragma once

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

#include <stdint.h>

/**
 * @brief Describes the level of BCeNabler support for a driver
 */
enum adrenotools_bcn_type {
    ADRENOTOOLS_BCN_INCOMPATIBLE, //!< Driver isn't supported by BCeNabler
    ADRENOTOOLS_BCN_BLOB, //!< Driver already supports BCn textures so BCeNabler isn't necessary
    ADRENOTOOLS_BCN_PATCH //!< Driver can be patched with BCeNabler to support BCn textures
};

/**
 * @brief Checks the status of BCn support in the supplied driver
 * @param major The major part of VkPhysicalDeviceProperties::driverVersion
 * @param minor The minor part of VkPhysicalDeviceProperties::driverVersion
 * @param vendorId VkPhysicalDeviceProperties::vendorId
 */
enum adrenotools_bcn_type adrenotools_get_bcn_type(uint32_t major, uint32_t minor, uint32_t vendorId);

/**
 * @brief Patches the Adreno graphics driver to enable support for BCn compressed formats
 * @note adrenotools_get_bcn_type MUST be checked to equal ADRENOTOOLS_BCN_PATCH before calling this
 * @param vkGetPhysicalDeviceFormatPropertiesFn A pointer to vkGetPhysicalDeviceFormatProperties obtained through vkGetInstanceProcAddr. This is used to find the correct function to patch
 * @return If the patching succeeded, if false the driver will be in an undefined state
 */
bool adrenotools_patch_bcn(void *vkGetPhysicalDeviceFormatPropertiesFn);

#ifdef __cplusplus
}
#endif
