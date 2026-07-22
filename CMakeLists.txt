# Setting it to a range tells it that it supports the features on the newer
# versions as well, avoiding setting policies.
cmake_minimum_required(VERSION 3.16...3.25)

# Enabling this cmake policy gets rid of warnings regarding LTO.
cmake_policy(SET CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "PCSX2 does not support in-tree builds.  Please make a build directory that is not the PCSX2 source directory and generate your CMake project there using either `cmake -B build_directory` or by running cmake from the build directory.")
endif()

# Project Name
project(Pcsx2 C CXX)

# Variable to check that people use the good file
set(TOP_CMAKE_WAS_SOURCED TRUE)

# set module path
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Write binaries to the bin directory under the build directory.
# This makes it simple to run development builds directly out of the build directory.
if (UNIX AND NOT APPLE)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()

# include some generic functions to ensure correctness of the env
include(Pcsx2Utils)

check_no_parenthesis_in_path()
detect_operating_system()
detect_compiler()

#-------------------------------------------------------------------------------
# Include specific module
include(BuildParameters)
include(SearchForStuff)

# Must be done after SearchForStuff
get_git_version_info()
write_svnrev_h()

# make common
add_subdirectory(common)

# make pcsx2
add_subdirectory(pcsx2)

if(ENABLE_QT_UI)
	add_subdirectory(pcsx2-qt)
endif()

# Updater is Windows only for now.
if (WIN32)
	add_subdirectory(updater)
endif()

# tests
if(ENABLE_TESTS)
	add_subdirectory(3rdparty/googletest)
	add_subdirectory(tests/ctest)
endif()

# gsrunner
if(ENABLE_GSRUNNER)
	add_subdirectory(pcsx2-gsrunner)
else()
	add_subdirectory(pcsx2-gsrunner EXCLUDE_FROM_ALL)
endif()

# eerunner — headless EE (R5900) JIT-vs-interpreter divergence localizer.
# Built on demand (`--target pcsx2-eerunner`); EXCLUDE_FROM_ALL otherwise.
if(ENABLE_EERUNNER)
	add_subdirectory(pcsx2-eerunner)
else()
	add_subdirectory(pcsx2-eerunner EXCLUDE_FROM_ALL)
endif()

# vurunner — headless VU microprogram replayer for codegen iteration. Needs
# the recompiler test hooks since it shares the harness's plumbing.
if(ENABLE_VURUNNER)
	if(NOT ENABLE_RECOMPILER_TEST_HOOKS)
		message(FATAL_ERROR "ENABLE_VURUNNER requires ENABLE_RECOMPILER_TEST_HOOKS=ON.")
	endif()
	add_subdirectory(pcsx2-vurunner)
else()
	add_subdirectory(pcsx2-vurunner EXCLUDE_FROM_ALL)
endif()

# SDL3 / kmsdrm frontend (handheld target)
if(UNIX AND NOT APPLE)
	if(ENABLE_SDL_FRONTEND)
		add_subdirectory(pcsx2-sdl)
	else()
		add_subdirectory(pcsx2-sdl EXCLUDE_FROM_ALL)
	endif()
endif()

# libretro core (armsx2_libretro.so) — opt-in only. When ENABLE_LIBRETRO is OFF
# (the default) the subdirectory is not added at all, so neither the target nor
# its install(TARGETS) rule exist. Adding it EXCLUDE_FROM_ALL instead keeps the
# target out of the default `all`, but its unconditional install() rule still
# drags the core into install-driven builds (ninja/make install, packaging) and
# "build all targets" flows (IDEs) — which is how a plain SDL build ended
# up compiling libretro. Build it with -DENABLE_LIBRETRO=ON.
if(UNIX AND NOT APPLE AND ENABLE_LIBRETRO)
	add_subdirectory(pcsx2-libretro)
endif()

#-------------------------------------------------------------------------------
if(NOT IS_SUPPORTED_COMPILER)
	message(WARNING "
*************** UNSUPPORTED CONFIGURATION ***************
You are not compiling PCSX2 with a supported compiler.
It may not even build successfully.
PCSX2 only supports the Clang and MSVC compilers.
No support will be provided, continue at your own risk.
*********************************************************")
endif()

if(ARCH_ARM64)
	message(STATUS "ARM64 build: EE, IOP, and VU0/VU1 recompilers are all available.")
endif()
