#.rst:
# FindGTK3
# --------
#
# FindGTK3.cmake
#
# This module can find the GTK3 widget libraries and several of its
# other optional components like gtkmm, glade, and glademm.
#
# NOTE: If you intend to use version checking, CMake 2.6.2 or later is
#
# ::
#
#        required.
#
#
#
# Specify one or more of the following components as you call this find
# module.  See example below.
#
# ::
#
#    gtk
#    gtkmm
#    glade
#    glademm
#
#
#
# The following variables will be defined for your use
#
# ::
#
#    GTK3_FOUND - Were all of your specified components found?
#    GTK3_INCLUDE_DIRS - All include directories
#    GTK3_LIBRARIES - All libraries
#    GTK3_DEFINITIONS - Additional compiler flags
#
#
#
# ::
#
#    GTK3_VERSION - The version of GTK3 found (x.y.z)
#    GTK3_MAJOR_VERSION - The major version of GTK3
#    GTK3_MINOR_VERSION - The minor version of GTK3
#    GTK3_PATCH_VERSION - The patch version of GTK3
#
#
#
# Optional variables you can define prior to calling this module:
#
# ::
#
#    GTK3_DEBUG - Enables verbose debugging of the module
#    GTK3_ADDITIONAL_SUFFIXES - Allows defining additional directories to
#                               search for include files
#
#
#
# ================= Example Usage:
#
# ::
#
#    Call find_package() once, here are some examples to pick from:
#
#
#
# ::
#
#    Require GTK 3.6 or later
#        find_package(GTK3 3.6 REQUIRED gtk)
#
#
#
# ::
#
#    Require GTK 3.10 or later and Glade
#        find_package(GTK3 3.10 REQUIRED gtk glade)
#
#
#
# ::
#
#    Search for GTK/GTKMM 3.8 or later
#        find_package(GTK3 3.8 COMPONENTS gtk gtkmm)
#
#
#
# ::
#
#    if(GTK3_FOUND)
#       include_directories(${GTK3_INCLUDE_DIRS})
#       add_executable(mygui mygui.cc)
#       target_link_libraries(mygui ${GTK3_LIBRARIES})
#    endif()

#=============================================================================
# Copyright 2009 Kitware, Inc.
# Copyright 2008-2012 Philip Lowman <philip@yhbt.com>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

# Version gtk3-0.1 (12/27/2014)
#   * Initial port of FindGTK2 (CMake 3.1.0) to FindGTK3.
#       sed -i 's/GTK2/GTK3/g
#               s/gtk2/gtk3/g
#               s,${CMAKE_CURRENT_LIST_DIR}/,,g
#               s,.cmake),),g
#               s/gtk-x11/gtk/g
#               s/gdk-x11/gdk/g
#               s/gdkmm-2.4/gdkmm-3.0/g
#               s/gtk-2.0/gtk-3.0/g
#               s/gtkmm-2.4/gtkmm-3.0/g
#               s/2.4;Path/3.0;Path/g
#               s,GDKCONFIG gdkconfig.h,GDKCONFIG gdk/gdkconfig.h,g
#               s,set(_versions 2,set(_versions 3.0 3 2,g
#               s/_gtk_/_gtk3_/g' FindGTK2.cmake
#   * Detect cairo-gobject library.
#   * Update the dependencies of gdk, gtk, pangomm, gdkmm, gtkmm, glade, and
#     glademm based on their .pc files. This ammounts to dropping  pangoft2
#     and gthread from the dependencies (they are still detected) while adding
#     cairo-gobject. All of the optional dependencies are required in GTK3.
# Version 1.6 (CMake 3.0)
#   * Create targets for each library
#   * Do not link libfreetype
# Version 1.5 (CMake 2.8.12)
#   * 14236: Detect gthread library
#            Detect pangocairo on windows
#            Detect pangocairo with gtk module instead of with gtkmm
#   * 14259: Use vc100 libraries with MSVC11
#   * 14260: Export a GTK2_DEFINITIONS variable to set /vd2 when appropriate
#            (i.e. MSVC)
#   * Use the optimized/debug syntax for _LIBRARY and _LIBRARIES variables when
#     appropriate. A new set of _RELEASE variables was also added.
#   * Remove GTK2_SKIP_MARK_AS_ADVANCED option, as now the variables are
#     marked as advanced by SelectLibraryConfigurations
#   * Detect gmodule, pangoft2 and pangoxft libraries
# Version 1.4 (10/4/2012) (CMake 2.8.10)
#   * 12596: Missing paths for FindGTK2 on NetBSD
#   * 12049: Fixed detection of GTK include files in the lib folder on
#            multiarch systems.
# Version 1.3 (11/9/2010) (CMake 2.8.4)
#   * 11429: Add support for detecting GTK2 built with Visual Studio 10.
#            Thanks to Vincent Levesque for the patch.
# Version 1.2 (8/30/2010) (CMake 2.8.3)
#   * Merge patch for detecting gdk-pixbuf library (split off
#     from core GTK in 2.21).  Thanks to Vincent Untz for the patch
#     and Ricardo Cruz for the heads up.
# Version 1.1 (8/19/2010) (CMake 2.8.3)
#   * Add support for detecting GTK2 under macports (thanks to Gary Kramlich)
# Version 1.0 (8/12/2010) (CMake 2.8.3)
#   * Add support for detecting new pangommconfig.h header file
#     (Thanks to Sune Vuorela & the Debian Project for the patch)
#   * Add support for detecting fontconfig.h header
#   * Call find_package(Freetype) since it's required
#   * Add support for allowing users to add additional library directories
#     via the GTK2_ADDITIONAL_SUFFIXES variable (kind of a future-kludge in
#     case the GTK developers change versions on any of the directories in the
#     future).
# Version 0.8 (1/4/2010)
#   * Get module working under MacOSX fink by adding /sw/include, /sw/lib
#     to PATHS and the gobject library
# Version 0.7 (3/22/09)
#   * Checked into CMake CVS
#   * Added versioning support
#   * Module now defaults to searching for GTK if COMPONENTS not specified.
#   * Added HKCU prior to HKLM registry key and GTKMM specific environment
#      variable as per mailing list discussion.
#   * Added lib64 to include search path and a few other search paths where GTK
#      may be installed on Unix systems.
#   * Switched to lowercase CMake commands
#   * Prefaced internal variables with _GTK2 to prevent collision
#   * Changed internal macros to functions
#   * Enhanced documentation
# Version 0.6 (1/8/08)
#   Added GTK2_SKIP_MARK_AS_ADVANCED option
# Version 0.5 (12/19/08)
#   Second release to cmake mailing list

#=============================================================
# _GTK3_GET_VERSION
# Internal function to parse the version number in gtkversion.h
#   _OUT_major = Major version number
#   _OUT_minor = Minor version number
#   _OUT_micro = Micro version number
#   _gtkversion_hdr = Header file to parse
#=============================================================

include(SelectLibraryConfigurations)
include(CMakeParseArguments)

function(_GTK3_GET_VERSION _OUT_major _OUT_minor _OUT_micro _gtkversion_hdr)
	file(STRINGS ${_gtkversion_hdr} _contents REGEX "#define GTK_M[A-Z]+_VERSION[ \t]+")
	if(_contents)
		string(REGEX REPLACE ".*#define GTK_MAJOR_VERSION[ \t]+\\(([0-9]+)\\).*" "\\1" ${_OUT_major} "${_contents}")
		string(REGEX REPLACE ".*#define GTK_MINOR_VERSION[ \t]+\\(([0-9]+)\\).*" "\\1" ${_OUT_minor} "${_contents}")
		string(REGEX REPLACE ".*#define GTK_MICRO_VERSION[ \t]+\\(([0-9]+)\\).*" "\\1" ${_OUT_micro} "${_contents}")

		if(NOT ${_OUT_major} MATCHES "[0-9]+")
			message(FATAL_ERROR "Version parsing failed for GTK3_MAJOR_VERSION!")
		endif()
		if(NOT ${_OUT_minor} MATCHES "[0-9]+")
			message(FATAL_ERROR "Version parsing failed for GTK3_MINOR_VERSION!")
		endif()
		if(NOT ${_OUT_micro} MATCHES "[0-9]+")
			message(FATAL_ERROR "Version parsing failed for GTK3_MICRO_VERSION!")
		endif()

		set(${_OUT_major} ${${_OUT_major}} PARENT_SCOPE)
		set(${_OUT_minor} ${${_OUT_minor}} PARENT_SCOPE)
		set(${_OUT_micro} ${${_OUT_micro}} PARENT_SCOPE)
	else()
		message(FATAL_ERROR "Include file ${_gtkversion_hdr} does not exist")
	endif()
endfunction()

#=============================================================
# _GTK3_FIND_INCLUDE_DIR
# Internal function to find the GTK include directories
#   _var = variable to set (_INCLUDE_DIR is appended)
#   _hdr = header file to look for
#=============================================================
function(_GTK3_FIND_INCLUDE_DIR _var _hdr)

	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}] "
		               "_GTK3_FIND_INCLUDE_DIR( ${_var} ${_hdr} )")
	endif()

	set(_gtk3_packages
		# If these ever change, things will break.
		${GTK3_ADDITIONAL_SUFFIXES}
		glibmm-2.4
		glib-2.0
		atk-1.0
		atkmm-1.6
		cairo
		cairomm-1.0
		gdk-pixbuf-2.0
		gdkmm-3.0
		giomm-2.4
		gtk-3.0
		gtkmm-3.0
		libglade-2.0
		libglademm-2.4
		pango-1.0
		pangomm-1.4
		sigc++-2.0
	)

	#
	# NOTE: The following suffixes cause searching for header files in both of
	# these directories:
	#         /usr/include/<pkg>
	#         /usr/lib/<pkg>/include
	#

	set(_suffixes)
	foreach(_d ${_gtk3_packages})
		list(APPEND _suffixes ${_d})
		list(APPEND _suffixes ${_d}/include) # for /usr/lib/gtk-3.0/include
	endforeach()

	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}]     "
		               "include suffixes = ${_suffixes}")
	endif()

	if(CMAKE_LIBRARY_ARCHITECTURE)
		set(_gtk3_arch_dir /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE})
		if(GTK3_DEBUG)
			message(STATUS "Adding ${_gtk3_arch_dir} to search path for multiarch support")
		endif()
	endif()
	find_path(GTK3_${_var}_INCLUDE_DIR ${_hdr}
		PATHS
			${_gtk3_arch_dir}
			/usr/local/lib64
			/usr/local/lib
			/usr/lib64
			/usr/lib
			/usr/X11R6/include
			/usr/X11R6/lib
			/opt/gnome/include
			/opt/gnome/lib
			/opt/openwin/include
			/usr/openwin/lib
			/sw/include
			/sw/lib
			/opt/local/include
			/opt/local/lib
			/usr/pkg/lib
			/usr/pkg/include/glib
			$ENV{GTKMM_BASEPATH}/include
			$ENV{GTKMM_BASEPATH}/lib
			[HKEY_CURRENT_USER\\SOFTWARE\\gtkmm\\3.0;Path]/include
			[HKEY_CURRENT_USER\\SOFTWARE\\gtkmm\\3.0;Path]/lib
			[HKEY_LOCAL_MACHINE\\SOFTWARE\\gtkmm\\3.0;Path]/include
			[HKEY_LOCAL_MACHINE\\SOFTWARE\\gtkmm\\3.0;Path]/lib
		PATH_SUFFIXES
			${_suffixes}
	)
	mark_as_advanced(GTK3_${_var}_INCLUDE_DIR)

	if(GTK3_${_var}_INCLUDE_DIR)
		set(GTK3_INCLUDE_DIRS ${GTK3_INCLUDE_DIRS} ${GTK3_${_var}_INCLUDE_DIR} PARENT_SCOPE)
	endif()

endfunction()

#=============================================================
# _GTK3_FIND_LIBRARY
# Internal function to find libraries packaged with GTK3
#   _var = library variable to create (_LIBRARY is appended)
#=============================================================
function(_GTK3_FIND_LIBRARY _var _lib _expand_vc _append_version)

	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}] "
		               "_GTK3_FIND_LIBRARY( ${_var} ${_lib} ${_expand_vc} ${_append_version} )")
	endif()

	# Not GTK versions per se but the versions encoded into Windows
	# import libraries (GtkMM 2.14.1 has a gtkmm-vc80-2_4.lib for example)
	# Also the MSVC libraries use _ for . (this is handled below)
	set(_versions 3.0 3 2.20 2.18 2.16 2.14 2.12
	              2.10  2.8  2.6  2.4  2.2 2.0
	              1.20 1.18 1.16 1.14 1.12
	              1.10  1.8  1.6  1.4  1.2 1.0)

	set(_library)
	set(_library_d)

	set(_library ${_lib})

	if(_expand_vc AND MSVC)
		# Add vc80/vc90/vc100 midfixes
		if(MSVC80)
			set(_library   ${_library}-vc80)
		elseif(MSVC90)
			set(_library   ${_library}-vc90)
		elseif(MSVC10)
			set(_library ${_library}-vc100)
		elseif(MSVC11)
			# Up to gtkmm-win 2.22.0-2 there are no vc110 libraries but vc100 can be used
			set(_library ${_library}-vc100)
		endif()
		set(_library_d ${_library}-d)
	endif()

	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}]     "
		               "After midfix addition = ${_library} and ${_library_d}")
	endif()

	set(_lib_list)
	set(_libd_list)
	if(_append_version)
		foreach(_ver ${_versions})
			list(APPEND _lib_list  "${_library}-${_ver}")
			list(APPEND _libd_list "${_library_d}-${_ver}")
		endforeach()
	else()
		set(_lib_list ${_library})
		set(_libd_list ${_library_d})
	endif()

	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}]     "
		               "library list = ${_lib_list} and library debug list = ${_libd_list}")
	endif()

	# For some silly reason the MSVC libraries use _ instead of .
	# in the version fields
	if(_expand_vc AND MSVC)
		set(_no_dots_lib_list)
		set(_no_dots_libd_list)
		foreach(_l ${_lib_list})
			string(REPLACE "." "_" _no_dots_library ${_l})
			list(APPEND _no_dots_lib_list ${_no_dots_library})
		endforeach()
		# And for debug
		set(_no_dots_libsd_list)
		foreach(_l ${_libd_list})
			string(REPLACE "." "_" _no_dots_libraryd ${_l})
			list(APPEND _no_dots_libd_list ${_no_dots_libraryd})
		endforeach()

		# Copy list back to original names
		set(_lib_list ${_no_dots_lib_list})
		set(_libd_list ${_no_dots_libd_list})
	endif()

	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}]     "
		               "While searching for GTK3_${_var}_LIBRARY, our proposed library list is ${_lib_list}")
	endif()

	find_library(GTK3_${_var}_LIBRARY_RELEASE
		NAMES ${_lib_list}
		PATHS
			/opt/gnome/lib
			/usr/openwin/lib
			/sw/lib
			$ENV{GTKMM_BASEPATH}/lib
			[HKEY_CURRENT_USER\\SOFTWARE\\gtkmm\\3.0;Path]/lib
			[HKEY_LOCAL_MACHINE\\SOFTWARE\\gtkmm\\3.0;Path]/lib
		)

	if(_expand_vc AND MSVC)
		if(GTK3_DEBUG)
			message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}]     "
			               "While searching for GTK3_${_var}_LIBRARY_DEBUG our proposed library list is ${_libd_list}")
		endif()

		find_library(GTK3_${_var}_LIBRARY_DEBUG
			NAMES ${_libd_list}
			PATHS
			$ENV{GTKMM_BASEPATH}/lib
			[HKEY_CURRENT_USER\\SOFTWARE\\gtkmm\\3.0;Path]/lib
			[HKEY_LOCAL_MACHINE\\SOFTWARE\\gtkmm\\3.0;Path]/lib
		)
	endif()

	select_library_configurations(GTK3_${_var})

	set(GTK3_${_var}_LIBRARY ${GTK3_${_var}_LIBRARY} PARENT_SCOPE)
	set(GTK3_${_var}_FOUND ${GTK3_${_var}_FOUND} PARENT_SCOPE)

	if(GTK3_${_var}_FOUND)
		set(GTK3_LIBRARIES ${GTK3_LIBRARIES} ${GTK3_${_var}_LIBRARY})
		set(GTK3_LIBRARIES ${GTK3_LIBRARIES} PARENT_SCOPE)
	endif()

	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}]     "
		               "GTK3_${_var}_LIBRARY_RELEASE = \"${GTK3_${_var}_LIBRARY_RELEASE}\"")
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}]     "
		               "GTK3_${_var}_LIBRARY_DEBUG   = \"${GTK3_${_var}_LIBRARY_DEBUG}\"")
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}]     "
		               "GTK3_${_var}_LIBRARY         = \"${GTK3_${_var}_LIBRARY}\"")
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}]     "
		               "GTK3_${_var}_FOUND           = \"${GTK3_${_var}_FOUND}\"")
	endif()

endfunction()


function(_GTK3_ADD_TARGET_DEPENDS_INTERNAL _var _property)
	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}] "
		               "_GTK3_ADD_TARGET_DEPENDS_INTERNAL( ${_var} ${_property} )")
	endif()

	string(TOLOWER "${_var}" _basename)

	if (TARGET GTK3::${_basename})
		foreach(_depend ${ARGN})
			set(_valid_depends)
			if (TARGET GTK3::${_depend})
				list(APPEND _valid_depends GTK3::${_depend})
			endif()
			if (_valid_depends)
				set_property(TARGET GTK3::${_basename} APPEND PROPERTY ${_property} "${_valid_depends}")
			endif()
			set(_valid_depends)
		endforeach()
	endif()
endfunction()

function(_GTK3_ADD_TARGET_DEPENDS _var)
	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}] "
		               "_GTK3_ADD_TARGET_DEPENDS( ${_var} )")
	endif()

	string(TOLOWER "${_var}" _basename)

	if(TARGET GTK3::${_basename})
		get_target_property(_configs GTK3::${_basename} IMPORTED_CONFIGURATIONS)
		_GTK3_ADD_TARGET_DEPENDS_INTERNAL(${_var} INTERFACE_LINK_LIBRARIES ${ARGN})
		foreach(_config ${_configs})
			_GTK3_ADD_TARGET_DEPENDS_INTERNAL(${_var} IMPORTED_LINK_INTERFACE_LIBRARIES_${_config} ${ARGN})
		endforeach()
	endif()
endfunction()

function(_GTK3_ADD_TARGET_INCLUDE_DIRS _var)
	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}] "
		               "_GTK3_ADD_TARGET_INCLUDE_DIRS( ${_var} )")
	endif()

	string(TOLOWER "${_var}" _basename)

	if(TARGET GTK3::${_basename})
		foreach(_include ${ARGN})
			set_property(TARGET GTK3::${_basename} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${_include}")
		endforeach()
	endif()
endfunction()

#=============================================================
# _GTK3_ADD_TARGET
# Internal function to create targets for GTK3
#   _var = target to create
#=============================================================
function(_GTK3_ADD_TARGET _var)
	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}] "
		               "_GTK3_ADD_TARGET( ${_var} )")
	endif()

	string(TOLOWER "${_var}" _basename)

	cmake_parse_arguments(_${_var} "" "" "GTK3_DEPENDS;GTK3_OPTIONAL_DEPENDS;OPTIONAL_INCLUDES" ${ARGN})

	if(GTK3_${_var}_FOUND AND NOT TARGET GTK3::${_basename})
		# Do not create the target if dependencies are missing
		foreach(_dep ${_${_var}_GTK3_DEPENDS})
			if(NOT TARGET GTK3::${_dep})
				message("Not creating GTK3::${_basename}, missing GTK3::${_dep}")
				return()
			endif()
		endforeach()

		add_library(GTK3::${_basename} UNKNOWN IMPORTED)

		if(GTK3_${_var}_LIBRARY_RELEASE)
			set_property(TARGET GTK3::${_basename} APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
			set_property(TARGET GTK3::${_basename}        PROPERTY IMPORTED_LOCATION_RELEASE "${GTK3_${_var}_LIBRARY_RELEASE}" )
		endif()

		if(GTK3_${_var}_LIBRARY_DEBUG)
			set_property(TARGET GTK3::${_basename} APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
			set_property(TARGET GTK3::${_basename}        PROPERTY IMPORTED_LOCATION_DEBUG "${GTK3_${_var}_LIBRARY_DEBUG}" )
		endif()

		if(GTK3_${_var}_INCLUDE_DIR)
			set_property(TARGET GTK3::${_basename} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${GTK3_${_var}_INCLUDE_DIR}")
		endif()

		if(GTK3_${_var}CONFIG_INCLUDE_DIR AND NOT "x${GTK3_${_var}CONFIG_INCLUDE_DIR}" STREQUAL "x${GTK3_${_var}_INCLUDE_DIR}")
			set_property(TARGET GTK3::${_basename} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${GTK3_${_var}CONFIG_INCLUDE_DIR}")
		endif()

		if(GTK3_DEFINITIONS)
			set_property(TARGET GTK3::${_basename} PROPERTY INTERFACE_COMPILE_DEFINITIONS "${GTK3_DEFINITIONS}")
		endif()

		if(_${_var}_GTK3_DEPENDS)
			_GTK3_ADD_TARGET_DEPENDS(${_var} ${_${_var}_GTK3_DEPENDS} ${_${_var}_GTK3_OPTIONAL_DEPENDS})
		endif()

		if(_${_var}_OPTIONAL_INCLUDES)
			foreach(_D ${_${_var}_OPTIONAL_INCLUDES})
				if(_D)
					_GTK3_ADD_TARGET_INCLUDE_DIRS(${_var} ${_D})
				endif()
			endforeach()
		endif()

		if(GTK3_USE_IMPORTED_TARGETS)
			set(GTK3_${_var}_LIBRARY GTK3::${_basename} PARENT_SCOPE)
		endif()

	endif()
endfunction()



#=============================================================

#
# main()
#

set(GTK3_FOUND)
set(GTK3_INCLUDE_DIRS)
set(GTK3_LIBRARIES)
set(GTK3_DEFINITIONS)

if(NOT GTK3_FIND_COMPONENTS)
	# Assume they only want GTK
	set(GTK3_FIND_COMPONENTS gtk)
endif()

#
# If specified, enforce version number
#
if(GTK3_FIND_VERSION)
	set(GTK3_FAILED_VERSION_CHECK true)
	if(GTK3_DEBUG)
		message(STATUS "[FindGTK3.cmake:${CMAKE_CURRENT_LIST_LINE}] "
		               "Searching for version ${GTK3_FIND_VERSION}")
	endif()
	_GTK3_FIND_INCLUDE_DIR(GTK gtk/gtk.h)
	if(GTK3_GTK_INCLUDE_DIR)
		_GTK3_GET_VERSION(GTK3_MAJOR_VERSION
		                  GTK3_MINOR_VERSION
		                  GTK3_PATCH_VERSION
		                  ${GTK3_GTK_INCLUDE_DIR}/gtk/gtkversion.h)
		set(GTK3_VERSION
			${GTK3_MAJOR_VERSION}.${GTK3_MINOR_VERSION}.${GTK3_PATCH_VERSION})
		if(GTK3_FIND_VERSION_EXACT)
			if(GTK3_VERSION VERSION_EQUAL GTK3_FIND_VERSION)
				set(GTK3_FAILED_VERSION_CHECK false)
			endif()
		else()
			if(GTK3_VERSION VERSION_EQUAL   GTK3_FIND_VERSION OR
				GTK3_VERSION VERSION_GREATER GTK3_FIND_VERSION)
				set(GTK3_FAILED_VERSION_CHECK false)
			endif()
		endif()
	else()
		# If we can't find the GTK include dir, we can't do version checking
		if(GTK3_FIND_REQUIRED AND NOT GTK3_FIND_QUIETLY)
			message(FATAL_ERROR "Could not find GTK3 include directory")
		endif()
		return()
	endif()

	if(GTK3_FAILED_VERSION_CHECK)
		if(GTK3_FIND_REQUIRED AND NOT GTK3_FIND_QUIETLY)
			if(GTK3_FIND_VERSION_EXACT)
				message(FATAL_ERROR "GTK3 version check failed.  Version ${GTK3_VERSION} was found, version ${GTK3_FIND_VERSION} is needed exactly.")
			else()
				message(FATAL_ERROR "GTK3 version check failed.  Version ${GTK3_VERSION} was found, at least version ${GTK3_FIND_VERSION} is required")
			endif()
		endif()

		# If the version check fails, exit out of the module here
		return()
	endif()
endif()

#
# On MSVC, according to https://wiki.gnome.org/gtkmm/MSWindows, the /vd2 flag needs to be
# passed to the compiler in order to use gtkmm
#
if(MSVC)
	foreach(_GTK3_component ${GTK3_FIND_COMPONENTS})
		if(_GTK3_component STREQUAL "gtkmm")
			set(GTK3_DEFINITIONS "/vd2")
		elseif(_GTK3_component STREQUAL "glademm")
			set(GTK3_DEFINITIONS "/vd2")
		endif()
	endforeach()
endif()

#
# Find all components
#

find_package(Freetype QUIET)
if(FREETYPE_INCLUDE_DIR_ft2build AND FREETYPE_INCLUDE_DIR_freetype2)
	list(APPEND GTK3_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2})
endif()

foreach(_GTK3_component ${GTK3_FIND_COMPONENTS})
	if(_GTK3_component STREQUAL "gtk")
		# Left for compatibility with previous versions.
		_GTK3_FIND_INCLUDE_DIR(FONTCONFIG fontconfig/fontconfig.h)
		_GTK3_FIND_INCLUDE_DIR(X11 X11/Xlib.h)

		_GTK3_FIND_INCLUDE_DIR(GLIB glib.h)
		_GTK3_FIND_INCLUDE_DIR(GLIBCONFIG glibconfig.h)
		_GTK3_FIND_LIBRARY    (GLIB glib false true)
		_GTK3_ADD_TARGET      (GLIB)

		_GTK3_FIND_INCLUDE_DIR(GOBJECT glib-object.h)
		_GTK3_FIND_LIBRARY    (GOBJECT gobject false true)
		_GTK3_ADD_TARGET      (GOBJECT GTK3_DEPENDS glib)

		_GTK3_FIND_INCLUDE_DIR(ATK atk/atk.h)
		_GTK3_FIND_LIBRARY    (ATK atk false true)
		_GTK3_ADD_TARGET      (ATK GTK3_DEPENDS gobject glib)

		_GTK3_FIND_LIBRARY    (GIO gio false true)
		_GTK3_ADD_TARGET      (GIO GTK3_DEPENDS gobject glib)

		_GTK3_FIND_LIBRARY    (GTHREAD gthread false true)
		_GTK3_ADD_TARGET      (GTHREAD GTK3_DEPENDS glib)

		_GTK3_FIND_LIBRARY    (GMODULE gmodule false true)
		_GTK3_ADD_TARGET      (GMODULE GTK3_DEPENDS glib)

		_GTK3_FIND_INCLUDE_DIR(GDK_PIXBUF gdk-pixbuf/gdk-pixbuf.h)
		_GTK3_FIND_LIBRARY    (GDK_PIXBUF gdk_pixbuf false true)
		_GTK3_ADD_TARGET      (GDK_PIXBUF GTK3_DEPENDS gobject glib)

		_GTK3_FIND_INCLUDE_DIR(CAIRO cairo.h)
		_GTK3_FIND_LIBRARY    (CAIRO cairo false false)
		_GTK3_ADD_TARGET      (CAIRO)

		_GTK3_FIND_INCLUDE_DIR(CAIRO_GOBJECT cairo-gobject.h)
		_GTK3_FIND_LIBRARY    (CAIRO_GOBJECT cairo-gobject false false)
		_GTK3_ADD_TARGET      (CAIRO_GOBJECT GTK3_DEPENDS cairo gobject glib)

		_GTK3_FIND_INCLUDE_DIR(PANGO pango/pango.h)
		_GTK3_FIND_LIBRARY    (PANGO pango false true)
		_GTK3_ADD_TARGET      (PANGO GTK3_DEPENDS gobject glib)

		_GTK3_FIND_LIBRARY    (PANGOCAIRO pangocairo false true)
		_GTK3_ADD_TARGET      (PANGOCAIRO GTK3_DEPENDS pango cairo gobject glib)

		_GTK3_FIND_LIBRARY    (PANGOFT2 pangoft2 false true)
		_GTK3_ADD_TARGET      (PANGOFT2 GTK3_DEPENDS pango gobject glib
		                                OPTIONAL_INCLUDES ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2}
		                                                  ${GTK3_FONTCONFIG_INCLUDE_DIR}
		                                                  ${GTK3_X11_INCLUDE_DIR})

		_GTK3_FIND_LIBRARY    (PANGOXFT pangoxft false true)
		_GTK3_ADD_TARGET      (PANGOXFT GTK3_DEPENDS pangoft2 pango gobject glib
		                                OPTIONAL_INCLUDES ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2}
		                                                  ${GTK3_FONTCONFIG_INCLUDE_DIR}
		                                                  ${GTK3_X11_INCLUDE_DIR})

		_GTK3_FIND_INCLUDE_DIR(GDK gdk/gdk.h)
		_GTK3_FIND_INCLUDE_DIR(GDKCONFIG gdk/gdkconfig.h)
		if(UNIX)
			if(APPLE)
				_GTK3_FIND_LIBRARY    (GDK gdk-quartz false true)
			endif()
			if(NOT GTK3_GDK_FOUND)
				_GTK3_FIND_LIBRARY    (GDK gdk false true)
			endif()
		else()
			_GTK3_FIND_LIBRARY    (GDK gdk-win32 false true)
		endif()
		_GTK3_ADD_TARGET (GDK GTK3_DEPENDS pangocairo pango cairo_gobject cairo gdk_pixbuf gobject glib)

		_GTK3_FIND_INCLUDE_DIR(GTK gtk/gtk.h)
		if(UNIX)
			if(APPLE)
				_GTK3_FIND_LIBRARY    (GTK gtk-quartz false true)
			endif()
			if(NOT GTK3_GTK_FOUND)
				_GTK3_FIND_LIBRARY    (GTK gtk false true)
			endif()
		else()
			_GTK3_FIND_LIBRARY    (GTK gtk-win32 false true)
		endif()
		_GTK3_ADD_TARGET (GTK GTK3_DEPENDS gdk atk pangocairo pango cairo_gobject cairo gdk_pixbuf gio gobject glib)

	elseif(_GTK3_component STREQUAL "gtkmm")

		_GTK3_FIND_INCLUDE_DIR(SIGC++ sigc++/sigc++.h)
		_GTK3_FIND_INCLUDE_DIR(SIGC++CONFIG sigc++config.h)
		_GTK3_FIND_LIBRARY    (SIGC++ sigc true true)
		_GTK3_ADD_TARGET      (SIGC++)

		_GTK3_FIND_INCLUDE_DIR(GLIBMM glibmm.h)
		_GTK3_FIND_INCLUDE_DIR(GLIBMMCONFIG glibmmconfig.h)
		_GTK3_FIND_LIBRARY    (GLIBMM glibmm true true)
		_GTK3_ADD_TARGET      (GLIBMM GTK3_DEPENDS gobject sigc++ glib)

		_GTK3_FIND_INCLUDE_DIR(GIOMM giomm.h)
		_GTK3_FIND_INCLUDE_DIR(GIOMMCONFIG giommconfig.h)
		_GTK3_FIND_LIBRARY    (GIOMM giomm true true)
		_GTK3_ADD_TARGET      (GIOMM GTK3_DEPENDS gio glibmm gobject sigc++ glib)

		_GTK3_FIND_INCLUDE_DIR(ATKMM atkmm.h)
		_GTK3_FIND_LIBRARY    (ATKMM atkmm true true)
		_GTK3_ADD_TARGET      (ATKMM GTK3_DEPENDS atk glibmm gobject sigc++ glib)

		_GTK3_FIND_INCLUDE_DIR(CAIROMM cairomm/cairomm.h)
		_GTK3_FIND_INCLUDE_DIR(CAIROMMCONFIG cairommconfig.h)
		_GTK3_FIND_LIBRARY    (CAIROMM cairomm true true)
		_GTK3_ADD_TARGET      (CAIROMM GTK3_DEPENDS cairo sigc++
		                               OPTIONAL_INCLUDES ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2}
		                                                 ${GTK3_FONTCONFIG_INCLUDE_DIR}
		                                                 ${GTK3_X11_INCLUDE_DIR})

		_GTK3_FIND_INCLUDE_DIR(PANGOMM pangomm.h)
		_GTK3_FIND_INCLUDE_DIR(PANGOMMCONFIG pangommconfig.h)
		_GTK3_FIND_LIBRARY    (PANGOMM pangomm true true)
		_GTK3_ADD_TARGET      (PANGOMM GTK3_DEPENDS glibmm cairomm sigc++ pangocairo pango cairo gobject glib
		                               OPTIONAL_INCLUDES ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2}
		                                                 ${GTK3_FONTCONFIG_INCLUDE_DIR}
		                                                 ${GTK3_X11_INCLUDE_DIR})

		_GTK3_FIND_INCLUDE_DIR(GDKMM gdkmm.h)
		_GTK3_FIND_INCLUDE_DIR(GDKMMCONFIG gdkmmconfig.h)
		_GTK3_FIND_LIBRARY    (GDKMM gdkmm true true)
		_GTK3_ADD_TARGET      (GDKMM GTK3_DEPENDS pangomm giomm glibmm cairomm sigc++ gtk gdk atk pangocairo pango cairo-gobject cairo gdk_pixbuf gio gobject glib
		                             OPTIONAL_INCLUDES ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2}
		                                               ${GTK3_FONTCONFIG_INCLUDE_DIR}
		                                               ${GTK3_X11_INCLUDE_DIR})

		_GTK3_FIND_INCLUDE_DIR(GTKMM gtkmm.h)
		_GTK3_FIND_INCLUDE_DIR(GTKMMCONFIG gtkmmconfig.h)
		_GTK3_FIND_LIBRARY    (GTKMM gtkmm true true)
		_GTK3_ADD_TARGET      (GTKMM GTK3_DEPENDS atkmm gdkmm pangomm giomm glibmm cairomm sigc++ gtk gdk atk pangocairo pango cairo-gobject cairo gdk_pixbuf gio gobject glib
		                             OPTIONAL_INCLUDES ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2}
		                                               ${GTK3_FONTCONFIG_INCLUDE_DIR}
		                                               ${GTK3_X11_INCLUDE_DIR})

	elseif(_GTK3_component STREQUAL "glade")

		_GTK3_FIND_INCLUDE_DIR(GLADE glade/glade.h)
		_GTK3_FIND_LIBRARY    (GLADE glade false true)
		_GTK3_ADD_TARGET      (GLADE GTK3_DEPENDS gtk gdk atk pangocairo pango cairo-gobject cairo gdk_pixbuf gio gobject glib
		                             OPTIONAL_INCLUDES ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2}
		                                               ${GTK3_FONTCONFIG_INCLUDE_DIR}
		                                               ${GTK3_X11_INCLUDE_DIR})

	elseif(_GTK3_component STREQUAL "glademm")

		_GTK3_FIND_INCLUDE_DIR(GLADEMM libglademm.h)
		_GTK3_FIND_INCLUDE_DIR(GLADEMMCONFIG libglademmconfig.h)
		_GTK3_FIND_LIBRARY    (GLADEMM glademm true true)
		_GTK3_ADD_TARGET      (GLADEMM GTK3_DEPENDS gtkmm glade atkmm gdkmm pangomm giomm glibmm cairomm sigc++ gtk gdk atk pangocairo pango cairo-gobject cairo gdk_pixbuf gio gobject glib
		                               OPTIONAL_INCLUDES ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2}
		                                                 ${GTK3_FONTCONFIG_INCLUDE_DIR}
		                                                 ${GTK3_X11_INCLUDE_DIR})

	else()
		message(FATAL_ERROR "Unknown GTK3 component ${_component}")
	endif()
endforeach()

#
# Solve for the GTK3 version if we haven't already
#
if(NOT GTK3_FIND_VERSION AND GTK3_GTK_INCLUDE_DIR)
	_GTK3_GET_VERSION(GTK3_MAJOR_VERSION
	                  GTK3_MINOR_VERSION
	                  GTK3_PATCH_VERSION
	                  ${GTK3_GTK_INCLUDE_DIR}/gtk/gtkversion.h)
	set(GTK3_VERSION ${GTK3_MAJOR_VERSION}.${GTK3_MINOR_VERSION}.${GTK3_PATCH_VERSION})
endif()

#
# Try to enforce components
#

set(_GTK3_did_we_find_everything true)  # This gets set to GTK3_FOUND

include(FindPackageHandleStandardArgs)

foreach(_GTK3_component ${GTK3_FIND_COMPONENTS})
	string(TOUPPER ${_GTK3_component} _COMPONENT_UPPER)

	set(GTK3_${_COMPONENT_UPPER}_FIND_QUIETLY ${GTK3_FIND_QUIETLY})

	if(_GTK3_component STREQUAL "gtk")
		FIND_PACKAGE_HANDLE_STANDARD_ARGS(GTK3_${_COMPONENT_UPPER} "Some or all of the gtk libraries were not found."
			GTK3_GTK_LIBRARY
			GTK3_GTK_INCLUDE_DIR

			GTK3_GDK_INCLUDE_DIR
			GTK3_GDKCONFIG_INCLUDE_DIR
			GTK3_GDK_LIBRARY

			GTK3_GLIB_INCLUDE_DIR
			GTK3_GLIBCONFIG_INCLUDE_DIR
			GTK3_GLIB_LIBRARY
		)
	elseif(_GTK3_component STREQUAL "gtkmm")
		FIND_PACKAGE_HANDLE_STANDARD_ARGS(GTK3_${_COMPONENT_UPPER} "Some or all of the gtkmm libraries were not found."
			GTK3_GTKMM_LIBRARY
			GTK3_GTKMM_INCLUDE_DIR
			GTK3_GTKMMCONFIG_INCLUDE_DIR

			GTK3_GDKMM_INCLUDE_DIR
			GTK3_GDKMMCONFIG_INCLUDE_DIR
			GTK3_GDKMM_LIBRARY

			GTK3_GLIBMM_INCLUDE_DIR
			GTK3_GLIBMMCONFIG_INCLUDE_DIR
			GTK3_GLIBMM_LIBRARY

			FREETYPE_INCLUDE_DIR_ft2build
			FREETYPE_INCLUDE_DIR_freetype2
		)
	elseif(_GTK3_component STREQUAL "glade")
		FIND_PACKAGE_HANDLE_STANDARD_ARGS(GTK3_${_COMPONENT_UPPER} "The glade library was not found."
			GTK3_GLADE_LIBRARY
			GTK3_GLADE_INCLUDE_DIR
		)
	elseif(_GTK3_component STREQUAL "glademm")
		FIND_PACKAGE_HANDLE_STANDARD_ARGS(GTK3_${_COMPONENT_UPPER} "The glademm library was not found."
			GTK3_GLADEMM_LIBRARY
			GTK3_GLADEMM_INCLUDE_DIR
			GTK3_GLADEMMCONFIG_INCLUDE_DIR
		)
	endif()

	if(NOT GTK3_${_COMPONENT_UPPER}_FOUND)
		set(_GTK3_did_we_find_everything false)
	endif()
endforeach()

if(_GTK3_did_we_find_everything AND NOT GTK3_VERSION_CHECK_FAILED)
	set(GTK3_FOUND true)
else()
	# Unset our variables.
	set(GTK3_FOUND false)
	set(GTK3_VERSION)
	set(GTK3_VERSION_MAJOR)
	set(GTK3_VERSION_MINOR)
	set(GTK3_VERSION_PATCH)
	set(GTK3_INCLUDE_DIRS)
	set(GTK3_LIBRARIES)
	set(GTK3_DEFINITIONS)
endif()

if(GTK3_INCLUDE_DIRS)
	list(REMOVE_DUPLICATES GTK3_INCLUDE_DIRS)
endif()

