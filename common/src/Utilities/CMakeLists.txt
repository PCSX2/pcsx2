# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)


set(Output Utilities)

set(CommonFlags 
	-fno-strict-aliasing
    )

# Debug - Build
if(CMAKE_BUILD_TYPE STREQUAL Debug)
	
	# add defines
	set(UtilitiesFinalFlags
		${UtilitiesFinalFlags}
		${CommonFlags} -DPCSX2_DEBUG -DPCSX2_DEVBUILD
	)
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

# Devel - Build
if(CMAKE_BUILD_TYPE STREQUAL Devel)
	
	# add defines
	set(UtilitiesFinalFlags
		${UtilitiesFinalFlags}
		${CommonFlags} -DPCSX2_DEVBUILD
	)
endif(CMAKE_BUILD_TYPE STREQUAL Devel)

# Release - Build
if(CMAKE_BUILD_TYPE STREQUAL Release)
	
	# add defines
	set(UtilitiesFinalFlags
		${UtilitiesFinalFlags}
		${CommonFlags}
	)
endif(CMAKE_BUILD_TYPE STREQUAL Release)

# variable with all sources of this library
set(UtilitiesSources
	VirtualMemory.cpp
	AlignedMalloc.cpp
	../../include/Utilities/FixedPointTypes.inl
	../../include/Utilities/EventSource.inl
	../../include/Utilities/SafeArray.inl
	../../include/Utilities/TlsVariable.inl
	CheckedStaticBox.cpp
	Console.cpp
	EventSource.cpp
	Exceptions.cpp
	FastFormatString.cpp
	HashTools.cpp
	IniInterface.cpp
	Linux/LnxHostSys.cpp
	Mutex.cpp
	PathUtils.cpp
	PrecompiledHeader.cpp
	pxCheckBox.cpp
	pxRadioPanel.cpp
	pxStaticText.cpp
	pxStreams.cpp
	pxTranslate.cpp
	pxWindowTextWriter.cpp
	StringHelpers.cpp
	ThreadingDialogs.cpp
	ThreadTools.cpp
	vssprintf.cpp
	wxAppWithHelpers.cpp
	wxGuiTools.cpp
	wxHelpers.cpp
	)

# variable with all headers of this library
set(UtilitiesHeaders
	../../include/Utilities/Assertions.h
	../../include/Utilities/CheckedStaticBox.h
	../../include/Utilities/Console.h
	../../include/Utilities/Dependencies.h
	../../include/Utilities/EventSource.h
	../../include/Utilities/Exceptions.h
	../../include/Utilities/FixedPointTypes.h
	../../include/Utilities/General.h
	../../include/Utilities/HashMap.h
	../../include/Utilities/lnx_memzero.h
	../../include/Utilities/MemcpyFast.h
	../../include/Utilities/MemsetFast.inl
	../../include/Utilities/Path.h
	../../include/Utilities/PageFaultSource.h
	../../include/Utilities/pxCheckBox.h
	../../include/Utilities/pxForwardDefs.h
	../../include/Utilities/pxRadioPanel.h
	../../include/Utilities/pxStaticText.h
	../../include/Utilities/pxStreams.h
	../../include/Utilities/RedtapeWindows.h
	../../include/Utilities/SafeArray.h
	../../include/Utilities/ScopedAlloc.h
	../../include/Utilities/ScopedPtr.h
	../../include/Utilities/ScopedPtrMT.h
	../../include/Utilities/StringHelpers.h
	../../include/Utilities/Threading.h
	../../include/Utilities/ThreadingDialogs.h
	../../include/Utilities/TraceLog.h
	../../include/Utilities/wxAppWithHelpers.h
	../../include/Utilities/wxBaseTools.h
	../../include/Utilities/wxGuiTools.h
	PrecompiledHeader.h
)

if(APPLE)
	LIST(APPEND UtilitiesSources
		Darwin/DarwinThreads.cpp
		Darwin/DarwinMisc.cpp
		Darwin/DarwinSemaphore.cpp
	)
else()
	LIST(APPEND UtilitiesSources
		Linux/LnxThreads.cpp
		Linux/LnxMisc.cpp
		Semaphore.cpp
	)
endif()

set(UtilitiesFinalSources
	${UtilitiesSources}
	${UtilitiesHeaders}
)

set(UtilitiesFinalLibs
    ${LIBC_LIBRARIES} # Gold (new linux linker) does not get automatically dependency of dependency
	${wxWidgets_LIBRARIES}
)

add_pcsx2_lib(${Output} "${UtilitiesFinalSources}" "${UtilitiesFinalLibs}" "${UtilitiesFinalFlags}")
add_pcsx2_lib(${Output}_NO_TLS "${UtilitiesFinalSources}" "${UtilitiesFinalLibs}" "${UtilitiesFinalFlags} -DPCSX2_THREAD_LOCAL=0")
