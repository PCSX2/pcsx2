#.rst:
# FindWayland
# -----------
#
# Find Wayland installation
#
# Try to find Wayland. The following values are defined
#
# ::
#
#   WAYLAND_FOUND        - True if Wayland is found
#   WAYLAND_LIBRARIES    - Link these to use Wayland
#   WAYLAND_INCLUDE_DIRS - Include directories for Wayland
#   WAYLAND_DEFINITIONS  - Compiler flags for using Wayland
#
# and also the following more fine grained variables:
#
# ::
#
#   WAYLAND_CLIENT_FOUND,  WAYLAND_CLIENT_INCLUDE_DIRS,  WAYLAND_CLIENT_LIBRARIES
#   WAYLAND_EGL_FOUND,     WAYLAND_EGL_INCLUDE_DIRS,     WAYLAND_EGL_LIBRARIES
#
#=============================================================================
# Copyright (c) 2015 Jari Vetoniemi
#               2013 Martin Gräßlin <mgraesslin@kde.org>
#
# Distributed under the OSI-approved BSD License (the "License");
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

find_package(PkgConfig)
pkg_check_modules(PC_WAYLAND QUIET wayland-client wayland-egl)

find_library(WAYLAND_CLIENT_LIBRARIES NAMES wayland-client   HINTS ${PC_WAYLAND_LIBRARY_DIRS})
find_library(WAYLAND_EGL_LIBRARIES    NAMES wayland-egl      HINTS ${PC_WAYLAND_LIBRARY_DIRS})

find_path(WAYLAND_CLIENT_INCLUDE_DIRS  NAMES wayland-client.h HINTS ${PC_WAYLAND_INCLUDE_DIRS})
find_path(WAYLAND_EGL_INCLUDE_DIRS     NAMES wayland-egl.h    HINTS ${PC_WAYLAND_INCLUDE_DIRS})

set(WAYLAND_INCLUDE_DIRS ${WAYLAND_CLIENT_INCLUDE_DIRS} ${WAYLAND_EGL_INCLUDE_DIRS})
set(WAYLAND_LIBRARIES ${WAYLAND_CLIENT_LIBRARIES} ${WAYLAND_EGL_LIBRARIES})
set(WAYLAND_DEFINITIONS ${PC_WAYLAND_CFLAGS})

list(REMOVE_DUPLICATES WAYLAND_INCLUDE_DIRS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WAYLAND_CLIENT  DEFAULT_MSG  WAYLAND_CLIENT_LIBRARIES  WAYLAND_CLIENT_INCLUDE_DIRS)
find_package_handle_standard_args(WAYLAND_EGL     DEFAULT_MSG  WAYLAND_EGL_LIBRARIES     WAYLAND_EGL_INCLUDE_DIRS)
find_package_handle_standard_args(WAYLAND         DEFAULT_MSG  WAYLAND_LIBRARIES         WAYLAND_INCLUDE_DIRS)

mark_as_advanced(
  WAYLAND_INCLUDE_DIRS         WAYLAND_LIBRARIES
  WAYLAND_CLIENT_INCLUDE_DIRS  WAYLAND_CLIENT_LIBRARIES
  WAYLAND_EGL_INCLUDE_DIRS     WAYLAND_EGL_LIBRARIES
  WAYLAND_DEFINITIONS
  )
