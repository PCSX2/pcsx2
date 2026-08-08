function(shader_to_cpp SHADER_FILES_OUT SHADER_FILES_CPP_OUT CPP_OUTPUT_DIR_OUT)
	set(SHADER_FILES "")
	set(SHADER_FILES_CPP "")
	set(CPP_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders_cpp")
	file(MAKE_DIRECTORY ${CPP_OUTPUT_DIR})
	file(GLOB_RECURSE DIR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../bin/resources/shaders/*")
	foreach(path IN LISTS DIR_FILES)
		if(NOT ("${path}" MATCHES ".*\.glsl$" OR "${path}" MATCHES ".*\.fx$"))
			continue()
		endif()
		if (NOT WIN32 AND "${path}" MATCHES "/dx11/") # Don't include unneccessary stuff
			continue()
		endif()
		get_filename_component(DIR ${path} DIRECTORY)
		get_filename_component(API ${DIR} NAME)
		get_filename_component(BASE ${path} NAME_WE)
		set(cpp_path "${CPP_OUTPUT_DIR}/${API}_${BASE}.cpp")
		add_custom_command(
			OUTPUT ${cpp_path}
			COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/../tools/shader_to_cpp.py ${path} ${cpp_path} "${API}_${BASE}"
			DEPENDS ${path} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/shader_to_cpp.py
			COMMENT "Shader to CPP: ${path} -> ${cpp_path}"
			VERBATIM
		)
		list(APPEND SHADER_FILES ${path})
		list(APPEND SHADER_FILES_CPP ${cpp_path})
	endforeach()
	set(${SHADER_FILES_OUT} ${SHADER_FILES} PARENT_SCOPE)
	set(${SHADER_FILES_CPP_OUT} ${SHADER_FILES_CPP} PARENT_SCOPE)
	set(${CPP_OUTPUT_DIR_OUT} ${CPP_OUTPUT_DIR} PARENT_SCOPE)
endfunction()