# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)


set(Output x86emitter)

set(CommonFlags
    )

set(x86emitterFinalFlags ${CommonFlags})

# variable with all sources of this library
set(x86emitterSources
	bmi.cpp
	cpudetect.cpp
	fpu.cpp
	groups.cpp
	jmp.cpp
	legacy.cpp
	legacy_sse.cpp
	movs.cpp
	PrecompiledHeader.cpp
	simd.cpp
	x86emitter.cpp)

# variable with all headers of this library
set(x86emitterHeaders
	../../include/x86emitter/implement/dwshift.h
	../../include/x86emitter/implement/group1.h
	../../include/x86emitter/implement/group2.h
	../../include/x86emitter/implement/group3.h
	../../include/x86emitter/implement/helpers.h
	../../include/x86emitter/implement/incdec.h
	../../include/x86emitter/implement/jmpcall.h
	../../include/x86emitter/implement/movs.h
	../../include/x86emitter/implement/simd_arithmetic.h
	../../include/x86emitter/implement/simd_comparisons.h
	../../include/x86emitter/implement/simd_helpers.h
	../../include/x86emitter/implement/simd_moremovs.h
	../../include/x86emitter/implement/simd_shufflepack.h
	../../include/x86emitter/implement/simd_templated_helpers
	../../include/x86emitter/implement/test.h
	../../include/x86emitter/implement/xchg.h
	../../include/x86emitter/instructions.h
	../../include/x86emitter/internal.h
	../../include/x86emitter/legacy_instructions.h
	../../include/x86emitter/legacy_internal.h
	../../include/x86emitter/legacy_types.h
	../../include/x86emitter/tools.h
	../../include/x86emitter/x86emitter.h
	../../include/x86emitter/x86types.h
	PrecompiledHeader.h
	cpudetect_internal.h
	)

if(Windows)
	LIST(APPEND x86emitterSources WinCpuDetect.cpp)
else()
	LIST(APPEND x86emitterSources LnxCpuDetect.cpp)
endif()

set(x86emitterFinalSources
	${x86emitterSources}
	${x86emitterHeaders}
)

set(x86emitterFinalLibs
	${wxWidgets_LIBRARIES}
)

add_pcsx2_lib(${Output} "${x86emitterFinalSources}" "${x86emitterFinalLibs}" "${x86emitterFinalFlags}")
target_compile_features(${Output} PUBLIC cxx_std_17)

if(COMMAND target_precompile_headers)
	target_precompile_headers(${Output} PRIVATE PrecompiledHeader.h)
endif()
