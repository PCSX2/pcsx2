#-------------------------------------------------------------------------------
#                       Search all libraries on the system
#-------------------------------------------------------------------------------
# iOS has no system package manager, so EVERY dependency is built from vendored
# sources via add_subdirectory -- there are no find_package() calls for the core
# deps (unlike the desktop root SearchForStuff.cmake).
#
# DEP SOURCING: the iOS core builds deps from two vendored trees:
#   * ${ARMSX2_ROOT}/3rdparty/  -- the monorepo's canonical copy, used for every
#     dep that lives there (plutovg, plutosvg, fmt, fast_float, libchdr,
#     soundtouch, simpleini, imgui, cpuinfo, libzip, rcheevos, rapidjson,
#     discord-rpc, freesurround, demangler, ccc, vixl, cubeb, ...).
#   * ${ARMSX2_ANDROID_VENDORED}/  -- the Android app's vendored copy under
#     platforms/android/app/src/main/cpp/3rdparty/, used ONLY for the deps that
#     are not in the monorepo root (zstd, freetype, SDL3, libjpeg-turbo, libpng,
#     libwebp, lz4, rapidyaml). These are shared verbatim with Android so there
#     is a single source of truth; reaching across to the Android tree is the
#     lesser evil vs. duplicating security-sensitive libs under iOS. A future
#     unify pass should fold these into the root 3rdparty/.
#
# iOS is METAL-ONLY: no OpenGL, no Vulkan, no shaderc (unlike Android, which
# needs Vulkan + shaderc). There is also no oboe (Android audio) -- iOS uses
# CoreAudio via cubeb. FFMPEG is headers-only (dynamically loaded at runtime).
#-------------------------------------------------------------------------------
find_package(Git)

# Require threads on all OSes.
find_package(Threads REQUIRED)

# zlib: iOS/macOS ship zlib in the SDK, so a system find works. (Neither the
# monorepo root nor the Android tree vendors zlib.)
find_package(ZLIB REQUIRED)

# Path to the Android app's vendored 3rdparty tree (shared dep source).
get_filename_component(ARMSX2_ANDROID_VENDORED
	"${ARMSX2_ROOT}/platforms/android/app/src/main/cpp/3rdparty" ABSOLUTE)

# --- Always-vendored deps (no system packages available) ---
# These 9 live ONLY in the Android-vendored tree (not in the monorepo root), so
# they are sourced from there. Aliases match what the core links (common +
# pcsx2 CMakeLists).

# zstd -- builds libzstd_static, we alias to Zstd::Zstd
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory("${ARMSX2_ANDROID_VENDORED}/zstd" "${CMAKE_BINARY_DIR}/3rdparty/zstd" EXCLUDE_FROM_ALL)
if(NOT TARGET Zstd::Zstd)
	add_library(Zstd::Zstd ALIAS libzstd_static)
endif()

# freetype -- builds 'freetype' target, we alias to Freetype::Freetype
# (Disable every optional freetype backend: iOS provides none of them and we do
#  not want freetype pulling in zlib/png/bzip2/brotli/harfbuzz as build deps.)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
add_subdirectory("${ARMSX2_ANDROID_VENDORED}/freetype" "${CMAKE_BINARY_DIR}/3rdparty/freetype" EXCLUDE_FROM_ALL)
if(NOT TARGET Freetype::Freetype)
	add_library(Freetype::Freetype ALIAS freetype)
endif()

# plutovg -- builds plutovg::plutovg alias natively (sourced from root)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/plutovg" "${CMAKE_BINARY_DIR}/3rdparty/plutovg" EXCLUDE_FROM_ALL)

# plutosvg -- builds plutosvg::plutosvg alias natively, links plutovg (root)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/plutosvg" "${CMAKE_BINARY_DIR}/3rdparty/plutosvg" EXCLUDE_FROM_ALL)

# SDL3 -- builds SDL3-static, we alias to SDL3::SDL3 (Android-vendored copy)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${ARMSX2_ANDROID_VENDORED}/sdl3" "${CMAKE_BINARY_DIR}/3rdparty/sdl3" EXCLUDE_FROM_ALL)
if(NOT TARGET SDL3::SDL3)
	add_library(SDL3::SDL3 ALIAS SDL3-static)
endif()

# libjpeg-turbo -- builds jpeg-static, we alias to JPEG::JPEG (Android-vendored)
set(ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(ENABLE_STATIC ON CACHE BOOL "" FORCE)
set(WITH_TURBOJPEG OFF CACHE BOOL "" FORCE)
add_subdirectory("${ARMSX2_ANDROID_VENDORED}/libjpeg-turbo" "${CMAKE_BINARY_DIR}/3rdparty/libjpeg-turbo" EXCLUDE_FROM_ALL)
# libjpeg-turbo doesn't set public include dirs; headers are in src/, config in build dir
target_include_directories(jpeg-static PUBLIC
	$<BUILD_INTERFACE:${ARMSX2_ANDROID_VENDORED}/libjpeg-turbo/src>
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/3rdparty/libjpeg-turbo>
)
if(NOT TARGET JPEG::JPEG)
	add_library(JPEG::JPEG ALIAS jpeg-static)
endif()

# libpng -- builds png_static, we alias to PNG::PNG (Android-vendored)
set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory("${ARMSX2_ANDROID_VENDORED}/libpng" "${CMAKE_BINARY_DIR}/3rdparty/libpng" EXCLUDE_FROM_ALL)
if(NOT TARGET PNG::PNG)
	add_library(PNG::PNG ALIAS png_static)
endif()

# libwebp -- builds webp, we alias to WebP::libwebp (Android-vendored)
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
add_subdirectory("${ARMSX2_ANDROID_VENDORED}/libwebp" "${CMAKE_BINARY_DIR}/3rdparty/libwebp" EXCLUDE_FROM_ALL)
if(NOT TARGET WebP::libwebp)
	add_library(WebP::libwebp ALIAS webp)
endif()

# lz4 (Android-vendored; CMake lives under build/cmake).
# LZ4_BUILD_* are lz4-specific and safe to FORCE-set. BUILD_SHARED_LIBS /
# BUILD_STATIC_LIBS are generic globals — use normal (non-CACHE) variables so
# they only affect this subdirectory scope and don't leak into cubeb/fmt/etc.
set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)
add_subdirectory("${ARMSX2_ANDROID_VENDORED}/lz4/build/cmake" "${CMAKE_BINARY_DIR}/3rdparty/lz4" EXCLUDE_FROM_ALL)
if(NOT TARGET LZ4::LZ4)
	add_library(LZ4::LZ4 ALIAS lz4_static)
endif()

# --- Core deps that live in the monorepo root ---

add_subdirectory("${ARMSX2_ROOT}/3rdparty/fast_float" "${CMAKE_BINARY_DIR}/3rdparty/fast_float" EXCLUDE_FROM_ALL)
add_subdirectory("${ARMSX2_ANDROID_VENDORED}/rapidyaml" "${CMAKE_BINARY_DIR}/3rdparty/rapidyaml" EXCLUDE_FROM_ALL)
# The monorepo core (common/CMakeLists.txt) links the target as ryml::ryml, but the
# vendored rapidyaml only exports it as pcsx2-rapidyaml (+ a rapidyaml::rapidyaml
# ALIAS). Alias to the REAL target -- CMake forbids an ALIAS of an ALIAS.
if(NOT TARGET ryml::ryml)
	add_library(ryml::ryml ALIAS pcsx2-rapidyaml)
endif()
add_subdirectory("${ARMSX2_ROOT}/3rdparty/lzma" "${CMAKE_BINARY_DIR}/3rdparty/lzma" EXCLUDE_FROM_ALL)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/libchdr" "${CMAKE_BINARY_DIR}/3rdparty/libchdr" EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(libchdr)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/soundtouch" "${CMAKE_BINARY_DIR}/3rdparty/soundtouch" EXCLUDE_FROM_ALL)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/simpleini" "${CMAKE_BINARY_DIR}/3rdparty/simpleini" EXCLUDE_FROM_ALL)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/imgui" "${CMAKE_BINARY_DIR}/3rdparty/imgui" EXCLUDE_FROM_ALL)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/cpuinfo" "${CMAKE_BINARY_DIR}/3rdparty/cpuinfo" EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(cpuinfo)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/libzip" "${CMAKE_BINARY_DIR}/3rdparty/libzip" EXCLUDE_FROM_ALL)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/rcheevos" "${CMAKE_BINARY_DIR}/3rdparty/rcheevos" EXCLUDE_FROM_ALL)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/rapidjson" "${CMAKE_BINARY_DIR}/3rdparty/rapidjson" EXCLUDE_FROM_ALL)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/discord-rpc" "${CMAKE_BINARY_DIR}/3rdparty/discord-rpc" EXCLUDE_FROM_ALL)
add_subdirectory("${ARMSX2_ROOT}/3rdparty/freesurround" "${CMAKE_BINARY_DIR}/3rdparty/freesurround" EXCLUDE_FROM_ALL)

# iOS is Metal-only: OpenGL and Vulkan are force-OFF by the top-level iOS
# CMakeLists.txt, so glad/vulkan/shaderc are NOT built here (unlike Android).

add_subdirectory("${ARMSX2_ROOT}/3rdparty/cubeb" "${CMAKE_BINARY_DIR}/3rdparty/cubeb" EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(cubeb)
disable_compiler_warnings_for_target(speex)

# Find the Qt components that we need. (Not used on iOS, but declared for parity
# with the core's option surface -- ENABLE_QT_UI is OFF on iOS.)
if(ENABLE_QT_UI)
	find_package(Qt6 6.10.1 COMPONENTS CoreTools Core GuiTools Gui WidgetsTools Widgets LinguistTools REQUIRED)

	if(NOT WIN32 AND NOT APPLE)
		if (Qt6_VERSION VERSION_GREATER_EQUAL 6.10.0)
			find_package(Qt6 COMPONENTS CorePrivate GuiPrivate WidgetsPrivate REQUIRED)
		endif()
	endif()

	# The docking system for the debugger.
	find_package(KDDockWidgets-qt6 2.3.0 REQUIRED)
endif()

# Demangler for the debugger.
add_subdirectory("${ARMSX2_ROOT}/3rdparty/demangler" "${CMAKE_BINARY_DIR}/3rdparty/demangler" EXCLUDE_FROM_ALL)

# Symbol table parser.
add_subdirectory("${ARMSX2_ROOT}/3rdparty/ccc" "${CMAKE_BINARY_DIR}/3rdparty/ccc" EXCLUDE_FROM_ALL)

# Architecture-specific. iOS is ARM64.
if(ARCH_X86)
	add_subdirectory("${ARMSX2_ROOT}/3rdparty/zydis" "${CMAKE_BINARY_DIR}/3rdparty/zydis" EXCLUDE_FROM_ALL)
elseif(ARCH_ARM64)
	add_subdirectory("${ARMSX2_ROOT}/3rdparty/vixl" "${CMAKE_BINARY_DIR}/3rdparty/vixl" EXCLUDE_FROM_ALL)
endif()

# Prevent fmt from being built with exceptions, or being thrown at call sites.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFMT_USE_EXCEPTIONS=0 -DFMT_USE_RTTI=0")
add_subdirectory("${ARMSX2_ROOT}/3rdparty/fmt" "${CMAKE_BINARY_DIR}/3rdparty/fmt" EXCLUDE_FROM_ALL)

# FFMPEG: headers-only on iOS (dynamically loaded at runtime via dlopen, like
# Android). The bundled headers live in the monorepo root 3rdparty/ffmpeg/include.
set(FFMPEG_INCLUDE_DIRS "${ARMSX2_ROOT}/3rdparty/ffmpeg/include")

# Deliberately at the end. We don't want to set the flag on third-party projects.
if(MSVC)
	# Don't warn about "deprecated" POSIX functions.
	add_definitions("-D_CRT_NONSTDC_NO_WARNINGS" "-D_CRT_SECURE_NO_WARNINGS" "-DCRT_SECURE_NO_DEPRECATE")
endif()
