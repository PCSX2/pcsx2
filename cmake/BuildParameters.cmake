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
# FIXME do a proper detection
set(SDL2_LIBRARY "-lSDL2")
option(SDL2_API "Use SDL2 on spu2x and onepad")
option(WX28_API "Force wxWidget 2.8 lib. Default:ON" ON)

if(PACKAGE_MODE)
    if(NOT DEFINED PLUGIN_DIR)
        set(PLUGIN_DIR "${CMAKE_INSTALL_PREFIX}/lib/games/pcsx2")
    endif(NOT DEFINED PLUGIN_DIR)

    if(NOT DEFINED GAMEINDEX_DIR)
        set(GAMEINDEX_DIR "${CMAKE_INSTALL_PREFIX}/share/games/pcsx2")
    endif(NOT DEFINED GAMEINDEX_DIR)

    # Compile all source codes with these 2 defines
    add_definitions(-DPLUGIN_DIR_COMPILATION=${PLUGIN_DIR} -DGAMEINDEX_DIR_COMPILATION=${GAMEINDEX_DIR})
endif(PACKAGE_MODE)

#-------------------------------------------------------------------------------
# Compiler extra
#-------------------------------------------------------------------------------
option(USE_CLANG "Use llvm/clang to build PCSX2 (developer option)")
option(USE_ASAN "Enable address sanitizer")

#-------------------------------------------------------------------------------
# Select the architecture
#-------------------------------------------------------------------------------
option(64BIT_BUILD_DONT_WORK "Enable a x86_64 build instead of cross compiling (WARNING: NOTHING WORK)" OFF)

# Architecture bitness detection
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_ARCH_64 1)
else()
	set(_ARCH_32 1)
endif()

# Print a clear message that 64bits is not supported
if(_ARCH_64)
    message(WARNING "
    PCSX2 does not support a 64-bits environment and has no yet a plan to support it.
    It would need a complete rewrite of the core emulator and a lot of time.

    You can still run a 32-bits binary if you install all 32-bits libraries (runtime and dev).")
endif()

# 64 bits cross-compile specific configuration
if(_ARCH_64 AND 64BIT_BUILD_DONT_WORK)
    message("Compiling 64bit build on 64bit architecture")
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

    set(ARCH_FLAG "-m64 -msse -msse2")
    add_definitions(-D_ARCH_64=1 -D_M_X86=1 -D_M_X86_64=1)
    set(_ARCH_64 1)
    set(_M_X86 1)
    set(_M_X86_64 1)
else()
    message("Compiling 32bit build on 32/64bit architecture")
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

    set(ARCH_FLAG "-m32 -msse -msse2 -march=i686")
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
endif(NOT CMAKE_BUILD_TYPE MATCHES "Debug|Devel|Release")

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
# -Wstrict-aliasing: to fix one day aliasing issue
# -Wno-missing-field-initializers: standard allow to init only the begin of struct/array in static init. Just a silly warning.
# -Wno-unused-function: warn for function not used in release build
# -Wno-unused-variable: just annoying to manage different level of logging, a couple of extra var won't kill any serious compiler.
# -Wno-unused-value: lots of warning for this kind of statements "0 && ...". There are used to disable some parts of code in release/dev build.
set(DEFAULT_WARNINGS "-Wall -Wno-attributes -Wstrict-aliasing -Wno-missing-field-initializers -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -Wno-unused-value")

if (USE_CLANG AND NOT APPLE)
    # -Wno-deprecated-register: glib issue...
    set(DEFAULT_WARNINGS "${DEFAULT_WARNINGS}  -Wno-deprecated-register")
    set(COMMON_FLAG "${COMMON_FLAG} -no-integrated-as")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug|Devel")
    set(DEBUG_FLAG "-g")
else()
    set(DEBUG_FLAG "")
endif()

if (USE_ASAN)
    set(ASAN_FLAG "-fsanitize=address -fno-omit-frame-pointer -g -mpreferred-stack-boundary=4 -mincoming-stack-boundary=2 -DASAN_WORKAROUND")
else()
    set(ASAN_FLAG "")
endif()

set(DEFAULT_GCC_FLAG "${ARCH_FLAG} ${COMMON_FLAG} ${DEFAULT_WARNINGS} ${HARDENING_FLAG} ${DEBUG_FLAG} ${ASAN_FLAG}")
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
