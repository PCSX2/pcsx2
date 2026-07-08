# Vendored from https://github.com/bylaws/libadrenotools (BSD-2-Clause).
# The upstream project ships as three CMake subdirs (root + src/hook + lib/linkernsbypass);
# we flatten them here so they slot under the existing PCSX2 add_subdirectory(3rdparty/...).
#
# Targets:
#   linkernsbypass        STATIC  — linker namespace bypass helper (private to adrenotools).
#   adrenotools           STATIC  — driver.cpp + bcenabler.cpp + the public adrenotools_open_libvulkan API. Linked into emucore.
#   hook_impl             SHARED  — shipped in lib/<abi>/; runtime dep of the three hook shims below.
#   main_hook             SHARED  — android_dlopen_ext shim, loaded by the driver via dlopen.
#   file_redirect_hook    SHARED  — file-IO redirect shim, ditto.
#   gsl_alloc_hook        SHARED  — GSL allocator shim for user GPU-memory mapping (we don't use this feature today but ship the .so for parity with upstream).
#
# Adrenotools is hard-gated to arm64-v8a Android. Stub out cleanly on every other target so
# the existing desktop / GS-runner CMake flows still configure.

if(NOT ANDROID OR NOT (CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a"))
    message(STATUS "adrenotools: skipping (requires Android arm64-v8a)")
    add_library(adrenotools INTERFACE)
    return()
endif()

# ---- linkernsbypass (static, private) ----
add_library(linkernsbypass STATIC
    lib/linkernsbypass/android_linker_ns.cpp
    lib/linkernsbypass/android_linker_ns.h
    lib/linkernsbypass/elf_soname_patcher.cpp
    lib/linkernsbypass/elf_soname_patcher.h
)
target_compile_options(linkernsbypass PRIVATE -Wall -Wextra)
target_link_libraries(linkernsbypass PUBLIC android dl)
target_include_directories(linkernsbypass PUBLIC lib/linkernsbypass)
set_property(TARGET linkernsbypass PROPERTY POSITION_INDEPENDENT_CODE ON)

# ---- adrenotools (static, linked into emucore) ----
add_library(adrenotools STATIC
    src/bcenabler.cpp
    src/bcenabler_patch.s
    src/driver.cpp
    include/adrenotools/bcenabler.h
    include/adrenotools/driver.h
    include/adrenotools/priv.h
)
target_include_directories(adrenotools PUBLIC include)
target_include_directories(adrenotools PRIVATE .)
target_compile_options(adrenotools PRIVATE -Wall -Wextra)
target_link_libraries(adrenotools PUBLIC android PRIVATE linkernsbypass)
set_property(TARGET adrenotools PROPERTY POSITION_INDEPENDENT_CODE ON)

# ---- hook_impl (shared, runtime dep of the hook shims) ----
add_library(hook_impl SHARED
    src/hook/hook_impl.cpp
    src/hook/hook_impl.h
    src/hook/hook_impl_params.h
)
target_compile_options(hook_impl PRIVATE -Wall -Wextra)
target_include_directories(hook_impl PRIVATE include)
target_link_libraries(hook_impl PRIVATE linkernsbypass log)
set_target_properties(hook_impl PROPERTIES CXX_VISIBILITY_PRESET hidden)

# ---- main_hook, file_redirect_hook, gsl_alloc_hook (shared, dlopen'd by driver) ----
foreach(_hook main_hook file_redirect_hook gsl_alloc_hook)
    add_library(${_hook} SHARED src/hook/${_hook}.c)
    target_compile_options(${_hook} PRIVATE -Wall -Wextra)
    target_link_options(${_hook} PRIVATE "-Wl,-z,global")
    target_include_directories(${_hook} PRIVATE include src/hook)
    target_link_libraries(${_hook} PRIVATE hook_impl)
    set_target_properties(${_hook} PROPERTIES CXX_VISIBILITY_PRESET hidden)
endforeach()
