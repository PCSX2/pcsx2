# $Id: $
#
# For a "How-To" please refer to the Portaudio documentation at:
# http://www.portaudio.com/trac/wiki/TutorialDir/Compile/CMake
#

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# Check if the user is building PortAudio stand-alone or as part of a larger
# project. If this is part of a larger project (i.e. the CMakeLists.txt has
# been imported by some other CMakeLists.txt), we don't want to trump over
# the top of that project's global settings.
IF(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_LIST_DIR})
  IF(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    MESSAGE(STATUS "Setting CMAKE_BUILD_TYPE type to 'Debug' as none was specified.")
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    SET_PROPERTY(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
  ENDIF()

  PROJECT(portaudio)

  SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

  IF(WIN32 AND MSVC)
    OPTION(PA_DLL_LINK_WITH_STATIC_RUNTIME "Link with static runtime libraries (minimizes runtime dependencies)" ON)
    IF(PA_DLL_LINK_WITH_STATIC_RUNTIME)
      FOREACH(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        IF(${flag_var} MATCHES "/MD")
          STRING(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        ENDIF()
      ENDFOREACH()
    ENDIF()
  ENDIF()
ENDIF()

SET(PA_PKGCONFIG_VERSION 19)

# Most of the code from this point onwards is related to populating the
# following variables:
#   PA_PUBLIC_INCLUDES - This contains the list of public PortAudio header
#       files. These files will be copied into /include paths on Unix'y
#       systems when "make install" is invoked.
#   PA_PRIVATE_INCLUDES - This contains the list of header files which
#       are not part of PortAudio, but are required by the various hostapis.
#       It is only used by CMake IDE generators (like Visual Studio) to
#       provide quick-links to useful headers. It has no impact on build
#       output.
#   PA_PRIVATE_INCLUDE_PATHS - This contains the list of include paths which
#       will be passed to the compiler while PortAudio is being built which
#       are not required by applications using the PortAudio API.
#   PA_PRIVATE_COMPILE_DEFINITIONS - This contains a list of preprocessor
#       macro definitions which will be set when compiling PortAudio source
#       files.
#   PA_SOURCES - This contains the list of source files which will be built
#       into the static and shared PortAudio libraries.
#   PA_NON_UNICODE_SOURCES - This also contains a list of source files which
#       will be build into the static and shared PortAudio libraries. However,
#       these sources will not have any unicode compiler definitions added
#       to them. This list should only contain external source dependencies.
#   PA_EXTRA_SHARED_SOURCES - Contains a list of extra files which will be
#       associated only with the shared PortAudio library. This only seems
#       relevant for Windows shared libraries which require a list of export
#       symbols.
# Where other PA_* variables are set, these are almost always only used to
# preserve the historic SOURCE_GROUP behavior (which again only has an impact
# on IDE-style generators for visual appearance) or store the output of
# find_library() calls.

SET(PA_COMMON_INCLUDES
  src/common/pa_allocation.h
  src/common/pa_converters.h
  src/common/pa_cpuload.h
  src/common/pa_debugprint.h
  src/common/pa_dither.h
  src/common/pa_endianness.h
  src/common/pa_hostapi.h
  src/common/pa_memorybarrier.h
  src/common/pa_process.h
  src/common/pa_ringbuffer.h
  src/common/pa_stream.h
  src/common/pa_trace.h
  src/common/pa_types.h
  src/common/pa_util.h
)

SET(PA_COMMON_SOURCES
  src/common/pa_allocation.c
  src/common/pa_converters.c
  src/common/pa_cpuload.c
  src/common/pa_debugprint.c
  src/common/pa_dither.c
  src/common/pa_front.c
  src/common/pa_process.c
  src/common/pa_ringbuffer.c
  src/common/pa_stream.c
  src/common/pa_trace.c
)

SOURCE_GROUP("common" FILES ${PA_COMMON_INCLUDES} ${PA_COMMON_SOURCES})

SET(PA_PUBLIC_INCLUDES include/portaudio.h)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake_support)

SET(PA_SKELETON_SOURCES src/hostapi/skeleton/pa_hostapi_skeleton.c)
SOURCE_GROUP("hostapi\\skeleton" ${PA_SKELETON_SOURCES})
SET(PA_SOURCES ${PA_COMMON_SOURCES} ${PA_SKELETON_SOURCES})
SET(PA_PRIVATE_INCLUDE_PATHS src/common ${CMAKE_CURRENT_BINARY_DIR})

IF(WIN32)
  SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} _CRT_SECURE_NO_WARNINGS)

  SET(PA_PLATFORM_SOURCES
    src/os/win/pa_win_hostapis.c
    src/os/win/pa_win_util.c
    src/os/win/pa_win_waveformat.c
    src/os/win/pa_win_wdmks_utils.c
    src/os/win/pa_win_coinitialize.c)
  SET(PA_PLATFORM_INCLUDES
    src/os/win/pa_win_coinitialize.h
    src/os/win/pa_win_wdmks_utils.h)

  IF(MSVC)
    SET(PA_PLATFORM_SOURCES ${PA_PLATFORM_SOURCES} src/os/win/pa_x86_plain_converters.c)
    SET(PA_PLATFORM_INCLUDES ${PA_PLATFORM_INCLUDES} src/os/win/pa_x86_plain_converters.h)
  ELSE()
    SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} _WIN32_WINNT=0x0501 WINVER=0x0501)
    SET(DEF_EXCLUDE_X86_PLAIN_CONVERTERS ";")
  ENDIF()

  SOURCE_GROUP("os\\win" FILES ${PA_PLATFORM_SOURCES} ${PA_PLATFORM_INCLUDES})
  SET(PA_SOURCES ${PA_SOURCES} ${PA_PLATFORM_SOURCES})
  SET(PA_PRIVATE_INCLUDES ${PA_PRIVATE_INCLUDES} ${PA_PLATFORM_INCLUDES})
  SET(PA_PRIVATE_INCLUDE_PATHS ${PA_PRIVATE_INCLUDE_PATHS} src/os/win)

  SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} winmm)

  # Try to find ASIO SDK (assumes that portaudio and asiosdk folders are side-by-side, see
  # http://www.portaudio.com/trac/wiki/TutorialDir/Compile/WindowsASIOMSVC)
  FIND_PACKAGE(ASIOSDK)
  IF(ASIOSDK_FOUND)
    OPTION(PA_USE_ASIO "Enable support for ASIO" ON)
  ELSE()
    OPTION(PA_USE_ASIO "Enable support for ASIO" OFF)
  ENDIF()
  IF(PA_USE_ASIO)
    SET(PA_PRIVATE_INCLUDE_PATHS ${PA_PRIVATE_INCLUDE_PATHS} ${ASIOSDK_ROOT_DIR}/common)
    SET(PA_PRIVATE_INCLUDE_PATHS ${PA_PRIVATE_INCLUDE_PATHS} ${ASIOSDK_ROOT_DIR}/host)
    SET(PA_PRIVATE_INCLUDE_PATHS ${PA_PRIVATE_INCLUDE_PATHS} ${ASIOSDK_ROOT_DIR}/host/pc)
    SET(PA_ASIO_SOURCES src/hostapi/asio/pa_asio.cpp src/hostapi/asio/iasiothiscallresolver.cpp)
    SET(PA_ASIOSDK_SOURCES ${ASIOSDK_ROOT_DIR}/common/asio.cpp ${ASIOSDK_ROOT_DIR}/host/pc/asiolist.cpp ${ASIOSDK_ROOT_DIR}/host/asiodrivers.cpp)
    SOURCE_GROUP("hostapi\\ASIO" FILES ${PA_ASIO_SOURCES})
    SOURCE_GROUP("hostapi\\ASIO\\ASIOSDK" FILES ${PA_ASIOSDK_SOURCES})
    SET(PA_PUBLIC_INCLUDES ${PA_PUBLIC_INCLUDES} include/pa_asio.h)
    SET(PA_SOURCES ${PA_SOURCES} ${PA_ASIO_SOURCES})
    SET(PA_NON_UNICODE_SOURCES ${PA_NON_UNICODE_SOURCES} ${PA_ASIOSDK_SOURCES})
    SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} ole32 uuid)
  ELSE()
    # Set variables for DEF file expansion
    SET(DEF_EXCLUDE_ASIO_SYMBOLS ";")
  ENDIF()

  # Try to find DirectX SDK
  FIND_PACKAGE(DXSDK)
  IF(DXSDK_FOUND)
    OPTION(PA_USE_DS "Enable support for DirectSound" ON)
  ELSE()
    OPTION(PA_USE_DS "Enable support for DirectSound" OFF)
  ENDIF()
  IF(PA_USE_DS)
    OPTION(PA_USE_DIRECTSOUNDFULLDUPLEXCREATE "Use DirectSound full duplex create" ON)
    MARK_AS_ADVANCED(PA_USE_DIRECTSOUNDFULLDUPLEXCREATE)
    IF(PA_USE_DIRECTSOUNDFULLDUPLEXCREATE)
      SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} PAWIN_USE_DIRECTSOUNDFULLDUPLEXCREATE)
    ENDIF()
    SET(PA_PRIVATE_INCLUDE_PATHS ${PA_PRIVATE_INCLUDE_PATHS} ${DXSDK_INCLUDE_DIR})
    SET(PA_DS_INCLUDES src/hostapi/dsound/pa_win_ds_dynlink.h)
    SET(PA_DS_SOURCES src/hostapi/dsound/pa_win_ds.c src/hostapi/dsound/pa_win_ds_dynlink.c)
    SOURCE_GROUP("hostapi\\dsound" FILES ${PA_DS_INCLUDES} ${PA_DS_SOURCES})
    SET(PA_PUBLIC_INCLUDES ${PA_PUBLIC_INCLUDES} include/pa_win_ds.h)
    SET(PA_PRIVATE_INCLUDES ${PA_PRIVATE_INCLUDES} ${PA_DS_INCLUDES})
    SET(PA_SOURCES ${PA_SOURCES} ${PA_DS_SOURCES})

    # If we use DirectSound, we need this for the library to be found (if not in VS project settings)
    IF(DXSDK_FOUND)
      SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} ${DXSDK_DSOUND_LIBRARY})
    ENDIF()
  ENDIF()

  OPTION(PA_USE_WMME "Enable support for MME" ON)
  IF(PA_USE_WMME)
    SET(PA_WMME_SOURCES src/hostapi/wmme/pa_win_wmme.c)
    SOURCE_GROUP("hostapi\\wmme" FILES ${PA_WMME_SOURCES})
    SET(PA_PUBLIC_INCLUDES ${PA_PUBLIC_INCLUDES} include/pa_win_wmme.h)
    SET(PA_SOURCES ${PA_SOURCES} ${PA_WMME_SOURCES})
    SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} ole32 uuid)
  ENDIF()

  IF(MSVC)
    OPTION(PA_USE_WASAPI "Enable support for WASAPI" ON)
  ELSE()
    # I was unable to get WASAPI to compile outside of Visual Studio. If
    # anyone can figure out how to make this work with MinGW, please fix me.
    SET(PA_USE_WASAPI OFF)
  ENDIF()
  IF(PA_USE_WASAPI)
    SET(PA_WASAPI_SOURCES src/hostapi/wasapi/pa_win_wasapi.c)
    SOURCE_GROUP("hostapi\\wasapi" FILES ${PA_WASAPI_SOURCES})
    SET(PA_PUBLIC_INCLUDES ${PA_PUBLIC_INCLUDES} include/pa_win_wasapi.h)
    SET(PA_SOURCES ${PA_SOURCES} ${PA_WASAPI_SOURCES})
    SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} ole32 uuid)
    IF(NOT MSVC)
      SET(PA_PRIVATE_INCLUDE_PATHS ${PA_PRIVATE_INCLUDE_PATHS} src/hostapi/wasapi/mingw-include)
    ENDIF()
  ELSE()
    SET(DEF_EXCLUDE_WASAPI_SYMBOLS ";")
  ENDIF()

  OPTION(PA_USE_WDMKS "Enable support for WDMKS" ON)
  IF(PA_USE_WDMKS)
    SET(PA_WDMKS_SOURCES src/hostapi/wdmks/pa_win_wdmks.c)
    SOURCE_GROUP("hostapi\\wdmks" FILES ${PA_WDMKS_SOURCES})
    SET(PA_PUBLIC_INCLUDES ${PA_PUBLIC_INCLUDES} include/pa_win_wdmks.h)
    SET(PA_SOURCES ${PA_SOURCES} ${PA_WDMKS_SOURCES})
    SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} setupapi ole32 uuid)
  ENDIF()

  OPTION(PA_USE_WDMKS_DEVICE_INFO "Use WDM/KS API for device info" ON)
  MARK_AS_ADVANCED(PA_USE_WDMKS_DEVICE_INFO)
  IF(PA_USE_WDMKS_DEVICE_INFO)
    SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} PAWIN_USE_WDMKS_DEVICE_INFO)
  ENDIF()

  SET(GENERATED_MESSAGE "CMake generated file, do NOT edit! Use CMake-GUI to change configuration instead.")
  CONFIGURE_FILE(cmake_support/template_portaudio.def ${CMAKE_CURRENT_BINARY_DIR}/portaudio_cmake.def @ONLY)
  CONFIGURE_FILE(cmake_support/options_cmake.h.in ${CMAKE_CURRENT_BINARY_DIR}/options_cmake.h @ONLY)
  SET(PA_PRIVATE_INCLUDES ${PA_PRIVATE_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR}/options_cmake.h)
  SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} PORTAUDIO_CMAKE_GENERATED)
  SOURCE_GROUP("cmake_generated" FILES ${CMAKE_CURRENT_BINARY_DIR}/portaudio_cmake.def ${CMAKE_CURRENT_BINARY_DIR}/options_cmake.h)

  SET(PA_EXTRA_SHARED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/portaudio_cmake.def)

ELSE()

  SET(PA_PRIVATE_INCLUDE_PATHS ${PA_PRIVATE_INCLUDE_PATHS} src/os/unix)
  SET(PA_PLATFORM_SOURCES src/os/unix/pa_unix_hostapis.c src/os/unix/pa_unix_util.c)
  SOURCE_GROUP("os\\unix" FILES ${PA_PLATFORM_SOURCES})
  SET(PA_SOURCES ${PA_SOURCES} ${PA_PLATFORM_SOURCES})

  IF(APPLE)

    SET(CMAKE_MACOSX_RPATH 1)
    OPTION(PA_USE_COREAUDIO "Enable support for CoreAudio" ON)
    IF(PA_USE_COREAUDIO)
      SET(PA_COREAUDIO_SOURCES
        src/hostapi/coreaudio/pa_mac_core.c
        src/hostapi/coreaudio/pa_mac_core_blocking.c
        src/hostapi/coreaudio/pa_mac_core_utilities.c)
      SET(PA_COREAUDIO_INCLUDES
        src/hostapi/coreaudio/pa_mac_core_blocking.h
        src/hostapi/coreaudio/pa_mac_core_utilities.h)
      SOURCE_GROUP("hostapi\\coreaudio" FILES ${PA_COREAUDIO_SOURCES} ${PA_COREAUDIO_INCLUDES})
      SET(PA_PUBLIC_INCLUDES ${PA_PUBLIC_INCLUDES} include/pa_mac_core.h)
      SET(PA_PRIVATE_INCLUDES ${PA_PRIVATE_INCLUDES} ${PA_COREAUDIO_INCLUDES})
      SET(PA_SOURCES ${PA_SOURCES} ${PA_COREAUDIO_SOURCES})

      FIND_LIBRARY(COREAUDIO_LIBRARY CoreAudio REQUIRED)
      FIND_LIBRARY(AUDIOTOOLBOX_LIBRARY AudioToolbox REQUIRED)
      FIND_LIBRARY(AUDIOUNIT_LIBRARY AudioUnit REQUIRED)
      FIND_LIBRARY(CARBON_LIBRARY Carbon REQUIRED)
      MARK_AS_ADVANCED(COREAUDIO_LIBRARY AUDIOTOOLBOX_LIBRARY AUDIOUNIT_LIBRARY CARBON_LIBRARY)
      SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} ${COREAUDIO_LIBRARY} ${AUDIOTOOLBOX_LIBRARY} ${AUDIOUNIT_LIBRARY} ${CARBON_LIBRARY})
      SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} PA_USE_COREAUDIO)
      SET(PA_PKGCONFIG_LDFLAGS "${PA_PKGCONFIG_LDFLAGS} -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Carbon")
    ENDIF()

  ELSEIF(UNIX)

    FIND_PACKAGE(Jack)
    IF(JACK_FOUND)
      OPTION(PA_USE_JACK "Enable support for Jack" ON)
    ELSE()
      OPTION(PA_USE_JACK "Enable support for Jack" OFF)
    ENDIF()
    IF(PA_USE_JACK)
      SET(PA_PRIVATE_INCLUDE_PATHS ${PA_PRIVATE_INCLUDE_PATHS} ${JACK_INCLUDE_DIRS})
      SET(PA_JACK_SOURCES src/hostapi/jack/pa_jack.c)
      SOURCE_GROUP("hostapi\\JACK" FILES ${PA_JACK_SOURCES})
      SET(PA_PUBLIC_INCLUDES ${PA_PUBLIC_INCLUDES} include/pa_jack.h)
      SET(PA_SOURCES ${PA_SOURCES} ${PA_JACK_SOURCES})
      SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} PA_USE_JACK)
      SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} ${JACK_LIBRARIES})
      SET(PA_PKGCONFIG_LDFLAGS "${PA_PKGCONFIG_LDFLAGS} -ljack")
    ENDIF()

    FIND_PACKAGE(ALSA)
    IF(ALSA_FOUND)
      OPTION(PA_USE_ALSA "Enable support for ALSA" ON)
    ELSE()
      OPTION(PA_USE_ALSA "Enable support for ALSA" OFF)
    ENDIF()
    IF(PA_USE_ALSA)
      SET(PA_PRIVATE_INCLUDE_PATHS ${PA_PRIVATE_INCLUDE_PATHS} ${ALSA_INCLUDE_DIRS})
      SET(PA_ALSA_SOURCES src/hostapi/alsa/pa_linux_alsa.c)
      SOURCE_GROUP("hostapi\\ALSA" FILES ${PA_ALSA_SOURCES})
      SET(PA_PUBLIC_INCLUDES ${PA_PUBLIC_INCLUDES} include/pa_linux_alsa.h)
      SET(PA_SOURCES ${PA_SOURCES} ${PA_ALSA_SOURCES})
      SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} PA_USE_ALSA)
      SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} ${ALSA_LIBRARIES})
      SET(PA_PKGCONFIG_LDFLAGS "${PA_PKGCONFIG_LDFLAGS} -lasound")
    ENDIF()

  ENDIF()

  SET(PA_PKGCONFIG_LDFLAGS "${PA_PKGCONFIG_LDFLAGS} -lm -lpthread")
  SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} m pthread)

ENDIF()

SOURCE_GROUP("include" FILES ${PA_PUBLIC_INCLUDES})

SET(PA_INCLUDES ${PA_PRIVATE_INCLUDES} ${PA_PUBLIC_INCLUDES})

IF(WIN32)
  OPTION(PA_UNICODE_BUILD "Enable Portaudio Unicode build" ON)
  IF(PA_UNICODE_BUILD)
    SET_SOURCE_FILES_PROPERTIES(${PA_SOURCES} PROPERTIES COMPILE_DEFINITIONS "UNICODE;_UNICODE")
  ENDIF()
ENDIF()

OPTION(PA_ENABLE_DEBUG_OUTPUT "Enable debug output for Portaudio" OFF)
IF(PA_ENABLE_DEBUG_OUTPUT)
  SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} PA_ENABLE_DEBUG_OUTPUT)
ENDIF()

INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
IF(IS_BIG_ENDIAN)
  SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} PA_BIG_ENDIAN)
ELSE()
  SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} PA_LITTLE_ENDIAN)
ENDIF()

ADD_LIBRARY(portaudio SHARED ${PA_INCLUDES} ${PA_COMMON_INCLUDES} ${PA_SOURCES} ${PA_NON_UNICODE_SOURCES} ${PA_EXTRA_SHARED_SOURCES})
SET_PROPERTY(TARGET portaudio APPEND_STRING PROPERTY COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS})
TARGET_INCLUDE_DIRECTORIES(portaudio PRIVATE ${PA_PRIVATE_INCLUDE_PATHS})
TARGET_INCLUDE_DIRECTORIES(portaudio PUBLIC include)
TARGET_LINK_LIBRARIES(portaudio ${PA_LIBRARY_DEPENDENCIES})

ADD_LIBRARY(portaudio_static STATIC ${PA_INCLUDES} ${PA_COMMON_INCLUDES} ${PA_SOURCES} ${PA_NON_UNICODE_SOURCES})
SET_PROPERTY(TARGET portaudio_static APPEND_STRING PROPERTY COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS})
TARGET_INCLUDE_DIRECTORIES(portaudio_static PRIVATE ${PA_PRIVATE_INCLUDE_PATHS})
TARGET_INCLUDE_DIRECTORIES(portaudio_static PUBLIC include)
TARGET_LINK_LIBRARIES(portaudio_static ${PA_LIBRARY_DEPENDENCIES})

IF(WIN32 AND MSVC)
  OPTION(PA_CONFIG_LIB_OUTPUT_PATH "Make sure that output paths are kept neat" OFF)
  IF(CMAKE_CL_64)
    SET(TARGET_POSTFIX x64)
    IF(PA_CONFIG_LIB_OUTPUT_PATH)
      SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin/x64)
    ENDIF()
  ELSE()
    SET(TARGET_POSTFIX x86)
    IF(PA_CONFIG_LIB_OUTPUT_PATH)
      SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin/Win32)
    ENDIF()
  ENDIF()
  SET_TARGET_PROPERTIES(portaudio PROPERTIES OUTPUT_NAME portaudio_${TARGET_POSTFIX} FOLDER "Portaudio")
  SET_TARGET_PROPERTIES(portaudio_static PROPERTIES OUTPUT_NAME portaudio_static_${TARGET_POSTFIX} FOLDER "Portaudio")
ELSE()
  IF(APPLE AND CMAKE_VERSION VERSION_GREATER 3.4.2)
    OPTION(PA_OUTPUT_OSX_FRAMEWORK "Generate an OS X framework instead of the simple library" OFF)
    IF(PA_OUTPUT_OSX_FRAMEWORK)
      SET_TARGET_PROPERTIES(portaudio PROPERTIES
        FRAMEWORK TRUE
        MACOSX_FRAMEWORK_IDENTIFIER com.portaudio
        FRAMEWORK_VERSION A
        PUBLIC_HEADER "${PA_PUBLIC_INCLUDES}"
        VERSION 19.0
        SOVERSION 19.0)
    ENDIF()
  ENDIF()

  IF(NOT PA_OUTPUT_OSX_FRAMEWORK)
    CONFIGURE_FILE(cmake_support/portaudio-2.0.pc.in ${CMAKE_CURRENT_BINARY_DIR}/portaudio-2.0.pc @ONLY)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/portaudio-2.0.pc DESTINATION lib/pkgconfig)
    INSTALL(FILES ${PA_PUBLIC_INCLUDES} DESTINATION include)
    INSTALL(TARGETS portaudio DESTINATION lib)
  ENDIF()
ENDIF()

# Prepared for inclusion of test files
OPTION(PA_BUILD_TESTS "Include test projects" OFF)
IF(PA_BUILD_TESTS)
  SUBDIRS(test)
ENDIF()

# Prepared for inclusion of test files
OPTION(PA_BUILD_EXAMPLES "Include example projects" OFF)
IF(PA_BUILD_EXAMPLES)
  SUBDIRS(examples)
ENDIF()

