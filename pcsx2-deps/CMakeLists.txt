cmake_minimum_required(VERSION 3.16)

project(pcsx2-deps C CXX)

include(ExternalProject)
include(ProcessorCount)

ProcessorCount(NPROCS)

if(NOT NPROCS OR NPROCS EQUAL 0)
	set(NPROCS 1)
endif()

set(DEPS_INSTALL_DIR
	"${CMAKE_INSTALL_PREFIX}"
	CACHE PATH "Dependency installation directory"
)

if(BUILD_DEPENDENCIES_AS_SHARED_LIBS)
	set(BUILD_DEPENDENCIES_AS_STATIC_LIBS OFF)
else()
	set(BUILD_DEPENDENCIES_AS_STATIC_LIBS ON)
endif()

# ---------------------------------------------------------------------------
# Versions
# ---------------------------------------------------------------------------

set(FREETYPE 2.14.1)
set(HARFBUZZ 12.2.0)
set(KDDOCKWIDGETS 2.4.1)
set(LIBBACKTRACE ad106d5fdd5d960bd33fae1c48a351af567fd075)
set(LIBPNG 1.6.51)
set(LIBWEBP 1.6.0)
set(MOLTENVK 1.4.1)
set(SDL SDL3-3.2.26)
set(LZ4 1.10.0)
set(ZSTD 1.5.7)
set(PLUTOVG 1.3.2)
set(PLUTOSVG 0.0.7)
set(RAPIDYAML 0.12.1)

set(SHADERC v2025.4)
set(SHADERC_GLSLANG 7a47e2531cb334982b2a2dd8513dca0a3de4373d)
set(SHADERC_SPIRVHEADERS b824a462d4256d720bebb40e78b9eb8f78bbb305)
set(SHADERC_SPIRVTOOLS 971a7b6e8d7740035bbff089bbbf9f42951ecfd5)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

set(DEPS_INSTALL_DIR
	"${CMAKE_BINARY_DIR}/deps"
	CACHE PATH "PCSX2 dependency installation directory"
)

set(DEPS_DOWNLOAD_DIR
	"${CMAKE_BINARY_DIR}/build-deps/downloads"
)

file(MAKE_DIRECTORY
	"${DEPS_INSTALL_DIR}"
	"${DEPS_DOWNLOAD_DIR}"
)

set(COMMON_ARGS
	-DCMAKE_BUILD_TYPE=Release
	-DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}
	-DCMAKE_PREFIX_PATH=${DEPS_INSTALL_DIR}
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON
	-DBUILD_SHARED_LIBS=${BUILD_DEPENDENCIES_AS_SHARED_LIBS}
)

# Pass the parent's toolchain/cross compiler through.
foreach(_var
	CMAKE_TOOLCHAIN_FILE
	CMAKE_C_COMPILER
	CMAKE_CXX_COMPILER
	CMAKE_C_COMPILER_LAUNCHER
	CMAKE_CXX_COMPILER_LAUNCHER
	CMAKE_SYSROOT
	CMAKE_OSX_DEPLOYMENT_TARGET
	CMAKE_OSX_ARCHITECTURES
	ANDROID_ABI
	ANDROID_PLATFORM
)
	if(DEFINED ${_var})
		list(APPEND COMMON_ARGS
			-D${_var}=${${_var}}
		)
	endif()
endforeach()

# Avoid CMake 4.x CMP0135 warning.
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

# ---------------------------------------------------------------------------
# libbacktrace
# ---------------------------------------------------------------------------

if(UNIX AND NOT APPLE)

	if(CMAKE_CROSSCOMPILING)
		if(CMAKE_C_COMPILER_TARGET)
			set(LIBBACKTRACE_HOST
				"--host=${CMAKE_C_COMPILER_TARGET}"
			)
		elseif(CMAKE_C_COMPILER)
			execute_process(
				COMMAND ${CMAKE_C_COMPILER} -dumpmachine
				OUTPUT_VARIABLE LIBBACKTRACE_TARGET
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)

			set(LIBBACKTRACE_HOST
				"--host=${LIBBACKTRACE_TARGET}"
			)
		endif()
	endif()

	ExternalProject_Add(libbacktrace
		URL
			https://github.com/ianlancetaylor/libbacktrace/archive/${LIBBACKTRACE}.tar.gz

		DOWNLOAD_DIR
			${DEPS_DOWNLOAD_DIR}

		BUILD_IN_SOURCE ON

		CONFIGURE_COMMAND
			<SOURCE_DIR>/configure
			--prefix=${DEPS_INSTALL_DIR}
			--with-pic
			${LIBBACKTRACE_HOST}

		BUILD_COMMAND
			make -j${NPROCS}

		INSTALL_COMMAND
			make install

		DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	)

endif()

# ---------------------------------------------------------------------------
# libpng
# ---------------------------------------------------------------------------

ExternalProject_Add(libpng
	URL
		https://downloads.sourceforge.net/project/libpng/libpng16/${LIBPNG}/libpng-${LIBPNG}.tar.xz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	CMAKE_ARGS
		${COMMON_ARGS}
		-DPNG_TESTS=OFF
		-DPNG_STATIC=${BUILD_DEPENDENCIES_AS_STATIC_LIBS}
		-DPNG_SHARED=${BUILD_DEPENDENCIES_AS_SHARED_LIBS}
		-DPNG_TOOLS=OFF

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# LZ4
# ---------------------------------------------------------------------------

ExternalProject_Add(lz4
	URL
		https://github.com/lz4/lz4/releases/download/v${LZ4}/lz4-${LZ4}.tar.gz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	SOURCE_SUBDIR
		build/cmake

	CMAKE_ARGS
		${COMMON_ARGS}
		-DLZ4_BUILD_CLI=OFF
		-DLZ4_BUILD_LEGACY_LZ4C=OFF

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# Zstandard
# ---------------------------------------------------------------------------

ExternalProject_Add(zstd
	URL
		https://github.com/facebook/zstd/releases/download/v${ZSTD}/zstd-${ZSTD}.tar.gz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	SOURCE_SUBDIR
		build/cmake

	CMAKE_ARGS
		${COMMON_ARGS}
		-DZSTD_BUILD_SHARED=${BUILD_DEPENDENCIES_AS_SHARED_LIBS}
		-DZSTD_BUILD_STATIC=${BUILD_DEPENDENCIES_AS_STATIC_LIBS}
		-DZSTD_BUILD_PROGRAMS=OFF

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# FreeType WITHOUT HarfBuzz
# ---------------------------------------------------------------------------

ExternalProject_Add(freetype_no_harfbuzz
	URL
		https://sourceforge.net/projects/freetype/files/freetype2/${FREETYPE}/freetype-${FREETYPE}.tar.xz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	CMAKE_ARGS
		${COMMON_ARGS}
		-DFT_REQUIRE_ZLIB=ON
		-DFT_REQUIRE_PNG=ON
		-DFT_DISABLE_BZIP2=TRUE
		-DFT_DISABLE_BROTLI=TRUE
		-DFT_DISABLE_HARFBUZZ=TRUE

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# HarfBuzz
# ---------------------------------------------------------------------------

ExternalProject_Add(harfbuzz
	DEPENDS freetype_no_harfbuzz

	URL
		https://github.com/harfbuzz/harfbuzz/archive/${HARFBUZZ}/harfbuzz-${HARFBUZZ}.tar.gz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	CMAKE_ARGS
		${COMMON_ARGS}
		-DHB_BUILD_UTILS=OFF
		-DHB_HAVE_FREETYPE=ON

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# WebP
# ---------------------------------------------------------------------------

ExternalProject_Add(webp
	URL
		https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${LIBWEBP}.tar.gz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	CMAKE_ARGS
		${COMMON_ARGS}
		-DWEBP_BUILD_ANIM_UTILS=OFF
		-DWEBP_BUILD_CWEBP=OFF
		-DWEBP_BUILD_DWEBP=OFF
		-DWEBP_BUILD_GIF2WEBP=OFF
		-DWEBP_BUILD_IMG2WEBP=OFF
		-DWEBP_BUILD_VWEBP=OFF
		-DWEBP_BUILD_WEBPINFO=OFF
		-DWEBP_BUILD_WEBPMUX=OFF
		-DWEBP_BUILD_EXTRAS=OFF

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# FreeType WITH HarfBuzz
# ---------------------------------------------------------------------------

ExternalProject_Add(freetype
	DEPENDS harfbuzz

	URL
		https://sourceforge.net/projects/freetype/files/freetype2/${FREETYPE}/freetype-${FREETYPE}.tar.xz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	CMAKE_ARGS
		${COMMON_ARGS}
		-DFT_REQUIRE_ZLIB=ON
		-DFT_REQUIRE_PNG=ON
		-DFT_DISABLE_BZIP2=TRUE
		-DFT_DISABLE_BROTLI=TRUE
		-DFT_REQUIRE_HARFBUZZ=TRUE

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------

ExternalProject_Add(SDL
	URL
		https://libsdl.org/release/${SDL}.tar.gz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	CMAKE_ARGS
		${COMMON_ARGS}
		-DSDL_SHARED=${BUILD_DEPENDENCIES_AS_SHARED_LIBS}
		-DSDL_STATIC=${BUILD_DEPENDENCIES_AS_STATIC_LIBS}
		-DSDL_VIDEO=OFF
		-DSDL_POWER=OFF
		-DSDL_SENSOR=OFF
		-DSDL_DIALOG=OFF
		-DSDL_TRAY=OFF
		-DSDL_TEST_LIBRARY=OFF
		-DSDL_UNIX_CONSOLE_BUILD=ON

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# PlutoVG
# ---------------------------------------------------------------------------

ExternalProject_Add(plutovg
	URL
		https://github.com/sammycage/plutovg/archive/v${PLUTOVG}/plutovg-${PLUTOVG}.tar.gz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	CMAKE_ARGS
		${COMMON_ARGS}
		-DPLUTOVG_BUILD_EXAMPLES=OFF

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# PlutoSVG
# ---------------------------------------------------------------------------

ExternalProject_Add(plutosvg
	DEPENDS
		freetype
		plutovg

	URL
		https://github.com/sammycage/plutosvg/archive/v${PLUTOSVG}/plutosvg-${PLUTOSVG}.tar.gz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	CMAKE_ARGS
		${COMMON_ARGS}
		-DPLUTOSVG_ENABLE_FREETYPE=ON
		-DPLUTOSVG_BUILD_EXAMPLES=OFF

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# RapidYAML
# ---------------------------------------------------------------------------

ExternalProject_Add(rapidyaml
	URL
		https://github.com/biojppm/rapidyaml/releases/download/v${RAPIDYAML}/rapidyaml-${RAPIDYAML}-src.tgz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	CMAKE_ARGS
		${COMMON_ARGS}

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# KDDockWidgets
# ---------------------------------------------------------------------------

ExternalProject_Add(kddockwidgets
	URL
		https://github.com/KDAB/KDDockWidgets/archive/v${KDDOCKWIDGETS}/KDDockWidgets-${KDDOCKWIDGETS}.tar.gz

	DOWNLOAD_DIR
		${DEPS_DOWNLOAD_DIR}

	CMAKE_ARGS
		${COMMON_ARGS}
		-DKDDockWidgets_QT6=ON
		-DKDDockWidgets_EXAMPLES=OFF
		-DKDDockWidgets_FRONTENDS=qtwidgets

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--target install

	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---------------------------------------------------------------------------
# DirectX-Headers
# ---------------------------------------------------------------------------

if(WIN32)
	ExternalProject_Add(directx-headers
		GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers
		GIT_TAG ${DXHEADERS}
		${SHALLOW_CLONE}
		CMAKE_ARGS ${COMMON_ARGS} -DDXHEADERS_BUILD_TEST=OFF
			-DDXHEADERS_BUILD_GOOGLE_TEST=OFF -DDXHEADERS_INSTALL=ON
		BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --parallel ${NPROCS}
	)
endif()

# ---------------------------------------------------------------------------
# Shaderc
# ---------------------------------------------------------------------------

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(SHADERC_ARGS
	-DSHADERC_SKIP_TESTS=ON
	-DSHADERC_SKIP_EXAMPLES=ON
	-DSHADERC_SKIP_COPYRIGHT_CHECK=ON
	-DPython3_EXECUTABLE=${Python3_EXECUTABLE}
	-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}
)

if(MSVC)
	list(APPEND SHADERC_ARGS -DSHADERC_ENABLE_SHARED_CRT=OFF)
	set(SHADERC_COMBINED shaderc_combined.lib)
else()
	set(SHADERC_COMBINED libshaderc_combined.a)
endif()

# ---------------------------------------------------------------------------
# Shaderc
# ---------------------------------------------------------------------------

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(SHADERC_ARGS
	-DSHADERC_SKIP_TESTS=ON
	-DSHADERC_SKIP_EXAMPLES=ON
	-DSHADERC_SKIP_COPYRIGHT_CHECK=ON
	-DPython3_EXECUTABLE=${Python3_EXECUTABLE}
	-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}
)

if(MSVC)
	list(APPEND SHADERC_ARGS -DSHADERC_ENABLE_SHARED_CRT=OFF)
	set(SHADERC_COMBINED shaderc_combined.lib)
else()
	set(SHADERC_COMBINED libshaderc_combined.a)
endif()

# ---------------------------------------------------------------------------
# Shaderc
# ---------------------------------------------------------------------------

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(SHADERC_ARGS
	-DSHADERC_SKIP_TESTS=ON
	-DSHADERC_SKIP_EXAMPLES=ON
	-DSHADERC_SKIP_COPYRIGHT_CHECK=ON
	-DPython3_EXECUTABLE=${Python3_EXECUTABLE}
	-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}
	-DSHADERC_SKIP_INSTALL=OFF
)

if(MSVC)
	list(APPEND SHADERC_ARGS -DSHADERC_ENABLE_SHARED_CRT=OFF)
	set(SHADERC_LIBRARY shaderc_shared.lib)
else()
	set(SHADERC_LIBRARY libshaderc_shared.so)
endif()

ExternalProject_Add(shaderc
	GIT_REPOSITORY https://github.com/google/shaderc
	GIT_TAG ${SHADERC}
	${SHALLOW_CLONE}

	PATCH_COMMAND
		${Python3_EXECUTABLE} <SOURCE_DIR>/utils/git-sync-deps

	COMMAND
		${CMAKE_COMMAND} -E chdir <SOURCE_DIR>/third_party/glslang
		git fetch --depth 1 origin ${SHADERC_GLSLANG}

	COMMAND
		${CMAKE_COMMAND} -E chdir <SOURCE_DIR>/third_party/glslang
		git checkout --detach FETCH_HEAD

	COMMAND
		${CMAKE_COMMAND} -E chdir <SOURCE_DIR>/third_party/spirv-headers
		git fetch --depth 1 origin ${SHADERC_SPIRVHEADERS}

	COMMAND
		${CMAKE_COMMAND} -E chdir <SOURCE_DIR>/third_party/spirv-headers
		git checkout --detach FETCH_HEAD

	COMMAND
		${CMAKE_COMMAND} -E chdir <SOURCE_DIR>/third_party/spirv-tools
		git fetch --depth 1 origin ${SHADERC_SPIRVTOOLS}

	COMMAND
		${CMAKE_COMMAND} -E chdir <SOURCE_DIR>/third_party/spirv-tools
		git checkout --detach FETCH_HEAD

	CMAKE_ARGS
		${COMMON_ARGS}
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5
		${SHADERC_ARGS}

	BUILD_COMMAND
		${CMAKE_COMMAND}
		--build <BINARY_DIR>
		--parallel ${NPROCS}

	INSTALL_COMMAND
		${CMAKE_COMMAND} --build <BINARY_DIR> --target install
)

# ---------------------------------------------------------------------------
# MoltenVK
# ---------------------------------------------------------------------------

if(APPLE)
	ExternalProject_Add(moltenvk
		URL
			https://github.com/KhronosGroup/MoltenVK/releases/download/${MOLTENVK}/MoltenVK-macos.tar

		DOWNLOAD_NO_PROGRESS ON

		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""

		INSTALL_COMMAND
			${CMAKE_COMMAND} -E make_directory
			${DEPS_INSTALL_DIR}/lib

		COMMAND
			${CMAKE_COMMAND} -E copy
			<SOURCE_DIR>/MoltenVK/dylib/macOS/libMoltenVK.dylib
			${DEPS_INSTALL_DIR}/lib/libMoltenVK.dylib

		DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	)
endif()
