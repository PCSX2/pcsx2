# bin2cpp tool

# executable name
set(bin2cppName bin2cpp)

# Debug - Build
if(CMAKE_BUILD_TYPE STREQUAL Debug)
	# add defines
	set(bin2cppFinalFlags 
		-O2 -s -Wall -fexceptions
	)
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

# Devel - Build
if(CMAKE_BUILD_TYPE STREQUAL Devel)
	# add defines
	set(bin2cppFinalFlags 
		-O2 -s -Wall -fexceptions
	)
endif(CMAKE_BUILD_TYPE STREQUAL Devel)

# Release - Build
if(CMAKE_BUILD_TYPE STREQUAL Release)
	# add defines
	set(bin2cppFinalFlags 
		-O2 -s -Wall -fexceptions
	)
endif(CMAKE_BUILD_TYPE STREQUAL Release)

# variable with all sources of this executable
set(bin2cppSources
	bin2cpp.cpp)

set(bin2cppHeaders
	)

# add executable
set(bin2cppFinalSources 
	${bin2cppSources} 
	${bin2cppHeaders}
)

add_pcsx2_executable(${bin2cppName} "${bin2cppFinalSources}" "" "${bin2cppFinalFlags}")

# set output directory
# set_target_properties(${bin2cppName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/tools/bin)

