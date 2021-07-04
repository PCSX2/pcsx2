include(CMakeParseArguments)

# Generates the resource XML controlling file from resource list (and saves it
# to xml_path). It's not recommended to use this function directly, since it
# doesn't handle invalid arguments. It is used by the function
# COMPILE_GRESOURCES() to create a custom command, so that this function is
# invoked at build-time in script mode from CMake.
function(GENERATE_GXML xml_path)
	# Available options:
	# COMPRESS_ALL, NO_COMPRESS_ALL       Overrides the COMPRESS flag in all
	#                                     registered resources.
	# STRIPBLANKS_ALL, NO_STRIPBLANKS_ALL Overrides the STRIPBLANKS flag in all
	#                                     registered resources.
	# TOPIXDATA_ALL, NO_TOPIXDATA_ALL     Overrides the TOPIXDATA flag in all
	#                                     registered resources.
	set(GXML_OPTIONS COMPRESS_ALL NO_COMPRESS_ALL
	                 STRIPBLANKS_ALL NO_STRIPBLANKS_ALL
	                 TOPIXDATA_ALL NO_TOPIXDATA_ALL)

	# Available one value options:
	# PREFIX     Overrides the resource prefix that is prepended to each
	#            relative file name in registered resources.
	set(GXML_ONEVALUEARGS PREFIX)

	# Available multi-value options:
	# RESOURCES The list of resource files. Whether absolute or relative path is
	#           equal, absolute paths are stripped down to relative ones. If the
	#           absolute path is not inside the given base directory SOURCE_DIR
	#           or CMAKE_CURRENT_SOURCE_DIR (if SOURCE_DIR is not overriden),
	#           this function aborts.
	set(GXML_MULTIVALUEARGS RESOURCES)

	# Parse the arguments.
	cmake_parse_arguments(GXML_ARG
	                      "${GXML_OPTIONS}"
	                      "${GXML_ONEVALUEARGS}"
	                      "${GXML_MULTIVALUEARGS}"
	                      "${ARGN}")

	# Variable to store the double-quote (") string. Since escaping
	# double-quotes in strings is not possible we need a helper variable that
	# does this job for us.
	set(Q \")

	# Process resources and generate XML file.
	# Begin with the XML header and header nodes.
	set(GXML_XML_FILE "<?xml version=${Q}1.0${Q} encoding=${Q}UTF-8${Q}?>")
	set(GXML_XML_FILE "${GXML_XML_FILE}<gresources><gresource prefix=${Q}")

	# Set the prefix for the resources. Depending on the user-override we choose
	# the standard prefix "/" or the override.
	if (GXML_ARG_PREFIX)
		set(GXML_XML_FILE "${GXML_XML_FILE}${GXML_ARG_PREFIX}")
	else()
		set(GXML_XML_FILE "${GXML_XML_FILE}/")
	endif()

	set(GXML_XML_FILE "${GXML_XML_FILE}${Q}>")

	# Process each resource.
	foreach(res ${GXML_ARG_RESOURCES})
		if ("${res}" STREQUAL "COMPRESS")
			set(GXML_COMPRESSION_FLAG ON)
		elseif ("${res}" STREQUAL "STRIPBLANKS")
			set(GXML_STRIPBLANKS_FLAG ON)
		elseif ("${res}" STREQUAL "TOPIXDATA")
			set(GXML_TOPIXDATA_FLAG ON)
		else()
			# The file name.
			set(GXML_RESOURCE_PATH "${res}")

			# Append to real resource file dependency list.
			list(APPEND GXML_RESOURCES_DEPENDENCIES ${GXML_RESOURCE_PATH})

			# Assemble <file> node.
			set(GXML_RES_LINE "<file")
			if ((GXML_ARG_COMPRESS_ALL OR GXML_COMPRESSION_FLAG) AND NOT
					GXML_ARG_NO_COMPRESS_ALL)
				set(GXML_RES_LINE "${GXML_RES_LINE} compressed=${Q}true${Q}")
			endif()

			# Check preprocess flag validity.
			if ((GXML_ARG_STRIPBLANKS_ALL OR GXML_STRIPBLANKS_FLAG) AND
					(GXML_ARG_TOPIXDATA_ALL OR GXML_TOPIXDATA_FLAG))
				set(GXML_ERRMSG "Resource preprocessing option conflict. Tried")
				set(GXML_ERRMSG "${GXML_ERRMSG} to specify both, STRIPBLANKS")
				set(GXML_ERRMSG "${GXML_ERRMSG} and TOPIXDATA. In resource")
				set(GXML_ERRMSG "${GXML_ERRMSG} ${GXML_RESOURCE_PATH} in")
				set(GXML_ERRMSG "${GXML_ERRMSG} function COMPILE_GRESOURCES.")
				message(FATAL_ERROR ${GXML_ERRMSG})
			endif()

			if ((GXML_ARG_STRIPBLANKS_ALL OR GXML_STRIPBLANKS_FLAG) AND NOT
					GXML_ARG_NO_STRIPBLANKS_ALL)
				set(GXML_RES_LINE "${GXML_RES_LINE} preprocess=")
				set(GXML_RES_LINE "${GXML_RES_LINE}${Q}xml-stripblanks${Q}")
			elseif((GXML_ARG_TOPIXDATA_ALL OR GXML_TOPIXDATA_FLAG) AND NOT
					GXML_ARG_NO_TOPIXDATA_ALL)
				set(GXML_RES_LINE "${GXML_RES_LINE} preprocess=")
				set(GXML_RES_LINE "${GXML_RES_LINE}${Q}to-pixdata${Q}")
			endif()

			set(GXML_RES_LINE "${GXML_RES_LINE}>${GXML_RESOURCE_PATH}</file>")

			# Append to file string.
			set(GXML_XML_FILE "${GXML_XML_FILE}${GXML_RES_LINE}")

			# Unset variables.
			unset(GXML_COMPRESSION_FLAG)
			unset(GXML_STRIPBLANKS_FLAG)
			unset(GXML_TOPIXDATA_FLAG)
		endif()

	endforeach()

	# Append closing nodes.
	set(GXML_XML_FILE "${GXML_XML_FILE}</gresource></gresources>")

	# Use "file" function to generate XML controlling file.
	get_filename_component(xml_path_only_name "${xml_path}" NAME)
	file(WRITE ${xml_path} ${GXML_XML_FILE})

endfunction()

