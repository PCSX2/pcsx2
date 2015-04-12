# CDVDlinuz Plugin

# plugin name
set(Output CDVDlinuz)

set(CommonFlags
    -D_LARGEFILE64_SOURCE
    )

set(OptimizationFlags
    -O2
    -fomit-frame-pointer
    )

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CDVDlinuzFinalFlags ${CommonFlags})

elseif(CMAKE_BUILD_TYPE STREQUAL Devel)
	set(CDVDlinuzFinalFlags ${CommonFlags} ${OptimizationFlags})

elseif(CMAKE_BUILD_TYPE STREQUAL Release)
	set(CDVDlinuzFinalFlags ${CommonFlags} ${OptimizationFlags})

endif()

# CDVDlinuz sources
set(CDVDlinuzSources
    buffer.c
    convert.c
    ini.c
    version.c)

# CDVDlinuz headers
set(CDVDlinuzHeaders
    buffer.h
    convert.h
    ini.h
    PS2Edefs.h
    PS2Etypes.h
    # ERROR It takes /usr/include/alsa/version.h
    # The content was move to Linux/CDVDlinuz.h
    # version.h
)

# CDVDlinuz Unix sources
set(CDVDlinuzUnixSources
    Linux/aboutbox.c
    Linux/actualfile.c
    Linux/CD.c
    Linux/CDVDlinuz.c
    Linux/conf.c
    Linux/device.c
    Linux/DVD.c
    Linux/interface.c
    Linux/logfile.c
    Linux/mainbox.c)

# CDVDlinuz Unix headers
set(CDVDlinuzUnixHeaders
    Linux/aboutbox.h
    Linux/actualfile.h
    Linux/CD.h
    Linux/CDVDlinuz.h
    Linux/conf.h
    Linux/device.h
    Linux/DVD.h
    Linux/logfile.h
    Linux/mainbox.h)

set(CDVDlinuzFinalSources
    ${CDVDlinuzSources}
    ${CDVDlinuzHeaders}
    ${CDVDlinuzUnixSources}
    ${CDVDlinuzUnixHeaders}
)

set(CDVDlinuzFinalLibs
)

# add additional include directories
include_directories(Linux)

add_pcsx2_plugin(${Output} "${CDVDlinuzFinalSources}" "${CDVDlinuzFinalLibs}" "${CDVDlinuzFinalFlags}")
