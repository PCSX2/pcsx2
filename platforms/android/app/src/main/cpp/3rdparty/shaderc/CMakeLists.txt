# Copyright 2020 The Shaderc Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.22.1)

project(shaderc)
enable_testing()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
  message(STATUS "No build type selected, default to Debug")
  set(CMAKE_BUILD_TYPE "Debug")
endif()

message(STATUS "Shaderc: build type is \"${CMAKE_BUILD_TYPE}\".")

option(SHADERC_ENABLE_WGSL_OUTPUT "Enable WGSL output" OFF)

option(SHADERC_SKIP_INSTALL "Skip installation" ${SHADERC_SKIP_INSTALL})
if(NOT ${SHADERC_SKIP_INSTALL})
  set(SHADERC_ENABLE_INSTALL ON)
endif()

option(SHADERC_SKIP_TESTS "Skip building tests" ${SHADERC_SKIP_TESTS})
if(NOT ${SHADERC_SKIP_TESTS})
  set(SHADERC_ENABLE_TESTS ON)
endif()
if(${SHADERC_ENABLE_TESTS})
  message(STATUS "Configuring Shaderc to build tests.")
else()
  message(STATUS "Configuring Shaderc to avoid building tests.")
endif()

option(SHADERC_SKIP_EXAMPLES "Skip building examples" ${SHADERC_SKIP_EXAMPLES})
if(NOT ${SHADERC_SKIP_EXAMPLES})
  set(SHADERC_ENABLE_EXAMPLES ON)
endif()
if(${SHADERC_ENABLE_EXAMPLES})
  message(STATUS "Configuring Shaderc to build examples.")
else()
  message(STATUS "Configuring Shaderc to avoid building examples.")
endif()

option(SHADERC_SKIP_EXECUTABLES "Skip building executables." ${SHADERC_SKIP_EXECUTABLES})
if(NOT ${SHADERC_SKIP_EXECUTABLES})
  set(SHADERC_ENABLE_EXECUTABLES ON)
endif()
if (${SHADERC_ENABLE_EXECUTABLES})
  message(STATUS "Configuring Shaderc to build executables.")
else()
  message(STATUS "Configuring Shaderc to avoid building executables.")
endif()

option(SHADERC_SKIP_COPYRIGHT_CHECK "Skip copyright check" ${SHADERC_SKIP_COPYRIGHT_CHECK})
if(NOT ${SHADERC_SKIP_COPYRIGHT_CHECK})
  set(SHADERC_ENABLE_COPYRIGHT_CHECK ON)
endif()
if(${SHADERC_ENABLE_COPYRIGHT_CHECK})
  message(STATUS "Configuring Shaderc to check copyrights.")
else()
  message(STATUS "Configuring Shaderc to avoid checking copyrights.")
endif()

option(SHADERC_ENABLE_WERROR_COMPILE "Enable passing -Werror to compiler, if available" ON)

set (CMAKE_CXX_STANDARD 17)

include(GNUInstallDirs)
include(cmake/setup_build.cmake)
include(cmake/utils.cmake)
include(CheckCXXCompilerFlag)

set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS ${DISABLE_EXCEPTIONS} CACHE BOOL "Coupling SPIRV-Cross exception conversion to DISABLE_EXCEPTIONS" FORCE)
if(DISABLE_EXCEPTIONS)
  # Need to set additional values here, since some of the wrapped code occurs in
  # .h/.hpp files, so maybe included outside of the library.
 add_definitions(-DSPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
endif()

# These flags are not supported on Windows and some older version of GCC
# that our bots use.
# Warning about implicit fallthrough in switch blocks
check_cxx_compiler_flag(-Wimplicit-fallthrough COMPILER_SUPPORTS_FALLTHROUGH_WARNING)
if (COMPILER_SUPPORTS_FALLTHROUGH_WARNING)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wimplicit-fallthrough")
endif()

# Warning about extra semi-colons
check_cxx_compiler_flag(-Wextra-semi COMPILER_SUPPORTS_EXTRA_SEMI_WARNING)
if (COMPILER_SUPPORTS_EXTRA_SEMI_WARNING)
    add_compile_options("-Wextra-semi")
endif()

find_package(Python COMPONENTS Interpreter REQUIRED)

if (SHADERC_ENABLE_COPYRIGHT_CHECK)
  add_custom_target(check-copyright ALL
    ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/utils/add_copyright.py
    --check
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Check copyright")
endif()

add_custom_target(add-copyright
  ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/utils/add_copyright.py
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Add copyright")

if(MSVC)
  option(SHADERC_ENABLE_SHARED_CRT
          "Use the shared CRT instead of the static CRT"
		  OFF)
  if (SHADERC_ENABLE_SHARED_CRT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif(MSVC)


# Configure subdirectories.
# We depend on these for later projects, so they should come first.
add_subdirectory(third_party)

add_subdirectory(libshaderc_util)
add_subdirectory(libshaderc)
add_subdirectory(glslc)
if(${SHADERC_ENABLE_EXAMPLES})
    add_subdirectory(examples)
endif()

add_custom_target(build-version
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/utils/update_build_version.py
  ${shaderc_SOURCE_DIR} ${spirv-tools_SOURCE_DIR} ${glslang_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/build-version.inc
  COMMENT "Update build-version.inc in the Shaderc build directory (if necessary).")

function(define_pkg_config_file NAME LIBS)
  add_custom_target(${NAME}-pkg-config ALL
    COMMAND ${CMAKE_COMMAND}
      -DCHANGES_FILE=${CMAKE_CURRENT_SOURCE_DIR}/CHANGES
      -DTEMPLATE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/${NAME}.pc.in
      -DOUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/${NAME}.pc
      -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
      -DCMAKE_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}
      -DLIBS=${LIBS}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/write_pkg_config.cmake
  DEPENDS "CHANGES" "cmake/${NAME}.pc.in" "cmake/write_pkg_config.cmake")

  if (SHADERC_ENABLE_INSTALL)
    install(
      FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.pc
      DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
  endif()
endfunction()

define_pkg_config_file(shaderc -lshaderc_shared)
define_pkg_config_file(shaderc_static "-lshaderc ${EXTRA_STATIC_PKGCONFIG_LIBS} -lshaderc_util")
define_pkg_config_file(shaderc_combined -lshaderc_combined)
