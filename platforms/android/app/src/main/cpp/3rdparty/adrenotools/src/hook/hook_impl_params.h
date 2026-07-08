// SPDX-License-Identifier: BSD-2-Clause
// Copyright © 2021 Billy Laws

#pragma once

#include <string>
#include <adrenotools/priv.h>

/**
 * @brief Holds the parameters needed for all hooks
 * @note See comments for adrenotools_open_libvulkan as a reference for member variables
 */
struct HookImplParams {
    int featureFlags;
    std::string tmpLibDir;
    std::string hookLibDir;
    std::string customDriverDir;
    std::string customDriverName;
    std::string fileRedirectDir;
    adrenotools_gpu_mapping *nextGpuMapping;

    HookImplParams(int featureFlags, const char *tmpLibDir, const char *hookLibDir, const char *customDriverDir,
                  const char *customDriverName, const char *fileRedirectDir, adrenotools_gpu_mapping *nextGpuMapping)
        : featureFlags(featureFlags),
          tmpLibDir(tmpLibDir ? tmpLibDir : ""),
          hookLibDir(hookLibDir),
          customDriverDir(customDriverDir ? customDriverDir : ""),
          customDriverName(customDriverName ? customDriverName : ""),
          fileRedirectDir(fileRedirectDir ? fileRedirectDir : ""),
          nextGpuMapping(nextGpuMapping) {}
};
