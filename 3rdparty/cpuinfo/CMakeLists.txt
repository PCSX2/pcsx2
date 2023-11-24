CMAKE_MINIMUM_REQUIRED(VERSION 3.5 FATAL_ERROR)

# ---[ Setup project
PROJECT(
    cpuinfo
    LANGUAGES C
    )

# ---[ Options.
SET(CPUINFO_LIBRARY_TYPE "default" CACHE STRING "Type of cpuinfo library (shared, static, or default) to build")
SET_PROPERTY(CACHE CPUINFO_LIBRARY_TYPE PROPERTY STRINGS default static shared)
SET(CPUINFO_RUNTIME_TYPE "default" CACHE STRING "Type of runtime library (shared, static, or default) to use")
SET_PROPERTY(CACHE CPUINFO_RUNTIME_TYPE PROPERTY STRINGS default static shared)
SET(CPUINFO_LOG_LEVEL "default" CACHE STRING "Minimum logging level (info with lower severity will be ignored)")
SET_PROPERTY(CACHE CPUINFO_LOG_LEVEL PROPERTY STRINGS default debug info warning error fatal none)
IF(ANDROID)
  OPTION(CPUINFO_LOG_TO_STDIO "Log errors, warnings, and information to stdout/stderr" OFF)
ELSE()
  OPTION(CPUINFO_LOG_TO_STDIO "Log errors, warnings, and information to stdout/stderr" ON)
ENDIF()
OPTION(CPUINFO_BUILD_TOOLS "Build command-line tools" OFF)
OPTION(CPUINFO_BUILD_UNIT_TESTS "Build cpuinfo unit tests" OFF)
OPTION(CPUINFO_BUILD_MOCK_TESTS "Build cpuinfo mock tests" OFF)
OPTION(CPUINFO_BUILD_BENCHMARKS "Build cpuinfo micro-benchmarks" OFF)
OPTION(CPUINFO_BUILD_PKG_CONFIG "Build pkg-config manifest" OFF)
OPTION(USE_SYSTEM_LIBS "Use system libraries instead of downloading and building them" OFF)
OPTION(USE_SYSTEM_GOOGLEBENCHMARK "Use system Google Benchmark library instead of downloading and building it" ${USE_SYSTEM_LIBS})
OPTION(USE_SYSTEM_GOOGLETEST "Use system Google Test library instead of downloading and building it" ${USE_SYSTEM_LIBS})

# ---[ CMake options
INCLUDE(GNUInstallDirs)

IF(CPUINFO_BUILD_UNIT_TESTS OR CPUINFO_BUILD_MOCK_TESTS)
  ENABLE_TESTING()
ENDIF()

MACRO(CPUINFO_TARGET_ENABLE_C99 target)
  SET_TARGET_PROPERTIES(${target} PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS NO)
ENDMACRO()

MACRO(CPUINFO_TARGET_ENABLE_CXX11 target)
  SET_TARGET_PROPERTIES(${target} PROPERTIES
    CXX_STANDARD 14
    CXX_EXTENSIONS NO)
ENDMACRO()

MACRO(CPUINFO_TARGET_RUNTIME_LIBRARY target)
  IF(MSVC AND NOT CPUINFO_RUNTIME_TYPE STREQUAL "default")
    IF(CPUINFO_RUNTIME_TYPE STREQUAL "shared")
      TARGET_COMPILE_OPTIONS(${target} PRIVATE
        "/MD$<$<CONFIG:Debug>:d>")
    ELSEIF(CPUINFO_RUNTIME_TYPE STREQUAL "static")
      TARGET_COMPILE_OPTIONS(${target} PRIVATE
        "/MT$<$<CONFIG:Debug>:d>")
    ENDIF()
  ENDIF()
ENDMACRO()

# -- [ Determine whether building for Apple's desktop or mobile OSes
IF(CMAKE_SYSTEM_NAME MATCHES "^(Darwin|iOS|tvOS|watchOS)$")
  SET(IS_APPLE_OS TRUE)
ELSE()
  SET(IS_APPLE_OS FALSE)
ENDIF()

# -- [ Determine target processor
SET(CPUINFO_TARGET_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")
IF(IS_APPLE_OS AND CMAKE_OSX_ARCHITECTURES MATCHES "^(x86_64|arm64.*)$")
  SET(CPUINFO_TARGET_PROCESSOR "${CMAKE_OSX_ARCHITECTURES}")
ELSEIF(CMAKE_GENERATOR MATCHES "^Visual Studio " AND CMAKE_VS_PLATFORM_NAME)
  IF(CMAKE_VS_PLATFORM_NAME STREQUAL "Win32")
    SET(CPUINFO_TARGET_PROCESSOR "x86")
  ELSEIF(CMAKE_VS_PLATFORM_NAME STREQUAL "x64")
    SET(CPUINFO_TARGET_PROCESSOR "x86_64")
  ELSEIF(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
    SET(CPUINFO_TARGET_PROCESSOR "arm64")
  ELSEIF(CMAKE_VS_PLATFORM_NAME MATCHES "^(ARM64EC|arm64ec|ARM64E|arm64e)")
    SET(CPUINFO_TARGET_PROCESSOR "arm64")
  ELSE()
    MESSAGE(FATAL_ERROR "Unsupported Visual Studio architecture \"${CMAKE_VS_PLATFORM_NAME}\"")
  ENDIF()
ENDIF()

# ---[ Build flags
SET(CPUINFO_SUPPORTED_PLATFORM TRUE)
IF(NOT CMAKE_SYSTEM_PROCESSOR)
  IF(NOT IOS)
    MESSAGE(WARNING
      "Target processor architecture is not specified. "
      "cpuinfo will compile, but cpuinfo_initialize() will always fail.")
    SET(CPUINFO_SUPPORTED_PLATFORM FALSE)
  ENDIF()
ELSEIF(NOT CPUINFO_TARGET_PROCESSOR MATCHES "^(i[3-6]86|AMD64|x86(_64)?|armv[5-8].*|aarch64|arm64.*|ARM64.*|riscv(32|64))$")
  MESSAGE(WARNING
    "Target processor architecture \"${CPUINFO_TARGET_PROCESSOR}\" is not supported in cpuinfo. "
    "cpuinfo will compile, but cpuinfo_initialize() will always fail.")
  SET(CPUINFO_SUPPORTED_PLATFORM FALSE)
ENDIF()


IF(NOT CMAKE_SYSTEM_NAME)
    MESSAGE(WARNING
      "Target operating system is not specified. "
      "cpuinfo will compile, but cpuinfo_initialize() will always fail.")
  SET(CPUINFO_SUPPORTED_PLATFORM FALSE)
ELSEIF(NOT CMAKE_SYSTEM_NAME MATCHES "^(Windows|WindowsStore|CYGWIN|MSYS|Darwin|Linux|Android)$")
  IF(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.14" AND NOT IS_APPLE_OS)
    MESSAGE(WARNING
      "Target operating system \"${CMAKE_SYSTEM_NAME}\" is not supported in cpuinfo. "
      "cpuinfo will compile, but cpuinfo_initialize() will always fail.")
    SET(CPUINFO_SUPPORTED_PLATFORM FALSE)
  ENDIF()
ENDIF()

IF(CPUINFO_SUPPORTED_PLATFORM)
  IF(CPUINFO_BUILD_MOCK_TESTS OR CPUINFO_BUILD_UNIT_TESTS OR CPUINFO_BUILD_BENCHMARKS)
    ENABLE_LANGUAGE(CXX)
  ENDIF()
ENDIF()

# ---[ Download deps
SET(CONFU_DEPENDENCIES_SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps
  CACHE PATH "Confu-style dependencies source directory")
SET(CONFU_DEPENDENCIES_BINARY_DIR ${CMAKE_BINARY_DIR}/deps
  CACHE PATH "Confu-style dependencies binary directory")

IF(CPUINFO_SUPPORTED_PLATFORM AND (CPUINFO_BUILD_MOCK_TESTS OR CPUINFO_BUILD_UNIT_TESTS))
  IF(USE_SYSTEM_GOOGLETEST)
    FIND_PACKAGE(GTest REQUIRED)
  ELSEIF(NOT DEFINED GOOGLETEST_SOURCE_DIR)
    MESSAGE(STATUS "Downloading Google Test to ${CONFU_DEPENDENCIES_SOURCE_DIR}/googletest (define GOOGLETEST_SOURCE_DIR to avoid it)")
    CONFIGURE_FILE(cmake/DownloadGoogleTest.cmake "${CONFU_DEPENDENCIES_BINARY_DIR}/googletest-download/CMakeLists.txt")
    EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
      WORKING_DIRECTORY "${CONFU_DEPENDENCIES_BINARY_DIR}/googletest-download")
    EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" --build .
      WORKING_DIRECTORY "${CONFU_DEPENDENCIES_BINARY_DIR}/googletest-download")
    SET(GOOGLETEST_SOURCE_DIR "${CONFU_DEPENDENCIES_SOURCE_DIR}/googletest" CACHE STRING "Google Test source directory")
  ENDIF()
ENDIF()

IF(CPUINFO_SUPPORTED_PLATFORM AND CPUINFO_BUILD_BENCHMARKS)
  IF(USE_SYSTEM_GOOGLEBENCHMARK)
    FIND_PACKAGE(benchmark REQUIRED)
  ELSEIF(NOT DEFINED GOOGLEBENCHMARK_SOURCE_DIR)
    MESSAGE(STATUS "Downloading Google Benchmark to ${CONFU_DEPENDENCIES_SOURCE_DIR}/googlebenchmark (define GOOGLEBENCHMARK_SOURCE_DIR to avoid it)")
    CONFIGURE_FILE(cmake/DownloadGoogleBenchmark.cmake "${CONFU_DEPENDENCIES_BINARY_DIR}/googlebenchmark-download/CMakeLists.txt")
    EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
      WORKING_DIRECTORY "${CONFU_DEPENDENCIES_BINARY_DIR}/googlebenchmark-download")
    EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" --build .
      WORKING_DIRECTORY "${CONFU_DEPENDENCIES_BINARY_DIR}/googlebenchmark-download")
    SET(GOOGLEBENCHMARK_SOURCE_DIR "${CONFU_DEPENDENCIES_SOURCE_DIR}/googlebenchmark" CACHE STRING "Google Benchmark source directory")
  ENDIF()
ENDIF()

# ---[ cpuinfo library
SET(CPUINFO_SRCS src/api.c src/cache.c src/init.c src/log.c)

IF(CPUINFO_SUPPORTED_PLATFORM)
  IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten" AND (CPUINFO_TARGET_PROCESSOR MATCHES "^(i[3-6]86|AMD64|x86(_64)?)$" OR IOS_ARCH MATCHES "^(i386|x86_64)$"))
    LIST(APPEND CPUINFO_SRCS
      src/x86/init.c
      src/x86/info.c
      src/x86/vendor.c
      src/x86/uarch.c
      src/x86/name.c
      src/x86/topology.c
      src/x86/isa.c
      src/x86/cache/init.c
      src/x86/cache/descriptor.c
      src/x86/cache/deterministic.c)
    IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
      LIST(APPEND CPUINFO_SRCS
        src/x86/linux/init.c
        src/x86/linux/cpuinfo.c)
    ELSEIF(IS_APPLE_OS)
      LIST(APPEND CPUINFO_SRCS src/x86/mach/init.c)
    ELSEIF(CMAKE_SYSTEM_NAME MATCHES "^(Windows|WindowsStore|CYGWIN|MSYS)$")
      LIST(APPEND CPUINFO_SRCS src/x86/windows/init.c)
    ENDIF()
  ELSEIF(CMAKE_SYSTEM_NAME MATCHES "^Windows" AND CPUINFO_TARGET_PROCESSOR MATCHES "^(ARM64|arm64)$")
    LIST(APPEND CPUINFO_SRCS
      src/arm/windows/init-by-logical-sys-info.c
      src/arm/windows/init.c)
  ELSEIF(CPUINFO_TARGET_PROCESSOR MATCHES "^(armv[5-8].*|aarch64|arm64.*)$" OR IOS_ARCH MATCHES "^(armv7.*|arm64.*)$")
    LIST(APPEND CPUINFO_SRCS
      src/arm/uarch.c
      src/arm/cache.c)
    IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
      LIST(APPEND CPUINFO_SRCS
        src/arm/linux/init.c
        src/arm/linux/cpuinfo.c
        src/arm/linux/clusters.c
        src/arm/linux/chipset.c
        src/arm/linux/midr.c
        src/arm/linux/hwcap.c)
      IF(CMAKE_SYSTEM_PROCESSOR MATCHES "^armv[5-8]")
        LIST(APPEND CPUINFO_SRCS src/arm/linux/aarch32-isa.c)
        IF(CMAKE_SYSTEM_NAME STREQUAL "Android" AND ANDROID_ABI STREQUAL "armeabi")
          SET_SOURCE_FILES_PROPERTIES(src/arm/linux/aarch32-isa.c PROPERTIES COMPILE_FLAGS -marm)
        ENDIF()
      ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        LIST(APPEND CPUINFO_SRCS src/arm/linux/aarch64-isa.c)
      ENDIF()
    ELSEIF(IS_APPLE_OS AND CPUINFO_TARGET_PROCESSOR MATCHES "arm64.*")
      LIST(APPEND CPUINFO_SRCS src/arm/mach/init.c)
    ENDIF()
    IF(CMAKE_SYSTEM_NAME STREQUAL "Android")
      LIST(APPEND CPUINFO_SRCS
        src/arm/android/properties.c)
    ENDIF()
  ELSEIF(CPUINFO_TARGET_PROCESSOR MATCHES "^(riscv(32|64))$")
    LIST(APPEND CPUINFO_SRCS
      src/riscv/uarch.c)
    IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      LIST(APPEND CPUINFO_SRCS
        src/riscv/linux/init.c
	src/riscv/linux/riscv-hw.c
	src/riscv/linux/riscv-isa.c)
    ENDIF()
  ENDIF()

  IF(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    LIST(APPEND CPUINFO_SRCS
      src/emscripten/init.c)
  ENDIF()

  IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    LIST(APPEND CPUINFO_SRCS
      src/linux/smallfile.c
      src/linux/multiline.c
      src/linux/cpulist.c
      src/linux/processors.c)
  ELSEIF(IS_APPLE_OS)
    LIST(APPEND CPUINFO_SRCS src/mach/topology.c)
  ENDIF()

  IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    SET(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    SET(THREADS_PREFER_PTHREAD_FLAG TRUE)
    FIND_PACKAGE(Threads REQUIRED)
  ENDIF()
ENDIF()

IF(CPUINFO_LIBRARY_TYPE STREQUAL "default")
  ADD_LIBRARY(cpuinfo ${CPUINFO_SRCS})
ELSEIF(CPUINFO_LIBRARY_TYPE STREQUAL "shared")
  ADD_LIBRARY(cpuinfo SHARED ${CPUINFO_SRCS})
ELSEIF(CPUINFO_LIBRARY_TYPE STREQUAL "static")
  ADD_LIBRARY(cpuinfo STATIC ${CPUINFO_SRCS})
ELSE()
  MESSAGE(FATAL_ERROR "Unsupported library type ${CPUINFO_LIBRARY_TYPE}")
ENDIF()
ADD_LIBRARY(cpuinfo_internals STATIC ${CPUINFO_SRCS})
CPUINFO_TARGET_ENABLE_C99(cpuinfo)
CPUINFO_TARGET_ENABLE_C99(cpuinfo_internals)
CPUINFO_TARGET_RUNTIME_LIBRARY(cpuinfo)
IF(CMAKE_SYSTEM_NAME MATCHES "^(Windows|WindowsStore|CYGWIN|MSYS)$")
  # Target Windows 7+ API
  TARGET_COMPILE_DEFINITIONS(cpuinfo PRIVATE _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS)
  TARGET_COMPILE_DEFINITIONS(cpuinfo_internals PRIVATE _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS)
  # Explicitly link Kernel32 for UWP build
  if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    TARGET_LINK_LIBRARIES(cpuinfo PUBLIC Kernel32)
  endif()
ENDIF()
IF(ANDROID AND NOT CPUINFO_LOG_TO_STDIO)
  TARGET_LINK_LIBRARIES(cpuinfo PRIVATE "log")
ENDIF()
SET_TARGET_PROPERTIES(cpuinfo PROPERTIES PUBLIC_HEADER include/cpuinfo.h)
TARGET_INCLUDE_DIRECTORIES(cpuinfo BEFORE PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
TARGET_INCLUDE_DIRECTORIES(cpuinfo BEFORE PRIVATE src)
TARGET_INCLUDE_DIRECTORIES(cpuinfo_internals BEFORE PUBLIC include src)
TARGET_COMPILE_DEFINITIONS(cpuinfo PRIVATE "CPUINFO_LOG_TO_STDIO=$<BOOL:${CPUINFO_LOG_TO_STDIO}>")
IF(CPUINFO_LOG_LEVEL STREQUAL "default")
  # default logging level: error (subject to change)
  TARGET_COMPILE_DEFINITIONS(cpuinfo PRIVATE "CPUINFO_LOG_LEVEL=2")
ELSEIF(CPUINFO_LOG_LEVEL STREQUAL "debug")
  TARGET_COMPILE_DEFINITIONS(cpuinfo PRIVATE "CPUINFO_LOG_LEVEL=5")
ELSEIF(CPUINFO_LOG_LEVEL STREQUAL "info")
  TARGET_COMPILE_DEFINITIONS(cpuinfo PRIVATE "CPUINFO_LOG_LEVEL=4")
ELSEIF(CPUINFO_LOG_LEVEL STREQUAL "warning")
  TARGET_COMPILE_DEFINITIONS(cpuinfo PRIVATE "CPUINFO_LOG_LEVEL=3")
ELSEIF(CPUINFO_LOG_LEVEL STREQUAL "error")
  TARGET_COMPILE_DEFINITIONS(cpuinfo PRIVATE "CPUINFO_LOG_LEVEL=2")
ELSEIF(CPUINFO_LOG_LEVEL STREQUAL "fatal")
  TARGET_COMPILE_DEFINITIONS(cpuinfo PRIVATE "CPUINFO_LOG_LEVEL=1")
ELSEIF(CPUINFO_LOG_LEVEL STREQUAL "none")
  TARGET_COMPILE_DEFINITIONS(cpuinfo PRIVATE "CPUINFO_LOG_LEVEL=0")
ELSE()
  MESSAGE(FATAL_ERROR "Unsupported logging level ${CPUINFO_LOG_LEVEL}")
ENDIF()
TARGET_COMPILE_DEFINITIONS(cpuinfo_internals PRIVATE "CPUINFO_LOG_LEVEL=0")
TARGET_COMPILE_DEFINITIONS(cpuinfo_internals PRIVATE "CPUINFO_LOG_TO_STDIO=1")

IF(CPUINFO_SUPPORTED_PLATFORM)
  TARGET_COMPILE_DEFINITIONS(cpuinfo INTERFACE CPUINFO_SUPPORTED_PLATFORM=1)
  IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    TARGET_LINK_LIBRARIES(cpuinfo PUBLIC ${CMAKE_THREAD_LIBS_INIT})
    TARGET_LINK_LIBRARIES(cpuinfo_internals PUBLIC ${CMAKE_THREAD_LIBS_INIT})
    TARGET_COMPILE_DEFINITIONS(cpuinfo PRIVATE _GNU_SOURCE=1)
    TARGET_COMPILE_DEFINITIONS(cpuinfo_internals PRIVATE _GNU_SOURCE=1)
  ENDIF()
ELSE()
  TARGET_COMPILE_DEFINITIONS(cpuinfo INTERFACE CPUINFO_SUPPORTED_PLATFORM=0)
ENDIF()

ADD_LIBRARY(${PROJECT_NAME}::cpuinfo ALIAS cpuinfo)

# support find_package(cpuinfo CONFIG)
INCLUDE(CMakePackageConfigHelpers)
GET_FILENAME_COMPONENT(CONFIG_FILE_PATH ${CMAKE_CURRENT_BINARY_DIR}/cpuinfo-config.cmake ABSOLUTE)
CONFIGURE_PACKAGE_CONFIG_FILE(
  cmake/cpuinfo-config.cmake.in ${CONFIG_FILE_PATH}
  INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})
INSTALL(FILES ${CONFIG_FILE_PATH}
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}) # cpuinfo_DIR ${prefix}/share/cpuinfo

INSTALL(TARGETS cpuinfo
  EXPORT cpuinfo-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

INSTALL(EXPORT cpuinfo-targets
  NAMESPACE ${PROJECT_NAME}:: # IMPORTED cpuinfo::cpuinfo
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})

# ---[ cpuinfo micro-benchmarks
IF(CPUINFO_SUPPORTED_PLATFORM AND CPUINFO_BUILD_BENCHMARKS)
  # ---[ Build google benchmark
  IF(NOT TARGET benchmark AND NOT USE_SYSTEM_GOOGLEBENCHMARK)
    SET(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "")
    ADD_SUBDIRECTORY(
      "${GOOGLEBENCHMARK_SOURCE_DIR}"
      "${CONFU_DEPENDENCIES_BINARY_DIR}/googlebenchmark")
  ENDIF()

  IF(CMAKE_SYSTEM_NAME MATCHES "^(Linux|Android)$")
    ADD_EXECUTABLE(get-current-bench bench/get-current.cc)
    TARGET_LINK_LIBRARIES(get-current-bench cpuinfo benchmark)
  ENDIF()

  ADD_EXECUTABLE(init-bench bench/init.cc)
  TARGET_LINK_LIBRARIES(init-bench cpuinfo benchmark)
ENDIF()

IF(CPUINFO_SUPPORTED_PLATFORM)
  IF(CPUINFO_BUILD_MOCK_TESTS OR CPUINFO_BUILD_UNIT_TESTS)
    # ---[ Build google test
    IF(NOT TARGET gtest AND NOT USE_SYSTEM_GOOGLETEST)
      IF(MSVC AND NOT CPUINFO_RUNTIME_TYPE STREQUAL "static")
        SET(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
      ENDIF()
      ADD_SUBDIRECTORY(
        "${GOOGLETEST_SOURCE_DIR}"
        "${CONFU_DEPENDENCIES_BINARY_DIR}/googletest")
    ENDIF()
  ENDIF()
ENDIF()

# ---[ cpuinfo mock library and mock tests
IF(CPUINFO_SUPPORTED_PLATFORM AND CPUINFO_BUILD_MOCK_TESTS)
  SET(CPUINFO_MOCK_SRCS "${CPUINFO_SRCS}")
  IF(CPUINFO_TARGET_PROCESSOR MATCHES "^(i[3-6]86|AMD64|x86(_64)?)$")
    LIST(APPEND CPUINFO_MOCK_SRCS src/x86/mockcpuid.c)
  ENDIF()
  IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    LIST(APPEND CPUINFO_MOCK_SRCS src/linux/mockfile.c)
  ENDIF()

  ADD_LIBRARY(cpuinfo_mock STATIC ${CPUINFO_MOCK_SRCS})
  CPUINFO_TARGET_ENABLE_C99(cpuinfo_mock)
  CPUINFO_TARGET_RUNTIME_LIBRARY(cpuinfo_mock)
  SET_TARGET_PROPERTIES(cpuinfo_mock PROPERTIES PUBLIC_HEADER include/cpuinfo.h)
  TARGET_INCLUDE_DIRECTORIES(cpuinfo_mock BEFORE PUBLIC include)
  TARGET_INCLUDE_DIRECTORIES(cpuinfo_mock BEFORE PRIVATE src)
  TARGET_COMPILE_DEFINITIONS(cpuinfo_mock PUBLIC "CPUINFO_MOCK=1")
  TARGET_COMPILE_DEFINITIONS(cpuinfo_mock PRIVATE "CPUINFO_LOG_LEVEL=5")
  TARGET_COMPILE_DEFINITIONS(cpuinfo_mock PRIVATE "CPUINFO_LOG_TO_STDIO=1")
  IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    TARGET_LINK_LIBRARIES(cpuinfo_mock PUBLIC ${CMAKE_THREAD_LIBS_INIT})
    TARGET_COMPILE_DEFINITIONS(cpuinfo_mock PRIVATE _GNU_SOURCE=1)
  ENDIF()

  IF(CMAKE_SYSTEM_NAME STREQUAL "Android" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv5te|armv7-a)$")
    ADD_EXECUTABLE(atm7029b-tablet-test test/mock/atm7029b-tablet.cc)
    TARGET_INCLUDE_DIRECTORIES(atm7029b-tablet-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(atm7029b-tablet-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME atm7029b-tablet-test COMMAND atm7029b-tablet-test)

    ADD_EXECUTABLE(blu-r1-hd-test test/mock/blu-r1-hd.cc)
    TARGET_INCLUDE_DIRECTORIES(blu-r1-hd-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(blu-r1-hd-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME blu-r1-hd-test COMMAND blu-r1-hd-test)

    ADD_EXECUTABLE(galaxy-a3-2016-eu-test test/mock/galaxy-a3-2016-eu.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-a3-2016-eu-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-a3-2016-eu-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-a3-2016-eu-test COMMAND galaxy-a3-2016-eu-test)

    ADD_EXECUTABLE(galaxy-a8-2016-duos-test test/mock/galaxy-a8-2016-duos.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-a8-2016-duos-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-a8-2016-duos-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-a8-2016-duos-test COMMAND galaxy-a8-2016-duos-test)

    ADD_EXECUTABLE(galaxy-grand-prime-value-edition-test test/mock/galaxy-grand-prime-value-edition.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-grand-prime-value-edition-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-grand-prime-value-edition-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-grand-prime-value-edition-test COMMAND galaxy-grand-prime-value-edition-test)

    ADD_EXECUTABLE(galaxy-j1-2016-test test/mock/galaxy-j1-2016.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-j1-2016-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-j1-2016-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-j1-2016-test COMMAND galaxy-j1-2016-test)

    ADD_EXECUTABLE(galaxy-j5-test test/mock/galaxy-j5.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-j5-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-j5-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-j5-test COMMAND galaxy-j5-test)

    ADD_EXECUTABLE(galaxy-j7-prime-test test/mock/galaxy-j7-prime.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-j7-prime-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-j7-prime-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-j7-prime-test COMMAND galaxy-j7-prime-test)

    ADD_EXECUTABLE(galaxy-j7-tmobile-test test/mock/galaxy-j7-tmobile.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-j7-tmobile-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-j7-tmobile-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-j7-tmobile-test COMMAND galaxy-j7-tmobile-test)

    ADD_EXECUTABLE(galaxy-j7-uae-test test/mock/galaxy-j7-uae.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-j7-uae-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-j7-uae-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-j7-uae-test COMMAND galaxy-j7-uae-test)

    ADD_EXECUTABLE(galaxy-s3-us-test test/mock/galaxy-s3-us.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s3-us-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s3-us-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s3-us-test COMMAND galaxy-s3-us-test)

    ADD_EXECUTABLE(galaxy-s4-us-test test/mock/galaxy-s4-us.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s4-us-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s4-us-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s4-us-test COMMAND galaxy-s4-us-test)

    ADD_EXECUTABLE(galaxy-s5-global-test test/mock/galaxy-s5-global.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s5-global-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s5-global-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s5-global-test COMMAND galaxy-s5-global-test)

    ADD_EXECUTABLE(galaxy-s5-us-test test/mock/galaxy-s5-us.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s5-us-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s5-us-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s5-us-test COMMAND galaxy-s5-us-test)

    ADD_EXECUTABLE(galaxy-tab-3-7.0-test test/mock/galaxy-tab-3-7.0.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-tab-3-7.0-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-tab-3-7.0-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-tab-3-7.0-test COMMAND galaxy-tab-3-7.0-test)

    ADD_EXECUTABLE(galaxy-tab-3-lite-test test/mock/galaxy-tab-3-lite.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-tab-3-lite-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-tab-3-lite-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-tab-3-lite-test COMMAND galaxy-tab-3-lite-test)

    ADD_EXECUTABLE(galaxy-win-duos-test test/mock/galaxy-win-duos.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-win-duos-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-win-duos-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-win-duos-test COMMAND galaxy-win-duos-test)

    ADD_EXECUTABLE(huawei-ascend-p7-test test/mock/huawei-ascend-p7.cc)
    TARGET_INCLUDE_DIRECTORIES(huawei-ascend-p7-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(huawei-ascend-p7-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME huawei-ascend-p7-test COMMAND huawei-ascend-p7-test)

    ADD_EXECUTABLE(huawei-honor-6-test test/mock/huawei-honor-6.cc)
    TARGET_INCLUDE_DIRECTORIES(huawei-honor-6-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(huawei-honor-6-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME huawei-honor-6-test COMMAND huawei-honor-6-test)

    ADD_EXECUTABLE(lenovo-a6600-plus-test test/mock/lenovo-a6600-plus.cc)
    TARGET_INCLUDE_DIRECTORIES(lenovo-a6600-plus-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(lenovo-a6600-plus-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME lenovo-a6600-plus-test COMMAND lenovo-a6600-plus-test)

    ADD_EXECUTABLE(lenovo-vibe-x2-test test/mock/lenovo-vibe-x2.cc)
    TARGET_INCLUDE_DIRECTORIES(lenovo-vibe-x2-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(lenovo-vibe-x2-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME lenovo-vibe-x2-test COMMAND lenovo-vibe-x2-test)

    ADD_EXECUTABLE(lg-k10-eu-test test/mock/lg-k10-eu.cc)
    TARGET_INCLUDE_DIRECTORIES(lg-k10-eu-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(lg-k10-eu-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME lg-k10-eu-test COMMAND lg-k10-eu-test)

    ADD_EXECUTABLE(lg-optimus-g-pro-test test/mock/lg-optimus-g-pro.cc)
    TARGET_INCLUDE_DIRECTORIES(lg-optimus-g-pro-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(lg-optimus-g-pro-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME lg-optimus-g-pro-test COMMAND lg-optimus-g-pro-test)

    ADD_EXECUTABLE(moto-e-gen1-test test/mock/moto-e-gen1.cc)
    TARGET_INCLUDE_DIRECTORIES(moto-e-gen1-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(moto-e-gen1-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME moto-e-gen1-test COMMAND moto-e-gen1-test)

    ADD_EXECUTABLE(moto-g-gen1-test test/mock/moto-g-gen1.cc)
    TARGET_INCLUDE_DIRECTORIES(moto-g-gen1-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(moto-g-gen1-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME moto-g-gen1-test COMMAND moto-g-gen1-test)

    ADD_EXECUTABLE(moto-g-gen2-test test/mock/moto-g-gen2.cc)
    TARGET_INCLUDE_DIRECTORIES(moto-g-gen2-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(moto-g-gen2-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME moto-g-gen2-test COMMAND moto-g-gen2-test)

    ADD_EXECUTABLE(moto-g-gen3-test test/mock/moto-g-gen3.cc)
    TARGET_INCLUDE_DIRECTORIES(moto-g-gen3-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(moto-g-gen3-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME moto-g-gen3-test COMMAND moto-g-gen3-test)

    ADD_EXECUTABLE(moto-g-gen4-test test/mock/moto-g-gen4.cc)
    TARGET_INCLUDE_DIRECTORIES(moto-g-gen4-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(moto-g-gen4-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME moto-g-gen4-test COMMAND moto-g-gen4-test)

    ADD_EXECUTABLE(moto-g-gen5-test test/mock/moto-g-gen5.cc)
    TARGET_INCLUDE_DIRECTORIES(moto-g-gen5-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(moto-g-gen5-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME moto-g-gen5-test COMMAND moto-g-gen5-test)

    ADD_EXECUTABLE(nexus-s-test test/mock/nexus-s.cc)
    TARGET_INCLUDE_DIRECTORIES(nexus-s-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(nexus-s-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME nexus-s-test COMMAND nexus-s-test)

    ADD_EXECUTABLE(nexus4-test test/mock/nexus4.cc)
    TARGET_INCLUDE_DIRECTORIES(nexus4-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(nexus4-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME nexus4-test COMMAND nexus4-test)

    ADD_EXECUTABLE(nexus6-test test/mock/nexus6.cc)
    TARGET_INCLUDE_DIRECTORIES(nexus6-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(nexus6-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME nexus6-test COMMAND nexus6-test)

    ADD_EXECUTABLE(nexus10-test test/mock/nexus10.cc)
    TARGET_INCLUDE_DIRECTORIES(nexus10-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(nexus10-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME nexus10-test COMMAND nexus10-test)

    ADD_EXECUTABLE(padcod-10.1-test test/mock/padcod-10.1.cc)
    TARGET_INCLUDE_DIRECTORIES(padcod-10.1-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(padcod-10.1-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME padcod-10.1-test COMMAND padcod-10.1-test)

    ADD_EXECUTABLE(xiaomi-redmi-2a-test test/mock/xiaomi-redmi-2a.cc)
    TARGET_INCLUDE_DIRECTORIES(xiaomi-redmi-2a-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(xiaomi-redmi-2a-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME xiaomi-redmi-2a-test COMMAND xiaomi-redmi-2a-test)

    ADD_EXECUTABLE(xperia-sl-test test/mock/xperia-sl.cc)
    TARGET_INCLUDE_DIRECTORIES(xperia-sl-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(xperia-sl-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME xperia-sl-test COMMAND xperia-sl-test)
  ENDIF()

  IF(CMAKE_SYSTEM_NAME STREQUAL "Android" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv5te|armv7-a|aarch64)$")
    ADD_EXECUTABLE(alcatel-revvl-test test/mock/alcatel-revvl.cc)
    TARGET_INCLUDE_DIRECTORIES(alcatel-revvl-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(alcatel-revvl-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME alcatel-revvl-test COMMAND alcatel-revvl-test)

    ADD_EXECUTABLE(galaxy-a8-2018-test test/mock/galaxy-a8-2018.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-a8-2018-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-a8-2018-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-a8-2018-test COMMAND galaxy-a8-2018-test)

    ADD_EXECUTABLE(galaxy-c9-pro-test test/mock/galaxy-c9-pro.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-c9-pro-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-c9-pro-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-c9-pro-test COMMAND galaxy-c9-pro-test)

    ADD_EXECUTABLE(galaxy-s6-test test/mock/galaxy-s6.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s6-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s6-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s6-test COMMAND galaxy-s6-test)

    ADD_EXECUTABLE(galaxy-s7-us-test test/mock/galaxy-s7-us.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s7-us-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s7-us-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s7-us-test COMMAND galaxy-s7-us-test)

    ADD_EXECUTABLE(galaxy-s7-global-test test/mock/galaxy-s7-global.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s7-global-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s7-global-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s7-global-test COMMAND galaxy-s7-global-test)

    ADD_EXECUTABLE(galaxy-s8-us-test test/mock/galaxy-s8-us.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s8-us-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s8-us-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s8-us-test COMMAND galaxy-s8-us-test)

    ADD_EXECUTABLE(galaxy-s8-global-test test/mock/galaxy-s8-global.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s8-global-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s8-global-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s8-global-test COMMAND galaxy-s8-global-test)

    ADD_EXECUTABLE(galaxy-s9-us-test test/mock/galaxy-s9-us.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s9-us-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s9-us-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s9-us-test COMMAND galaxy-s9-us-test)

    ADD_EXECUTABLE(galaxy-s9-global-test test/mock/galaxy-s9-global.cc)
    TARGET_INCLUDE_DIRECTORIES(galaxy-s9-global-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(galaxy-s9-global-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME galaxy-s9-global-test COMMAND galaxy-s9-global-test)

    ADD_EXECUTABLE(huawei-mate-8-test test/mock/huawei-mate-8.cc)
    TARGET_INCLUDE_DIRECTORIES(huawei-mate-8-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(huawei-mate-8-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME huawei-mate-8-test COMMAND huawei-mate-8-test)

    ADD_EXECUTABLE(huawei-mate-9-test test/mock/huawei-mate-9.cc)
    TARGET_INCLUDE_DIRECTORIES(huawei-mate-9-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(huawei-mate-9-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME huawei-mate-9-test COMMAND huawei-mate-9-test)

    ADD_EXECUTABLE(huawei-mate-10-test test/mock/huawei-mate-10.cc)
    TARGET_INCLUDE_DIRECTORIES(huawei-mate-10-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(huawei-mate-10-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME huawei-mate-10-test COMMAND huawei-mate-10-test)

    ADD_EXECUTABLE(huawei-mate-20-test test/mock/huawei-mate-20.cc)
    TARGET_INCLUDE_DIRECTORIES(huawei-mate-20-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(huawei-mate-20-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME huawei-mate-20-test COMMAND huawei-mate-20-test)

    ADD_EXECUTABLE(huawei-p8-lite-test test/mock/huawei-p8-lite.cc)
    TARGET_INCLUDE_DIRECTORIES(huawei-p8-lite-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(huawei-p8-lite-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME huawei-p8-lite-test COMMAND huawei-p8-lite-test)

    ADD_EXECUTABLE(huawei-p9-lite-test test/mock/huawei-p9-lite.cc)
    TARGET_INCLUDE_DIRECTORIES(huawei-p9-lite-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(huawei-p9-lite-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME huawei-p9-lite-test COMMAND huawei-p9-lite-test)

    ADD_EXECUTABLE(huawei-p20-pro-test test/mock/huawei-p20-pro.cc)
    TARGET_INCLUDE_DIRECTORIES(huawei-p20-pro-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(huawei-p20-pro-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME huawei-p20-pro-test COMMAND huawei-p20-pro-test)

    ADD_EXECUTABLE(iconia-one-10-test test/mock/iconia-one-10.cc)
    TARGET_INCLUDE_DIRECTORIES(iconia-one-10-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(iconia-one-10-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME iconia-one-10-test COMMAND iconia-one-10-test)

    ADD_EXECUTABLE(meizu-pro-6-test test/mock/meizu-pro-6.cc)
    TARGET_INCLUDE_DIRECTORIES(meizu-pro-6-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(meizu-pro-6-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME meizu-pro-6-test COMMAND meizu-pro-6-test)

    ADD_EXECUTABLE(meizu-pro-6s-test test/mock/meizu-pro-6s.cc)
    TARGET_INCLUDE_DIRECTORIES(meizu-pro-6s-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(meizu-pro-6s-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME meizu-pro-6s-test COMMAND meizu-pro-6s-test)

    ADD_EXECUTABLE(meizu-pro-7-plus-test test/mock/meizu-pro-7-plus.cc)
    TARGET_INCLUDE_DIRECTORIES(meizu-pro-7-plus-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(meizu-pro-7-plus-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME meizu-pro-7-plus-test COMMAND meizu-pro-7-plus-test)

    ADD_EXECUTABLE(nexus5x-test test/mock/nexus5x.cc)
    TARGET_INCLUDE_DIRECTORIES(nexus5x-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(nexus5x-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME nexus5x-test COMMAND nexus5x-test)

    ADD_EXECUTABLE(nexus6p-test test/mock/nexus6p.cc)
    TARGET_INCLUDE_DIRECTORIES(nexus6p-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(nexus6p-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME nexus6p-test COMMAND nexus6p-test)

    ADD_EXECUTABLE(nexus9-test test/mock/nexus9.cc)
    TARGET_INCLUDE_DIRECTORIES(nexus9-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(nexus9-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME nexus9-test COMMAND nexus9-test)

    ADD_EXECUTABLE(oneplus-3t-test test/mock/oneplus-3t.cc)
    TARGET_INCLUDE_DIRECTORIES(oneplus-3t-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(oneplus-3t-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME oneplus-3t-test COMMAND oneplus-3t-test)

    ADD_EXECUTABLE(oneplus-5-test test/mock/oneplus-5.cc)
    TARGET_INCLUDE_DIRECTORIES(oneplus-5-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(oneplus-5-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME oneplus-5-test COMMAND oneplus-5-test)

    ADD_EXECUTABLE(oneplus-5t-test test/mock/oneplus-5t.cc)
    TARGET_INCLUDE_DIRECTORIES(oneplus-5t-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(oneplus-5t-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME oneplus-5t-test COMMAND oneplus-5t-test)

    ADD_EXECUTABLE(oppo-a37-test test/mock/oppo-a37.cc)
    TARGET_INCLUDE_DIRECTORIES(oppo-a37-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(oppo-a37-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME oppo-a37-test COMMAND oppo-a37-test)

    ADD_EXECUTABLE(oppo-r9-test test/mock/oppo-r9.cc)
    TARGET_INCLUDE_DIRECTORIES(oppo-r9-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(oppo-r9-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME oppo-r9-test COMMAND oppo-r9-test)

    ADD_EXECUTABLE(oppo-r15-test test/mock/oppo-r15.cc)
    TARGET_INCLUDE_DIRECTORIES(oppo-r15-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(oppo-r15-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME oppo-r15-test COMMAND oppo-r15-test)

    ADD_EXECUTABLE(pixel-test test/mock/pixel.cc)
    TARGET_INCLUDE_DIRECTORIES(pixel-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(pixel-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME pixel-test COMMAND pixel-test)

    ADD_EXECUTABLE(pixel-c-test test/mock/pixel-c.cc)
    TARGET_INCLUDE_DIRECTORIES(pixel-c-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(pixel-c-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME pixel-c-test COMMAND pixel-c-test)

    ADD_EXECUTABLE(pixel-xl-test test/mock/pixel-xl.cc)
    TARGET_INCLUDE_DIRECTORIES(pixel-xl-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(pixel-xl-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME pixel-xl-test COMMAND pixel-xl-test)

    ADD_EXECUTABLE(pixel-2-xl-test test/mock/pixel-2-xl.cc)
    TARGET_INCLUDE_DIRECTORIES(pixel-2-xl-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(pixel-2-xl-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME pixel-2-xl-test COMMAND pixel-2-xl-test)

    ADD_EXECUTABLE(xiaomi-mi-5c-test test/mock/xiaomi-mi-5c.cc)
    TARGET_INCLUDE_DIRECTORIES(xiaomi-mi-5c-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(xiaomi-mi-5c-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME xiaomi-mi-5c-test COMMAND xiaomi-mi-5c-test)

    ADD_EXECUTABLE(xiaomi-redmi-note-3-test test/mock/xiaomi-redmi-note-3.cc)
    TARGET_INCLUDE_DIRECTORIES(xiaomi-redmi-note-3-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(xiaomi-redmi-note-3-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME xiaomi-redmi-note-3-test COMMAND xiaomi-redmi-note-3-test)

    ADD_EXECUTABLE(xiaomi-redmi-note-4-test test/mock/xiaomi-redmi-note-4.cc)
    TARGET_INCLUDE_DIRECTORIES(xiaomi-redmi-note-4-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(xiaomi-redmi-note-4-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME xiaomi-redmi-note-4-test COMMAND xiaomi-redmi-note-4-test)

    ADD_EXECUTABLE(xperia-c4-dual-test test/mock/xperia-c4-dual.cc)
    TARGET_INCLUDE_DIRECTORIES(xperia-c4-dual-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(xperia-c4-dual-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME xperia-c4-dual-test COMMAND xperia-c4-dual-test)
  ENDIF()

  IF(CMAKE_SYSTEM_NAME STREQUAL "Android" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(i686|x86_64)$")
    ADD_EXECUTABLE(alldocube-iwork8-test test/mock/alldocube-iwork8.cc)
    TARGET_INCLUDE_DIRECTORIES(alldocube-iwork8-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(alldocube-iwork8-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME alldocube-iwork8-test COMMAND alldocube-iwork8-test)

    ADD_EXECUTABLE(leagoo-t5c-test test/mock/leagoo-t5c.cc)
    TARGET_INCLUDE_DIRECTORIES(leagoo-t5c-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(leagoo-t5c-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME leagoo-t5c-test COMMAND leagoo-t5c-test)

    ADD_EXECUTABLE(memo-pad-7-test test/mock/memo-pad-7.cc)
    TARGET_INCLUDE_DIRECTORIES(memo-pad-7-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(memo-pad-7-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME memo-pad-7-test COMMAND memo-pad-7-test)

    ADD_EXECUTABLE(zenfone-c-test test/mock/zenfone-c.cc)
    TARGET_INCLUDE_DIRECTORIES(zenfone-c-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(zenfone-c-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME zenfone-c-test COMMAND zenfone-c-test)

    ADD_EXECUTABLE(zenfone-2-test test/mock/zenfone-2.cc)
    TARGET_INCLUDE_DIRECTORIES(zenfone-2-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(zenfone-2-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME zenfone-2-test COMMAND zenfone-2-test)

    ADD_EXECUTABLE(zenfone-2e-test test/mock/zenfone-2e.cc)
    TARGET_INCLUDE_DIRECTORIES(zenfone-2e-test BEFORE PRIVATE test/mock)
    TARGET_LINK_LIBRARIES(zenfone-2e-test PRIVATE cpuinfo_mock gtest)
    ADD_TEST(NAME zenfone-2e-test COMMAND zenfone-2e-test)
  ENDIF()
ENDIF()

# ---[ cpuinfo unit tests
IF(CPUINFO_SUPPORTED_PLATFORM AND CPUINFO_BUILD_UNIT_TESTS)
  ADD_EXECUTABLE(init-test test/init.cc)
  CPUINFO_TARGET_ENABLE_CXX11(init-test)
  CPUINFO_TARGET_RUNTIME_LIBRARY(init-test)
  TARGET_LINK_LIBRARIES(init-test PRIVATE cpuinfo gtest gtest_main)
  ADD_TEST(NAME init-test COMMAND init-test)

  IF(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    ADD_EXECUTABLE(get-current-test test/get-current.cc)
    CPUINFO_TARGET_ENABLE_CXX11(get-current-test)
    CPUINFO_TARGET_RUNTIME_LIBRARY(get-current-test)
    TARGET_LINK_LIBRARIES(get-current-test PRIVATE cpuinfo gtest gtest_main)
    ADD_TEST(NAME get-current-test COMMAND get-current-test)
  ENDIF()

  IF(CPUINFO_TARGET_PROCESSOR MATCHES "^(i[3-6]86|AMD64|x86(_64)?)$")
    ADD_EXECUTABLE(brand-string-test test/name/brand-string.cc)
    CPUINFO_TARGET_ENABLE_CXX11(brand-string-test)
    CPUINFO_TARGET_RUNTIME_LIBRARY(brand-string-test)
    TARGET_LINK_LIBRARIES(brand-string-test PRIVATE cpuinfo_internals gtest gtest_main)
    ADD_TEST(NAME brand-string-test COMMAND brand-string-test)
  ENDIF()

  IF(CMAKE_SYSTEM_NAME STREQUAL "Android" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv[5-8].*|aarch64)$")
    ADD_LIBRARY(android_properties_interface STATIC test/name/android-properties-interface.c)
    CPUINFO_TARGET_ENABLE_C99(android_properties_interface)
    CPUINFO_TARGET_RUNTIME_LIBRARY(android_properties_interface)
    TARGET_LINK_LIBRARIES(android_properties_interface PRIVATE cpuinfo_internals)

    ADD_EXECUTABLE(chipset-test
      test/name/proc-cpuinfo-hardware.cc
      test/name/ro-product-board.cc
      test/name/ro-board-platform.cc
      test/name/ro-mediatek-platform.cc
      test/name/ro-arch.cc
      test/name/ro-chipname.cc
      test/name/android-properties.cc)
    CPUINFO_TARGET_ENABLE_CXX11(chipset-test)
    CPUINFO_TARGET_RUNTIME_LIBRARY(chipset-test)
    TARGET_LINK_LIBRARIES(chipset-test PRIVATE android_properties_interface gtest gtest_main)
    ADD_TEST(NAME chipset-test COMMAND chipset-test)

    ADD_EXECUTABLE(cache-test test/arm-cache.cc)
    CPUINFO_TARGET_ENABLE_CXX11(cache-test)
    CPUINFO_TARGET_RUNTIME_LIBRARY(cache-test)
    TARGET_COMPILE_DEFINITIONS(cache-test PRIVATE __STDC_LIMIT_MACROS=1 __STDC_CONSTANT_MACROS=1)
    TARGET_LINK_LIBRARIES(cache-test PRIVATE cpuinfo_internals gtest gtest_main)
    ADD_TEST(NAME cache-test COMMAND cache-test)
  ENDIF()
ENDIF()

# ---[ Helper and debug tools
IF(CPUINFO_SUPPORTED_PLATFORM AND CPUINFO_BUILD_TOOLS)
  ADD_EXECUTABLE(isa-info tools/isa-info.c)
  CPUINFO_TARGET_ENABLE_C99(isa-info)
  CPUINFO_TARGET_RUNTIME_LIBRARY(isa-info)
  TARGET_LINK_LIBRARIES(isa-info PRIVATE cpuinfo)
  INSTALL(TARGETS isa-info RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  ADD_EXECUTABLE(cpu-info tools/cpu-info.c)
  CPUINFO_TARGET_ENABLE_C99(cpu-info)
  CPUINFO_TARGET_RUNTIME_LIBRARY(cpu-info)
  TARGET_LINK_LIBRARIES(cpu-info PRIVATE cpuinfo)
  INSTALL(TARGETS cpu-info RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  ADD_EXECUTABLE(cache-info tools/cache-info.c)
  CPUINFO_TARGET_ENABLE_C99(cache-info)
  CPUINFO_TARGET_RUNTIME_LIBRARY(cache-info)
  TARGET_LINK_LIBRARIES(cache-info PRIVATE cpuinfo)
  INSTALL(TARGETS cache-info RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  IF(CMAKE_SYSTEM_NAME MATCHES "^(Android|Linux)$" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv[5-8].*|aarch64)$")
    ADD_EXECUTABLE(auxv-dump tools/auxv-dump.c)
    CPUINFO_TARGET_ENABLE_C99(auxv-dump)
    CPUINFO_TARGET_RUNTIME_LIBRARY(auxv-dump)
    TARGET_LINK_LIBRARIES(auxv-dump PRIVATE ${CMAKE_DL_LIBS} cpuinfo)

    ADD_EXECUTABLE(cpuinfo-dump tools/cpuinfo-dump.c)
    CPUINFO_TARGET_ENABLE_C99(cpuinfo-dump)
    CPUINFO_TARGET_RUNTIME_LIBRARY(cpuinfo-dump)
  ENDIF()

  IF(CPUINFO_TARGET_PROCESSOR MATCHES "^(i[3-6]86|AMD64|x86(_64)?)$")
    ADD_EXECUTABLE(cpuid-dump tools/cpuid-dump.c)
    CPUINFO_TARGET_ENABLE_C99(cpuid-dump)
    CPUINFO_TARGET_RUNTIME_LIBRARY(cpuid-dump)
    TARGET_INCLUDE_DIRECTORIES(cpuid-dump BEFORE PRIVATE src)
    TARGET_INCLUDE_DIRECTORIES(cpuid-dump BEFORE PRIVATE include)
    INSTALL(TARGETS cpuid-dump RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  ENDIF()
ENDIF()

# ---[ pkg-config manifest. This is mostly from JsonCpp...
IF(CPUINFO_BUILD_PKG_CONFIG)

  FUNCTION(JOIN_PATHS joined_path first_path_segment)
    SET(temp_path "${first_path_segment}")
    FOREACH(current_segment IN LISTS ARGN)
      IF(NOT ("${current_segment}" STREQUAL ""))
        IF(IS_ABSOLUTE "${current_segment}")
          SET(temp_path "${current_segment}")
        ELSE()
          SET(temp_path "${temp_path}/${current_segment}")
        ENDIF()
      ENDIF()
    ENDFOREACH()
    SET(${joined_path} "${temp_path}" PARENT_SCOPE)
  ENDFUNCTION()

  JOIN_PATHS(libdir_for_pc_file "\${exec_prefix}" "${CMAKE_INSTALL_LIBDIR}")
  JOIN_PATHS(includedir_for_pc_file "\${prefix}" "${CMAKE_INSTALL_INCLUDEDIR}")

  CONFIGURE_FILE(
    "libcpuinfo.pc.in"
    "libcpuinfo.pc"
    @ONLY)

  INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/libcpuinfo.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

ENDIF()
