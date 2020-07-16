#Check that people use the good file
if (NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advised to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()

#plugin name(no version number to ease future version bump and bisect)
set(Output cdvdGigaherz)

set(FinalFlags
    -fvisibility=hidden
    -Wall
    -Wno-parentheses
    )

set(Sources
    CDVD.cpp
    ReadThread.cpp
    Settings.cpp
    TocStuff.cpp
)

set(Headers
    CDVD.h
    Settings.h
)

if(Windows)
    LIST(APPEND Sources
        Windows/config.cpp
        Windows/resource.h
        )
else()
    LIST(APPEND Sources
        Unix/GtkGui.cpp
        Unix/LinuxConfig.cpp
        Unix/LinuxIOCtlSrc.cpp
        )
endif()

set(FinalSources
    ${Sources}
    ${Headers}
)

set(FinalLibs
    ${GTK2_LIBRARIES}
    ${LIBC_LIBRARIES}
    ${LIBUDEV_LIBRARIES}
)

add_pcsx2_plugin(${Output} "${FinalSources}" "${FinalLibs}" "${FinalFlags}")
