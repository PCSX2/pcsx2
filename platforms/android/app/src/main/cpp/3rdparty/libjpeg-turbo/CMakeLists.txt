cmake_minimum_required(VERSION 3.15...3.28)

if(CMAKE_EXECUTABLE_SUFFIX)
  set(CMAKE_EXECUTABLE_SUFFIX_TMP ${CMAKE_EXECUTABLE_SUFFIX})
endif()

set(CMAKE_C_FLAGS_RELEASE_INITIALIZED_TO_DEFAULT 1)
if(CMAKE_C_FLAGS_RELEASE)
  set(CMAKE_C_FLAGS_RELEASE_INITIALIZED_TO_DEFAULT 0)
endif()
set(CMAKE_C_FLAGS_RELWITHDEBINFO_INITIALIZED_TO_DEFAULT 1)
if(CMAKE_C_FLAGS_RELWITHDEBINFO)
  set(CMAKE_C_FLAGS_RELWITHDEBINFO_INITIALIZED_TO_DEFAULT 0)
endif()

project(libjpeg-turbo C)
set(VERSION 3.1.90)
set(COPYRIGHT_YEAR "1991-2026")
string(REPLACE "." ";" VERSION_TRIPLET ${VERSION})
list(GET VERSION_TRIPLET 0 VERSION_MAJOR)
list(GET VERSION_TRIPLET 1 VERSION_MINOR)
list(GET VERSION_TRIPLET 2 VERSION_REVISION)
function(pad_number NUMBER OUTPUT_LEN)
  string(LENGTH "${${NUMBER}}" INPUT_LEN)
  if(INPUT_LEN LESS OUTPUT_LEN)
    math(EXPR ZEROES "${OUTPUT_LEN} - ${INPUT_LEN} - 1")
    set(NUM ${${NUMBER}})
    foreach(C RANGE ${ZEROES})
      set(NUM "0${NUM}")
    endforeach()
    set(${NUMBER} ${NUM} PARENT_SCOPE)
  endif()
endfunction()
set(TMP_VERSION_MINOR ${VERSION_MINOR})
set(TMP_VERSION_REVISION ${VERSION_REVISION})
pad_number(TMP_VERSION_MINOR 3)
pad_number(TMP_VERSION_REVISION 3)
set(LIBJPEG_TURBO_VERSION_NUMBER
  ${VERSION_MAJOR}${TMP_VERSION_MINOR}${TMP_VERSION_REVISION})
set(MSVC_LIKE 0)
if(MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
  set(MSVC_LIKE 1)
endif()

# The libjpeg-turbo build system has never supported and will never support
# being integrated into another build system using add_subdirectory(), because
# doing so would require that we (minimally):
#
# 1. avoid using certain CMake variables, such as CMAKE_SOURCE_DIR,
#    CMAKE_BINARY_DIR, and CMAKE_PROJECT_NAME;
# 2. avoid using implicit include directories and relative paths;
# 3. optionally provide a way to skip the installation of libjpeg-turbo
#    components when the 'install' target is built;
# 4. optionally provide a way to postfix target names, to avoid namespace
#    conflicts;
# 5. restructure the top-level CMakeLists.txt so that it properly sets the
#    PROJECT_VERSION variable; and
# 6. design automated regression tests to ensure that new commits don't break
#    any of the above.
#
# Even if we did all of that, issues would still arise, because it is
# impossible for an upstream build system to anticipate the widely varying
# needs of every downstream build system.  That's why the CMake
# ExternalProject_Add() function exists.  Downstream projects that wish to
# integrate libjpeg-turbo as a subdirectory should either use
# ExternalProject_Add() or make downstream modifications to the libjpeg-turbo
# build system to suit their specific needs.  Please do not file bug reports,
# feature requests, or pull requests regarding this.
# add_subdirectory guard removed for vendored build

# CMake 3.14 and later sets CMAKE_MACOSX_BUNDLE to TRUE by default when
# CMAKE_SYSTEM_NAME is iOS, tvOS, or watchOS, which breaks the libjpeg-turbo
# build.  (Specifically, when CMAKE_MACOSX_BUNDLE is TRUE, executables for
# Apple platforms are built as application bundles, which causes CMake to
# complain that our install() directives for executables do not specify a
# BUNDLE DESTINATION.  Even if CMake did not complain, building executables as
# application bundles would break our iOS packages.)
set(CMAKE_MACOSX_BUNDLE FALSE)

get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY
  GENERATOR_IS_MULTI_CONFIG)
# If the GENERATOR_IS_MULTI_CONFIG property doesn't exist (CMake < 3.9), then
# set the GENERATOR_IS_MULTI_CONFIG variable manually if the generator is
# Visual Studio or Xcode (the only multi-config generators in CMake < 3.9).
if(NOT GENERATOR_IS_MULTI_CONFIG AND (MSVC_IDE OR XCODE))
  set(GENERATOR_IS_MULTI_CONFIG TRUE)
endif()

string(TIMESTAMP DEFAULT_BUILD "%Y%m%d")
set(BUILD ${DEFAULT_BUILD} CACHE STRING "Build string (default: ${DEFAULT_BUILD})")

# NOTE: On Windows, this does nothing except when using MinGW or Cygwin.
# CMAKE_BUILD_TYPE has no meaning in Visual Studio, and it always defaults to
# Debug when using NMake.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

message(STATUS "VERSION = ${VERSION}, BUILD = ${BUILD}")

include(cmakescripts/PackageInfo.cmake)

# Detect CPU type and whether we're building 64-bit or 32-bit code
math(EXPR BITS "${CMAKE_SIZEOF_VOID_P} * 8")
string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} CMAKE_SYSTEM_PROCESSOR_LC)
set(COUNT 1)
foreach(ARCH ${CMAKE_OSX_ARCHITECTURES})
  if(COUNT GREATER 1)
    message(FATAL_ERROR "${CMAKE_PROJECT_NAME} contains assembly code, so it cannot be built with multiple values in CMAKE_OSX_ARCHITECTURES.")
  endif()
  math(EXPR COUNT "${COUNT}+1")
endforeach()
if(CMAKE_SYSTEM_PROCESSOR_LC MATCHES "x86_64" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "amd64" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "i[0-9]86" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "x86" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "ia32")
  if(BITS EQUAL 64 OR CMAKE_C_COMPILER_ABI MATCHES "ELF X32")
    set(CPU_TYPE x86_64)
  else()
    set(CPU_TYPE i386)
  endif()
  if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL ${CPU_TYPE})
    set(CMAKE_SYSTEM_PROCESSOR ${CPU_TYPE})
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR_LC STREQUAL "aarch64" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "^arm")
  if(BITS EQUAL 64)
    set(CPU_TYPE arm64)
  else()
    set(CPU_TYPE arm)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR_LC MATCHES "^ppc" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "^powerpc")
  set(CPU_TYPE powerpc)
else()
  set(CPU_TYPE ${CMAKE_SYSTEM_PROCESSOR_LC})
endif()
if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" OR
  CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR
  CMAKE_OSX_ARCHITECTURES MATCHES "i386")
  set(CPU_TYPE ${CMAKE_OSX_ARCHITECTURES})
endif()
if(CMAKE_OSX_ARCHITECTURES MATCHES "ppc")
  set(CPU_TYPE powerpc)
endif()
if(MSVC_IDE AND CMAKE_GENERATOR_PLATFORM MATCHES "arm64")
  set(CPU_TYPE arm64)
endif()

message(STATUS "${BITS}-bit build (${CPU_TYPE})")


###############################################################################
# INSTALL DIRECTORIES
###############################################################################

if(WIN32)
  if(MSVC_LIKE)
    set(CMAKE_INSTALL_DEFAULT_PREFIX "c:/${CMAKE_PROJECT_NAME}")
  else()
    set(CMAKE_INSTALL_DEFAULT_PREFIX "c:/${CMAKE_PROJECT_NAME}-gcc")
  endif()
  if(BITS EQUAL 64)
    set(CMAKE_INSTALL_DEFAULT_PREFIX "${CMAKE_INSTALL_DEFAULT_PREFIX}64")
  endif()
else()
  if(NOT CMAKE_INSTALL_DEFAULT_PREFIX)
    set(CMAKE_INSTALL_DEFAULT_PREFIX /opt/${CMAKE_PROJECT_NAME})
  endif()
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_DEFAULT_PREFIX}" CACHE PATH
    "Directory into which to install ${CMAKE_PROJECT_NAME} (default: ${CMAKE_INSTALL_DEFAULT_PREFIX})"
    FORCE)
endif()
message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")

# When the prefix is /opt/${CMAKE_PROJECT_NAME}, we assume that an "official"
# build is being created, and thus we install things into specific locations.

if(CMAKE_INSTALL_PREFIX STREQUAL "${CMAKE_INSTALL_DEFAULT_PREFIX}")
  set(CMAKE_INSTALL_DEFAULT_DATAROOTDIR "")
  set(CMAKE_INSTALL_DEFAULT_DOCDIR "<CMAKE_INSTALL_DATAROOTDIR>/doc")
  set(CMAKE_INSTALL_DEFAULT_JAVADIR "<CMAKE_INSTALL_DATAROOTDIR>/classes")
  if(UNIX AND NOT APPLE)
    if(BITS EQUAL 64)
      set(CMAKE_INSTALL_DEFAULT_LIBDIR "lib64")
    elseif(CMAKE_C_COMPILER_ABI MATCHES "ELF X32")
      set(CMAKE_INSTALL_DEFAULT_LIBDIR "libx32")
    else()
      set(CMAKE_INSTALL_DEFAULT_LIBDIR "lib32")
    endif()
  endif()
endif()

include(cmakescripts/GNUInstallDirs.cmake)

macro(report_directory var)
  if(CMAKE_INSTALL_${var} STREQUAL CMAKE_INSTALL_FULL_${var})
    message(STATUS "CMAKE_INSTALL_${var} = ${CMAKE_INSTALL_${var}}")
  else()
    message(STATUS "CMAKE_INSTALL_${var} = ${CMAKE_INSTALL_${var}} (${CMAKE_INSTALL_FULL_${var}})")
  endif()
  mark_as_advanced(CLEAR CMAKE_INSTALL_${var})
endmacro()

set(DIRLIST "BINDIR;DATAROOTDIR;DOCDIR;INCLUDEDIR;LIBDIR")
if(UNIX)
  list(APPEND DIRLIST "MANDIR")
endif()
foreach(dir ${DIRLIST})
  # CMAKE_INSTALL_BINDIR, CMAKE_INSTALL_DOCDIR, CMAKE_INSTALL_INCLUDEDIR,
  # CMAKE_INSTALL_LIBDIR, and CMAKE_INSTALL_MANDIR cannot be blank, because
  # either CMake will fail to configure the build, or install() will attempt to
  # install files to the root directory.
  if(NOT dir STREQUAL "DATAROOTDIR" AND NOT CMAKE_INSTALL_${dir})
    message(FATAL_ERROR "CMAKE_INSTALL_${dir} cannot be blank")
  endif()
  report_directory(${dir})
endforeach()


###############################################################################
# CONFIGURATION OPTIONS
###############################################################################

macro(boolean_number var)
  if(${var})
    set(${var} 1 ${ARGN})
  else()
    set(${var} 0 ${ARGN})
  endif()
endmacro()

option(ENABLE_SHARED "Build shared libraries" TRUE)
boolean_number(ENABLE_SHARED)
option(ENABLE_STATIC "Build static libraries" TRUE)
boolean_number(ENABLE_STATIC)
option(REQUIRE_SIMD
  "Generate a fatal error if SIMD extensions are not available for this platform (default is to fall back to a non-SIMD build)"
  FALSE)
boolean_number(REQUIRE_SIMD)
option(WITH_ARITH_DEC
  "Include arithmetic decoding support when emulating the libjpeg v6b API/ABI"
  TRUE)
boolean_number(WITH_ARITH_DEC)
option(WITH_ARITH_ENC
  "Include arithmetic encoding support when emulating the libjpeg v6b API/ABI"
  TRUE)
boolean_number(WITH_ARITH_ENC)
if(CMAKE_C_COMPILER_ABI MATCHES "ELF X32")
  unset(WITH_JNA)
else()
  set(WITH_JNA "" CACHE PATH
    "Build TurboJPEG Java Native Access interfaces (TurboJPEG/JNA) using the specified JNA JAR path (implies ENABLE_SHARED=1 and WITH_TURBOJPEG=1)")
  if(WITH_JNA AND NOT EXISTS ${WITH_JNA})
    message(FATAL_ERROR "${WITH_JNA} does not exist.")
  endif()
endif()
option(WITH_JPEG7
  "Emulate libjpeg v7 API/ABI (this makes ${CMAKE_PROJECT_NAME} backward-incompatible with libjpeg v6b)"
  FALSE)
boolean_number(WITH_JPEG7)
option(WITH_JPEG8
  "Emulate libjpeg v8 API/ABI (this makes ${CMAKE_PROJECT_NAME} backward-incompatible with libjpeg v6b)"
  FALSE)
boolean_number(WITH_JPEG8)
option(WITH_SIMD "Include SIMD extensions, if available for this platform"
  TRUE)
boolean_number(WITH_SIMD)
option(WITH_TURBOJPEG
  "Include the TurboJPEG API library and associated command-line tools/test programs"
  TRUE)
boolean_number(WITH_TURBOJPEG)
option(WITH_TOOLS
  "Build command-line tools (Disabling this disables WITH_TESTS)" TRUE)
boolean_number(WITH_TOOLS)
option(WITH_TESTS "Enable regression tests, and build associated test programs (WARNING: Disable this at your own risk. If a build is not validated with the regression tests, then it is the responsibility of the builder to ensure, via other means, that the build produces mathematically correct results.)"
  TRUE)
boolean_number(WITH_TESTS)
if(NOT WITH_TOOLS)
  set(WITH_TESTS 0)
endif()
option(WITH_FUZZ "Build fuzz targets" FALSE)
option(WITH_PROFILE "Enable low-level JPEG algorithm performance profiling (useful for SIMD module development but not suitable for production)"
  FALSE)

macro(report_option var desc)
  if(${var})
    set(ENABLED enabled)
  else()
    set(ENABLED disabled)
  endif()
  set(OPTION_REPORT "${desc} ${ENABLED} (${var} = ${${var}})")
  string(LENGTH ${OPTION_REPORT} OPTION_REPORT_LENGTH)
  if(OPTION_REPORT_LENGTH GREATER 76)
    set(OPTION_REPORT "${desc} ${ENABLED}\n   (${var} = ${${var}})")
  endif()
  message(STATUS "${OPTION_REPORT}")
endmacro()

if(WITH_JNA)
  set(ENABLE_SHARED 1)
  set(WITH_TURBOJPEG 1)
endif()

# Explicitly setting CMAKE_POSITION_INDEPENDENT_CODE=FALSE disables PIC for all
# targets, which will cause the shared library builds to fail.  Thus, if shared
# libraries are enabled and CMAKE_POSITION_INDEPENDENT_CODE is explicitly set
# to FALSE, we need to unset it, thus restoring the default behavior
# (automatically using PIC for shared library targets.)
if(DEFINED CMAKE_POSITION_INDEPENDENT_CODE AND
  NOT CMAKE_POSITION_INDEPENDENT_CODE AND ENABLE_SHARED)
  unset(CMAKE_POSITION_INDEPENDENT_CODE CACHE)
endif()

report_option(ENABLE_SHARED "Shared libraries")
report_option(ENABLE_STATIC "Static libraries")

if(ENABLE_SHARED)
  set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
endif()

if(WITH_JPEG8 OR WITH_JPEG7)
  set(WITH_ARITH_ENC 1)
  set(WITH_ARITH_DEC 1)
endif()

if(WITH_ARITH_DEC)
  set(D_ARITH_CODING_SUPPORTED 1)
endif()
report_option(WITH_ARITH_DEC "Arithmetic decoding support")

if(WITH_ARITH_ENC)
  set(C_ARITH_CODING_SUPPORTED 1)
endif()
report_option(WITH_ARITH_ENC "Arithmetic encoding support")

report_option(WITH_TURBOJPEG "TurboJPEG API library")
report_option(WITH_JNA "TurboJPEG Java Native Access interfaces (TurboJPEG/JNA)")
report_option(WITH_TOOLS "Command-line tools")
report_option(WITH_TESTS "Regression tests")

if(NOT WITH_JPEG8)
  set(MEM_SRCDST_FUNCTIONS "global:  jpeg_mem_dest;  jpeg_mem_src;")
endif()

# 0: Original libjpeg v6b/v7/v8 API/ABI
#
# libjpeg v6b/v7 API/ABI emulation:
# 1: + In-memory source/destination managers (libjpeg-turbo 1.3.x)
# 2: + Partial image decompression functions (libjpeg-turbo 1.5.x)
# 3: + ICC functions (libjpeg-turbo 2.0.x)
# 4: + 12-bit-per-sample and lossless functions (libjpeg-turbo 3.0.x)
#
# libjpeg v8 API/ABI emulation:
# 1: + Partial image decompression functions (libjpeg-turbo 1.5.x)
# 2: + ICC functions (libjpeg-turbo 2.0.x)
# 3: + 12-bit-per-sample and lossless functions (libjpeg-turbo 3.0.x)
set(SO_AGE 3)
if(NOT WITH_JPEG8)
  set(SO_AGE 4)
endif()

if(WITH_JPEG8)
  set(JPEG_LIB_VERSION 80)
elseif(WITH_JPEG7)
  set(JPEG_LIB_VERSION 70)
else()
  set(JPEG_LIB_VERSION 62)
endif()

math(EXPR JPEG_LIB_VERSION_DIV10 "${JPEG_LIB_VERSION} / 10")
math(EXPR JPEG_LIB_VERSION_MOD10 "${JPEG_LIB_VERSION} % 10")
if(JPEG_LIB_VERSION STREQUAL "62")
  set(DEFAULT_SO_MAJOR_VERSION ${JPEG_LIB_VERSION})
else()
  set(DEFAULT_SO_MAJOR_VERSION ${JPEG_LIB_VERSION_DIV10})
endif()
if(JPEG_LIB_VERSION STREQUAL "80")
  set(DEFAULT_SO_MINOR_VERSION 2)
else()
  set(DEFAULT_SO_MINOR_VERSION 0)
endif()

# This causes SO_MAJOR_VERSION/SO_MINOR_VERSION to reset to defaults if
# WITH_JPEG7 or WITH_JPEG8 has changed.
if((DEFINED WITH_JPEG7_INT AND NOT WITH_JPEG7 EQUAL WITH_JPEG7_INT) OR
  (DEFINED WITH_JPEG8_INT AND NOT WITH_JPEG8 EQUAL WITH_JPEG8_INT))
  set(FORCE_SO_VERSION "FORCE")
endif()
set(WITH_JPEG7_INT ${WITH_JPEG7} CACHE INTERNAL "")
set(WITH_JPEG8_INT ${WITH_JPEG8} CACHE INTERNAL "")

set(SO_MAJOR_VERSION ${DEFAULT_SO_MAJOR_VERSION} CACHE STRING
  "Major version of the libjpeg API shared library (default: ${DEFAULT_SO_MAJOR_VERSION})"
  ${FORCE_SO_VERSION})
set(SO_MINOR_VERSION ${DEFAULT_SO_MINOR_VERSION} CACHE STRING
  "Minor version of the libjpeg API shared library (default: ${DEFAULT_SO_MINOR_VERSION})"
  ${FORCE_SO_VERSION})

set(JPEG_LIB_VERSION_DECIMAL "${JPEG_LIB_VERSION_DIV10}.${JPEG_LIB_VERSION_MOD10}")
message(STATUS "Emulating libjpeg API/ABI v${JPEG_LIB_VERSION_DECIMAL} (WITH_JPEG7 = ${WITH_JPEG7}, WITH_JPEG8 = ${WITH_JPEG8})")
message(STATUS "libjpeg API shared library version = ${SO_MAJOR_VERSION}.${SO_AGE}.${SO_MINOR_VERSION}")

# Because the TurboJPEG API library uses versioned symbols and changes the
# names of functions whenever they are modified in a backward-incompatible
# manner, it is always backward-ABI-compatible with itself, so the major and
# minor SO versions don't change.  However, we increase the middle number (the
# SO "age") whenever functions are added to the API, because adding functions
# affects forward API/ABI compatibility.
set(TURBOJPEG_SO_MAJOR_VERSION 0)
# 0: TurboJPEG 1.3.x API
# 1: TurboJPEG 1.4.x API
#    The TurboJPEG 1.5.x API modified some of the function prototypes, adding
#    the const keyword in front of pointers to unmodified buffers, but that did
#    not affect forward API/ABI compatibility.
# 2: TurboJPEG 2.0.x API
#    The TurboJPEG 2.1.x API modified the behavior of the tjDecompressHeader3()
#    function so that it accepts "abbreviated table specification" (AKA
#    "tables-only") datastreams as well as JPEG images, but that did not affect
#    forward API/ABI compatibility.
# 3: TurboJPEG 3.0.x API
# 4: TurboJPEG 3.1.x API
# 5: TurboJPEG 3.2.x API
set(TURBOJPEG_SO_AGE 5)
set(TURBOJPEG_SO_VERSION 0.${TURBOJPEG_SO_AGE}.0)


###############################################################################
# COMPILER SETTINGS
###############################################################################

string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_UC)

if(MSVC_LIKE)
  option(WITH_CRT_DLL
    "Link all ${CMAKE_PROJECT_NAME} libraries and executables with the C run-time DLL (msvcr*.dll) instead of the static C run-time library (libcmt*.lib.)  The default is to use the C run-time DLL only with the libraries and executables that need it."
    FALSE)
  boolean_number(WITH_CRT_DLL)
  if(NOT WITH_CRT_DLL)
    # Use the static C library for all build types
    if(CMAKE_BUILD_TYPE_UC STREQUAL "DEBUG")
      set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebug)
    elseif(MSVC_IDE)
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
      set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
    endif()
    message(STATUS "Visual C++ run-time library: ${CMAKE_MSVC_RUNTIME_LIBRARY} (WITH_CRT_DLL = ${WITH_CRT_DLL})")
  endif()
  add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  # Use the maximum optimization level for release builds
  foreach(build_type RELEASE RELWITHDEBINFO)
    if(CMAKE_C_FLAGS_${build_type} MATCHES "-O2" AND
      CMAKE_C_FLAGS_${build_type}_INITIALIZED_TO_DEFAULT)
      string(REGEX REPLACE "-O2" "-O3" CMAKE_C_FLAGS_${build_type}
        "${CMAKE_C_FLAGS_${build_type}}")
    endif()
  endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  if(CMAKE_C_COMPILER_ID MATCHES "SunPro")
    # Use the maximum optimization level for release builds
    foreach(build_type RELEASE RELWITHDEBINFO)
      if(CMAKE_C_FLAGS_${build_type} MATCHES "-xO3" AND
        CMAKE_C_FLAGS_${build_type}_INITIALIZED_TO_DEFAULT)
        string(REGEX REPLACE "-xO3" "-xO5" CMAKE_C_FLAGS_${build_type}
          "${CMAKE_C_FLAGS_${build_type}}")
      endif()
      if(CMAKE_C_FLAGS_${build_type} MATCHES "-xO2" AND
        CMAKE_C_FLAGS_${build_type}_INITIALIZED_TO_DEFAULT)
        string(REGEX REPLACE "-xO2" "-xO5" CMAKE_C_FLAGS_${build_type}
          "${CMAKE_C_FLAGS_${build_type}}")
      endif()
    endforeach()
  endif()
endif()

set(EFFECTIVE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UC}}")
message(STATUS "Compiler flags = ${EFFECTIVE_C_FLAGS}")

set(EFFECTIVE_LD_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_${CMAKE_BUILD_TYPE_UC}}")
message(STATUS "Linker flags = ${EFFECTIVE_LD_FLAGS}")

include(CheckCSourceCompiles)
include(CheckIncludeFiles)
include(CheckTypeSize)

check_type_size("size_t" SIZE_T)
check_type_size("unsigned long" UNSIGNED_LONG)

if(SIZE_T EQUAL UNSIGNED_LONG)
  check_c_source_compiles("int main(int argc, char **argv) { unsigned long a = argc;  return __builtin_ctzl(a); }"
    HAVE_BUILTIN_CTZL)
endif()
if(MSVC_LIKE)
  check_include_files("intrin.h" HAVE_INTRIN_H)
endif()

if(UNIX)
  if(CMAKE_CROSSCOMPILING)
    set(RIGHT_SHIFT_IS_UNSIGNED 0)
  else()
    include(CheckCSourceRuns)
    check_c_source_runs("
      #include <stdio.h>
      #include <stdlib.h>
      static int is_shifting_signed (long arg) {
        long res = arg >> 4;
        if (res == -0x7F7E80CL)
          return 1; /* right shift is signed */
        /* see if unsigned-shift hack will fix it. */
        /* we can't just test exact value since it depends on width of long... */
        res |= 0xFFFFFFFFL << (32-4);
        if (res == -0x7F7E80CL)
          return 0; /* right shift is unsigned */
        printf(\"Right shift isn't acting as I expect it to.\\\\n\");
        printf(\"I fear the JPEG software will not work at all.\\\\n\\\\n\");
        return 0; /* try it with unsigned anyway */
      }
      int main (void) {
        exit(is_shifting_signed(-0x7F7E80B1L));
      }" RIGHT_SHIFT_IS_UNSIGNED)
  endif()
endif()

if(NOT WIN32)
  check_c_source_compiles("extern const int table[1]; const int __attribute__((visibility(\"hidden\"))) table[1] = { 0 }; int main(void) { return table[0]; }"
    HIDDEN_WORKS)
  if(HIDDEN_WORKS)
    set(HIDDEN "__attribute__((visibility(\"hidden\")))")
    message(STATUS "HIDDEN = ${HIDDEN}")
  endif()
endif()

if(MSVC)
  set(INLINE_OPTIONS "__inline;inline")
else()
  set(INLINE_OPTIONS "__inline__;inline")
endif()
option(FORCE_INLINE "Force function inlining" TRUE)
boolean_number(FORCE_INLINE)
if(FORCE_INLINE)
  if(MSVC)
    list(INSERT INLINE_OPTIONS 0 "__forceinline")
  else()
    list(INSERT INLINE_OPTIONS 0 "inline __attribute__((always_inline))")
    list(INSERT INLINE_OPTIONS 0 "__inline__ __attribute__((always_inline))")
  endif()
endif()
foreach(inline ${INLINE_OPTIONS})
  check_c_source_compiles("${inline} static int foo(void) { return 0; } int main(void) { return foo(); }"
    INLINE_WORKS)
  if(INLINE_WORKS)
    set(INLINE ${inline})
    break()
  endif()
endforeach()
if(NOT INLINE_WORKS)
  message(FATAL_ERROR "Could not determine how to inline functions.")
endif()
message(STATUS "INLINE = ${INLINE} (FORCE_INLINE = ${FORCE_INLINE})")

if(MSVC)
  set(THREAD_LOCAL "__declspec(thread)")
else()
  set(THREAD_LOCAL "__thread")
endif()
check_c_source_compiles("static ${THREAD_LOCAL} int i;  int main(void) { i = 0;  return i; }"
  HAVE_THREAD_LOCAL)
if(HAVE_THREAD_LOCAL)
  message(STATUS "THREAD_LOCAL = ${THREAD_LOCAL}")
else()
  message(WARNING "Thread-local storage is not available.  The TurboJPEG API library's global error handler will not be thread-safe.")
  unset(THREAD_LOCAL)
endif()

if(UNIX AND NOT APPLE)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/conftest.map "VERS_1 { global: *; };")
  set(CMAKE_REQUIRED_FLAGS
    "-Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/conftest.map")
  check_c_source_compiles("int main(void) { return 0; }" HAVE_VERSION_SCRIPT)
  set(CMAKE_REQUIRED_FLAGS)
  file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/conftest.map)
  if(HAVE_VERSION_SCRIPT)
    message(STATUS "Linker supports GNU-style version scripts")
    set(MAPFLAG "-Wl,--version-script,")
    set(TJMAPFLAG "-Wl,--version-script,")
  else()
    message(STATUS "Linker does not support GNU-style version scripts")
    if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
      # The Solaris linker doesn't like our version script for the libjpeg API
      # library, but the version script for the TurboJPEG API library should
      # still work.
      file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/conftest.map
        "VERS_1 { global: foo;  local: *; }; VERS_2 { global: foo2; } VERS_1;")
      set(CMAKE_REQUIRED_FLAGS "-Wl,-M,${CMAKE_CURRENT_BINARY_DIR}/conftest.map -shared")
      check_c_source_compiles("int foo() { return 0; } int foo2() { return 2; }"
        HAVE_MAPFILE)
      set(CMAKE_REQUIRED_FLAGS)
      file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/conftest.map)
      if(HAVE_MAPFILE)
        message(STATUS "Linker supports mapfiles")
        set(TJMAPFLAG "-Wl,-M,")
      else()
        message(STATUS "Linker does not support mapfiles")
      endif()
    endif()
  endif()
endif()

# Generate files
configure_file(src/jversion.h.in jversion.h)
if(UNIX)
  configure_file(src/libjpeg.map.in libjpeg.map)
endif()

# Include directories and compiler definitions
include_directories(${CMAKE_CURRENT_BINARY_DIR})


###############################################################################
# TARGETS
###############################################################################

if(CMAKE_EXECUTABLE_SUFFIX_TMP)
  set(CMAKE_EXECUTABLE_SUFFIX ${CMAKE_EXECUTABLE_SUFFIX_TMP})
endif()
message(STATUS "CMAKE_EXECUTABLE_SUFFIX = ${CMAKE_EXECUTABLE_SUFFIX}")

add_custom_target(wrappers COMMAND ${CMAKE_COMMAND}
 -DSOURCE_DIR=${CMAKE_SOURCE_DIR}/src/wrapper
 -P ${CMAKE_CURRENT_SOURCE_DIR}/cmakescripts/GenerateWrappers.cmake)

set(JPEG_SOURCES src/jcapimin.c
  src/wrapper/jcapistd-8.c src/wrapper/jcapistd-12.c src/wrapper/jcapistd-16.c
  src/wrapper/jccoefct-8.c src/wrapper/jccoefct-12.c
  src/wrapper/jccolor-8.c src/wrapper/jccolor-12.c src/wrapper/jccolor-16.c
  src/wrapper/jcdctmgr-8.c src/wrapper/jcdctmgr-12.c
  src/wrapper/jcdiffct-8.c src/wrapper/jcdiffct-12.c src/wrapper/jcdiffct-16.c
  src/jchuff.c src/jcicc.c src/jcinit.c src/jclhuff.c
  src/wrapper/jclossls-8.c src/wrapper/jclossls-12.c src/wrapper/jclossls-16.c
  src/wrapper/jcmainct-8.c src/wrapper/jcmainct-12.c src/wrapper/jcmainct-16.c
  src/jcmarker.c src/jcmaster.c src/jcomapi.c src/jcparam.c src/jcphuff.c
  src/wrapper/jcprepct-8.c src/wrapper/jcprepct-12.c src/wrapper/jcprepct-16.c
  src/wrapper/jcsample-8.c src/wrapper/jcsample-12.c src/wrapper/jcsample-16.c
  src/jctrans.c src/jdapimin.c
  src/wrapper/jdapistd-8.c src/wrapper/jdapistd-12.c src/wrapper/jdapistd-16.c
  src/jdatadst.c src/jdatasrc.c
  src/wrapper/jdcoefct-8.c src/wrapper/jdcoefct-12.c
  src/wrapper/jdcolor-8.c src/wrapper/jdcolor-12.c src/wrapper/jdcolor-16.c
  src/wrapper/jddctmgr-8.c src/wrapper/jddctmgr-12.c
  src/wrapper/jddiffct-8.c src/wrapper/jddiffct-12.c src/wrapper/jddiffct-16.c
  src/jdhuff.c src/jdicc.c src/jdinput.c src/jdlhuff.c
  src/wrapper/jdlossls-8.c src/wrapper/jdlossls-12.c src/wrapper/jdlossls-16.c
  src/wrapper/jdmainct-8.c src/wrapper/jdmainct-12.c src/wrapper/jdmainct-16.c
  src/jdmarker.c src/jdmaster.c
  src/wrapper/jdmerge-8.c src/wrapper/jdmerge-12.c
  src/jdphuff.c
  src/wrapper/jdpostct-8.c src/wrapper/jdpostct-12.c src/wrapper/jdpostct-16.c
  src/wrapper/jdsample-8.c src/wrapper/jdsample-12.c src/wrapper/jdsample-16.c
  src/jdtrans.c src/jerror.c src/jfdctflt.c
  src/wrapper/jfdctfst-8.c src/wrapper/jfdctfst-12.c
  src/wrapper/jfdctint-8.c src/wrapper/jfdctint-12.c
  src/wrapper/jidctflt-8.c src/wrapper/jidctflt-12.c
  src/wrapper/jidctfst-8.c src/wrapper/jidctfst-12.c
  src/wrapper/jidctint-8.c src/wrapper/jidctint-12.c
  src/wrapper/jidctred-8.c src/wrapper/jidctred-12.c
  src/jmemmgr.c src/jmemnobs.c src/jpeg_nbits.c
  src/wrapper/jquant1-8.c src/wrapper/jquant1-12.c
  src/wrapper/jquant2-8.c src/wrapper/jquant2-12.c
  src/wrapper/jutils-8.c src/wrapper/jutils-12.c src/wrapper/jutils-16.c)
if(WITH_PROFILE)
  set(JPEG_SOURCES ${JPEG_SOURCES} src/tjutil.c)
endif()

if(WITH_ARITH_ENC OR WITH_ARITH_DEC)
  set(JPEG_SOURCES ${JPEG_SOURCES} src/jaricom.c)
endif()

if(WITH_ARITH_ENC)
  set(JPEG_SOURCES ${JPEG_SOURCES} src/jcarith.c)
endif()

if(WITH_ARITH_DEC)
  set(JPEG_SOURCES ${JPEG_SOURCES} src/jdarith.c)
endif()

if(WITH_SIMD)
  add_subdirectory(simd)
else()
  message(STATUS "SIMD extensions: None (WITH_SIMD = ${WITH_SIMD})")
  set(SIMD_ARCHITECTURE NONE)
endif()

# We have to generate these here, because if the build system tries and fails
# to enable the SIMD extensions, the value of WITH_SIMD will have changed.
configure_file(src/jconfig.h.in jconfig.h)
configure_file(src/jconfigint.h.in jconfigint.h)

if(WITH_SIMD)
  message(STATUS "SIMD extensions: ${SIMD_ARCHITECTURE} (WITH_SIMD = ${WITH_SIMD})")
  if(MSVC_IDE OR XCODE)
    set_source_files_properties(${SIMD_OBJS} PROPERTIES GENERATED 1)
  endif()
  set(SIMD_TARGET_OBJECTS $<TARGET_OBJECTS:simd>)
endif()

if(WITH_JNA)
  add_subdirectory(jna)
endif()

if(WITH_TURBOJPEG OR WITH_TOOLS OR WITH_TESTS)
  option(WITH_SYSTEM_SPNG
    "Build ${CMAKE_PROJECT_NAME} against the system-supplied libspng implementation rather than the implementation in the ${CMAKE_PROJECT_NAME} source tree"
    OFF)
  boolean_number(WITH_SYSTEM_SPNG)
  report_option(WITH_SYSTEM_SPNG "System-supplied libspng")
  if(WITH_SYSTEM_SPNG)
    include(FindPkgConfig)
    pkg_check_modules(spng REQUIRED spng IMPORTED_TARGET)
    unset(SPNG_STATIC_SOURCES)
    set(SPNG_LIBRARY PkgConfig::spng)
  else()
    option(WITH_SYSTEM_ZLIB
      "Build ${CMAKE_PROJECT_NAME} against the system-supplied zlib implementation rather than the implementation in the ${CMAKE_PROJECT_NAME} source tree"
      OFF)
    boolean_number(WITH_SYSTEM_ZLIB)
    report_option(WITH_SYSTEM_ZLIB "System-supplied zlib")
    add_subdirectory(src/spng)
    set(SPNG_STATIC_SOURCES $<TARGET_OBJECTS:spng-static>)
    if(UNIX)
      set(SPNG_LIBRARY m)
    else()
      unset(SPNG_LIBRARY)
    endif()
  endif()
  if(WITH_SYSTEM_ZLIB OR WITH_SYSTEM_SPNG)
    find_package(ZLIB REQUIRED)
    unset(ZLIB_STATIC_SOURCES)
    set(ZLIB_LIBRARY ZLIB::ZLIB)
  else()
    set(ZLIB_STATIC_SOURCES $<TARGET_OBJECTS:zlib-static>)
    unset(ZLIB_LIBRARY)
  endif()
endif()

if(ENABLE_SHARED)
  add_subdirectory(sharedlib)
endif()

if(ENABLE_STATIC)
  add_library(jpeg-static STATIC ${JPEG_SOURCES} ${SIMD_TARGET_OBJECTS}
    ${SIMD_OBJS})
  if(NOT MSVC_LIKE)
    set_target_properties(jpeg-static PROPERTIES OUTPUT_NAME jpeg)
  endif()
endif()

if(WITH_TURBOJPEG)
  set(TURBOJPEG_SOURCES ${JPEG_SOURCES} ${SIMD_TARGET_OBJECTS} ${SIMD_OBJS}
    src/turbojpeg.c src/transupp.c src/jdatadst-tj.c src/jdatasrc-tj.c
    src/rdbmp.c
    src/wrapper/rdpng-8.c src/wrapper/rdpng-12.c src/wrapper/rdpng-16.c
    ${ZLIB_STATIC_SOURCES} ${SPNG_STATIC_SOURCES}
    src/wrapper/rdppm-8.c src/wrapper/rdppm-12.c src/wrapper/rdppm-16.c
    src/wrbmp.c
    src/wrapper/wrpng-8.c src/wrapper/wrpng-12.c src/wrapper/wrpng-16.c
    src/wrapper/wrppm-8.c src/wrapper/wrppm-12.c src/wrapper/wrppm-16.c)
  if(ENABLE_SHARED)
    set(TJMAPFILE ${CMAKE_CURRENT_SOURCE_DIR}/src/turbojpeg-mapfile)
    if(MSVC_LIKE)
      configure_file(${CMAKE_SOURCE_DIR}/win/turbojpeg.rc.in
        ${CMAKE_BINARY_DIR}/win/turbojpeg.rc)
      set(TURBOJPEG_SOURCES ${TURBOJPEG_SOURCES}
        ${CMAKE_BINARY_DIR}/win/turbojpeg.rc)
    endif()
    add_library(turbojpeg SHARED ${TURBOJPEG_SOURCES})
    if(SPNG_LIBRARY)
      target_link_libraries(turbojpeg ${SPNG_LIBRARY})
    endif()
    if(NOT WITH_SYSTEM_SPNG AND ZLIB_LIBRARY)
      target_link_libraries(turbojpeg ${ZLIB_LIBRARY})
    endif()
    set_property(TARGET turbojpeg PROPERTY COMPILE_FLAGS
      "-DBMP_SUPPORTED -DPNG_SUPPORTED -DSPNG_STATIC -DPPM_SUPPORTED")
    if(WIN32)
      set_target_properties(turbojpeg PROPERTIES DEFINE_SYMBOL DLLDEFINE)
    endif()
    if(MINGW)
      set_target_properties(turbojpeg PROPERTIES LINK_FLAGS -Wl,--kill-at)
    endif()
    if(APPLE AND (NOT CMAKE_OSX_DEPLOYMENT_TARGET OR
                  CMAKE_OSX_DEPLOYMENT_TARGET VERSION_GREATER 10.4))
      if(NOT CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG)
        set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG "-Wl,-rpath,")
      endif()
      set_target_properties(turbojpeg PROPERTIES MACOSX_RPATH 1)
    endif()
    set_target_properties(turbojpeg PROPERTIES
      SOVERSION ${TURBOJPEG_SO_MAJOR_VERSION} VERSION ${TURBOJPEG_SO_VERSION})
    if(TJMAPFLAG)
      set_target_properties(turbojpeg PROPERTIES
        LINK_FLAGS "${TJMAPFLAG}${TJMAPFILE}")
    endif()

    if(WITH_TESTS)
      add_executable(tjunittest src/tjunittest.c src/tjutil.c src/md5/md5.c
        src/md5/md5hl.c)
      target_link_libraries(tjunittest turbojpeg)

      add_executable(tjcomp src/tjcomp.c)
      target_link_libraries(tjcomp turbojpeg)

      add_executable(tjdecomp src/tjdecomp.c)
      target_link_libraries(tjdecomp turbojpeg)

      add_executable(tjtran src/tjtran.c)
      target_link_libraries(tjtran turbojpeg)
    endif()

    if(WITH_TOOLS)
      add_executable(tjbench src/tjbench.c src/tjutil.c)
      target_link_libraries(tjbench turbojpeg)
      if(UNIX)
        target_link_libraries(tjbench m)
      endif()
    endif()

    add_custom_target(tjdoc COMMAND doxygen -s ../doc/doxygen.config
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src)
  endif()

  if(ENABLE_STATIC)
    add_library(turbojpeg-static STATIC ${TURBOJPEG_SOURCES})
    if(SPNG_LIBRARY)
      target_link_libraries(turbojpeg-static ${SPNG_LIBRARY})
    endif()
    if(NOT WITH_SYSTEM_SPNG AND ZLIB_LIBRARY)
      target_link_libraries(turbojpeg-static ${ZLIB_LIBRARY})
    endif()
    set_property(TARGET turbojpeg-static PROPERTY COMPILE_FLAGS
      "-DBMP_SUPPORTED -DPNG_SUPPORTED -DSPNG_STATIC -DPPM_SUPPORTED")
    if(NOT MSVC_LIKE)
      set_target_properties(turbojpeg-static PROPERTIES OUTPUT_NAME turbojpeg)
    endif()

    if(WITH_TESTS)
      add_executable(tjunittest-static src/tjunittest.c src/tjutil.c
        src/md5/md5.c src/md5/md5hl.c)
      target_link_libraries(tjunittest-static turbojpeg-static)
    endif()

    if(WITH_TOOLS)
      add_executable(tjbench-static src/tjbench.c src/tjutil.c)
      target_link_libraries(tjbench-static turbojpeg-static)
      if(UNIX)
        target_link_libraries(tjbench-static m)
      endif()
    endif()
  endif()
endif()

if(WITH_TOOLS)
  if(WIN32)
    set(USE_SETMODE "-DUSE_SETMODE")
  endif()
  set(CDJPEG_COMPILE_FLAGS
    "-DBMP_SUPPORTED -DGIF_SUPPORTED -DPNG_SUPPORTED -DSPNG_STATIC -DPPM_SUPPORTED -DTARGA_SUPPORTED ${USE_SETMODE}")
endif()

if(ENABLE_STATIC)
  if(WITH_TOOLS)
    add_executable(cjpeg-static src/cjpeg.c src/cdjpeg.c src/rdbmp.c
      src/rdgif.c
      src/wrapper/rdpng-8.c src/wrapper/rdpng-12.c src/wrapper/rdpng-16.c
      ${ZLIB_STATIC_SOURCES} ${SPNG_STATIC_SOURCES}
      src/wrapper/rdppm-8.c src/wrapper/rdppm-12.c src/wrapper/rdppm-16.c
      src/rdswitch.c src/rdtarga.c)
    set_property(TARGET cjpeg-static PROPERTY COMPILE_FLAGS
      ${CDJPEG_COMPILE_FLAGS})
    target_link_libraries(cjpeg-static jpeg-static)
    if(SPNG_LIBRARY)
      target_link_libraries(cjpeg-static ${SPNG_LIBRARY})
    endif()
    if(NOT WITH_SYSTEM_SPNG AND ZLIB_LIBRARY)
      target_link_libraries(cjpeg-static ${ZLIB_LIBRARY})
    endif()

    add_executable(djpeg-static src/djpeg.c src/cdjpeg.c
      src/wrapper/rdcolmap-8.c src/wrapper/rdcolmap-12.c
      src/rdswitch.c src/wrbmp.c
      src/wrapper/wrgif-8.c src/wrapper/wrgif-12.c
      src/wrapper/wrpng-8.c src/wrapper/wrpng-12.c src/wrapper/wrpng-16.c
      ${ZLIB_STATIC_SOURCES} ${SPNG_STATIC_SOURCES}
      src/wrapper/wrppm-8.c src/wrapper/wrppm-12.c src/wrapper/wrppm-16.c
      src/wrtarga.c)
    set_property(TARGET djpeg-static PROPERTY COMPILE_FLAGS
      ${CDJPEG_COMPILE_FLAGS})
    target_link_libraries(djpeg-static jpeg-static)
    if(SPNG_LIBRARY)
      target_link_libraries(djpeg-static ${SPNG_LIBRARY})
    endif()
    if(NOT WITH_SYSTEM_SPNG AND ZLIB_LIBRARY)
      target_link_libraries(djpeg-static ${ZLIB_LIBRARY})
    endif()

    add_executable(jpegtran-static src/jpegtran.c src/cdjpeg.c src/rdswitch.c
      src/transupp.c)
    target_link_libraries(jpegtran-static jpeg-static)
    set_property(TARGET jpegtran-static PROPERTY COMPILE_FLAGS
      "${USE_SETMODE}")
  endif()

  if(WITH_TESTS)
    add_executable(example-static src/example.c)
    target_link_libraries(example-static jpeg-static)

    include(CheckFunctionExists)
    check_function_exists(fmemopen HAVE_FMEMOPEN)
    if(HAVE_FMEMOPEN)
      add_executable(bmpsizetest-static src/bmpsizetest.c
        src/rdbmp.c src/wrbmp.c
        src/rdgif.c src/wrapper/wrgif-8.c src/wrapper/wrgif-12.c
        ${ZLIB_STATIC_SOURCES} ${SPNG_STATIC_SOURCES}
        src/wrapper/rdpng-8.c src/wrapper/rdpng-12.c src/wrapper/rdpng-16.c
        src/wrapper/wrpng-8.c src/wrapper/wrpng-12.c src/wrapper/wrpng-16.c
        src/wrapper/rdppm-8.c src/wrapper/rdppm-12.c src/wrapper/rdppm-16.c
        src/wrapper/wrppm-8.c src/wrapper/wrppm-12.c src/wrapper/wrppm-16.c
        src/rdtarga.c src/wrtarga.c)
      set_property(TARGET bmpsizetest-static PROPERTY COMPILE_FLAGS
        ${CDJPEG_COMPILE_FLAGS})
      target_link_libraries(bmpsizetest-static jpeg-static)
      if(SPNG_LIBRARY)
        target_link_libraries(bmpsizetest-static ${SPNG_LIBRARY})
      endif()
      if(ZLIB_LIBRARY)
        target_link_libraries(bmpsizetest-static ${ZLIB_LIBRARY})
      endif()
    endif()
  endif()
endif()

if(WITH_TOOLS)
  add_executable(rdjpgcom src/rdjpgcom.c)

  add_executable(wrjpgcom src/wrjpgcom.c)
endif()


###############################################################################
# TESTS
###############################################################################

if(WITH_FUZZ)
  add_definitions(-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
  add_subdirectory(fuzz)
endif()

if(WITH_TESTS)

add_executable(strtest src/strtest.c)

add_subdirectory(src/md5)

if(GENERATOR_IS_MULTI_CONFIG)
  set(OBJDIR "\${CTEST_CONFIGURATION_TYPE}/")
else()
  set(OBJDIR "")
endif()

enable_testing()

if(WITH_JNA)
  if(WIN32)
    set(CLASSPATH "jna/turbojpeg-jna.jar\\;${WITH_JNA}")
  else()
    set(CLASSPATH "jna/turbojpeg-jna.jar:${WITH_JNA}")
  endif()
  add_test(NAME TJUnitTest
    COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
      -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      -cp ${CLASSPATH} TJUnitTest)
  add_test(NAME TJUnitTest-alloc
    COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
      -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      -cp ${CLASSPATH} TJUnitTest -alloc)
  add_test(NAME TJUnitTest-yuv
    COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
      -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      -cp ${CLASSPATH} TJUnitTest -yuv)
  add_test(NAME TJUnitTest-yuv-alloc
    COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
      -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      -cp ${CLASSPATH} TJUnitTest -yuv -alloc)
  add_test(NAME TJUnitTest-yuv-nopad
    COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
      -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      -cp ${CLASSPATH} TJUnitTest -yuv -noyuvpad)
  add_test(NAME TJUnitTest-lossless
    COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
      -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      -cp ${CLASSPATH} TJUnitTest -lossless)
  add_test(NAME TJUnitTest-lossless-alloc
    COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
      -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      -cp ${CLASSPATH} TJUnitTest -lossless -alloc)
  add_test(NAME TJUnitTest-bmp
    COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
      -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      -cp ${CLASSPATH} TJUnitTest -bmp)
  add_test(NAME TJUnitTest12
    COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
      -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      -cp ${CLASSPATH} TJUnitTest -precision 12)
  add_test(NAME TJUnitTest12-alloc
    COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
      -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      -cp ${CLASSPATH} TJUnitTest -precision 12 -alloc)
  foreach(sample_bits 2 3 4 5 6 7 9 10 11 12 13 14 15 16)
    add_test(NAME TJUnitTest${sample_bits}-lossless
      COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
        -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
        -cp ${CLASSPATH}
        TJUnitTest -precision ${sample_bits} -lossless)
    add_test(NAME TJUnitTest${sample_bits}-lossless-alloc
      COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
        -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
        -cp ${CLASSPATH}
        TJUnitTest -precision ${sample_bits} -lossless -alloc)
    add_test(NAME TJUnitTest${sample_bits}-bmp
      COMMAND ${Java_JAVA_EXECUTABLE} ${JAVAARGS}
        -Djna.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
        -cp ${CLASSPATH}
        TJUnitTest -precision ${sample_bits} -bmp)
  endforeach()
endif()

set(TEST_LIBTYPES "")
if(ENABLE_SHARED)
  set(TEST_LIBTYPES ${TEST_LIBTYPES} shared)
endif()
if(ENABLE_STATIC)
  set(TEST_LIBTYPES ${TEST_LIBTYPES} static)
endif()

set(TESTIMAGES ${CMAKE_CURRENT_SOURCE_DIR}/testimages)
if(CMAKE_CROSSCOMPILING)
  file(RELATIVE_PATH TESTIMAGES ${CMAKE_CURRENT_BINARY_DIR} ${TESTIMAGES})
endif()

# The output of the floating point DCT/IDCT algorithms differs depending on the
# type of floating point math used, so the FLOATTEST8 and FLOATTEST12 CMake
# variables must be set in order to tell the testing system which floating
# point results it should expect:
#
# sse = validate against the expected results from the libjpeg-turbo SSE SIMD
#       extensions
# no-fp-contract = validate against the expected results from the C code when
#                  floating point expression contraction is disabled (the
#                  default with Clang 13 and earlier, when building for
#                  platforms that lack fused multiply-add [FMA] instructions,
#                  or when passing -ffp-contract=off to GCC or Clang)
# fp-contract = validate against the expected results from the C code when
#               floating point expression contraction is enabled (the default
#               with Clang 14 and later, with GCC when building for platforms
#               that have fused multiply-add [FMA] instructions, or when
#               passing -ffp-contract=fast to GCC or -ffp-contract=on to Clang)
# 387 = validate against the expected results from the C code when the 387 FPU
#       is being used for floating point math (which is generally the default
#       with x86 compilers)
# msvc = validate against the expected results from the C code when compiled
#        with a 32-bit version of Visual C++ and /arch:IA32 (which was the
#        default in Visual C++ 2010 and earlier)

if(CPU_TYPE STREQUAL "x86_64" OR CPU_TYPE STREQUAL "i386")
  if(WITH_SIMD)
    set(DEFAULT_FLOATTEST8 sse)
  elseif(CPU_TYPE STREQUAL "x86_64")
    set(DEFAULT_FLOATTEST8 no-fp-contract)
  endif()
elseif(CPU_TYPE STREQUAL "powerpc" OR CPU_TYPE STREQUAL "arm64")
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    if(CMAKE_C_COMPILER_ID MATCHES "AppleClang")
      # Xcode 14.3 and later
      set(MIN_FP_CONTRACT_VERSION 14.0.1)
    else()
      set(MIN_FP_CONTRACT_VERSION 14.0.0)
    endif()
    if(CMAKE_C_COMPILER_VERSION VERSION_EQUAL ${MIN_FP_CONTRACT_VERSION} OR
      CMAKE_C_COMPILER_VERSION VERSION_GREATER ${MIN_FP_CONTRACT_VERSION})
      set(DEFAULT_FLOATTEST8 fp-contract)
    else()
      set(DEFAULT_FLOATTEST8 no-fp-contract)
    endif()
  elseif(CMAKE_COMPILER_IS_GNUCC)
    set(DEFAULT_FLOATTEST8 fp-contract)
  endif()
# else we can't really set an intelligent default for FLOATTEST8.  The
# appropriate value could be no-fp-contract, fp-contract, 387, or msvc,
# depending on the compiler and compiler options.  We leave it to the user to
# set FLOATTEST8 manually.
endif()

# This causes FLOATTEST8 to reset to the default value if WITH_SIMD has
# changed.
if(DEFINED WITH_SIMD_INT AND NOT WITH_SIMD EQUAL WITH_SIMD_INT)
  set(FORCE_FLOATTEST8 "FORCE")
endif()
set(WITH_SIMD_INT ${WITH_SIMD} CACHE INTERNAL "")
set(FLOATTEST8 ${DEFAULT_FLOATTEST8} CACHE STRING
  "The type of floating point math used by the 8-bit-per-sample floating point DCT/IDCT algorithms.  This tells the testing system which numerical results it should expect from those tests.  [sse = ${CMAKE_PROJECT_NAME} x86/x86-64 SIMD extensions, no-fp-contract = generic FPU with floating point expression contraction disabled, fp-contract = generic FPU with floating point expression contraction enabled, 387 = 387 FPU, msvc = 32-bit Visual Studio + 387 FPU] (default = ${DEFAULT_FLOATTEST8})"
  ${FORCE_FLOATTEST8})
message(STATUS "FLOATTEST8 = ${FLOATTEST8}")

if(FLOATTEST8)
  string(TOUPPER ${FLOATTEST8} FLOATTEST8_UC)
  string(REGEX REPLACE "-" "_" FLOATTEST8_UC ${FLOATTEST8_UC})
  string(TOLOWER ${FLOATTEST8} FLOATTEST8)
  if(NOT FLOATTEST8 STREQUAL "sse" AND
    NOT FLOATTEST8 STREQUAL "no-fp-contract" AND
    NOT FLOATTEST8 STREQUAL "fp-contract" AND NOT FLOATTEST8 STREQUAL "387" AND
    NOT FLOATTEST8 STREQUAL "msvc")
    message(FATAL_ERROR "\"${FLOATTEST8}\" is not a valid value for FLOATTEST8.")
  endif()
endif()

if(CPU_TYPE STREQUAL "x86_64")
  set(DEFAULT_FLOATTEST12 no-fp-contract)
elseif(CPU_TYPE STREQUAL "powerpc" OR CPU_TYPE STREQUAL "arm64")
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    if(CMAKE_C_COMPILER_ID MATCHES "AppleClang")
      # Xcode 14.3 and later
      set(MIN_FP_CONTRACT_VERSION 14.0.1)
    else()
      set(MIN_FP_CONTRACT_VERSION 14.0.0)
    endif()
    if(CMAKE_C_COMPILER_VERSION VERSION_EQUAL ${MIN_FP_CONTRACT_VERSION} OR
      CMAKE_C_COMPILER_VERSION VERSION_GREATER ${MIN_FP_CONTRACT_VERSION})
      set(DEFAULT_FLOATTEST12 fp-contract)
    else()
      set(DEFAULT_FLOATTEST12 no-fp-contract)
    endif()
  elseif(CMAKE_COMPILER_IS_GNUCC)
    set(DEFAULT_FLOATTEST12 fp-contract)
  endif()
# else we can't really set an intelligent default for FLOATTEST12.  The
# appropriate value could be no-fp-contract, fp-contract, or something else,
# depending on the compiler and compiler options.  We leave it to the user to
# set FLOATTEST12 manually.
endif()

set(FLOATTEST12 ${DEFAULT_FLOATTEST12} CACHE STRING
  "The type of floating point math used by the 12-bit-per-sample floating point DCT/IDCT algorithms.  This tells the testing system which numerical results it should expect from those tests.  [sse = ${CMAKE_PROJECT_NAME} x86/x86-64 SIMD extensions, no-fp-contract = generic FPU with floating point expression contraction disabled, fp-contract = generic FPU with floating point expression contraction enabled, 387 = 387 FPU, msvc = 32-bit Visual Studio + 387 FPU] (default = ${DEFAULT_FLOATTEST12})")
message(STATUS "FLOATTEST12 = ${FLOATTEST12}")

if(FLOATTEST12)
  string(TOUPPER ${FLOATTEST12} FLOATTEST12_UC)
  string(REGEX REPLACE "-" "_" FLOATTEST12_UC ${FLOATTEST12_UC})
  string(TOLOWER ${FLOATTEST12} FLOATTEST12)
  if(NOT FLOATTEST12 STREQUAL "sse" AND
    NOT FLOATTEST12 STREQUAL "no-fp-contract" AND
    NOT FLOATTEST12 STREQUAL "fp-contract" AND
    NOT FLOATTEST12 STREQUAL "387" AND
    NOT FLOATTEST12 STREQUAL "msvc")
    message(FATAL_ERROR "\"${FLOATTEST12}\" is not a valid value for FLOATTEST12.")
  endif()
endif()

foreach(libtype ${TEST_LIBTYPES})
  if(libtype STREQUAL "static")
    set(suffix -static)
  endif()
  if(WITH_TURBOJPEG)
    add_test(NAME tjunittest-${libtype}
      COMMAND tjunittest${suffix})
    add_test(NAME tjunittest-${libtype}-alloc
      COMMAND tjunittest${suffix} -alloc)
    add_test(NAME tjunittest-${libtype}-yuv
      COMMAND tjunittest${suffix} -yuv)
    add_test(NAME tjunittest-${libtype}-yuv-alloc
      COMMAND tjunittest${suffix} -yuv -alloc)
    add_test(NAME tjunittest-${libtype}-yuv-nopad
      COMMAND tjunittest${suffix} -yuv -noyuvpad)
    add_test(NAME tjunittest-${libtype}-lossless
      COMMAND tjunittest${suffix} -lossless)
    add_test(NAME tjunittest-${libtype}-lossless-alloc
      COMMAND tjunittest${suffix} -lossless -alloc)
    add_test(NAME tjunittest-${libtype}-bmp
      COMMAND tjunittest${suffix} -bmp)
    add_test(NAME tjunittest12-${libtype}
      COMMAND tjunittest${suffix} -precision 12)
    add_test(NAME tjunittest12-${libtype}-alloc
      COMMAND tjunittest${suffix} -precision 12 -alloc)
    foreach(sample_bits 2 3 4 5 6 7 9 10 11 12 13 14 15 16)
      add_test(NAME tjunittest${sample_bits}-${libtype}-lossless
        COMMAND tjunittest${suffix} -precision ${sample_bits} -lossless)
      add_test(NAME tjunittest${sample_bits}-${libtype}-lossless-alloc
        COMMAND tjunittest${suffix} -precision ${sample_bits} -lossless -alloc)
      add_test(NAME tjunittest${sample_bits}-${libtype}-bmp
        COMMAND tjunittest${suffix} -precision ${sample_bits} -bmp)
    endforeach()

    foreach(sample_bits 8 12)

      if(sample_bits EQUAL 12)
        set(tjbench tjbench12)
        set(testout testout12${suffix})
        set(SRCIMG ${TESTIMAGES}/monkey16.png)
        set(ext png)
        set(EXT PNG)

        set(MD5_PNG_GRAY_TILE 999a5fd03e8bb1e0b32c9ccb29cec846)
        set(MD5_PNG_420_8x8_TILE 0c921be3a229b3376f5e671928c741ff)
        set(MD5_PNG_420_16x16_TILE 5e0d2e2e69ca0d1ad97797791ff1a30b)
        set(MD5_PNG_420_32x32_TILE 4949016568cf8aecc08b1af5be822bd1)
        set(MD5_PNG_420_64x64_TILE 6df139058c430b34e44b346bc5899706)
        set(MD5_PNG_420_128x128_TILE 0b42e6aca0bd21b2c5846d34e652b2b6)
        set(MD5_PNG_420M_8x8_TILE 540bea1d82d6964dab66003ca12823e0)
        set(MD5_PNG_420M_TILE 825368d2d974eed4111f25b0854643d8)
        set(MD5_PNG_422_8x8_TILE f39d59bae49dd10a8532f4ab943f3e5a)
        set(MD5_PNG_422_16x16_TILE f4cc229c2ceefa33052d0ca98ff79855)
        set(MD5_PNG_422_32x32_TILE 2d25e57eff9e316409523740def79876)
        set(MD5_PNG_422_64x64_TILE a6b531c0899653baff5e20a2484a8bfa)
        set(MD5_PNG_422_128x128_TILE 579ead8169a4e7144d5c3b624f4d1cc3)
        set(MD5_PNG_422M_8x8_TILE 70920ea8f550524fb684cefb23dfeb4d)
        set(MD5_PNG_422M_TILE ba85405622ea0449f33ddb5d1892853f)
        set(MD5_PNG_444_TILE 0dffd24a3a64c945139960cc0f553272)
      else()
        set(tjbench tjbench)
        set(testout testout${suffix})
        set(SRCIMG ${TESTIMAGES}/testorig.ppm)
        set(ext ppm)
        set(EXT PPM)

        set(MD5_PPM_GRAY_TILE 2c3b567086e6ca0c5e6d34ad8d6f6fe8)
        set(MD5_PPM_420_8x8_TILE efca1bdf0226df01777137778cf986ec)
        set(MD5_PPM_420_16x16_TILE 8c92c7453870d9e11c6d1dec3a8c9101)
        set(MD5_PPM_420_32x32_TILE 3f7651872a95e469d1c7115f1b11ecef)
        set(MD5_PPM_420_64x64_TILE f64c71af03fdea12363b62f1a3096aab)
        set(MD5_PPM_420_128x128_TILE 5a5ef57517558c671bf5e75793588d69)
        set(MD5_PPM_420M_8x8_TILE 66bd869b315a32a00fef1a025661ce72)
        set(MD5_PPM_420M_TILE bf9ec2ab4875abb2efcce8f876fe2c2a)
        set(MD5_PPM_422_8x8_TILE c300553ce1b3b90fd414ec96b62fe988)
        set(MD5_PPM_422_16x16_TILE 6559ddb1191f5b2d3eb41081b254c4e0)
        set(MD5_PPM_422_32x32_TILE 58691797f4584c4c5ed5965a6bb9aec0)
        set(MD5_PPM_422_64x64_TILE 7f9e34942ae46af7b784f459ec133f5e)
        set(MD5_PPM_422_128x128_TILE 6afcb77580d85dd3eacb04b3c2bc7710)
        set(MD5_PPM_422M_8x8_TILE 55df1f96bcfb631aedeb940cf3f011f5)
        set(MD5_PPM_422M_TILE 6502031018c2d2f69bc6353347f8df4d)
        set(MD5_PPM_444_TILE 87bd58005eec73f0f313c8e38d0d793c)
      endif()

      # Test compressing from/decompressing to an arbitrary subregion of a
      # larger image buffer
      add_test(NAME ${tjbench}-${libtype}-tile-cp
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${SRCIMG} ${testout}_tile.${ext})
      add_test(NAME ${tjbench}-${libtype}-tile
        COMMAND tjbench${suffix} ${testout}_tile.${ext} 95
          -precision ${sample_bits} -rgb -quiet -tile -benchtime 0.01
          -warmup 0)
      set_tests_properties(${tjbench}-${libtype}-tile
        PROPERTIES DEPENDS ${tjbench}-${libtype}-tile-cp)

      foreach(tile 8 16 32 64 128)
        add_test(NAME ${tjbench}-${libtype}-tile-gray-${tile}x${tile}-cmp
          COMMAND md5cmp ${MD5_${EXT}_GRAY_TILE}
            ${testout}_tile_GRAY_Q95_${tile}x${tile}.${ext})
        foreach(subsamp 420 422)
          add_test(NAME ${tjbench}-${libtype}-tile-${subsamp}-${tile}x${tile}-cmp
            COMMAND md5cmp ${MD5_${EXT}_${subsamp}_${tile}x${tile}_TILE}
              ${testout}_tile_${subsamp}_Q95_${tile}x${tile}.${ext})
        endforeach()
        add_test(NAME ${tjbench}-${libtype}-tile-444-${tile}x${tile}-cmp
          COMMAND md5cmp ${MD5_${EXT}_444_TILE}
            ${testout}_tile_444_Q95_${tile}x${tile}.${ext})
        foreach(subsamp gray 420 422 444)
          set_tests_properties(
            ${tjbench}-${libtype}-tile-${subsamp}-${tile}x${tile}-cmp
            PROPERTIES DEPENDS ${tjbench}-${libtype}-tile)
        endforeach()
      endforeach()

      add_test(NAME ${tjbench}-${libtype}-tilem-cp
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${SRCIMG} ${testout}_tilem.${ext})
      add_test(NAME ${tjbench}-${libtype}-tilem
        COMMAND tjbench${suffix} ${testout}_tilem.${ext} 95
          -precision ${sample_bits} -rgb -fastupsample -quiet -tile
          -benchtime 0.01 -warmup 0)
      set_tests_properties(${tjbench}-${libtype}-tilem
        PROPERTIES DEPENDS ${tjbench}-${libtype}-tilem-cp)

      add_test(NAME ${tjbench}-${libtype}-tile-420m-8x8-cmp
        COMMAND md5cmp ${MD5_${EXT}_420M_8x8_TILE}
          ${testout}_tilem_420_Q95_8x8.${ext})
      add_test(NAME ${tjbench}-${libtype}-tile-422m-8x8-cmp
        COMMAND md5cmp ${MD5_${EXT}_422M_8x8_TILE}
          ${testout}_tilem_422_Q95_8x8.${ext})
      foreach(tile 16 32 64 128)
        foreach(subsamp 420 422)
          add_test(NAME ${tjbench}-${libtype}-tile-${subsamp}m-${tile}x${tile}-cmp
            COMMAND md5cmp ${MD5_${EXT}_${subsamp}M_TILE}
              ${testout}_tilem_${subsamp}_Q95_${tile}x${tile}.${ext})
        endforeach()
      endforeach()
      foreach(tile 8 16 32 64 128)
        foreach(subsamp 420 422)
          set_tests_properties(
            ${tjbench}-${libtype}-tile-${subsamp}m-${tile}x${tile}-cmp
            PROPERTIES DEPENDS ${tjbench}-${libtype}-tilem)
        endforeach()
      endforeach()

    endforeach()

  endif()

  if(UNIX)
    add_test(NAME bmpsizetest-${libtype} COMMAND bmpsizetest${suffix})
  endif()

  # These tests are carefully crafted to provide full coverage of as many of
  # the underlying algorithms as possible (including all of the
  # SIMD-accelerated ones.)

  macro(add_bittest PROG NAME ARGS OUTFILE INFILE MD5SUM)
    if(${PROG} STREQUAL "cjpeg16")
      set(ACTUAL_ARGS "${ARGS};-precision;16")
    elseif(${PROG} STREQUAL "cjpeg12")
      set(ACTUAL_ARGS "${ARGS};-precision;12")
    else()
      set(ACTUAL_ARGS ${ARGS})
    endif()
    string(REGEX REPLACE "16" "" ACTUAL_PROG ${PROG})
    string(REGEX REPLACE "12" "" ACTUAL_PROG ${ACTUAL_PROG})
    add_test(NAME ${PROG}-${libtype}-${NAME}
      COMMAND ${ACTUAL_PROG}${suffix} ${ACTUAL_ARGS} -outfile ${OUTFILE}
        ${INFILE})
    add_test(NAME ${PROG}-${libtype}-${NAME}-cmp
      COMMAND md5cmp ${MD5SUM} ${OUTFILE})
    set_tests_properties(${PROG}-${libtype}-${NAME}-cmp PROPERTIES
      DEPENDS ${PROG}-${libtype}-${NAME})
    if(${ARGC} GREATER 6)
      set(DEPENDS ${ARGN})
      set_tests_properties(${PROG}-${libtype}-${NAME} PROPERTIES
        DEPENDS ${DEPENDS})
    endif()
  endmacro()

  set(MD5_JPEG_LOSSLESS fdf975c44ddcbc86f144eb58f2417c97)
  set(MD5_PNG_LOSSLESS ae6de70fa4537456317631df3dab37c8)

  # Lossless (all arguments other than -lossless and -restart should have no
  # effect)
  add_bittest(cjpeg16 lossless
    "-lossless;4;-restart;1;-quality;1;-grayscale;-optimize;-dct;float;-smooth;100;-baseline;-qslots;1,0,0;-sample;1x2,3x4,2x1"
    testout16_lossless.jpg ${TESTIMAGES}/monkey16.png ${MD5_JPEG_LOSSLESS})
  add_bittest(djpeg16 lossless
    "-fast;-scale;1/8;-dct;float;-dither;none;-nosmooth;-onepass;-png"
    testout16_lossless.png testout16_lossless.jpg
    ${MD5_PNG_LOSSLESS} cjpeg16-${libtype}-lossless)

  foreach(sample_bits 8 12)

    if(sample_bits EQUAL 12)
      set(cjpeg cjpeg12)
      set(djpeg djpeg12)
      set(jpegtran jpegtran12)
      set(testout testout12${suffix})
      set(SRCIMG ${TESTIMAGES}/monkey16.png)
      set(ext png)
      set(EXT PNG)
      set(JPEGIMG ${TESTIMAGES}/monkey12.jpg)

      set(MD5_JPEG_RGB_ISLOW 412da084cd06bece1822dc7b92f01e1a)
      set(MD5_JPEG_RGB_ISLOW2 47d05e783674661f1c2c749e230d98b7)
      set(MD5_PNG_RGB_ISLOW ae3a3285a63b15f905951f1c6a8de6c2)
      set(MD5_JPEG_422_IFAST_OPT fd9d6d81531e33661de10495a0069001)
      set(MD5_PNG_422_IFAST 310d2ac448193a3d13b31832cee87b11)
      set(MD5_JPEG_440_ISLOW 3f8c31d8fef17c4ad92e5603116211c9)
      set(MD5_PNG_440_ISLOW 3e849499bbe903c0352ea4316ef9e136)
      set(MD5_PNG_422M_IFAST e41b8710266ef758d1b8c30ba322608d)
      set(MD5_JPEG_420_IFAST_Q100_PROG dafebba3616229772fcb157cc90efbc1)
      set(MD5_PNG_420_Q100_IFAST 4c4502a15a96bb3b8275b6d628475e66)
      set(MD5_PNG_420M_Q100_IFAST f63b8b627bc5e3df7fdee6c44824affe)
      set(MD5_JPEG_GRAY_ISLOW 4bd46c0943f1ba911437700b0a98bd56)
      set(MD5_PNG_GRAY_ISLOW 3d5c96b48535dcc5123bae2ec7ece092)
      set(MD5_PNG_GRAY_ISLOW_RGB 512ffa30491bdf5a58ae0204caee55c4)
      set(MD5_JPEG_420S_ISLOW_OPT 2b7de3331421c2e41cf6cd9b5ddcf8e0)

      set(MD5_JPEG_3x2_FLOAT_PROG_SSE 2fe2ef00578fd4d5f7c105e37090780c)
      set(MD5_PNG_3x2_FLOAT_SSE 68a9b2522b8d30bed42c2104028957cd)
      set(MD5_JPEG_3x2_FLOAT_PROG_NO_FP_CONTRACT
        ${MD5_JPEG_3x2_FLOAT_PROG_SSE})
      set(MD5_PNG_3x2_FLOAT_NO_FP_CONTRACT ${MD5_PNG_3x2_FLOAT_SSE})
      set(MD5_JPEG_3x2_FLOAT_PROG_FP_CONTRACT ${MD5_JPEG_3x2_FLOAT_PROG_SSE})
      if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(MD5_PNG_3x2_FLOAT_FP_CONTRACT ${MD5_PNG_3x2_FLOAT_SSE})
      else()
        set(MD5_PNG_3x2_FLOAT_FP_CONTRACT 824f3b074f5166e20313615e5b0a3b9c)
      endif()
      set(MD5_JPEG_3x2_FLOAT_PROG_387 5d203a595f448cf19ce86c9bb2c52dfe)
      set(MD5_PNG_3x2_FLOAT_387 05e50fa2fb929d335a144813cd97cd82)
      set(MD5_JPEG_3x2_FLOAT_PROG_MSVC af7f873ea14136271cc21ddbc4bee46c)
      set(MD5_PNG_3x2_FLOAT_MSVC 1790c395e3fbc8c60dbe503a8db3ffe7)

      set(MD5_JPEG_3x2_IFAST_PROG eb1ba1cb5656025dc2ba13d81bcd5a2d)
      set(MD5_PNG_3x2_IFAST def6c47bbfa0e6b797f8f326f6ef82c5)
      set(MD5_PNG_420M_ISLOW_2_1 9812e72849d2b18a25c3d0ee4a718cd6)
      set(MD5_PNG_420M_ISLOW_15_8 7b1bcb4ff1b3e123d98bde20d2bac816)
      set(MD5_PNG_420M_ISLOW_13_8 583f6d5a240ae7abdaae07a03c0de506)
      set(MD5_PNG_420M_ISLOW_11_8 b812cd1f78757b062582c7ddb5d4f5e2)
      set(MD5_PNG_420M_ISLOW_9_8 050747dff00afd2779f595ddd883ccbc)
      set(MD5_PNG_420M_ISLOW_7_8 66dce85a76a034f472e9e33c8cbad349)
      set(MD5_PNG_420M_ISLOW_3_4 a3c324540ef157fc43937516975af91c)
      set(MD5_PNG_420M_ISLOW_5_8 5edcf6ec475809766a639c02ace4323e)
      set(MD5_PNG_420M_ISLOW_1_2 131d3fe37905472aed58a2a73a146e3b)
      set(MD5_PNG_420M_ISLOW_3_8 75080741e2d9c33ebdaa73f1554b024b)
      set(MD5_PNG_420M_ISLOW_1_4 d76d67d6663df0aad9b9c839491ea64d)
      set(MD5_PNG_420M_ISLOW_1_8 37f958113630dce326d6bc26d7cd00fe)
      set(MD5_JPEG_LOSSLESS dacb841a16cc2e1140042d989bf6457e)
      set(MD5_PNG_LOSSLESS 60811cac46a04912c30ff1cad74c9f23)
      set(MD5_PNG_420_ISLOW_SKIP15_31 1c1737561a26e0104248654931b88307)
      set(MD5_PNG_420_ISLOW_PROG_CROP62x62_71_71
        5d46774035a03fc00d90fd7b107eca6f)
      set(MD5_PNG_444_ISLOW_SKIP1_6 f52ca3ef309bdcbab981e13f2b11cb0a)
      set(MD5_PNG_444_ISLOW_PROG_CROP98x98_13_13
        24c7a6cf12f7bfa57e12e7b0238add66)
      set(MD5_JPEG_CROP b816363e572435067ec23bfa390761b4)

      set(MD5_JPEG_EXAMPLE_COMPRESS 5e502da0c3c0f957a58c536f31e973dc)
      set(MD5_PPM_EXAMPLE_DECOMPRESS 49aac0119b87c4deb70e9a7daf151253)
    else()
      set(cjpeg cjpeg)
      set(djpeg djpeg)
      set(jpegtran jpegtran)
      set(testout testout${suffix})
      set(SRCIMG ${TESTIMAGES}/testorig.ppm)
      set(ext ppm)
      set(EXT PPM)
      set(JPEGIMG ${TESTIMAGES}/testorig.jpg)

      set(MD5_JPEG_RGB_ISLOW 1d44a406f61da743b5fd31c0a9abdca3)
      set(MD5_JPEG_RGB_ISLOW2 b811e5ad807beb9384e51ef6a47f715d)
      set(MD5_PPM_RGB_ISLOW 00a257f5393fef8821f2b88ac7421291)
      set(MD5_BMP_RGB_ISLOW_565 f07d2e75073e4bb10f6c6f4d36e2e3be)
      set(MD5_BMP_RGB_ISLOW_565D 4cfa0928ef3e6bb626d7728c924cfda4)
      set(MD5_JPEG_422_IFAST_OPT 2540287b79d913f91665e660303ab2c8)
      set(MD5_PPM_422_IFAST 35bd6b3f833bad23de82acea847129fa)
      set(MD5_JPEG_440_ISLOW 538bc02bd4b4658fd85de6ece6cbeda6)
      set(MD5_PPM_440_ISLOW 11e7eab7ef7ef3276934bb7e7b6bb377)
      set(MD5_PPM_422M_IFAST 8dbc65323d62cca7c91ba02dd1cfa81d)
      set(MD5_BMP_422M_IFAST_565 3294bd4d9a1f2b3d08ea6020d0db7065)
      set(MD5_BMP_422M_IFAST_565D da98c9c7b6039511be4a79a878a9abc1)
      set(MD5_JPEG_420_IFAST_Q100_PROG 0ba15f9dab81a703505f835f9dbbac6d)
      set(MD5_PPM_420_Q100_IFAST 5a732542015c278ff43635e473a8a294)
      set(MD5_PPM_420M_Q100_IFAST ff692ee9323a3b424894862557c092f1)
      set(MD5_JPEG_GRAY_ISLOW 72b51f894b8f4a10b3ee3066770aa38d)
      set(MD5_PPM_GRAY_ISLOW 8d3596c56eace32f205deccc229aa5ed)
      set(MD5_PPM_GRAY_ISLOW_RGB 116424ac07b79e5e801f00508eab48ec)
      set(MD5_BMP_GRAY_ISLOW_565 12f78118e56a2f48b966f792fedf23cc)
      set(MD5_BMP_GRAY_ISLOW_565D bdbbd616441a24354c98553df5dc82db)
      set(MD5_JPEG_420S_ISLOW_OPT 388708217ac46273ca33086b22827ed8)

      set(MD5_JPEG_3x2_FLOAT_PROG_SSE 343e3f8caf8af5986ebaf0bdc13b5c71)
      set(MD5_PPM_3x2_FLOAT_SSE 1a75f36e5904d6fc3a85a43da9ad89bb)
      set(MD5_JPEG_3x2_FLOAT_PROG_NO_FP_CONTRACT
        9bca803d2042bd1eb03819e2bf92b3e5)
      set(MD5_PPM_3x2_FLOAT_NO_FP_CONTRACT f6bfab038438ed8f5522fbd33595dcdc)
      set(MD5_JPEG_3x2_FLOAT_PROG_FP_CONTRACT
        ${MD5_JPEG_3x2_FLOAT_PROG_NO_FP_CONTRACT})
      if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(MD5_PPM_3x2_FLOAT_FP_CONTRACT ${MD5_PPM_3x2_FLOAT_NO_FP_CONTRACT})
      else()
        set(MD5_PPM_3x2_FLOAT_FP_CONTRACT 0e917a34193ef976b679a6b069b1be26)
      endif()
      set(MD5_JPEG_3x2_FLOAT_PROG_387 1657664a410e0822c924b54f6f65e6e9)
      set(MD5_PPM_3x2_FLOAT_387 cb0a1f027f3d2917c902b5640214e025)
      set(MD5_JPEG_3x2_FLOAT_PROG_MSVC 7999ce9cd0ee9b6c7043b7351ab7639d)
      set(MD5_PPM_3x2_FLOAT_MSVC 28cdc448a6b75e97892f0e0f8d4b21f3)

      set(MD5_JPEG_3x2_IFAST_PROG 1ee5d2c1a77f2da495f993c8c7cceca5)
      set(MD5_PPM_3x2_IFAST fd283664b3b49127984af0a7f118fccd)
      set(MD5_JPEG_420_ISLOW_ARI e986fb0a637a8d833d96e8a6d6d84ea1)
      set(MD5_JPEG_444_ISLOW_PROGARI 0a8f1c8f66e113c3cf635df0a475a617)
      set(MD5_PPM_420M_IFAST_ARI 57251da28a35b46eecb7177d82d10e0e)
      set(MD5_JPEG_420_ISLOW 9a68f56bc76e466aa7e52f415d0f4a5f)
      set(MD5_PPM_420M_ISLOW_2_1 9f9de8c0612f8d06869b960b05abf9c9)
      set(MD5_PPM_420M_ISLOW_15_8 b6875bc070720b899566cc06459b63b7)
      set(MD5_PPM_420M_ISLOW_13_8 bc3452573c8152f6ae552939ee19f82f)
      set(MD5_PPM_420M_ISLOW_11_8 d8cc73c0aaacd4556569b59437ba00a5)
      set(MD5_PPM_420M_ISLOW_9_8 d25e61bc7eac0002f5b393aa223747b6)
      set(MD5_PPM_420M_ISLOW_7_8 ddb564b7c74a09494016d6cd7502a946)
      set(MD5_PPM_420M_ISLOW_3_4 8ed8e68808c3fbc4ea764fc9d2968646)
      set(MD5_PPM_420M_ISLOW_5_8 a3363274999da2366a024efae6d16c9b)
      set(MD5_PPM_420M_ISLOW_1_2 e692a315cea26b988c8e8b29a5dbcd81)
      set(MD5_PPM_420M_ISLOW_3_8 79eca9175652ced755155c90e785a996)
      set(MD5_PPM_420M_ISLOW_1_4 79cd778f8bf1a117690052cacdd54eca)
      set(MD5_PPM_420M_ISLOW_1_8 391b3d4aca640c8567d6f8745eb2142f)
      set(MD5_BMP_420_ISLOW_256 4980185e3776e89bd931736e1cddeee6)
      set(MD5_BMP_420_ISLOW_565 bf9d13e16c4923b92e1faa604d7922cb)
      set(MD5_BMP_420_ISLOW_565D 6bde71526acc44bcff76f696df8638d2)
      set(MD5_BMP_420M_ISLOW_565 8dc0185245353cfa32ad97027342216f)
      set(MD5_BMP_420M_ISLOW_565D ce034037d212bc403330df6f915c161b)
      set(MD5_JPEG_LOSSLESS fc777b82d42d835ae1282ba1ee87c209)
      set(MD5_PPM_LOSSLESS 64072f1dbdc5b3a187777788604971a5)
      set(MD5_PPM_420_ISLOW_SKIP15_31 c4c65c1e43d7275cd50328a61e6534f0)
      set(MD5_PPM_420_ISLOW_ARI_SKIP16_139 087c6b123db16ac00cb88c5b590bb74a)
      set(MD5_PPM_420_ISLOW_PROG_CROP62x62_71_71
        26eb36ccc7d1f0cb80cdabb0ac8b5d99)
      set(MD5_PPM_420_ISLOW_ARI_CROP53x53_4_4 886c6775af22370257122f8b16207e6d)
      set(MD5_PPM_444_ISLOW_SKIP1_6 5606f86874cf26b8fcee1117a0a436a6)
      set(MD5_PPM_444_ISLOW_PROG_CROP98x98_13_13
        db87dc7ce26bcdc7a6b56239ce2b9d6c)
      set(MD5_PPM_444_ISLOW_ARI_CROP37x37_0_0 cb57b32bd6d03e35432362f7bf184b6d)
      set(MD5_JPEG_CROP b4197f377e621c4e9b1d20471432610d)

      set(MD5_JPEG_EXAMPLE_COMPRESS 95d4d72e2ef127332654c2599afb47bf)
      set(MD5_PPM_EXAMPLE_DECOMPRESS dea1d7bbc37e39adf628342c86096641)
    endif()

    # CC: null  SAMP: fullsize  FDCT: islow  ENT: huff
    add_bittest(${cjpeg} rgb-islow "-rgb;-dct;int;-icc;${TESTIMAGES}/test1.icc"
      ${testout}_rgb_islow.jpg ${SRCIMG}
      ${MD5_JPEG_RGB_ISLOW})

    # CC: null  SAMP: fullsize  IDCT: islow  ENT: huff
    add_bittest(${djpeg} rgb-islow
      "-dct;int;-${ext};-icc;${testout}_rgb_islow.icc"
      ${testout}_rgb_islow.${ext} ${testout}_rgb_islow.jpg
      ${MD5_${EXT}_RGB_ISLOW} ${cjpeg}-${libtype}-rgb-islow)

    add_test(NAME ${djpeg}-${libtype}-rgb-islow-icc-cmp
      COMMAND md5cmp b06a39d730129122e85c1363ed1bbc9e ${testout}_rgb_islow.icc)
    set_tests_properties(${djpeg}-${libtype}-rgb-islow-icc-cmp PROPERTIES
      DEPENDS ${djpeg}-${libtype}-rgb-islow)

    add_bittest(${jpegtran} icc "-copy;all;-icc;${TESTIMAGES}/test3.icc"
      ${testout}_rgb_islow2.jpg ${testout}_rgb_islow.jpg
      ${MD5_JPEG_RGB_ISLOW2} ${cjpeg}-${libtype}-rgb-islow)

    if(sample_bits EQUAL 8)
      # CC: RGB->RGB565  SAMP: fullsize  IDCT: islow  ENT: huff
      add_bittest(${djpeg} rgb-islow-565 "-dct;int;-rgb565;-dither;none;-bmp"
        ${testout}_rgb_islow_565.bmp ${testout}_rgb_islow.jpg
        ${MD5_BMP_RGB_ISLOW_565} ${cjpeg}-${libtype}-rgb-islow)

      # CC: RGB->RGB565 (dithered)  SAMP: fullsize  IDCT: islow  ENT: huff
      add_bittest(${djpeg} rgb-islow-565D "-dct;int;-rgb565;-bmp"
        ${testout}_rgb_islow_565D.bmp ${testout}_rgb_islow.jpg
        ${MD5_BMP_RGB_ISLOW_565D} ${cjpeg}-${libtype}-rgb-islow)
    endif()

    # CC: RGB->YCC  SAMP: fullsize/h2v1  FDCT: ifast  ENT: 2-pass huff
    add_bittest(${cjpeg} 422-ifast-opt "-sample;2x1;-dct;fast;-opt"
      ${testout}_422_ifast_opt.jpg ${SRCIMG}
      ${MD5_JPEG_422_IFAST_OPT})

    # CC: YCC->RGB  SAMP: fullsize/h2v1 fancy  IDCT: ifast  ENT: huff
    add_bittest(${djpeg} 422-ifast "-dct;fast;-${ext}"
      ${testout}_422_ifast.${ext} ${testout}_422_ifast_opt.jpg
      ${MD5_${EXT}_422_IFAST} ${cjpeg}-${libtype}-422-ifast-opt)

    # CC: RGB->YCC  SAMP: fullsize/h1v2  FDCT: islow  ENT: huff
    add_bittest(${cjpeg} 440-islow "-sample;1x2;-dct;int"
      ${testout}_440_islow.jpg ${SRCIMG}
      ${MD5_JPEG_440_ISLOW})

    # CC: YCC->RGB  SAMP: fullsize/h1v2 fancy  IDCT: islow  ENT: huff
    add_bittest(${djpeg} 440-islow "-dct;int;-${ext}"
      ${testout}_440_islow.${ext} ${testout}_440_islow.jpg
      ${MD5_${EXT}_440_ISLOW} ${cjpeg}-${libtype}-440-islow)

    # CC: YCC->RGB  SAMP: h2v1 merged  IDCT: ifast  ENT: huff
    add_bittest(${djpeg} 422m-ifast "-dct;fast;-nosmooth;-${ext}"
      ${testout}_422m_ifast.${ext} ${testout}_422_ifast_opt.jpg
      ${MD5_${EXT}_422M_IFAST} ${cjpeg}-${libtype}-422-ifast-opt)

    if(sample_bits EQUAL 8)
      # CC: YCC->RGB565  SAMP: h2v1 merged  IDCT: ifast  ENT: huff
      add_bittest(${djpeg} 422m-ifast-565
        "-dct;int;-nosmooth;-rgb565;-dither;none;-bmp"
        ${testout}_422m_ifast_565.bmp ${testout}_422_ifast_opt.jpg
        ${MD5_BMP_422M_IFAST_565} ${cjpeg}-${libtype}-422-ifast-opt)

      # CC: YCC->RGB565 (dithered)  SAMP: h2v1 merged  IDCT: ifast  ENT: huff
      add_bittest(${djpeg} 422m-ifast-565D "-dct;int;-nosmooth;-rgb565;-bmp"
        ${testout}_422m_ifast_565D.bmp ${testout}_422_ifast_opt.jpg
        ${MD5_BMP_422M_IFAST_565D} ${cjpeg}-${libtype}-422-ifast-opt)
    endif()

    # CC: RGB->YCC  SAMP: fullsize/h2v2  FDCT: ifast  ENT: prog huff
    add_bittest(${cjpeg} 420-q100-ifast-prog
      "-sample;2x2;-quality;100;-dct;fast;-scans;${TESTIMAGES}/test.scan"
      ${testout}_420_q100_ifast_prog.jpg ${SRCIMG}
      ${MD5_JPEG_420_IFAST_Q100_PROG})

    # CC: YCC->RGB  SAMP: fullsize/h2v2 fancy  IDCT: ifast  ENT: prog huff
    add_bittest(${djpeg} 420-q100-ifast-prog "-dct;fast;-${ext}"
      ${testout}_420_q100_ifast.${ext} ${testout}_420_q100_ifast_prog.jpg
      ${MD5_${EXT}_420_Q100_IFAST} ${cjpeg}-${libtype}-420-q100-ifast-prog)

    # CC: YCC->RGB  SAMP: h2v2 merged  IDCT: ifast  ENT: prog huff
    add_bittest(${djpeg} 420m-q100-ifast-prog "-dct;fast;-nosmooth;-${ext}"
      ${testout}_420m_q100_ifast.${ext} ${testout}_420_q100_ifast_prog.jpg
      ${MD5_${EXT}_420M_Q100_IFAST} ${cjpeg}-${libtype}-420-q100-ifast-prog)

    # CC: RGB->Gray  SAMP: fullsize  FDCT: islow  ENT: huff
    add_bittest(${cjpeg} gray-islow "-gray;-dct;int;-noicc"
      ${testout}_gray_islow.jpg ${SRCIMG}
      ${MD5_JPEG_GRAY_ISLOW})

    # CC: Gray->Gray  SAMP: fullsize  IDCT: islow  ENT: huff
    add_bittest(${djpeg} gray-islow "-dct;int;-${ext}"
      ${testout}_gray_islow.${ext} ${testout}_gray_islow.jpg
      ${MD5_${EXT}_GRAY_ISLOW} ${cjpeg}-${libtype}-gray-islow)

    # CC: Gray->RGB  SAMP: fullsize  IDCT: islow  ENT: huff
    add_bittest(${djpeg} gray-islow-rgb "-dct;int;-rgb;-${ext}"
      ${testout}_gray_islow_rgb.${ext} ${testout}_gray_islow.jpg
      ${MD5_${EXT}_GRAY_ISLOW_RGB} ${cjpeg}-${libtype}-gray-islow)

    if(sample_bits EQUAL 8)
      # CC: Gray->RGB565  SAMP: fullsize  IDCT: islow  ENT: huff
      add_bittest(${djpeg} gray-islow-565 "-dct;int;-rgb565;-dither;none;-bmp"
        ${testout}_gray_islow_565.bmp ${testout}_gray_islow.jpg
        ${MD5_BMP_GRAY_ISLOW_565} ${cjpeg}-${libtype}-gray-islow)

      # CC: Gray->RGB565 (dithered)  SAMP: fullsize  IDCT: islow  ENT: huff
      add_bittest(${djpeg} gray-islow-565D "-dct;int;-rgb565;-bmp"
        ${testout}_gray_islow_565D.bmp ${testout}_gray_islow.jpg
        ${MD5_BMP_GRAY_ISLOW_565D} ${cjpeg}-${libtype}-gray-islow)
    endif()

    # CC: RGB->YCC  SAMP: fullsize smooth/h2v2 smooth  FDCT: islow
    # ENT: 2-pass huff
    add_bittest(${cjpeg} 420s-islow-opt "-sample;2x2;-smooth;1;-dct;int;-opt"
      ${testout}_420s_islow_opt.jpg ${SRCIMG}
      ${MD5_JPEG_420S_ISLOW_OPT})

    if(FLOATTEST${sample_bits})
      # CC: RGB->YCC  SAMP: fullsize/int  FDCT: float  ENT: prog huff
      add_bittest(${cjpeg} 3x2-float-prog "-sample;3x2;-dct;float;-prog"
        ${testout}_3x2_float_prog.jpg ${SRCIMG}
        ${MD5_JPEG_3x2_FLOAT_PROG_${FLOATTEST${sample_bits}_UC}})

      # CC: YCC->RGB  SAMP: fullsize/int  IDCT: float  ENT: prog huff
      add_bittest(${djpeg} 3x2-float-prog "-dct;float;-${ext}"
        ${testout}_3x2_float.${ext} ${testout}_3x2_float_prog.jpg
        ${MD5_${EXT}_3x2_FLOAT_${FLOATTEST${sample_bits}_UC}}
        ${cjpeg}-${libtype}-3x2-float-prog)
    endif()

    # CC: RGB->YCC  SAMP: fullsize/int  FDCT: ifast  ENT: prog huff
    add_bittest(${cjpeg} 3x2-ifast-prog "-sample;3x2;-dct;fast;-prog"
      ${testout}_3x2_ifast_prog.jpg ${SRCIMG}
      ${MD5_JPEG_3x2_IFAST_PROG})

    # CC: YCC->RGB  SAMP: fullsize/int  IDCT: ifast  ENT: prog huff
    add_bittest(${djpeg} 3x2-ifast-prog "-dct;fast;-${ext}"
      ${testout}_3x2_ifast.${ext} ${testout}_3x2_ifast_prog.jpg
      ${MD5_${EXT}_3x2_IFAST} ${cjpeg}-${libtype}-3x2-ifast-prog)

    if(WITH_ARITH_ENC AND sample_bits EQUAL 8)
      # CC: YCC->RGB  SAMP: fullsize/h2v2  FDCT: islow  ENT: arith
      add_bittest(${cjpeg} 420-islow-ari "-dct;int;-arithmetic"
        ${testout}_420_islow_ari.jpg ${SRCIMG}
        ${MD5_JPEG_420_ISLOW_ARI})

      add_bittest(${jpegtran} 420-islow-ari "-arithmetic"
        ${testout}_420_islow_ari2.jpg ${TESTIMAGES}/testimgint.jpg
        ${MD5_JPEG_420_ISLOW_ARI})

      # CC: YCC->RGB  SAMP: fullsize  FDCT: islow  ENT: prog arith
      add_bittest(${cjpeg} 444-islow-progari
        "-sample;1x1;-dct;int;-prog;-arithmetic"
        ${testout}_444_islow_progari.jpg ${SRCIMG}
        ${MD5_JPEG_444_ISLOW_PROGARI})
    endif()

    if(WITH_ARITH_DEC AND sample_bits EQUAL 8)
      # CC: RGB->YCC  SAMP: h2v2 merged  IDCT: ifast  ENT: arith
      add_bittest(${djpeg} 420m-ifast-ari "-fast;-skip;1,20;-${ext}"
        ${testout}_420m_ifast_ari.${ext} ${TESTIMAGES}/testimgari.jpg
        ${MD5_${EXT}_420M_IFAST_ARI})

      add_bittest(${jpegtran} 420-islow ""
        ${testout}_420_islow.jpg ${TESTIMAGES}/testimgari.jpg
        ${MD5_JPEG_420_ISLOW})
    endif()

    # 2/1--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 16x16 islow  ENT: huff
    # 15/8--  CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 15x15 islow  ENT: huff
    # 13/8--  CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 13x13 islow  ENT: huff
    # 11/8--  CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 11x11 islow  ENT: huff
    # 9/8--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 9x9 islow  ENT: huff
    # 7/8--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 7x7 islow/14x14 islow
    #         ENT: huff
    # 3/4--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 6x6 islow/12x12 islow
    #         ENT: huff
    # 5/8--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 5x5 islow/10x10 islow
    #         ENT: huff
    # 1/2--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 4x4 islow/8x8 islow
    #         ENT: huff
    # 3/8--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 3x3 islow/6x6 islow
    #         ENT: huff
    # 1/4--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 2x2 islow/4x4 islow
    #         ENT: huff
    # 1/8--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 1x1 islow/2x2 islow
    #         ENT: huff
    foreach(scale 2_1 15_8 13_8 11_8 9_8 7_8 3_4 5_8 1_2 3_8 1_4 1_8)
      string(REGEX REPLACE "_" "/" scalearg ${scale})
      add_bittest(${djpeg} 420m-islow-${scale}
        "-dct;int;-scale;${scalearg};-nosmooth;-${ext}"
        ${testout}_420m_islow_${scale}.${ext} ${JPEGIMG}
        ${MD5_${EXT}_420M_ISLOW_${scale}})
    endforeach()

    if(sample_bits EQUAL 8)
      # CC: YCC->RGB (dithered)  SAMP: h2v2 fancy  IDCT: islow  ENT: huff
      add_bittest(${djpeg} 420-islow-256 "-dct;int;-colors;256;-bmp"
        ${testout}_420_islow_256.bmp ${JPEGIMG}
        ${MD5_BMP_420_ISLOW_256})

      # CC: YCC->RGB565  SAMP: h2v2 fancy  IDCT: islow  ENT: huff
      add_bittest(${djpeg} 420-islow-565 "-dct;int;-rgb565;-dither;none;-bmp"
        ${testout}_420_islow_565.bmp ${JPEGIMG}
        ${MD5_BMP_420_ISLOW_565})

      # CC: YCC->RGB565 (dithered)  SAMP: h2v2 fancy  IDCT: islow  ENT: huff
      add_bittest(${djpeg} 420-islow-565D "-dct;int;-rgb565;-bmp"
        ${testout}_420_islow_565D.bmp ${JPEGIMG}
        ${MD5_BMP_420_ISLOW_565D})

      # CC: YCC->RGB565  SAMP: h2v2 merged  IDCT: islow  ENT: huff
      add_bittest(${djpeg} 420m-islow-565
        "-dct;int;-nosmooth;-rgb565;-dither;none;-bmp"
        ${testout}_420m_islow_565.bmp ${JPEGIMG}
        ${MD5_BMP_420M_ISLOW_565})

      # CC: YCC->RGB565 (dithered)  SAMP: h2v2 merged  IDCT: islow  ENT: huff
      add_bittest(${djpeg} 420m-islow-565D "-dct;int;-nosmooth;-rgb565;-bmp"
        ${testout}_420m_islow_565D.bmp ${JPEGIMG}
        ${MD5_BMP_420M_ISLOW_565D})
    endif()

    # Lossless (all arguments other than -lossless and -restart should have no
    # effect)
    add_bittest(${cjpeg} lossless
      "-lossless;4;-restart;1;-quality;1;-grayscale;-optimize;-dct;float;-smooth;100;-baseline;-qslots;1,0,0;-sample;1x2,3x4,2x1"
      ${testout}_lossless.jpg ${SRCIMG}
      ${MD5_JPEG_LOSSLESS})
    add_bittest(${djpeg} lossless
      "-fast;-scale;1/8;-dct;float;-dither;none;-nosmooth;-onepass;-${ext}"
      ${testout}_lossless.${ext} ${testout}_lossless.jpg
      ${MD5_${EXT}_LOSSLESS} ${cjpeg}-${libtype}-lossless)

    # Partial decode tests.  These tests are designed to cover all of the
    # possible code paths in jpeg_skip_scanlines().

    # Context rows: Yes  Intra-iMCU row: Yes  iMCU row prefetch: No
    # ENT: huff
    add_bittest(${djpeg} 420-islow-skip15_31 "-dct;int;-skip;15,31;-${ext}"
      ${testout}_420_islow_skip15,31.${ext} ${JPEGIMG}
      ${MD5_${EXT}_420_ISLOW_SKIP15_31})

    # Context rows: Yes  Intra-iMCU row: No   iMCU row prefetch: Yes
    # ENT: arith
    if(WITH_ARITH_DEC AND sample_bits EQUAL 8)
      add_bittest(${djpeg} 420-islow-ari-skip16_139
        "-dct;int;-skip;16,139;-${ext}"
        ${testout}_420_islow_ari_skip16,139.${ext} ${TESTIMAGES}/testimgari.jpg
        ${MD5_${EXT}_420_ISLOW_ARI_SKIP16_139})
    endif()

    # Context rows: Yes  Intra-iMCU row: No   iMCU row prefetch: No
    # ENT: prog huff
    add_test(NAME ${cjpeg}-${libtype}-420-islow-prog
      COMMAND cjpeg${suffix} -dct int -prog -precision ${sample_bits}
        -outfile ${testout}_420_islow_prog.jpg ${SRCIMG})
    add_bittest(${djpeg} 420-islow-prog-crop62x62_71_71
      "-dct;int;-crop;62x62+71+71;-${ext}"
      ${testout}_420_islow_prog_crop62x62,71,71.${ext}
      ${testout}_420_islow_prog.jpg
      ${MD5_${EXT}_420_ISLOW_PROG_CROP62x62_71_71}
      ${cjpeg}-${libtype}-420-islow-prog)

    # Context rows: Yes  Intra-iMCU row: No   iMCU row prefetch: No
    # ENT: arith
    if(WITH_ARITH_DEC AND sample_bits EQUAL 8)
      add_bittest(${djpeg} 420-islow-ari-crop53x53_4_4
        "-dct;int;-crop;53x53+4+4;-${ext}"
        ${testout}_420_islow_ari_crop53x53,4,4.${ext}
        ${TESTIMAGES}/testimgari.jpg
        ${MD5_${EXT}_420_ISLOW_ARI_CROP53x53_4_4})
    endif()

    # Context rows: No   Intra-iMCU row: Yes  ENT: huff
    add_test(NAME ${cjpeg}-${libtype}-444-islow
      COMMAND cjpeg${suffix} -dct int -sample 1x1 -precision ${sample_bits}
        -outfile ${testout}_444_islow.jpg ${SRCIMG})
    add_bittest(${djpeg} 444-islow-skip1_6 "-dct;int;-skip;1,6;-${ext}"
      ${testout}_444_islow_skip1,6.${ext} ${testout}_444_islow.jpg
      ${MD5_${EXT}_444_ISLOW_SKIP1_6} ${cjpeg}-${libtype}-444-islow)

    # Context rows: No   Intra-iMCU row: No   ENT: prog huff
    add_test(NAME ${cjpeg}-${libtype}-444-islow-prog
      COMMAND cjpeg${suffix} -dct int -prog -precision ${sample_bits}
        -sample 1x1 -outfile ${testout}_444_islow_prog.jpg ${SRCIMG})
    add_bittest(${djpeg} 444-islow-prog-crop98x98_13_13
      "-dct;int;-crop;98x98+13+13;-${ext}"
      ${testout}_444_islow_prog_crop98x98,13,13.${ext}
      ${testout}_444_islow_prog.jpg
      ${MD5_${EXT}_444_ISLOW_PROG_CROP98x98_13_13}
      ${cjpeg}-${libtype}-444-islow-prog)

    # Context rows: No   Intra-iMCU row: No   ENT: arith
    if(WITH_ARITH_ENC AND sample_bits EQUAL 8)
      add_test(NAME ${cjpeg}-${libtype}-444-islow-ari
        COMMAND cjpeg${suffix} -dct int -arithmetic -sample 1x1
          -precision ${sample_bits} -outfile ${testout}_444_islow_ari.jpg
          ${SRCIMG})
      if(WITH_ARITH_DEC)
        add_bittest(${djpeg} 444-islow-ari-crop37x37_0_0
          "-dct;int;-crop;37x37+0+0;-${ext}"
          ${testout}_444_islow_ari_crop37x37,0,0.${ext}
          ${testout}_444_islow_ari.jpg
          ${MD5_${EXT}_444_ISLOW_ARI_CROP37x37_0_0}
          ${cjpeg}-${libtype}-444-islow-ari)
      endif()
    endif()

    add_bittest(${jpegtran} crop "-crop;120x90+20+50;-transpose;-perfect"
      ${testout}_crop.jpg ${JPEGIMG} ${MD5_JPEG_CROP})

    unset(EXAMPLE_12BIT_ARG)
    if(sample_bits EQUAL 12)
      set(EXAMPLE_12BIT_ARG "-precision;12")
    endif()

    add_test(NAME example-${sample_bits}bit-${libtype}-compress
      COMMAND example${suffix} compress -q 95 ${EXAMPLE_12BIT_ARG}
        ${testout}-example.jpg)
    add_test(NAME example-${sample_bits}bit-${libtype}-compress-cmp
      COMMAND md5cmp ${MD5_JPEG_EXAMPLE_COMPRESS} ${testout}-example.jpg)
    set_tests_properties(example-${sample_bits}bit-${libtype}-compress-cmp
      PROPERTIES DEPENDS example-${sample_bits}bit-${libtype}-compress)

    add_test(NAME example-${sample_bits}bit-${libtype}-decompress
      COMMAND example${suffix} decompress ${EXAMPLE_12BIT_ARG}
        ${JPEGIMG} ${testout}-example.ppm)
    add_test(NAME example-${sample_bits}bit-${libtype}-decompress-cmp
      COMMAND md5cmp ${MD5_PPM_EXAMPLE_DECOMPRESS} ${testout}-example.ppm)
    set_tests_properties(example-${sample_bits}bit-${libtype}-decompress-cmp
      PROPERTIES DEPENDS example-${sample_bits}bit-${libtype}-decompress)

  endforeach()

endforeach()

add_custom_target(testclean COMMAND ${CMAKE_COMMAND} -P
  ${CMAKE_CURRENT_SOURCE_DIR}/cmakescripts/testclean.cmake)

configure_file(test/croptest.in test/croptest @ONLY)
add_custom_target(croptest
  COMMAND echo croptest
  COMMAND ${BASH} ${CMAKE_CURRENT_BINARY_DIR}/test/croptest)

configure_file(test/indexedcolortest.in test/indexedcolortest @ONLY)
add_custom_target(indexedcolortest
  COMMAND echo indexedcolortest
  COMMAND ${BASH} ${CMAKE_CURRENT_BINARY_DIR}/test/indexedcolortest)

if(WITH_TURBOJPEG)
  configure_file(test/tjbenchtest.in test/tjbenchtest @ONLY)
  configure_file(test/tjcomptest.in test/tjcomptest @ONLY)
  configure_file(test/tjdecomptest.in test/tjdecomptest @ONLY)
  configure_file(test/tjtrantest.in test/tjtrantest @ONLY)
  if(WIN32)
    set(BASH bash)
  endif()
  add_custom_target(tjtest COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=8 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/test/tjbenchtest)
  add_custom_target(tjtest12 COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=12 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/test/tjbenchtest)
  add_custom_target(tjtest16 COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=2 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=3 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=4 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=5 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=6 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=7 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=9 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=10 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=11 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=13 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=14 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=15 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    COMMAND ${CMAKE_COMMAND} -DWITH_JNA=${WITH_JNA}
    -DPRECISION=16 -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/cmakescripts/tjbenchtest.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/test/tjbenchtest)
  add_custom_target(tjexampletest COMMAND ${CMAKE_COMMAND}
    -DWITH_JNA=${WITH_JNA}
    -P ${CMAKE_SOURCE_DIR}/cmakescripts/tjexampletest.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/cmakescripts/tjexampletest.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/test/tjcomptest
      ${CMAKE_CURRENT_BINARY_DIR}/test/tjdecomptest
      ${CMAKE_CURRENT_BINARY_DIR}/test/tjtrantest)
endif()

endif() # WITH_TESTS


###############################################################################
# INSTALLATION
###############################################################################

set(EXE ${CMAKE_EXECUTABLE_SUFFIX})

if(WITH_TURBOJPEG)
  if(ENABLE_SHARED)
    install(TARGETS turbojpeg EXPORT ${CMAKE_PROJECT_NAME}Targets
      INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin)
    if(WITH_TOOLS)
      install(TARGETS tjbench
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin)
    endif()
    if(MSVC_LIKE AND CMAKE_C_LINKER_SUPPORTS_PDB)
      install(FILES "$<TARGET_PDB_FILE:turbojpeg>"
        DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin OPTIONAL)
    endif()
  endif()
  if(ENABLE_STATIC)
    install(TARGETS turbojpeg-static EXPORT ${CMAKE_PROJECT_NAME}Targets
      INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib)
    if(NOT ENABLE_SHARED)
      if(GENERATOR_IS_MULTI_CONFIG)
        set(DIR "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}")
      else()
        set(DIR ${CMAKE_CURRENT_BINARY_DIR})
      endif()
      if(WITH_TOOLS)
        install(PROGRAMS ${DIR}/tjbench-static${EXE}
          DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin
          RENAME tjbench${EXE})
      endif()
    endif()
  endif()
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/turbojpeg.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT include)
endif()

if(ENABLE_STATIC)
  install(TARGETS jpeg-static EXPORT ${CMAKE_PROJECT_NAME}Targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib)
  if(NOT ENABLE_SHARED)
    if(GENERATOR_IS_MULTI_CONFIG)
      set(DIR "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}")
    else()
      set(DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()
    if(WITH_TOOLS)
      install(PROGRAMS ${DIR}/cjpeg-static${EXE}
        DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin RENAME cjpeg${EXE})
      install(PROGRAMS ${DIR}/djpeg-static${EXE}
        DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin RENAME djpeg${EXE})
      install(PROGRAMS ${DIR}/jpegtran-static${EXE}
        DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin
        RENAME jpegtran${EXE})
    endif()
  endif()
endif()

if(WITH_TOOLS)
  install(TARGETS rdjpgcom wrjpgcom
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin)
endif()

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.ijg
  ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  ${CMAKE_CURRENT_SOURCE_DIR}/src/example.c
  ${CMAKE_CURRENT_SOURCE_DIR}/doc/libjpeg.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/doc/structure.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/doc/usage.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/doc/wizard.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT doc)
if(WITH_TURBOJPEG)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/tjcomp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tjdecomp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tjtran.c
    DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT doc)
endif()
if(WITH_JNA)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/jna/TJ.java
    ${CMAKE_CURRENT_SOURCE_DIR}/jna/TJComp.java
    ${CMAKE_CURRENT_SOURCE_DIR}/jna/TJDecomp.java
    ${CMAKE_CURRENT_SOURCE_DIR}/jna/TJTran.java
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/jna COMPONENT doc)
endif()

if(UNIX OR MINGW)
  if(WITH_TOOLS)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/cjpeg.1
      ${CMAKE_CURRENT_SOURCE_DIR}/doc/djpeg.1
      ${CMAKE_CURRENT_SOURCE_DIR}/doc/jpegtran.1
      ${CMAKE_CURRENT_SOURCE_DIR}/doc/rdjpgcom.1
      ${CMAKE_CURRENT_SOURCE_DIR}/doc/wrjpgcom.1
      DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 COMPONENT man)
  endif()
endif()
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgscripts/libjpeg.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig COMPONENT lib)
if(WITH_TURBOJPEG)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgscripts/libturbojpeg.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig COMPONENT lib)
endif()
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/pkgscripts/${CMAKE_PROJECT_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/pkgscripts/${CMAKE_PROJECT_NAME}ConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME}
  COMPONENT lib)
install(EXPORT ${CMAKE_PROJECT_NAME}Targets
  NAMESPACE ${CMAKE_PROJECT_NAME}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME}
  COMPONENT lib)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/jconfig.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/jerror.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/jmorecfg.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/jpeglib.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT include)

include(cmakescripts/BuildPackages.cmake)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmakescripts/cmake_uninstall.cmake.in"
  "cmake_uninstall.cmake" IMMEDIATE @ONLY)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P cmake_uninstall.cmake)
endif()
