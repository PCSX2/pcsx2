include(CMakeParseArguments)

# Path to this file.
set(GCR_CMAKE_MACRO_DIR ${CMAKE_CURRENT_LIST_DIR})

# Compiles a gresource resource file from given resource files. Automatically
# creates the XML controlling file.
# The type of resource to generate (header, c-file or bundle) is automatically
# determined from TARGET file ending, if no TYPE is explicitly specified.
# The output file is stored in the provided variable "output".
# "xml_out" contains the variable where to output the XML path. Can be used to
# create custom targets or doing postprocessing.
# If you want to use preprocessing, you need to manually check the existence
# of the tools you use. This function doesn't check this for you, it just
# generates the XML file. glib-compile-resources will then throw a
# warning/error.
function(COMPILE_GRESOURCES output xml_out)
	# Available options:
	# COMPRESS_ALL, NO_COMPRESS_ALL       Overrides the COMPRESS flag in all
	#                                     registered resources.
	# STRIPBLANKS_ALL, NO_STRIPBLANKS_ALL Overrides the STRIPBLANKS flag in all
	#                                     registered resources.
	# TOPIXDATA_ALL, NO_TOPIXDATA_ALL     Overrides the TOPIXDATA flag in all
	#                                     registered resources.
	set(CG_OPTIONS COMPRESS_ALL NO_COMPRESS_ALL
	               STRIPBLANKS_ALL NO_STRIPBLANKS_ALL
	               TOPIXDATA_ALL NO_TOPIXDATA_ALL)

	# Available one value options:
	# TYPE       Type of resource to create. Valid options are:
	#            EMBED_C: A C-file that can be compiled with your project.
	#            EMBED_H: A header that can be included into your project.
	#            BUNDLE:  Generates a resource bundle file that can be loaded
	#                     at runtime.
	#            AUTO:    Determine from target file ending. Need to specify
	#                     target argument.
	# PREFIX     Overrides the resource prefix that is prepended to each
	#            relative file name in registered resources.
	# C_PREFIX   Specifies the prefix used for the C identifiers in the code generated
	#            when EMBED_C or EMBED_H are specified for TYPE.
	# SOURCE_DIR Overrides the resources base directory to search for resources.
	#            Normally this is set to the source directory with that CMake
	#            was invoked (CMAKE_CURRENT_SOURCE_DIR).
	# TARGET     Overrides the name of the output file/-s. Normally the output
	#            names from the glib-compile-resources tool are taken.
	set(CG_ONEVALUEARGS TYPE PREFIX C_PREFIX SOURCE_DIR TARGET)

	# Available multi-value options:
	# RESOURCES The list of resource files. Whether absolute or relative path is
	#           equal, absolute paths are stripped down to relative ones. If the
	#           absolute path is not inside the given base directory SOURCE_DIR
	#           or CMAKE_CURRENT_SOURCE_DIR (if SOURCE_DIR is not overriden),
	#           this function aborts.
	# OPTIONS   Extra command line options passed to glib-compile-resources.
	set(CG_MULTIVALUEARGS RESOURCES OPTIONS)

	# Parse the arguments.
	cmake_parse_arguments(CG_ARG
	                      "${CG_OPTIONS}"
	                      "${CG_ONEVALUEARGS}"
	                      "${CG_MULTIVALUEARGS}"
	                      "${ARGN}")

	# Variable to store the double-quote (") string. Since escaping
	# double-quotes in strings is not possible we need a helper variable that
	# does this job for us.
	set(Q \")

	# Check invocation validity with the <prefix>_UNPARSED_ARGUMENTS variable.
	# If other not recognized parameters were passed, throw error.
	if (CG_ARG_UNPARSED_ARGUMENTS)
		set(CG_WARNMSG "Invocation of COMPILE_GRESOURCES with unrecognized")
		set(CG_WARNMSG "${CG_WARNMSG} parameters. Parameters are:")
		set(CG_WARNMSG "${CG_WARNMSG} ${CG_ARG_UNPARSED_ARGUMENTS}.")
		message(WARNING ${CG_WARNMSG})
	endif()

	# Check invocation validity depending on generation mode (EMBED_C, EMBED_H
	# or BUNDLE).
	if ("${CG_ARG_TYPE}" STREQUAL "EMBED_C")
		# EMBED_C mode, output compilable C-file.
		list(APPEND CG_GENERATE_COMMAND_LINE --generate-source)
		if (NOT "${CG_ARG_C_PREFIX}" STREQUAL "")
			list(APPEND CG_GENERATE_COMMAND_LINE --c-name "${CG_ARG_C_PREFIX}")
		endif()
		set(CG_TARGET_FILE_ENDING "cpp")
	elseif ("${CG_ARG_TYPE}" STREQUAL "EMBED_H")
		# EMBED_H mode, output includable header file.
		list(APPEND CG_GENERATE_COMMAND_LINE --generate-header)
		if (NOT "${CG_ARG_C_PREFIX}" STREQUAL "")
			list(APPEND CG_GENERATE_COMMAND_LINE --c-name "${CG_ARG_C_PREFIX}")
		endif()
		set(CG_TARGET_FILE_ENDING "h")
	elseif ("${CG_ARG_TYPE}" STREQUAL "BUNDLE")
		# BUNDLE mode, output resource bundle. Don't do anything since
		# glib-compile-resources outputs a bundle when not specifying
		# something else.
		set(CG_TARGET_FILE_ENDING "gresource")
		if (NOT "${CG_ARG_C_PREFIX}" STREQUAL "")
			message(WARNING "Superfluously provided C_PREFIX=${CG_ARG_C_PREFIX} for BUNDLE.")
		endif()
	else()
		# Everything else is AUTO mode, determine from target file ending.
		if (CG_ARG_TARGET)
			list(APPEND CG_GENERATE_COMMAND_LINE --generate)
		else()
			set(CG_ERRMSG "AUTO mode given, but no target specified. Can't")
			set(CG_ERRMSG "${CG_ERRMSG} determine output type. In function")
			set(CG_ERRMSG "${CG_ERRMSG} COMPILE_GRESOURCES.")
			message(FATAL_ERROR ${CG_ERRMSG})
		endif()
	endif()

	# Check flag validity.
	if (CG_ARG_COMPRESS_ALL AND CG_ARG_NO_COMPRESS_ALL)
		set(CG_ERRMSG "COMPRESS_ALL and NO_COMPRESS_ALL simultaneously set. In")
		set(CG_ERRMSG "${CG_ERRMSG} function COMPILE_GRESOURCES.")
		message(FATAL_ERROR ${CG_ERRMSG})
	endif()
	if (CG_ARG_STRIPBLANKS_ALL AND CG_ARG_NO_STRIPBLANKS_ALL)
		set(CG_ERRMSG "STRIPBLANKS_ALL and NO_STRIPBLANKS_ALL simultaneously")
		set(CG_ERRMSG "${CG_ERRMSG} set. In function COMPILE_GRESOURCES.")
		message(FATAL_ERROR ${CG_ERRMSG})
	endif()
	if (CG_ARG_TOPIXDATA_ALL AND CG_ARG_NO_TOPIXDATA_ALL)
		set(CG_ERRMSG "TOPIXDATA_ALL and NO_TOPIXDATA_ALL simultaneously set.")
		set(CG_ERRMSG "${CG_ERRMSG} In function COMPILE_GRESOURCES.")
		message(FATAL_ERROR ${CG_ERRMSG})
	endif()

	# Check if there are any resources.
	if (NOT CG_ARG_RESOURCES)
		set(CG_ERRMSG "No resource files to process. In function")
		set(CG_ERRMSG "${CG_ERRMSG} COMPILE_GRESOURCES.")
		message(FATAL_ERROR ${CG_ERRMSG})
	endif()

	# Extract all dependencies for targets from resource list.
	foreach(res ${CG_ARG_RESOURCES})
		if (NOT(("${res}" STREQUAL "COMPRESS") OR
		        ("${res}" STREQUAL "STRIPBLANKS") OR
		        ("${res}" STREQUAL "TOPIXDATA")))

			list(APPEND CG_RESOURCES_DEPENDENCIES "${res}")
		endif()
	endforeach()

	# Construct .gresource.xml path.
	set(CG_XML_FILE_PATH "${CMAKE_CURRENT_BINARY_DIR}/.gresource.xml")

	# Generate gresources XML target.
	list(APPEND CG_CMAKE_SCRIPT_ARGS "-D")
	list(APPEND CG_CMAKE_SCRIPT_ARGS "GXML_OUTPUT=${CG_XML_FILE_PATH}")
	if(CG_ARG_COMPRESS_ALL)
		list(APPEND CG_CMAKE_SCRIPT_ARGS "-D")
		list(APPEND CG_CMAKE_SCRIPT_ARGS "GXML_COMPRESS_ALL=True")
	endif()
	if(CG_ARG_NO_COMPRESS_ALL)
		list(APPEND CG_CMAKE_SCRIPT_ARGS "-D")
		list(APPEND CG_CMAKE_SCRIPT_ARGS "GXML_NO_COMPRESS_ALL=True")
	endif()
	if(CG_ARG_STRPIBLANKS_ALL)
		list(APPEND CG_CMAKE_SCRIPT_ARGS "-D")
		list(APPEND CG_CMAKE_SCRIPT_ARGS "GXML_STRIPBLANKS_ALL=True")
	endif()
	if(CG_ARG_NO_STRIPBLANKS_ALL)
		list(APPEND CG_CMAKE_SCRIPT_ARGS "-D")
		list(APPEND CG_CMAKE_SCRIPT_ARGS "GXML_NO_STRIPBLANKS_ALL=True")
	endif()
	if(CG_ARG_TOPIXDATA_ALL)
		list(APPEND CG_CMAKE_SCRIPT_ARGS "-D")
		list(APPEND CG_CMAKE_SCRIPT_ARGS "GXML_TOPIXDATA_ALL=True")
	endif()
	if(CG_ARG_NO_TOPIXDATA_ALL)
		list(APPEND CG_CMAKE_SCRIPT_ARGS "-D")
		list(APPEND CG_CMAKE_SCRIPT_ARGS "GXML_NO_TOPIXDATA_ALL=True")
	endif()
	list(APPEND CG_CMAKE_SCRIPT_ARGS "-D")
	list(APPEND CG_CMAKE_SCRIPT_ARGS "GXML_PREFIX=${Q}${CG_ARG_PREFIX}${Q}")
	list(APPEND CG_CMAKE_SCRIPT_ARGS "-D")
	list(APPEND CG_CMAKE_SCRIPT_ARGS
		 "GXML_RESOURCES=${Q}${CG_ARG_RESOURCES}${Q}")
	list(APPEND CG_CMAKE_SCRIPT_ARGS "-P")
	list(APPEND CG_CMAKE_SCRIPT_ARGS
		 "${GCR_CMAKE_MACRO_DIR}/BuildTargetScript.cmake")

	get_filename_component(CG_XML_FILE_PATH_ONLY_NAME
	                       "${CG_XML_FILE_PATH}" NAME)
	set(CG_XML_CUSTOM_COMMAND_COMMENT
		"Creating gresources XML file (${CG_XML_FILE_PATH_ONLY_NAME})")
	add_custom_command(
		OUTPUT ${CG_XML_FILE_PATH}
		COMMAND ${CMAKE_COMMAND}
		ARGS ${CG_CMAKE_SCRIPT_ARGS}
		DEPENDS ${CG_RESOURCES_DEPENDENCIES}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT ${CG_XML_CUSTOM_COMMAND_COMMENT})

	# Create target manually if not set (to make sure glib-compile-resources
	# doesn't change behaviour with it's naming standards).
	if (NOT CG_ARG_TARGET)
		set(CG_ARG_TARGET "${CMAKE_CURRENT_BINARY_DIR}/resources")
		set(CG_ARG_TARGET "${CG_ARG_TARGET}.${CG_TARGET_FILE_ENDING}")
	endif()

	# Create source directory automatically if not set.
	if (NOT CG_ARG_SOURCE_DIR)
		set(CG_ARG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	endif()

	# Add compilation target for resources.
	add_custom_command(
		OUTPUT ${CG_ARG_TARGET}
		COMMAND ${GLIB_COMPILE_RESOURCES_EXECUTABLE}
		ARGS
			${OPTIONS}
			--target ${CG_ARG_TARGET}
			--sourcedir ${CG_ARG_SOURCE_DIR}
			${CG_GENERATE_COMMAND_LINE}
			${CG_XML_FILE_PATH}
		VERBATIM
		MAIN_DEPENDENCY ${CG_XML_FILE_PATH}
		DEPENDS ${CG_RESOURCES_DEPENDENCIES}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BUILD_DIR})

	# Set output and XML_OUT to parent scope.
	set(${xml_out} ${CG_XML_FILE_PATH} PARENT_SCOPE)
	set(${output} ${CG_ARG_TARGET} PARENT_SCOPE)

endfunction()
