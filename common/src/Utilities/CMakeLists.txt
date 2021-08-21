# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
	message(FATAL_ERROR "
	You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
	It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

add_library(Utilities)

# Utilities sources
target_sources(Utilities PRIVATE
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

# Utilities headers
target_sources(Utilities PRIVATE
	../../include/Utilities/Assertions.h
	../../include/Utilities/boost_spsc_queue.hpp
	../../include/Utilities/CheckedStaticBox.h
	../../include/Utilities/Console.h
	../../include/Utilities/Dependencies.h
	../../include/Utilities/EventSource.h
	../../include/Utilities/Exceptions.h
	../../include/Utilities/FixedPointTypes.h
	../../include/Utilities/General.h
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
	target_sources(Utilities PRIVATE
		Darwin/DarwinThreads.cpp
		Darwin/DarwinMisc.cpp
		Darwin/DarwinSemaphore.cpp
	)
elseif(Windows)
	target_sources(Utilities PRIVATE
		x86/MemcpyFast.cpp
		Windows/WinThreads.cpp
		Windows/WinHostSys.cpp
		Windows/WinMisc.cpp
	)
else()
	target_sources(Utilities PRIVATE
		Linux/LnxThreads.cpp
		Linux/LnxMisc.cpp
		Semaphore.cpp
	)
endif()

if(USE_VTUNE)
	target_link_libraries(Utilities PUBLIC Vtune::Vtune)
endif()

target_link_libraries(Utilities PRIVATE ${LIBC_LIBRARIES} PUBLIC wxWidgets::all)
target_compile_features(Utilities PUBLIC cxx_std_17)
target_include_directories(Utilities PUBLIC ../../../3rdparty/include ../../include PRIVATE ../../include/Utilities .)
target_compile_definitions(Utilities PUBLIC "${PCSX2_DEFS}")
target_compile_options(Utilities PRIVATE "${PCSX2_WARNINGS}")
if(COMMAND target_precompile_headers)
	target_precompile_headers(Utilities PRIVATE PrecompiledHeader.h)
endif()
