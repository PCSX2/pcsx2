# ARMSX2 Android native build.
#
# This is a THIN entry point: it builds the single shared PCSX2 core that lives
# at the repository root (../../../../../../{common,pcsx2,3rdparty}) and layers
# the Android JNI shared library (emucore) plus Android-only dependencies
# (adrenotools) on top. The core is NO LONGER vendored under this directory --
# see REFACTOR_STATUS.md for the migration details.
#
# NOTE: This wiring is structurally correct but has NOT been compiled against a
# real Android NDK yet. Expect to iterate on include paths / 3rdparty sourcing
# (root 3rdparty vs the Android-vendored 3rdparty in this folder) via CI.

cmake_minimum_required(VERSION 3.16...3.25)

cmake_policy(SET CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

project(ARMSX2Android C CXX)

# Absolute path to the repository root (the single source of truth for the core).
get_filename_component(ARMSX2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)

# Use the Android-aware cmake modules vendored beside this entry point (copies of
# the known-good v7-clean modules: Pcsx2Utils has the ANDROID branch, BuildParameters
# cross-compiles to aarch64 + consumes the page-size arg, SearchForStuff sources the
# vendored 3rdparty instead of desktop find_package). The root cmake/ modules are
# desktop-only and FATAL on Android. Fold these back into root as a later unify pass.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Android is a non-desktop frontend: no Qt UI.
set(ENABLE_QT_UI OFF CACHE BOOL "" FORCE)

# The core's common/ and pcsx2/ CMakeLists guard on this sentinel (it is normally
# set by the desktop top-level CMakeLists.txt, which the Android build bypasses).
set(TOP_CMAKE_WAS_SOURCED TRUE)

# The core's common/ and pcsx2/ CMakeLists guard on this sentinel (it is normally
# set by the desktop top-level CMakeLists.txt, which the Android build bypasses).
set(TOP_CMAKE_WAS_SOURCED TRUE)

include(Pcsx2Utils)
check_no_parenthesis_in_path()
detect_operating_system()
detect_compiler()

include(BuildParameters)
include(SearchForStuff)

get_git_version_info()
write_svnrev_h()

# Build the shared core from the repository root. Each add_subdirectory needs an
# explicit binary dir because the source dirs live outside this tree.
# NOTE: the individual 3rdparty libs are pulled in by SearchForStuff (above),
# relative to this cpp dir's vendored 3rdparty/. Root 3rdparty/ has no aggregate
# CMakeLists, so add_subdirectory'ing it would FATAL; do not re-add it here.
add_subdirectory("${ARMSX2_ROOT}/common"   "${CMAKE_BINARY_DIR}/common")
# Header-only libretro API definitions (INTERFACE target `libretro-headers`,
# not vendored under this dir's 3rdparty/): pcsx2's Vulkan GS source list
# includes VKLibretro.cpp unconditionally (runtime-gated outside the libretro
# frontend), and it needs libretro_vulkan.h. Must exist before pcsx2 below —
# CMake silently treats an unknown target name in target_link_libraries as a
# raw linker flag, so a missing definition surfaces only as a compile error.
add_subdirectory("${ARMSX2_ROOT}/3rdparty/libretro" "${CMAKE_BINARY_DIR}/3rdparty/libretro")
add_subdirectory("${ARMSX2_ROOT}/pcsx2"    "${CMAKE_BINARY_DIR}/pcsx2")

# Android-only 3rdparty: libadrenotools (custom Vulkan ICD loading for Adreno).
# Provides the static `adrenotools` lib linked into emucore plus runtime hook
# .so's shipped in the apk's lib/<abi>/.
add_subdirectory(3rdparty/adrenotools)

# librashader: RetroArch (.slangp) shader chains for the GS post-process path, for BOTH
# the Vulkan and OpenGL(ES) renderers. Rust-built and fetched at configure time from a
# pinned commit (see the subdir's CMakeLists for why it can't be vendored). OPTIONAL:
# with no cargo installed this leaves ARMSX2_HAVE_LIBRASHADER off and the feature simply
# compiles out, so a missing Rust toolchain can never break the build.
add_subdirectory(3rdparty/librashader)

set(ARMSX2_EMUCORE_LIBRARY_NAME "emucore_4k" CACHE STRING "Android emucore shared library target/output name")

add_library(${ARMSX2_EMUCORE_LIBRARY_NAME} SHARED
	native-lib.cpp
	# Recompiler/VIF/patch unit tests are compiled out: they call the arm64/mac
	# backend test hooks (EE_Test*/mVU0_Test*) not present in this core, plus a
	# gtest scaffold. No-op stubs keep native-lib's dev-only "run tests" JNIs linkable.
	tests/android_test_stubs.cpp
)

target_link_libraries(${ARMSX2_EMUCORE_LIBRARY_NAME} PRIVATE
	PCSX2
	PCSX2_FLAGS
	common
	imgui
	libzip::zip
	SDL3::SDL3
	shaderc
	vixl
	adrenotools
	android
	log
)

target_include_directories(${ARMSX2_EMUCORE_LIBRARY_NAME} PRIVATE
	"${ARMSX2_ROOT}"
	"${ARMSX2_ROOT}/pcsx2"
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

# Only link librashader when the Rust toolchain actually produced it; a cargo-less build
# leaves the shader chain #ifdef'd out instead of failing to link.
#
# The define MUST go on PCSX2_FLAGS, not PCSX2. The shader chain lives in
# GS/Renderers/*/GSDevice*.cpp, and release builds pass -DLTO_PCSX2_CORE=ON, which compiles
# those sources into a *separate* PCSX2_LTO target (see pcsx2/CMakeLists.txt); PRIVATE
# properties on PCSX2 never reach them. PCSX2_FLAGS is the INTERFACE target both PCSX2 and
# PCSX2_LTO link, so it is the only place the define (and the header's include dir) reaches
# every core translation unit under either LTO setting.
#
# Getting this wrong costs a build to notice: the chain is behind #ifdef ARMSX2_HAS_LIBRASHADER,
# so a missed define compiles cleanly, links cleanly, and silently ships the feature stubbed
# out. Verify with `strings` on the built core, never the build log.
if(ARMSX2_HAVE_LIBRASHADER)
	target_link_libraries(PCSX2_FLAGS INTERFACE librashader)
	target_compile_definitions(PCSX2_FLAGS INTERFACE ARMSX2_HAS_LIBRASHADER=1)
	target_link_libraries(${ARMSX2_EMUCORE_LIBRARY_NAME} PRIVATE librashader)
endif()
