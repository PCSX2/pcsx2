include(FindPkgConfig OPTIONAL)

macro(_internal_message msg)
	message("${msg}")
endmacro()

macro(check_lib var lib)
	set(_arg_list ${ARGN})

	if(PKG_CONFIG_FOUND AND NOT CMAKE_CROSSCOMPILING AND NOT DEFINED pcsx2_manually_found_${var})
		string(TOLOWER ${lib} lower_lib)
		pkg_search_module(${var} QUIET IMPORTED_TARGET ${lower_lib})
	endif()

	if(TARGET PkgConfig::${var})
		_internal_message("-- ${var} found pkg")
	else()
		find_library(${var}_LIBRARIES ${lib})
		if(_arg_list)
			find_path(${var}_INCLUDE ${_arg_list})
		else()
			set(${var}_INCLUDE FALSE)
		endif()

		if(${var}_LIBRARIES AND ${var}_INCLUDE)
			add_library(PkgConfig::${var} UNKNOWN IMPORTED GLOBAL)
			# Imitate what pkg-config would have found
			set_target_properties(PkgConfig::${var} PROPERTIES
				IMPORTED_LOCATION "${${var}_LIBRARIES}"
				INTERFACE_INCLUDE_DIRECTORIES "${${var}_INCLUDE}"
			)
			_internal_message("-- ${var} found")
			set(${var}_FOUND 1 CACHE INTERNAL "")
			set(pcsx2_manually_found_${var} 1 CACHE INTERNAL "")
		elseif(${var}_LIBRARIES)
			_internal_message("-- ${var} not found (miss include)")
		elseif(${var}_INCLUDE)
			_internal_message("-- ${var} not found (miss lib)")
		else()
			_internal_message("-- ${var} not found")
		endif()
	endif()
endmacro()
