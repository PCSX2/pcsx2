option(ENABLE_UNSUPPORTED_TESTS "Include tests that require instructions not supported by this computer" OFF)

set(avxdetect_code "
	#ifdef __GNUC__
		#include <cpuid.h>
		void cpuid(int result[4], int leaf, int subleaf) {
			__cpuid_count(leaf, subleaf, result[0], result[1], result[2], result[3]);
		}
	#else
		#include <intrin.h>
		#define cpuid __cpuidex
	#endif
	int test(int leaf, int subleaf, int reg, int bit) {
		int res[4];
		cpuid(res, 0, 0);
		if (res[0] < leaf) { return 0; }
		cpuid(res, leaf, subleaf);
		return !!(res[reg] & (1 << bit));
	}
	int main() {
		if (test(7, 0, 1,  5) /* AVX2  */) return 51;
		if (test(1, 0, 2, 28) /* AVX   */) return 50;
		if (test(1, 0, 2, 19) /* SSE41 */) return 41;
		return 0;
	}
")

if(ENABLE_UNSUPPORTED_TESTS)
	set(native_vector_isa 99)
else()
	set(cc_backup ${CMAKE_CROSSCOMPILING})
	set(CMAKE_CROSSCOMPILING 0)
	file(WRITE "${CMAKE_BINARY_DIR}/avxdetect.c" "${avxdetect_code}")

	try_run(
		native_vector_isa
		compile_result_unused
		"${CMAKE_BINARY_DIR}"
		"${CMAKE_BINARY_DIR}/avxdetect.c"
	)

	set(CMAKE_CROSSCOMPILING ${cc_backup})
endif()
if (MSVC)
	if(_M_X86_32)
		set(compile_options_sse4 /arch:SSE2)
	endif()
	set(compile_options_avx  /arch:AVX)
	set(compile_options_avx2 /arch:AVX2)
	set(definitions_sse4 _M_SSE=0x401)
	set(definitions_avx  _M_SSE=0x500)
	set(definitions_avx2 _M_SSE=0x501)
else()
	set(compile_options_sse4 -msse4.1)
	set(compile_options_avx  -mavx)
	set(compile_options_avx2 -mavx2 -mbmi -mbmi2)
endif()
set(isa_number_sse4 41)
set(isa_number_avx  50)
set(isa_number_avx2 51)

enable_testing()
add_custom_target(unittests)
add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND})

macro(add_pcsx2_test target)
	add_executable(${target} EXCLUDE_FROM_ALL ${ARGN})
	target_link_libraries(${target} PRIVATE gtest_main common)
	add_dependencies(unittests ${target})
	add_test(NAME ${target} COMMAND ${target})
endmacro()

add_subdirectory(x86emitter)
add_subdirectory(GS)
