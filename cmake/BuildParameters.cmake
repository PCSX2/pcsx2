# Extra preprocessor definitions that will be added to all pcsx2 builds
set(PCSX2_DEFS "")

#-------------------------------------------------------------------------------
# Misc option
#-------------------------------------------------------------------------------
option(ENABLE_TESTS "Enables building the unit tests" ON)
option(LTO_PCSX2_CORE "Enable LTO/IPO/LTCG on the subset of pcsx2 that benefits most from it but not anything else")
option(USE_VTUNE "Plug VTUNE to profile GS JIT.")

#-------------------------------------------------------------------------------
# Graphical option
#-------------------------------------------------------------------------------
option(USE_OPENGL "Enable OpenGL GS renderer" ON)
option(USE_VULKAN "Enable Vulkan GS renderer" ON)

#-------------------------------------------------------------------------------
# Path and lib option
#-------------------------------------------------------------------------------
if(UNIX AND NOT APPLE)
	option(ENABLE_SETCAP "Enable networking capability for DEV9" OFF)
	option(X11_API "Enable X11 support" ON)
	option(WAYLAND_API "Enable Wayland support" ON)
endif()

if(UNIX)
	option(USE_LINKED_FFMPEG "Links with ffmpeg instead of using dynamic loading" OFF)
endif()

if(APPLE)
	option(OSX_USE_DEFAULT_SEARCH_PATH "Don't prioritize system library paths" OFF)
	option(SKIP_POSTPROCESS_BUNDLE "Skip postprocessing bundle for redistributability" OFF)
endif()

#-------------------------------------------------------------------------------
# Compiler extra
#-------------------------------------------------------------------------------
option(USE_ASAN "Enable address sanitizer")

#-------------------------------------------------------------------------------
# if no build type is set, use Devel as default
# Note without the CMAKE_BUILD_TYPE options the value is still defined to ""
# Ensure that the value set by the User is correct to avoid some bad behavior later
#-------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE MATCHES "Debug|Devel|MinSizeRel|RelWithDebInfo|Release")
	set(CMAKE_BUILD_TYPE Devel)
	message(STATUS "BuildType set to ${CMAKE_BUILD_TYPE} by default")
endif()
# Add Devel build type
set(CMAKE_C_FLAGS_DEVEL "${CMAKE_C_FLAGS_RELWITHDEBINFO}"
	CACHE STRING "Flags used by the C compiler during development builds" FORCE)
set(CMAKE_CXX_FLAGS_DEVEL "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}"
	CACHE STRING "Flags used by the C++ compiler during development builds" FORCE)
set(CMAKE_LINKER_FLAGS_DEVEL "${CMAKE_LINKER_FLAGS_RELWITHDEBINFO}"
	CACHE STRING "Flags used for linking binaries during development builds" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DEVEL "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}"
	CACHE STRING "Flags used for linking shared libraries during development builds" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEVEL "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}"
	CACHE STRING "Flags used for linking executables during development builds" FORCE)
if(CMAKE_CONFIGURATION_TYPES)
	list(INSERT CMAKE_CONFIGURATION_TYPES 0 Devel)
endif()
mark_as_advanced(CMAKE_C_FLAGS_DEVEL CMAKE_CXX_FLAGS_DEVEL CMAKE_LINKER_FLAGS_DEVEL CMAKE_SHARED_LINKER_FLAGS_DEVEL CMAKE_EXE_LINKER_FLAGS_DEVEL)

#-------------------------------------------------------------------------------
# Select the architecture
#-------------------------------------------------------------------------------
if("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "x86_64" OR "${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "amd64" OR
   "${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "AMD64" OR "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64")
	# Multi-ISA only exists on x86.
	option(DISABLE_ADVANCE_SIMD "Disable advance use of SIMD (SSE2+ & AVX)" OFF)

	list(APPEND PCSX2_DEFS _M_X86=1)
	set(_M_X86 TRUE)
	if(DISABLE_ADVANCE_SIMD)
		message(STATUS "Building for x86-64 (Multi-ISA).")
	else()
		message(STATUS "Building for x86-64.")
	endif()

	if(MSVC)
		# SSE4.1 is not set by MSVC, it uses _M_SSE instead.
		list(APPEND PCSX2_DEFS __SSE4_1__=1)

		if(USE_CLANG_CL)
			# clang-cl => need to explicitly enable SSE4.1.
			add_compile_options("-msse4.1")
		endif()
	else()
		# Multi-ISA => SSE4, otherwise native.
		if (DISABLE_ADVANCE_SIMD)
			add_compile_options("-msse" "-msse2" "-msse4.1" "-mfxsr")
		else()
			# Can't use march=native on Apple Silicon.
			if(NOT APPLE OR "${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
				add_compile_options("-march=native")
			endif()
		endif()
	endif()
else()
	message(FATAL_ERROR "Unsupported architecture: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

# Require C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC AND NOT USE_CLANG_CL)
	add_compile_options(
		"$<$<COMPILE_LANGUAGE:CXX>:/Zc:externConstexpr>"
		"$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>"
		"$<$<COMPILE_LANGUAGE:CXX>:/permissive->"
		"/Zo"
		"/utf-8"
	)
endif()

if(MSVC)
	# Disable RTTI
	string(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

	# Disable Exceptions
	string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
else()
	add_compile_options(-pipe -fvisibility=hidden -pthread)
	add_compile_options(
		"$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>"
		"$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>"
	)
endif()

set(CONFIG_REL_NO_DEB $<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>)
set(CONFIG_ANY_REL $<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>)

if(WIN32)
	add_compile_definitions(
		$<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=2>
		$<$<CONFIG:Devel>:_ITERATOR_DEBUG_LEVEL=1>
		$<${CONFIG_ANY_REL}:_ITERATOR_DEBUG_LEVEL=0>
		_HAS_EXCEPTIONS=0
	)
	list(APPEND PCSX2_DEFS
		_CRT_NONSTDC_NO_WARNINGS
		_CRT_SECURE_NO_WARNINGS
		CRT_SECURE_NO_DEPRECATE
		_SCL_SECURE_NO_WARNINGS
		_UNICODE
		UNICODE
	)
else()
	# Assume everything else is POSIX.
	list(APPEND PCSX2_DEFS
		__POSIX__
	)
endif()

# Enable debug information in release builds for Linux.
# Makes the backtrace actually meaningful.
if(LINUX)
	add_compile_options($<$<CONFIG:Release>:-g1>)
endif()

if(MSVC)
	# Enable PDB generation in release builds
	add_compile_options(
		$<${CONFIG_REL_NO_DEB}:/Zi>
	)
	add_link_options(
		$<${CONFIG_REL_NO_DEB}:/DEBUG>
		$<${CONFIG_REL_NO_DEB}:/OPT:REF>
		$<${CONFIG_REL_NO_DEB}:/OPT:ICF>
	)
endif()

if(USE_VTUNE)
	list(APPEND PCSX2_DEFS ENABLE_VTUNE)
endif()

if(USE_OPENGL)
	list(APPEND PCSX2_DEFS ENABLE_OPENGL)
endif()

if(USE_VULKAN)
	list(APPEND PCSX2_DEFS ENABLE_VULKAN)
endif()

if(X11_API)
	list(APPEND PCSX2_DEFS X11_API)
endif()

if(WAYLAND_API)
	list(APPEND PCSX2_DEFS WAYLAND_API)
endif()

# -Wno-attributes: "always_inline function might not be inlinable" <= real spam (thousand of warnings!!!)
# -Wno-missing-field-initializers: standard allow to init only the begin of struct/array in static init. Just a silly warning.
# -Wno-unused-function: warn for function not used in release build

if (MSVC)
	set(DEFAULT_WARNINGS)
else()
	set(DEFAULT_WARNINGS -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

if (USE_PGO_GENERATE OR USE_PGO_OPTIMIZE)
	add_compile_options("-fprofile-dir=${CMAKE_SOURCE_DIR}/profile")
endif()

if (USE_PGO_GENERATE)
	add_compile_options(-fprofile-generate)
endif()

if(USE_PGO_OPTIMIZE)
	add_compile_options(-fprofile-use)
endif()

list(APPEND PCSX2_DEFS
	"$<$<CONFIG:Debug>:PCSX2_DEVBUILD;PCSX2_DEBUG;_DEBUG>"
	"$<$<CONFIG:Devel>:PCSX2_DEVBUILD;_DEVEL>")

if (USE_ASAN)
	add_compile_options(-fsanitize=address)
	add_link_options(-fsanitize=address)
	list(APPEND PCSX2_DEFS ASAN_WORKAROUND)
endif()

if(USE_CLANG AND TIMETRACE)
	add_compile_options(-ftime-trace)
endif()

set(PCSX2_WARNINGS ${DEFAULT_WARNINGS})

#-------------------------------------------------------------------------------
# MacOS-specific things
#-------------------------------------------------------------------------------

if(NOT CMAKE_GENERATOR MATCHES "Xcode")
	# Assume Xcode builds aren't being used for distribution
	# Helpful because Xcode builds don't build multiple metallibs for different macOS versions
	# Also helpful because Xcode's interactive shader debugger requires apps be built for the latest macOS
	set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
endif()

# CMake defaults the suffix for modules to .so on macOS but wx tells us that the
# extension is .dylib (so that's what we search for)
if(APPLE)
	set(CMAKE_SHARED_MODULE_SUFFIX ".dylib")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
	if(NOT OSX_USE_DEFAULT_SEARCH_PATH)
		# Hack up the path to prioritize the path to built-in OS libraries to
		# increase the chance of not depending on a bunch of copies of them
		# installed by MacPorts, Fink, Homebrew, etc, and ending up copying
		# them into the bundle.  Since we depend on libraries which are not
		# part of OS X (wx, etc.), however, don't remove the default path
		# entirely.  This is still kinda evil, since it defeats the user's
		# path settings...
		# See http://www.cmake.org/cmake/help/v3.0/command/find_program.html
		list(APPEND CMAKE_PREFIX_PATH "/usr")
	endif()

	add_link_options(-Wl,-dead_strip,-dead_strip_dylibs)
endif()
