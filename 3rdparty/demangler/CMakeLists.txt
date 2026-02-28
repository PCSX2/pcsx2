add_library(demanglegnu STATIC
	src/alloca.c
	src/cp-demangle.c
	src/cplus-dem.c
	src/d-demangle.c
	src/dyn-string.c
	src/rust-demangle.c
	src/safe-ctype.c
	src/xexit.c
	src/xmalloc.c
	src/xmemdup.c
	src/xstrdup.c
)

set(GNU_DEMANGLER_FLAGS -DHAVE_DECL_BASENAME=1 -DHAVE_LIMITS_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1)

target_include_directories(demanglegnu PUBLIC include)
target_compile_definitions(demanglegnu PUBLIC ${GNU_DEMANGLER_FLAGS})

if((CMAKE_SYSTEM_NAME STREQUAL "Linux") OR (CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
	add_executable(demangler_fuzzer testsuite/demangler-fuzzer.c)
	target_link_libraries(demangler_fuzzer demanglegnu)
	target_compile_definitions(demangler_fuzzer PUBLIC ${GNU_DEMANGLER_FLAGS})
endif()
add_executable(demangler_test testsuite/test-demangle.c)
target_link_libraries(demangler_test demanglegnu)
target_compile_definitions(demangler_test PUBLIC ${GNU_DEMANGLER_FLAGS})
