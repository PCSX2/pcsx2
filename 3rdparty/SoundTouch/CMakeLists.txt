set(SRCS
	AAFilter.cpp
	BPMDetect.cpp
	cpu_detect_x86.cpp
	FIFOSampleBuffer.cpp
	FIRFilter.cpp
	mmx_optimized.cpp
	PeakFinder.cpp
	RateTransposer.cpp
	SoundTouch.cpp
	sse_optimized.cpp
	TDStretch.cpp
	)

add_library(SoundTouch STATIC ${SRCS})
