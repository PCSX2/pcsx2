# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# variable with all sources of this library
set(x86emitterSources
	../../include/x86emitter/inlines.inl
	3dnow.cpp
	cpudetect.cpp
	fpu.cpp
	groups.cpp
	jmp.cpp
	legacy.cpp
	legacy_sse.cpp
	LnxCpuDetect.cpp
	movs.cpp
	PrecompiledHeader.cpp
	simd.cpp
	x86emitter.cpp)

set(x86emitterFinalLibs
${wxWidgets_LIBRARIES} 
${CMAKE_THREAD_LIBS_INIT}
	)

add_pcsx2_lib(x86emitter "${x86emitterSources}" "${x86emitterFinalLibs}")

