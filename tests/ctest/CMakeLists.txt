option(ENABLE_UNSUPPORTED_TESTS "Include tests that require instructions not supported by this computer" OFF)

set(avxdetect_code "
	int main()
	{
	#if defined(__AVX2__)
		return 51;
	#elif defined(__AVX__)
		return 50;
	#elif defined(__SSE4_1__)
		return 41;
	#else
		return 0;
	#endif
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
		CMAKE_FLAGS "-DCOMPILE_DEFINITIONS:STRING=-march=native"
	)

	set(CMAKE_CROSSCOMPILING ${cc_backup})
endif()

enable_testing()
add_custom_target(unittests)
add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND})

macro(add_pcsx2_test target)
	add_executable(${target} EXCLUDE_FROM_ALL ${ARGN})
	target_link_libraries(${target} PRIVATE x86emitter gtest_main Utilities)
	add_dependencies(unittests ${target})
	add_test(NAME ${target} COMMAND ${target})
endmacro()

add_subdirectory(x86emitter)
add_subdirectory(GS)
