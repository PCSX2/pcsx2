# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)


# plugin name
set(Output zzogl-cg-shader)

set(CommonFlags
    -pthread
    -DZEROGS_SSE2
    -fno-strict-aliasing
    -Wstrict-aliasing # Allow to track strict aliasing issue.
    -Wunused-variable
    -DNVIDIA_CG_API
    )

set(OptimizationFlags
    -O2
    -DNDEBUG
    )

# Debug - Build
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    # add defines
    set(zerogsshadersFinalFlags 
        ${CommonFlags} -g -Wall -D_DEBUG
    )
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

# Devel - Build
if(CMAKE_BUILD_TYPE STREQUAL Devel)
    # add defines
    set(zerogsshadersFinalFlags 
        ${CommonFlags} ${OptimizationFlags} -g -W -DZEROGS_DEVBUILD
    )
endif(CMAKE_BUILD_TYPE STREQUAL Devel)

# Release - Build
if(CMAKE_BUILD_TYPE STREQUAL Release)
    # add defines
    set(zerogsshadersFinalFlags 
        ${CommonFlags} ${OptimizationFlags} -W
    )
endif(CMAKE_BUILD_TYPE STREQUAL Release)

set(zerogsshadersFinalSources 
    zerogsshaders.cpp 
    zpipe.cpp
)

set(zerogsshadersFinalLibs 
    ${ZLIB_LIBRARIES}
    ${CG_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

add_pcsx2_executable(${Output} "${zerogsshadersFinalSources}" "${zerogsshadersFinalLibs}" "${zerogsshadersFinalFlags}")

