if (openSUSE)
    # openSUSE don't install wx in a standard library system
    # path. Let's bypass the dynamic linker and hardcode the path.
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
endif()

# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output spu2x-2.0.0)

set(OptimizationFlags
    -O2
    -DNDEBUG
    )

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(spu2xFinalFlags "")

elseif(CMAKE_BUILD_TYPE STREQUAL Devel)
	set(spu2xFinalFlags ${OptimizationFlags})

elseif(CMAKE_BUILD_TYPE STREQUAL Release)
	set(spu2xFinalFlags ${OptimizationFlags})

endif()

# spu2x sources
set(spu2xSources
    ADSR.cpp
    Debug.cpp
    DplIIdecoder.cpp
    Dma.cpp
    Lowpass.cpp
    Mixer.cpp
    PrecompiledHeader.cpp
    PS2E-spu2.cpp
    ReadInput.cpp
    RegLog.cpp
    RegTable.cpp
    Reverb.cpp
    SndOut.cpp
    SndOut_Portaudio.cpp
    SndOut_SDL.cpp
    spu2freeze.cpp
    Spu2replay.cpp
    spu2sys.cpp
    Timestretcher.cpp
    Wavedump_wav.cpp
    WavFile.cpp
    )

# spu2x headers
set(spu2xHeaders
    Config.h
    Debug.h
    defs.h
    Dma.h
    DPLII.h
    Global.h
    Lowpass.h
    Mixer.h
    PS2E-spu2.h
    regs.h
    SndOut.h
    spdif.h
    Spu2replay.h
    WavFile.h
)


# spu2x Linux sources
set(spu2xLinuxSources
    Linux/AboutBox.cpp
    Linux/Alsa.cpp
    Linux/CfgHelpers.cpp
    Linux/Config.cpp
    Linux/ConfigDebug.cpp
    Linux/ConfigSoundTouch.cpp
    Linux/Dialogs.cpp)

# spu2x Linux headers
set(spu2xLinuxHeaders
    Linux/Alsa.h
    Linux/Config.h
    Linux/Dialogs.h)

# add additional include directories
include_directories(Linux)

set(spu2xFinalSources
    ${spu2xSources}
    ${spu2xHeaders}
    ${spu2xLinuxSources}
    ${spu2xLinuxHeaders}
)

set(spu2xFinalLibs
    Utilities_NO_TLS
    ${ALSA_LIBRARIES}
    ${PORTAUDIO_LIBRARIES}
    ${GTK2_LIBRARIES}
    ${SOUNDTOUCH_LIBRARIES}
)

if (SDL2_API)
    set(spu2xFinalLibs
        ${spu2xFinalLibs}
        ${SDL2_LIBRARIES}
    )
else()
    set(spu2xFinalLibs
        ${spu2xFinalLibs}
        ${SDL_LIBRARY}
    )
endif()

add_pcsx2_plugin(${Output} "${spu2xFinalSources}" "${spu2xFinalLibs}" "${spu2xFinalFlags}")
