cmake_minimum_required(VERSION 3.15)

set(PLUTOVG_VERSION_MAJOR 1)
set(PLUTOVG_VERSION_MINOR 3)
set(PLUTOVG_VERSION_MICRO 2)

project(plutovg LANGUAGES C VERSION ${PLUTOVG_VERSION_MAJOR}.${PLUTOVG_VERSION_MINOR}.${PLUTOVG_VERSION_MICRO})

set(plutovg_sources
    source/plutovg-blend.c
    source/plutovg-canvas.c
    source/plutovg-font.c
    source/plutovg-matrix.c
    source/plutovg-paint.c
    source/plutovg-path.c
    source/plutovg-rasterize.c
    source/plutovg-surface.c
    source/plutovg-ft-math.c
    source/plutovg-ft-raster.c
    source/plutovg-ft-stroker.c
)

set(plutovg_headers
    include/plutovg.h
    source/plutovg-private.h
    source/plutovg-utils.h
    source/plutovg-ft-math.h
    source/plutovg-ft-raster.h
    source/plutovg-ft-stroker.h
    source/plutovg-ft-types.h
    source/plutovg-stb-image-write.h
    source/plutovg-stb-image.h
    source/plutovg-stb-truetype.h
)

add_library(plutovg ${plutovg_sources} ${plutovg_headers})
add_library(plutovg::plutovg ALIAS plutovg)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CheckIncludeFile)

set_target_properties(plutovg PROPERTIES
    SOVERSION ${PLUTOVG_VERSION_MAJOR}
    C_VISIBILITY_PRESET hidden
    C_STANDARD_REQUIRED ON
    C_STANDARD 11
)

target_include_directories(plutovg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_include_directories(plutovg PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/plutovg>
)

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(plutovg PRIVATE m)
endif()

check_include_file("threads.h" HAVE_THREADS_H)
if(HAVE_THREADS_H)
    target_compile_definitions(plutovg PRIVATE HAVE_THREADS_H)
endif()
find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(plutovg PRIVATE Threads::Threads)
endif()
find_library(STDTHREADS_LIBRARY stdthreads)
if(STDTHREADS_LIBRARY)
    target_link_libraries(plutovg PRIVATE stdthreads)
endif()

target_compile_definitions(plutovg PRIVATE PLUTOVG_BUILD)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(plutovg PUBLIC PLUTOVG_BUILD_STATIC)
endif()

option(PLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD "Disable loading font face cache from files and directories" OFF)
if(PLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD)
    target_compile_definitions(plutovg PRIVATE PLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD)
endif()

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/plutovgConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/plutovgConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plutovg
)

write_basic_package_version_file(plutovgConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/plutovg.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/plutovg
)

install(TARGETS plutovg
    EXPORT plutovgTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT plutovgTargets
    FILE plutovgTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plutovg
    NAMESPACE plutovg::
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/plutovgConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/plutovgConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plutovg
)

export(EXPORT plutovgTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/plutovgTargets.cmake
    NAMESPACE plutovg::
)

file(RELATIVE_PATH plutovg_pc_prefix_relative
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    "${CMAKE_INSTALL_PREFIX}"
)

set(plutovg_pc_cflags "")
set(plutovg_pc_libs_private "")

if(MATH_LIBRARY)
    string(APPEND plutovg_pc_libs_private " -lm")
endif()

if(Threads_FOUND AND CMAKE_THREAD_LIBS_INIT)
    string(APPEND plutovg_pc_libs_private " ${CMAKE_THREAD_LIBS_INIT}")
endif()

if(NOT BUILD_SHARED_LIBS)
    string(APPEND plutovg_pc_cflags " -DPLUTOVG_BUILD_STATIC")
endif()

string(CONFIGURE [[
prefix=${pcfiledir}/@plutovg_pc_prefix_relative@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@
libdir=${prefix}/@CMAKE_INSTALL_LIBDIR@

Name: PlutoVG
Description: Tiny 2D vector graphics library in C
Version: @PROJECT_VERSION@

Cflags: -I${includedir}/plutovg@plutovg_pc_cflags@
Libs: -L${libdir} -lplutovg
Libs.private:@plutovg_pc_libs_private@
]] plutovg_pc @ONLY)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/plutovg.pc" "${plutovg_pc}")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/plutovg.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

option(PLUTOVG_BUILD_EXAMPLES "Build examples" ON)
if(PLUTOVG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
