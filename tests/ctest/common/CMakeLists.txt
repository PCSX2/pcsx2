add_pcsx2_test(common_test
	path_tests.cpp
	string_util_tests.cpp
	x86emitter/codegen_tests.cpp
	x86emitter/codegen_tests.h
	x86emitter/codegen_tests_main.cpp
)

target_link_libraries(common_test PRIVATE
	common
)
