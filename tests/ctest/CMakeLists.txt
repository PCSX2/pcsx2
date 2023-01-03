add_library(unittests-stub-host STATIC
	StubHost.cpp
)
target_link_libraries(unittests-stub-host PUBLIC
	PCSX2_FLAGS
	PCSX2
	common
)

if(APPLE)
	# For some reason this doesn't get pulled in implicitly...
	target_link_libraries(unittests-stub-host PUBLIC
		"-framework Foundation"
		"-framework Cocoa"
	)
endif()

enable_testing()
add_custom_target(unittests)
add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND})

macro(add_pcsx2_test target)
	add_executable(${target} EXCLUDE_FROM_ALL ${ARGN})
	target_link_libraries(${target} PRIVATE gtest_main unittests-stub-host)
	add_dependencies(unittests ${target})
	add_test(NAME ${target} COMMAND ${target})
endmacro()

macro(add_pcsx2_multi_isa_test target)
	if(DISABLE_ADVANCE_SIMD)
		# CMake pukes if there's no sources for an executable..
		add_executable(${target} EXCLUDE_FROM_ALL "../StubHost.cpp")
		target_link_libraries(${target} PUBLIC
			PCSX2_FLAGS
			PCSX2
			common
			gtest
			gtest_main
		)
		if(APPLE)
			target_link_libraries(${target} PUBLIC
				"-framework Foundation"
				"-framework Cocoa"
			)
		endif()
		if(WIN32)
			set(compile_options_avx2 /arch:AVX2)
			set(compile_options_avx  /arch:AVX)
		elseif(USE_GCC)
			# GCC can't inline into multi-isa functions if we use march and mtune, but can if we use feature flags
			set(compile_options_avx2 -msse4.1 -mavx -mavx2 -mbmi -mbmi2 -mfma)
			set(compile_options_avx  -msse4.1 -mavx)
			set(compile_options_sse4 -msse4.1)
		else()
			set(compile_options_avx2 -march=haswell -mtune=haswell)
			set(compile_options_avx  -march=sandybridge -mtune=sandybridge)
			set(compile_options_sse4 -msse4.1 -mtune=nehalem)
		endif()
		# ODR violation time!
		# Everything would be fine if we only defined things in cpp files, but C++ tends to like inline functions (STL anyone?)
		# Each ISA will bring with it its own copies of these inline header functions, and the linker gets to choose whichever one it wants!  Not fun if the linker chooses the avx2 version and uses it with everything
		# Thankfully, most linkers don't choose at random.  When presented with a bunch of .o files, most linkers seem to choose the first implementation they see, so make sure you order these from oldest to newest
		# Note: ld64 (macOS's linker) does not act the same way when presented with .a files, unless linked with `-force_load` (cmake WHOLE_ARCHIVE).
		set(is_first_isa "1")
		foreach(isa "sse4" "avx" "avx2")
			add_library(${target}-${isa} STATIC ${ARGN})
			target_link_libraries(${target}-${isa} PRIVATE PCSX2_FLAGS gtest)
			target_compile_definitions(${target}-${isa} PRIVATE MULTI_ISA_UNSHARED_COMPILATION=isa_${isa} MULTI_ISA_IS_FIRST=${is_first_isa} ${pcsx2_defs_${isa}})
			target_compile_options(${target}-${isa} PRIVATE ${compile_options_${isa}})
			if (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.24)
				target_link_libraries(${target} PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${target}-${isa}>)
			elseif(APPLE)
				message(FATAL_ERROR "MacOS builds with DISABLE_ADVANCE_SIMD=ON require CMake 3.24")
			else()
				target_link_libraries(${target} PRIVATE ${target}-${isa})
			endif()
			set(is_first_isa "0")
		endforeach()
	else()
		add_executable(${target} EXCLUDE_FROM_ALL ${ARGN})
	endif()

	target_link_libraries(${target} PRIVATE gtest_main unittests-stub-host)
	add_dependencies(unittests ${target})
	add_test(NAME ${target} COMMAND ${target})
endmacro()

add_subdirectory(x86emitter)
add_subdirectory(GS)
add_subdirectory(common)
