set(SRCS
	include/FreeSurroundDecoder.h
	include/kiss_fft.h
	include/kiss_fftr.h
	src/_kiss_fft_guts.h
	src/ChannelMaps.cpp
	src/FreeSurroundDecoder.cpp
	src/kiss_fft.cpp
	src/kiss_fftr.cpp
)

add_library(freesurround ${SRCS})
target_include_directories(freesurround PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(freesurround INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
