// SPDX-License-Identifier: BSD-2-Clause
// Copyright © 2021 Billy Laws

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// https://cs.android.com/android/platform/superproject/+/0a492a4685377d41fef2b12e9af4ebfa6feef9c2:art/libnativeloader/include/nativeloader/dlext_namespaces.h;l=25;bpv=1;bpt=1
enum {
  ANDROID_NAMESPACE_TYPE_REGULAR = 0,
  ANDROID_NAMESPACE_TYPE_ISOLATED = 1,
  ANDROID_NAMESPACE_TYPE_SHARED = 2,
  ANDROID_NAMESPACE_TYPE_EXEMPT_LIST_ENABLED = 0x08000000,
  ANDROID_NAMESPACE_TYPE_ALSO_USED_AS_ANONYMOUS = 0x10000000,
  ANDROID_NAMESPACE_TYPE_SHARED_ISOLATED = ANDROID_NAMESPACE_TYPE_SHARED | ANDROID_NAMESPACE_TYPE_ISOLATED,
};

/**
 * @brief Checks if linkernsbypass loaded successfully and is safe to use
 * @note IMPORTANT: This should be called before any calls to the rest of the library are made
 * @return true if loading succeeded
 */
bool linkernsbypass_load_status();

// https://cs.android.com/android/platform/superproject/+/0a492a4685377d41fef2b12e9af4ebfa6feef9c2:art/libnativeloader/include/nativeloader/dlext_namespaces.h;l=86;bpv=1;bpt=1
struct android_namespace_t *android_create_namespace(const char *name,
                                                     const char *ld_library_path,
                                                     const char *default_library_path,
                                                     uint64_t type,
                                                     const char *permitted_when_isolated_path,
                                                     struct android_namespace_t *parent_namespace);

struct android_namespace_t *android_create_namespace_escape(const char *name,
                                                            const char *ld_library_path,
                                                            const char *default_library_path,
                                                            uint64_t type,
                                                            const char *permitted_when_isolated_path,
                                                            struct android_namespace_t *parent_namespace);

// https://cs.android.com/android/platform/superproject/+/dcb01ef31026b3b8aeb72dada3370af63fe66bbd:bionic/linker/linker.cpp;l=3554
typedef struct android_namespace_t *(*android_get_exported_namespace_t)(const char *);
extern android_get_exported_namespace_t android_get_exported_namespace;

// https://cs.android.com/android/platform/superproject/+/dcb01ef31026b3b8aeb72dada3370af63fe66bbd:bionic/linker/linker.cpp;l=2499
typedef bool (*android_link_namespaces_all_libs_t)(struct android_namespace_t *, struct android_namespace_t *);
extern android_link_namespaces_all_libs_t android_link_namespaces_all_libs;

// https://cs.android.com/android/platform/superproject/+/dcb01ef31026b3b8aeb72dada3370af63fe66bbd:bionic/linker/linker.cpp;l=2473
typedef bool (*android_link_namespaces_t)(struct android_namespace_t *, struct android_namespace_t *, const char *);
extern android_link_namespaces_t android_link_namespaces;

/**
 * @brief Like android_link_namespaces_all_libs but links from the default namespace
 */
bool linkernsbypass_link_namespace_to_default_all_libs(struct android_namespace_t *to);

/**
 * @brief Loads a library into a namespace
 * @note IMPORTANT: If `filename` is compiled with the '-z global' linker flag and RTLD_GLOBAL is supplied in `flags` the library will be added to the namespace's LD_PRELOAD list
 * @param filename The name of the library to load
 * @param flags The rtld flags for `filename`
 * @param ns The namespace to dlopen into
 */
void *linkernsbypass_namespace_dlopen(const char *filename, int flags, struct android_namespace_t *ns);

/**
 * @brief Force loads a unique instance of a library into a namespace
 * @param libPath The path to the library to load with hooks applied
 * @param libTargetDir A temporary directory to hold the soname patched library at `libPath`, will attempt to use memfd if nullptr
 * @param flags The rtld flags for `libName`
 * @param ns The namespace to dlopen into
 */
void *linkernsbypass_namespace_dlopen_unique(const char *libPath, const char *libTargetDir, int flags, struct android_namespace_t *ns);

#ifdef __cplusplus
}
#endif
