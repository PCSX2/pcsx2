# Project Name
project(Pcsx2)

# Debian-based distributions require at least 2.8.5 due to multiarch.
cmake_minimum_required(VERSION 2.8.5)

# Keep OLD policy. Code was updated to support the new one but wasn't tested
#
# cmake version 2.8.9
#   CMP0018
#        Ignore CMAKE_SHARED_LIBRARY_<Lang>_FLAGS variable.
# 
#        CMake 2.8.8 and lower compiled sources in SHARED and MODULE libraries
#        using the value of the undocumented CMAKE_SHARED_LIBRARY_<Lang>_FLAGS
#        platform variable.  The variable contained platform-specific flags
#        needed to compile objects for shared libraries.  Typically it included
#        a flag such as -fPIC for position independent code but also included
#        other flags needed on certain platforms.  CMake 2.8.9 and higher
#        prefer instead to use the POSITION_INDEPENDENT_CODE target property to
#        determine what targets should be position independent, and new
#        undocumented platform variables to select flags while ignoring
#        CMAKE_SHARED_LIBRARY_<Lang>_FLAGS completely.
# 
#        The default for either approach produces identical compilation flags,
#        but if a project modifies CMAKE_SHARED_LIBRARY_<Lang>_FLAGS from its
#        original value this policy determines which approach to use.
# 
#        The OLD behavior for this policy is to ignore the
#        POSITION_INDEPENDENT_CODE property for all targets and use the
#        modified value of CMAKE_SHARED_LIBRARY_<Lang>_FLAGS for SHARED and
#        MODULE libraries.
# 
#        The NEW behavior for this policy is to ignore
#        CMAKE_SHARED_LIBRARY_<Lang>_FLAGS whether it is modified or not and
#        honor the POSITION_INDEPENDENT_CODE target property.
if(POLICY CMP0018)
    cmake_policy(SET CMP0018 OLD)
endif()
if(POLICY CMP0022)
    cmake_policy(SET CMP0022 OLD)
endif()

# Variable to check that people use the good file
set(TOP_CMAKE_WAS_SOURCED TRUE)

# set module path
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# include some generic functions to ensure correctness of the env
include(Pcsx2Utils)

check_no_parenthesis_in_path()
detectOperatingSystem()
check_compiler_version("4.7" "4.5")

#-------------------------------------------------------------------------------
# Include specific module
# BuildParameters Must be done before SearchForStuff
include(BuildParameters)
# SearchForStuff be done before SelectPcsx2Plugins
include(SearchForStuff)
include(SelectPcsx2Plugins)

# Must be done after SearchForStuff
write_svnrev_h()

# make the translation
if(EXISTS "${CMAKE_SOURCE_DIR}/locales")
    add_subdirectory(locales)
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

#-------------------------------------------------------------------------------
# Install some files to ease package creation
if(PACKAGE_MODE)
    if(NOT DISABLE_CHEATS_ZIP)
        INSTALL(FILES     "${CMAKE_SOURCE_DIR}/bin/cheats_ws.zip" DESTINATION "${GAMEINDEX_DIR}")
    endif()
    INSTALL(FILES     "${CMAKE_SOURCE_DIR}/bin/GameIndex.dbf" DESTINATION "${GAMEINDEX_DIR}")

    # set categories depending on system/distribution in pcsx2.desktop
    if(openSUSE)
        set(PCSX2_MENU_CATEGORIES "System;Emulator;")
    else()
        set(PCSX2_MENU_CATEGORIES "Game;Emulator;")
    endif()
    configure_file("${CMAKE_SOURCE_DIR}/linux_various/PCSX2.desktop.in" "${CMAKE_BINARY_DIR}/linux_various/PCSX2.desktop" @ONLY)

    INSTALL(FILES "${CMAKE_BINARY_DIR}/linux_various/PCSX2.desktop" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/linux_various/PCSX2.xpm"     DESTINATION "${CMAKE_INSTALL_PREFIX}/share/pixmaps")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/PCSX2_FAQ.pdf"      DESTINATION "${DOC_DIR}")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/PCSX2_Readme.pdf"   DESTINATION "${DOC_DIR}")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/PCSX2.1"            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man1/")
    if(NOT DISABLE_PCSX2_WRAPPER)
        INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/PCSX2-linux.sh"          DESTINATION "${BIN_DIR}")
    endif()
endif()
