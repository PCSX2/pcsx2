# CMake support for LZ4
#
# To the extent possible under law, the author(s) have dedicated all
# copyright and related and neighboring rights to this software to
# the public domain worldwide. This software is distributed without
# any warranty.
#
# For details, see <http://creativecommons.org/publicdomain/zero/1.0/>.


# Use range version specification for policy control while maintaining 
# compatibility with older CMake versions
cmake_minimum_required(VERSION 3.5...4.0.2)

set(LZ4_TOP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

#-----------------------------------------------------------------------------
# VERSION EXTRACTION - Parse version information from header file
#-----------------------------------------------------------------------------
function(parse_lz4_version VERSION_TYPE)
    file(STRINGS "${LZ4_TOP_SOURCE_DIR}/lib/lz4.h" version_line REGEX "^#define LZ4_VERSION_${VERSION_TYPE} +([0-9]+).*$")
    string(REGEX REPLACE "^#define LZ4_VERSION_${VERSION_TYPE} +([0-9]+).*$" "\\1" version_number "${version_line}")
    set(LZ4_VERSION_${VERSION_TYPE} ${version_number} PARENT_SCOPE)
endfunction()

foreach(version_type IN ITEMS MAJOR MINOR RELEASE)
    parse_lz4_version(${version_type})
endforeach()

set(LZ4_VERSION_STRING "${LZ4_VERSION_MAJOR}.${LZ4_VERSION_MINOR}.${LZ4_VERSION_RELEASE}")
mark_as_advanced(LZ4_VERSION_STRING LZ4_VERSION_MAJOR LZ4_VERSION_MINOR LZ4_VERSION_RELEASE)

message(STATUS "Creating build script for LZ4 version: ${LZ4_VERSION_STRING}")

project(LZ4 VERSION ${LZ4_VERSION_STRING} LANGUAGES C)

#-----------------------------------------------------------------------------
# DEFAULT BUILD TYPE - Set Release as default when no build type is specified
#-----------------------------------------------------------------------------
# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

#-----------------------------------------------------------------------------
# BUILD OPTIONS - Configure build targets and features
#-----------------------------------------------------------------------------
option(LZ4_BUILD_CLI "Build lz4 program" ON)
option(LZ4_BUILD_LEGACY_LZ4C "Build lz4c program with legacy argument support" OFF)


# Determine if LZ4 is being built as part of another project.
# If LZ4 is bundled in another project, we don't want to install anything.
# Default behavior can be overridden by setting the LZ4_BUNDLED_MODE variable.
if(NOT DEFINED LZ4_BUNDLED_MODE)
  get_directory_property(LZ4_IS_SUBPROJECT PARENT_DIRECTORY)
  if(LZ4_IS_SUBPROJECT)
    set(LZ4_BUNDLED_MODE ON)
  else()
    set(LZ4_BUNDLED_MODE OFF)
  endif()
endif()
mark_as_advanced(LZ4_BUNDLED_MODE)

#-----------------------------------------------------------------------------
# PACKAGING - CPack configuration
#-----------------------------------------------------------------------------
if(NOT LZ4_BUNDLED_MODE AND NOT CPack_CMake_INCLUDED)
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "LZ4 compression library")
  set(CPACK_PACKAGE_DESCRIPTION_FILE "${LZ4_TOP_SOURCE_DIR}/README.md")
  set(CPACK_RESOURCE_FILE_LICENSE "${LZ4_TOP_SOURCE_DIR}/LICENSE")
  set(CPACK_PACKAGE_VERSION_MAJOR ${LZ4_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${LZ4_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${LZ4_VERSION_RELEASE})
  include(CPack)
endif(NOT LZ4_BUNDLED_MODE AND NOT CPack_CMake_INCLUDED)

#-----------------------------------------------------------------------------
# LIBRARY TYPE CONFIGURATION - Static vs Shared libraries
#-----------------------------------------------------------------------------
# Allow people to choose whether to build shared or static libraries
# via the BUILD_SHARED_LIBS option unless we are in bundled mode, in
# which case we always use static libraries.
include(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON "NOT LZ4_BUNDLED_MODE" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_STATIC_LIBS "Build static libraries" OFF "BUILD_SHARED_LIBS" ON)

if(NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
  message(FATAL_ERROR "Both BUILD_SHARED_LIBS and BUILD_STATIC_LIBS have been disabled")
endif(NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)

#-----------------------------------------------------------------------------
# SOURCE FILES & DIRECTORIES - Path setup and source file collection
#-----------------------------------------------------------------------------
set(LZ4_LIB_SOURCE_DIR "${LZ4_TOP_SOURCE_DIR}/lib")
set(LZ4_PROG_SOURCE_DIR "${LZ4_TOP_SOURCE_DIR}/programs")

include_directories("${LZ4_LIB_SOURCE_DIR}")

# CLI sources
file(GLOB LZ4_SOURCES
     "${LZ4_LIB_SOURCE_DIR}/*.c")
file(GLOB LZ4_CLI_SOURCES
     "${LZ4_PROG_SOURCE_DIR}/*.c")
list(APPEND LZ4_CLI_SOURCES ${LZ4_SOURCES}) # LZ4_CLI always use liblz4 sources directly.

#-----------------------------------------------------------------------------
# POSITION INDEPENDENT CODE - PIC settings for static libraries
#-----------------------------------------------------------------------------
# Whether to use position independent code for the static library.  If
# we're building a shared library this is ignored and PIC is always
# used.
if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE OR CMAKE_POSITION_INDEPENDENT_CODE)
  set(LZ4_POSITION_INDEPENDENT_LIB_DEFAULT ON)
else()
  set(LZ4_POSITION_INDEPENDENT_LIB_DEFAULT OFF)
endif(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE OR CMAKE_POSITION_INDEPENDENT_CODE)

option(LZ4_POSITION_INDEPENDENT_LIB "Use position independent code for static library (if applicable)" ${LZ4_POSITION_INDEPENDENT_LIB_DEFAULT})

#-----------------------------------------------------------------------------
# TARGETS - Library and executable targets
#-----------------------------------------------------------------------------
# liblz4
include(GNUInstallDirs)
set(LZ4_LIBRARIES_BUILT)
if(BUILD_SHARED_LIBS)
  add_library(lz4_shared SHARED ${LZ4_SOURCES})
  target_include_directories(lz4_shared
    PUBLIC $<BUILD_INTERFACE:${LZ4_LIB_SOURCE_DIR}>
    INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  set_target_properties(lz4_shared PROPERTIES
    OUTPUT_NAME lz4
    SOVERSION "${LZ4_VERSION_MAJOR}"
    VERSION "${LZ4_VERSION_STRING}")
  if(MSVC)
    target_compile_definitions(lz4_shared PRIVATE
      LZ4_DLL_EXPORT=1)
  endif(MSVC)
  list(APPEND LZ4_LIBRARIES_BUILT lz4_shared)
endif()
if(BUILD_STATIC_LIBS)
  set(STATIC_LIB_NAME lz4)
  if(MSVC AND BUILD_SHARED_LIBS)
    set(STATIC_LIB_NAME lz4_static)
  endif(MSVC AND BUILD_SHARED_LIBS)
  add_library(lz4_static STATIC ${LZ4_SOURCES})
  target_include_directories(lz4_static
    PUBLIC $<BUILD_INTERFACE:${LZ4_LIB_SOURCE_DIR}>
    INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  set_target_properties(lz4_static PROPERTIES
    OUTPUT_NAME ${STATIC_LIB_NAME}
    POSITION_INDEPENDENT_CODE ${LZ4_POSITION_INDEPENDENT_LIB})
  list(APPEND LZ4_LIBRARIES_BUILT lz4_static)
endif()
# Add unified target.
add_library(lz4 INTERFACE)
list(APPEND LZ4_LIBRARIES_BUILT lz4)
if(BUILD_SHARED_LIBS)
  target_link_libraries(lz4 INTERFACE lz4_shared)
else()
  target_link_libraries(lz4 INTERFACE lz4_static)
endif(BUILD_SHARED_LIBS)

#-----------------------------------------------------------------------------
# DEBUG CONFIGURATION - Configurable LZ4_DEBUG level
#-----------------------------------------------------------------------------
# LZ4_DEBUG levels:
# 0 - Disable everything (default for Release)
# 1 - Enable assert() statements  
# 2-8 - Enable debug traces with increasing verbosity
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(LZ4_DEBUG_LEVEL_DEFAULT 1)
else()
  set(LZ4_DEBUG_LEVEL_DEFAULT 0)
endif()

set(LZ4_DEBUG_LEVEL ${LZ4_DEBUG_LEVEL_DEFAULT} CACHE STRING 
    "LZ4 debug level: 0=disabled, 1=assert(), 2-8=debug traces with increasing verbosity")
set_property(CACHE LZ4_DEBUG_LEVEL PROPERTY STRINGS "0" "1" "2" "3" "4" "5" "6" "7" "8")

# Apply LZ4_DEBUG configuration if level > 0
if(LZ4_DEBUG_LEVEL GREATER 0)
  if(MSVC)
    add_definitions(/DLZ4_DEBUG=${LZ4_DEBUG_LEVEL})
  else()
    add_definitions(-DLZ4_DEBUG=${LZ4_DEBUG_LEVEL})
  endif()
endif()

#-----------------------------------------------------------------------------
# NAMESPACE CONFIGURATION - xxHash namespace settings
#-----------------------------------------------------------------------------
# xxhash namespace
if(BUILD_SHARED_LIBS)
  target_compile_definitions(lz4_shared PRIVATE
    XXH_NAMESPACE=LZ4_)
endif(BUILD_SHARED_LIBS)
if(BUILD_STATIC_LIBS)
  target_compile_definitions(lz4_static PRIVATE
    XXH_NAMESPACE=LZ4_)
endif(BUILD_STATIC_LIBS)

#-----------------------------------------------------------------------------
# CLI EXECUTABLES - Configuring command-line programs
#-----------------------------------------------------------------------------
# lz4
if(LZ4_BUILD_CLI)
  set(LZ4_PROGRAMS_BUILT lz4cli)
  add_executable(lz4cli ${LZ4_CLI_SOURCES})
  set_target_properties(lz4cli PROPERTIES OUTPUT_NAME lz4)
endif(LZ4_BUILD_CLI)

# lz4c
if(LZ4_BUILD_LEGACY_LZ4C)
  list(APPEND LZ4_PROGRAMS_BUILT lz4c)
  add_executable(lz4c ${LZ4_CLI_SOURCES})
  set_target_properties(lz4c PROPERTIES COMPILE_DEFINITIONS "ENABLE_LZ4C_LEGACY_OPTIONS")
endif(LZ4_BUILD_LEGACY_LZ4C)

#-----------------------------------------------------------------------------
# COMPILER FLAGS - Configure warning flags and compiler-specific options
#-----------------------------------------------------------------------------
# Extra warning flags
if(MSVC)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /W4")
else()
  include(CheckCCompilerFlag)
  foreach(flag
      # GCC-style
      -pedantic-errors
      -Wall
      -Wextra
      -Wundef
      -Wcast-qual
      -Wcast-align
      -Wshadow
      -Wswitch-enum
      -Wdeclaration-after-statement
      -Wstrict-prototypes
      -Wpointer-arith)

    # Because https://gcc.gnu.org/wiki/FAQ#wnowarning
    string(REGEX REPLACE "\\-Wno\\-(.+)" "-W\\1" flag_to_test "${flag}")
    string(REGEX REPLACE "[^a-zA-Z0-9]+" "_" test_name "CFLAG_${flag_to_test}")

    check_c_compiler_flag("${ADD_COMPILER_FLAGS_PREPEND} ${flag_to_test}" ${test_name})

    if(${test_name})
      set(CMAKE_C_FLAGS_DEBUG "${flag} ${CMAKE_C_FLAGS_DEBUG}")
    endif()

    unset(test_name)
    unset(flag_to_test)
  endforeach(flag)
endif(MSVC)


#-----------------------------------------------------------------------------
# INSTALLATION - Install targets, headers, and documentation
#-----------------------------------------------------------------------------
if(NOT LZ4_BUNDLED_MODE)
  install(TARGETS ${LZ4_PROGRAMS_BUILT}
    BUNDLE	DESTINATION "${CMAKE_INSTALL_BINDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(TARGETS ${LZ4_LIBRARIES_BUILT}
    EXPORT lz4Targets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(FILES
    "${LZ4_LIB_SOURCE_DIR}/lz4.h"
    "${LZ4_LIB_SOURCE_DIR}/lz4hc.h"
    "${LZ4_LIB_SOURCE_DIR}/lz4frame.h"
    "${LZ4_LIB_SOURCE_DIR}/lz4file.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(FILES "${LZ4_PROG_SOURCE_DIR}/lz4.1"
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/liblz4.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

#-----------------------------------------------------------------------------
# CMAKE PACKAGE CONFIG - Configure CMake package for find_package support
#-----------------------------------------------------------------------------
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/lz4ConfigVersion.cmake"
    VERSION ${LZ4_VERSION_STRING}
    COMPATIBILITY SameMajorVersion)

  set(LZ4_PKG_INSTALLDIR "${CMAKE_INSTALL_LIBDIR}/cmake/lz4")
  configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/lz4Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/lz4Config.cmake"
    INSTALL_DESTINATION ${LZ4_PKG_INSTALLDIR})
  export(EXPORT lz4Targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/lz4Targets.cmake
    NAMESPACE LZ4::)

  install(EXPORT lz4Targets
    FILE lz4Targets.cmake
    NAMESPACE LZ4::
    DESTINATION ${LZ4_PKG_INSTALLDIR})
  install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/lz4Config.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/lz4ConfigVersion.cmake
    DESTINATION ${LZ4_PKG_INSTALLDIR})

#-----------------------------------------------------------------------------
# SYMLINKS - Create and install Unix symlinks and manpage aliases
#-----------------------------------------------------------------------------
  # Install lz4cat and unlz4 symlinks on Unix systems
  if(UNIX AND LZ4_BUILD_CLI)
    foreach(cli_tool IN ITEMS lz4cat unlz4)
      # Create a custom target for the symlink creation
      add_custom_target("create_${cli_tool}_symlink" ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          $<TARGET_FILE_NAME:lz4cli> ${cli_tool}
        DEPENDS lz4cli
        COMMENT "Creating symlink for ${cli_tool}"
        VERBATIM)

      # Install the symlink into the binary installation directory
      install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${cli_tool}"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME ${cli_tool})
    endforeach()

    # create manpage aliases
    foreach(f lz4cat unlz4)
      file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${f}.1" ".so man1/lz4.1\n")
      install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${f}.1"
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
    endforeach()
  endif(UNIX AND LZ4_BUILD_CLI)
endif(NOT LZ4_BUNDLED_MODE)

#-----------------------------------------------------------------------------
# PKG-CONFIG - Generate and install pkg-config file
#-----------------------------------------------------------------------------
# pkg-config
set(PREFIX "${CMAKE_INSTALL_PREFIX}")

if("${CMAKE_INSTALL_FULL_LIBDIR}" STREQUAL "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
  set(LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
else()
  set(LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}")
endif("${CMAKE_INSTALL_FULL_LIBDIR}" STREQUAL "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

if("${CMAKE_INSTALL_FULL_INCLUDEDIR}" STREQUAL "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
  set(INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
else()
  set(INCLUDEDIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
endif("${CMAKE_INSTALL_FULL_INCLUDEDIR}" STREQUAL "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")

# for liblz4.pc substitution
set(VERSION ${LZ4_VERSION_STRING})
configure_file(${LZ4_LIB_SOURCE_DIR}/liblz4.pc.in liblz4.pc @ONLY)
