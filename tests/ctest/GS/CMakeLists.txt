foreach(isa "sse4" "avx" "avx2")
	set(GSDir ${CMAKE_SOURCE_DIR}/pcsx2/GS)

	if((${isa} STREQUAL "sse4" AND ${native_vector_isa} LESS 41)
	OR (${isa} STREQUAL "avx"  AND ${native_vector_isa} LESS 50)
	OR (${isa} STREQUAL "avx2" AND ${native_vector_isa} LESS 51))
		# Skip unsupported tests
		continue()
	endif()

	add_pcsx2_test(swizzle_test_${isa}
		swizzle_test_main.cpp
		${GSDir}/GSBlock.cpp
		${GSDir}/GSBlock.h
		${GSDir}/GSClut.cpp
		${GSDir}/GSClut.h
		${GSDir}/GSTables.cpp
		${GSDir}/GSTables.h)

	target_include_directories(swizzle_test_${isa} PRIVATE ${GSDir} ${CMAKE_SOURCE_DIR}/pcsx2/ ${CMAKE_SOURCE_DIR}/pcsx2/gui)

	# Prevent linker errors for functions in e.g. GSClut that we don't actually use depending on things we didn't include
	if(UNIX AND NOT APPLE)
		target_compile_options(swizzle_test_${isa} PRIVATE -ffunction-sections)
		target_link_options(swizzle_test_${isa} PRIVATE -Wl,-gc-sections)
	elseif(APPLE)
		target_link_options(swizzle_test_${isa} PRIVATE -Wl,-dead_strip)
	endif()

	if(${isa} STREQUAL "avx2")
		target_compile_options(swizzle_test_${isa} PRIVATE -mavx2 -mbmi -mbmi2)
	elseif(${isa} STREQUAL "avx")
		target_compile_options(swizzle_test_${isa} PRIVATE -mavx)
	else()
		target_compile_options(swizzle_test_${isa} PRIVATE -msse4.1)
	endif()
endforeach()
