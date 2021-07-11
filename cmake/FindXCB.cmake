#.rst:
# FindXCB
# -------
#
# Find XCB libraries
#
# Tries to find xcb libraries on unix systems.
#
# - Be sure to set the COMPONENTS to the components you want to link to
# - The XCB_LIBRARIES variable is set ONLY to your COMPONENTS list
# - To use only a specific component check the XCB_LIBRARIES_${COMPONENT} variable
#
# The following values are defined
#
# ::
#
#   XCB_FOUND         - True if xcb is available
#   XCB_INCLUDE_DIRS  - Include directories for xcb
#   XCB_LIBRARIES     - List of libraries for xcb
#   XCB_DEFINITIONS   - List of definitions for xcb
#
#=============================================================================
# Copyright (c) 2015 Jari Vetoniemi
#
# Distributed under the OSI-approved BSD License (the "License");
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

include(FeatureSummary)
set_package_properties(XCB PROPERTIES
   URL "http://xcb.freedesktop.org/"
   DESCRIPTION "X protocol C-language Binding")

find_package(PkgConfig)
pkg_check_modules(PC_XCB QUIET xcb ${XCB_FIND_COMPONENTS})

find_library(XCB_LIBRARIES xcb HINTS ${PC_XCB_LIBRARY_DIRS})
find_path(XCB_INCLUDE_DIRS xcb/xcb.h PATH_SUFFIXES xcb HINTS ${PC_XCB_INCLUDE_DIRS})

foreach(COMPONENT ${XCB_FIND_COMPONENTS})
	find_library(XCB_LIBRARIES_${COMPONENT} ${COMPONENT} HINTS ${PC_XCB_LIBRARY_DIRS})
	list(APPEND XCB_LIBRARIES ${XCB_LIBRARIES_${COMPONENT}})
	mark_as_advanced(XCB_LIBRARIES_${COMPONENT})
endforeach(COMPONENT ${XCB_FIND_COMPONENTS})

set(XCB_DEFINITIONS ${PC_XCB_CFLAGS_OTHER})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XCB DEFAULT_MSG XCB_LIBRARIES XCB_INCLUDE_DIRS)
mark_as_advanced(XCB_INCLUDE_DIRS XCB_LIBRARIES XCB_DEFINITIONS)
