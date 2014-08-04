# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)


# plugin name
set(Output spu2x-2.0.0)

set(CommonFlags
    -fvisibility=hidden
    -Wall
    )

set(OptimizationFlags
    -O2
    -DNDEBUG
    )

# Debug - Build
if(CMAKE_BUILD_TYPE STREQUAL Debug)
	# add defines
    set(spu2xFinalFlags 
        ${CommonFlags} -g
    )
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

# Devel - Build
if(CMAKE_BUILD_TYPE STREQUAL Devel)
	# add defines
	set(spu2xFinalFlags 
        ${CommonFlags} ${OptimizationFlags}
    )
endif(CMAKE_BUILD_TYPE STREQUAL Devel)

# Release - Build
if(CMAKE_BUILD_TYPE STREQUAL Release)
	# add defines
	set(spu2xFinalFlags 
        ${CommonFlags} ${OptimizationFlags}
    )
endif(CMAKE_BUILD_TYPE STREQUAL Release)

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

### Don't use yet c11 feature. I will bump gcc requirement to 4.7 when 4.9 is out.
### Note: actually it might work on 4.6 too
# # If compiling on GCC Version 4.7 or higher we build SDLMod in C++11 mode
# if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
#     execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
#     if(GCC_VERSION VERSION_GREATER 4.7)
#         set_source_files_properties(SndOut_SDL.cpp PROPERTIES COMPILE_FLAGS -std=c++11)
#         message(STATUS "GCC has C++11 support for SDLAudioMod")
#     endif(GCC_VERSION VERSION_GREATER 4.7)
# endif()

if(SDL_FOUND)
    list(APPEND spu2xSources SndOut_SDL.cpp)
    if (SDL2_API)
        set(spu2xFinalFlags 
            ${spu2xFinalFlags}
            -DSPU2X_SDL2
        )
    else()
        set(spu2xFinalFlags 
            ${spu2xFinalFlags}
            -DSPU2X_SDL
        )
    endif()
endif()

set(spu2xFinalSources 
    ${spu2xSources}
    ${spu2xHeaders}
    ${spu2xLinuxSources}
    ${spu2xLinuxHeaders}
)

set(spu2xFinalLibs 
    Utilities
    ${ALSA_LIBRARIES}
    portaudio
)

if (SDL2_API)
    set(spu2xFinalLibs 
        ${spu2xFinalLibs}
        ${SDL2_LIBRARY}
    )
else()
    set(spu2xFinalLibs 
        ${spu2xFinalLibs}
        ${SDL_LIBRARY}
    )
endif()

set(spu2xFinalLibs 
    ${spu2xFinalLibs}
    SoundTouch
    ${GTK2_LIBRARIES}
)

add_pcsx2_plugin(${Output} "${spu2xFinalSources}" "${spu2xFinalLibs}" "${spu2xFinalFlags}")
