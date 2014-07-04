########################################
# General setup
#
cmake_minimum_required(VERSION 2.8.5)
project(PCSX2)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(PCSX2_IS_STABLE FALSE)

option(PACKAGE_MODE "Package mode" OFF)
option(GLSL_API "Enable GLSL API" OFF)
option(EGL_API "Enable EGL API" ON)
option(GLES_API "Enable GLES API" OFF)
option(EXTRA_PLUGINS "Build additional plugins" ON)
option(REBUILD_SHADER "Rebuild shader" ON)
option(BUILD_REPLAY_LOADERS "Build replay loaders" OFF)
option(PLUGIN_DIR "Plugin directory" "${CMAKE_INSTALL_PREFIX}/lib/games/pcsx2")
option(GLSL_SHADER_DIR "GLSL Shader directory" "${CMAKE_INSTALL_PREFIX}/lib/games/pcsx2")
option(GAMEINDEX_DIR "Game index directory" "${CMAKE_INSTALL_PREFIX}/share/games/pcsx2")
option(XDG_STD "Follow XDG Standard" OFF)

# Default defs
set(TOP_CMAKE_WAS_SOURCED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF) 
set(SDL_BUILDING_LIBRARY ON)

# Mac OS X check
if(APPLE)
    message(FATAL_ERROR "Mac OS X isn't supported")
endif(APPLE)

find_package(Git)
if (GIT_FOUND)
    EXECUTE_PROCESS(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} show -s --format=%ci HEAD
        OUTPUT_VARIABLE PCSX2_WC_TIME
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    # Output: "YYYY-MM-DD HH:MM:SS +HHMM" (last part is time zone, offset from UTC)
    string(REGEX REPLACE "[%:\\-]" "" PCSX2_WC_TIME "${PCSX2_WC_TIME}")
    string(REGEX REPLACE "([0-9]+) ([0-9]+).*" "\\1\\2" PCSX2_WC_TIME "${PCSX2_WC_TIME}")
else()
    set(PCSX2_WC_TIME 0)
endif()

# 64 bit specific configuration
if(CMAKE_SIZEOF_VOID_P MATCHES "8")
    # Print a user message that 64-bit isn't supported, if package mode is enabled
    if(NOT PACKAGE_MODE)
        message(WARNING "64-bit system detected. PCSX2 does not currently support compiling in 64-bit mode. Forcing 32-bit compile mode.")
    endif(NOT PACKAGE_MODE)
    # Do not search library in /usr/lib64
    SET_PROPERTY(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS OFF)
    # Force 32-bit compile
    SET_PROPERTY(GLOBAL PROPERTY COMPILE_DEFINITIONS "-m32")
    # Force the search on 32-bit lib path.
    if(EXISTS "/usr/lib32")
        set(CMAKE_LIBRARY_ARCHITECTURE "../lib32")
    endif()

    #if(EXISTS "/usr/lib/i386-linux-gnu")
    #    set(CMAKE_LIBRARY_ARCHITECTURE "i386-linux-gnu")
    #endif()
endif(CMAKE_SIZEOF_VOID_P MATCHES "8")

include(CheckCXXCompilerFlag)
macro(check_and_add_flag var flag)
    CHECK_CXX_COMPILER_FLAG(${flag} FLAG_${var})
    if(FLAG_${var})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endif()
endmacro()

# Enabling all warnings in MSVC spams too much
if(NOT MSVC)
    add_definitions(-Wall)
    check_and_add_flag(NO_ATTRIBUTES -Wno-attributes)
    check_and_add_flag(STRICT_ALIASING -Wstrict-aliasing)
    check_and_add_flag(NO_MISSING_FIELD_INITIALIZERS -Wno-missing-field-initializers)
    check_and_add_flag(NO_UNUSED_FUNCTION -Wno-unused-function)
    check_and_add_flag(NO_INVALID_OFFSETOF -Wno-invalid-offsetof)
    check_and_add_flag(FORMAT -Wformat)
    check_and_add_flag(FORMAT_SECURITY -Wformat-security)
endif(NOT MSVC)

# Enable SSE2
add_definitions(-msse2)

# Check OS
set(Windows FALSE)
set(Linux FALSE)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(Linux TRUE)
endif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(Windows TRUE)
endif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

if(NOT DEFINED CMAKE_BUILD_PO)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_BUILD_PO TRUE)
        message(STATUS "PO files will be built")
    else(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_BUILD_PO FALSE)
        message(STATUS "PO files will not be built")
    endif(CMAKE_BUILD_TYPE STREQUAL "Release")
endif(NOT DEFINED CMAKE_BUILD_PO)

if(PACKAGE_MODE)
    # Compile all source codes with these 2 defines
    add_definitions(-DPLUGIN_DIR_COMPILATION=${PLUGIN_DIR})
    add_definitions(-DGAMEINDEX_DIR_COMPILATION=${GAMEINDEX_DIR})
endif(PACKAGE_MODE)

if(DEFINED EGL_API)
    message(STATUS "EGL is experimental and not expected to work yet")
endif(DEFINED EGL_API)

find_package(wxWidgets COMPONENTS base core adv)
find_package(ZLIB)

include(CheckLib)

check_lib(AIO aio aio.h)
check_lib(EGL egl EGL/egl.h)
check_lib(GLESV2 GLESv2 GLES2/gl2.h)
check_lib(PORTAUDIO portaudio portaudio.h pa_linux_alsa.h)
check_lib(SOUNDTOUCH SoundTouch soundtouch/SoundTouch.h)

if(Linux)
    find_package(GTK2 REQUIRED gtk)
    if(GTK2_FOUND)
        include_directories(${GTK2_INCLUDE_DIRS})
    endif()
endif()

find_package(ALSA)
if(ALSA_FOUND)
    include_directories(${ALSA_INCLUDE_DIRS})
endif()

find_package(BZip2)
if(BZIP2_FOUND)
    include_directories(${BZIP2_INCLUDE_DIR})
endif()

include(FindCg)
if(CG_FOUND)
    include_directories(${CG_INCLUDE_DIRS})
endif()

find_package(JPEG)
if(JPEG_FOUND)
    include_directories(${JPEG_INCLUDE_DIR})
endif()

include(FindGlew)
if(GLEW_FOUND)
    include_directories(${GLEW_INCLUDE_DIR})
endif()

find_package(OpenGL)
if(OPENGL_FOUND)
    include_directories(${OPENGL_INCLUDE_DIR})
endif()

find_package(SDL)
if(SDL_FOUND)
    include_directories(${SDL_INCLUDE_DIR})
endif()

find_package(Threads)
#sparsehash
include_directories(3rdparty)

if(wxWidgets_FOUND)
    if(Linux)
        if(CMAKE_SIZEOF_VOID_P MATCHES "8")
            if (EXISTS "/usr/lib32/wx")
                STRING(REGEX REPLACE "/usr/lib/wx" "/usr/lib32/wx" wxWidgets_INCLUDE_DIRS "${wxWidgets_INCLUDE_DIRS}")
                STRING(REGEX REPLACE "/usr/lib64/wx" "/usr/lib32/wx" wxWidgets_INCLUDE_DIRS "${wxWidgets_INCLUDE_DIRS}")
            endif (EXISTS "/usr/lib32/wx")
            if (EXISTS "/usr/lib/wx")
                STRING(REGEX REPLACE "/usr/lib64/wx" "/usr/lib/wx" wxWidgets_INCLUDE_DIRS "${wxWidgets_INCLUDE_DIRS}")
            endif (EXISTS "/usr/lib/wx")
            # Multiarch ubuntu/debian
            STRING(REGEX REPLACE "/usr/lib/x86_64-linux-gnu" "/usr/lib/i386-linux-gnu" wxWidgets_INCLUDE_DIRS "${wxWidgets_INCLUDE_DIRS}")
        endif()
    endif()

    include(${wxWidgets_USE_FILE})
endif()

if(ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
endif()

if(EXTRA_PLUGINS)
    if(BZIP2_FOUND AND GTK2_FOUND)
        set(CDVDiso TRUE)
    endif()
    set(CDVDlinuz TRUE)
endif()

if(OPENGL_FOUND AND X11_FOUND AND EGL_FOUND)
    set(GSdx TRUE)
endif()

if(SDL_FOUND)
    set(onepad TRUE)
endif()

if(ALSA_FOUND AND PORTAUDIO_FOUND AND SOUNDTOUCH_FOUND AND SDL_FOUND AND common_libs)
    set(spu2-x TRUE)
endif()

if(GTK2_FOUND)
    # null plugins
    set(CDVDnull TRUE)
    set(dev9null TRUE)
    set(FWnull TRUE)
    set(GSnull TRUE)
    set(PadNull TRUE)
    set(SPU2null TRUE)
    set(USBnull TRUE)
endif()

file(WRITE ${CMAKE_BINARY_DIR}/common/include/svnrev.h "#define SVN_REV ${PCSX2_WC_TIME}ll \n#define SVN_MODS 0")

# add additional project-wide include directories
include_directories(${CMAKE_SOURCE_DIR}/common/include
                    ${CMAKE_SOURCE_DIR}/common/include/Utilities
                    ${CMAKE_SOURCE_DIR}/common/include/x86emitter
                    # Files generated by Cmake
                    ${CMAKE_BINARY_DIR}/common/include
                    /usr/include/i386-linux-gnu
)

# make the translation
if(EXISTS "${CMAKE_SOURCE_DIR}/locales")
    add_subdirectory(locales)
endif(EXISTS "${CMAKE_SOURCE_DIR}/locales")

#add_pcsx2_plugin(common "${SRCS}" "${CMAKE_THREAD_LIBS_INIT}")
macro(add_pcsx2_plugin lib srcs libs)
    include_directories(.)
    add_library(${lib} MODULE ${srcs})
    target_link_libraries(${lib} ${libs})
    if(PACKAGE_MODE)
        install(TARGETS ${lib} DESTINATION ${PLUGIN_DIR})
    else(PACKAGE_MODE)
        install(TARGETS ${lib} DESTINATION ${CMAKE_SOURCE_DIR}/bin/plugins)
    endif(PACKAGE_MODE)
endmacro(add_pcsx2_plugin)
#add_pcsx2_lib(common "${SRCS}" "${CMAKE_THREAD_LIBS_INIT}")
macro(add_pcsx2_lib lib srcs libs)
    include_directories(.)
    add_library(${lib} STATIC ${srcs})
    target_link_libraries(${lib} ${libs})
endmacro(add_pcsx2_lib)
#add_pcsx2_executable(common "${SRCS}" "${CMAKE_THREAD_LIBS_INIT}")
macro(add_pcsx2_executable exe srcs libs)
    include_directories(.)
    add_executable(${exe} ${srcs})
    target_link_libraries(${exe} ${libs})
    if(PACKAGE_MODE)
        install(TARGETS ${exe} DESTINATION bin)
    else(PACKAGE_MODE)
        install(TARGETS ${exe} DESTINATION ${CMAKE_SOURCE_DIR}/bin)
    endif(PACKAGE_MODE)
endmacro(add_pcsx2_executable)

add_definitions(-D_LINUX -DNDEBUG)

#start making
add_definitions(-std=c++11)

add_subdirectory(common/src/Utilities)
add_subdirectory(common/src/x86emitter)

# make pcsx2
add_subdirectory(pcsx2)

# make plugins
add_subdirectory(plugins)

#-------------------------------------------------------------------------------
# Install some files to ease package creation
if(PACKAGE_MODE)
    INSTALL(FILES     "${CMAKE_SOURCE_DIR}/bin/cheats_ws.zip" DESTINATION "${GAMEINDEX_DIR}")
    INSTALL(FILES     "${CMAKE_SOURCE_DIR}/bin/GameIndex.dbf" DESTINATION "${GAMEINDEX_DIR}")

    INSTALL(FILES "${CMAKE_SOURCE_DIR}/linux_various/pcsx2.desktop" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/linux_various/pcsx2.xpm" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/pixmaps")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/PCSX2_FAQ.pdf" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/pcsx2")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/PCSX2_Readme.pdf" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/pcsx2")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/pcsx2.1" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man1/")
endif(PACKAGE_MODE)
