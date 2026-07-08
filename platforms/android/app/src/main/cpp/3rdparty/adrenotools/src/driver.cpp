// SPDX-License-Identifier: BSD-2-Clause
// Copyright © 2021 Billy Laws

#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <android/api-level.h>
#include <android/log.h>
#include <android_linker_ns.h>
#include "hook/kgsl.h"
#include "hook/hook_impl_params.h"
#include <adrenotools/driver.h>
#include <unistd.h>

void *adrenotools_open_libvulkan(int dlopenFlags, int featureFlags, const char *tmpLibDir, const char *hookLibDir, const char *customDriverDir, const char *customDriverName, const char *fileRedirectDir, void **userMappingHandle) {
    // Bail out if linkernsbypass failed to load, this probably means we're on api < 28
    if (!linkernsbypass_load_status())
        return nullptr;

    // Always use memfd on Q+ since it's guaranteed to work
    if (android_get_device_api_level() >= 29)
        tmpLibDir = nullptr;

    // Verify that params for specific features are only passed if they are enabled
    if (!(featureFlags & ADRENOTOOLS_DRIVER_FILE_REDIRECT) && fileRedirectDir)
        return nullptr;

    if (!(featureFlags & ADRENOTOOLS_DRIVER_CUSTOM) && (customDriverDir || customDriverName))
        return nullptr;

    if (!(featureFlags & ADRENOTOOLS_DRIVER_GPU_MAPPING_IMPORT) && userMappingHandle)
        return nullptr;

    // Verify that params for enabled features are correct
    struct stat buf{};

    if (featureFlags & ADRENOTOOLS_DRIVER_CUSTOM) {
        if (!customDriverName || !customDriverDir)
            return nullptr;

        if (stat((std::string(customDriverDir) + customDriverName).c_str(), &buf) != 0)
            return nullptr;
    }

    // Verify that params for enabled features are correct
    if (featureFlags & ADRENOTOOLS_DRIVER_FILE_REDIRECT) {
        if (!fileRedirectDir)
            return nullptr;

        if (stat(fileRedirectDir, &buf) != 0)
            return nullptr;
    }

    // Create a namespace that can isolate our hook from the classloader namespace
    auto hookNs{android_create_namespace("adrenotools-libvulkan", hookLibDir, nullptr, ANDROID_NAMESPACE_TYPE_SHARED, nullptr, nullptr)};

    // Link it to the default namespace so the hook can use libandroid etc
    if (!linkernsbypass_link_namespace_to_default_all_libs(hookNs))
        return nullptr;

    // Preload the hook implementation, otherwise we get a weird issue where despite being in NEEDED of the hook lib the hook's symbols will overwrite ours and cause an infinite loop
    auto hookImpl{linkernsbypass_namespace_dlopen("libhook_impl.so", RTLD_NOW, hookNs)};
    if (!hookImpl)
        return nullptr;

    // Pass parameters to the hook implementation
    auto initHookParam{reinterpret_cast<void (*)(const void *)>(dlsym(hookImpl, "init_hook_param"))};
    if (!initHookParam)
        return nullptr;


    auto importMapping{[&]() -> adrenotools_gpu_mapping * {
        if (featureFlags & ADRENOTOOLS_DRIVER_GPU_MAPPING_IMPORT) {
            // This will be leaked, but it's not a big deal since it's only a few bytes
            adrenotools_gpu_mapping *mapping{new adrenotools_gpu_mapping{}};
            *userMappingHandle = mapping;
            return mapping;
        } else {
            return nullptr;
        }
    }()};

    initHookParam(new HookImplParams(featureFlags, tmpLibDir, hookLibDir, customDriverDir, customDriverName, fileRedirectDir, importMapping));

    // Load the libvulkan hook into the isolated namespace
    if (!linkernsbypass_namespace_dlopen("libmain_hook.so", RTLD_GLOBAL, hookNs))
        return nullptr;

    return linkernsbypass_namespace_dlopen_unique("/system/lib64/libvulkan.so", tmpLibDir, dlopenFlags, hookNs);
}

bool adrenotools_import_user_mem(void *handle, void *hostPtr, uint64_t size) {
    auto importMapping{reinterpret_cast<adrenotools_gpu_mapping *>(handle)};

    kgsl_gpuobj_import_useraddr addr{
        .virtaddr = reinterpret_cast<uint64_t>(hostPtr),
    };

    kgsl_gpuobj_import userMemImport{
        .priv = reinterpret_cast<uint64_t>(&addr),
        .priv_len = size,
        .flags = KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT | KGSL_MEMFLAGS_IOCOHERENT,
        .type = KGSL_USER_MEM_TYPE_ADDR,

    };

    kgsl_gpuobj_info info{};

    int kgslFd{open("/dev/kgsl-3d0", O_RDWR)};
    if (kgslFd < 0)
        return false;

    int ret{ioctl(kgslFd, IOCTL_KGSL_GPUOBJ_IMPORT, &userMemImport)};
    if (ret)
        goto err;

    info.id = userMemImport.id;
    ret = ioctl(kgslFd, IOCTL_KGSL_GPUOBJ_INFO, &info);
    if (ret)
        goto err;

    importMapping->host_ptr = hostPtr;
    importMapping->gpu_addr = info.gpuaddr;
    importMapping->size = size;
    importMapping->flags = 0xc2600; //!< Unknown flags, but they are required for the mapping to work

    close(kgslFd);
    return true;

err:
    close(kgslFd);
    return false;
}

bool adrenotools_mem_gpu_allocate(void *handle, uint64_t *size) {
    auto mapping{reinterpret_cast<adrenotools_gpu_mapping *>(handle)};

    kgsl_gpuobj_alloc gpuobjAlloc{
        .size = *size,
        .flags = KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT | KGSL_MEMFLAGS_IOCOHERENT,
    };

    kgsl_gpuobj_info info{};

    int kgslFd{open("/dev/kgsl-3d0", O_RDWR)};
    if (kgslFd < 0)
        return false;

    int ret{ioctl(kgslFd, IOCTL_KGSL_GPUOBJ_ALLOC, &gpuobjAlloc)};
    if (ret)
        goto err;

    *size = gpuobjAlloc.mmapsize;

    info.id = gpuobjAlloc.id;

    ret = ioctl(kgslFd, IOCTL_KGSL_GPUOBJ_INFO, &info);
    if (ret)
        goto err;

    mapping->host_ptr = nullptr;
    mapping->gpu_addr = info.gpuaddr;
    mapping->size = *size;
    mapping->flags = 0xc2600; //!< Unknown flags, but they are required for the mapping to work

    close(kgslFd);
    return true;

err:
    close(kgslFd);
    return false;
}


bool adrenotools_mem_cpu_map(void *handle, void *hostPtr, uint64_t size) {
    auto mapping{reinterpret_cast<adrenotools_gpu_mapping *>(handle)};

    int kgslFd{open("/dev/kgsl-3d0", O_RDWR)};
    if (kgslFd < 0)
        return false;

    mapping->host_ptr = mmap(hostPtr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, kgslFd, mapping->gpu_addr);
    close(kgslFd);
    return mapping->host_ptr != nullptr;
}

bool adrenotools_validate_gpu_mapping(void *handle) {
    auto importMapping{reinterpret_cast<adrenotools_gpu_mapping *>(handle)};
    return importMapping->gpu_addr == ADRENOTOOLS_GPU_MAPPING_SUCCEEDED_MAGIC;
}

void adrenotools_set_turbo(bool turbo) {
    uint32_t enable{turbo ? 0U : 1U};
    kgsl_device_getproperty prop{
        .type = KGSL_PROP_PWRCTRL,
        .value = reinterpret_cast<void *>(&enable),
        .sizebytes = sizeof(enable),
    };

    int kgslFd{open("/dev/kgsl-3d0", O_RDWR)};
    if (kgslFd < 0)
        return;

    ioctl(kgslFd, IOCTL_KGSL_SETPROPERTY, &prop);
    close (kgslFd);
}
