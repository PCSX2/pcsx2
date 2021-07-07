foreach(isa "sse4" "avx" "avx2")
	set(GSDir ${CMAKE_SOURCE_DIR}/pcsx2/GS)

	if(${native_vector_isa} LESS ${isa_number_${isa}})
		# Skip unsupported tests
		continue()
	endif()

	add_pcsx2_test(swizzle_test_${isa}
		swizzle_test_main.cpp
		swizzle_test_nops.cpp
		${GSDir}/GSBlock.cpp
		${GSDir}/GSBlock.h
		${GSDir}/GSClut.cpp
		${GSDir}/GSClut.h
		${GSDir}/GSTables.cpp
		${GSDir}/GSTables.h)

	target_include_directories(swizzle_test_${isa} PRIVATE ${GSDir} ${CMAKE_SOURCE_DIR}/pcsx2/ ${CMAKE_SOURCE_DIR}/pcsx2/gui)
	if(WIN32)
		target_include_directories(swizzle_test_${isa} PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty)
	endif()

	target_compile_options(swizzle_test_${isa} PRIVATE ${compile_options_${isa}})
	target_compile_definitions(swizzle_test_${isa} PRIVATE ${definitions_${isa}})
	if(WIN32)
		target_compile_definitions(swizzle_test_${isa} PRIVATE
			WINVER=0x0603
			_WIN32_WINNT=0x0603
			WIN32_LEAN_AND_MEAN
		)
	endif()
endforeach()
