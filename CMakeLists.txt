# Project Name
project(Pcsx2)

# Debian-based distributions require at least 2.8.5 due to multiarch.
# Bumping up to 3.0 seems reasonable at this point, and will let us modernize
# things a bit.
#
# Setting it to a range tells it that it supports the features on the newer
# versions as well, avoiding setting policies.
cmake_minimum_required(VERSION 3.3.0...3.17)

# Variable to check that people use the good file
set(TOP_CMAKE_WAS_SOURCED TRUE)

# set module path
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# include some generic functions to ensure correctness of the env
include(Pcsx2Utils)

check_no_parenthesis_in_path()
detectOperatingSystem()
check_compiler_version("7.0" "7.0")

#-------------------------------------------------------------------------------
# Include specific module
# GNUInstallDirs must be done before BuildParameters
include(GNUInstallDirs)
# BuildParameters Must be done before SearchForStuff
include(BuildParameters)
include(SearchForStuff)

# Must be done after SearchForStuff
get_git_version_info()
write_svnrev_h()

if(NOT NO_TRANSLATION)
    # make the translation
    if(EXISTS "${CMAKE_SOURCE_DIR}/locales")
        add_subdirectory(locales)
    endif()
endif()

# make common
if(common_libs)
    add_subdirectory(common/src/Utilities)
    add_subdirectory(common/src/x86emitter)
endif()

# make pcsx2
if(EXISTS "${CMAKE_SOURCE_DIR}/pcsx2" AND pcsx2_core)
    add_subdirectory(pcsx2)
endif()

# make plugins
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins")
    add_subdirectory(plugins)
endif()

# tests
if(ACTUALLY_ENABLE_TESTS)
    add_subdirectory(3rdparty/gtest EXCLUDE_FROM_ALL)
    add_subdirectory(tests/ctest)
endif()

#-------------------------------------------------------------------------------

# Install some files to ease package creation
if(PACKAGE_MODE)
    if(NOT DISABLE_CHEATS_ZIP)
        INSTALL(FILES     "${CMAKE_SOURCE_DIR}/bin/cheats_ws.zip" DESTINATION "${CMAKE_INSTALL_DATADIR}/PCSX2")
    endif()
    INSTALL(FILES     "${CMAKE_SOURCE_DIR}/bin/GameIndex.yaml" DESTINATION "${CMAKE_INSTALL_DATADIR}/PCSX2")

    # set categories depending on system/distribution in pcsx2.desktop
    if(openSUSE)
        set(PCSX2_MENU_CATEGORIES "System;Emulator;")
    else()
        set(PCSX2_MENU_CATEGORIES "Game;Emulator;")
    endif()
    configure_file("${CMAKE_SOURCE_DIR}/linux_various/PCSX2.desktop.in" "${CMAKE_BINARY_DIR}/linux_various/PCSX2.desktop" @ONLY)

    INSTALL(FILES "${CMAKE_BINARY_DIR}/linux_various/PCSX2.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/linux_various/PCSX2.xpm"     DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/pixmaps")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/PCSX2_FAQ.pdf"      DESTINATION "${CMAKE_INSTALL_DOCDIR}")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/Configuration_Guide.pdf"   DESTINATION "${CMAKE_INSTALL_DOCDIR}")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/PCSX2.1"            DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
    if(NOT DISABLE_PCSX2_WRAPPER)
        INSTALL(FILES "${CMAKE_SOURCE_DIR}/linux_various/PCSX2-linux.sh"          DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()

else()
    if(NOT DISABLE_PCSX2_WRAPPER)
        # special case to avoid having linux files in windows
        INSTALL(FILES "${CMAKE_SOURCE_DIR}/linux_various/PCSX2-linux.sh"          DESTINATION "${CMAKE_SOURCE_DIR}/bin")
    endif()
endif()
