### TODO
# Hardcode GAMEINDEX_DIR, if default is fine for everybody

### Select the build type
# Use Release/Devel/Debug      : -DCMAKE_BUILD_TYPE=Release|Devel|Debug
# Enable/disable the stripping : -DCMAKE_BUILD_STRIP=TRUE|FALSE
# generation .po based on src  : -DCMAKE_BUILD_PO=TRUE|FALSE

### GCC optimization options
# control C flags             : -DUSER_CMAKE_C_FLAGS="cflags"
# control C++ flags           : -DUSER_CMAKE_CXX_FLAGS="cxxflags"
# control link flags          : -DUSER_CMAKE_LD_FLAGS="ldflags"

### Packaging options
# Plugin installation path    : -DPLUGIN_DIR="/usr/lib/pcsx2"
# GL Shader installation path : -DGLSL_SHADER_DIR="/usr/share/games/pcsx2"
# Game DB installation path   : -DGAMEINDEX_DIR="/usr/share/games/pcsx2"
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# Graphical option
#-------------------------------------------------------------------------------
option(GLSL_API "Replace zzogl CG backend by GLSL (experimental option)")
option(EGL_API "Use EGL on zzogl (experimental/developer option)")
option(GLES_API "Use GLES on GSdx (experimental/developer option)")
option(REBUILD_SHADER "Rebuild glsl/cg shader (developer option)")
option(BUILD_REPLAY_LOADERS "Build GS replayer to ease testing (developer option)")

#-------------------------------------------------------------------------------
# Path and lib option
#-------------------------------------------------------------------------------
option(PACKAGE_MODE "Use this option to ease packaging of PCSX2 (developer/distribution option)")
option(XDG_STD "Use XDG standard path instead of the standard PCSX2 path")
option(EXTRA_PLUGINS "Build various 'extra' plugins")
option(SDL2_API "Use SDL2 on spu2x and onepad (experimental/wxWidget mustn't be built with SDL1.2 support")
option(WX28_API "Force wxWidget 2.8 lib (deprecated)")
option(GTK3_API "Use GTK3 api (experimental/wxWidget must be built with GTK3 support)")

if(PACKAGE_MODE)
    if(NOT DEFINED PLUGIN_DIR)
        set(PLUGIN_DIR "${CMAKE_INSTALL_PREFIX}/lib/games/pcsx2")
    endif()

    if(NOT DEFINED GAMEINDEX_DIR)
        set(GAMEINDEX_DIR "${CMAKE_INSTALL_PREFIX}/share/games/pcsx2")
    endif()

    if(NOT DEFINED BIN_DIR)
        set(BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin")
    endif()

    if(NOT DEFINED DOC_DIR)
        set(DOC_DIR "${CMAKE_INSTALL_PREFIX}/share/doc/pcsx2")
    endif()

    # Compile all source codes with those defines
    add_definitions(-DPLUGIN_DIR_COMPILATION=${PLUGIN_DIR} -DGAMEINDEX_DIR_COMPILATION=${GAMEINDEX_DIR} -DDOC_DIR_COMPILATION=${DOC_DIR})
endif()

#-------------------------------------------------------------------------------
# Compiler extra
#-------------------------------------------------------------------------------
option(USE_CLANG "Use llvm/clang to build PCSX2 (developer option)")
option(USE_ASAN "Enable address sanitizer")

#-------------------------------------------------------------------------------
# Select the architecture
#-------------------------------------------------------------------------------
option(64BIT_BUILD_DONT_WORK "Enable a x86_64 build instead of cross compiling (WARNING: NOTHING WORK)" OFF)
option(DISABLE_ADVANCE_SIMD "Disable advance use of SIMD (SSE2+ & AVX)" OFF)

# Architecture bitness detection
include(TargetArch)
target_architecture(PCSX2_TARGET_ARCHITECTURES)
if(${PCSX2_TARGET_ARCHITECTURES} MATCHES "x86_64")
	set(_ARCH_64 1)
	if(CMAKE_BUILD_TYPE MATCHES "Release" OR PACKAGE_MODE)
		message(FATAL_ERROR "The ${PCSX2_TARGET_ARCHITECTURES} architecture is not ready yet.")
	endif()
elseif(${PCSX2_TARGET_ARCHITECTURES} MATCHES "i386")
	set(_ARCH_32 1)
else()
	message(FATAL_ERROR "Unsupported architecture: ${PCSX2_TARGET_ARCHITECTURES}")
endif()
message(STATUS "Compiling a ${PCSX2_TARGET_ARCHITECTURES} build on a ${CMAKE_HOST_SYSTEM_PROCESSOR} host.")

# Print a clear message that 64bits is not supported
if(_ARCH_64)
    message(WARNING "
    PCSX2 does not support a 64-bits environment and has no yet a plan to support it.
    It would need a complete rewrite of the core emulator and a lot of time.

    You can still run a 32-bits binary if you install all 32-bits libraries (runtime and dev).")
endif()

# 64 bits cross-compile specific configuration
if(_ARCH_64 AND 64BIT_BUILD_DONT_WORK)
    # Search library in /usr/lib64
    SET_PROPERTY(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS ON)
    # Probably useless but it will not harm
    SET_PROPERTY(GLOBAL PROPERTY COMPILE_DEFINITIONS "-m64")

    # Note: /usr/lib64 is already taken care above

    # For Debian/ubuntu multiarch
    if(EXISTS "/usr/lib/x86_64-linux-gnu")
        set(CMAKE_LIBRARY_ARCHITECTURE "x86_64-linux-gnu")
    endif()

    # x86_64 requires -fPIC
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    if (DISABLE_ADVANCE_SIMD)
        set(ARCH_FLAG "-m64 -msse -msse2")
    else()
        #set(ARCH_FLAG "-m64 -march=native -fabi-version=6")
        set(ARCH_FLAG "-m64 -march=native")
    endif()
    add_definitions(-D_ARCH_64=1 -D_M_X86=1 -D_M_X86_64=1)
    set(_ARCH_64 1)
    set(_M_X86 1)
    set(_M_X86_64 1)
else()
    # Do not search library in /usr/lib64
    SET_PROPERTY(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS OFF)
    # Probably useless but it will not harm
    SET_PROPERTY(GLOBAL PROPERTY COMPILE_DEFINITIONS "-m32")

    # Force the search on 32-bits path.
    if(EXISTS "/usr/lib32")
        set(CMAKE_LIBRARY_ARCHITECTURE "../lib32")
    endif()
    # Debian/ubuntu drop /usr/lib32 and move /usr/lib to /usr/lib/i386-linux-gnu
    if(EXISTS "/usr/lib/i386-linux-gnu")
        set(CMAKE_LIBRARY_ARCHITECTURE "i386-linux-gnu")
    endif()

    # * -fPIC option was removed for multiple reasons.
    #     - Code only supports the x86 architecture.
    #     - code uses the ebx register so it's not compliant with PIC.
    #     - Impacts the performance too much.
    #     - Only plugins. No package will link to them.
    set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

    if (DISABLE_ADVANCE_SIMD)
        set(ARCH_FLAG "-m32 -msse -msse2 -march=i686")
    else()
        # AVX requires some fix of the ABI (mangling) (default 2)
        # Note: V6 requires GCC 4.7
        #set(ARCH_FLAG "-m32 -march=native -fabi-version=6")
        set(ARCH_FLAG "-m32 -march=native")
    endif()
    add_definitions(-D_ARCH_32=1 -D_M_X86=1 -D_M_X86_32=1)
    set(_ARCH_32 1)
    set(_M_X86 1)
    set(_M_X86_32 1)
endif()

#-------------------------------------------------------------------------------
# if no build type is set, use Devel as default
# Note without the CMAKE_BUILD_TYPE options the value is still defined to ""
# Ensure that the value set by the User is correct to avoid some bad behavior later
#-------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE MATCHES "Debug|Devel|Release")
	set(CMAKE_BUILD_TYPE Devel)
	message(STATUS "BuildType set to ${CMAKE_BUILD_TYPE} by default")
endif()

# Initially strip was disabled on release build but it is not stackstrace friendly!
# It only cost several MB so disbable it by default
option(CMAKE_BUILD_STRIP "Srip binaries to save a couple of MB (developer option)")

if(NOT DEFINED CMAKE_BUILD_PO)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_BUILD_PO TRUE)
        message(STATUS "Enable the building of po files by default in ${CMAKE_BUILD_TYPE} build !!!")
    else()
        set(CMAKE_BUILD_PO FALSE)
        message(STATUS "Disable the building of po files by default in ${CMAKE_BUILD_TYPE} build !!!")
    endif()
endif()


#-------------------------------------------------------------------------------
# Control GCC flags
#-------------------------------------------------------------------------------
### Cmake set default value for various compilation variable
### Here the list of default value for documentation purpose
# ${CMAKE_SHARED_LIBRARY_CXX_FLAGS} = "-fPIC"
# ${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS} = "-rdynamic"
#
# ${CMAKE_C_FLAGS} = "-g -O2"
# ${CMAKE_CXX_FLAGS} = "-g -O2"
# Use in debug mode
# ${CMAKE_CXX_FLAGS_DEBUG} = "-g"
# Use in release mode
# ${CMAKE_CXX_FLAGS_RELEASE} = "-O3 -DNDEBUG"

#-------------------------------------------------------------------------------
# Do not use default cmake flags
#-------------------------------------------------------------------------------
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_DEVEL "")
set(CMAKE_CXX_FLAGS_DEVEL "")
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")

#-------------------------------------------------------------------------------
# Remove bad default option
#-------------------------------------------------------------------------------
# Remove -rdynamic option that can some segmentation fault when openining pcsx2 plugins
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
if(_ARCH_32)
	# Remove -fPIC option on 32bit architectures.
	# No good reason to use it for plugins, also it impacts performance.
	set(CMAKE_SHARED_LIBRARY_C_FLAGS "")
	set(CMAKE_SHARED_LIBRARY_CXX_FLAGS "")
endif()

#-------------------------------------------------------------------------------
# Set some default compiler flags
#-------------------------------------------------------------------------------
set(COMMON_FLAG "-pipe -std=c++0x -fvisibility=hidden -pthread")
set(HARDENING_FLAG "-D_FORTIFY_SOURCE=2  -Wformat -Wformat-security")
# -Wno-attributes: "always_inline function might not be inlinable" <= real spam (thousand of warnings!!!)
# -Wno-missing-field-initializers: standard allow to init only the begin of struct/array in static init. Just a silly warning.
# -Wno-unused-function: warn for function not used in release build
# -Wno-unused-variable: just annoying to manage different level of logging, a couple of extra var won't kill any serious compiler.
# -Wno-unused-value: lots of warning for this kind of statements "0 && ...". There are used to disable some parts of code in release/dev build.
set(DEFAULT_WARNINGS "-Wall -Wno-attributes -Wno-missing-field-initializers -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -Wno-unused-value ")
# -Wstrict-aliasing=n: to fix one day aliasing issue. n=1/2/3
set(AGGRESSIVE_WARNING "-Wstrict-aliasing -Wstrict-overflow=4 ")

if (USE_CLANG)
    # -Wno-deprecated-register: glib issue...
    set(DEFAULT_WARNINGS "${DEFAULT_WARNINGS}  -Wno-deprecated-register")
    set(COMMON_FLAG "${COMMON_FLAG} -no-integrated-as")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(DEBUG_FLAG "-g")
elseif(CMAKE_BUILD_TYPE MATCHES "Devel")
    set(DEBUG_FLAG "-g -DNDEBUG")
elseif(CMAKE_BUILD_TYPE MATCHES "Release")
    set(DEBUG_FLAG "-DNDEBUG")
endif()

if (USE_ASAN)
    set(ASAN_FLAG "-fsanitize=address -fno-omit-frame-pointer -g -mpreferred-stack-boundary=4 -mincoming-stack-boundary=2 -DASAN_WORKAROUND")
else()
    set(ASAN_FLAG "")
endif()

# Note: -DGTK_DISABLE_DEPRECATED can be used to test a build without gtk deprecated feature. It could be useful to port to a newer API
set(DEFAULT_GCC_FLAG "${ARCH_FLAG} ${COMMON_FLAG} ${DEFAULT_WARNINGS} ${AGGRESSIVE_WARNING} ${HARDENING_FLAG} ${DEBUG_FLAG} ${ASAN_FLAG}")
# c++ only flags
set(DEFAULT_CPP_FLAG "${DEFAULT_GCC_FLAG} -Wno-invalid-offsetof")

#-------------------------------------------------------------------------------
# Allow user to set some default flags
# Note: string STRIP must be used to remove trailing and leading spaces.
#       See policy CMP0004
#-------------------------------------------------------------------------------
# TODO: once we completely clean all flags management, this mess could be cleaned ;)
### linker flags
if(DEFINED USER_CMAKE_LD_FLAGS)
    message(STATUS "Pcsx2 is very sensible with gcc flags, so use USER_CMAKE_LD_FLAGS at your own risk !!!")
    string(STRIP "${USER_CMAKE_LD_FLAGS}" USER_CMAKE_LD_FLAGS)
else()
    set(USER_CMAKE_LD_FLAGS "")
endif()

# ask the linker to strip the binary
if(CMAKE_BUILD_STRIP)
    string(STRIP "${USER_CMAKE_LD_FLAGS} -s" USER_CMAKE_LD_FLAGS)
endif()


### c flags
# Note CMAKE_C_FLAGS is also send to the linker.
# By default allow build on amd64 machine
if(DEFINED USER_CMAKE_C_FLAGS)
    message(STATUS "Pcsx2 is very sensible with gcc flags, so use USER_CMAKE_C_FLAGS at your own risk !!!")
    string(STRIP "${USER_CMAKE_C_FLAGS}" CMAKE_C_FLAGS)
endif()
# Use some default machine flags
string(STRIP "${CMAKE_C_FLAGS} ${DEFAULT_GCC_FLAG}" CMAKE_C_FLAGS)


### C++ flags
# Note CMAKE_CXX_FLAGS is also send to the linker.
# By default allow build on amd64 machine
if(DEFINED USER_CMAKE_CXX_FLAGS)
    message(STATUS "Pcsx2 is very sensible with gcc flags, so use USER_CMAKE_CXX_FLAGS at your own risk !!!")
    string(STRIP "${USER_CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)
endif()
# Use some default machine flags
string(STRIP "${CMAKE_CXX_FLAGS} ${DEFAULT_CPP_FLAG}" CMAKE_CXX_FLAGS)

#-------------------------------------------------------------------------------
# Too much user/packager use experimental flags as release flags
#-------------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE MATCHES "Release" OR PACKAGE_MODE)
    if (64BIT_BUILD_DONT_WORK)
        message(FATAL_ERROR "64 bit is not suitable for end users. It will only crash and it will be 10 times slower than 32 bits !!!")
    endif()
    if (GTK3_API)
        message(FATAL_ERROR "GTK3 is highly experimental besides it requires a wxWidget built with __WXGTK3__ support !!!")
    endif()
endif()
