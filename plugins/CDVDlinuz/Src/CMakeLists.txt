# CDVDlinuz Plugin

# CDVDlinuz sources
set(CDVDlinuzSources
    buffer.c
    convert.c
    ini.c
    version.c)

# CDVDlinuz Linux sources
set(CDVDlinuzLinuxSources
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

# add additional include directories
include_directories(Linux)

# add library
set(CDVDlinuzFinalSources
    ${CDVDlinuzSources}
    ${CDVDlinuzLinuxSources}
    )
add_definitions(-D_LARGEFILE64_SOURCE)
add_pcsx2_plugin(CDVDlinuz "${CDVDlinuzFinalSources}" "")