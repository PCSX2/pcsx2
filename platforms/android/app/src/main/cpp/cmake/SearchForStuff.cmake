#-------------------------------------------------------------------------------
#                       Search all libraries on the system
#-------------------------------------------------------------------------------
find_package(Git)

# Require threads on all OSes.
find_package(Threads REQUIRED)

find_package(ZLIB REQUIRED)

# --- Always-vendored deps (no system packages available or version too new) ---

# zstd — builds libzstd_static, we alias to Zstd::Zstd
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/zstd EXCLUDE_FROM_ALL)
if(NOT TARGET Zstd::Zstd)
	add_library(Zstd::Zstd ALIAS libzstd_static)
endif()

# freetype — builds 'freetype' target, we alias to Freetype::Freetype
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/freetype EXCLUDE_FROM_ALL)
if(NOT TARGET Freetype::Freetype)
	add_library(Freetype::Freetype ALIAS freetype)
endif()

# plutovg — builds plutovg::plutovg alias natively
add_subdirectory(3rdparty/plutovg EXCLUDE_FROM_ALL)

# plutosvg — builds plutosvg::plutosvg alias natively, links plutovg
add_subdirectory(3rdparty/plutosvg EXCLUDE_FROM_ALL)

# SDL3
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/sdl3 EXCLUDE_FROM_ALL)
if(NOT TARGET SDL3::SDL3)
	add_library(SDL3::SDL3 ALIAS SDL3-static)
endif()

# libjpeg-turbo — builds jpeg-static, we alias to JPEG::JPEG
set(ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(ENABLE_STATIC ON CACHE BOOL "" FORCE)
set(WITH_TURBOJPEG OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/libjpeg-turbo EXCLUDE_FROM_ALL)
# libjpeg-turbo doesn't set public include dirs; headers are in src/, config in build dir
target_include_directories(jpeg-static PUBLIC
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src>
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/3rdparty/libjpeg-turbo>
)
if(NOT TARGET JPEG::JPEG)
	add_library(JPEG::JPEG ALIAS jpeg-static)
endif()

# libpng — builds png_static, we alias to PNG::PNG
set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/libpng EXCLUDE_FROM_ALL)
if(NOT TARGET PNG::PNG)
	add_library(PNG::PNG ALIAS png_static)
endif()

# libwebp — builds webp, we alias to WebP::libwebp
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/libwebp EXCLUDE_FROM_ALL)
if(NOT TARGET WebP::libwebp)
	add_library(WebP::libwebp ALIAS webp)
endif()

# lz4
set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/lz4/build/cmake EXCLUDE_FROM_ALL)
if(NOT TARGET LZ4::LZ4)
	add_library(LZ4::LZ4 ALIAS lz4_static)
endif()

# --- Platform-specific deps ---

if(ANDROID)
	# oboe — Android audio backend
	add_subdirectory(3rdparty/oboe EXCLUDE_FROM_ALL)

	# shaderc — build from source as a static library on Android
	if(USE_VULKAN)
		set(SHADERC_SKIP_INSTALL ON CACHE BOOL "" FORCE)
		set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
		set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
		set(SHADERC_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
		set(SHADERC_SKIP_COPYRIGHT_CHECK ON CACHE BOOL "" FORCE)
		set(SHADERC_ENABLE_WERROR_COMPILE OFF CACHE BOOL "" FORCE)
		set(SHADERC_THIRD_PARTY_ROOT_DIR "${CMAKE_SOURCE_DIR}/3rdparty/shaderc/third_party" CACHE STRING "" FORCE)
		set(SHADERC_SPIRV_TOOLS_DIR "${SHADERC_THIRD_PARTY_ROOT_DIR}/SPIRV-Tools" CACHE STRING "" FORCE)
		set(SHADERC_SPIRV_HEADERS_DIR "${SHADERC_THIRD_PARTY_ROOT_DIR}/SPIRV-Headers" CACHE STRING "" FORCE)
		set(SHADERC_GLSLANG_DIR "${SHADERC_THIRD_PARTY_ROOT_DIR}/glslang" CACHE STRING "" FORCE)
		set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
		set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
		set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
		set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
		set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
		set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
		set(SPIRV_TOOLS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
		add_subdirectory("${CMAKE_SOURCE_DIR}/3rdparty/shaderc" "${CMAKE_BINARY_DIR}/3rdparty/shaderc" EXCLUDE_FROM_ALL)
		set(SHADERC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/shaderc/libshaderc/include")
	endif()

	set(FFMPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include")
elseif(UNIX AND NOT APPLE)
	# Desktop Linux deps
	find_package(CURL REQUIRED)
	find_package(PCAP REQUIRED)

	find_package(FFMPEG COMPONENTS avcodec avformat avutil swresample swscale)
	if(NOT FFMPEG_FOUND)
		message(WARNING "FFmpeg not found, using bundled headers.")
		set(FFMPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include")
	endif()

	include(CheckLib)

	find_package(Fontconfig REQUIRED)
	if(LINUX)
		check_lib(LIBUDEV libudev libudev.h)
	endif()

	if(X11_API)
		find_package(X11 REQUIRED)
		if (NOT X11_Xrandr_FOUND)
			message(FATAL_ERROR "XRandR extension is required")
		endif()
	endif()

	if(WAYLAND_API)
		find_package(ECM REQUIRED NO_MODULE)
		list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
		find_package(Wayland REQUIRED Egl)
	endif()

	if(USE_BACKTRACE)
		find_package(Libbacktrace REQUIRED)
	endif()

	find_package(PkgConfig REQUIRED)
	pkg_check_modules(DBUS REQUIRED dbus-1)

	# shaderc: dynamically loaded at runtime (install libshaderc-dev)
else()
	# Upstream desktop (Windows/macOS) — kept for reference, not used by ARMSX2
	set(FIND_FRAMEWORK_BACKUP ${CMAKE_FIND_FRAMEWORK})
	set(CMAKE_FIND_FRAMEWORK NEVER)
	find_package(Zstd 1.5.5 REQUIRED)
	find_package(Freetype 2.12 REQUIRED)
	find_package(plutovg 1.1.0 REQUIRED)
	find_package(plutosvg 0.0.7 REQUIRED)
	find_package(PNG 1.6.40 REQUIRED)
	find_package(JPEG REQUIRED)
	find_package(LZ4 REQUIRED)
	find_package(WebP REQUIRED)
	find_package(SDL3 3.2.6 REQUIRED)

	if(USE_VULKAN)
		find_package(Shaderc REQUIRED)
	endif()

	if (WIN32)
		add_subdirectory(3rdparty/D3D12MemAlloc EXCLUDE_FROM_ALL)
		add_subdirectory(3rdparty/winpixeventruntime EXCLUDE_FROM_ALL)
		add_subdirectory(3rdparty/winwil EXCLUDE_FROM_ALL)
		set(FFMPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include")
		find_package(Vtune)
	else()
		find_package(CURL REQUIRED)
		find_package(PCAP REQUIRED)
		find_package(Vtune)

		find_package(FFMPEG COMPONENTS avcodec avformat avutil swresample swscale)
		if(NOT FFMPEG_FOUND)
			message(WARNING "FFmpeg not found, using bundled headers.")
			set(FFMPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include")
		endif()

		include(CheckLib)

		if(UNIX AND NOT APPLE)
			find_package(Fontconfig REQUIRED)
			if(LINUX)
				check_lib(LIBUDEV libudev libudev.h)
			endif()

			if(X11_API)
				find_package(X11 REQUIRED)
				if (NOT X11_Xrandr_FOUND)
					message(FATAL_ERROR "XRandR extension is required")
				endif()
			endif()

			if(WAYLAND_API)
				find_package(ECM REQUIRED NO_MODULE)
				list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
				find_package(Wayland REQUIRED Egl)
			endif()

			if(USE_BACKTRACE)
				find_package(Libbacktrace REQUIRED)
			endif()

			find_package(PkgConfig REQUIRED)
			pkg_check_modules(DBUS REQUIRED dbus-1)
		endif()
	endif()

	set(CMAKE_FIND_FRAMEWORK ${FIND_FRAMEWORK_BACKUP})
endif()

add_subdirectory(3rdparty/fast_float EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/rapidyaml EXCLUDE_FROM_ALL)
# The monorepo core (common/CMakeLists.txt) links the target as ryml::ryml, but the
# vendored rapidyaml only exports it as pcsx2-rapidyaml (+ a rapidyaml::rapidyaml
# ALIAS). Alias to the REAL target -- CMake forbids an ALIAS of an ALIAS.
if(NOT TARGET ryml::ryml)
	add_library(ryml::ryml ALIAS pcsx2-rapidyaml)
endif()
add_subdirectory(3rdparty/lzma EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/libchdr EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(libchdr)
add_subdirectory(3rdparty/soundtouch EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/simpleini EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/imgui EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/cpuinfo EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(cpuinfo)
add_subdirectory(3rdparty/libzip EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/rcheevos EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/rapidjson EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/discord-rpc EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/freesurround EXCLUDE_FROM_ALL)

if(USE_OPENGL)
	add_subdirectory(3rdparty/glad EXCLUDE_FROM_ALL)
endif()

if(USE_VULKAN)
	add_subdirectory(3rdparty/vulkan EXCLUDE_FROM_ALL)
endif()

add_subdirectory(3rdparty/cubeb EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(cubeb)
disable_compiler_warnings_for_target(speex)

# Find the Qt components that we need.
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

if(WIN32)
	add_subdirectory(3rdparty/rainterface EXCLUDE_FROM_ALL)
endif()

# Demangler for the debugger.
add_subdirectory(3rdparty/demangler EXCLUDE_FROM_ALL)

# Symbol table parser.
add_subdirectory(3rdparty/ccc EXCLUDE_FROM_ALL)

# Architecture-specific.
if(ARCH_X86)
	add_subdirectory(3rdparty/zydis EXCLUDE_FROM_ALL)
elseif(ARCH_ARM64)
	add_subdirectory(3rdparty/vixl EXCLUDE_FROM_ALL)
endif()

# Prevent fmt from being built with exceptions, or being thrown at call sites.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFMT_USE_EXCEPTIONS=0 -DFMT_USE_RTTI=0")
add_subdirectory(3rdparty/fmt EXCLUDE_FROM_ALL)

# Deliberately at the end. We don't want to set the flag on third-party projects.
if(MSVC)
	# Don't warn about "deprecated" POSIX functions.
	add_definitions("-D_CRT_NONSTDC_NO_WARNINGS" "-D_CRT_SECURE_NO_WARNINGS" "-DCRT_SECURE_NO_DEPRECATE")
endif()
