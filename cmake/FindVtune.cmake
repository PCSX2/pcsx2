# Find Intel's VTUNE tool

# VTUNE_FOUND			found Vtune
# VTUNE_INCLUDE_DIRS	include path to jitprofiling.h
# VTUNE_LIBRARIES		path to vtune libs

find_path(VTUNE_INCLUDE_DIRS NAMES jitprofiling.h PATHS
    /opt/intel/vtune_amplifier_xe_2018/include
    /opt/intel/vtune_amplifier_xe_2017/include
    /opt/intel/vtune_amplifier_xe_2016/include
    )

if(${PCSX2_TARGET_ARCHITECTURES} MATCHES "i386")
    find_library(VTUNE_LIBRARIES NAMES libjitprofiling.a PATHS
        /opt/intel/vtune_amplifier_xe_2018/lib32
        /opt/intel/vtune_amplifier_xe_2017/lib32
        /opt/intel/vtune_amplifier_xe_2016/lib32
        )
else()
    find_library(VTUNE_LIBRARIES NAMES libjitprofiling.a PATHS
        /opt/intel/vtune_amplifier_xe_2018/lib64
        /opt/intel/vtune_amplifier_xe_2017/lib64
        /opt/intel/vtune_amplifier_xe_2016/lib64
        )
endif()

# handle the QUIETLY and REQUIRED arguments and set VTUNE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vtune DEFAULT_MSG VTUNE_LIBRARIES VTUNE_INCLUDE_DIRS)

mark_as_advanced(VTUNE_FOUND VTUNE_INCLUDE_DIRS VTUNE_LIBRARIES)

