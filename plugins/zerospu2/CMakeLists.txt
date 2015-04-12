# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output zerospu2)

set(OptimizationFlags
    -O2
    )

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(zerospu2FinalFlags "")

elseif(CMAKE_BUILD_TYPE STREQUAL Devel)
	set(zerospu2FinalFlags ${OptimizationFlags})

elseif(CMAKE_BUILD_TYPE STREQUAL Release)
	set(zerospu2FinalFlags ${OptimizationFlags})

endif(CMAKE_BUILD_TYPE STREQUAL Release)

# zerospu2 sources
set(zerospu2Sources
	voices.cpp
    WavFile.cpp
	zerodma.cpp
	zerospu2.cpp
	zeroworker.cpp)

# zerospu2 headers
set(zerospu2Headers
	misc.h
	reg.h
    WavFile.h
	zerodma.h
	zerospu2.h
	zeroworker.h)

# zerospu2 Unix sources
set(zerospu2UnixSources
	Linux/Linux.cpp
	Targets/Alsa.cpp
	Targets/OSS.cpp
#	Targets/PortAudio.cpp
	Targets/SoundTargets.cpp)

# zerospu2 Unix headers
set(zerospu2UnixHeaders
	Linux/Linux.h
	Targets/Alsa.h
	Targets/OSS.h
#	Targets/PortAudio.h
	Targets/SoundTargets.h)

# zerospu2 Windows sources
set(zerospu2WindowsSources
	Targets/dsound51.cpp
	Windows/Win32.cpp
	Windows/ZeroSPU2.def
	Windows/ZeroSPU2.rc)

# zerospu2 Windows headers
set(zerospu2WindowsHeaders
	resources.h
	Targets/dsound51.h)

# add additional include directories
include_directories(Linux Targets)

set(zerospu2FinalSources
	${zerospu2Sources}
	${zerospu2Headers}
	${zerospu2UnixSources}
	${zerospu2UnixHeaders}
)

set(zerospu2FinalLibs
	${ALSA_LIBRARIES}
	# PortAudio would go here.
	${SOUNDTOUCH_LIBRARIES}
)

if(PORTAUDIO_FOUND)
	# link target with PortAudio
	#target_link_libraries(${Output} ${PORTAUDIO_LIBRARIES})
endif(PORTAUDIO_FOUND)

add_pcsx2_plugin(${Output} "${zerospu2FinalSources}" "${zerospu2FinalLibs}" "${zerospu2FinalFlags}")
