# Find Intel's VTUNE tool

# VTUNE_FOUND        found Vtune
# Vtune::Vtune       Imported target, if found
# VTUNE_INCLUDE_DIRS include path to jitprofiling.h
# VTUNE_LIBRARIES    path to vtune libs

set(VTUNE_PATHS
	/opt/intel/oneapi/vtune/latest
	/opt/intel/vtune_amplifier_xe_2018
	/opt/intel/vtune_amplifier_xe_2017
	/opt/intel/vtune_amplifier_xe_2016
	"C:\\Program Files (x86)\\Intel\\oneAPI\\vtune\\latest"
)

find_path(VTUNE_INCLUDE_DIRS NAMES jitprofiling.h PATHS ${VTUNE_PATHS} PATH_SUFFIXES include)

find_library(VTUNE_LIBRARIES
	NAMES ${CMAKE_STATIC_LIBRARY_PREFIX}jitprofiling${CMAKE_STATIC_LIBRARY_SUFFIX}
	PATHS ${VTUNE_PATHS}
	PATH_SUFFIXES lib64
)

# handle the QUIETLY and REQUIRED arguments and set VTUNE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vtune DEFAULT_MSG VTUNE_LIBRARIES VTUNE_INCLUDE_DIRS)

if(VTUNE_LIBRARIES AND NOT TARGET Vtune::Vtune)
	add_library(Vtune::Vtune UNKNOWN IMPORTED GLOBAL)
	set_target_properties(Vtune::Vtune PROPERTIES
		IMPORTED_LOCATION "${VTUNE_LIBRARIES}"
		INTERFACE_INCLUDE_DIRECTORIES "${VTUNE_INCLUDE_DIRS}")
endif()

mark_as_advanced(VTUNE_FOUND VTUNE_INCLUDE_DIRS VTUNE_LIBRARIES)
