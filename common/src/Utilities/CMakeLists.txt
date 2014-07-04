# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# variable with all sources of this library
set(UtilitiesSources
	../../include/Utilities/FixedPointTypes.inl
	../../include/Utilities/EventSource.inl
    ../../include/Utilities/SafeArray.inl
    ../../include/Utilities/TlsVariable.inl
	AlignedMalloc.cpp
	CheckedStaticBox.cpp
	Console.cpp
	EventSource.cpp
	Exceptions.cpp
	FastFormatString.cpp
	HashTools.cpp
	IniInterface.cpp
	Linux/LnxHostSys.cpp
	Linux/LnxMisc.cpp
	Linux/LnxThreads.cpp
	Mutex.cpp
	PathUtils.cpp
	PrecompiledHeader.cpp
	pxCheckBox.cpp
	pxRadioPanel.cpp
	pxStaticText.cpp
	pxStreams.cpp
	pxTranslate.cpp
	pxWindowTextWriter.cpp
	Semaphore.cpp
	StringHelpers.cpp
	ThreadingDialogs.cpp
	ThreadTools.cpp
	vssprintf.cpp
	VirtualMemory.cpp
	wxAppWithHelpers.cpp
	wxGuiTools.cpp
	wxHelpers.cpp
	x86/MemcpyVibes.cpp
#	x86/MemcpyFast.cpp
	)

# collect .S files
set(UtilitiesSSources
	x86/MemcpyFast.S)
set(UtilitiesFinalSources 
	${UtilitiesSources} 
	${UtilitiesSSources}
	)
set(UtilitiesFinalLibs
	${wxWidgets_LIBRARIES} 
	${CMAKE_THREAD_LIBS_INIT}
	)
# change language of .S-files to c++
set_source_files_properties(${UtilitiesSSources} PROPERTIES LANGUAGE CXX)

add_pcsx2_lib(Utilities "${UtilitiesFinalSources}" "${UtilitiesFinalLibs}")

