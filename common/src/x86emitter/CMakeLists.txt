# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
	message(FATAL_ERROR "
	You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
	It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

add_library(x86emitter)

# x86emitter sources
target_sources(x86emitter PRIVATE
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

# x86emitter headers
target_sources(x86emitter PRIVATE
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
	../../include/x86emitter/implement/simd_templated_helpers.h
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
	target_sources(x86emitter PRIVATE WinCpuDetect.cpp)
else()
	target_sources(x86emitter PRIVATE LnxCpuDetect.cpp)
endif()

target_link_libraries(x86emitter PRIVATE Utilities wxWidgets::all)
target_compile_features(x86emitter PUBLIC cxx_std_17)
target_include_directories(x86emitter PUBLIC ../../include PRIVATE ../../include/x86emitter)
target_compile_definitions(x86emitter PUBLIC "${PCSX2_DEFS}")
target_compile_options(x86emitter PRIVATE "${PCSX2_WARNINGS}")

if(COMMAND target_precompile_headers)
	target_precompile_headers(x86emitter PRIVATE PrecompiledHeader.h)
endif()
