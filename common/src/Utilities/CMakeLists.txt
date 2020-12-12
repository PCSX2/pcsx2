# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

set(Output Utilities)

set(CommonFlags
    )

set(UtilitiesFinalFlags ${CommonFlags})

# variable with all sources of this library
set(UtilitiesSources
	VirtualMemory.cpp
	AlignedMalloc.cpp
	../../include/Utilities/FixedPointTypes.inl
	../../include/Utilities/EventSource.inl
	../../include/Utilities/SafeArray.inl
	CheckedStaticBox.cpp
	Console.cpp
	EventSource.cpp
	Exceptions.cpp
	FastFormatString.cpp
	IniInterface.cpp
	Linux/LnxHostSys.cpp
	Mutex.cpp
	PathUtils.cpp
	PrecompiledHeader.cpp
	Perf.cpp
	pxCheckBox.cpp
	pxRadioPanel.cpp
	pxStaticText.cpp
	pxStreams.cpp
	pxTranslate.cpp
	pxWindowTextWriter.cpp
	RwMutex.cpp
	StringHelpers.cpp
	ThreadingDialogs.cpp
	ThreadTools.cpp
	wxAppWithHelpers.cpp
	wxGuiTools.cpp
	wxHelpers.cpp
	)

# variable with all headers of this library
set(UtilitiesHeaders
	../../include/Utilities/Assertions.h
	../../include/Utilities/boost_spsc_queue.hpp
	../../include/Utilities/CheckedStaticBox.h
	../../include/Utilities/Console.h
	../../include/Utilities/Dependencies.h
	../../include/Utilities/EventSource.h
	../../include/Utilities/Exceptions.h
	../../include/Utilities/FixedPointTypes.h
	../../include/Utilities/General.h
	../../include/Utilities/MakeUnique.h
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
	../../include/Utilities/RwMutex.h
	../../include/Utilities/SafeArray.h
	../../include/Utilities/ScopedAlloc.h
	../../include/Utilities/ScopedPtrMT.h
	../../include/Utilities/StringHelpers.h
	../../include/Utilities/Threading.h
	../../include/Utilities/ThreadingDialogs.h
	../../include/Utilities/TraceLog.h
	../../include/Utilities/wxAppWithHelpers.h
	../../include/Utilities/wxBaseTools.h
	../../include/Utilities/wxGuiTools.h
	PrecompiledHeader.h
	ThreadingInternal.h
)

if(APPLE)
	LIST(APPEND UtilitiesSources
		Darwin/DarwinThreads.cpp
		Darwin/DarwinMisc.cpp
		Darwin/DarwinSemaphore.cpp
	)
elseif(Windows)
	LIST(APPEND UtilitiesSources
		x86/MemcpyFast.cpp
		Windows/WinThreads.cpp
		Windows/WinHostSys.cpp
		Windows/WinMisc.cpp
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
	yaml-cpp
)

add_pcsx2_lib(${Output} "${UtilitiesFinalSources}" "${UtilitiesFinalLibs}" "${UtilitiesFinalFlags}")
add_pcsx2_lib(${Output}_NO_TLS "${UtilitiesFinalSources}" "${UtilitiesFinalLibs}" "${UtilitiesFinalFlags} -DPCSX2_THREAD_LOCAL=0")

target_compile_features(${Output} PUBLIC cxx_std_17)
target_compile_features(${Output}_NO_TLS PUBLIC cxx_std_17)
target_include_directories(${Output} PUBLIC ../../../3rdparty/include)
target_include_directories(${Output}_NO_TLS PUBLIC ../../../3rdparty/include)

if(COMMAND target_precompile_headers)
	target_precompile_headers(${Output} PRIVATE PrecompiledHeader.h)
	target_precompile_headers(${Output}_NO_TLS PRIVATE PrecompiledHeader.h)
endif()
