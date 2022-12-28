enable_testing()
add_custom_target(unittests)
add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)

macro(add_pcsx2_test target)
	add_executable(${target} EXCLUDE_FROM_ALL ${ARGN})
	target_link_libraries(${target} PRIVATE gtest gtest_main)
	if(APPLE)
		# For some reason this doesn't get pulled in implicitly...
		target_link_libraries(${target} PRIVATE
			"-framework Foundation"
			"-framework Cocoa"
		)
	endif()

	add_dependencies(unittests ${target})
	add_test(NAME ${target} COMMAND ${target})
endmacro()

add_subdirectory(gtest)
add_subdirectory(common)
add_subdirectory(core)
