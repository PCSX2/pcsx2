cmake_minimum_required(VERSION 3.15)

set(PLUTOSVG_VERSION_MAJOR 0)
set(PLUTOSVG_VERSION_MINOR 0)
set(PLUTOSVG_VERSION_MICRO 7)

project(plutosvg LANGUAGES C VERSION ${PLUTOSVG_VERSION_MAJOR}.${PLUTOSVG_VERSION_MINOR}.${PLUTOSVG_VERSION_MICRO})

if (NOT TARGET plutovg::plutovg)
    find_package(plutovg 1.0.0 QUIET)
    if(NOT plutovg_FOUND)
        add_subdirectory(plutovg)
    endif()
endif()

set(plutosvg_sources
    source/plutosvg.h
    source/plutosvg-ft.h
    source/plutosvg.c
)

add_library(plutosvg ${plutosvg_sources})
add_library(plutosvg::plutosvg ALIAS plutosvg)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set_target_properties(plutosvg PROPERTIES
    SOVERSION ${PLUTOSVG_VERSION_MAJOR}
    C_VISIBILITY_PRESET hidden
    C_STANDARD_REQUIRED ON
    C_STANDARD 99
)

target_include_directories(plutosvg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_include_directories(plutosvg PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/plutosvg>
)

target_link_libraries(plutosvg PUBLIC plutovg::plutovg)

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(plutosvg PRIVATE m)
endif()

target_compile_definitions(plutosvg PRIVATE PLUTOSVG_BUILD)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(plutosvg PUBLIC PLUTOSVG_BUILD_STATIC)
endif()

option(PLUTOSVG_ENABLE_FREETYPE "Enable Freetype integration" OFF)
if(PLUTOSVG_ENABLE_FREETYPE)
    find_package(Freetype 2.12 REQUIRED)
    target_compile_definitions(plutosvg PUBLIC PLUTOSVG_HAS_FREETYPE)
    target_link_libraries(plutosvg PUBLIC Freetype::Freetype)
endif()

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/plutosvgConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/plutosvgConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plutosvg
)

write_basic_package_version_file(plutosvgConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plutosvg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plutosvg-ft.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/plutosvg
)

install(TARGETS plutosvg
    EXPORT plutosvgTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT plutosvgTargets
    FILE plutosvgTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plutosvg
    NAMESPACE plutosvg::
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/plutosvgConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/plutosvgConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plutosvg
)

export(EXPORT plutosvgTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/plutosvgTargets.cmake
    NAMESPACE plutosvg::
)

file(RELATIVE_PATH plutosvg_pc_prefix_relative
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    "${CMAKE_INSTALL_PREFIX}"
)

set(plutosvg_pc_cflags "")
set(plutosvg_pc_libs_private "")
set(plutosvg_pc_requires "")

if(MATH_LIBRARY)
    string(APPEND plutosvg_pc_libs_private " -lm")
endif()

if(NOT BUILD_SHARED_LIBS)
    string(APPEND plutosvg_pc_cflags " -DPLUTOSVG_BUILD_STATIC")
endif()

if(PLUTOSVG_ENABLE_FREETYPE)
    string(APPEND plutosvg_pc_cflags " -DPLUTOSVG_HAS_FREETYPE")
    string(APPEND plutosvg_pc_requires " freetype2 >= 2.12")
endif()

string(CONFIGURE [[
prefix=${pcfiledir}/@plutosvg_pc_prefix_relative@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@
libdir=${prefix}/@CMAKE_INSTALL_LIBDIR@

Name: PlutoSVG
Description: Tiny SVG rendering library in C
Version: @PROJECT_VERSION@

Requires: plutovg@plutosvg_pc_requires@
Cflags: -I${includedir}/plutosvg@plutosvg_pc_cflags@
Libs: -L${libdir} -lplutosvg
Libs.private:@plutosvg_pc_libs_private@
]] plutosvg_pc @ONLY)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/plutosvg.pc" "${plutosvg_pc}")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/plutosvg.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

option(PLUTOSVG_BUILD_EXAMPLES "Build examples" ON)
if(PLUTOSVG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
