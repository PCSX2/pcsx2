
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "This file isn't meant to be used by itself, use the top level one!")
endif()


# plugin name
set(Output MultiNull)

set(OptimizationFlags
    -O2
)

set(MultiNullFinalFlags 
	-DMULTINULL_BUILD_ALL
)

if(CMAKE_BUILD_TYPE STREQUAL Debug)

elseif(CMAKE_BUILD_TYPE STREQUAL Devel)
	set(MultiNullFinalFlags ${MultiNullFinalFlags} ${OptimizationFlags})
elseif(CMAKE_BUILD_TYPE STREQUAL Release)
	set(MultiNullFinalFlags ${MultiNullFinalFlags} ${OptimizationFlags})
endif()

if(MULTINULL_ENABLE_LOGGING)
	set(MultiNullFinalFlags 
		${MultiNullFinalFlags}
		-DMULTINULL_ENABLE_LOGGING
	)
endif()

# MultiNull sources
set(MultiNullSources
	src/Main.cpp
	src/Logging.cpp
	src/CDVD.cpp
	src/DEV9.cpp
	src/FW.cpp
	src/GS.cpp
	src/PAD.cpp
	src/SPU2.cpp
	src/USB.cpp
)

# MultiNull headers
set(MultiNullHeaders
	src/Main.h
	src/Logging.h
)

# MultiNull Linux sources
set(MultiNullLinuxSources
)

# MultiNull Linux headers
set(MultiNullLinuxHeaders
)

# MultiNull Windows sources
set(MultiNullWindowsSources
)

# MultiNull Windows headers
set(MultiNullWindowsHeaders
)

set(MultiNullFinalSources
	${MultiNullSources}
	${MultiNullHeaders}
	${MultiNullLinuxSources}
	${MultiNullLinuxHeaders}
)

set(MultiNullFinalLibs
)

add_pcsx2_plugin(${Output} "${MultiNullFinalSources}" "${MultiNullFinalLibs}" "${MultiNullFinalFlags}")
