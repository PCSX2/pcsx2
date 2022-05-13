# Setting it to a range tells it that it supports the features on the newer
# versions as well, avoiding setting policies.
cmake_minimum_required(VERSION 3.11...3.22)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "PCSX2 does not support in-tree builds.  Please make a build directory that is not the PCSX2 source directory and generate your CMake project there using either `cmake -B build_directory` or by running cmake from the build directory.")
endif()

# Project Name
project(Pcsx2)

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

# make common
add_subdirectory(common)

# make pcsx2
add_subdirectory(pcsx2)

if (QT_BUILD)
	add_subdirectory(pcsx2-qt)

	# Updater is Windows only for now.
	if (WIN32)
		add_subdirectory(updater)
	endif()
endif()

# tests
if(ACTUALLY_ENABLE_TESTS)
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	add_subdirectory(3rdparty/gtest EXCLUDE_FROM_ALL)
	add_subdirectory(tests/ctest)
endif()

#-------------------------------------------------------------------------------

# Install some files to ease package creation
if(PACKAGE_MODE)
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
