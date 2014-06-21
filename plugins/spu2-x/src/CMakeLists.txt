# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

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

if(SDL_FOUND)
    list(APPEND spu2xSources SndOut_SDL.cpp)
    add_definitions(-DSPU2X_SDL)
endif()

add_pcsx2_plugin(spu2x "${spu2xSources} ${spu2xHeaders} ${spu2xLinuxSources} ${spu2xLinuxHeaders}" "Utilities ${SDL_LIBRARY} ${ALSA_LIBRARIES} ${PORTAUDIO_LIBRARIES} ${SOUNDTOUCH_LIBRARIES} ${GTK2_LIBRARIES} ${X11_LIBRARIES}")

