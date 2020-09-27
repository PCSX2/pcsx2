# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# plugin name
set(Output dev9ghzdrk-0.4)

set(CommonFlags
    -fvisibility=hidden
    -Wall
    -Wno-parentheses
    -I${LIBXML2_INCLUDE_DIR}
    -I${PCAP_INCLUDE_DIR}
    )

set(OptimizationFlags
    -O2
    -DNDEBUG
    )

# Debug - Build
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    # add defines
    set(dev9ghzdrkFinalFlags
        ${CommonFlags} -g
    )
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

# Devel - Build
if(CMAKE_BUILD_TYPE STREQUAL Devel)
    # add defines
    set(dev9ghzdrkFinalFlags
        ${CommonFlags} ${OptimizationFlags}
    )
endif(CMAKE_BUILD_TYPE STREQUAL Devel)

# Release - Build
if(CMAKE_BUILD_TYPE STREQUAL Release)
    # add defines
    set(dev9ghzdrkFinalFlags
        ${CommonFlags} ${OptimizationFlags}
    )
endif(CMAKE_BUILD_TYPE STREQUAL Release)

# dev9ghzdrk sources
set(dev9ghzdrkSources
    smap.cpp
    DEV9.cpp
    flash.cpp
    pcap_io.cpp
)

# dev9ghzdrk headers
set(dev9ghzdrkHeaders
)

compile_gresources( dev9ghzdrkUI_C 
                   dev9ghzdrkUI_XML 
                   TYPE EMBED_C
                   RESOURCES "Linux/dev9ghzdrk.ui"
                   PREFIX "/net/pcsx2/dev9ghzdrk"
                   COMPRESS_ALL
                   STRIPBLANKS_ALL
)

compile_gresources( dev9ghzdrkUI_H 
                   dev9ghzdrkUI_XML 
                   TYPE EMBED_H
                   RESOURCES "Linux/dev9ghzdrk.ui"
                   PREFIX "/net/pcsx2/dev9ghzdrk"
                   COMPRESS_ALL
                   STRIPBLANKS_ALL
)

# dev9ghzdrk Linux sources
set(dev9ghzdrkLinuxSources
    Linux/Config.cpp
    Linux/Linux.cpp
    Linux/net.cpp
    ${dev9ghzdrkUI_C}
)

# dev9ghzdrk Linux headers
set(dev9ghzdrkLinuxHeaders
    ${dev9ghzdrkUI_H}
)

# dev9ghzdrk Windows sources
set(dev9ghzdrkWindowsSources
    Win32/DEV9ghzdrk.def
    Win32/Config.cpp
    Win32/Win32.cpp
    Win32/net.cpp
    Win32/tap-win32.cpp
)

# dev9ghzdrk Windows headers
set(dev9ghzdrkWindowsHeaders
)

set(dev9ghzdrkFinalSources
    ${dev9ghzdrkSources}
    ${dev9ghzdrkHeaders}
    ${dev9ghzdrkLinuxSources}
    ${dev9ghzdrkLinuxHeaders}
)

set(dev9ghzdrkFinalLibs
    ${GTK2_LIBRARIES} ${PCAP_LIBRARY} ${LIBXML2_LIBRARIES}
)

add_pcsx2_plugin(${Output} "${dev9ghzdrkFinalSources}" "${dev9ghzdrkFinalLibs}" "${dev9ghzdrkFinalFlags}")
target_compile_features(${Output} PRIVATE cxx_std_17)

#if(PACKAGE_MODE)
#    install(FILES Linux/dev9ghzdrk.ui DESTINATION bin )
#else()
#    install(FILES Linux/dev9ghzdrk.ui DESTINATION ${CMAKE_SOURCE_DIR}/bin )
#endif()
