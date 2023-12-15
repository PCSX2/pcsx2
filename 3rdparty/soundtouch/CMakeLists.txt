add_library(pcsx2-soundtouch
	source/SoundTouch/AAFilter.cpp
	source/SoundTouch/BPMDetect.cpp
	source/SoundTouch/cpu_detect_x86.cpp
	source/SoundTouch/FIFOSampleBuffer.cpp
	source/SoundTouch/FIRFilter.cpp
	source/SoundTouch/InterpolateCubic.cpp
	source/SoundTouch/InterpolateLinear.cpp
	source/SoundTouch/InterpolateShannon.cpp
	source/SoundTouch/mmx_optimized.cpp
	source/SoundTouch/PeakFinder.cpp
	source/SoundTouch/RateTransposer.cpp
	source/SoundTouch/SoundTouch.cpp
	source/SoundTouch/sse_optimized.cpp
	source/SoundTouch/TDStretch.cpp
	soundtouch/BPMDetect.h
	soundtouch/FIFOSampleBuffer.h
	soundtouch/FIFOSamplePipe.h
	soundtouch/SoundTouch.h
	soundtouch/STTypes.h
	source/SoundTouch/AAFilter.h
	source/SoundTouch/cpu_detect.h
	source/SoundTouch/FIRFilter.h
	source/SoundTouch/InterpolateCubic.h
	source/SoundTouch/InterpolateLinear.h
	source/SoundTouch/InterpolateShannon.h
	source/SoundTouch/PeakFinder.h
	source/SoundTouch/RateTransposer.h
	source/SoundTouch/TDStretch.h
)

target_include_directories(pcsx2-soundtouch PUBLIC soundtouch)
target_compile_definitions(pcsx2-soundtouch PUBLIC SOUNDTOUCH_FLOAT_SAMPLES ST_NO_EXCEPTION_HANDLING)
set_property(TARGET pcsx2-soundtouch PROPERTY CXX_STANDARD 17)
set_property(TARGET pcsx2-soundtouch PROPERTY CXX_STANDARD_REQUIRED ON)

if(NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug")
	if(MSVC)
		target_compile_options(pcsx2-soundtouch PRIVATE /O2 /fp:fast)
	else()
		target_compile_options(pcsx2-soundtouch PRIVATE -Ofast)
	endif()
endif()

add_library(SoundTouch::SoundTouch ALIAS pcsx2-soundtouch)

