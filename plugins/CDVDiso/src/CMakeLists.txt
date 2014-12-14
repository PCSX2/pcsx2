# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output CDVDiso)

set(OptimizationFlags
    -O2
    -DNDEBUG
    )

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CDVDisoFinalFlags "")

elseif(CMAKE_BUILD_TYPE STREQUAL Devel)
	set(CDVDisoFinalFlags ${OptimizationFlags})

elseif(CMAKE_BUILD_TYPE STREQUAL Release)
	set(CDVDisoFinalFlags ${OptimizationFlags})

endif()

# CDVDiso sources
set(CDVDisoSources
    CDVDisop.cpp
    libiso.cpp)

# CDVDiso headers
set(CDVDisoHeaders
    CDVDiso.h
    libiso.h)

# CDVDiso Linux sources
set(CDVDisoLinuxSources
    Linux/Config.cpp
    Linux/interface.c
    Linux/Linux.cpp
    Linux/support.c)

# CDVDiso Linux headers
set(CDVDisoLinuxHeaders
    Linux/callbacks.h
    Linux/Config.h
    Linux/interface.h
    Linux/support.h)

set(CDVDisoFinalSources
    ${CDVDisoSources}
    ${CDVDisoHeaders}
    ${CDVDisoLinuxSources}
    ${CDVDisoLinuxHeaders}
)

set(CDVDisoFinalLibs
    ${BZIP2_LIBRARIES}
)

# add additional include directories
include_directories(Linux)

add_pcsx2_plugin(${Output} "${CDVDisoFinalSources}" "${CDVDisoFinalLibs}" "${CDVDisoFinalFlags}")
