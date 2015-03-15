# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "This CMake file isn't intended to be used by itself, go to the top level pcsx2 directory and build from there.")
endif()

# plugin name
set(Output MultiNull)

set(OptimizationFlags
    -O2
)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
	set(MultiNullFinalFlags "")
elseif(CMAKE_BUILD_TYPE STREQUAL Devel)
	set(MultiNullFinalFlags ${OptimizationFlags})
elseif(CMAKE_BUILD_TYPE STREQUAL Release)
	set(MultiNullFinalFlags ${OptimizationFlags})
endif()

# MultiNull sources
set(MultiNullSources
	CDVD.cpp
	DEV9.cpp
	FW.cpp
	GS.cpp
	gsnull_GifTransfer.cpp
	gsnull_Registers.cpp
	Pad.cpp
	SPU2.cpp
	USB.cpp
	PS2Einterface.cpp
)

# MultiNull headers
set(MultiNullHeaders
	CDVD.h
	DEV9.h
	FW.h
	GS.h
	gsnull_GifTransfer.h
	gsnull_Registers.h
	Pad.h
	SPU2.h
	USB.h
)

set(MultiNullFinalSources
	${MultiNullSources}
	${MultiNullHeaders}
)

set(MultiNullFinalLibs
	${GTK2_LIBRARIES}
)

add_pcsx2_plugin(${Output} "${MultiNullFinalSources}" "${MultiNullFinalLibs}" "${MultiNullFinalFlags}")
