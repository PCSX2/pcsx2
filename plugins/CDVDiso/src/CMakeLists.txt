# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# CDVDiso sources
set(CDVDisoSources
    CDVDisop.cpp
    libiso.cpp)

# CDVDiso Linux sources
set(CDVDisoLinuxSources
    Linux/Config.cpp
    Linux/interface.c
    Linux/Linux.cpp
    Linux/support.c)

set(CDVDisoFinalSources
    ${CDVDisoSources} 
    ${CDVDisoLinuxSources}
)

# add additional include directories
include_directories(Linux)

add_pcsx2_plugin(CDVDiso "${CDVDisoFinalSources}" "${CMAKE_THREAD_LIBS_INIT} ${BZIP2_LIBRARIES}")
