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
