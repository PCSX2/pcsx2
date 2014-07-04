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

# spu2x Linux sources
set(spu2xLinuxSources
    Linux/AboutBox.cpp
    Linux/Alsa.cpp
    Linux/CfgHelpers.cpp
    Linux/Config.cpp
    Linux/ConfigDebug.cpp
    Linux/ConfigSoundTouch.cpp
    Linux/Dialogs.cpp)

set(spu2xFinalSources
${spu2xSources} 
${spu2xLinuxSources}
)

# add additional include directories
include_directories(Linux)

if(SDL_FOUND)
    list(APPEND spu2xSources SndOut_SDL.cpp)
    add_definitions(-DSPU2X_SDL)
endif()

add_pcsx2_plugin(spu2x "${spu2xFinalSources}" "${CMAKE_THREAD_LIBS_INIT} Utilities ${SDL_LIBRARY} ${ALSA_LIBRARIES} ${PORTAUDIO_LIBRARIES} ${SOUNDTOUCH_LIBRARIES} ${GTK2_LIBRARIES} ${X11_LIBRARIES}")

